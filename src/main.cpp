#include "SDL.h"
#include "SDL_ttf.h"
#include "lightcyclesGame.h"
#include "menu/mainMenu.h"
#include "network/steamSession.h"
#include "res_path.h"
#include "systems/mainMenuSystem.h"
#include "timeController.h"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <optional>
#include <vector>

int main(int, char**) {
	std::srand(static_cast<unsigned int>(std::time(nullptr)));

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
		return 1;
	}

	if (TTF_Init() != 0) {
		SDL_Quit();
		return 1;
	}

	SDL_DisplayMode desktop{};
	int initialWidth = LightcyclesGame::DEFAULT_WINDOW_WIDTH;
	int initialHeight = LightcyclesGame::DEFAULT_WINDOW_HEIGHT;
	if (SDL_GetDesktopDisplayMode(0, &desktop) == 0 && desktop.w > 0 && desktop.h > 0) {
		initialWidth = desktop.w;
		initialHeight = desktop.h;
	}

	// Common resolutions that fit the desktop, plus the desktop resolution itself.
	std::vector<DisplayMode> displayModes;
	for (const DisplayMode& candidate : std::vector<DisplayMode>{ { 960, 640 }, { 1024, 768 }, { 1280, 720 },
		{ 1280, 800 }, { 1366, 768 }, { 1600, 900 }, { 1680, 1050 }, { 1920, 1080 }, { 2560, 1440 }, { 3840, 2160 } }) {
		if (candidate.width <= initialWidth && candidate.height <= initialHeight) displayModes.push_back(candidate);
	}
	const DisplayMode desktopMode{ initialWidth, initialHeight };
	if (std::find(displayModes.begin(), displayModes.end(), desktopMode) == displayModes.end()) {
		displayModes.push_back(desktopMode);
	}
	std::sort(displayModes.begin(), displayModes.end(), [](const DisplayMode& a, const DisplayMode& b) {
		return a.width * a.height < b.width * b.height;
	});
	const std::size_t desktopIndex = static_cast<std::size_t>(
		std::find(displayModes.begin(), displayModes.end(), desktopMode) - displayModes.begin());

	SDL_Window* window = SDL_CreateWindow("Vertex2D Lightcycles",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		initialWidth, initialHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
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
	SDL_RenderSetLogicalSize(renderer, initialWidth, initialHeight);

	{
		SteamSessionService steamSession;
		steamSession.initialize();

		LightcyclesGame game(renderer, &steamSession);
		game.setResolution(initialWidth, initialHeight);
		MainMenu mainMenu;
		mainMenu.setRenderer(renderer);
		mainMenu.setResourcePath(getResourcePath());
		mainMenu.setAvailableLevels(game.getAvailableLevelNames());
		mainMenu.setDisplayModes(displayModes, desktopIndex, false);

		RenderContext menuRenderContext;
		menuRenderContext.renderer = renderer;
		menuRenderContext.camera = { 0, 0, initialWidth, initialHeight };

		mainMenu.setDisplayChangedCallback([&](const DisplayMode& mode, bool fullscreen) {
			if (fullscreen) {
				SDL_SetWindowSize(window, mode.width, mode.height);
				SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
			}
			else {
				SDL_SetWindowFullscreen(window, 0);
				SDL_SetWindowSize(window, mode.width, mode.height);
				SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
			}
			SDL_RenderSetLogicalSize(renderer, mode.width, mode.height);
			menuRenderContext.camera = { 0, 0, mode.width, mode.height };
			game.setResolution(mode.width, mode.height);
		});
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
