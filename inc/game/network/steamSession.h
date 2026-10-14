#pragma once

#include "network/onlineMatchState.h"

#if __has_include("steam/steam_api.h")
#include "steam/steam_api.h"
#include "steam/isteammatchmaking.h"
#define PROJECT_HAS_STEAMWORKS 1
#elif __has_include("steam_api.h")
#include "steam_api.h"
#include "isteammatchmaking.h"
#define PROJECT_HAS_STEAMWORKS 1
#else
#define PROJECT_HAS_STEAMWORKS 0
#endif

#include <string>
#include <vector>

enum class SteamSessionState {
	Offline,
	Lobby,
	Ready,
	InGame,
	Ended
};

enum class SteamSessionRole {
	None,
	Host,
	Client
};

struct SteamPeerInfo {
	std::string steamId;
	std::string displayName;
	bool ready = false;
};

struct SteamLobbyInfo {
	std::string lobbyId;
	std::string name;
	int maxPlayers = 4;
	int memberCount = 0;
	bool isHost = false;
	bool available = false;
};

enum class SteamMessageType {
	Input,
	Snapshot,
	Ready,
	Leave,
	Ping
};

struct SteamNetworkMessage {
	SteamMessageType type = SteamMessageType::Input;
	std::uint32_t senderId = 0;
	std::uint32_t targetId = 0;
	std::uint32_t tick = 0;
	std::string payload;
};

class SteamSessionService {
public:
	SteamSessionService() = default;
	~SteamSessionService();

	bool initialize();
	bool isAvailable() const { return available_; }
	void shutdown();
	void update();

	bool createLobby(const std::string& name, int maxPlayers = 4);
	bool startHostSession(const std::string& name, int maxPlayers = 4) { return createLobby(name, maxPlayers); }
	bool joinLobbyById(const std::string& lobbyId);
	bool joinAsClient(const std::string& lobbyId) { return joinLobbyById(lobbyId); }
	void leaveLobby();

	SteamSessionState state() const { return state_; }
	SteamSessionRole role() const { return role_; }
	const SteamLobbyInfo& lobbyInfo() const { return lobbyInfo_; }
	const std::vector<SteamPeerInfo>& peers() const { return peers_; }
	const std::vector<std::string>& availableLobbies() const { return availableLobbies_; }
	bool isHost() const { return role_ == SteamSessionRole::Host; }
	bool isReady() const { return ready_; }
	void refreshLobbyList();
	void setLocalReady(bool ready);
	void setRole(SteamSessionRole role) { role_ = role; }
	void queueInputPacket(const OnlineInputPacket& packet);
	std::vector<OnlineInputPacket> drainInputPackets();
	void queueSnapshot(const OnlineMatchSnapshot& snapshot);
	std::vector<OnlineMatchSnapshot> drainSnapshots();
	void queueTransportMessage(const SteamNetworkMessage& message);
	std::vector<SteamNetworkMessage> drainTransportMessages();
	std::string serializeInputPacket(const OnlineInputPacket& packet) const;
	OnlineInputPacket deserializeInputPacket(const std::string& payload) const;
	std::string serializeSnapshot(const OnlineMatchSnapshot& snapshot) const;
	OnlineMatchSnapshot deserializeSnapshot(const std::string& payload) const;
	std::size_t pendingInputCount() const { return inputQueue_.size(); }
	std::size_t pendingSnapshotCount() const { return snapshotQueue_.size(); }
	std::size_t pendingTransportCount() const { return transportQueue_.size(); }

private:
#if PROJECT_HAS_STEAMWORKS
	STEAM_CALLBACK(SteamSessionService, onLobbyCreated, LobbyCreated_t);
	STEAM_CALLBACK(SteamSessionService, onLobbyEntered, LobbyEnter_t);
	STEAM_CALLBACK(SteamSessionService, onLobbyMatchList, LobbyMatchList_t);
#endif

	void resetLocalPeer();
	void logPacketRelay(const char* tag, const OnlineInputPacket& packet) const;
	void logSnapshotRelay(const char* tag, const OnlineMatchSnapshot& snapshot) const;
	void logTransportRelay(const char* tag, const SteamNetworkMessage& message) const;

	std::uint64_t currentLobbyId_ = 0;
	SteamSessionState state_ = SteamSessionState::Offline;
	SteamSessionRole role_ = SteamSessionRole::None;
	SteamLobbyInfo lobbyInfo_{};
	std::vector<SteamPeerInfo> peers_;
	std::vector<std::string> availableLobbies_;
	std::vector<OnlineInputPacket> inputQueue_;
	std::vector<OnlineMatchSnapshot> snapshotQueue_;
	std::vector<SteamNetworkMessage> transportQueue_;
	bool realSteamTransport_ = false;
	bool ready_ = false;
	bool initialized_ = false;
	bool available_ = false;
};
