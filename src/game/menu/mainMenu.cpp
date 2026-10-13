#include "menu/mainMenu.h"
#include "drawing_functions.h"

#include <algorithm>
#include <fstream>

MainMenu::MainMenu() {
	menuState = MenuState::Active;
	screen_ = MainMenuScreen::Root;
	index = 0;
	availableLevelNames_ = { "Level 1" };
}

MainMenu::~MainMenu() = default;

void MainMenu::setRenderer(SDL_Renderer* rendererContext) {
	Ui::setRenderer(rendererContext);
}

void MainMenu::setAvailableLevels(const std::vector<std::string>& levelNames) {
	availableLevelNames_ = levelNames;
	if (config_.levelIndex >= availableLevelNames_.size()) {
		config_.levelIndex = 0;
	}
	if (!availableLevelNames_.empty()) {
		config_.levelName = availableLevelNames_[config_.levelIndex];
	}
}

std::string MainMenu::resolveFontPath(const std::string& relative) const {
	const std::string full = resPath_ + relative;
	if (std::ifstream(full).good()) {
		return full;
	}
	const std::string fallback = resPath_ + fallbackFontFile_;
	if (std::ifstream(fallback).good()) {
		return fallback;
	}
	return "";
}

void MainMenu::renderString(const std::string& text, int x, int y, SDL_Color color, int fontSize, bool center) {
	if (text.empty() || renderer_ == nullptr) return;
	const std::string font = resolveFontPath(fontFile_);
	if (font.empty()) return;

	SDL_Texture* tex = renderText(text, font, color, fontSize, renderer_);
	if (tex != nullptr) {
		int w = 0, h = 0;
		SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
		int drawX = center ? (x - w / 2) : x;
		SDL_Rect dst = { drawX, y, w, h };
		SDL_RenderCopy(renderer_, tex, nullptr, &dst);
		SDL_DestroyTexture(tex);
	}
}

void MainMenu::draw() {
	if (menuState == MenuState::Inactive || renderer_ == nullptr) return;

	drawMenuBackground();
	drawText();
}

void MainMenu::drawMenuBackground() {
	// Dark retro grid background
	SDL_SetRenderDrawColor(renderer_, 6, 10, 16, 255);
	SDL_Rect bg = { 0, 0, 960, 640 };
	SDL_RenderFillRect(renderer_, &bg);

	// Grid lines
	SDL_SetRenderDrawColor(renderer_, 14, 22, 34, 255);
	for (int x = 0; x < 960; x += 32) {
		SDL_RenderDrawLine(renderer_, x, 0, x, 640);
	}
	for (int y = 0; y < 640; y += 32) {
		SDL_RenderDrawLine(renderer_, 0, y, 960, y);
	}

	// Title
	renderString("VERTEX2D", 480, 50, { 20, 235, 255, 255 }, 42, true);
	renderString("LIGHTCYCLES", 480, 100, { 255, 155, 35, 255 }, 28, true);
}

void MainMenu::drawText() {
	switch (screen_) {
	case MainMenuScreen::Root:
		drawRootMenu();
		break;
	case MainMenuScreen::PlaySubmenu:
		drawPlaySubmenu();
		break;
	case MainMenuScreen::HostConfig:
		drawHostConfig();
		break;
	}
}

void MainMenu::drawRootMenu() {
	const std::vector<std::string> items = { "PLAY", "EXIT" };
	const int startY = 260;
	const int itemSpacing = 60;

	for (std::size_t i = 0; i < items.size(); ++i) {
		const bool selected = (index == static_cast<int>(i));
		const SDL_Color textColor = selected ? SDL_Color{ 20, 255, 170, 255 } : SDL_Color{ 160, 185, 210, 255 };
		const std::string prefix = selected ? ">  " : "   ";
		renderString(prefix + items[i], 420, startY + static_cast<int>(i) * itemSpacing, textColor, 26, false);
	}

	renderString("UP / DOWN: Navigate   |   ENTER / SPACE: Select", 480, 580, { 100, 120, 145, 255 }, 16, true);
}

