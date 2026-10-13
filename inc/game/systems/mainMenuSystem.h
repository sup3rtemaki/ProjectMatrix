#pragma once

#include "menu/mainMenu.h"
#include "renderContext.h"

#include "SDL.h"
#include <functional>

class MainMenuSystem {
public:
	MainMenuSystem(MainMenu* menu, RenderContext& context);
	~MainMenuSystem();

	bool update(SDL_Event& event);
	void render();

	void setStartGameCallback(std::function<void(const GameConfig&)> cb);
	void setQuitCallback(std::function<void()> cb);

private:
	MainMenu* menu_ = nullptr;
	RenderContext& context_;
	std::function<void(const GameConfig&)> startGameCallback_;
	std::function<void()> quitCallback_;

	void handleKeyDown(SDL_Keycode scancode);
};
