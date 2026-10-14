#pragma once

#include "animationSet.h"
#include "cameraController.h"
#include "ai/enemyPersonality.h"
#include "cycle.h"
#include "inputCommand.h"
#include "keyboardInput.h"
#include "level/levelDefinition.h"
#include "menu/gameConfig.h"
#include "renderContext.h"
#include "ui/hudUi.h"
#include "world/arenaObjects.h"

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
	static constexpr int HUD_HEIGHT = 36;
	static constexpr int VIEWPORT_HEIGHT = WINDOW_HEIGHT - HUD_HEIGHT;
	static constexpr float CAMERA_ZOOM = 0.64f;
	static constexpr int CAMERA_WIDTH = static_cast<int>(WINDOW_WIDTH * CAMERA_ZOOM);
	static constexpr int CAMERA_HEIGHT = static_cast<int>(VIEWPORT_HEIGHT * CAMERA_ZOOM);

	explicit LightcyclesGame(SDL_Renderer* renderer);
	~LightcyclesGame();

	bool isRunning() const;
	void handleEvent(SDL_Event& event);
	void update(float elapsedSeconds);
	void render();

	std::vector<std::string> getAvailableLevelNames() const;
	bool startMatch(const GameConfig& config);
	void returnToMenu();
	bool isExitRequested() const { return exitRequested_; }
	bool isInMenu() const { return inMenu_; }

private:
	static constexpr int CELL_SIZE = 8;
	static constexpr int MIN_GRID_WIDTH = (WINDOW_WIDTH + CELL_SIZE - 1) / CELL_SIZE;
	static constexpr int MIN_GRID_HEIGHT = (VIEWPORT_HEIGHT + CELL_SIZE - 1) / CELL_SIZE;
	static constexpr float CYCLE_SPEED = CELL_SIZE / 0.1f; // pixels per second
	inline static const std::string CYCLE_ANIMATION_SET = "cycle.fdset";
	inline static const std::string LEVELS_FOLDER = "levels/"; // level1.json, level2.json, ...

	std::string levelPath(std::size_t index) const;
	std::size_t countLevels() const;
	bool isPlayable(const LevelDefinition& level) const;
	bool loadLevel(std::size_t index);

	ArenaView buildArenaView() const;
	bool checkCycleCollision(const Cycle& cycle, Vec2 before, Vec2 after, Cycle*& outKiller);
	void updateCycles(float elapsedSeconds);
	void moveCycle(Cycle& cycle, EnemyBrain* brain, std::size_t enemyIndex, float elapsedSeconds);
	void handleCycleCrash(Cycle& cycle);
	void updateCycleRespawn(Cycle& cycle, float elapsedSeconds);
	bool findSafeRespawn(SDL_Point& outCell, Direction& outDir);
	void respawnCycle(Cycle& cycle);
	void crashCollidingHeads();
	void updateResult();
	void reset();
	void setDrawColor(SDL_Color color) const;

	SDL_Renderer* renderer_;
	SDL_Texture* worldTexture_ = nullptr; // world drawn at camera size, then scaled to the game area
	std::string resPath_;
	RenderContext renderContext_;
	KeyboardInput keyboard_;
	CameraController cameraController_;
	AnimationSet cycleAnimSet_;
	HudUi hudUi_;
	Cycle player_;
	std::vector<Cycle> enemies_;
	std::vector<std::unique_ptr<EnemyBrain>> brains_;
	std::vector<float> enemyDecisionTimers_;
	ArenaObjects objects_;
	LevelDefinition level_;
	std::size_t levelIndex_ = 0;
	std::size_t levelCount_ = 0;
	GameResult result_ = GameResult::Running;
	bool running_ = true;
	bool inMenu_ = true;
	bool exitRequested_ = false;
	GameConfig activeConfig_;
};
