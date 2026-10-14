#pragma once

#include "ai/enemyBrain.h"

#include <algorithm>

class AmbusherBrain : public EnemyBrain {
protected:
	Direction act(const Cycle& self, const Cycle& player, const ArenaView& arena) override {
		const Vec2 here = player.center();
		if (geom::distance(self.center(), here) <= CLOSE_RANGE) {
			return towards(self, arena, here);
		}

		const Vec2 ahead = directionVector(player.direction()) * LEAD_DISTANCE;
		const Vec2 target = {
			std::clamp(here.x + ahead.x, 16.0f, arena.worldWidth - 16.0f),
			std::clamp(here.y + ahead.y, 16.0f, arena.worldHeight - 16.0f)
		};
		return towards(self, arena, target);
	}

	// Prefers a long trail to wall the player in.
	float pickupAppetite(ObjectType type, const Cycle&, const Cycle&) const override {
		return preference(type, 0.6f, 1.0f);
	}

	float pickupSightRange() const override { return 160.0f; }

private:
	static constexpr float LEAD_DISTANCE = 96.0f;
	static constexpr float CLOSE_RANGE = 60.0f;
};
