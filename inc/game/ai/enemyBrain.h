#pragma once

#include "ai/arenaView.h"
#include "collision/geometry2D.h"
#include "cycle.h"

#include <functional>

// Decides where an enemy cycle turns next; each personality is a subclass.
class EnemyBrain {
public:
	virtual ~EnemyBrain() = default;

	// Goes for a power-up when the personality wants it enough, otherwise acts on its own style.
	Direction decide(const Cycle& self, const Cycle& player, const ArenaView& arena, float elapsedSeconds);

	static Vec2 directionVector(Direction dir);
	static Direction turnLeft(Direction dir);
	static Direction turnRight(Direction dir);

protected:
	virtual float chaseBurstDuration() const { return 1.5f; }
	virtual float chaseTargetPriority(bool isPlayer) const { return isPlayer ? 1.0f : 0.72f; }
	virtual Direction act(const Cycle& self, const Cycle& player, const ArenaView& arena) = 0;
	// How much this personality wants the power-up, from 0 to 1.
	virtual float pickupAppetite(ObjectType type, const Cycle& self, const Cycle& player) const = 0;
	// Power-ups farther than this (in pixels) are ignored.
	virtual float pickupSightRange() const { return 150.0f; }

	static float preference(ObjectType type, float boost, float longTrail);

	static constexpr float SENSOR_RANGE = 200.0f;
	static constexpr float MIN_SAFE_CLEARANCE = 28.0f;
	static constexpr float MIN_PURSUIT_SCORE = 0.25f;

	// Highest-scoring turn (forward/left/right) that does not lead into a dead end.
	static Direction pickBest(const Cycle& self, const ArenaView& arena,
		const std::function<float(Direction dir, Vec2 nextPos, float clearance)>& score);
	static Direction towards(const Cycle& self, const ArenaView& arena, Vec2 target);
	static Direction awayFrom(const Cycle& self, const ArenaView& arena, Vec2 target);
	static Direction mostOpenSpace(const Cycle& self, const ArenaView& arena);
	// Keeps going forward with the given chance, otherwise turns randomly.
	static Direction wander(const Cycle& self, const ArenaView& arena, int keepForwardPercent);

	float pursuitTimer_ = 0.0f;
	Vec2 lastPursuitTarget_{};
};
