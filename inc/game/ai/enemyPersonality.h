#pragma once

#include "ai/enemyBrain.h"

#include <memory>

enum class EnemyPersonality {
	Aggressor,  // chases the player's current position
	Ambusher,   // aims ahead of the player to cut them off
	Strategist, // plays for survival, always heading for open space
	Coward,     // wanders, but runs from the player when close
	Erratic     // mostly random
};

std::unique_ptr<EnemyBrain> createEnemyBrain(EnemyPersonality personality);
