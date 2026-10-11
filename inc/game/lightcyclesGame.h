#pragma once

#include "cameraController.h"
#include "cameraTarget.h"
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
	static constexpr int WORLD_WIDTH = 160;
	static constexpr int WORLD_HEIGHT = 100;
	static constexpr int GRID_WIDTH = WORLD_WIDTH;
	static constexpr int GRID_HEIGHT = WORLD_HEIGHT;
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
	CameraController cameraController_;
	CameraTarget cameraTarget_;
	SDL_Rect camera_ = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
	Cycle player_;
	std::vector<Cycle> enemies_;
	std::vector<std::uint8_t> occupied_;
	GameResult result_ = GameResult::Running;
	float stepAccumulator_ = 0;
	bool running_ = true;
};
