#pragma once

#include "SDL.h"
#include <string>

struct GameConfig {
	std::string gameMode = "Free For All";
	std::size_t levelIndex = 0;
	std::string levelName = "Open Arena";
	int colorIndex = 0;
	SDL_Color playerGlow = { 0, 90, 135, 255 };
	SDL_Color playerCore = { 20, 235, 255, 255 };
	int totalPlayers = 4; // Total human players (currently 1 local)
	int aiCount = 3;      // Number of AI opponents
};

struct PlayerColorOption {
	std::string name;
	SDL_Color glow;
	SDL_Color core;
};

inline const PlayerColorOption PLAYER_COLOR_OPTIONS[] = {
	{ "Cyan (Default)", { 0, 90, 135, 255 }, { 20, 235, 255, 255 } },
	{ "Blue",           { 15, 45, 140, 255 }, { 60, 130, 255, 255 } },
	{ "Green",          { 10, 95, 45, 255 },  { 60, 255, 130, 255 } },
	{ "Yellow",         { 110, 95, 5, 255 },  { 255, 235, 50, 255 } },
	{ "Orange",         { 105, 50, 5, 255 },  { 255, 155, 35, 255 } },
	{ "Red",            { 105, 15, 30, 255 }, { 255, 45, 75, 255 } },
	{ "Purple",         { 65, 20, 115, 255 }, { 185, 95, 255, 255 } },
	{ "White",          { 80, 80, 90, 255 },  { 240, 245, 255, 255 } }
};

inline constexpr std::size_t NUM_PLAYER_COLOR_OPTIONS = sizeof(PLAYER_COLOR_OPTIONS) / sizeof(PLAYER_COLOR_OPTIONS[0]);
