#pragma once

#include <cmath>
#include <algorithm>

struct Vec2 {
	float x = 0.0f;
	float y = 0.0f;

	Vec2() = default;
	Vec2(float x_, float y_) : x(x_), y(y_) {}

	Vec2 operator+(const Vec2& o) const { return { x + o.x, y + o.y }; }
	Vec2 operator-(const Vec2& o) const { return { x - o.x, y - o.y }; }
	Vec2 operator*(float s) const { return { x * s, y * s }; }
	Vec2 operator/(float s) const { return { x / s, y / s }; }

	float lengthSq() const { return x * x + y * y; }
	float length() const { return std::sqrt(lengthSq()); }

	Vec2 normalized() const {
		float len = length();
		return len > 0.0001f ? (*this / len) : Vec2{ 0, 0 };
	}
};

class Cycle; // Forward declaration

struct LineSegment {
	Vec2 a;
	Vec2 b;
	const Cycle* owner = nullptr;
};

struct RectF {
	float x = 0.0f;
	float y = 0.0f;
	float w = 0.0f;
	float h = 0.0f;

	bool contains(Vec2 p) const {
		return p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h;
	}
};

namespace geom {

inline float distanceSq(Vec2 p1, Vec2 p2) {
	float dx = p1.x - p2.x;
	float dy = p1.y - p2.y;
	return dx * dx + dy * dy;
}

inline float distance(Vec2 p1, Vec2 p2) {
	return std::sqrt(distanceSq(p1, p2));
}

// Tests intersection of segment (a1->a2) with segment (b1->b2).
// Returns true on intersection and fills outHit and outT (param along a1->a2 in [0, 1]).
bool intersectSegments(Vec2 a1, Vec2 a2, Vec2 b1, Vec2 b2, Vec2* outHit = nullptr, float* outT = nullptr);

// Tests ray (origin + t * dir, 0 <= t <= maxDist) against segment (segA->segB).
// Returns true if hit and fills outDist.
bool rayIntersectSegment(Vec2 origin, Vec2 dir, float maxDist, Vec2 segA, Vec2 segB, float* outDist = nullptr);

// Tests ray against axis-aligned box.
bool rayIntersectAABB(Vec2 origin, Vec2 dir, float maxDist, const RectF& box, float* outDist = nullptr);

// Tests if line segment (a->b) intersects axis-aligned box.
bool segmentIntersectAABB(Vec2 a, Vec2 b, const RectF& box);

} // namespace geom
