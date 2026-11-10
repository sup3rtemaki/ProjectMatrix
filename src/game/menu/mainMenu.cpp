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

void MainMenu::setAvailableLobbies(const std::vector<std::string>& lobbyIds) {
	availableLobbies_ = lobbyIds;
	if (screen_ == MainMenuScreen::SearchLobby && !availableLobbies_.empty()) {
		if (index >= static_cast<int>(availableLobbies_.size())) {
			index = static_cast<int>(availableLobbies_.size()) - 1;
		}
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

void MainMenu::setScreenSize(int width, int height) {
	screenWidth_ = std::max(1, width);
	screenHeight_ = std::max(1, height);
	scale_ = std::min(screenWidth_ / 960.0f, screenHeight_ / 640.0f);
	offsetX_ = static_cast<int>((screenWidth_ - 960 * scale_) / 2.0f);
	offsetY_ = static_cast<int>((screenHeight_ - 640 * scale_) / 2.0f);
}

void MainMenu::setDisplayModes(const std::vector<DisplayMode>& modes, std::size_t currentIndex, bool fullscreen) {
	if (modes.empty()) return;
	displayModes_ = modes;
	displayIndex_ = std::min(currentIndex, modes.size() - 1);
	fullscreen_ = fullscreen;
	setScreenSize(displayModes_[displayIndex_].width, displayModes_[displayIndex_].height);
}

SDL_Rect MainMenu::designRect(int x, int y, int w, int h) const {
	return { offsetX_ + static_cast<int>(x * scale_), offsetY_ + static_cast<int>(y * scale_),
		std::max(1, static_cast<int>(w * scale_)), std::max(1, static_cast<int>(h * scale_)) };
}

void MainMenu::renderString(const std::string& text, int x, int y, SDL_Color color, int fontSize, bool center) {
	if (text.empty() || renderer_ == nullptr) return;
	const std::string font = resolveFontPath(fontFile_);
	if (font.empty()) return;

	SDL_Texture* tex = renderText(text, font, color, std::max(1, static_cast<int>(fontSize * scale_ + 0.5f)), renderer_);
	if (tex != nullptr) {
		int w = 0, h = 0;
		SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
		const int baseX = offsetX_ + static_cast<int>(x * scale_);
		const int drawX = center ? (baseX - w / 2) : baseX;
		SDL_Rect dst = { drawX, offsetY_ + static_cast<int>(y * scale_), w, h };
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
	// Dark retro grid background covering the whole screen
	SDL_SetRenderDrawColor(renderer_, 6, 10, 16, 255);
	SDL_Rect bg = { 0, 0, screenWidth_, screenHeight_ };
	SDL_RenderFillRect(renderer_, &bg);

	SDL_SetRenderDrawColor(renderer_, 14, 22, 34, 255);
	const float step = std::max(8.0f, 32.0f * scale_);
	for (float x = static_cast<float>(offsetX_ % static_cast<int>(step)); x < screenWidth_; x += step) {
		SDL_RenderDrawLine(renderer_, static_cast<int>(x), 0, static_cast<int>(x), screenHeight_);
	}
	for (float y = static_cast<float>(offsetY_ % static_cast<int>(step)); y < screenHeight_; y += step) {
		SDL_RenderDrawLine(renderer_, 0, static_cast<int>(y), screenWidth_, static_cast<int>(y));
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
	case MainMenuScreen::SearchLobby:
		drawSearchLobby();
		break;
	case MainMenuScreen::Options:
		drawOptions();
		break;
	}
}

void MainMenu::drawOptions() {
	renderString("- OPTIONS -", 480, 170, { 200, 220, 240, 255 }, 22, true);

	const DisplayMode& mode = displayModes_[displayIndex_];
	const std::vector<std::pair<std::string, std::string>> rows = {
		{ "RESOLUTION", "< " + std::to_string(mode.width) + " x " + std::to_string(mode.height) + " >" },
		{ "DISPLAY MODE", std::string("< ") + (fullscreen_ ? "Fullscreen" : "Windowed") + " >" },
		{ "BACK", "" }
	};
	const int startY = 250;
	const int rowSpacing = 55;
	for (std::size_t i = 0; i < rows.size(); ++i) {
		const bool selected = (index == static_cast<int>(i));
		const SDL_Color labelColor = selected ? SDL_Color{ 20, 255, 170, 255 } : SDL_Color{ 160, 185, 210, 255 };
		const SDL_Color valueColor = selected ? SDL_Color{ 255, 255, 255, 255 } : SDL_Color{ 140, 170, 200, 255 };
		const int y = startY + static_cast<int>(i) * rowSpacing;
		renderString((selected ? ">  " : "   ") + rows[i].first, 250, y, labelColor, 22, false);
		renderString(rows[i].second, 560, y, valueColor, 22, false);
	}

	renderString("UP / DOWN: Navigate   |   LEFT / RIGHT: Change   |   ESC: Back", 480, 580, { 100, 120, 145, 255 }, 16, true);
}

void MainMenu::drawRootMenu() {
	const std::vector<std::string> items = { "PLAY", "OPTIONS", "EXIT" };
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

void MainMenu::drawSearchLobby() {
	renderString("- SEARCH LOBBIES -", 480, 150, { 30, 230, 182, 255 }, 24, true);

	if (availableLobbies_.empty()) {
		renderString("NO LOBBIES FOUND", 480, 250, { 180, 200, 220, 255 }, 22, true);
		renderString("Press ENTER to refresh the lobby list", 480, 290, { 140, 170, 210, 255 }, 18, true);
		renderString("ESC: Back", 480, 580, { 100, 120, 145, 255 }, 16, true);
		return;
	}

	const int startY = 210;
	const int itemSpacing = 34;
	for (std::size_t i = 0; i < availableLobbies_.size(); ++i) {
		const bool selected = (index == static_cast<int>(i));
		const SDL_Color textColor = selected ? SDL_Color{ 20, 255, 170, 255 } : SDL_Color{ 180, 205, 230, 255 };
		const std::string prefix = selected ? ">  " : "   ";
		renderString(prefix + "Lobby " + availableLobbies_[i], 220, startY + static_cast<int>(i) * itemSpacing, textColor, 20, false);
	}

	renderString("UP / DOWN: Select Lobby   |   ENTER: Join   |   ESC: Back", 480, 580, { 100, 120, 145, 255 }, 16, true);
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
	std::string modeDisplay = config_.onlineMode ? "Steam Lobby" : "Free For All";
	std::string playersDisplay = config_.onlineMode ? std::to_string(config_.totalPlayers) + " Players" : std::to_string(config_.totalPlayers) + " (Online disabled)";
	std::string aiDisplay = std::to_string(config_.aiCount) + " Opponents";

	const std::vector<ConfigRow> rows = {
		{ "GAME MODE",       "< " + modeDisplay + " >",     "Toggle local vs online lobby" },
		{ "MAP / LEVEL",      "< " + mapDisplay + " >",       "Select arena level" },
		{ "PLAYER COLOR",   "< " + colorDisplay + " >",     "Lightcycle glow/core color" },
		{ "HUMAN PLAYERS",  "< " + playersDisplay + " >",   "Includes the local player" },
		{ "AI OPPONENTS",    "< " + aiDisplay + " >",        "0 to 5 AI bots" },
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
			const SDL_Rect glow = designRect(720, startY + static_cast<int>(i) * rowSpacing + 2, 20, 20);
			const SDL_Rect core = designRect(723, startY + static_cast<int>(i) * rowSpacing + 5, 14, 14);
			SDL_SetRenderDrawColor(renderer_, config_.playerGlow.r, config_.playerGlow.g, config_.playerGlow.b, 255);
			SDL_RenderFillRect(renderer_, &glow);
			SDL_SetRenderDrawColor(renderer_, config_.playerCore.r, config_.playerCore.g, config_.playerCore.b, 255);
			SDL_RenderFillRect(renderer_, &core);
		}
	}

	renderString("UP / DOWN: Select Option   |   LEFT / RIGHT: Change Value   |   ENTER: Confirm", 480, 580, { 100, 120, 145, 255 }, 16, true);
}

int MainMenu::maxIndexForScreen() const {
	switch (screen_) {
	case MainMenuScreen::PlaySubmenu: return 2;
	case MainMenuScreen::HostConfig: return 6;
	case MainMenuScreen::Options: return 2;
	case MainMenuScreen::SearchLobby:
		return availableLobbies_.empty() ? 0 : static_cast<int>(availableLobbies_.size()) - 1;
	case MainMenuScreen::Root: break;
	}
	return 2;
}

void MainMenu::changeDisplay(int delta, bool toggleFullscreen) {
	if (toggleFullscreen) {
		fullscreen_ = !fullscreen_;
	}
	else {
		const int count = static_cast<int>(displayModes_.size());
		displayIndex_ = static_cast<std::size_t>((static_cast<int>(displayIndex_) + delta + count) % count);
	}
	const DisplayMode& mode = displayModes_[displayIndex_];
	setScreenSize(mode.width, mode.height);
	if (onDisplayChanged_) onDisplayChanged_(mode, fullscreen_);
}

void MainMenu::onIndexUp() {
	const int maxIndex = maxIndexForScreen();

	--index;
	if (index < 0) index = maxIndex;
}

void MainMenu::onIndexDown() {
	const int maxIndex = maxIndexForScreen();

	++index;
	if (index > maxIndex) index = 0;
}

void MainMenu::onIndexLeft() {
	if (screen_ == MainMenuScreen::Options) {
		if (index == 0) changeDisplay(-1, false);
		else if (index == 1) changeDisplay(0, true);
		return;
	}
	if (screen_ != MainMenuScreen::HostConfig) return;

	switch (index) {
	case 0: // Game mode
		config_.onlineMode = !config_.onlineMode;
		config_.gameMode = config_.onlineMode ? "Steam Lobby" : "Free For All";
		if (config_.onlineMode) {
			config_.totalPlayers = std::max(1, config_.totalPlayers);
			config_.aiCount = std::max(0, config_.aiCount);
		}
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
	case 3: // Human players (1 to 8)
		if (config_.totalPlayers > 1) {
			--config_.totalPlayers;
		}
		break;
	case 4: // AI count (0 to 5)
		if (config_.aiCount > 0) {
			--config_.aiCount;
		}
		break;
	default:
		break;
	}
}

void MainMenu::onIndexRight() {
	if (screen_ == MainMenuScreen::Options) {
		if (index == 0) changeDisplay(1, false);
		else if (index == 1) changeDisplay(0, true);
		return;
	}
	if (screen_ != MainMenuScreen::HostConfig) return;

	switch (index) {
	case 0: // Game mode
		config_.onlineMode = !config_.onlineMode;
		config_.gameMode = config_.onlineMode ? "Steam Lobby" : "Free For All";
		if (config_.onlineMode) {
			config_.totalPlayers = std::max(1, config_.totalPlayers);
			config_.aiCount = std::max(0, config_.aiCount);
		}
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
	case 4: // AI count (0 to 5)
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
	case MainMenuScreen::SearchLobby:
		handleSearchLobbySelect();
		break;
	case MainMenuScreen::Options:
		if (index == 2) {
			screen_ = MainMenuScreen::Root;
			index = 1;
		}
		else {
			onIndexRight();
		}
		break;
	}
}

void MainMenu::handleRootSelect() {
	switch (index) {
	case 0: // PLAY
		screen_ = MainMenuScreen::PlaySubmenu;
		index = 1; // Default to "HOST GAME"
		break;
	case 1: // OPTIONS
		screen_ = MainMenuScreen::Options;
		index = 0;
		break;
	case 2: // EXIT
		if (onQuit_) onQuit_();
		break;
	}
}

void MainMenu::handlePlaySubmenuSelect() {
	switch (index) {
	case 0: // SEARCH GAME
		config_.onlineMode = true;
		config_.gameMode = "Steam Lobby";
		config_.lobbyId.clear();
		config_.totalPlayers = std::max(1, config_.totalPlayers);
		config_.aiCount = std::max(0, config_.aiCount);
		screen_ = MainMenuScreen::SearchLobby;
		index = 0;
		break;
	case 1: // HOST GAME
		config_.onlineMode = true;
		config_.gameMode = "Steam Lobby";
		config_.lobbyId.clear();
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
		if (config_.onlineMode) {
			config_.gameMode = "Steam Lobby";
		}
		config_.totalPlayers = std::max(1, config_.totalPlayers);
		config_.aiCount = std::max(0, config_.aiCount);
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

void MainMenu::handleSearchLobbySelect() {
	if (availableLobbies_.empty()) {
		return;
	}
	config_.onlineMode = true;
	config_.gameMode = "Steam Lobby";
	config_.lobbyId = availableLobbies_[index];
	config_.totalPlayers = std::max(1, config_.totalPlayers);
	config_.aiCount = std::max(0, config_.aiCount);
	if (onStartGame_) onStartGame_(config_);
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
	case MainMenuScreen::SearchLobby:
		screen_ = MainMenuScreen::PlaySubmenu;
		index = 0;
		break;
	case MainMenuScreen::Options:
		screen_ = MainMenuScreen::Root;
		index = 1;
		break;
	}
}
