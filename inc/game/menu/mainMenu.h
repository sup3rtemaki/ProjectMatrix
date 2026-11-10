#pragma once

#include "menu/gameConfig.h"
#include "menu/menu.h"

#include <functional>
#include <string>
#include <vector>

enum class MainMenuScreen {
	Root,
	PlaySubmenu,
	HostConfig,
	SearchLobby,
	Options
};

struct DisplayMode {
	int width = 0;
	int height = 0;
	bool operator==(const DisplayMode& other) const { return width == other.width && height == other.height; }
};

class MainMenu : public Menu {
public:
	MainMenu();
	~MainMenu() override;

	void setRenderer(SDL_Renderer* rendererContext) override;
	void draw() override;

	void onIndexUp() override;
	void onIndexDown() override;
	void onIndexLeft() override;
	void onIndexRight() override;
	void onSelect() override;
	void onBack() override;

	void setAvailableLevels(const std::vector<std::string>& levelNames);
	void setAvailableLobbies(const std::vector<std::string>& lobbyIds);
	const GameConfig& currentConfig() const { return config_; }

	void setStartGameCallback(std::function<void(const GameConfig&)> cb) { onStartGame_ = cb; }
	void setQuitCallback(std::function<void()> cb) { onQuit_ = cb; }

	MainMenuScreen currentScreen() const { return screen_; }

	// The menu lays itself out in a 960x640 design space scaled and centered to the screen size.
	void setScreenSize(int width, int height);
	void setDisplayModes(const std::vector<DisplayMode>& modes, std::size_t currentIndex, bool fullscreen);
	void setDisplayChangedCallback(std::function<void(const DisplayMode&, bool)> cb) { onDisplayChanged_ = cb; }
	const DisplayMode& currentDisplayMode() const { return displayModes_[displayIndex_]; }
	bool fullscreen() const { return fullscreen_; }

private:
	MainMenuScreen screen_ = MainMenuScreen::Root;
	GameConfig config_;
	std::vector<std::string> availableLevelNames_;
	std::vector<std::string> availableLobbies_;
	std::vector<DisplayMode> displayModes_ = { { 960, 640 } };
	std::size_t displayIndex_ = 0;
	bool fullscreen_ = false;
	int screenWidth_ = 960;
	int screenHeight_ = 640;
	float scale_ = 1.0f;
	int offsetX_ = 0;
	int offsetY_ = 0;

	std::function<void(const GameConfig&)> onStartGame_;
	std::function<void()> onQuit_;
	std::function<void(const DisplayMode&, bool)> onDisplayChanged_;

	std::string fontFile_ = "fonts/vermin_vibes_1989.ttf";
	std::string fallbackFontFile_ = "fonts/Berylium.ttf";

	std::string resolveFontPath(const std::string& relative) const;

	void drawMenuBackground() override;
	void drawText() override;

	void drawRootMenu();
	void drawPlaySubmenu();
	void drawHostConfig();
	void drawSearchLobby();
	void drawOptions();

	void handleRootSelect();
	void handlePlaySubmenuSelect();
	void handleHostConfigSelect();
	void handleSearchLobbySelect();
	void changeDisplay(int delta, bool toggleFullscreen);
	int maxIndexForScreen() const;
	SDL_Rect designRect(int x, int y, int w, int h) const;

	void renderString(const std::string& text, int x, int y, SDL_Color color, int fontSize, bool center = false);
};
