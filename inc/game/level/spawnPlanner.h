#pragma once

#include "level/levelDefinition.h"

#include <vector>

struct ResolvedSpawn {
	SDL_Point cell;
	Direction direction;
};

// Concrete spawns for the player (index 0) followed by the enemies, in level order.
// Spawns the level leaves open are chosen apart from each other and from fixed walls.
std::vector<ResolvedSpawn> resolveSpawns(const LevelDefinition& level);
