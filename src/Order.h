#ifndef ORDERBOOK_ORDER_H
#define ORDERBOOK_ORDER_H

#include "Types.h"

class Order; // Forward declaration
using OrderPointer = Order*; // Use raw pointer

class Order {
private:
	ID id; /**< Order id */
	ID agent_id; /**< id of the agent who placed the order */
	OrderType type; /**< Order type (buy/sell) */
	Price price; /**< Limit price of the order */
	Volume initial_volume; /**< Initial volume/number of shares in the order */
	Volume volume; /**< Volume/number of remaining shares in the order */
	OrderStatus status;  /**< Current status of the order */

	/** As orders are stored in a doubly-linked list, each order stores its previous and next orders */
	Order* prev; /**< Previous order in the list */
	Order* next; /**< Next order in the list */

public:
	Order(ID id, ID agent_id, OrderType type, Price price, Volume volume):
			id(id), agent_id(agent_id), type(type), price(price), initial_volume(volume), volume(volume), status(ACTIVE), prev(nullptr), next(nullptr) {}

	/**
	 * @brief fills the order with a given volume (quantity)
	 * @param fill_volume: volume to fill
	 */
	void fill(Volume fill_volume);
	/**
	 * @brief Checks if the order is fulfilled
	 * @return true if the order is fulfilled false otherwise
	 */
	bool is_fulfilled();

	/** Getters and setters */
	ID get_id() const;
	ID get_agent_id() const;
	OrderType get_type() const;
	Price get_price() const;
	Volume get_volume();
	OrderStatus get_status();
	void set_status(OrderStatus status);
	Order* get_prev(); // Return raw pointer
	void set_prev(Order* prev); // Accept raw pointer
	Order* get_next(); // Return raw pointer
	void set_next(Order* next); // Accept raw pointer

	/** Print method */
	void print();
};

// using OrderPointer = Order*; // Replaced by alias at line 7

#endif //ORDERBOOK_ORDER_H
