#include "ai/enemyBrain.h"

#include "randomNumber.h"

#include <cstdlib>
#include <limits>
#include <vector>

Vec2 EnemyBrain::directionVector(Direction dir) {
	switch (dir) {
	case Direction::North: return { 0.0f, -1.0f };
	case Direction::East:  return { 1.0f, 0.0f };
	case Direction::South: return { 0.0f, 1.0f };
	case Direction::West:  return { -1.0f, 0.0f };
	}
	return { 1.0f, 0.0f };
}

Direction EnemyBrain::turnLeft(Direction dir) {
	return static_cast<Direction>((static_cast<int>(dir) + 3) % 4);
}

Direction EnemyBrain::turnRight(Direction dir) {
	return static_cast<Direction>((static_cast<int>(dir) + 1) % 4);
}

float EnemyBrain::preference(ObjectType type, float boost, float longTrail) {
	switch (type) {
	case ObjectType::BoostPickup: return boost;
	case ObjectType::LongTrailPickup: return longTrail;
	default: return 0.0f;
	}
}

Direction EnemyBrain::decide(const Cycle& self, const Cycle& player, const ArenaView& arena, float elapsedSeconds) {
	if (pursuitTimer_ > 0.0f) {
		pursuitTimer_ -= elapsedSeconds;
	}
	if (pursuitTimer_ <= 0.0f) {
		pursuitTimer_ = chaseBurstDuration() * (0.7f + getRandomNumber(1.4f));
	}

	const float sight = pickupSightRange();
	const PickupSighting* target = nullptr;
	float bestScore = MIN_PURSUIT_SCORE;

	for (const PickupSighting& pickup : arena.pickups) {
		const Vec2 pickupPos = { static_cast<float>(pickup.cell.x * 8 + 4), static_cast<float>(pickup.cell.y * 8 + 4) };
		const float away = geom::distance(self.center(), pickupPos);
		if (away > sight) {
			continue;
		}

		const float score = pickupAppetite(pickup.type, self, player) * (1.0f - away / sight);
		if (score > bestScore) {
			const Vec2 toPickup = (pickupPos - self.center()).normalized();
			const float clearance = arena.evaluateClearance(self.center(), toPickup, away, &self);
			if (clearance >= away - 4.0f) {
				bestScore = score;
				target = &pickup;
			}
		}
	}

	Vec2 bestBikeTarget = player.center();
	float bestBikeScore = -std::numeric_limits<float>::infinity();
	for (const Vec2& candidate : arena.chaseTargets) {
		if (geom::distance(self.center(), candidate) < 12.0f) {
			continue;
		}
		const bool isPlayer = geom::distance(candidate, player.center()) < 8.0f;
		const float dist = geom::distance(self.center(), candidate);
		const float proximity = 1.0f / std::max(dist, 16.0f);
		const float timeBoost = (pursuitTimer_ > 0.0f) ? 0.85f : 0.35f;
		const float score = (proximity * 120.0f) + (chaseTargetPriority(isPlayer) * 60.0f) + timeBoost;
		if (score > bestBikeScore) {
			bestBikeScore = score;
			bestBikeTarget = candidate;
		}
	}

	if (target != nullptr) {
		const Vec2 targetPos = { static_cast<float>(target->cell.x * 8 + 4), static_cast<float>(target->cell.y * 8 + 4) };
		return towards(self, arena, targetPos);
	}

	lastPursuitTarget_ = bestBikeTarget;
	return towards(self, arena, bestBikeTarget);
}

Direction EnemyBrain::pickBest(const Cycle& self, const ArenaView& arena,
	const std::function<float(Direction dir, Vec2 nextPos, float clearance)>& score) {
	const Direction options[3] = {
		self.direction(),
		turnLeft(self.direction()),
		turnRight(self.direction())
	};

	Direction best = self.direction();
	float bestScore = -std::numeric_limits<float>::infinity();
	bool foundSafe = false;

	Direction roomiest = self.direction();
	float maxClearance = -1.0f;

	for (const Direction option : options) {
		const Vec2 dirVec = directionVector(option);
		const float clearance = arena.evaluateClearance(self.center(), dirVec, SENSOR_RANGE, &self);

		if (clearance > maxClearance) {
			maxClearance = clearance;
			roomiest = option;
		}

		if (clearance < MIN_SAFE_CLEARANCE) {
			continue; // Dangerously close to a wall, mine or trail!
		}

		const Vec2 lookAheadPos = self.center() + dirVec * std::min(clearance, 40.0f);
		const float optScore = score(option, lookAheadPos, clearance);

		if (!foundSafe || optScore > bestScore) {
			foundSafe = true;
			bestScore = optScore;
			best = option;
		}
	}

	return foundSafe ? best : roomiest;
}

Direction EnemyBrain::towards(const Cycle& self, const ArenaView& arena, Vec2 target) {
	return pickBest(self, arena, [target](Direction, Vec2 nextPos, float clearance) {
		// Prefer getting closer to target, with a small bonus for open space
		return -geom::distance(nextPos, target) + clearance * 0.15f;
	});
}

Direction EnemyBrain::awayFrom(const Cycle& self, const ArenaView& arena, Vec2 target) {
	return pickBest(self, arena, [target](Direction, Vec2 nextPos, float clearance) {
		return geom::distance(nextPos, target) + clearance * 0.2f;
	});
}

Direction EnemyBrain::mostOpenSpace(const Cycle& self, const ArenaView& arena) {
	return pickBest(self, arena, [](Direction, Vec2, float clearance) {
		return clearance;
	});
}

Direction EnemyBrain::wander(const Cycle& self, const ArenaView& arena, int keepForwardPercent) {
	const Direction forward = self.direction();
	const Vec2 fwdVec = directionVector(forward);
	const float fwdClearance = arena.evaluateClearance(self.center(), fwdVec, SENSOR_RANGE, &self);

	if (fwdClearance >= MIN_SAFE_CLEARANCE && getRandomNumber(100) < keepForwardPercent) {
		return forward;
	}

	return pickBest(self, arena, [](Direction, Vec2, float clearance) {
		return clearance + static_cast<float>(getRandomNumber(20));
	});
}
