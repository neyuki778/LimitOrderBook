#include <iostream>
#include "Limit.h"

void Limit::insert_order(OrderPointer order) { // Accept raw pointer
	if (length == 0) {
		head = tail = order;
	} else {
		tail->set_next(order);
		order->set_prev(tail);
		tail = order;
	}
	total_volume += order->get_volume();
	length++;
}

void Limit::delete_order(OrderPointer order) { // Accept raw pointer
	if (not order) return;

	if (length == 1) { // only order in the list
		head = nullptr; // Use nullptr for raw pointers
		tail = nullptr;
	} else if (order == head) { // order at head of list
		head = order->get_next();
		// No reset needed for raw pointers, just set the pointer
		if (order->get_next()) { // Check if next exists
			order->get_next()->set_prev(nullptr);
		}
	} else if (order == tail) { // order at tail of list
		tail = order->get_prev();
		// No reset needed for raw pointers, just set the pointer
		if (order->get_prev()) { // Check if prev exists
			order->get_prev()->set_next(nullptr);
		}
	} else { // order in the list
		order->get_prev()->set_next(order->get_next());
		order->get_next()->set_prev(order->get_prev());
	}

	// No reset needed for raw pointers, just clear the pointers in the deleted order
	order->set_prev(nullptr);
	order->set_next(nullptr);
	if (order->get_status() != FULFILLED)
		order->set_status(DELETED);
	total_volume -= order->get_volume();
	length--;
}

// Note: The Book class is responsible for destroying orders now.
// Limit::match_order should return the list of fulfilled orders to the Book.
// However, for minimal changes now, we'll assume Book handles destruction elsewhere
// based on the returned Trades. A better approach might involve returning fulfilled order IDs/pointers.
Trades Limit::match_order(OrderPointer order) { // Accept raw pointer
	Trades trades;

	while (length > 0 and not order->is_fulfilled()) {
		Volume fill_volume = std::min(head->get_volume(), order->get_volume());
		head->fill(fill_volume);
		order->fill(fill_volume);
		total_volume -= fill_volume;
		trades.emplace_back(order->get_id(), head->get_id(), price, fill_volume);
		if (head->is_fulfilled()) {
			Order* fulfilled_order = head; // Store pointer before deleting from list
			delete_order(fulfilled_order); // Remove from this limit's list
			// IMPORTANT: The actual destruction (calling pool.destroy())
			// should happen in the Book class after processing trades.
			// This function (Limit::match_order) doesn't own the pool.
			// We rely on the caller (Book::place_order) to handle destruction.
		}
	}

	return trades;
}

bool Limit::is_empty() {
	return length == 0;
}

Price Limit::get_price() const { return price; }
Length Limit::get_length() { return length; }
Volume Limit::get_total_volume() { return total_volume; }

void Limit::print() {
	std::cout << "Limit(Price: " << price << ", Total Volume: "<< total_volume << ", #Orders: " << length << "):" << std::endl;
	Order* curr = head; // Use raw pointer
	while (curr) {
		std::cout << "\t";
		curr->print();
		curr = curr->get_next();
	}
}

// This comparator might not be used anymore if LimitPointer is just Limit*
// If it's used with std::set or similar, it needs to compare Limit*
// For now, commenting it out as its usage isn't clear from the provided code snippets.
// const std::function<bool(const Limit*&, const Limit*&)> cmp_limits = [](const Limit* a, const Limit* b) {
//	 return a->get_price() < b->get_price();
// };
