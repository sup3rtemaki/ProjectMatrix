#pragma once

#include "ai/enemyBrain.h"

class ErraticBrain : public EnemyBrain {
protected:
	Direction act(const Cycle& self, const Cycle&, const ArenaView& arena) override {
		return wander(self, arena, 70);
	}

	// Half-hearted about everything.
	float pickupAppetite(ObjectType type, const Cycle&, const Cycle&) const override {
		return preference(type, 0.5f, 0.5f);
	}

	float pickupSightRange() const override { return 100.0f; }
};
