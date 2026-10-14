#pragma once

#include "level/levelDefinition.h"
#include "world/pickupSighting.h"
#include "world/worldObject.h"

#include <cstdint>
#include <memory>
#include <vector>

// Owns the power-ups, traps and obstacles of the arena and finds them by grid cell.
class ArenaObjects {
public:
	explicit ArenaObjects(int cellSize);

	// Rebuilds the arena content from the level; scattered objects keep away from keepClear cells.
	void reset(const LevelDefinition& level, const std::vector<SDL_Point>& keepClear,
		const std::vector<std::uint8_t>& occupied);
	void update(float elapsedSeconds, const std::vector<std::uint8_t>& occupied);
	// Triggers the object under the cell, if any.
	void touch(Cycle& cycle, SDL_Point cell);
	void draw(const RenderContext& context);

	// 1 on cells holding something dangerous, for the enemy AI.
	const std::vector<std::uint8_t>& dangerGrid() const;
	// Power-ups currently in the arena, for the enemy AI.
	const std::vector<PickupSighting>& pickups() const;

	std::vector<RectF> getWallBoxes() const;
	std::vector<Vec2> getMinePositions() const;

private:
	struct Entry {
		ObjectType type;
		std::unique_ptr<WorldObject> object;
	};
	struct PendingRespawn {
		ObjectType type;
		float remaining;
	};

	std::size_t cellIndex(int x, int y) const;
	bool tryPlace(ObjectType type, const std::vector<std::uint8_t>& occupied,
		const std::vector<SDL_Point>& keepClear, int minLength = 3, int maxLength = 10);
	bool isAreaFree(const SDL_Rect& area, const std::vector<std::uint8_t>& occupied,
		const std::vector<SDL_Point>& keepClear) const;
	bool isInsideGrid(const SDL_Rect& area) const;
	void add(ObjectType type, const SDL_Rect& area);
	void remove(WorldObject* object);

	int gridWidth_ = 0;
	int gridHeight_ = 0;
	int cellSize_;
	float respawnSeconds_ = 0;
	std::vector<Entry> entries_;
	std::vector<WorldObject*> cells_;
	std::vector<std::uint8_t> danger_;
	std::vector<PickupSighting> pickups_;
	std::vector<PendingRespawn> respawns_;
};
