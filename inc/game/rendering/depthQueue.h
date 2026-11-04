#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <vector>

// Collects draw calls and runs them from the smallest Y to the largest, so what is lower on the screen
// is drawn over what is higher up. Ties keep the order they were added in.
class DepthQueue {
public:
	void add(float sortY, std::function<void()> draw) {
		items_.push_back({ sortY, std::move(draw) });
	}

	void flush() {
		std::stable_sort(items_.begin(), items_.end(),
			[](const Item& a, const Item& b) { return a.sortY < b.sortY; });
		for (const Item& item : items_) {
			item.draw();
		}
		items_.clear();
	}

private:
	struct Item {
		float sortY;
		std::function<void()> draw;
	};

	std::vector<Item> items_;
};
