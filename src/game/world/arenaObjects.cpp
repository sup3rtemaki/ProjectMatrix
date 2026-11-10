#include "world/arenaObjects.h"

#include "randomNumber.h"
#include "world/obstacles/wall.h"
#include "world/obstacles/tower.h"
#include "world/pickups/boostPickup.h"
#include "world/pickups/longTrailPickup.h"
#include "world/traps/mineTrap.h"
#include "world/traps/slowTrap.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <tuple>

namespace {
constexpr float RETRY_SECONDS = 1.0f;
constexpr int EDGE_MARGIN = 3;
constexpr int KEEP_CLEAR_RADIUS = 12;
constexpr int PLACEMENT_ATTEMPTS = 60;
constexpr SDL_Point SCATTER_SIZE = { 2, 2 };

std::unique_ptr<WorldObject> create(ObjectType type, const SDL_Rect& area, int cellSize, SDL_Renderer* renderer) {
	switch (type) {
	case ObjectType::Wall: return std::make_unique<Wall>(area, cellSize);
	case ObjectType::BoostPickup: return std::make_unique<BoostPickup>(area, cellSize);
	case ObjectType::LongTrailPickup: return std::make_unique<LongTrailPickup>(area, cellSize);
	case ObjectType::MineTrap: return std::make_unique<MineTrap>(area, cellSize);
	case ObjectType::SlowTrap: return std::make_unique<SlowTrap>(area, cellSize);
	case ObjectType::Tower: return std::make_unique<Tower>(area, cellSize, renderer);
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
	walls_.reset(gridWidth_, gridHeight_, cellSize_);
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
			tryPlace(scatter.type, occupied, keepClear);
		}
	}
}

