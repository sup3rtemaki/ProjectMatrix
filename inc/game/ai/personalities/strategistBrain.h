#pragma once

#include "ai/enemyBrain.h"

class StrategistBrain : public EnemyBrain {
public:
	Direction decide(const Cycle& self, const Cycle&, const ArenaView& arena) override {
		return mostOpenSpace(self, arena);
	}
};
