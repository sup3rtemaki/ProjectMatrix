#pragma once

#include "ai/enemyBrain.h"

#include <algorithm>

class AmbusherBrain : public EnemyBrain {
protected:
	Direction act(const Cycle& self, const Cycle& player, const ArenaView& arena) override {
		const SDL_Point here = player.position();
		if (distance(self.position(), here) <= CLOSE_RANGE) {
			return towards(self, arena, here);
		}

		const SDL_Point ahead = player.nextPosition();
		const SDL_Point target = {
			std::clamp(here.x + (ahead.x - here.x) * LEAD_CELLS, 0, arena.width - 1),
			std::clamp(here.y + (ahead.y - here.y) * LEAD_CELLS, 0, arena.height - 1) };
		return towards(self, arena, target);
	}

	// Prefers a long trail to wall the player in.
	float pickupAppetite(ObjectType type, const Cycle&, const Cycle&) const override {
		return preference(type, 0.6f, 1.0f);
	}

	int pickupSightRange() const override { return 18; }

private:
	static constexpr int LEAD_CELLS = 12;
	static constexpr int CLOSE_RANGE = 8;
};
