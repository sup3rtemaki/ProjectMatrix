#pragma once

#include "SDL.h"
#include "world/pickupSighting.h"

#include <cstdint>
#include <vector>

// Read-only snapshot of the arena grid handed to enemy brains.
struct ArenaView {
	int width;
	int height;
	const std::vector<std::uint8_t>& occupied;
	const std::vector<std::uint8_t>& hazards; // mines, walls and the like
	const std::vector<PickupSighting>& pickups;

	bool isBlocked(int x, int y) const;
	// Free cells reachable from start (flood fill), counted up to limit.
	int reachableCells(SDL_Point start, int limit) const;
};