void MainMenu::drawPlaySubmenu() {
	renderString("- SELECT MODE -", 480, 170, { 200, 220, 240, 255 }, 20, true);

	const std::vector<std::pair<std::string, std::string>> items = {
		{ "SEARCH GAME", " (Online match - Coming soon)" },
		{ "HOST GAME",   " (Create custom lobby)" },
		{ "BACK",        "" }
	};
	const int startY = 240;
	const int itemSpacing = 55;

	for (std::size_t i = 0; i < items.size(); ++i) {
		const bool selected = (index == static_cast<int>(i));
		const SDL_Color textColor = selected ? SDL_Color{ 20, 255, 170, 255 } : SDL_Color{ 160, 185, 210, 255 };
		const std::string prefix = selected ? ">  " : "   ";
		renderString(prefix + items[i].first, 340, startY + static_cast<int>(i) * itemSpacing, textColor, 22, false);

		if (!items[i].second.empty()) {
			renderString(items[i].second, 510, startY + static_cast<int>(i) * itemSpacing + 4, { 110, 130, 150, 255 }, 16, false);
		}
	}

	renderString("UP / DOWN: Navigate   |   ENTER: Select   |   ESC: Back", 480, 580, { 100, 120, 145, 255 }, 16, true);
}

void MainMenu::drawHostConfig() {
	renderString("- HOST ROOM CONFIGURATION -", 480, 155, { 255, 210, 50, 255 }, 22, true);

	struct ConfigRow {
		std::string label;
		std::string value;
		std::string note;
	};

	std::string mapDisplay = availableLevelNames_.empty()
		? "Default"
		: availableLevelNames_[config_.levelIndex % availableLevelNames_.size()];

	std::string colorDisplay = PLAYER_COLOR_OPTIONS[config_.colorIndex].name;
	std::string playersDisplay = std::to_string(config_.totalPlayers) + " (Online disabled)";
	std::string aiDisplay = std::to_string(config_.aiCount) + " Opponents";

	const std::vector<ConfigRow> rows = {
		{ "GAME MODE",       "< " + config_.gameMode + " >", "Only Free For All available" },
		{ "MAP / LEVEL",      "< " + mapDisplay + " >",       "Select arena level" },
		{ "PLAYER COLOR",   "< " + colorDisplay + " >",     "Lightcycle glow/core color" },
		{ "HUMAN PLAYERS",  "< " + playersDisplay + " >",   "Fills with AIs currently" },
		{ "AI OPPONENTS",    "< " + aiDisplay + " >",        "1 to 5 AI bots" },
		{ "START GAME",     "[ PLAY NOW ]",                 "Launch match with settings" },
		{ "BACK",           "[ CANCEL ]",                   "Return to menu" }
	};

	const int startY = 205;
	const int rowSpacing = 42;

	for (std::size_t i = 0; i < rows.size(); ++i) {
		const bool selected = (index == static_cast<int>(i));
		const SDL_Color labelColor = selected ? SDL_Color{ 20, 255, 170, 255 } : SDL_Color{ 180, 205, 230, 255 };
		const SDL_Color valueColor = selected ? SDL_Color{ 255, 255, 255, 255 } : SDL_Color{ 140, 170, 200, 255 };
		const std::string prefix = selected ? ">  " : "   ";

		renderString(prefix + rows[i].label, 180, startY + static_cast<int>(i) * rowSpacing, labelColor, 18, false);
		renderString(rows[i].value, 460, startY + static_cast<int>(i) * rowSpacing, valueColor, 18, false);

		// Color preview square for PLAYER COLOR row (index 2)
		if (i == 2) {
			const SDL_Rect glow = { 720, startY + static_cast<int>(i) * rowSpacing + 2, 20, 20 };
			const SDL_Rect core = { 723, startY + static_cast<int>(i) * rowSpacing + 5, 14, 14 };
			SDL_SetRenderDrawColor(renderer_, config_.playerGlow.r, config_.playerGlow.g, config_.playerGlow.b, 255);
			SDL_RenderFillRect(renderer_, &glow);
			SDL_SetRenderDrawColor(renderer_, config_.playerCore.r, config_.playerCore.g, config_.playerCore.b, 255);
			SDL_RenderFillRect(renderer_, &core);
		}
	}

	renderString("UP / DOWN: Select Option   |   LEFT / RIGHT: Change Value   |   ENTER: Confirm", 480, 580, { 100, 120, 145, 255 }, 16, true);
}

void MainMenu::onIndexUp() {
	int maxIndex = 1;
	if (screen_ == MainMenuScreen::PlaySubmenu) maxIndex = 2;
	else if (screen_ == MainMenuScreen::HostConfig) maxIndex = 6;

	--index;
	if (index < 0) index = maxIndex;
}

