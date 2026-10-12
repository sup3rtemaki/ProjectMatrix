#pragma once

#include "ai/enemyBrain.h"

class AggressorBrain : public EnemyBrain {
public:
	Direction decide(const Cycle& self, const Cycle& player, const ArenaView& arena) override {
		return towards(self, arena, player.position());
	}
};
