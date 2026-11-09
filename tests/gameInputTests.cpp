#define SDL_MAIN_HANDLED
#include "lightcyclesGame.h"

#include <cassert>
#include <cmath>
#include <iostream>

struct LightcyclesGameInputTest {
	static void sendKey(LightcyclesGame& game, SDL_Scancode key, bool pressed) {
		SDL_Event event{};
		event.type = pressed ? SDL_KEYDOWN : SDL_KEYUP;
		event.key.keysym.scancode = key;
		game.handleEvent(event);
	}

	static void step(LightcyclesGame& game) {
		game.updateCycles(0.05f);
		assert(game.player_.alive());
		assert(game.result_ == GameResult::Running);
	}

	static void run(SDL_Renderer* renderer) {
		LightcyclesGame game(renderer);
		game.level_.width = 200;
		game.level_.height = 200;
		game.objects_.reset(game.level_, {}, {});
		game.player_.reset({ 100, 100 }, Direction::East);
		game.onlineMatch_.enabled = true;

		sendKey(game, SDL_SCANCODE_W, true);
		step(game);
		const float accelerated = game.player_.speed();
		assert(accelerated > Cycle::BASE_SPEED);
		sendKey(game, SDL_SCANCODE_W, false);
		step(game);
		assert(std::abs(game.player_.speed() - accelerated) < 0.001f);
		sendKey(game, SDL_SCANCODE_S, true);
		step(game);
		assert(game.player_.speed() < accelerated);
		sendKey(game, SDL_SCANCODE_S, false);

		sendKey(game, SDL_SCANCODE_D, true);
		step(game);
		const float rightHeading = game.player_.headingDegrees();
		assert(rightHeading > 0.0f && rightHeading < 180.0f);
		sendKey(game, SDL_SCANCODE_D, false);
		step(game);
		assert(std::abs(game.player_.headingDegrees() - rightHeading) < 0.001f);
		sendKey(game, SDL_SCANCODE_A, true);
		step(game);
		assert(game.player_.headingDegrees() < rightHeading);

		SDL_Event lostFocus{};
		lostFocus.type = SDL_WINDOWEVENT;
		lostFocus.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
		game.handleEvent(lostFocus);
		const float heading = game.player_.headingDegrees();
		step(game);
		assert(std::abs(game.player_.headingDegrees() - heading) < 0.001f);

		game.player_.reset({ 100, 50 }, Direction::East);
		game.enemies_.emplace_back(SDL_Color{ 0, 0, 0, 255 }, SDL_Color{ 255, 0, 0, 255 }, nullptr, 8);
		game.brains_.push_back(createEnemyBrain(EnemyPersonality::Aggressor));
		Cycle& enemy = game.enemies_.front();
		enemy.reset({ 100, 100 }, Direction::East);
		game.moveCycle(enemy, game.brains_.front().get(), 0, 0.05f);
		const float firstTurn = enemy.headingDegrees();
		assert(firstTurn > 0.1f && firstTurn < 359.9f);
		assert(std::abs(std::remainder(firstTurn, 90.0f)) > 1.0f);
		for (int i = 0; i < 5; ++i) game.moveCycle(enemy, game.brains_.front().get(), 0, 0.05f);
		assert(enemy.alive());
		assert(enemy.trail().getSegments(&enemy).size() > 1);
		game.brains_.front()->resetDriving();
		enemy.respawn({ 100, 100 }, Direction::East);
		assert(std::abs(enemy.headingDegrees()) < 0.001f);

		game.level_.placements.push_back({ ObjectType::Wall, { 109, 95, 1, 11 } });
		game.objects_.reset(game.level_, {}, {});
		game.player_.reset({ 150, 100 }, Direction::East);
		game.brains_.front()->resetDriving();
		for (int i = 0; i < 50 && enemy.alive(); ++i) {
			game.moveCycle(enemy, game.brains_.front().get(), 0, 0.02f);
		}
		assert(enemy.alive());
		assert(std::abs(std::remainder(enemy.headingDegrees(), 360.0f)) > 10.0f);
	}
};

int main() {
	assert(SDL_setenv("SDL_VIDEODRIVER", "dummy", 1) == 0);
	assert(SDL_Init(SDL_INIT_VIDEO) == 0);
	SDL_Surface* surface = SDL_CreateRGBSurface(0, 64, 64, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
	assert(surface != nullptr);
	SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
	assert(renderer != nullptr);
	LightcyclesGameInputTest::run(renderer);
	SDL_DestroyRenderer(renderer);
	SDL_FreeSurface(surface);
	SDL_Quit();
	std::cout << "Game input tests passed\n";
}
