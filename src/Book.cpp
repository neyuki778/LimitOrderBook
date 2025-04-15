#include "Book.h"
#include<iostream>

// Trades Book::place_order(OrderPointer& order) {
// 	if (order->get_price() <= 0)
// 		return {};

// 	Trades trades;
// 	id_to_order[order->get_id()] = order;

// 	if (order->get_type() == BUY) {
// 		while (best_sell and order->get_price() >= best_sell and order->get_status() != FULFILLED) {
// 			Trades trades_at_limit = sell_limits[best_sell]->match_order(order);
// 			trades.insert(trades.end(), trades_at_limit.begin(), trades_at_limit.end());
// 			check_for_empty_sell_limit(best_sell);
// 		}
// 	} else {
// 		while (best_buy and order->get_price() <= best_buy and order->get_status() != FULFILLED) {
// 			Trades trades_at_limit = buy_limits[best_buy]->match_order(order);
// 			trades.insert(trades.end(), trades_at_limit.begin(), trades_at_limit.end());
// 			check_for_empty_buy_limit(best_buy);
// 		}
// 	}

// 	if (order->get_status() != FULFILLED)
// 		insert_order(order);
// 	return trades;
// }

Trades Book::place_order(ID id, ID agent_id, OrderType type, Price price, Volume volume) {
	// --- 使用内存池创建 Order 对象 (使用 construct 获取裸指针) ---
	// 价格检查
    if (price <= 0)
		return {};

    // --- 使用内存池创建 Order 对象 ---
    // 1. 检查是否已存在相同 ID 的订单 (可选但推荐)
    if (id_to_order.count(id)) {
         // 处理重复订单 ID 的情况，例如返回错误或忽略
         std::cerr << "Warning: Order ID " << id << " already exists." << std::endl;
         return {};
    }

	// 2. 使用 order_pool.malloc + placement new 分配和构造 Order 对象
	Order* order = static_cast<Order*>(order_pool.malloc());
	if (!order) {
		// 处理内存分配失败
		throw std::bad_alloc();
	}
	try {
		// 使用 placement new 在分配的内存上构造对象
		new(order) Order(id, agent_id, type, price, volume);
	} catch (...) {
		// 如果构造失败，释放内存并重新抛出异常
		order_pool.free(order);
		throw;
	}
	// No shared_ptr or custom deleter needed anymore
	   // --- End Order 对象创建 ---


    // --- 接下来的逻辑与之前类似，但使用新创建的 'order' ---
	Trades trades;
	// id_to_order[order->get_id()] = order; // Moved to later, only if order rests

	if (order->get_type() == BUY) {
		// 注意：这里 price 来自参数，不再是 order->get_price()，因为 order 是新创建的
        // 但逻辑上应该是一样的，使用 order->get_price() 也没问题
		while (best_sell != 0 && order->get_price() >= best_sell && order->get_status() != FULFILLED) {
			// get_or_create_limit 内部已修改为使用 pool
			Limit* sell_limit = get_or_create_limit(best_sell, false); // 获取 Limit* 指针
            if (!sell_limit) continue; // 防御性编程

			Trades trades_at_limit = sell_limit->match_order(order); // 使用 Limit* 对象
			trades.insert(trades.end(), trades_at_limit.begin(), trades_at_limit.end());

			// --- 销毁在此 Limit 被完全撮合的订单 ---
			for (const auto& trade : trades_at_limit) {
				ID matched_id = trade.get_matched_order();
				auto it = id_to_order.find(matched_id);
				if (it != id_to_order.end()) {
					Order* matched_order_ptr = it->second;
					if (matched_order_ptr->get_status() == FULFILLED) {
						id_to_order.erase(it); // 从 map 中移除
						order_pool.destroy(matched_order_ptr); // 归还给内存池
					}
				}
			}
			// --- End 销毁 ---

			check_for_empty_sell_limit(best_sell); // 检查 Limit 是否变空 (内部会 destroy Limit)
		}
	} else { // SELL order
		while (best_buy != 0 && order->get_price() <= best_buy && order->get_status() != FULFILLED) {
            Limit* buy_limit = get_or_create_limit(best_buy, true); // 获取 Limit* 指针
            if (!buy_limit) continue;

			Trades trades_at_limit = buy_limit->match_order(order); // 使用 Limit* 对象
			trades.insert(trades.end(), trades_at_limit.begin(), trades_at_limit.end());

			// --- 销毁在此 Limit 被完全撮合的订单 ---
			for (const auto& trade : trades_at_limit) {
				ID matched_id = trade.get_matched_order();
				auto it = id_to_order.find(matched_id);
				if (it != id_to_order.end()) {
					Order* matched_order_ptr = it->second;
					if (matched_order_ptr->get_status() == FULFILLED) {
						id_to_order.erase(it); // 从 map 中移除
						order_pool.destroy(matched_order_ptr); // 归还给内存池
					}
				}
			}
			// --- End 销毁 ---

			check_for_empty_buy_limit(best_buy); // 检查 Limit 是否变空 (内部会 destroy Limit)
		}
	}

	// 如果订单未完全成交，将其插入对应的 Limit
	if (order->get_status() != FULFILLED) {
		// Order is resting, add it to the map and the limit
		id_to_order[order->get_id()] = order; // Add resting order to map
		insert_order(order); // insert_order now takes Order*
	} else {
		// Incoming order was fully matched and never rested, destroy it
		order->~Order(); // Explicitly call destructor for placement new
		order_pool.free(order); // Return memory to pool
	}
    // --- End 之前逻辑 ---

	return trades;
}

