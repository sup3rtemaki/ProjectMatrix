#pragma once

#include "collision/geometry2D.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

class WallIndex {
public:
	void reset(int width, int height, int cellSize) {
		width_ = (width * cellSize + BUCKET_SIZE - 1) / BUCKET_SIZE;
		height_ = (height * cellSize + BUCKET_SIZE - 1) / BUCKET_SIZE;
		boxes_.clear();
		buckets_.assign(static_cast<std::size_t>(width_) * height_, {});
	}

	void add(RectF box) {
		const std::size_t index = boxes_.size();
		boxes_.push_back(box);
		int left, top, right, bottom;
		if (!range(box, left, top, right, bottom)) return;
		for (int y = top; y <= bottom; ++y) {
			for (int x = left; x <= right; ++x) buckets_[static_cast<std::size_t>(y) * width_ + x].push_back(index);
		}
	}

	const std::vector<RectF>& boxes() const { return boxes_; }

	std::vector<std::size_t> query(RectF area) const {
		std::vector<std::size_t> result;
		int left, top, right, bottom;
		if (!range(area, left, top, right, bottom)) return result;
		for (int y = top; y <= bottom; ++y) {
			for (int x = left; x <= right; ++x) {
				const auto& bucket = buckets_[static_cast<std::size_t>(y) * width_ + x];
				result.insert(result.end(), bucket.begin(), bucket.end());
			}
		}
		// Preserve insertion order for equal-distance hits and visit large walls only once.
		std::sort(result.begin(), result.end());
		result.erase(std::unique(result.begin(), result.end()), result.end());
		return result;
	}

private:
	static constexpr int BUCKET_SIZE = 128;
	bool range(RectF area, int& left, int& top, int& right, int& bottom) const {
		if (width_ <= 0 || height_ <= 0 || area.x + area.w < 0 || area.y + area.h < 0
			|| area.x > static_cast<float>(width_ * BUCKET_SIZE)
			|| area.y > static_cast<float>(height_ * BUCKET_SIZE)) return false;
		left = std::clamp(static_cast<int>(std::floor(area.x / BUCKET_SIZE)), 0, width_ - 1);
		top = std::clamp(static_cast<int>(std::floor(area.y / BUCKET_SIZE)), 0, height_ - 1);
		right = std::clamp(static_cast<int>(std::floor((area.x + area.w) / BUCKET_SIZE)), 0, width_ - 1);
		bottom = std::clamp(static_cast<int>(std::floor((area.y + area.h) / BUCKET_SIZE)), 0, height_ - 1);
		return left <= right && top <= bottom;
	}

	int width_ = 0;
	int height_ = 0;
	std::vector<RectF> boxes_;
	std::vector<std::vector<std::size_t>> buckets_;
};

inline RectF sweptBounds(const OrientedBox& box, Vec2 displacement) {
	const float radiusX = std::abs(box.axisX.x) * box.halfWidth + std::abs(box.axisY.x) * box.halfHeight;
	const float radiusY = std::abs(box.axisX.y) * box.halfWidth + std::abs(box.axisY.y) * box.halfHeight;
	constexpr float margin = 0.01f;
	return { box.center.x - radiusX + std::min(displacement.x, 0.0f) - margin,
		box.center.y - radiusY + std::min(displacement.y, 0.0f) - margin,
		2.0f * radiusX + std::abs(displacement.x) + 2.0f * margin,
		2.0f * radiusY + std::abs(displacement.y) + 2.0f * margin };
}
