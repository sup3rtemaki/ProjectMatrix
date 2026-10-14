#include "SDL.h"
#include "SDL_ttf.h"
#include "lightcyclesGame.h"
#include "menu/mainMenu.h"
#include "network/steamSession.h"
#include "res_path.h"
#include "systems/mainMenuSystem.h"
#include "timeController.h"

#include <cstdlib>
#include <ctime>

int main(int, char**) {
	std::srand(static_cast<unsigned int>(std::time(nullptr)));

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
		return 1;
	}

	if (TTF_Init() != 0) {
		SDL_Quit();
		return 1;
	}

	SDL_Window* window = SDL_CreateWindow("Vertex2D Lightcycles",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		LightcyclesGame::WINDOW_WIDTH, LightcyclesGame::WINDOW_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	if (window == nullptr) {
		SDL_Quit();
		return 1;
	}

	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (renderer == nullptr) {
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	}
	if (renderer == nullptr) {
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
	SDL_RenderSetLogicalSize(renderer,
		LightcyclesGame::WINDOW_WIDTH,
		LightcyclesGame::WINDOW_HEIGHT);

	{
		SteamSessionService steamSession;
		steamSession.initialize();

		LightcyclesGame game(renderer);
		MainMenu mainMenu;
		mainMenu.setRenderer(renderer);
		mainMenu.setResourcePath(getResourcePath());
		mainMenu.setAvailableLevels(game.getAvailableLevelNames());

		RenderContext menuRenderContext;
		menuRenderContext.renderer = renderer;
		menuRenderContext.camera = { 0, 0, LightcyclesGame::WINDOW_WIDTH, LightcyclesGame::WINDOW_HEIGHT };

		MainMenuSystem menuSystem(&mainMenu, menuRenderContext);

		bool appRunning = true;

		menuSystem.setStartGameCallback([&](const GameConfig& config) {
			game.startMatch(config);
		});

		menuSystem.setQuitCallback([&]() {
			appRunning = false;
		});

		TimeController gameTime;
		gameTime.reset();

		while (appRunning && !game.isExitRequested()) {
			SDL_Event event;
			while (SDL_PollEvent(&event)) {
				if (game.isInMenu()) {
					menuSystem.update(event);
				}
				else {
					game.handleEvent(event);
				}
			}

			gameTime.updateTime();

			steamSession.update();

			if (game.isInMenu()) {
				SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
				SDL_RenderClear(renderer);
				menuSystem.render();
				SDL_RenderPresent(renderer);
			}
			else {
				game.update(gameTime.dT);
				game.render();
			}
		}
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	TTF_Quit();
	SDL_Quit();
	return 0;
}
