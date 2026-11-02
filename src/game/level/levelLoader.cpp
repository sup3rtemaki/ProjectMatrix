#include "level/levelLoader.h"

#include "nlohmann/json.hpp"
#include "tileson/tileson.hpp"

#include <algorithm>
#include <any>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;

constexpr const char* WALLS_LAYER = "Walls";
constexpr const char* WALLS_TILESET = "walls";
constexpr const char* PROPS_LAYER = "OProps";
constexpr const char* CLASS_ID_PROPERTY = "classId";
constexpr int TOWER_CLASS_ID = 1001;

struct ColorPair {
	SDL_Color glow;
	SDL_Color core;
};

// Enemies get these colors in order.
const ColorPair DEFAULT_PALETTE[] = {
	{ { 105, 15, 30, 255 }, { 255, 45, 75, 255 } },
	{ { 105, 50, 5, 255 }, { 255, 155, 35, 255 } },
	{ { 65, 20, 115, 255 }, { 185, 95, 255, 255 } },
	{ { 10, 95, 45, 255 }, { 60, 255, 130, 255 } },
	{ { 110, 95, 5, 255 }, { 255, 235, 50, 255 } },
	{ { 120, 20, 90, 255 }, { 255, 90, 200, 255 } }
};

// The map has no spawn points: every cycle starts at a random free cell.
const EnemyPersonality DEFAULT_ENEMIES[] = {
	EnemyPersonality::Aggressor, EnemyPersonality::Ambusher, EnemyPersonality::Strategist,
	EnemyPersonality::Coward, EnemyPersonality::Strategist
};

const ObjectScatter DEFAULT_SCATTER[] = {
	{ ObjectType::BoostPickup, 3 },
	{ ObjectType::LongTrailPickup, 3 },
	{ ObjectType::MineTrap, 5 },
	{ ObjectType::SlowTrap, 5 }
};

// Tiled stores tilesets as ../Tiled/name.tsx; Tileson reads the name.json export next to it.
std::string readMapWithTilesetPaths(const fs::path& mapPath) {
	std::ifstream file(mapPath);
	if (!file.good()) {
		throw std::runtime_error("file not found");
	}
	Json json = Json::parse(file, nullptr, false);
	if (json.is_discarded()) {
		throw std::runtime_error("not valid JSON");
	}

	if (!json.contains("tilesets")) {
		throw std::runtime_error("map has no tilesets");
	}
	for (Json& tileset : json["tilesets"]) {
		if (!tileset.contains("source")) {
			continue;
		}
		fs::path source = mapPath.parent_path() / tileset.at("source").get<std::string>();
		source.replace_extension(".json");
		tileset["source"] = source.lexically_normal().generic_string();
	}
	return json.dump();
}

int imageIndex(LevelDefinition& level, const std::string& imageName) {
	const auto found = std::find(level.tileImages.begin(), level.tileImages.end(), imageName);
	if (found != level.tileImages.end()) {
		return static_cast<int>(found - level.tileImages.begin());
	}
	level.tileImages.push_back(imageName);
	return static_cast<int>(level.tileImages.size()) - 1;
}

// In the Walls layer every tile is a wall, except the first tile of the walls tileset, which is empty.
void addTileLayer(LevelDefinition& level, tson::Layer& layer, tson::Map& map, int cellSize) {
	const bool isWallLayer = layer.getName() == WALLS_LAYER;
	const tson::Vector2i tileSize = map.getTileSize();

	for (auto& [position, tileObject] : layer.getTileObjects()) {
		tson::Tile* tile = tileObject.getTile();
		if (tile == nullptr || tile->getTileset() == nullptr) {
			continue;
		}
		const tson::Tileset* tileset = tile->getTileset();
		if (isWallLayer && tileset->getName() == WALLS_TILESET
			&& tile->getGid() == static_cast<uint32_t>(tileset->getFirstgid())) {
			continue;
		}

		const SDL_Rect destination = { std::get<0>(position) * tileSize.x, std::get<1>(position) * tileSize.y,
			tileSize.x, tileSize.y };
		const tson::Rect& source = tileObject.getDrawingRect();
		level.tiles.push_back({ imageIndex(level, tile->getTileset()->getImage().filename().string()),
			{ source.x, source.y, source.width, source.height }, destination });

		if (isWallLayer) {
			level.placements.push_back({ ObjectType::Wall,
				{ destination.x / cellSize, destination.y / cellSize, destination.w / cellSize, destination.h / cellSize } });
		}
	}
}

