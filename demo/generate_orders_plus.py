import numpy as np
import random
import csv
import math
import time

# --- 配置参数 ---
INITIAL_MID_PRICE = 85000  # 初始中间价
PRICE_SCALE = INITIAL_MID_PRICE * 0.08         # 价格波动范围基准
ORDER_COUNT = 2_000_000     # 生成的操作总数
LO_MO_RATIO = 0.5          # 限价单/市价单 事件比例 (近似值)
CANCEL_RATIO_OF_LO = 0.8  # 限价单事件中取消操作的比例 (近似值)

# 订单规模分布参数
VOLUME_POWER_LAW_ALPHA = 1.44 # 幂律分布指数 (近似)
VOLUME_BASE_MEAN = 50       # 基础规模均值 (用于幂律分布截断或调整)
VOLUME_ROUND_NUMBERS = {0.1: 0.15, 0.5: 0.1, 1.0: 0.1, 5.0: 0.05, 10.0: 0.05} # 圆整数及其概率
VOLUME_POWER_LAW_PROB = 1.0 - sum(VOLUME_ROUND_NUMBERS.values()) # 幂律分布的概率

# 价格放置分布参数 (简化模型: 正态分布 + 均匀分布尾部)
PRICE_PLACEMENT_NEAR_PROB = 0.8 # 价格靠近最优买卖价的概率
PRICE_PLACEMENT_NEAR_SCALE_RATIO = 0.5 # 靠近时价格的标准差与 SPREAD 的比例
PRICE_PLACEMENT_FAR_RANGE_RATIO = 3 # 远离时价格的最大范围与 SPREAD 的比例 (从 10 减小)
AGGRESSIVE_LIMIT_ORDER_PROB = 0.1 # 例如 10% 的限价单尝试穿越价差

# 取消逻辑参数 (简化模型: 基于时间和距离)
CANCEL_BASE_PROB = 0.05      # 基础取消概率
CANCEL_TIME_FACTOR = 0.01    # 时间越长，取消概率增加因子
CANCEL_DISTANCE_FACTOR = 0.02 # 距离越远，取消概率增加因子 (远离有利方向)

# 价格动态参数
MEAN_REVERSION_STRENGTH = 0.0005 # 新增：均值回归强度 (值越小，回归越慢)
PRICE_DRIFT_VOLATILITY = 0.03    # 新增：基础价格漂移波动率 (替代原 0.05)

# --- 辅助函数 ---

def generate_order_id(used_order_ids):
    """生成唯一的订单ID"""
    new_id = random.randint(1, 100_000_000)
    while new_id in used_order_ids:
        new_id = random.randint(1, 100_000_000)
    used_order_ids.add(new_id)
    return new_id

def generate_volume():
    """生成符合混合分布的订单规模"""
    rand_choice = random.random()
    cumulative_prob = 0.0
    for number, prob in VOLUME_ROUND_NUMBERS.items():
        cumulative_prob += prob
        if rand_choice < cumulative_prob:
            return number

    # 生成符合幂律分布的规模 (使用 Pareto 分布近似)
    # 注意：直接生成精确的幂律分布较复杂，这里用 Pareto 近似，并做一些调整
    # scale 参数影响最小值，alpha 是形状参数
    # 我们需要调整使其更符合实际情况，例如增加一个最小值或截断
    volume = np.random.pareto(VOLUME_POWER_LAW_ALPHA) + 1 # 加1避免为0
    # 可以根据需要调整或截断过大的值，或乘以一个基础值
    volume = max(0.01, round(volume * (VOLUME_BASE_MEAN / 5), 4)) # 简单缩放和限制最小值
    return volume


