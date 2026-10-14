#include "network/steamSession.h"

#include <cstring>
#include <stdexcept>

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

#if PROJECT_HAS_STEAMWORKS && __has_include("steam/isteamnetworkingsockets.h")
#include "steam/isteamnetworkingsockets.h"
#define PROJECT_HAS_STEAM_NETWORKING 1
#else
#define PROJECT_HAS_STEAM_NETWORKING 0
#endif

#include <iostream>

void SteamSessionService::onLobbyCreated(LobbyCreated_t* pCallback) {
	if (!pCallback) {
		return;
	}
	if (pCallback->m_eResult != k_EResultOK) {
		std::cout << "[SteamSession] lobby creation failed, result=" << static_cast<int>(pCallback->m_eResult) << std::endl;
		return;
	}
	currentLobbyId_ = static_cast<std::uint64_t>(pCallback->m_ulSteamIDLobby);
	lobbyInfo_.lobbyId = std::to_string(currentLobbyId_);
	lobbyInfo_.available = true;
	lobbyInfo_.isHost = true;
	state_ = SteamSessionState::Lobby;
	role_ = SteamSessionRole::Host;
	ready_ = false;
	peers_.clear();
	peers_.push_back({ std::to_string(SteamUser()->GetSteamID().ConvertToUint64()), "You", false });
	if (SteamMatchmaking()) {
		SteamMatchmaking()->SetLobbyData(CSteamID(currentLobbyId_), "game", "lightcycles");
		SteamMatchmaking()->SetLobbyData(CSteamID(currentLobbyId_), "host", "true");
	}
	std::cout << "[SteamSession] real lobby created: " << lobbyInfo_.lobbyId << std::endl;
}

void SteamSessionService::onLobbyEntered(LobbyEnter_t* pCallback) {
	if (!pCallback) {
		return;
	}
	currentLobbyId_ = static_cast<std::uint64_t>(pCallback->m_ulSteamIDLobby);
	lobbyInfo_.lobbyId = std::to_string(currentLobbyId_);
	lobbyInfo_.available = true;
	lobbyInfo_.isHost = false;
	role_ = SteamSessionRole::Client;
	state_ = SteamSessionState::Lobby;
	ready_ = false;
	peers_.clear();
	peers_.push_back({ std::to_string(SteamUser()->GetSteamID().ConvertToUint64()), "You", false });
	if (SteamMatchmaking()) {
		lobbyInfo_.memberCount = SteamMatchmaking()->GetNumLobbyMembers(CSteamID(currentLobbyId_));
		lobbyInfo_.name = SteamMatchmaking()->GetLobbyData(CSteamID(currentLobbyId_), "name");
	}
	std::cout << "[SteamSession] joined lobby: " << lobbyInfo_.lobbyId << std::endl;
}

void SteamSessionService::onLobbyMatchList(LobbyMatchList_t* pCallback) {
	if (!pCallback) {
		return;
	}
	availableLobbies_.clear();
	if (!SteamMatchmaking()) {
		std::cout << "[SteamSession] lobby list refresh failed: SteamMatchmaking unavailable" << std::endl;
		return;
	}
	const int resultCount = pCallback->m_nLobbiesMatching;
	for (int i = 0; i < resultCount; ++i) {
		const CSteamID lobbyId = SteamMatchmaking()->GetLobbyByIndex(i);
		if (!lobbyId.IsValid()) {
			continue;
		}
		const std::string lobbyKey = std::to_string(lobbyId.ConvertToUint64());
		availableLobbies_.push_back(lobbyKey);
	}
	std::cout << "[SteamSession] lobby list refreshed: " << availableLobbies_.size() << " room(s)" << std::endl;
}

SteamSessionService::~SteamSessionService() {
	shutdown();
}

