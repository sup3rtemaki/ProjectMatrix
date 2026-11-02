#pragma once

#include "ai/enemyPersonality.h"
#include "cycle.h"
#include "world/objectType.h"

#include <optional>
#include <string>
#include <vector>

// A missing cell or direction means "pick one at random when the level starts".
struct SpawnPoint {
	std::optional<SDL_Point> cell;
	std::optional<Direction> direction;
};

struct EnemyDefinition {
	EnemyPersonality personality;
	SDL_Color glow;
	SDL_Color core;
	SpawnPoint spawn;
};

// An object fixed at an exact place, in grid cells.
struct ObjectPlacement {
	ObjectType type;
	SDL_Rect cells;
};

// Objects dropped at random free places.
struct ObjectScatter {
	ObjectType type;
	int count;
};

// One drawn tile of a Tiled layer.
struct TileSprite {
	int image;            // index into LevelDefinition::tileImages
	SDL_Rect source;      // in the tileset image, pixels
	SDL_Rect destination; // in the world, pixels
};

struct LevelDefinition {
	std::string name = "Unnamed";
	int width = 160;  // in grid cells
	int height = 100;
	float respawnSeconds = 15.0f; // consumed pickups and traps return after this; 0 disables
	SpawnPoint player;
	std::vector<EnemyDefinition> enemies;
	std::vector<ObjectPlacement> placements;
	std::vector<ObjectScatter> scatter;
	std::vector<std::string> tileImages; // texture file names, relative to the textures folder
	std::vector<TileSprite> tiles;       // in layer order, back to front
};
