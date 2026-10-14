#include "ai/arenaView.h"
#include "cycle.h"

#include <algorithm>
#include <cmath>

RaycastHit ArenaView::raycast(Vec2 origin, Vec2 dir, float maxDist, const Cycle* self, float safeSelfDist) const {
	RaycastHit closest;
	closest.distance = maxDist;

	// 1. Boundary walls
	const Vec2 bounds[4][2] = {
		{ { 0, 0 }, { worldWidth, 0 } },
		{ { 0, worldHeight }, { worldWidth, worldHeight } },
		{ { 0, 0 }, { 0, worldHeight } },
		{ { worldWidth, 0 }, { worldWidth, worldHeight } }
	};
	for (const auto& edge : bounds) {
		float dist = 0.0f;
		if (geom::rayIntersectSegment(origin, dir, closest.distance, edge[0], edge[1], &dist)) {
			if (dist > 0.001f && dist < closest.distance) {
				closest.hit = true;
				closest.distance = dist;
				closest.point = origin + dir * dist;
				closest.hitCycle = nullptr;
			}
		}
	}

	// 2. Obstacle walls
	for (const RectF& wall : walls) {
		float dist = 0.0f;
		if (geom::rayIntersectAABB(origin, dir, closest.distance, wall, &dist)) {
			if (dist > 0.001f && dist < closest.distance) {
				closest.hit = true;
				closest.distance = dist;
				closest.point = origin + dir * dist;
				closest.hitCycle = nullptr;
			}
		}
	}

	// 3. Mine traps
	for (const Vec2& mine : mines) {
		const RectF mineBox = { mine.x - 8.0f, mine.y - 8.0f, 16.0f, 16.0f };
		float dist = 0.0f;
		if (geom::rayIntersectAABB(origin, dir, closest.distance, mineBox, &dist)) {
			if (dist > 0.001f && dist < closest.distance) {
				closest.hit = true;
				closest.distance = dist;
				closest.point = origin + dir * dist;
				closest.hitCycle = nullptr;
			}
		}
	}

	// 4. Trail segments
	for (const LineSegment& seg : segments) {
		if (seg.owner == self) {
			if (geom::distanceSq(origin, seg.a) < safeSelfDist * safeSelfDist
				&& geom::distanceSq(origin, seg.b) < safeSelfDist * safeSelfDist) {
				continue;
			}
		}

		float dist = 0.0f;
		if (geom::rayIntersectSegment(origin, dir, closest.distance, seg.a, seg.b, &dist)) {
			if (dist > 0.001f && dist < closest.distance) {
				closest.hit = true;
				closest.distance = dist;
				closest.point = origin + dir * dist;
				closest.hitCycle = seg.owner;
			}
		}
	}

	return closest;
}

float ArenaView::evaluateClearance(Vec2 origin, Vec2 dir, float maxDist, const Cycle* self, float safeSelfDist) const {
	RaycastHit hit = raycast(origin, dir, maxDist, self, safeSelfDist);
	return hit.hit ? hit.distance : maxDist;
}
