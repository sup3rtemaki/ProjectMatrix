#pragma once

#include "animationSet.h"
#include "cameraController.h"
#include "ai/enemyPersonality.h"
#include "cycle.h"
#include "level/levelDefinition.h"
#include "menu/gameConfig.h"
#include "network/onlineMatchState.h"
#include "renderContext.h"
#include "rendering/cycleSkins.h"
#include "rendering/depthQueue.h"
#include "rendering/renderLayers.h"
#include "rendering/tileMapRenderer.h"
#include "ui/hudUi.h"
#include "world/arenaObjects.h"

class SteamSessionService;

#include <cstdint>
#include <memory>
#include <map>
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

	explicit LightcyclesGame(SDL_Renderer* renderer, SteamSessionService* steamSession = nullptr);
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
	friend struct LightcyclesGameInputTest;
	static constexpr int CELL_SIZE = 8;
	static constexpr int MIN_GRID_WIDTH = (WINDOW_WIDTH + CELL_SIZE - 1) / CELL_SIZE;
	static constexpr int MIN_GRID_HEIGHT = (VIEWPORT_HEIGHT + CELL_SIZE - 1) / CELL_SIZE;
	inline static const std::string LEVELS_FOLDER = "levels/";
	inline static const std::string TEXTURES_FOLDER = "textures/";
	inline static const std::vector<std::string> LEVEL_FILES = { "big_map.json" }; // Tiled maps in LEVELS_FOLDER

	std::string levelPath(std::size_t index) const;
	std::size_t countLevels() const;
	bool isPlayable(const LevelDefinition& level) const;
	bool loadLevel(std::size_t index);

	ArenaView buildArenaView() const;
	bool checkCycleCollision(const Cycle& cycle, Vec2 before, Vec2 after, Cycle*& outKiller);
	void updateCycles(float elapsedSeconds);
	void moveCycle(Cycle& cycle, EnemyBrain* brain, std::size_t enemyIndex, float elapsedSeconds,
		const DrivingInput* remoteInput = nullptr);
	void updateOnlineInputs(float elapsedSeconds);
	void sendOnlineFrame(float elapsedSeconds);
	bool applyOnlineFrame(const OnlineFrame& frame, std::uint64_t localId, std::uint64_t hostId,
		const std::vector<std::uint64_t>& members);
	void updateVisualInterpolation(float elapsedSeconds);
	CycleSnapshot captureCycleSnapshot(std::uint64_t hostId, std::uint32_t sequence) const;
	void synchronizeRemoteCycles(const std::vector<std::uint64_t>& members, float elapsedSeconds);
	bool spawnRemoteCycle(Cycle& cycle);
	void handleCycleCrash(Cycle& cycle);
	void updateCycleRespawn(Cycle& cycle, float elapsedSeconds);
	bool findSafeRespawn(SDL_Point& outCell, Direction& outDir, bool allowFallback = true);
	void respawnCycle(Cycle& cycle);
	void crashCollidingHeads();
	void updateResult();
	void reset();
	void setDrawColor(SDL_Color color) const;

	SDL_Renderer* renderer_;
	SteamSessionService* steamSession_ = nullptr;
	SDL_Texture* worldTexture_ = nullptr; // world drawn at camera size, then scaled to the game area
	std::string resPath_;
	RenderContext renderContext_;
	CameraController cameraController_;
	CycleSkins cycleSkins_;
	HudUi hudUi_;
	Cycle player_;
	std::vector<Cycle> enemies_;
	std::vector<std::unique_ptr<EnemyBrain>> brains_;
	struct RemoteCycle {
		std::unique_ptr<Cycle> cycle;
		bool connected = false;
		bool retired = false;
		float spawnRetrySeconds = 0.0f;
	};
	std::map<std::uint64_t, RemoteCycle> remoteCycles_;
	HostDrivingInputs remoteInputs_;
	std::uint32_t inputSequence_ = 0;
	std::uint64_t sessionHostId_ = 0;
	float inputSendTimer_ = 0.0f;
	float readySendTimer_ = 0.0f;
	float timeWithoutFrame_ = 0.0f;
	std::map<std::uint64_t, std::uint8_t> readyPlayers_;
	float snapshotSendTimer_ = 0.0f;
	std::uint32_t snapshotSendSequence_ = 0;
	OnlineFrameAssembler frameAssembler_;
	SnapshotSequence receivedFrameSequence_;
	std::map<std::pair<SnapshotCycleKind, std::uint64_t>, std::unique_ptr<Cycle>> visualCycles_;
	struct VisualMotion {
		Vec2 from;
		Vec2 to;
		float fromHeading = 0.0f;
		float headingDelta = 0.0f;
	};
	std::map<std::pair<SnapshotCycleKind, std::uint64_t>, VisualMotion> visualMotions_;
	std::uint64_t visualLocalId_ = 0;
	float visualInterpolationTime_ = 0.0f;
	bool receivedFrame_ = false;
	bool matchStarted_ = false;
	OnlineMatchState onlineMatch_;
	ArenaObjects objects_;
	TileMapRenderer tileMap_;
	DepthQueue depthQueue_;
	LevelDefinition level_;
	std::size_t levelIndex_ = 0;
	std::size_t levelCount_ = 0;
	GameResult result_ = GameResult::Running;
	bool running_ = true;
	bool inMenu_ = true;
	bool exitRequested_ = false;
	bool accelerating_ = false;
	bool braking_ = false;
	bool steeringLeft_ = false;
	bool steeringRight_ = false;
	GameConfig activeConfig_;
};
