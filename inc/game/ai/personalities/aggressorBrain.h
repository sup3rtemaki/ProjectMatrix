#pragma once

#include "ai/enemyBrain.h"

class AggressorBrain : public EnemyBrain {
protected:
	Direction act(const Cycle& self, const Cycle& player, const ArenaView& arena) override {
		return towards(self, arena, player.position());
	}

	// Likes speed to run the player down.
	float pickupAppetite(ObjectType type, const Cycle&, const Cycle&) const override {
		return preference(type, 1.0f, 0.4f);
	}

	int pickupSightRange() const override { return 20; }
};