void Book::delete_order(ID id) {
	if (not id_to_order.contains(id))
		return;
	auto it = id_to_order.find(id);
	if (it == id_to_order.end()) {
		// Order not found in map, might have been fulfilled already or never existed
		return;
	}
	Order* order = it->second;
	/*if (not buy_limits.contains(order->get_price()) and not sell_limits.contains(order->get_price()))
		return;*/
	if (order->get_status() == ACTIVE) {
		delete_order(order, order->get_type() == BUY); // Remove from Limit list
		id_to_order.erase(it); // Remove from map
		order->~Order(); // Explicitly call destructor
		order_pool.free(order); // Return memory to pool
	}
	// If status is not ACTIVE (e.g., FULFILLED), it should have been removed already.
	// If it's DELETED, it means it was already processed for deletion.
}


bool Book::is_in_buy_limits(Price price) {
	return buy_limits.contains(price);
}

bool Book::is_in_sell_limits(Price price) {
	return sell_limits.contains(price);
}

void Book::update_best_buy() {
	if (not buy_tree.empty())
		best_buy = *buy_tree.rbegin();
	else
		best_buy = 0;
}

void Book::update_best_sell() {
	if (not sell_tree.empty())
		best_sell = *sell_tree.begin();
	else
		best_sell = 0;
}

void Book::check_for_empty_buy_limit(Price price) {
	if (is_in_buy_limits(price) and buy_limits[price]->is_empty()) {
		Limit* limit_to_delete = buy_limits[price]; // Get pointer before erasing
		buy_limits.erase(price);
		buy_tree.erase(price);
		limit_to_delete->~Limit(); // Explicitly call destructor
		limit_pool.free(limit_to_delete); // Return memory to pool
		if (price == best_buy)
			update_best_buy();
	}
}

void Book::check_for_empty_sell_limit(Price price) {
	if (is_in_sell_limits(price) and sell_limits[price]->is_empty()) {
		Limit* limit_to_delete = sell_limits[price]; // Get pointer before erasing
		sell_limits.erase(price);
		sell_tree.erase(price);
		limit_to_delete->~Limit(); // Explicitly call destructor
		limit_pool.free(limit_to_delete); // Return memory to pool
		if (price == best_sell)
			update_best_sell();
	}
}

