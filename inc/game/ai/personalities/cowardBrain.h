#pragma once

#include "ai/enemyBrain.h"

class CowardBrain : public EnemyBrain {
protected:
	Direction act(const Cycle& self, const Cycle& player, const ArenaView& arena) override {
		if (distance(self.position(), player.position()) < FLEE_DISTANCE) {
			return awayFrom(self, arena, player.position());
		}
		return wander(self, arena, 90);
	}

	// Ignores power-ups while the player is close, running comes first.
	float pickupAppetite(ObjectType type, const Cycle& self, const Cycle& player) const override {
		if (distance(self.position(), player.position()) < FLEE_DISTANCE) {
			return 0.0f;
		}
		return preference(type, 1.0f, 0.5f);
	}

	int pickupSightRange() const override { return 14; }

private:
	static constexpr int FLEE_DISTANCE = 20;
};
