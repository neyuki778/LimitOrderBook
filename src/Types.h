#ifndef ORDERBOOK_TYPES_H
#define ORDERBOOK_TYPES_H

#include <cstdint>

// 增加易读性
using ID = std::uint64_t;
using Price = std::uint32_t;
using Volume = std::uint64_t;
using Length = std::uint64_t;

// 枚举，暂时不明白优点
enum OrderType { BUY, SELL };

enum OrderStatus { ACTIVE, FULFILLED, DELETED };

#endif //ORDERBOOK_TYPES_H