def generate_limit_order_price(mid_price, spread, side):
    """生成限价单价格，考虑集中与分散"""
    best_bid = mid_price - spread / 2
    best_ask = mid_price + spread / 2
    is_aggressive = random.random() < AGGRESSIVE_LIMIT_ORDER_PROB

    if is_aggressive:
        # 生成一个可能立即成交的价格
        if side == 0: # BUY
            # 价格设定在 best_ask 附近或之上
            price = np.random.normal(best_ask, spread * 0.1) # 在 best_ask 附近小幅波动
            price = max(price, best_ask + 1) # 确保至少比 best_ask 高一点，以促进成交
        else: # SELL
            # 价格设定在 best_bid 附近或之下
            price = np.random.normal(best_bid, spread * 0.1) # 在 best_bid 附近小幅波动
            price = min(price, best_bid - 1) # 确保至少比 best_bid 低一点，以促进成交
    elif random.random() < PRICE_PLACEMENT_NEAR_PROB:
        # 价格靠近最优买卖价 (被动挂单)
        scale = spread * PRICE_PLACEMENT_NEAR_SCALE_RATIO
        if side == 0: # BUY
            price = np.random.normal(best_bid, scale)
            price = min(price, best_bid) # 不高于 best_bid
        else: # SELL
            price = np.random.normal(best_ask, scale)
            price = max(price, best_ask) # 不低于 best_ask
    else:
        # 价格远离最优买卖价 (长尾，被动挂单)
        far_range = spread * PRICE_PLACEMENT_FAR_RANGE_RATIO
        if side == 0: # BUY
            price = random.uniform(mid_price - far_range, best_bid)
        else: # SELL
            price = random.uniform(best_ask, mid_price + far_range)

    return max(1, int(price)) # 确保价格为正整数

# --- 主生成逻辑 ---

