#pragma once

#include "level/levelDefinition.h"
#include "collision/wallIndex.h"
#include "rendering/depthQueue.h"
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
	void update(float elapsedSeconds, const std::vector<std::uint8_t>& occupied, const std::vector<Cycle*>& cycles);
	// Triggers the object under the cell, if any.
	void touch(Cycle& cycle, SDL_Point cell);
	// Queues the objects in the Objects layer, sorted by their Y. The context must outlive queue.flush().
	void queueDraw(const RenderContext& context, DepthQueue& queue);
	void drawProjectiles(const RenderContext& context);
	void setRenderer(SDL_Renderer* renderer) { renderer_ = renderer; }

	// 1 on cells holding something dangerous, for the enemy AI.
	const std::vector<std::uint8_t>& dangerGrid() const;
	// Power-ups currently in the arena, for the enemy AI.
	const std::vector<PickupSighting>& pickups() const;

	// True if the pixel box leaves the arena or overlaps an object that blocks projectiles.
	bool blocksProjectiles(const SDL_Rect& box) const;

	const std::vector<RectF>& getWallBoxes() const { return walls_.boxes(); }
	const WallIndex& wallIndex() const { return walls_; }
	std::vector<Vec2> getMinePositions() const;
	WorldSnapshot snapshot(std::uint32_t sequence) const;
	// Applies visual state only; the authoritative host continues to simulate the arena.
	bool applySnapshot(const WorldSnapshot& snapshot);

private:
	struct Entry {
		ObjectType type;
		SDL_Rect placement;
		std::unique_ptr<WorldObject> object;
	};
	struct PendingRespawn {
		ObjectType type;
		float remaining;
	};

	std::size_t cellIndex(int x, int y) const;
	bool tryPlace(ObjectType type, const std::vector<std::uint8_t>& occupied,
		const std::vector<SDL_Point>& keepClear);
	bool isAreaFree(const SDL_Rect& area, const std::vector<std::uint8_t>& occupied,
		const std::vector<SDL_Point>& keepClear) const;
	bool isInsideGrid(const SDL_Rect& area) const;
	void add(ObjectType type, const SDL_Rect& area);
	void remove(WorldObject* object);

	int gridWidth_ = 0;
	int gridHeight_ = 0;
	int cellSize_;
	float respawnSeconds_ = 0;
	SDL_Renderer* renderer_ = nullptr;
	std::vector<Entry> entries_;
	// Walls are rebuilt on reset and are neither consumed nor replaced by snapshots.
	WallIndex walls_;
	std::vector<WorldObject*> cells_;
	std::vector<std::uint8_t> danger_;
	std::vector<PickupSighting> pickups_;
	std::vector<PendingRespawn> respawns_;
};
