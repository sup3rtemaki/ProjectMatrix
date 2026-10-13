#include "level/levelLoader.h"

#include "jsonFileStore.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
using Json = nlohmann::json;

struct ColorPair {
	SDL_Color glow;
	SDL_Color core;
};

// Used by enemies that do not define their own colors.
const ColorPair DEFAULT_PALETTE[] = {
	{ { 105, 15, 30, 255 }, { 255, 45, 75, 255 } },
	{ { 105, 50, 5, 255 }, { 255, 155, 35, 255 } },
	{ { 65, 20, 115, 255 }, { 185, 95, 255, 255 } },
	{ { 10, 95, 45, 255 }, { 60, 255, 130, 255 } },
	{ { 110, 95, 5, 255 }, { 255, 235, 50, 255 } },
	{ { 120, 20, 90, 255 }, { 255, 90, 200, 255 } }
};

Direction parseDirection(const std::string& name) {
	if (name == "north") return Direction::North;
	if (name == "east") return Direction::East;
	if (name == "south") return Direction::South;
	if (name == "west") return Direction::West;
	throw std::runtime_error("unknown direction '" + name + "'");
}

EnemyPersonality parsePersonality(const std::string& name) {
	if (name == "aggressor") return EnemyPersonality::Aggressor;
	if (name == "ambusher") return EnemyPersonality::Ambusher;
	if (name == "strategist") return EnemyPersonality::Strategist;
	if (name == "coward") return EnemyPersonality::Coward;
	if (name == "erratic") return EnemyPersonality::Erratic;
	throw std::runtime_error("unknown personality '" + name + "'");
}

ObjectType parseObjectType(const std::string& name) {
	if (name == "wall") return ObjectType::Wall;
	if (name == "boost") return ObjectType::BoostPickup;
	if (name == "longTrail") return ObjectType::LongTrailPickup;
	if (name == "mine") return ObjectType::MineTrap;
	if (name == "slow") return ObjectType::SlowTrap;
	throw std::runtime_error("unknown object type '" + name + "'");
}

SDL_Color parseColor(const Json& node, const char* key, SDL_Color fallback) {
	if (!node.contains(key)) {
		return fallback;
	}
	const Json& rgb = node.at(key);
	return { rgb.at(0).get<Uint8>(), rgb.at(1).get<Uint8>(), rgb.at(2).get<Uint8>(), 255 };
}

SpawnPoint parseSpawn(const Json& node) {
	SpawnPoint spawn;
	if (node.contains("position")) {
		const Json& position = node.at("position");
		spawn.cell = SDL_Point{ position.at(0).get<int>(), position.at(1).get<int>() };
	}
	if (node.contains("direction")) {
		spawn.direction = parseDirection(node.at("direction").get<std::string>());
	}
	return spawn;
}

LevelDefinition parseLevel(const Json& json) {
	LevelDefinition level;
	level.name = json.value("name", level.name);
	if (json.contains("size")) {
		level.width = json.at("size").at(0).get<int>();
		level.height = json.at("size").at(1).get<int>();
	}
	level.respawnSeconds = json.value("respawnSeconds", level.respawnSeconds);
	level.player = parseSpawn(json.value("player", Json::object()));

	if (json.contains("enemies")) {
		const std::size_t paletteSize = sizeof(DEFAULT_PALETTE) / sizeof(DEFAULT_PALETTE[0]);
		for (const Json& node : json.at("enemies")) {
			const ColorPair& colors = DEFAULT_PALETTE[level.enemies.size() % paletteSize];
			level.enemies.push_back({ parsePersonality(node.at("personality").get<std::string>()),
				parseColor(node, "glow", colors.glow), parseColor(node, "core", colors.core),
				parseSpawn(node) });
		}
	}

	if (json.contains("objects")) {
		for (const Json& node : json.at("objects")) {
			const Json& cells = node.at("cells");
			level.placements.push_back({ parseObjectType(node.at("type").get<std::string>()),
				{ cells.at(0).get<int>(), cells.at(1).get<int>(), cells.at(2).get<int>(), cells.at(3).get<int>() } });
		}
	}

	if (json.contains("scatter")) {
		for (const Json& node : json.at("scatter")) {
			ObjectScatter scatter{ parseObjectType(node.at("type").get<std::string>()), node.at("count").get<int>() };
			scatter.minLength = node.value("minLength", scatter.minLength);
			scatter.maxLength = std::max(scatter.minLength, node.value("maxLength", scatter.maxLength));
			level.scatter.push_back(scatter);
		}
	}
	return level;
}
}

namespace LevelLoader {
std::optional<LevelDefinition> load(const std::string& path) {
	Json json;
	switch (JsonFileStore::readJsonFile(path, json)) {
	case JsonFileResult::Success:
		break;
	case JsonFileResult::FileNotFound:
		std::cerr << "Level not found: " << path << std::endl;
		return std::nullopt;
	case JsonFileResult::ParseError:
		std::cerr << "Level is not valid JSON: " << path << std::endl;
		return std::nullopt;
	}

	try {
		return parseLevel(json);
	}
	catch (const std::exception& error) {
		std::cerr << "Invalid level " << path << ": " << error.what() << std::endl;
		return std::nullopt;
	}
}
}
