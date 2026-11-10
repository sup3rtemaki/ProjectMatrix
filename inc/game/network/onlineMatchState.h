#pragma once

#include <cstdint>
#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

struct DrivingInput {
	bool accelerate = false;
	bool brake = false;
	bool left = false;
	bool right = false;

	float throttle() const { return static_cast<float>(accelerate) - static_cast<float>(brake); }
	float steering() const { return static_cast<float>(right) - static_cast<float>(left); }
};

struct DrivingInputPacket {
	std::uint32_t sequence = 0;
	DrivingInput input;
};

// The authenticated Steam sender supplies the player identity, never the payload.
std::string serializeDrivingInput(const DrivingInputPacket& packet);
std::optional<DrivingInputPacket> deserializeDrivingInput(const std::string& payload);
std::string serializePlayerReady(std::uint8_t colorIndex);
std::optional<std::uint8_t> deserializePlayerReady(const std::string& payload);

class RemoteDrivingInput {
public:
	static constexpr float TIMEOUT_SECONDS = 0.5f;

	bool accept(const DrivingInputPacket& packet);
	void update(float elapsedSeconds);
	void reset();
	DrivingInput input() const { return input_; }

private:
	DrivingInput input_{};
	std::uint32_t sequence_ = 0;
	float elapsed_ = 0.0f;
	bool received_ = false;
};

class HostDrivingInputs {
public:
	bool addPlayer(std::uint64_t playerId);
	void removePlayer(std::uint64_t playerId);
	void retainPlayers(const std::vector<std::uint64_t>& playerIds);
	bool accept(std::uint64_t senderId, const DrivingInputPacket& packet);
	std::optional<DrivingInput> input(std::uint64_t playerId) const;
	void update(float elapsedSeconds);
	void clear();

private:
	static constexpr std::size_t MAX_REMOTE_PLAYERS = 7;
	std::map<std::uint64_t, RemoteDrivingInput> players_;
};

enum class SnapshotCycleKind : std::uint8_t {
	Human,
	Bot
};

struct SnapshotPoint {
	float x = 0.0f;
	float y = 0.0f;
};

struct SnapshotCycle {
	SnapshotCycleKind kind = SnapshotCycleKind::Human;
	// Steam ID for a human; stable nonzero slot number for a bot.
	std::uint64_t id = 0;
	SnapshotPoint position;
	float heading = 0.0f;
	float speed = 0.0f;
	std::uint32_t score = 0;
	std::uint32_t lives = 0;
	bool alive = false;
	bool respawning = false;
	bool boost = false;
	bool longTrail = false;
	bool slow = false;
	std::array<std::uint8_t, 4> glow{};
	std::array<std::uint8_t, 4> core{};
	std::uint32_t animationFrame = 0;
	std::vector<SnapshotPoint> trail;
};

struct CycleSnapshot {
	static constexpr std::size_t MAX_HUMANS = 8;
	static constexpr std::size_t MAX_BOTS = 5;
	static constexpr std::size_t MAX_TRAIL_POINTS = 512;
	static constexpr std::size_t MAX_BYTES = 64 * 1024;
	std::uint32_t sequence = 0;
	std::vector<SnapshotCycle> cycles;
};

// Visual cycle state only; world objects, projectiles and match state require a separate snapshot.
// The transport authenticates the sender; decoding alone does not establish authority or freshness.
bool validateCycleSnapshot(const CycleSnapshot& snapshot);
std::optional<std::string> serializeCycleSnapshot(const CycleSnapshot& snapshot);
std::optional<CycleSnapshot> deserializeCycleSnapshot(const std::string& payload);

class SnapshotSequence {
public:
	bool accept(std::uint32_t sequence);
	void reset() { received_ = false; sequence_ = 0; }

private:
	std::uint32_t sequence_ = 0;
	bool received_ = false;
};

enum class SnapshotObjectType : std::uint8_t {
	Boost,
	LongTrail,
	Mine,
	Slow
};

struct SnapshotCellRect {
	std::uint32_t x = 0;
	std::uint32_t y = 0;
	std::uint32_t w = 0;
	std::uint32_t h = 0;
};

