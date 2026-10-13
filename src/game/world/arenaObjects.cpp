#include "world/arenaObjects.h"

#include "randomNumber.h"
#include "world/obstacles/wall.h"
#include "world/pickups/boostPickup.h"
#include "world/pickups/longTrailPickup.h"
#include "world/traps/mineTrap.h"
#include "world/traps/slowTrap.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace {
constexpr float RETRY_SECONDS = 1.0f;
constexpr int EDGE_MARGIN = 3;
constexpr int KEEP_CLEAR_RADIUS = 12;
constexpr int PLACEMENT_ATTEMPTS = 60;

SDL_Point randomSize(ObjectType type, int minLength, int maxLength) {
	if (type != ObjectType::Wall) {
		return { 2, 2 };
	}
	const int length = minLength + getRandomNumber(maxLength - minLength + 1);
	const int thickness = 1 + getRandomNumber(2);
	return getRandomNumber(2) == 0 ? SDL_Point{ length, thickness } : SDL_Point{ thickness, length };
}

std::unique_ptr<WorldObject> create(ObjectType type, const SDL_Rect& area, int cellSize) {
	switch (type) {
	case ObjectType::Wall: return std::make_unique<Wall>(area, cellSize);
	case ObjectType::BoostPickup: return std::make_unique<BoostPickup>(area, cellSize);
	case ObjectType::LongTrailPickup: return std::make_unique<LongTrailPickup>(area, cellSize);
	case ObjectType::MineTrap: return std::make_unique<MineTrap>(area, cellSize);
	case ObjectType::SlowTrap: return std::make_unique<SlowTrap>(area, cellSize);
	}
	return nullptr;
}
}

ArenaObjects::ArenaObjects(int cellSize) : cellSize_(cellSize) {
}

void ArenaObjects::reset(const LevelDefinition& level, const std::vector<SDL_Point>& keepClear,
	const std::vector<std::uint8_t>& occupied) {
	gridWidth_ = level.width;
	gridHeight_ = level.height;
	respawnSeconds_ = level.respawnSeconds;
	entries_.clear();
	respawns_.clear();
	pickups_.clear();
	cells_.assign(static_cast<std::size_t>(gridWidth_ * gridHeight_), nullptr);
	danger_.assign(static_cast<std::size_t>(gridWidth_ * gridHeight_), 0);

	for (const ObjectPlacement& placement : level.placements) {
		if (isInsideGrid(placement.cells) && isAreaFree(placement.cells, occupied, {})) {
			add(placement.type, placement.cells);
		}
		else {
			std::cerr << "Level " << level.name << ": object at " << placement.cells.x << ","
				<< placement.cells.y << " is outside the arena or overlaps another one" << std::endl;
		}
	}

	for (const ObjectScatter& scatter : level.scatter) {
		for (int count = 0; count < scatter.count; ++count) {
			tryPlace(scatter.type, occupied, keepClear, scatter.minLength, scatter.maxLength);
		}
	}
}

void ArenaObjects::update(float elapsedSeconds, const std::vector<std::uint8_t>& occupied) {
	for (std::size_t index = 0; index < respawns_.size();) {
		respawns_[index].remaining -= elapsedSeconds;
		if (respawns_[index].remaining > 0) {
			++index;
		}
		else if (tryPlace(respawns_[index].type, occupied, {})) {
			respawns_.erase(respawns_.begin() + static_cast<std::ptrdiff_t>(index));
		}
		else {
			respawns_[index].remaining = RETRY_SECONDS;
			++index;
		}
	}
}

void ArenaObjects::touch(Cycle& cycle, SDL_Point cell) {
	if (cell.x < 0 || cell.x >= gridWidth_ || cell.y < 0 || cell.y >= gridHeight_) {
		return;
	}
	WorldObject* object = cells_[cellIndex(cell.x, cell.y)];
	if (object == nullptr) {
		return;
	}

	object->onTouch(cycle);
	if (!object->isConsumedOnTouch()) {
		return;
	}

	const auto entry = std::find_if(entries_.begin(), entries_.end(),
		[object](const Entry& candidate) { return candidate.object.get() == object; });
	if (respawnSeconds_ > 0 && entry->type != ObjectType::Wall) {
		respawns_.push_back({ entry->type, respawnSeconds_ });
	}
	remove(object);
}

