#include "SDL.h"
#include "lightcyclesGame.h"
#include "timeController.h"

#include <cstdlib>
#include <ctime>

int main(int, char**) {
	std::srand(static_cast<unsigned int>(std::time(nullptr)));

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
		return 1;
	}

	SDL_Window* window = SDL_CreateWindow("Vertex2D Lightcycles",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		LightcyclesGame::WINDOW_WIDTH, LightcyclesGame::WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
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

	LightcyclesGame game(renderer);
	TimeController gameTime;
	gameTime.reset();

	while (game.isRunning()) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			game.handleEvent(event);
		}

		gameTime.updateTime();
		game.update(gameTime.dT);
		game.render();
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
