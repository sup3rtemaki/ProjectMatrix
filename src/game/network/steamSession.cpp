#include "network/steamSession.h"

#include <cstring>
#include <cmath>
#include <chrono>
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

namespace {
class PacketReader {
public:
	explicit PacketReader(const std::string& data) : data_(data) {}

	template<typename T>
	bool read(T& value) {
		if (sizeof(T) > data_.size() - offset_) return false;
		std::memcpy(&value, data_.data() + offset_, sizeof(T));
		offset_ += sizeof(T);
		return true;
	}

	bool readBool(bool& value) {
		unsigned char byte = 0;
		if (!read(byte) || byte > 1) return false;
		value = byte != 0;
		return true;
	}

	bool readString(std::string& value, std::size_t length) {
		if (length > data_.size() - offset_) return false;
		value.assign(data_.data() + offset_, length);
		offset_ += length;
		return true;
	}

	bool finished() const { return offset_ == data_.size(); }

private:
	const std::string& data_;
	std::size_t offset_ = 0;
};
}

void SteamSessionService::onLobbyCreated(LobbyCreated_t* pCallback, bool ioFailure) {
	if (!pCallback || ioFailure) {
		std::cerr << "[SteamSession] Lobby creation request failed" << std::endl;
		state_ = SteamSessionState::Offline;
		role_ = SteamSessionRole::None;
		return;
	}
	if (pCallback->m_eResult != k_EResultOK) {
		std::cout << "[SteamSession] lobby creation failed, result=" << static_cast<int>(pCallback->m_eResult) << std::endl;
		state_ = SteamSessionState::Offline;
		role_ = SteamSessionRole::None;
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
		SteamMatchmaking()->SetLobbyData(CSteamID(currentLobbyId_), "name", lobbyInfo_.name.c_str());
	}
	refreshLobbyMembers();
	std::cout << "[SteamSession] real lobby created: " << lobbyInfo_.lobbyId << std::endl;
}

void SteamSessionService::onLobbyEntered(LobbyEnter_t* pCallback, bool ioFailure) {
	if (!pCallback || ioFailure || pCallback->m_EChatRoomEnterResponse != k_EChatRoomEnterResponseSuccess) {
		std::cerr << "[SteamSession] Lobby join failed" << std::endl;
		state_ = SteamSessionState::Offline;
		role_ = SteamSessionRole::None;
		currentLobbyId_ = 0;
		return;
	}
	currentLobbyId_ = static_cast<std::uint64_t>(pCallback->m_ulSteamIDLobby);
	lobbyInfo_.lobbyId = std::to_string(currentLobbyId_);
	lobbyInfo_.available = true;
	lobbyInfo_.isHost = SteamMatchmaking()->GetLobbyOwner(CSteamID(currentLobbyId_)) == SteamUser()->GetSteamID();
	role_ = lobbyInfo_.isHost ? SteamSessionRole::Host : SteamSessionRole::Client;
	state_ = SteamSessionState::Lobby;
	ready_ = false;
	peers_.clear();
	peers_.push_back({ std::to_string(SteamUser()->GetSteamID().ConvertToUint64()), "You", false });
	if (SteamMatchmaking()) {
		lobbyInfo_.memberCount = SteamMatchmaking()->GetNumLobbyMembers(CSteamID(currentLobbyId_));
		lobbyInfo_.name = SteamMatchmaking()->GetLobbyData(CSteamID(currentLobbyId_), "name");
	}
	refreshLobbyMembers();
	std::cout << "[SteamSession] joined lobby: " << lobbyInfo_.lobbyId << std::endl;
}

