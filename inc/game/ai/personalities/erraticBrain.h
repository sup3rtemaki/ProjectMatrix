#pragma once

#include "ai/enemyBrain.h"

class ErraticBrain : public EnemyBrain {
public:
	Direction decide(const Cycle& self, const Cycle&, const ArenaView& arena) override {
		return wander(self, arena, 70);
	}
};