if __name__ == "__main__":
    used_order_ids = set()
    # 存储活跃限价单信息: {order_id: (side, price, volume, timestamp)}
    active_limit_orders = {}
    operations = []
    current_mid_price = INITIAL_MID_PRICE
    current_spread = PRICE_SCALE * 0.1 # 初始价差，可以动态调整

    start_time = time.time()
    generated_count = 0

    print("开始生成订单操作...")

    while generated_count < ORDER_COUNT:
        # 动态调整中间价和价差 (加入均值回归)
        deviation = current_mid_price - INITIAL_MID_PRICE
        # 回归力: 将价格拉回初始中间价
        reversion_force = -deviation * MEAN_REVERSION_STRENGTH
        # 随机波动
        random_drift = np.random.normal(0, PRICE_SCALE * PRICE_DRIFT_VOLATILITY)
        # 更新中间价
        current_mid_price += reversion_force + random_drift
        current_mid_price = max(0.5 * INITIAL_MID_PRICE, current_mid_price) # 避免价格过低
        current_spread = max(10, np.random.normal(PRICE_SCALE * 0.1, PRICE_SCALE * 0.02))

        # 决定生成哪种类型的事件
        event_rand = random.random()
        is_market_order_event = event_rand < (1 / (LO_MO_RATIO + 1))

        if is_market_order_event:
            # --- 生成市价单 (Taker) ---
            order_id = generate_order_id(used_order_ids)
            order_side = random.choice([0, 1]) # 0: BUY, 1: SELL
            volume = generate_volume()
            
            # 计算市价单的执行价格 (近似为当前最优买/卖价)
            if order_side == 0: # BUY Market Order -> executes at best ask
                market_price = current_mid_price + current_spread / 2 
            else: # SELL Market Order -> executes at best bid
                market_price = current_mid_price - current_spread / 2
            market_price = max(1, int(market_price)) # 确保价格为正整数

            operations.append({
                'operation': 'MARKET',
                'order_id': order_id,
                'side': order_side,
                'price': market_price, # 市价单价格
                'volume': volume,
                'timestamp': time.time() - start_time
            })
            generated_count += 1
            # 市价单会影响中间价 (简化模拟)
            price_impact = volume * 0.01 # 简单模拟价格影响
            if order_side == 0: # BUY
                current_mid_price += price_impact
            else: # SELL
                current_mid_price -= price_impact

        else:
            # --- 生成限价单相关事件 (Maker) ---
            is_cancel_event = random.random() < CANCEL_RATIO_OF_LO and active_limit_orders

            if is_cancel_event:
                # --- 生成取消订单 ---
                # 选择一个订单进行取消 (考虑时间和距离的简化逻辑)
                candidates = list(active_limit_orders.keys())
                cancel_probs = []
                now = time.time() - start_time
                for oid in candidates:
                    side, price, _, place_time = active_limit_orders[oid]
                    time_elapsed = now - place_time
                    distance = abs(price - current_mid_price)
                    # 距离市场有利方向越远，取消概率越高
                    price_diff_factor = 0
                    if side == 0 and price < current_mid_price: # 买单价格低于中间价
                        price_diff_factor = (current_mid_price - price) / current_spread
                    elif side == 1 and price > current_mid_price: # 卖单价格高于中间价
                        price_diff_factor = (price - current_mid_price) / current_spread

                    prob = CANCEL_BASE_PROB + time_elapsed * CANCEL_TIME_FACTOR + price_diff_factor * CANCEL_DISTANCE_FACTOR
                    cancel_probs.append(prob)

                # 归一化概率
                total_prob = sum(cancel_probs)
                if total_prob > 0:
                    normalized_probs = [p / total_prob for p in cancel_probs]
                    order_id_to_cancel = np.random.choice(candidates, p=normalized_probs)

                    if order_id_to_cancel in active_limit_orders:
                        operations.append({
                            'operation': 'CANCEL',
                            'order_id': order_id_to_cancel,
                            'side': active_limit_orders[order_id_to_cancel][0], # 保留side信息
                            'price': active_limit_orders[order_id_to_cancel][1], # 保留price信息
                            'volume': 0, # 取消操作量为0
                            'timestamp': now
                        })
                        del active_limit_orders[order_id_to_cancel]
                        generated_count += 1
                # else: # 如果没有合适的订单取消，可以跳过或生成一个提交订单

            # 如果不是取消事件，或者没有订单可取消，则生成提交订单
            #if not is_cancel_event or not active_limit_orders or generated_count % (LO_MO_RATIO // (1-CANCEL_RATIO_OF_LO) if (1-CANCEL_RATIO_OF_LO)>0 else LO_MO_RATIO) == 0 : # 保证有提交
            if not is_cancel_event or not active_limit_orders:
                 # --- 生成提交限价单 ---
                order_id = generate_order_id(used_order_ids)
                order_side = random.choice([0, 1]) # 0: BUY, 1: SELL
                price = generate_limit_order_price(current_mid_price, current_spread, order_side)
                volume = generate_volume()
                timestamp = time.time() - start_time

                operations.append({
                    'operation': 'LIMIT',
                    'order_id': order_id,
                    'side': order_side,
                    'price': price,
                    'volume': volume,
                    'timestamp': timestamp
                })
                active_limit_orders[order_id] = (order_side, price, volume, timestamp)
                generated_count += 1

        if generated_count % (ORDER_COUNT // 10) == 0 and generated_count > 0:
             print(f"已生成 {generated_count}/{ORDER_COUNT} 个操作...")


    print(f"生成完成，耗时: {time.time() - start_time:.2f} 秒")

    # --- 写入 CSV 文件 ---
    csv_filename = r'E:/Project/OrderBook-master/demo/sample_operations.csv'
    print(f"正在写入 CSV 文件: {csv_filename}")
    with open(csv_filename, 'w', newline='') as csvfile:
        fieldnames = ['Operation', 'Order ID', 'Type', 'Price', 'Volume']
        csvwriter = csv.DictWriter(csvfile, fieldnames=fieldnames)
        csvwriter.writeheader()
        for op in operations:
            # 格式化输出
            op_data = {
                'Operation': 'DELETE' if op['operation'] == 'CANCEL' else 'PLACE',
                'Order ID': op['order_id'],
                'Type': op['side'],
                'Price': op['price'], 
                'Volume': max(1, int(op['volume'])),
            }
            # 对CANCEL操作，Price和Volume可能不需要，但保留side和price信息可能有用
            if op['operation'] == 'CANCEL':
                 op_data['Volume'] = '' # 取消操作量为空
                 op_data['Price'] = '' # 也可以清空价格
                 op_data['Type'] = ''

            csvwriter.writerow(op_data)

    print(f"CSV 文件 '{csv_filename}' 已生成，包含 {len(operations)} 个操作。")