// The object's own property wins over the one set on its tile in the tileset.
int readClassId(tson::Map& map, tson::Object& object) {
	if (tson::Property* own = object.getProp(CLASS_ID_PROPERTY)) {
		return std::any_cast<int>(own->getValue());
	}
	tson::Tileset* tileset = map.getTilesetByGid(object.getGid());
	if (tileset == nullptr) {
		return 0;
	}
	tson::Tile* tile = tileset->getTile(object.getGid() - tileset->getFirstgid() + 1);
	tson::Property* property = (tile == nullptr) ? nullptr : tile->getProp(CLASS_ID_PROPERTY);
	return (property == nullptr) ? 0 : std::any_cast<int>(property->getValue());
}

// OProps only says where to spawn special entities; its objects are not drawn.
void addProps(LevelDefinition& level, tson::Layer& layer, tson::Map& map, int cellSize) {
	for (tson::Object& object : layer.getObjects()) {
		const int classId = readClassId(map, object);
		if (classId != TOWER_CLASS_ID) {
			std::cerr << "Level " << level.name << ": object " << object.getId() << " in " << PROPS_LAYER
				<< " has unknown classId " << classId << std::endl;
			continue;
		}

		// Tiled anchors tile objects at their bottom-left corner.
		const tson::Vector2i size = object.getSize();
		const int left = object.getPosition().x;
		const int top = object.getPosition().y - size.y;
		level.placements.push_back({ ObjectType::Tower,
			{ left / cellSize, top / cellSize, size.x / cellSize, size.y / cellSize } });
	}
}

LevelDefinition parseLevel(const fs::path& mapPath, int cellSize) {
	const std::string text = readMapWithTilesetPaths(mapPath);
	tson::Tileson tileson;
	std::unique_ptr<tson::Map> map = tileson.parse(text.data(), text.size());
	if (map == nullptr || map->getStatus() != tson::ParseStatus::OK) {
		throw std::runtime_error(map == nullptr ? "map not parsed" : map->getStatusMessage());
	}

	LevelDefinition level;
	level.name = mapPath.stem().string();
	level.width = map->getSize().x * map->getTileSize().x / cellSize;
	level.height = map->getSize().y * map->getTileSize().y / cellSize;

	for (tson::Layer& layer : map->getLayers()) {
		if (layer.getType() == tson::LayerType::TileLayer && layer.isVisible()) {
			addTileLayer(level, layer, *map, cellSize);
		}
	}
	if (tson::Layer* props = map->getLayer(PROPS_LAYER)) {
		addProps(level, *props, *map, cellSize);
	}

	const std::size_t paletteSize = sizeof(DEFAULT_PALETTE) / sizeof(DEFAULT_PALETTE[0]);
	for (const EnemyPersonality personality : DEFAULT_ENEMIES) {
		const ColorPair& colors = DEFAULT_PALETTE[level.enemies.size() % paletteSize];
		level.enemies.push_back({ personality, colors.glow, colors.core, {} });
	}
	level.scatter.assign(std::begin(DEFAULT_SCATTER), std::end(DEFAULT_SCATTER));
	return level;
}
}

namespace LevelLoader {
std::optional<LevelDefinition> load(const std::string& path, int cellSize) {
	try {
		return parseLevel(fs::path(path), cellSize);
	}
	catch (const std::exception& error) {
		std::cerr << "Invalid level " << path << ": " << error.what() << std::endl;
		return std::nullopt;
	}
}
}
