#include "level/spawnPlanner.h"

#include "randomNumber.h"

#include <algorithm>
#include <cstdlib>

namespace {
constexpr int EDGE_MARGIN = 10;
constexpr int MIN_SPAWN_DISTANCE = 25;
constexpr int WALL_CLEARANCE = 3;
constexpr int LOOK_AHEAD = 12;
constexpr int ATTEMPTS = 200;

bool touchesFixedObject(const LevelDefinition& level, SDL_Point cell, int clearance) {
	return std::any_of(level.placements.begin(), level.placements.end(),
		[&](const ObjectPlacement& placement) {
			const SDL_Rect& area = placement.cells;
			return cell.x >= area.x - clearance && cell.x < area.x + area.w + clearance
				&& cell.y >= area.y - clearance && cell.y < area.y + area.h + clearance;
		});
}

bool isClear(const LevelDefinition& level, SDL_Point cell) {
	return cell.x >= 0 && cell.x < level.width && cell.y >= 0 && cell.y < level.height
		&& !touchesFixedObject(level, cell, 0);
}

Direction randomDirection(const LevelDefinition& level, SDL_Point cell) {
	const SDL_Point offsets[] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };
	std::vector<Direction> open;
	for (int index = 0; index < 4; ++index) {
		bool clear = true;
		for (int step = 1; step <= LOOK_AHEAD && clear; ++step) {
			clear = isClear(level, { cell.x + offsets[index].x * step, cell.y + offsets[index].y * step });
		}
		if (clear) {
			open.push_back(static_cast<Direction>(index));
		}
	}
	return open.empty() ? Direction::East : open[static_cast<std::size_t>(getRandomNumber(static_cast<int>(open.size())))];
}

SDL_Point randomCell(const LevelDefinition& level, const std::vector<ResolvedSpawn>& taken) {
	SDL_Point cell{ level.width / 2, level.height / 2 };
	for (int attempt = 0; attempt < ATTEMPTS; ++attempt) {
		cell = { EDGE_MARGIN + getRandomNumber(level.width - 2 * EDGE_MARGIN),
			EDGE_MARGIN + getRandomNumber(level.height - 2 * EDGE_MARGIN) };
		const bool farFromOthers = std::all_of(taken.begin(), taken.end(), [&](const ResolvedSpawn& other) {
			return std::max(std::abs(other.cell.x - cell.x), std::abs(other.cell.y - cell.y)) >= MIN_SPAWN_DISTANCE;
		});
		if (farFromOthers && !touchesFixedObject(level, cell, WALL_CLEARANCE)) {
			break;
		}
	}
	return cell;
}
}

std::vector<ResolvedSpawn> resolveSpawns(const LevelDefinition& level) {
	std::vector<const SpawnPoint*> requests = { &level.player };
	for (const EnemyDefinition& enemy : level.enemies) {
		requests.push_back(&enemy.spawn);
	}

	// Fixed cells go first so the random ones can keep away from them.
	std::vector<ResolvedSpawn> resolved(requests.size());
	std::vector<ResolvedSpawn> taken;
	for (std::size_t index = 0; index < requests.size(); ++index) {
		if (requests[index]->cell) {
			resolved[index].cell = *requests[index]->cell;
			taken.push_back(resolved[index]);
		}
	}
	for (std::size_t index = 0; index < requests.size(); ++index) {
		if (!requests[index]->cell) {
			resolved[index].cell = randomCell(level, taken);
			taken.push_back(resolved[index]);
		}
		resolved[index].direction = requests[index]->direction
			? *requests[index]->direction : randomDirection(level, resolved[index].cell);
	}
	return resolved;
}
