#pragma once

#include <cstdint>
#include <string>
#include <vector>

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
