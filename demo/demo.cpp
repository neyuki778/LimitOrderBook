#include <iostream>
#include <fstream>
// #include <sstream>
#include<charconv>
#include <chrono>
#include "Book.h"

// std::vector<std::string> split(const std::string &line, char delimiter) {
// 	std::vector<std::string> tokens;
// 	std::string token;
// 	std::istringstream tokenStream(line);
// 	while (std::getline(tokenStream, token, delimiter)) {
// 		tokens.push_back(token);
// 	}
// 	return tokens;
// }

// template<typename T>
// T sv_to_num(std::string_view sv) {
//     T value = 0;
//     std::from_chars(sv.data(), sv.data() + sv.size(), value);
//     return value;
// }

int sv_to_num(std::string_view sv) {
    int value = 0;
    std::from_chars(sv.data(), sv.data() + sv.size(), value);
    return value;
}

void split(std::string_view line, char delimiter, std::vector<std::string_view>& out_tokens){
	out_tokens.clear();

	size_t start = 0;
	size_t end = 0;

	while ((end = line.find(delimiter, start)) != std::string_view::npos){
		out_tokens.emplace_back(line.substr(start, end - start));
		start = end + 1;
	}

	if (start < line.size()){
		out_tokens.emplace_back(line.substr(start));
	}
}

void write_final_order_book(Book& book) {
	std::ofstream file("../demo/final_order_book.csv");
	if (!file.is_open()) {
		std::cerr << "Error opening file.\n";
		return;
	}
	file << "Price Limit,Side,Volume\n";
	for (auto& limit: book.get_buy_limits())
		file << limit.first << ",BUY,"<< limit.second->get_total_volume() << "\n";
	for (auto& limit: book.get_sell_limits())
		file << limit.first << ",SELL,"<< limit.second->get_total_volume() << "\n";
}

void demo() {
	// 打开操作指令CSV文件
	// std::string exe_path = std::filesystem::current_path().string();
	// std::string file_path = exe_path + "/demo/sample_operations.csv";
	std::ifstream file("../demo/sample_operations.csv");
	if (!file.is_open()) {
		std::cerr << "Error opening file.\n";
		return;
	}

	// 创建订单簿对象
	Book book;
	// 操作计数器，用于统计性能
	std::uint64_t nb_op = 0;
	// 用于性能计时的时间点变量
	std::chrono::time_point<std::chrono::system_clock> start, end;

	// 读取并跳过CSV文件的标题行
	std::string line;
	std::getline(file, line);

	// 开始计时
	start = std::chrono::system_clock::now();
	while (std::getline(file, line)) {
		// 按逗号分割每行数据
		std::vector<std::string_view> fields;
		fields.reserve(5);
		split(line, ',', fields);

		if (fields[0] == "PLACE") {
			// 创建新订单。参数依次为：订单ID, agent_id(这里设为0), 买卖方向(BUY/SELL), 价格, 数量
			// OrderPointer是std::shared_ptr<Order>的类型别名，用于智能管理Order对象的内存
			// OrderPointer order(new Order(stoi(fields[1]), 0, stoi(fields[2]) == BUY ? BUY : SELL, stoi(fields[3]), stoi(fields[4])));
			OrderPointer order(new Order (sv_to_num(fields[1]),0 ,sv_to_num(fields[2]) == BUY ? BUY : SELL, sv_to_num(fields[3]), sv_to_num(fields[4])));
			// 将订单放入订单簿，返回产生的交易列表
			// Trades是std::vector<Trade>的类型别名，记录了所有因此订单而产生的成交
			Trades trades = book.place_order(order);
			// 统计操作数：每笔交易算一次操作；如果订单状态为ACTIVE，表示它被插入了订单簿，也算一次操作
			nb_op += trades.size() + (order->get_status() == ACTIVE); // each trade is an operation; if the order is active it has been inserted otherwise no operation
		} else {
			// 删除订单（假设其他操作为DELETE）
			book.delete_order(sv_to_num(fields[1]));
			nb_op++;
		}
	}
	// 结束计时
	end = std::chrono::system_clock::now();
	// 计算执行时间（秒）
	std::chrono::duration<double> elapsed_seconds = end - start;

	// 输出性能统计信息
	std::cout << "Elapsed time: " << elapsed_seconds.count() << "s" << std::endl;
	std::cout << "Operations per second: " << (double)nb_op / elapsed_seconds.count() << std::endl;

	// 关闭文件
	file.close();

	// 将最终订单簿状态写入文件
	write_final_order_book(book);
}
