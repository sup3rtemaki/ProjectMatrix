#pragma once

#include "ai/arenaView.h"
#include "cycle.h"

#include <functional>

// Decides where an enemy cycle turns next; each personality is a subclass.
class EnemyBrain {
public:
	virtual ~EnemyBrain() = default;
	virtual Direction decide(const Cycle& self, const Cycle& player, const ArenaView& arena) = 0;

protected:
	static constexpr int MIN_SAFE_SPACE = 30;
	static constexpr int SPACE_LIMIT = 400;

	static int distance(SDL_Point from, SDL_Point to);

	// Highest-scoring turn (forward/left/right) that does not lead into a dead end.
	static Direction pickBest(const Cycle& self, const ArenaView& arena,
		const std::function<float(SDL_Point)>& score);
	static Direction towards(const Cycle& self, const ArenaView& arena, SDL_Point target);
	static Direction awayFrom(const Cycle& self, const ArenaView& arena, SDL_Point target);
	static Direction mostOpenSpace(const Cycle& self, const ArenaView& arena);
	// Keeps going forward with the given chance, otherwise turns randomly.
	static Direction wander(const Cycle& self, const ArenaView& arena, int keepForwardPercent);
};
