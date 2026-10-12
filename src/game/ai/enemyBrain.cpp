#include "ai/enemyBrain.h"

#include "randomNumber.h"

#include <cstdlib>
#include <limits>
#include <vector>

namespace {
std::vector<Direction> turnOptions(const Cycle& self) {
	const int forward = static_cast<int>(self.direction());
	return { self.direction(), static_cast<Direction>((forward + 3) % 4),
		static_cast<Direction>((forward + 1) % 4) };
}
}

int EnemyBrain::distance(SDL_Point from, SDL_Point to) {
	return std::abs(from.x - to.x) + std::abs(from.y - to.y);
}

Direction EnemyBrain::pickBest(const Cycle& self, const ArenaView& arena,
	const std::function<float(SDL_Point)>& score) {
	Direction best = self.direction();
	float bestScore = -std::numeric_limits<float>::infinity();
	bool foundSafe = false;
	Direction roomiest = self.direction();
	int roomiestSpace = -1;

	for (const Direction option : turnOptions(self)) {
		const SDL_Point next = self.nextPosition(option);
		if (arena.isBlocked(next.x, next.y)) {
			continue;
		}

		const int space = arena.reachableCells(next, MIN_SAFE_SPACE);
		if (space > roomiestSpace) {
			roomiestSpace = space;
			roomiest = option;
		}
		if (space < MIN_SAFE_SPACE) {
			continue;
		}

		const float optionScore = score(next);
		if (!foundSafe || optionScore > bestScore) {
			foundSafe = true;
			bestScore = optionScore;
			best = option;
		}
	}
	return foundSafe ? best : roomiest;
}

Direction EnemyBrain::towards(const Cycle& self, const ArenaView& arena, SDL_Point target) {
	return pickBest(self, arena, [target](SDL_Point next) {
		return -static_cast<float>(distance(next, target));
	});
}

Direction EnemyBrain::awayFrom(const Cycle& self, const ArenaView& arena, SDL_Point target) {
	return pickBest(self, arena, [target](SDL_Point next) {
		return static_cast<float>(distance(next, target));
	});
}

Direction EnemyBrain::mostOpenSpace(const Cycle& self, const ArenaView& arena) {
	return pickBest(self, arena, [&arena](SDL_Point next) {
		return static_cast<float>(arena.reachableCells(next, SPACE_LIMIT));
	});
}

Direction EnemyBrain::wander(const Cycle& self, const ArenaView& arena, int keepForwardPercent) {
	std::vector<Direction> open;
	bool forwardOpen = false;
	for (const Direction option : turnOptions(self)) {
		const SDL_Point next = self.nextPosition(option);
		if (arena.isBlocked(next.x, next.y)) {
			continue;
		}
		if (option == self.direction()) {
			forwardOpen = true;
		}
		else {
			open.push_back(option);
		}
	}

	if (forwardOpen && (open.empty() || getRandomNumber(100) < keepForwardPercent)) {
		return self.direction();
	}
	if (open.empty()) {
		return self.direction();
	}
	return open[static_cast<std::size_t>(getRandomNumber(static_cast<int>(open.size())))];
}