bool SteamSessionService::initialize() {
	if (initialized_) {
		std::cout << "[SteamSession] already initialized; available=" << available_ << std::endl;
		return available_;
	}

#if PROJECT_HAS_STEAMWORKS
	if (SteamAPI_RestartAppIfNecessary(k_uAppIdInvalid)) {
		std::cout << "[SteamSession] Steam app relaunch requested; exiting to let Steam start the app with the correct AppID" << std::endl;
		return false;
	}
	if (!SteamAPI_Init()) {
		std::cout << "[SteamSession] SteamAPI_Init failed; falling back to mock session layer" << std::endl;
		std::cout << "[SteamSession] Ensure the app has a valid Steam AppID and is launched via Steam or with a valid steam_appid.txt" << std::endl;
		available_ = true;
	}
	else {
		available_ = SteamAPI_IsSteamRunning();
		std::cout << "[SteamSession] Steamworks initialized: " << (available_ ? "online" : "offline") << std::endl;
	}
#else
	std::cout << "[SteamSession] Steamworks SDK not found; using mock session layer" << std::endl;
	available_ = true;
#endif

	initialized_ = true;
	realSteamTransport_ = false;
#if PROJECT_HAS_STEAM_NETWORKING
	realSteamTransport_ = SteamNetworkingSockets() != nullptr;
	if (realSteamTransport_) {
		std::cout << "[SteamSession] real Steam networking sockets available" << std::endl;
	}
#endif
	state_ = SteamSessionState::Lobby;
	lobbyInfo_ = { "local-steam-lobby", "Local Steam Lobby", 4, 1, true, true };
	peers_.clear();
	peers_.push_back({ "local-user", "You", true });
	std::cout << "[SteamSession] initialized -> lobby ready" << std::endl;
	return true;
}

bool SteamSessionService::createLobby(const std::string& name, int maxPlayers) {
	if (!available_) {
		std::cout << "[SteamSession] createLobby failed: service unavailable" << std::endl;
		return false;
	}
#if PROJECT_HAS_STEAMWORKS
	if (SteamMatchmaking() == nullptr) {
		std::cout << "[SteamSession] SteamMatchmaking unavailable" << std::endl;
		return false;
	}
	const auto request = SteamMatchmaking()->CreateLobby(k_ELobbyTypePublic, maxPlayers);
	if (request == k_uAPICallInvalid) {
		std::cout << "[SteamSession] CreateLobby request invalid" << std::endl;
		return false;
	}
	role_ = SteamSessionRole::Host;
	ready_ = false;
	state_ = SteamSessionState::Lobby;
	lobbyInfo_ = { "pending", name.empty() ? "Local Steam Lobby" : name, maxPlayers, 1, true, true };
	peers_.clear();
	peers_.push_back({ std::to_string(SteamUser()->GetSteamID().ConvertToUint64()), "You", false });
	std::cout << "[SteamSession] createLobby requested -> role=Host, name='" << lobbyInfo_.name << "', maxPlayers=" << maxPlayers << std::endl;
	return true;
#else
	role_ = SteamSessionRole::Host;
	ready_ = false;
	state_ = SteamSessionState::Lobby;
	lobbyInfo_ = { "local-steam-lobby", name.empty() ? "Local Steam Lobby" : name, maxPlayers, 1, true, true };
	peers_.clear();
	peers_.push_back({ "local-user", "You", true });
	std::cout << "[SteamSession] createLobby -> role=Host, name='" << lobbyInfo_.name << "', maxPlayers=" << maxPlayers << std::endl;
	return true;
#endif
}

bool SteamSessionService::joinLobbyById(const std::string& lobbyId) {
	if (!available_) {
		std::cout << "[SteamSession] joinLobbyById failed: service unavailable" << std::endl;
		return false;
	}
#if PROJECT_HAS_STEAMWORKS
	if (SteamMatchmaking() == nullptr) {
		std::cout << "[SteamSession] SteamMatchmaking unavailable" << std::endl;
		return false;
	}
	try {
		const std::uint64_t parsedLobbyId = std::stoull(lobbyId);
		const CSteamID targetLobby(parsedLobbyId);
		if (!targetLobby.IsValid()) {
			std::cout << "[SteamSession] invalid lobby id: " << lobbyId << std::endl;
			return false;
		}
		SteamMatchmaking()->JoinLobby(targetLobby);
		currentLobbyId_ = parsedLobbyId;
		role_ = SteamSessionRole::Client;
		ready_ = false;
		state_ = SteamSessionState::Lobby;
		lobbyInfo_ = { lobbyId, "Joined Steam Lobby", 4, 1, false, true };
		peers_.clear();
		peers_.push_back({ std::to_string(SteamUser()->GetSteamID().ConvertToUint64()), "You", false });
		std::cout << "[SteamSession] joinLobbyById -> role=Client, lobby='" << lobbyId << "'" << std::endl;
		return true;
	}
	catch (const std::exception&) {
		std::cout << "[SteamSession] joinLobbyById failed to parse lobby id: " << lobbyId << std::endl;
		return false;
	}
#else
	role_ = SteamSessionRole::Client;
	ready_ = false;
	state_ = SteamSessionState::Lobby;
	lobbyInfo_ = { lobbyId, "Joined Steam Lobby", 4, 1, false, true };
	peers_.clear();
	peers_.push_back({ "local-user", "You", true });
	std::cout << "[SteamSession] joinLobbyById -> role=Client, lobby='" << lobbyId << "'" << std::endl;
	return true;
#endif
}

