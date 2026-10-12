#pragma once

#include "animationSet.h"
#include "cameraController.h"
#include "ai/enemyPersonality.h"
#include "cycle.h"
#include "inputCommand.h"
#include "keyboardInput.h"
#include "renderContext.h"

#include <cstdint>
#include <memory>
#include <string>
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
	void render();

private:
	static constexpr int CELL_SIZE = 12;
	static constexpr int WORLD_WIDTH = 160;
	static constexpr int WORLD_HEIGHT = 100;
	static constexpr int GRID_WIDTH = WORLD_WIDTH;
	static constexpr int GRID_HEIGHT = WORLD_HEIGHT;
	static constexpr float CYCLE_SPEED = CELL_SIZE / 0.1f; // pixels per second
	inline static const std::string CYCLE_ANIMATION_SET = "cycle.fdset";

	bool isInsideGrid(int x, int y) const;
	bool isOccupied(int x, int y) const;
	std::size_t gridIndex(int x, int y) const;
	void updateCycles(float elapsedSeconds);
	void moveCycle(Cycle& cycle, EnemyBrain* brain, float distance);
	void lightCell(Cycle& cycle, SDL_Point cell);
	void crashCollidingHeads();
	void updateResult();
	void reset();
	void drawTrail(const Cycle& cycle) const;
	void setDrawColor(SDL_Color color) const;

	SDL_Renderer* renderer_;
	std::string resPath_;
	RenderContext renderContext_;
	KeyboardInput keyboard_;
	CameraController cameraController_;
	AnimationSet cycleAnimSet_;
	Cycle player_;
	std::vector<Cycle> enemies_;
	std::vector<std::unique_ptr<EnemyBrain>> brains_;
	std::vector<std::uint8_t> occupied_;
	GameResult result_ = GameResult::Running;
	bool running_ = true;
};
