#pragma once

#include "ai/enemyBrain.h"

class StrategistBrain : public EnemyBrain {
protected:
	Direction act(const Cycle& self, const Cycle&, const ArenaView& arena) override {
		return mostOpenSpace(self, arena);
	}

	// Plans ahead: wants every power-up and sees farther.
	float pickupAppetite(ObjectType type, const Cycle&, const Cycle&) const override {
		return preference(type, 0.8f, 0.8f);
	}

	int pickupSightRange() const override { return 24; }
};
