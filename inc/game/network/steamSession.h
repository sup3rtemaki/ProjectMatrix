#pragma once

#include <string>
#include <vector>

enum class SteamSessionState {
	Offline,
	Lobby,
	Ready,
	InGame,
	Ended
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

class SteamSessionService {
public:
	SteamSessionService() = default;
	~SteamSessionService();

	bool initialize();
	bool isAvailable() const { return available_; }
	void shutdown();
	void update();

	bool createLobby(const std::string& name, int maxPlayers = 4);
	bool joinLobbyById(const std::string& lobbyId);
	void leaveLobby();

	SteamSessionState state() const { return state_; }
	const SteamLobbyInfo& lobbyInfo() const { return lobbyInfo_; }
	const std::vector<SteamPeerInfo>& peers() const { return peers_; }
	bool isHost() const { return lobbyInfo_.isHost; }
	bool isReady() const { return ready_; }
	void setLocalReady(bool ready);

private:
	void resetLocalPeer();

	SteamSessionState state_ = SteamSessionState::Offline;
	SteamLobbyInfo lobbyInfo_{};
	std::vector<SteamPeerInfo> peers_;
	bool ready_ = false;
	bool initialized_ = false;
	bool available_ = false;
};
