#pragma once

#include "SDL.h"

#include <cstdint>
#include <vector>

// Read-only snapshot of the arena grid handed to enemy brains.
struct ArenaView {
	int width;
	int height;
	const std::vector<std::uint8_t>& occupied;

	bool isBlocked(int x, int y) const;
	// Free cells reachable from start (flood fill), counted up to limit.
	int reachableCells(SDL_Point start, int limit) const;
};
