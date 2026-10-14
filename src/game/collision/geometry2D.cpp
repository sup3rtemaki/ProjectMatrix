#include "collision/geometry2D.h"

namespace geom {

static inline float cross2D(Vec2 u, Vec2 v) {
	return u.x * v.y - u.y * v.x;
}

bool intersectSegments(Vec2 a1, Vec2 a2, Vec2 b1, Vec2 b2, Vec2* outHit, float* outT) {
	const Vec2 r = a2 - a1;
	const Vec2 s = b2 - b1;
	const float denom = cross2D(r, s);

	if (std::abs(denom) < 1e-6f) {
		return false; // Collinear or parallel
	}

	const Vec2 diff = b1 - a1;
	const float t = cross2D(diff, s) / denom;
	const float u = cross2D(diff, r) / denom;

	if (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f) {
		if (outHit != nullptr) {
			*outHit = a1 + r * t;
		}
		if (outT != nullptr) {
			*outT = t;
		}
		return true;
	}
	return false;
}

bool rayIntersectSegment(Vec2 origin, Vec2 dir, float maxDist, Vec2 segA, Vec2 segB, float* outDist) {
	const Vec2 s = segB - segA;
	const float denom = cross2D(dir, s);

	if (std::abs(denom) < 1e-6f) {
		return false;
	}

	const Vec2 diff = segA - origin;
	const float t = cross2D(diff, s) / denom;
	const float u = cross2D(diff, dir) / denom;

	if (t >= 0.0f && t <= maxDist && u >= 0.0f && u <= 1.0f) {
		if (outDist != nullptr) {
			*outDist = t;
		}
		return true;
	}
	return false;
}

bool rayIntersectAABB(Vec2 origin, Vec2 dir, float maxDist, const RectF& box, float* outDist) {
	float tmin = 0.0f;
	float tmax = maxDist;

	// X slab
	if (std::abs(dir.x) < 1e-6f) {
		if (origin.x < box.x || origin.x > box.x + box.w) {
			return false;
		}
	}
	else {
		float invDirX = 1.0f / dir.x;
		float t1 = (box.x - origin.x) * invDirX;
		float t2 = (box.x + box.w - origin.x) * invDirX;
		if (t1 > t2) std::swap(t1, t2);
		tmin = std::max(tmin, t1);
		tmax = std::min(tmax, t2);
		if (tmin > tmax) return false;
	}

	// Y slab
	if (std::abs(dir.y) < 1e-6f) {
		if (origin.y < box.y || origin.y > box.y + box.h) {
			return false;
		}
	}
	else {
		float invDirY = 1.0f / dir.y;
		float t1 = (box.y - origin.y) * invDirY;
		float t2 = (box.y + box.h - origin.y) * invDirY;
		if (t1 > t2) std::swap(t1, t2);
		tmin = std::max(tmin, t1);
		tmax = std::min(tmax, t2);
		if (tmin > tmax) return false;
	}

	if (outDist != nullptr) {
		*outDist = tmin;
	}
	return true;
}

bool segmentIntersectAABB(Vec2 a, Vec2 b, const RectF& box) {
	if (box.contains(a) || box.contains(b)) {
		return true;
	}

	const Vec2 p1 = { box.x, box.y };
	const Vec2 p2 = { box.x + box.w, box.y };
	const Vec2 p3 = { box.x + box.w, box.y + box.h };
	const Vec2 p4 = { box.x, box.y + box.h };

	return intersectSegments(a, b, p1, p2)
		|| intersectSegments(a, b, p2, p3)
		|| intersectSegments(a, b, p3, p4)
		|| intersectSegments(a, b, p4, p1);
}

} // namespace geom