void ArenaObjects::draw(const RenderContext& context) {
	for (Entry& entry : entries_) {
		entry.object->draw(context);
	}
}

const std::vector<std::uint8_t>& ArenaObjects::dangerGrid() const {
	return danger_;
}

const std::vector<PickupSighting>& ArenaObjects::pickups() const {
	return pickups_;
}

std::size_t ArenaObjects::cellIndex(int x, int y) const {
	return static_cast<std::size_t>(y * gridWidth_ + x);
}

bool ArenaObjects::tryPlace(ObjectType type, const std::vector<std::uint8_t>& occupied,
	const std::vector<SDL_Point>& keepClear, int minLength, int maxLength) {
	for (int attempt = 0; attempt < PLACEMENT_ATTEMPTS; ++attempt) {
		const SDL_Point size = randomSize(type, minLength, maxLength);
		const int spanX = gridWidth_ - size.x - 2 * EDGE_MARGIN;
		const int spanY = gridHeight_ - size.y - 2 * EDGE_MARGIN;
		if (spanX <= 0 || spanY <= 0) {
			return false;
		}
		const SDL_Rect area = { EDGE_MARGIN + getRandomNumber(spanX),
			EDGE_MARGIN + getRandomNumber(spanY), size.x, size.y };
		if (isAreaFree(area, occupied, keepClear)) {
			add(type, area);
			return true;
		}
	}
	return false;
}

bool ArenaObjects::isInsideGrid(const SDL_Rect& area) const {
	return area.w > 0 && area.h > 0 && area.x >= 0 && area.y >= 0
		&& area.x + area.w <= gridWidth_ && area.y + area.h <= gridHeight_;
}

bool ArenaObjects::isAreaFree(const SDL_Rect& area, const std::vector<std::uint8_t>& occupied,
	const std::vector<SDL_Point>& keepClear) const {
	for (const SDL_Point point : keepClear) {
		if (point.x >= area.x - KEEP_CLEAR_RADIUS && point.x < area.x + area.w + KEEP_CLEAR_RADIUS
			&& point.y >= area.y - KEEP_CLEAR_RADIUS && point.y < area.y + area.h + KEEP_CLEAR_RADIUS) {
			return false;
		}
	}

	for (int y = area.y; y < area.y + area.h; ++y) {
		for (int x = area.x; x < area.x + area.w; ++x) {
			const std::size_t index = cellIndex(x, y);
			if (cells_[index] != nullptr || occupied[index] != 0) {
				return false;
			}
		}
	}
	return true;
}

void ArenaObjects::add(ObjectType type, const SDL_Rect& area) {
	entries_.push_back({ type, create(type, area, cellSize_) });
	WorldObject* object = entries_.back().object.get();
	if (object->isPickup()) {
		pickups_.push_back({ type, { area.x + area.w / 2, area.y + area.h / 2 } });
	}
	for (int y = area.y; y < area.y + area.h; ++y) {
		for (int x = area.x; x < area.x + area.w; ++x) {
			cells_[cellIndex(x, y)] = object;
			if (object->isDangerous()) {
				danger_[cellIndex(x, y)] = 1;
			}
		}
	}
}

void ArenaObjects::remove(WorldObject* object) {
	const SDL_Rect area = object->cells();
	const SDL_Point center = { area.x + area.w / 2, area.y + area.h / 2 };
	pickups_.erase(std::remove_if(pickups_.begin(), pickups_.end(),
		[center](const PickupSighting& pickup) {
			return pickup.cell.x == center.x && pickup.cell.y == center.y;
		}), pickups_.end());
	for (int y = area.y; y < area.y + area.h; ++y) {
		for (int x = area.x; x < area.x + area.w; ++x) {
			cells_[cellIndex(x, y)] = nullptr;
			danger_[cellIndex(x, y)] = 0;
		}
	}
	entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
		[object](const Entry& entry) { return entry.object.get() == object; }), entries_.end());
}