void SteamSessionService::onLobbyMatchList(LobbyMatchList_t* pCallback, bool ioFailure) {
	if (!pCallback || ioFailure) {
		std::cerr << "[SteamSession] Lobby search failed" << std::endl;
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

void SteamSessionService::onLobbyMembershipChanged(LobbyChatUpdate_t* callback) {
	if (callback == nullptr || currentLobbyId_ != callback->m_ulSteamIDLobby) return;
	refreshLobbyMembers();
}

void SteamSessionService::refreshLobbyMembers() {
	if (!steamInitialized_ || currentLobbyId_ == 0 || SteamMatchmaking() == nullptr
		|| SteamFriends() == nullptr || SteamUser() == nullptr) return;
	const CSteamID lobby(currentLobbyId_);
	lobbyInfo_.memberCount = SteamMatchmaking()->GetNumLobbyMembers(lobby);
	peers_.clear();
	for (int i = 0; i < lobbyInfo_.memberCount; ++i) {
		const CSteamID member = SteamMatchmaking()->GetLobbyMemberByIndex(lobby, i);
		peers_.push_back({ std::to_string(member.ConvertToUint64()),
			SteamFriends()->GetFriendPersonaName(member), member == SteamUser()->GetSteamID() && ready_ });
	}
}

bool SteamSessionService::isLobbyMember(std::uint64_t peerId) const {
	if (!steamInitialized_ || currentLobbyId_ == 0 || peerId == 0 || SteamMatchmaking() == nullptr) return false;
	const CSteamID lobby(currentLobbyId_);
	const int count = SteamMatchmaking()->GetNumLobbyMembers(lobby);
	for (int i = 0; i < count; ++i) {
		if (SteamMatchmaking()->GetLobbyMemberByIndex(lobby, i).ConvertToUint64() == peerId) return true;
	}
	return false;
}

std::uint64_t SteamSessionService::localSteamId() const {
	if (!steamInitialized_ || SteamUser() == nullptr) return 0;
	return SteamUser()->GetSteamID().ConvertToUint64();
}

std::uint64_t SteamSessionService::hostSteamId() const {
	if (!steamInitialized_ || currentLobbyId_ == 0 || SteamMatchmaking() == nullptr) return 0;
	return SteamMatchmaking()->GetLobbyOwner(CSteamID(currentLobbyId_)).ConvertToUint64();
}

std::vector<std::uint64_t> SteamSessionService::lobbyMemberIds() const {
	std::vector<std::uint64_t> members;
	if (!steamInitialized_ || currentLobbyId_ == 0 || SteamMatchmaking() == nullptr) return members;
	const CSteamID lobby(currentLobbyId_);
	const int count = SteamMatchmaking()->GetNumLobbyMembers(lobby);
	for (int i = 0; i < count; ++i) {
		members.push_back(SteamMatchmaking()->GetLobbyMemberByIndex(lobby, i).ConvertToUint64());
	}
	return members;
}

bool SteamSessionService::setLobbyJoinable(bool joinable) {
	if (!steamInitialized_ || currentLobbyId_ == 0 || !isHost()
		|| hostSteamId() != localSteamId() || SteamMatchmaking() == nullptr
		|| !SteamMatchmaking()->SetLobbyJoinable(CSteamID(currentLobbyId_), joinable)) {
		std::cerr << "[SteamSession] Failed to update lobby joinability" << std::endl;
		return false;
	}
	return true;
}

void SteamSessionService::onPeerSessionRequest(SteamNetworkingMessagesSessionRequest_t* callback) {
	if (callback == nullptr || SteamNetworkingMessages() == nullptr) return;
	if (!isLobbyMember(callback->m_identityRemote.GetSteamID64())) {
		std::cerr << "[SteamSession] Rejected networking session from non-member" << std::endl;
		return;
	}
	if (!SteamNetworkingMessages()->AcceptSessionWithUser(callback->m_identityRemote)) {
		std::cerr << "[SteamSession] Failed to accept networking session" << std::endl;
	}
}

void SteamSessionService::onPeerSessionFailed(SteamNetworkingMessagesSessionFailed_t* callback) {
	if (callback != nullptr) {
		std::cerr << "[SteamSession] Peer session failed: " << callback->m_info.m_szEndDebug << std::endl;
	}
}

bool SteamSessionService::sendPeerMessage(std::uint64_t peerId, SteamMessageType type, const std::string& payload) {
	constexpr std::size_t MAX_PAYLOAD = 64 * 1024;
	if (!isLobbyMember(peerId) || peerId == SteamUser()->GetSteamID().ConvertToUint64()
		|| SteamNetworkingMessages() == nullptr || payload.size() > MAX_PAYLOAD
		|| static_cast<unsigned int>(type) > static_cast<unsigned int>(SteamMessageType::Ping)) {
		std::cerr << "[SteamSession] Cannot send peer message: invalid peer, type or size" << std::endl;
		return false;
	}
	std::string wire(1, static_cast<char>(type));
	wire.append(payload);
	SteamNetworkingIdentity identity;
	identity.Clear();
	identity.SetSteamID64(peerId);
	const EResult result = SteamNetworkingMessages()->SendMessageToUser(identity, wire.data(),
		static_cast<std::uint32_t>(wire.size()), k_nSteamNetworkingSend_Reliable, 0);
	if (result != k_EResultOK) {
		std::cerr << "[SteamSession] Peer send failed: " << static_cast<int>(result) << std::endl;
		return false;
	}
	return true;
}

bool SteamSessionService::sendDrivingInput(std::uint64_t hostId, const DrivingInputPacket& input) {
	if (!steamInitialized_ || role_ != SteamSessionRole::Client || currentLobbyId_ == 0
		|| hostId == 0 || hostSteamId() != hostId) {
		std::cerr << "[SteamSession] Cannot send controls: recipient is not the lobby host" << std::endl;
		return false;
	}
	return sendPeerMessage(hostId, SteamMessageType::Input, serializeDrivingInput(input));
}

bool SteamSessionService::synchronizeDrivingPlayers(HostDrivingInputs& inputs) {
	if (role_ != SteamSessionRole::Host || hostSteamId() == 0 || hostSteamId() != localSteamId()) {
		std::cerr << "[SteamSession] Cannot synchronize controls without an authoritative lobby host" << std::endl;
		inputs.clear();
		return false;
	}
	const CSteamID lobby(currentLobbyId_);
	const int count = SteamMatchmaking()->GetNumLobbyMembers(lobby);
	std::vector<std::uint64_t> players;
	for (int i = 0; i < count; ++i) {
		const std::uint64_t id = SteamMatchmaking()->GetLobbyMemberByIndex(lobby, i).ConvertToUint64();
		if (id != localSteamId()) players.push_back(id);
	}
	inputs.retainPlayers(players);
	for (const std::uint64_t id : players) {
		if (!inputs.addPlayer(id)) {
			std::cerr << "[SteamSession] Cannot register lobby player for driving input" << std::endl;
			return false;
		}
	}
	return true;
}

bool SteamSessionService::acceptDrivingInput(const SteamPeerMessage& message, HostDrivingInputs& inputs) {
	if (role_ != SteamSessionRole::Host || message.type != SteamMessageType::Input
		|| hostSteamId() == 0 || hostSteamId() != localSteamId()
		|| !isLobbyMember(message.senderId) || message.senderId == localSteamId()) {
		std::cerr << "[SteamSession] Rejected driving input from invalid sender" << std::endl;
		return false;
	}
	const auto packet = deserializeDrivingInput(message.payload);
	if (!packet || !inputs.accept(message.senderId, *packet)) {
		std::cerr << "[SteamSession] Rejected invalid, stale or unregistered driving input" << std::endl;
		return false;
	}
	return true;
}

std::vector<SteamPeerMessage> SteamSessionService::receivePeerMessages() {
	std::vector<SteamPeerMessage> result;
	if (!steamInitialized_ || currentLobbyId_ == 0 || SteamNetworkingMessages() == nullptr) return result;
	SteamNetworkingMessage_t* messages[32];
	const int count = SteamNetworkingMessages()->ReceiveMessagesOnChannel(0, messages, 32);
	for (int i = 0; i < count; ++i) {
		SteamNetworkingMessage_t* message = messages[i];
		const std::uint64_t sender = message->m_identityPeer.GetSteamID64();
		if (message->m_cbSize < 1 || message->m_cbSize > 64 * 1024 + 1
			|| message->m_pData == nullptr || !isLobbyMember(sender)) {
			std::cerr << "[SteamSession] Rejected invalid peer message" << std::endl;
		}
		else {
			const char* data = static_cast<const char*>(message->m_pData);
			const unsigned char type = static_cast<unsigned char>(data[0]);
			if (type > static_cast<unsigned int>(SteamMessageType::Ping)) {
				std::cerr << "[SteamSession] Rejected unknown peer message type" << std::endl;
			}
			else {
				result.push_back({ sender, static_cast<SteamMessageType>(type),
					std::string(data + 1, static_cast<std::size_t>(message->m_cbSize - 1)) });
			}
		}
		message->Release();
	}
	return result;
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
		std::cerr << "[SteamSession] SteamAPI_Init failed; online unavailable. Check Steam and steam_appid.txt" << std::endl;
		available_ = false;
	}
	else {
		steamInitialized_ = true;
		available_ = SteamAPI_IsSteamRunning() && SteamMatchmaking() != nullptr;
		std::cout << "[SteamSession] Steamworks initialized: " << (available_ ? "online" : "offline") << std::endl;
	}
#else
	std::cerr << "[SteamSession] Steamworks SDK not found; online unavailable" << std::endl;
	available_ = false;
#endif

	initialized_ = true;
	realSteamTransport_ = false;
#if PROJECT_HAS_STEAM_NETWORKING
	realSteamTransport_ = steamInitialized_ && SteamNetworkingSockets() != nullptr;
	if (realSteamTransport_) {
		std::cout << "[SteamSession] real Steam networking sockets available" << std::endl;
	}
#endif
	state_ = SteamSessionState::Offline;
	lobbyInfo_ = {};
	peers_.clear();
	return available_;
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
	lobbyCreatedResult_.Set(request, this, &SteamSessionService::onLobbyCreated);
	role_ = SteamSessionRole::Host;
	ready_ = false;
	state_ = SteamSessionState::Lobby;
	lobbyInfo_ = { "pending", name.empty() ? "Local Steam Lobby" : name, maxPlayers, 1, true, false };
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
		const SteamAPICall_t request = SteamMatchmaking()->JoinLobby(targetLobby);
		if (request == k_uAPICallInvalid) {
			std::cerr << "[SteamSession] JoinLobby request invalid" << std::endl;
			return false;
		}
		lobbyEnteredResult_.Set(request, this, &SteamSessionService::onLobbyEntered);
		role_ = SteamSessionRole::Client;
		ready_ = false;
		state_ = SteamSessionState::Lobby;
		lobbyInfo_ = { lobbyId, "Joined Steam Lobby", 4, 1, false, false };
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
	lobbyCreatedResult_.Cancel();
	lobbyEnteredResult_.Cancel();
	lobbyListResult_.Cancel();
	if (steamInitialized_ && SteamNetworkingMessages() != nullptr && SteamMatchmaking() != nullptr && currentLobbyId_ != 0) {
		const CSteamID lobby(currentLobbyId_);
		const int count = SteamMatchmaking()->GetNumLobbyMembers(lobby);
		for (int i = 0; i < count; ++i) {
			SteamNetworkingIdentity identity;
			identity.Clear();
			identity.SetSteamID(SteamMatchmaking()->GetLobbyMemberByIndex(lobby, i));
			if (identity.GetSteamID() != SteamUser()->GetSteamID()) SteamNetworkingMessages()->CloseSessionWithUser(identity);
		}
	}
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
	if (!initialized_) return;
	leaveLobby();
	initialized_ = false;
	available_ = false;
#if PROJECT_HAS_STEAMWORKS
	if (steamInitialized_) SteamAPI_Shutdown();
#endif
	steamInitialized_ = false;
	std::cout << "[SteamSession] shutdown -> session terminated" << std::endl;
}

void SteamSessionService::refreshLobbyList() {
	if (!available_ || SteamMatchmaking() == nullptr) {
		return;
	}
	const auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
	const std::uint32_t now = static_cast<std::uint32_t>(
		std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
	if (lobbyListResult_.IsActive() || (lobbyListRequested_ && now - lastLobbyListRequest_ < 5000)) return;
	SteamMatchmaking()->AddRequestLobbyListStringFilter("game", "lightcycles", k_ELobbyComparisonEqual);
	const SteamAPICall_t request = SteamMatchmaking()->RequestLobbyList();
	if (request == k_uAPICallInvalid) {
		std::cerr << "[SteamSession] Lobby search request invalid" << std::endl;
		return;
	}
	lobbyListRequested_ = true;
	lastLobbyListRequest_ = now;
	lobbyListResult_.Set(request, this, &SteamSessionService::onLobbyMatchList);
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

std::optional<OnlineInputPacket> SteamSessionService::deserializeInputPacket(const std::string& payload) const {
	OnlineInputPacket packet{};
	PacketReader reader(payload);
	if (!reader.read(packet.sequence) || !reader.read(packet.playerId) || !reader.read(packet.angle)
		|| !reader.readBool(packet.turnQueued) || !reader.finished() || !std::isfinite(packet.angle)) {
		std::cerr << "[SteamSession] Invalid input packet" << std::endl;
		return std::nullopt;
	}
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

std::optional<OnlineMatchSnapshot> SteamSessionService::deserializeSnapshot(const std::string& payload) const {
	OnlineMatchSnapshot snapshot{};
	PacketReader reader(payload);
	std::uint32_t count = 0;
	if (!reader.read(snapshot.tick) || !reader.read(snapshot.hostPlayerId) || !reader.read(count) || count > 64) {
		std::cerr << "[SteamSession] Invalid snapshot header" << std::endl;
		return std::nullopt;
	}
	for (std::uint32_t i = 0; i < count; ++i) {
		OnlinePlayerState player{};
		std::uint32_t nameLength = 0;
		if (!reader.read(player.playerId) || !reader.read(nameLength) || nameLength > 256
			|| !reader.readString(player.name, nameLength) || !reader.readBool(player.local)
			|| !reader.readBool(player.alive) || !reader.readBool(player.ready) || !reader.readBool(player.isHost)
			|| !reader.read(player.x) || !reader.read(player.y) || !reader.read(player.direction)
			|| !reader.read(player.score) || !reader.read(player.lives)
			|| !std::isfinite(player.x) || !std::isfinite(player.y) || !std::isfinite(player.direction)) {
			std::cerr << "[SteamSession] Invalid snapshot player" << std::endl;
			return std::nullopt;
		}
		snapshot.players.push_back(std::move(player));
	}
	if (!reader.finished()) {
		std::cerr << "[SteamSession] Trailing bytes in snapshot" << std::endl;
		return std::nullopt;
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