void MainMenu::onIndexDown() {
	int maxIndex = 1;
	if (screen_ == MainMenuScreen::PlaySubmenu) maxIndex = 2;
	else if (screen_ == MainMenuScreen::HostConfig) maxIndex = 6;

	++index;
	if (index > maxIndex) index = 0;
}

void MainMenu::onIndexLeft() {
	if (screen_ != MainMenuScreen::HostConfig) return;

	switch (index) {
	case 0: // Game mode (only Free For All for now)
		break;
	case 1: // Map
		if (!availableLevelNames_.empty()) {
			if (config_.levelIndex == 0) {
				config_.levelIndex = availableLevelNames_.size() - 1;
			}
			else {
				--config_.levelIndex;
			}
			config_.levelName = availableLevelNames_[config_.levelIndex];
		}
		break;
	case 2: // Player color
		if (config_.colorIndex == 0) {
			config_.colorIndex = static_cast<int>(NUM_PLAYER_COLOR_OPTIONS - 1);
		}
		else {
			--config_.colorIndex;
		}
		config_.playerGlow = PLAYER_COLOR_OPTIONS[config_.colorIndex].glow;
		config_.playerCore = PLAYER_COLOR_OPTIONS[config_.colorIndex].core;
		break;
	case 3: // Human players (2 to 8, currently fills with AI)
		if (config_.totalPlayers > 2) {
			--config_.totalPlayers;
		}
		break;
	case 4: // AI count (1 to 5)
		if (config_.aiCount > 1) {
			--config_.aiCount;
		}
		break;
	default:
		break;
	}
}

void MainMenu::onIndexRight() {
	if (screen_ != MainMenuScreen::HostConfig) return;

	switch (index) {
	case 0: // Game mode
		break;
	case 1: // Map
		if (!availableLevelNames_.empty()) {
			config_.levelIndex = (config_.levelIndex + 1) % availableLevelNames_.size();
			config_.levelName = availableLevelNames_[config_.levelIndex];
		}
		break;
	case 2: // Player color
		config_.colorIndex = (config_.colorIndex + 1) % NUM_PLAYER_COLOR_OPTIONS;
		config_.playerGlow = PLAYER_COLOR_OPTIONS[config_.colorIndex].glow;
		config_.playerCore = PLAYER_COLOR_OPTIONS[config_.colorIndex].core;
		break;
	case 3: // Human players
		if (config_.totalPlayers < 8) {
			++config_.totalPlayers;
		}
		break;
	case 4: // AI count (1 to 5)
		if (config_.aiCount < 5) {
			++config_.aiCount;
		}
		break;
	default:
		break;
	}
}

void MainMenu::onSelect() {
	switch (screen_) {
	case MainMenuScreen::Root:
		handleRootSelect();
		break;
	case MainMenuScreen::PlaySubmenu:
		handlePlaySubmenuSelect();
		break;
	case MainMenuScreen::HostConfig:
		handleHostConfigSelect();
		break;
	}
}

void MainMenu::handleRootSelect() {
	switch (index) {
	case 0: // PLAY
		screen_ = MainMenuScreen::PlaySubmenu;
		index = 1; // Default to "HOST GAME"
		break;
	case 1: // EXIT
		if (onQuit_) onQuit_();
		break;
	}
}

void MainMenu::handlePlaySubmenuSelect() {
	switch (index) {
	case 0: // SEARCH GAME (does nothing yet)
		break;
	case 1: // HOST GAME
		screen_ = MainMenuScreen::HostConfig;
		index = 5; // Default focus to "START GAME"
		break;
	case 2: // BACK
		screen_ = MainMenuScreen::Root;
		index = 0;
		break;
	}
}

void MainMenu::handleHostConfigSelect() {
	switch (index) {
	case 5: // START GAME
		if (onStartGame_) onStartGame_(config_);
		break;
	case 6: // BACK
		screen_ = MainMenuScreen::PlaySubmenu;
		index = 1;
		break;
	default:
		// Hitting enter on a configurable row moves right to cycle values
		onIndexRight();
		break;
	}
}

void MainMenu::onBack() {
	switch (screen_) {
	case MainMenuScreen::Root:
		if (onQuit_) onQuit_();
		break;
	case MainMenuScreen::PlaySubmenu:
		screen_ = MainMenuScreen::Root;
		index = 0;
		break;
	case MainMenuScreen::HostConfig:
		screen_ = MainMenuScreen::PlaySubmenu;
		index = 1;
		break;
	}
}
