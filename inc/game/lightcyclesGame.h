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
#include <optional>
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
	static constexpr int DEFAULT_WINDOW_WIDTH = 960;
	static constexpr int DEFAULT_WINDOW_HEIGHT = 640;
	// World pixels are drawn at this many screen pixels at 768p; larger screens scale it up.
	static constexpr float BASE_PIXEL_SCALE = 1.5625f;

	explicit LightcyclesGame(SDL_Renderer* renderer, SteamSessionService* steamSession = nullptr);
	~LightcyclesGame();

	void setResolution(int width, int height);
	int windowWidth() const { return windowWidth_; }
	int windowHeight() const { return windowHeight_; }

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
	int windowWidth_ = DEFAULT_WINDOW_WIDTH;
	int windowHeight_ = DEFAULT_WINDOW_HEIGHT;
	int hudHeight_ = 36;
	int viewportHeight_ = DEFAULT_WINDOW_HEIGHT - 36;
	void applyCameraSize();
	inline static const std::string LEVELS_FOLDER = "levels/";
	inline static const std::string TEXTURES_FOLDER = "textures/";
	inline static const std::vector<std::string> LEVEL_FILES = { "big_map.json" }; // Tiled maps in LEVELS_FOLDER

	std::string levelPath(std::size_t index) const;
	std::size_t countLevels() const;
	bool isPlayable(const LevelDefinition& level) const;
	bool loadLevel(std::size_t index);
	using CycleNameKey = std::pair<SnapshotCycleKind, std::uint64_t>;
	std::string cycleDisplayName(SnapshotCycleKind kind, std::uint64_t id) const;
	Cycle* findSpectatorCycle(const CycleNameKey& key);
	void updateSpectatorCamera();
	void drawCycleName(Cycle& cycle, const CycleNameKey& key, const std::string& name,
		const RenderContext& context);
	void clearCycleNameTags();

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
	struct CycleNameTag {
		std::string text;
		SDL_Texture* texture = nullptr;
		int width = 0;
		int height = 0;
	};
	std::string cycleNameFontPath_;
	std::map<CycleNameKey, CycleNameTag> cycleNameTags_;
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
	bool isSpectating_ = false;
	std::optional<CycleNameKey> spectatorTarget_;
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
