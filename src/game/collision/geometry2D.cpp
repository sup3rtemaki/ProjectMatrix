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

OrientedBox orientedBox(const RectF& local, Vec2 origin, float radians) {
	const float c = std::cos(radians);
	const float s = std::sin(radians);
	const Vec2 axisX = { c, s };
	const Vec2 axisY = { -s, c };
	return { origin + axisX * (local.x + local.w * 0.5f) + axisY * (local.y + local.h * 0.5f),
		axisX, axisY, local.w * 0.5f, local.h * 0.5f };
}

static float dot(Vec2 a, Vec2 b) {
	return a.x * b.x + a.y * b.y;
}

static float projectionRadius(const OrientedBox& box, Vec2 axis) {
	return box.halfWidth * std::abs(dot(box.axisX, axis)) + box.halfHeight * std::abs(dot(box.axisY, axis));
}

static bool sweepAxis(float center, float radius, float velocity, float otherMin, float otherMax,
	float& first, float& last) {
	if (std::abs(velocity) < 1e-6f) {
		return center + radius >= otherMin && center - radius <= otherMax;
	}
	float enter = (otherMin - center - radius) / velocity;
	float exit = (otherMax - center + radius) / velocity;
	if (enter > exit) std::swap(enter, exit);
	first = std::max(first, enter);
	last = std::min(last, exit);
	return first <= last;
}

bool sweptBoxIntersectsBox(const OrientedBox& moving, Vec2 displacement, const OrientedBox& other) {
	const Vec2 axes[] = { moving.axisX, moving.axisY, other.axisX, other.axisY };
	float first = 0.0f, last = 1.0f;
	for (Vec2 axis : axes) {
		const float middle = dot(other.center, axis);
		const float radius = projectionRadius(other, axis);
		if (!sweepAxis(dot(moving.center, axis), projectionRadius(moving, axis), dot(displacement, axis),
			middle - radius, middle + radius, first, last)) return false;
	}
	return true;
}

bool sweptBoxIntersectsSegment(const OrientedBox& moving, Vec2 displacement, Vec2 a, Vec2 b) {
	const Vec2 edge = b - a;
	const Vec2 axes[] = { moving.axisX, moving.axisY, { -edge.y, edge.x } };
	float first = 0.0f, last = 1.0f;
	for (Vec2 axis : axes) {
		if (axis.lengthSq() < 1e-10f) continue;
		const float pa = dot(a, axis), pb = dot(b, axis);
		if (!sweepAxis(dot(moving.center, axis), projectionRadius(moving, axis), dot(displacement, axis),
			std::min(pa, pb), std::max(pa, pb), first, last)) return false;
	}
	return true;
}

bool boxInsideRect(const OrientedBox& box, const RectF& rect) {
	const float rx = projectionRadius(box, { 1.0f, 0.0f });
	const float ry = projectionRadius(box, { 0.0f, 1.0f });
	return box.center.x - rx >= rect.x && box.center.x + rx <= rect.x + rect.w
		&& box.center.y - ry >= rect.y && box.center.y + ry <= rect.y + rect.h;
}

} // namespace geom