struct SnapshotObject {
	SnapshotObjectType type = SnapshotObjectType::Boost;
	SnapshotCellRect cells;
};

struct SnapshotProjectile {
	SnapshotPoint position;
	std::uint32_t animationFrame = 0;
};

struct SnapshotTower {
	// The original Tiled placement identifies a tower, not its collision footprint.
	SnapshotCellRect placement;
	bool attacking = false;
	std::uint32_t animationFrame = 0;
	std::vector<SnapshotProjectile> projectiles;
};

struct WorldSnapshot {
	static constexpr std::size_t MAX_OBJECTS = 1024;
	static constexpr std::size_t MAX_TOWERS = 128;
	static constexpr std::size_t MAX_PROJECTILES = 1024;
	static constexpr std::size_t MAX_BYTES = 64 * 1024;
	std::uint32_t sequence = 0;
	std::vector<SnapshotObject> objects;
	std::vector<SnapshotTower> towers;
};

// Visual state only: static walls and tiles are loaded from the agreed level.
bool validateWorldSnapshot(const WorldSnapshot& snapshot);
std::optional<std::string> serializeWorldSnapshot(const WorldSnapshot& snapshot);
std::optional<WorldSnapshot> deserializeWorldSnapshot(const std::string& payload);

enum class OnlinePhase : std::uint8_t {
	Waiting,
	Running,
	Finished
};

struct OnlineFrame {
	std::uint16_t levelIndex = 0;
	OnlinePhase phase = OnlinePhase::Waiting;
	CycleSnapshot cycles;
	WorldSnapshot world;
};

// Two reliable ordered messages keep each payload under Steam's 64 KiB limit.
std::optional<std::array<std::string, 2>> serializeOnlineFrame(const OnlineFrame& frame);

class OnlineFrameAssembler {
public:
	std::optional<OnlineFrame> accept(const std::string& payload);
	void reset();

private:
	struct Header {
		std::uint16_t levelIndex = 0;
		OnlinePhase phase = OnlinePhase::Waiting;
	};
	std::optional<Header> cycleHeader_;
	std::optional<Header> worldHeader_;
	std::optional<CycleSnapshot> cycles_;
	std::optional<WorldSnapshot> world_;
};

struct OnlineInputPacket {
	std::uint32_t sequence = 0;
	std::uint32_t playerId = 0;
	float angle = 0.0f;
	bool turnQueued = false;
};

struct OnlinePlayerState {
	std::uint32_t playerId = 0;
	std::string name;
	bool local = false;
	bool alive = true;
	bool ready = false;
	bool isHost = false;
	float x = 0.0f;
	float y = 0.0f;
	float direction = 0.0f;
	int score = 0;
	int lives = 3;
};

struct OnlineMatchSnapshot {
	std::uint32_t tick = 0;
	std::uint32_t hostPlayerId = 0;
	std::vector<OnlinePlayerState> players;
};

struct OnlineMatchState {
	bool enabled = false;
	bool host = false;
	std::uint32_t localPlayerId = 0;
	std::uint32_t tick = 0;
	std::uint32_t lastInputSequence = 0;
	float tickRate = 1.0f / 30.0f;
	float accumulator = 0.0f;
	std::vector<OnlinePlayerState> remotePlayers;
	std::vector<OnlineInputPacket> pendingInputs;
	std::vector<OnlineMatchSnapshot> snapshots;

	void reset();
	void beginSession(bool isHost, std::uint32_t localPlayerIdValue, int remotePlayerCount = 0);
	void queueInput(std::uint32_t playerId, float angle, bool turnQueued = false);
	void applyRemoteSnapshot(const OnlineMatchSnapshot& snapshot);
	void advanceTick(float elapsedSeconds);
};

inline void OnlineMatchState::reset() {
	enabled = false;
	host = false;
	localPlayerId = 0;
	tick = 0;
	lastInputSequence = 0;
	tickRate = 1.0f / 30.0f;
	accumulator = 0.0f;
	remotePlayers.clear();
	pendingInputs.clear();
	snapshots.clear();
}
