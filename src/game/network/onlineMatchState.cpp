#include "network/onlineMatchState.h"

#include <iostream>
#include <utility>

void OnlineMatchState::beginSession(bool isHost, std::uint32_t localPlayerIdValue, int remotePlayerCount) {
	enabled = true;
	host = isHost;
	localPlayerId = localPlayerIdValue;
	remotePlayers.clear();
	pendingInputs.clear();
	snapshots.clear();
	std::cout << "[OnlineMatch] beginSession: host=" << host << ", localPlayerId=" << localPlayerId << ", remotePlayers=" << remotePlayerCount << std::endl;
}

void OnlineMatchState::queueInput(std::uint32_t playerId, float angle, bool turnQueued) {
	if (!enabled) {
		return;
	}
	OnlineInputPacket packet;
	packet.sequence = ++lastInputSequence;
	packet.playerId = playerId;
	packet.angle = angle;
	packet.turnQueued = turnQueued;
	pendingInputs.push_back(packet);
	std::cout << "[OnlineMatch] queued input: player=" << playerId << ", angle=" << angle << ", seq=" << packet.sequence << std::endl;
}

void OnlineMatchState::applyRemoteSnapshot(const OnlineMatchSnapshot& snapshot) {
	if (!enabled) {
		return;
	}
	snapshots.push_back(snapshot);
	if (!snapshot.players.empty()) {
		remotePlayers.clear();
		for (const auto& player : snapshot.players) {
			if (player.playerId != localPlayerId) {
				remotePlayers.push_back(player);
			}
		}
		std::cout << "[OnlineMatch] applied snapshot: tick=" << snapshot.tick << ", remoteCount=" << remotePlayers.size() << std::endl;
	}
}

void OnlineMatchState::advanceTick(float elapsedSeconds) {
	if (!enabled) {
		return;
	}
	accumulator += elapsedSeconds;
	while (accumulator >= tickRate) {
		accumulator -= tickRate;
		++tick;
		std::cout << "[OnlineMatch] tick=" << tick << ", pendingInputs=" << pendingInputs.size() << std::endl;
	}
}
