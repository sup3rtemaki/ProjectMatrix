#pragma once

#include "collision/geometry2D.h"
#include "world/pickupSighting.h"

#include <vector>

class Cycle;

struct RaycastHit {
	bool hit = false;
	float distance = 0.0f;
	Vec2 point;
	const Cycle* hitCycle = nullptr;
};

// Continuous representation of the arena handed to enemy brains.
struct ArenaView {
	float worldWidth = 0.0f;
	float worldHeight = 0.0f;
	std::vector<RectF> walls;
	std::vector<Vec2> mines;
	std::vector<LineSegment> segments; // all active trail segments from all cycles
	std::vector<Vec2> chaseTargets; // all alive bikes and the player, used for pursuit selection
	const std::vector<PickupSighting>& pickups;

	// Casts a ray from origin in dir up to maxDist, ignoring self's recent trail (within safeSelfDist).
	RaycastHit raycast(Vec2 origin, Vec2 dir, float maxDist, const Cycle* self, float safeSelfDist = 24.0f) const;

	// Returns clearance distance [0, maxDist] along dir.
	float evaluateClearance(Vec2 origin, Vec2 dir, float maxDist, const Cycle* self, float safeSelfDist = 24.0f) const;
};
