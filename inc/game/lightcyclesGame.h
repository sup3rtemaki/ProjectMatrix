#pragma once

#include "cycle.h"
#include "inputCommand.h"
#include "keyboardInput.h"

#include <cstdint>
#include <vector>

enum class GameResult {
	Running,
	PlayerWon,
	PlayerLost,
	Draw
};

class LightcyclesGame {
public:
	static constexpr int WINDOW_WIDTH = 960;
	static constexpr int WINDOW_HEIGHT = 640;

	explicit LightcyclesGame(SDL_Renderer* renderer);

	bool isRunning() const;
	void handleEvent(SDL_Event& event);
	void update(float elapsedSeconds);
	void render() const;

private:
	static constexpr int CELL_SIZE = 12;
	static constexpr int GRID_WIDTH = WINDOW_WIDTH / CELL_SIZE;
	static constexpr int GRID_HEIGHT = WINDOW_HEIGHT / CELL_SIZE;
	static constexpr int GRID_TOP = (WINDOW_HEIGHT - GRID_HEIGHT * CELL_SIZE) / 2;
	static constexpr float MOVE_STEP_SECONDS = 0.12f;

	struct PlannedMove {
		Cycle* cycle;
		SDL_Point destination;
		bool crashed;
	};

	bool isInsideGrid(int x, int y) const;
	bool isOccupied(int x, int y) const;
	std::size_t gridIndex(int x, int y) const;
	bool isSafeTurn(const Cycle& cycle, Direction direction) const;
	void chooseEnemyDirection(Cycle& enemy);
	void updateCycles();
	void reset();
	void drawCycle(const Cycle& cycle) const;
	void setDrawColor(SDL_Color color) const;

	SDL_Renderer* renderer_;
	KeyboardInput keyboard_;
	Cycle player_;
	std::vector<Cycle> enemies_;
	std::vector<std::uint8_t> occupied_;
	GameResult result_ = GameResult::Running;
	float stepAccumulator_ = 0;
	bool running_ = true;
};
