#include "network/steamSession.h"

#include <iostream>

SteamSessionService::~SteamSessionService() {
	shutdown();
}

bool SteamSessionService::initialize() {
	if (initialized_) {
		return available_;
	}

	initialized_ = true;
	available_ = true;
	state_ = SteamSessionState::Lobby;
	lobbyInfo_ = { "local-steam-lobby", "Local Steam Lobby", 4, 1, true, true };
	peers_.clear();
	peers_.push_back({ "local-user", "You", true });
	return true;
}

bool SteamSessionService::createLobby(const std::string& name, int maxPlayers) {
	if (!available_) {
		return false;
	}
	ready_ = false;
	state_ = SteamSessionState::Lobby;
	lobbyInfo_ = { "local-steam-lobby", name.empty() ? "Local Steam Lobby" : name, maxPlayers, 1, true, true };
	peers_.clear();
	peers_.push_back({ "local-user", "You", true });
	return true;
}

bool SteamSessionService::joinLobbyById(const std::string& lobbyId) {
	if (!available_) {
		return false;
	}
	ready_ = false;
	state_ = SteamSessionState::Lobby;
	lobbyInfo_ = { lobbyId, "Joined Steam Lobby", 4, 2, false, true };
	peers_.clear();
	peers_.push_back({ "local-user", "You", true });
	peers_.push_back({ "remote-user", "Remote Player", false });
	return true;
}

void SteamSessionService::leaveLobby() {
	ready_ = false;
	state_ = SteamSessionState::Offline;
	lobbyInfo_ = {};
	peers_.clear();
}

void SteamSessionService::shutdown() {
	leaveLobby();
	initialized_ = false;
	available_ = false;
}

void SteamSessionService::update() {
	if (!available_) {
		return;
	}
	if (state_ == SteamSessionState::Ready) {
		state_ = SteamSessionState::InGame;
	}
}

void SteamSessionService::setLocalReady(bool ready) {
	ready_ = ready;
	if (peers_.empty()) {
		peers_.push_back({ "local-user", "You", ready });
		return;
	}
	peers_[0].ready = ready;
	state_ = ready ? SteamSessionState::Ready : SteamSessionState::Lobby;
}

void SteamSessionService::resetLocalPeer() {
	if (!peers_.empty()) {
		peers_[0] = { "local-user", "You", ready_ };
	}
}
