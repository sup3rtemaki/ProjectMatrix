#pragma once

#include "ai/enemyBrain.h"

class CowardBrain : public EnemyBrain {
protected:
	Direction act(const Cycle& self, const Cycle& player, const ArenaView& arena) override {
		if (geom::distance(self.center(), player.center()) < FLEE_DISTANCE) {
			return awayFrom(self, arena, player.center());
		}
		return wander(self, arena, 90);
	}

	// Ignores power-ups while the player is close, running comes first.
	float pickupAppetite(ObjectType type, const Cycle& self, const Cycle& player) const override {
		if (geom::distance(self.center(), player.center()) < FLEE_DISTANCE) {
			return 0.0f;
		}
		return preference(type, 1.0f, 0.5f);
	}

	float pickupSightRange() const override { return 120.0f; }

private:
	static constexpr float FLEE_DISTANCE = 150.0f;
};