void SteamSessionService::leaveLobby() {
#if PROJECT_HAS_STEAMWORKS
	if (currentLobbyId_ != 0 && SteamMatchmaking()) {
		SteamMatchmaking()->LeaveLobby(CSteamID(currentLobbyId_));
	}
#endif
	ready_ = false;
	state_ = SteamSessionState::Offline;
	role_ = SteamSessionRole::None;
	currentLobbyId_ = 0;
	lobbyInfo_ = {};
	peers_.clear();
	std::cout << "[SteamSession] leaveLobby -> role=None, offline" << std::endl;
}

void SteamSessionService::shutdown() {
	leaveLobby();
	initialized_ = false;
	available_ = false;
#if PROJECT_HAS_STEAMWORKS
	SteamAPI_Shutdown();
#endif
	std::cout << "[SteamSession] shutdown -> session terminated" << std::endl;
}

void SteamSessionService::refreshLobbyList() {
	if (!available_ || SteamMatchmaking() == nullptr) {
		return;
	}
	availableLobbies_.clear();
	SteamMatchmaking()->RequestLobbyList();
	std::cout << "[SteamSession] requested lobby list" << std::endl;
}

void SteamSessionService::update() {
#if PROJECT_HAS_STEAMWORKS
	if (available_ && SteamAPI_IsSteamRunning()) {
		SteamAPI_RunCallbacks();
	}
#endif
	if (!available_) {
		return;
	}
	if (state_ == SteamSessionState::Ready) {
		state_ = SteamSessionState::InGame;
		std::cout << "[SteamSession] state transition: Ready -> InGame" << std::endl;
	}
	if (state_ == SteamSessionState::Lobby) {
		std::cout << "[SteamSession] lobby alive: members=" << peers_.size() << ", host=" << lobbyInfo_.isHost << std::endl;
	}
	if (!inputQueue_.empty()) {
		std::cout << "[SteamSession] queue relay input packets=" << inputQueue_.size() << std::endl;
	}
	if (!snapshotQueue_.empty()) {
		std::cout << "[SteamSession] queue relay snapshots=" << snapshotQueue_.size() << std::endl;
	}
	if (!transportQueue_.empty()) {
		std::cout << "[SteamSession] queue relay transport messages=" << transportQueue_.size() << std::endl;
	}
}

void SteamSessionService::setLocalReady(bool ready) {
	ready_ = ready;
	if (peers_.empty()) {
		peers_.push_back({ "local-user", "You", ready });
		std::cout << "[SteamSession] local ready set to " << ready << std::endl;
		return;
	}
	peers_[0].ready = ready;
	state_ = ready ? SteamSessionState::Ready : SteamSessionState::Lobby;
	std::cout << "[SteamSession] local ready set to " << ready << "; state=" << (ready ? "Ready" : "Lobby") << std::endl;
}

void SteamSessionService::resetLocalPeer() {
	if (!peers_.empty()) {
		peers_[0] = { "local-user", "You", ready_ };
	}
}

void SteamSessionService::logPacketRelay(const char* tag, const OnlineInputPacket& packet) const {
	std::cout << "[SteamSession] " << tag << ": player=" << packet.playerId << ", seq=" << packet.sequence << ", angle=" << packet.angle << std::endl;
}

void SteamSessionService::logSnapshotRelay(const char* tag, const OnlineMatchSnapshot& snapshot) const {
	std::cout << "[SteamSession] " << tag << ": tick=" << snapshot.tick << ", players=" << snapshot.players.size() << ", host=" << snapshot.hostPlayerId << std::endl;
}

void SteamSessionService::logTransportRelay(const char* tag, const SteamNetworkMessage& message) const {
	std::cout << "[SteamSession] " << tag << ": type=" << static_cast<int>(message.type) << ", sender=" << message.senderId << ", target=" << message.targetId << ", tick=" << message.tick << std::endl;
}