void ArenaObjects::update(float elapsedSeconds, const std::vector<std::uint8_t>& occupied,
	const std::vector<Cycle*>& cycles) {
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

	for (const Entry& entry : entries_) {
		if (entry.object != nullptr) {
			entry.object->tick(elapsedSeconds, cycles);
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

void ArenaObjects::queueDraw(const RenderContext& context, DepthQueue& queue) {
	for (Entry& entry : entries_) {
		WorldObject* object = entry.object.get();
		queue.add(object->y, [object, &context]() { object->draw(context); });
	}
}

void ArenaObjects::drawProjectiles(const RenderContext& context) {
	for (Entry& entry : entries_) {
		entry.object->drawProjectiles(context);
	}
}

const std::vector<std::uint8_t>& ArenaObjects::dangerGrid() const {
	return danger_;
}

const std::vector<PickupSighting>& ArenaObjects::pickups() const {
	return pickups_;
}

bool ArenaObjects::blocksProjectiles(const SDL_Rect& box) const {
	if (box.x < 0 || box.y < 0) {
		return true;
	}
	const int left = box.x / cellSize_;
	const int top = box.y / cellSize_;
	const int right = (box.x + box.w - 1) / cellSize_;
	const int bottom = (box.y + box.h - 1) / cellSize_;
	if (right >= gridWidth_ || bottom >= gridHeight_) {
		return true;
	}
	for (int y = top; y <= bottom; ++y) {
		for (int x = left; x <= right; ++x) {
			const WorldObject* object = cells_[cellIndex(x, y)];
			if (object != nullptr && object->blocksProjectiles()) {
				return true;
			}
		}
	}
	return false;
}

std::vector<Vec2> ArenaObjects::getMinePositions() const {
	std::vector<Vec2> mines;
	for (const Entry& entry : entries_) {
		if (entry.type == ObjectType::MineTrap && entry.object != nullptr) {
			mines.push_back({ entry.object->x, entry.object->y });
		}
	}
	return mines;
}

WorldSnapshot ArenaObjects::snapshot(std::uint32_t sequence) const {
	WorldSnapshot result;
	result.sequence = sequence;
	for (const Entry& entry : entries_) {
		if (entry.type == ObjectType::Wall) continue;
		if (entry.type == ObjectType::Tower) {
			const Tower& tower = static_cast<const Tower&>(*entry.object);
			result.towers.push_back(tower.snapshot(entry.placement));
			continue;
		}
		SnapshotObjectType type;
		switch (entry.type) {
		case ObjectType::BoostPickup: type = SnapshotObjectType::Boost; break;
		case ObjectType::LongTrailPickup: type = SnapshotObjectType::LongTrail; break;
		case ObjectType::MineTrap: type = SnapshotObjectType::Mine; break;
		case ObjectType::SlowTrap: type = SnapshotObjectType::Slow; break;
		default:
			std::cerr << "[ArenaObjects] Cannot capture unknown object type" << std::endl;
			continue;
		}
		const SDL_Rect& cells = entry.placement;
		result.objects.push_back({ type, { static_cast<std::uint32_t>(cells.x), static_cast<std::uint32_t>(cells.y),
			static_cast<std::uint32_t>(cells.w), static_cast<std::uint32_t>(cells.h) } });
	}
	return result;
}

bool ArenaObjects::applySnapshot(const WorldSnapshot& snapshot) {
	const auto reject = []() {
		std::cerr << "[ArenaObjects] Rejected incompatible world snapshot" << std::endl;
		return false;
	};
	if (!validateWorldSnapshot(snapshot) || gridWidth_ <= 0 || gridHeight_ <= 0) return reject();
	using Key = std::tuple<int, int, int, int, int>;
	const auto key = [](ObjectType type, const SDL_Rect& area) {
		return Key(static_cast<int>(type), area.x, area.y, area.w, area.h);
	};
	const auto toRect = [](const SnapshotCellRect& cells) {
		return SDL_Rect{ static_cast<int>(cells.x), static_cast<int>(cells.y),
			static_cast<int>(cells.w), static_cast<int>(cells.h) };
	};
	const auto objectType = [](SnapshotObjectType type) -> std::optional<ObjectType> {
		switch (type) {
		case SnapshotObjectType::Boost: return ObjectType::BoostPickup;
		case SnapshotObjectType::LongTrail: return ObjectType::LongTrailPickup;
		case SnapshotObjectType::Mine: return ObjectType::MineTrap;
		case SnapshotObjectType::Slow: return ObjectType::SlowTrap;
		}
		return std::nullopt;
	};
	std::vector<std::uint8_t> occupied(cells_.size(), 0);
	std::map<Key, Tower*> towers;
	std::map<Key, WorldObject*> dynamic;
	for (const Entry& entry : entries_) {
		if (entry.type == ObjectType::Wall || entry.type == ObjectType::Tower) {
			if (entry.type == ObjectType::Tower) {
				towers.emplace(key(entry.type, entry.placement), static_cast<Tower*>(entry.object.get()));
			}
			const SDL_Rect& area = entry.object->cells();
			for (int y = std::max(area.y, 0); y < std::min(area.y + area.h, gridHeight_); ++y) {
				for (int x = std::max(area.x, 0); x < std::min(area.x + area.w, gridWidth_); ++x) {
					occupied[cellIndex(x, y)] = 1;
				}
			}
		}
		else {
			dynamic.emplace(key(entry.type, entry.placement), entry.object.get());
		}
	}
	if (towers.size() != snapshot.towers.size()) return reject();
	std::vector<std::pair<Tower*, const SnapshotTower*>> towerStates;
	for (const SnapshotTower& state : snapshot.towers) {
		const auto found = towers.find(key(ObjectType::Tower, toRect(state.placement)));
		if (found == towers.end() || !found->second->canApplySnapshot(state)) return reject();
		towerStates.emplace_back(found->second, &state);
	}
	std::map<Key, std::pair<ObjectType, SDL_Rect>> requested;
	for (const SnapshotObject& state : snapshot.objects) {
		const SDL_Rect area = toRect(state.cells);
		if (!isInsideGrid(area)) return reject();
		for (int y = area.y; y < area.y + area.h; ++y) {
			for (int x = area.x; x < area.x + area.w; ++x) {
				const std::size_t index = cellIndex(x, y);
				if (occupied[index]) return reject();
				occupied[index] = 1;
			}
		}
		const auto type = objectType(state.type);
		if (!type) return reject();
		requested.emplace(key(*type, area), std::make_pair(*type, area));
	}
	for (const auto& entry : dynamic) {
		if (requested.find(entry.first) == requested.end()) remove(entry.second);
	}
	for (const auto& entry : requested) {
		if (dynamic.find(entry.first) == dynamic.end()) add(entry.second.first, entry.second.second);
	}
	for (const auto& state : towerStates) state.first->applySnapshot(*state.second);
	respawns_.clear();
	return true;
}

std::size_t ArenaObjects::cellIndex(int x, int y) const {
	return static_cast<std::size_t>(y * gridWidth_ + x);
}

bool ArenaObjects::tryPlace(ObjectType type, const std::vector<std::uint8_t>& occupied,
	const std::vector<SDL_Point>& keepClear) {
	for (int attempt = 0; attempt < PLACEMENT_ATTEMPTS; ++attempt) {
		const SDL_Point size = SCATTER_SIZE;
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
	entries_.push_back({ type, area, create(type, area, cellSize_, renderer_) });
	WorldObject* object = entries_.back().object.get();
	if (type == ObjectType::Wall) {
		const SDL_Rect& cells = object->cells();
		walls_.add({ static_cast<float>(cells.x * cellSize_), static_cast<float>(cells.y * cellSize_),
			static_cast<float>(cells.w * cellSize_), static_cast<float>(cells.h * cellSize_) });
	}
	object->setSolidQuery([this](const SDL_Rect& box) { return blocksProjectiles(box); });
	if (object->isPickup()) {
		pickups_.push_back({ type, { area.x + area.w / 2, area.y + area.h / 2 } });
	}
	// An object may occupy less than the placed area, like a tower's collision box.
	const SDL_Rect footprint = object->cells();
	for (int y = std::max(footprint.y, 0); y < std::min(footprint.y + footprint.h, gridHeight_); ++y) {
		for (int x = std::max(footprint.x, 0); x < std::min(footprint.x + footprint.w, gridWidth_); ++x) {
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
