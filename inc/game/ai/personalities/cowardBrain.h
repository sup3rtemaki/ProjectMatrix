#pragma once

#include "ai/enemyBrain.h"

class CowardBrain : public EnemyBrain {
public:
	Direction decide(const Cycle& self, const Cycle& player, const ArenaView& arena) override {
		if (distance(self.position(), player.position()) < FLEE_DISTANCE) {
			return awayFrom(self, arena, player.position());
		}
		return wander(self, arena, 90);
	}

private:
	static constexpr int FLEE_DISTANCE = 20;
};