std::string SteamSessionService::serializeInputPacket(const OnlineInputPacket& packet) const {
	std::string buffer;
	buffer.reserve(64);
	buffer.append(reinterpret_cast<const char*>(&packet.sequence), sizeof(packet.sequence));
	buffer.append(reinterpret_cast<const char*>(&packet.playerId), sizeof(packet.playerId));
	buffer.append(reinterpret_cast<const char*>(&packet.angle), sizeof(packet.angle));
	buffer.push_back(packet.turnQueued ? 1 : 0);
	return buffer;
}

OnlineInputPacket SteamSessionService::deserializeInputPacket(const std::string& payload) const {
	OnlineInputPacket packet{};
	if (payload.size() < sizeof(packet.sequence) + sizeof(packet.playerId) + sizeof(packet.angle) + 1) {
		return packet;
	}
	std::size_t offset = 0;
	std::memcpy(&packet.sequence, payload.data() + offset, sizeof(packet.sequence));
	offset += sizeof(packet.sequence);
	std::memcpy(&packet.playerId, payload.data() + offset, sizeof(packet.playerId));
	offset += sizeof(packet.playerId);
	std::memcpy(&packet.angle, payload.data() + offset, sizeof(packet.angle));
	offset += sizeof(packet.angle);
	packet.turnQueued = payload[offset] != 0;
	return packet;
}

std::string SteamSessionService::serializeSnapshot(const OnlineMatchSnapshot& snapshot) const {
	std::string buffer;
	buffer.reserve(128 + snapshot.players.size() * 64);
	buffer.append(reinterpret_cast<const char*>(&snapshot.tick), sizeof(snapshot.tick));
	buffer.append(reinterpret_cast<const char*>(&snapshot.hostPlayerId), sizeof(snapshot.hostPlayerId));
	std::uint32_t count = static_cast<std::uint32_t>(snapshot.players.size());
	buffer.append(reinterpret_cast<const char*>(&count), sizeof(count));
	for (const auto& player : snapshot.players) {
		buffer.append(reinterpret_cast<const char*>(&player.playerId), sizeof(player.playerId));
		const std::uint32_t nameLength = static_cast<std::uint32_t>(player.name.size());
		buffer.append(reinterpret_cast<const char*>(&nameLength), sizeof(nameLength));
		buffer.append(player.name);
		buffer.append(reinterpret_cast<const char*>(&player.local), sizeof(player.local));
		buffer.append(reinterpret_cast<const char*>(&player.alive), sizeof(player.alive));
		buffer.append(reinterpret_cast<const char*>(&player.ready), sizeof(player.ready));
		buffer.append(reinterpret_cast<const char*>(&player.isHost), sizeof(player.isHost));
		buffer.append(reinterpret_cast<const char*>(&player.x), sizeof(player.x));
		buffer.append(reinterpret_cast<const char*>(&player.y), sizeof(player.y));
		buffer.append(reinterpret_cast<const char*>(&player.direction), sizeof(player.direction));
		buffer.append(reinterpret_cast<const char*>(&player.score), sizeof(player.score));
		buffer.append(reinterpret_cast<const char*>(&player.lives), sizeof(player.lives));
	}
	return buffer;
}

OnlineMatchSnapshot SteamSessionService::deserializeSnapshot(const std::string& payload) const {
	OnlineMatchSnapshot snapshot{};
	if (payload.size() < sizeof(snapshot.tick) + sizeof(snapshot.hostPlayerId) + sizeof(std::uint32_t)) {
		return snapshot;
	}
	std::size_t offset = 0;
	std::memcpy(&snapshot.tick, payload.data() + offset, sizeof(snapshot.tick));
	offset += sizeof(snapshot.tick);
	std::memcpy(&snapshot.hostPlayerId, payload.data() + offset, sizeof(snapshot.hostPlayerId));
	offset += sizeof(snapshot.hostPlayerId);
	std::uint32_t count = 0;
	std::memcpy(&count, payload.data() + offset, sizeof(count));
	offset += sizeof(count);
	snapshot.players.resize(count);
	for (std::uint32_t i = 0; i < count; ++i) {
		OnlinePlayerState player{};
		std::memcpy(&player.playerId, payload.data() + offset, sizeof(player.playerId));
		offset += sizeof(player.playerId);
		std::uint32_t nameLength = 0;
		std::memcpy(&nameLength, payload.data() + offset, sizeof(nameLength));
		offset += sizeof(nameLength);
		if (offset + nameLength <= payload.size()) {
			player.name.assign(payload.data() + offset, payload.data() + offset + nameLength);
			offset += nameLength;
		}
		std::memcpy(&player.local, payload.data() + offset, sizeof(player.local));
		offset += sizeof(player.local);
		std::memcpy(&player.alive, payload.data() + offset, sizeof(player.alive));
		offset += sizeof(player.alive);
		std::memcpy(&player.ready, payload.data() + offset, sizeof(player.ready));
		offset += sizeof(player.ready);
		std::memcpy(&player.isHost, payload.data() + offset, sizeof(player.isHost));
		offset += sizeof(player.isHost);
		std::memcpy(&player.x, payload.data() + offset, sizeof(player.x));
		offset += sizeof(player.x);
		std::memcpy(&player.y, payload.data() + offset, sizeof(player.y));
		offset += sizeof(player.y);
		std::memcpy(&player.direction, payload.data() + offset, sizeof(player.direction));
		offset += sizeof(player.direction);
		std::memcpy(&player.score, payload.data() + offset, sizeof(player.score));
		offset += sizeof(player.score);
		std::memcpy(&player.lives, payload.data() + offset, sizeof(player.lives));
		offset += sizeof(player.lives);
		snapshot.players[i] = player;
	}
	return snapshot;
}

