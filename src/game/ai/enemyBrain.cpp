#include "ai/enemyBrain.h"

#include "randomNumber.h"

#include <cstdlib>
#include <limits>
#include <vector>

namespace {
constexpr float PI = 3.14159265358979323846f;
constexpr float DECISION_INTERVAL = 0.18f;
constexpr float PREDICTION_STEP = 0.08f;
constexpr int PREDICTION_STEPS = 10;

float angleDifference(float target, float current) {
	return std::remainder(target - current, 360.0f);
}

float directionAngle(Direction direction) {
	const Vec2 vector = EnemyBrain::directionVector(direction);
	return std::atan2(vector.y, vector.x) * 180.0f / PI;
}

Vec2 rotate(Vec2 value, float degrees) {
	const float radians = degrees * PI / 180.0f;
	const float c = std::cos(radians), s = std::sin(radians);
	return { c * value.x - s * value.y, s * value.x + c * value.y };
}

float predictClearance(const Cycle& cycle, const ArenaView& arena, const std::vector<LineSegment>& ownTrail,
	float targetHeading) {
	Vec2 position = cycle.center();
	float heading = cycle.headingDegrees();
	const OrientedBox original = cycle.terrainBoxAt(position);
	const Vec2 centerOffset = original.center - position;
	const RectF world = { 0.0f, 0.0f, arena.worldWidth, arena.worldHeight };
	const float stepDistance = cycle.speed() * PREDICTION_STEP;
	for (int step = 0; step < PREDICTION_STEPS; ++step) {
		const float turn = std::clamp(angleDifference(targetHeading, heading),
			-Cycle::TURN_SPEED * PREDICTION_STEP, Cycle::TURN_SPEED * PREDICTION_STEP);
		heading += turn;
		const float rotation = heading - cycle.headingDegrees();
		const OrientedBox box = { position + rotate(centerOffset, rotation),
			rotate(original.axisX, rotation), rotate(original.axisY, rotation), original.halfWidth, original.halfHeight };
		const Vec2 displacement = rotate({ stepDistance, 0.0f }, heading);
		OrientedBox end = box;
		end.center = end.center + displacement;
		if (!geom::boxInsideRect(box, world) || !geom::boxInsideRect(end, world)) return step * stepDistance;
		if (arena.wallIndex != nullptr) {
			for (const std::size_t index : arena.wallIndex->query(sweptBounds(box, displacement))) {
				if (geom::sweptBoxIntersectsBox(box, displacement, geom::orientedBox(arena.walls[index], { 0, 0 }, 0)))
					return step * stepDistance;
			}
		}
		else {
			for (const RectF& wall : arena.walls) {
				if (geom::sweptBoxIntersectsBox(box, displacement, geom::orientedBox(wall, { 0, 0 }, 0)))
					return step * stepDistance;
			}
		}
		for (const LineSegment& segment : arena.segments) {
			if (segment.owner != &cycle && geom::sweptBoxIntersectsSegment(box, displacement, segment.a, segment.b))
				return step * stepDistance;
		}
		for (const LineSegment& segment : ownTrail) {
			if (geom::sweptBoxIntersectsSegment(box, displacement, segment.a, segment.b))
				return step * stepDistance;
		}
		for (Vec2 mine : arena.mines) {
			const RectF mineBox = { mine.x - 8.0f, mine.y - 8.0f, 16.0f, 16.0f };
			if (geom::sweptBoxIntersectsBox(box, displacement, geom::orientedBox(mineBox, { 0, 0 }, 0)))
				return step * stepDistance;
		}
		position = position + displacement;
	}
	return PREDICTION_STEPS * stepDistance;
}
}

void EnemyBrain::resetDriving() {
	drivingDecisionTimer_ = 0.0f;
	lastDecisionElapsed_ = 0.0f;
	hasDrivingTarget_ = false;
}

void EnemyBrain::drive(Cycle& self, const Cycle& player, const ArenaView& arena, float elapsedSeconds) {
	if (elapsedSeconds <= 0.0f) return;
	drivingDecisionTimer_ -= elapsedSeconds;
	lastDecisionElapsed_ += elapsedSeconds;
	if (!hasDrivingTarget_ || drivingDecisionTimer_ <= 0.0f) {
		const float desired = directionAngle(decide(self, player, arena, lastDecisionElapsed_));
		const float heading = self.headingDegrees();
		const float candidates[] = { heading, heading - 90.0f, heading + 90.0f, desired };
		const auto ownTrail = self.trail().getSegments(&self, true, 28.0f);
		float bestScore = -std::numeric_limits<float>::infinity();
		for (float candidate : candidates) {
			if (std::abs(angleDifference(candidate, heading)) > 135.0f) continue;
			const float clearance = predictClearance(self, arena, ownTrail, candidate);
			const float alignment = std::abs(angleDifference(desired, candidate));
			const float score = clearance - alignment * 0.15f
				- (hasDrivingTarget_ ? std::abs(angleDifference(targetHeading_, candidate)) * 0.03f : 0.0f);
			if (score > bestScore) {
				bestScore = score;
				targetHeading_ = candidate;
			}
		}
		hasDrivingTarget_ = true;
		drivingDecisionTimer_ = DECISION_INTERVAL;
		lastDecisionElapsed_ = 0.0f;
	}

	const float error = angleDifference(targetHeading_, self.headingDegrees());
	const float steering = std::clamp(error / (Cycle::TURN_SPEED * elapsedSeconds), -1.0f, 1.0f);
	const float forwardClearance = arena.evaluateClearance(self.center(), self.headingVector(), 100.0f, &self);
	const float targetSpeed = forwardClearance < 80.0f ? Cycle::MIN_SPEED : Cycle::BASE_SPEED;
	const float throttle = self.baseSpeed() > targetSpeed + 1.0f ? -1.0f
		: (self.baseSpeed() < targetSpeed - 1.0f ? 1.0f : 0.0f);
	self.steer(steering, throttle, elapsedSeconds);
}

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
