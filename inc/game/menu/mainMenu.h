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
	SearchLobby
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

private:
	MainMenuScreen screen_ = MainMenuScreen::Root;
	GameConfig config_;
	std::vector<std::string> availableLevelNames_;
	std::vector<std::string> availableLobbies_;

	std::function<void(const GameConfig&)> onStartGame_;
	std::function<void()> onQuit_;

	std::string fontFile_ = "fonts/vermin_vibes_1989.ttf";
	std::string fallbackFontFile_ = "fonts/Berylium.ttf";

	std::string resolveFontPath(const std::string& relative) const;

	void drawMenuBackground() override;
	void drawText() override;

	void drawRootMenu();
	void drawPlaySubmenu();
	void drawHostConfig();
	void drawSearchLobby();

	void handleRootSelect();
	void handlePlaySubmenuSelect();
	void handleHostConfigSelect();
	void handleSearchLobbySelect();

	void renderString(const std::string& text, int x, int y, SDL_Color color, int fontSize, bool center = false);
};