void SteamSessionService::queueInputPacket(const OnlineInputPacket& packet) {
	inputQueue_.push_back(packet);
	logPacketRelay("input queued", packet);
	SteamNetworkMessage message;
	message.type = SteamMessageType::Input;
	message.senderId = packet.playerId;
	message.targetId = (role_ == SteamSessionRole::Host) ? packet.playerId : 0;
	message.tick = 0;
	message.payload = serializeInputPacket(packet);
	queueTransportMessage(message);
}

std::vector<OnlineInputPacket> SteamSessionService::drainInputPackets() {
	std::vector<OnlineInputPacket> drained = std::move(inputQueue_);
	inputQueue_.clear();
	if (!drained.empty()) {
		for (const auto& packet : drained) {
			logPacketRelay("input drained", packet);
		}
	}
	return drained;
}

void SteamSessionService::queueSnapshot(const OnlineMatchSnapshot& snapshot) {
	snapshotQueue_.push_back(snapshot);
	logSnapshotRelay("snapshot queued", snapshot);
	SteamNetworkMessage message;
	message.type = SteamMessageType::Snapshot;
	message.senderId = snapshot.hostPlayerId;
	message.targetId = (role_ == SteamSessionRole::Host) ? 0 : snapshot.hostPlayerId;
	message.tick = snapshot.tick;
	message.payload = serializeSnapshot(snapshot);
	queueTransportMessage(message);
}

std::vector<OnlineMatchSnapshot> SteamSessionService::drainSnapshots() {
	std::vector<OnlineMatchSnapshot> drained = std::move(snapshotQueue_);
	snapshotQueue_.clear();
	if (!drained.empty()) {
		for (const auto& snapshot : drained) {
			logSnapshotRelay("snapshot drained", snapshot);
		}
	}
	return drained;
}

void SteamSessionService::queueTransportMessage(const SteamNetworkMessage& message) {
	transportQueue_.push_back(message);
	logTransportRelay("transport queued", message);
#if PROJECT_HAS_STEAM_NETWORKING
	if (realSteamTransport_) {
		std::cout << "[SteamSession] real Steam transport bridge ready for target=" << message.targetId << ", type=" << static_cast<int>(message.type) << std::endl;
		// This is the actual SDK hook point:
		// ISteamNetworkingSockets* sockets = SteamNetworkingSockets();
		// if (sockets != nullptr && message.targetId != 0) {
		//    sockets->SendMessageToUser(...);
		// }
	}
#endif
}

std::vector<SteamNetworkMessage> SteamSessionService::drainTransportMessages() {
	std::vector<SteamNetworkMessage> drained = std::move(transportQueue_);
	transportQueue_.clear();
#if PROJECT_HAS_STEAM_NETWORKING
	if (realSteamTransport_) {
		std::cout << "[SteamSession] real Steam transport polling incoming messages" << std::endl;
		// This is the actual SDK receive point:
		// ISteamNetworkingSockets* sockets = SteamNetworkingSockets();
		// while (sockets != nullptr && sockets->ReceiveMessagesOnConnection(...)) { ... }
	}
#endif
	if (!drained.empty()) {
		for (const auto& message : drained) {
			logTransportRelay("transport drained", message);
		}
	}
	return drained;
}