void Book::insert_order(Order* order) { // Accept raw pointer
	Price price = order->get_price();
	bool is_buy = order->get_type() == BUY;

	Limit* limit = get_or_create_limit(price, is_buy); // Get raw pointer

	if (!limit) return;

	if (is_buy and (not best_buy or price > best_buy)) {
		best_buy = price;
	} else if (not is_buy and (not best_sell or price < best_sell)) {
		best_sell = price;
	}

	limit->insert_order(order);
}

Limit* Book::get_or_create_limit(Price price, bool is_buy) { // Return raw pointer
	// LimitPointer limit;
	// if (is_buy) {
	// 	if (is_in_buy_limits(price)) {
	// 		limit = buy_limits[price];
	// 	} else {
	// 		limit = std::make_shared<Limit>(price);
	// 		buy_tree.insert(price);
	// 		buy_limits[price] = limit;
	// 	}
	// } else {
	// 	if (is_in_sell_limits(price)) {
	// 		limit = sell_limits[price];
	// 	} else {
	// 		limit = std::make_shared<Limit>(price);
	// 		sell_tree.insert(price);
	// 		sell_limits[price] = limit;
	// 	}
	// }
	// return limit;
	auto& limits = is_buy ? buy_limits : sell_limits;
	auto it = limits.find(price);

	if (it != limits.end()) {
		return it->second; // Limit 已存在，直接返回 Limit*
	} else {
		// Limit 不存在，需要使用内存池创建
		auto& tree = is_buy ? buy_tree : sell_tree;

		// Limit 不存在，使用 limit_pool.malloc + placement new 创建
		Limit* limit = static_cast<Limit*>(limit_pool.malloc());
		if (!limit) {
			throw std::bad_alloc();
		}
		try {
			new(limit) Limit(price); // Placement new
		} catch (...) {
			limit_pool.free(limit);
			throw;
		}

		// 将新的 Limit* 添加到 map 和 tree 中
		limits.emplace(price, limit); // Store raw pointer
		tree.insert(price);

		return limit; // Return raw pointer
	}

}

// Helper function to remove order from its limit list (doesn't destroy the order object)
void Book::delete_order(Order* order, bool is_buy) { // Accept raw pointer
	if (is_buy) {
		if (not is_in_buy_limits(order->get_price()))
			return;
		Limit* limit = buy_limits[order->get_price()];
		if (limit) limit->delete_order(order);
		check_for_empty_buy_limit(order->get_price());
	} else {
		if (not is_in_sell_limits(order->get_price()))
			return;
		Limit* limit = sell_limits[order->get_price()];
		if (limit) limit->delete_order(order);
		check_for_empty_sell_limit(order->get_price());
	}
}

void Book::print() {
	for (Price price : buy_tree){
		buy_limits[price]->print();
	}
	std::cout << "==== BUY SIDE ===" << std::endl;
	std::cout << "Best buy: " << best_buy << std::endl;
	std::cout << "Best sell: " << best_sell << std::endl;
	std::cout << "=== SELL SIDE ===" << std::endl;
	for (Price price : sell_tree){
		sell_limits[price]->print();
	}
}

Price Book::get_spread() {
	return best_sell - best_buy;
}
double Book::get_mid_price() {
	return (best_sell + best_buy) / 2.;
}

PriceTree& Book::get_buy_tree() { return buy_tree; }
PriceLimitMap& Book::get_buy_limits() { return buy_limits; }
PriceTree& Book::get_sell_tree() { return sell_tree; }
PriceLimitMap& Book::get_sell_limits() { return sell_limits; }
Price Book::get_best_buy(){ return best_buy; }
Price Book::get_best_sell() { return best_sell; }
Orders& Book::get_id_to_order() { return id_to_order; }
OrderStatus Book::get_order_status(ID id) {
	auto it = id_to_order.find(id); // Find the order
	if (it != id_to_order.end()) {
		// Order found, return its status
		return it->second->get_status();
	}
	// Order not found in the map (might be fulfilled or deleted)
	return DELETED; // Or potentially another status indicating 'not found'
}
