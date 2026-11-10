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
#include <optional>

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

		LightcyclesGame game(renderer, &steamSession);
		MainMenu mainMenu;
		mainMenu.setRenderer(renderer);
		mainMenu.setResourcePath(getResourcePath());
		mainMenu.setAvailableLevels(game.getAvailableLevelNames());

		RenderContext menuRenderContext;
		menuRenderContext.renderer = renderer;
		menuRenderContext.camera = { 0, 0, LightcyclesGame::WINDOW_WIDTH, LightcyclesGame::WINDOW_HEIGHT };

		MainMenuSystem menuSystem(&mainMenu, menuRenderContext);

		bool appRunning = true;
		std::optional<GameConfig> pendingOnlineMatch;

		menuSystem.setStartGameCallback([&](const GameConfig& config) {
			if (pendingOnlineMatch) {
				std::cerr << "[App] A lobby request is already in progress" << std::endl;
				return;
			}
			if (config.onlineMode) {
				bool requested = false;
				if (!config.lobbyId.empty()) {
					requested = steamSession.joinLobbyById(config.lobbyId);
				}
				else if (config.gameMode == "Steam Lobby") {
					requested = steamSession.startHostSession(config.levelName.empty() ? "Steam Lobby" : config.levelName,
						std::max(1, config.totalPlayers));
				}
				if (requested) pendingOnlineMatch = config;
				else std::cerr << "[App] Could not request Steam lobby" << std::endl;
				return;
			}
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
					if (pendingOnlineMatch && event.type == SDL_KEYDOWN
						&& event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
						steamSession.leaveLobby();
						pendingOnlineMatch.reset();
						continue;
					}
					menuSystem.update(event);
				}
				else {
					game.handleEvent(event);
				}
			}

			gameTime.updateTime();

			steamSession.update();
			if (pendingOnlineMatch) {
				if (steamSession.lobbyInfo().available) {
					steamSession.setLocalReady(true);
					if (!game.startMatch(*pendingOnlineMatch)) {
						steamSession.leaveLobby();
					}
					pendingOnlineMatch.reset();
				}
				else if (steamSession.state() == SteamSessionState::Offline) {
					std::cerr << "[App] Steam lobby request failed" << std::endl;
					pendingOnlineMatch.reset();
				}
			}
			if (game.isInMenu()) {
				steamSession.refreshLobbyList();
				mainMenu.setAvailableLobbies(steamSession.availableLobbies());
			}

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
