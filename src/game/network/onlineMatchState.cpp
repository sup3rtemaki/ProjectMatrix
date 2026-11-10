#include "network/onlineMatchState.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <set>
#include <utility>

namespace {
constexpr std::uint8_t CYCLE_SNAPSHOT_VERSION = 1;
constexpr std::uint8_t WORLD_SNAPSHOT_VERSION = 1;
constexpr float MAX_SNAPSHOT_COORDINATE = 1000000.0f;
constexpr float MAX_SNAPSHOT_SPEED = 1000.0f;

bool validPoint(const SnapshotPoint& point) {
	return std::isfinite(point.x) && std::isfinite(point.y)
		&& std::abs(point.x) <= MAX_SNAPSHOT_COORDINATE && std::abs(point.y) <= MAX_SNAPSHOT_COORDINATE;
}

bool validSnapshot(const CycleSnapshot& snapshot) {
	if (snapshot.cycles.size() > CycleSnapshot::MAX_HUMANS + CycleSnapshot::MAX_BOTS) return false;
	std::size_t humans = 0;
	std::size_t bots = 0;
	std::set<std::pair<SnapshotCycleKind, std::uint64_t>> identities;
	for (const SnapshotCycle& cycle : snapshot.cycles) {
		if (cycle.kind == SnapshotCycleKind::Human) ++humans;
		else if (cycle.kind == SnapshotCycleKind::Bot) ++bots;
		else return false;
		if (cycle.id == 0 || !identities.emplace(cycle.kind, cycle.id).second
			|| !validPoint(cycle.position) || !std::isfinite(cycle.heading)
			|| cycle.heading < 0.0f || cycle.heading >= 360.0f
			|| !std::isfinite(cycle.speed) || cycle.speed < 0.0f || cycle.speed > MAX_SNAPSHOT_SPEED
			|| cycle.score > static_cast<std::uint32_t>(std::numeric_limits<int>::max())
			|| cycle.lives > 3 || cycle.animationFrame > 0xffffu || (cycle.alive && cycle.respawning)
			|| cycle.trail.size() > CycleSnapshot::MAX_TRAIL_POINTS) return false;
		for (const SnapshotPoint& point : cycle.trail) {
			if (!validPoint(point)) return false;
		}
	}
	return humans <= CycleSnapshot::MAX_HUMANS && bots <= CycleSnapshot::MAX_BOTS;
}

class SnapshotWriter {
public:
	void integer(std::uint64_t value, int bytes) {
		for (int i = 0; i < bytes; ++i) data_.push_back(static_cast<char>((value >> (i * 8)) & 0xffu));
	}
	void real(float value) {
		static_assert(sizeof(float) == sizeof(std::uint32_t), "Snapshot requires 32-bit floats");
		static_assert(std::numeric_limits<float>::is_iec559, "Snapshot requires IEEE 754 floats");
		std::uint32_t bits;
		std::memcpy(&bits, &value, sizeof(bits));
		integer(bits, 4);
	}
	std::string take() { return std::move(data_); }

private:
	std::string data_;
};

class SnapshotReader {
public:
	explicit SnapshotReader(const std::string& data) : data_(data) {}
	bool integer(std::uint64_t& value, int bytes) {
		if (static_cast<std::size_t>(bytes) > data_.size() - offset_) return false;
		value = 0;
		for (int i = 0; i < bytes; ++i) {
			value |= static_cast<std::uint64_t>(static_cast<unsigned char>(data_[offset_++])) << (i * 8);
		}
		return true;
	}
	bool real(float& value) {
		std::uint64_t raw;
		if (!integer(raw, 4)) return false;
		const std::uint32_t bits = static_cast<std::uint32_t>(raw);
		std::memcpy(&value, &bits, sizeof(value));
		return true;
	}
	bool finished() const { return offset_ == data_.size(); }

private:
	const std::string& data_;
	std::size_t offset_ = 0;
};

bool validCellRect(const SnapshotCellRect& cells) {
	return cells.x <= 0xffffu && cells.y <= 0xffffu && cells.w > 0 && cells.h > 0
		&& cells.w <= 0xffffu && cells.h <= 0xffffu
		&& cells.x + cells.w <= 0x10000u && cells.y + cells.h <= 0x10000u;
}

bool validWorldSnapshot(const WorldSnapshot& snapshot) {
	if (snapshot.objects.size() > WorldSnapshot::MAX_OBJECTS || snapshot.towers.size() > WorldSnapshot::MAX_TOWERS) return false;
	std::set<std::pair<std::uint32_t, std::uint32_t>> locations;
	for (const SnapshotObject& object : snapshot.objects) {
		if (static_cast<std::uint32_t>(object.type) > static_cast<std::uint32_t>(SnapshotObjectType::Slow)
			|| !validCellRect(object.cells) || !locations.emplace(object.cells.x, object.cells.y).second) return false;
	}
	locations.clear();
	std::size_t projectileCount = 0;
	for (const SnapshotTower& tower : snapshot.towers) {
		if (!validCellRect(tower.placement) || !locations.emplace(tower.placement.x, tower.placement.y).second
			|| tower.animationFrame > 0xffffu || tower.projectiles.size() > WorldSnapshot::MAX_PROJECTILES - projectileCount) return false;
		projectileCount += tower.projectiles.size();
		for (const SnapshotProjectile& projectile : tower.projectiles) {
			if (!validPoint(projectile.position) || projectile.animationFrame > 0xffffu) return false;
		}
	}
	return true;
}

void writeCellRect(SnapshotWriter& writer, const SnapshotCellRect& cells) {
	writer.integer(cells.x, 2);
	writer.integer(cells.y, 2);
	writer.integer(cells.w, 2);
	writer.integer(cells.h, 2);
}

bool readCellRect(SnapshotReader& reader, SnapshotCellRect& cells) {
	std::uint64_t value;
	if (!reader.integer(value, 2)) return false;
	cells.x = static_cast<std::uint32_t>(value);
	if (!reader.integer(value, 2)) return false;
	cells.y = static_cast<std::uint32_t>(value);
	if (!reader.integer(value, 2)) return false;
	cells.w = static_cast<std::uint32_t>(value);
	if (!reader.integer(value, 2)) return false;
	cells.h = static_cast<std::uint32_t>(value);
	return true;
}
}

bool validateWorldSnapshot(const WorldSnapshot& snapshot) {
	return validWorldSnapshot(snapshot);
}

std::optional<std::string> serializeWorldSnapshot(const WorldSnapshot& snapshot) {
	if (!validWorldSnapshot(snapshot)) {
		std::cerr << "[OnlineMatch] Invalid world snapshot" << std::endl;
		return std::nullopt;
	}
	SnapshotWriter writer;
	writer.integer(WORLD_SNAPSHOT_VERSION, 1);
	writer.integer(snapshot.sequence, 4);
	writer.integer(snapshot.objects.size(), 2);
	writer.integer(snapshot.towers.size(), 2);
	for (const SnapshotObject& object : snapshot.objects) {
		writer.integer(static_cast<std::uint8_t>(object.type), 1);
		writeCellRect(writer, object.cells);
	}
	for (const SnapshotTower& tower : snapshot.towers) {
		writeCellRect(writer, tower.placement);
		writer.integer(tower.attacking ? 1 : 0, 1);
		writer.integer(tower.animationFrame, 2);
		writer.integer(tower.projectiles.size(), 2);
		for (const SnapshotProjectile& projectile : tower.projectiles) {
			writer.real(projectile.position.x);
			writer.real(projectile.position.y);
			writer.integer(projectile.animationFrame, 2);
		}
	}
	std::string payload = writer.take();
	if (payload.size() > WorldSnapshot::MAX_BYTES) {
		std::cerr << "[OnlineMatch] World snapshot exceeds transport limit" << std::endl;
		return std::nullopt;
	}
	return payload;
}

std::optional<WorldSnapshot> deserializeWorldSnapshot(const std::string& payload) {
	const auto reject = []() -> std::optional<WorldSnapshot> {
		std::cerr << "[OnlineMatch] Invalid world snapshot packet" << std::endl;
		return std::nullopt;
	};
	if (payload.size() > WorldSnapshot::MAX_BYTES) return reject();
	SnapshotReader reader(payload);
	std::uint64_t value;
	if (!reader.integer(value, 1) || value != WORLD_SNAPSHOT_VERSION) return reject();
	if (!reader.integer(value, 4)) return reject();
	WorldSnapshot snapshot;
	snapshot.sequence = static_cast<std::uint32_t>(value);
	if (!reader.integer(value, 2) || value > WorldSnapshot::MAX_OBJECTS) return reject();
	const std::size_t objectCount = static_cast<std::size_t>(value);
	if (!reader.integer(value, 2) || value > WorldSnapshot::MAX_TOWERS) return reject();
	const std::size_t towerCount = static_cast<std::size_t>(value);
	snapshot.objects.reserve(objectCount);
	snapshot.towers.reserve(towerCount);
	for (std::size_t i = 0; i < objectCount; ++i) {
		SnapshotObject object;
		if (!reader.integer(value, 1) || value > static_cast<std::uint8_t>(SnapshotObjectType::Slow)) return reject();
		object.type = static_cast<SnapshotObjectType>(value);
		if (!readCellRect(reader, object.cells)) return reject();
		snapshot.objects.push_back(object);
	}
	std::size_t totalProjectiles = 0;
	for (std::size_t i = 0; i < towerCount; ++i) {
		SnapshotTower tower;
		if (!readCellRect(reader, tower.placement) || !reader.integer(value, 1) || value > 1) return reject();
		tower.attacking = value != 0;
		if (!reader.integer(value, 2)) return reject();
		tower.animationFrame = static_cast<std::uint32_t>(value);
		if (!reader.integer(value, 2) || value > WorldSnapshot::MAX_PROJECTILES - totalProjectiles) return reject();
		const std::size_t count = static_cast<std::size_t>(value);
		totalProjectiles += count;
		tower.projectiles.reserve(count);
		for (std::size_t p = 0; p < count; ++p) {
			SnapshotProjectile projectile;
			if (!reader.real(projectile.position.x) || !reader.real(projectile.position.y)
				|| !reader.integer(value, 2)) return reject();
			projectile.animationFrame = static_cast<std::uint32_t>(value);
			tower.projectiles.push_back(projectile);
		}
		snapshot.towers.push_back(std::move(tower));
	}
	if (!reader.finished() || !validWorldSnapshot(snapshot)) return reject();
	return snapshot;
}

bool validateCycleSnapshot(const CycleSnapshot& snapshot) {
	return validSnapshot(snapshot);
}

std::optional<std::string> serializeCycleSnapshot(const CycleSnapshot& snapshot) {
	if (!validSnapshot(snapshot)) {
		std::cerr << "[OnlineMatch] Invalid cycle snapshot" << std::endl;
		return std::nullopt;
	}
	SnapshotWriter writer;
	writer.integer(CYCLE_SNAPSHOT_VERSION, 1);
	writer.integer(snapshot.sequence, 4);
	writer.integer(snapshot.cycles.size(), 1);
	for (const SnapshotCycle& cycle : snapshot.cycles) {
		writer.integer(static_cast<std::uint8_t>(cycle.kind), 1);
		writer.integer(cycle.id, 8);
		writer.real(cycle.position.x);
		writer.real(cycle.position.y);
		writer.real(cycle.heading);
		writer.real(cycle.speed);
		writer.integer(cycle.score, 4);
		writer.integer(cycle.lives, 1);
		const std::uint8_t flags = (cycle.alive ? 1 : 0) | (cycle.respawning ? 2 : 0)
			| (cycle.boost ? 4 : 0) | (cycle.longTrail ? 8 : 0) | (cycle.slow ? 16 : 0);
		writer.integer(flags, 1);
		for (std::uint8_t value : cycle.glow) writer.integer(value, 1);
		for (std::uint8_t value : cycle.core) writer.integer(value, 1);
		writer.integer(cycle.animationFrame, 2);
		writer.integer(cycle.trail.size(), 2);
		for (const SnapshotPoint& point : cycle.trail) {
			writer.real(point.x);
			writer.real(point.y);
		}
	}
	std::string payload = writer.take();
	if (payload.size() > CycleSnapshot::MAX_BYTES) {
		std::cerr << "[OnlineMatch] Cycle snapshot exceeds transport limit" << std::endl;
		return std::nullopt;
	}
	return payload;
}

std::optional<CycleSnapshot> deserializeCycleSnapshot(const std::string& payload) {
	const auto reject = []() -> std::optional<CycleSnapshot> {
		std::cerr << "[OnlineMatch] Invalid cycle snapshot packet" << std::endl;
		return std::nullopt;
	};
	if (payload.size() > CycleSnapshot::MAX_BYTES) return reject();
	SnapshotReader reader(payload);
	std::uint64_t value;
	if (!reader.integer(value, 1) || value != CYCLE_SNAPSHOT_VERSION) return reject();
	if (!reader.integer(value, 4)) return reject();
	CycleSnapshot snapshot;
	snapshot.sequence = static_cast<std::uint32_t>(value);
	if (!reader.integer(value, 1) || value > CycleSnapshot::MAX_HUMANS + CycleSnapshot::MAX_BOTS) return reject();
	const std::size_t count = static_cast<std::size_t>(value);
	snapshot.cycles.reserve(count);
	for (std::size_t i = 0; i < count; ++i) {
		SnapshotCycle cycle;
		if (!reader.integer(value, 1) || value > static_cast<std::uint8_t>(SnapshotCycleKind::Bot)) return reject();
		cycle.kind = static_cast<SnapshotCycleKind>(value);
		if (!reader.integer(cycle.id, 8) || !reader.real(cycle.position.x) || !reader.real(cycle.position.y)
			|| !reader.real(cycle.heading) || !reader.real(cycle.speed) || !reader.integer(value, 4)) return reject();
		cycle.score = static_cast<std::uint32_t>(value);
		if (!reader.integer(value, 1)) return reject();
		cycle.lives = static_cast<std::uint32_t>(value);
		if (!reader.integer(value, 1) || (value & ~0x1fu) != 0) return reject();
		cycle.alive = (value & 1) != 0;
		cycle.respawning = (value & 2) != 0;
		cycle.boost = (value & 4) != 0;
		cycle.longTrail = (value & 8) != 0;
		cycle.slow = (value & 16) != 0;
		for (std::uint8_t& color : cycle.glow) {
			if (!reader.integer(value, 1)) return reject();
			color = static_cast<std::uint8_t>(value);
		}
		for (std::uint8_t& color : cycle.core) {
			if (!reader.integer(value, 1)) return reject();
			color = static_cast<std::uint8_t>(value);
		}
		if (!reader.integer(value, 2)) return reject();
		cycle.animationFrame = static_cast<std::uint32_t>(value);
		if (!reader.integer(value, 2) || value > CycleSnapshot::MAX_TRAIL_POINTS) return reject();
		const std::size_t points = static_cast<std::size_t>(value);
		cycle.trail.reserve(points);
		for (std::size_t p = 0; p < points; ++p) {
			SnapshotPoint point;
			if (!reader.real(point.x) || !reader.real(point.y)) return reject();
			cycle.trail.push_back(point);
		}
		snapshot.cycles.push_back(std::move(cycle));
	}
	if (!reader.finished() || !validSnapshot(snapshot)) return reject();
	return snapshot;
}

bool SnapshotSequence::accept(std::uint32_t sequence) {
	const std::uint32_t difference = sequence - sequence_;
	if (received_ && (difference == 0 || difference >= 0x80000000u)) return false;
	sequence_ = sequence;
	received_ = true;
	return true;
}

std::optional<std::array<std::string, 2>> serializeOnlineFrame(const OnlineFrame& frame) {
	if (frame.cycles.sequence != frame.world.sequence
		|| static_cast<unsigned int>(frame.phase) > static_cast<unsigned int>(OnlinePhase::Finished)) {
		std::cerr << "[OnlineMatch] Inconsistent frame metadata" << std::endl;
		return std::nullopt;
	}
	const auto cycles = serializeCycleSnapshot(frame.cycles);
	const auto world = serializeWorldSnapshot(frame.world);
	if (!cycles || !world) return std::nullopt;
	std::array<std::string, 2> result;
	for (std::size_t i = 0; i < result.size(); ++i) {
		result[i].push_back(1);
		result[i].push_back(static_cast<char>(i));
		result[i].push_back(static_cast<char>(frame.levelIndex & 0xffu));
		result[i].push_back(static_cast<char>((frame.levelIndex >> 8) & 0xffu));
		result[i].push_back(static_cast<char>(frame.phase));
		result[i] += i == 0 ? *cycles : *world;
		if (result[i].size() > CycleSnapshot::MAX_BYTES) {
			std::cerr << "[OnlineMatch] Frame part exceeds transport limit" << std::endl;
			return std::nullopt;
		}
	}
	return result;
}

void OnlineFrameAssembler::reset() {
	cycleHeader_.reset();
	worldHeader_.reset();
	cycles_.reset();
	world_.reset();
}

std::optional<OnlineFrame> OnlineFrameAssembler::accept(const std::string& payload) {
	if (payload.size() < 5 || payload.size() > CycleSnapshot::MAX_BYTES
		|| static_cast<unsigned char>(payload[0]) != 1
		|| static_cast<unsigned char>(payload[1]) > 1
		|| static_cast<unsigned char>(payload[4]) > static_cast<unsigned char>(OnlinePhase::Finished)) {
		std::cerr << "[OnlineMatch] Invalid frame part header" << std::endl;
		return std::nullopt;
	}
	const Header header{
		static_cast<std::uint16_t>(static_cast<unsigned char>(payload[2])
			| (static_cast<unsigned int>(static_cast<unsigned char>(payload[3])) << 8)),
		static_cast<OnlinePhase>(static_cast<unsigned char>(payload[4]))
	};
	if (payload[1] == 0) {
		auto parsed = deserializeCycleSnapshot(payload.substr(5));
		if (!parsed) return std::nullopt;
		cycles_ = std::move(*parsed);
		cycleHeader_ = header;
	}
	else {
		auto parsed = deserializeWorldSnapshot(payload.substr(5));
		if (!parsed) return std::nullopt;
		world_ = std::move(*parsed);
		worldHeader_ = header;
	}
	if (!cycles_ || !world_) return std::nullopt;
	if (cycles_->sequence != world_->sequence) {
		const std::uint32_t diff = cycles_->sequence - world_->sequence;
		if (diff < 0x80000000u) { world_.reset(); worldHeader_.reset(); }
		else { cycles_.reset(); cycleHeader_.reset(); }
		return std::nullopt;
	}
	if (cycleHeader_->levelIndex != worldHeader_->levelIndex || cycleHeader_->phase != worldHeader_->phase) {
		std::cerr << "[OnlineMatch] Frame parts disagree on metadata" << std::endl;
		reset();
		return std::nullopt;
	}
	OnlineFrame frame{ header.levelIndex, header.phase, std::move(*cycles_), std::move(*world_) };
	reset();
	return frame;
}

std::string serializeDrivingInput(const DrivingInputPacket& packet) {
	std::string bytes;
	bytes.reserve(6);
	bytes.push_back(1);
	for (int shift = 0; shift < 32; shift += 8) {
		bytes.push_back(static_cast<char>((packet.sequence >> shift) & 0xffu));
	}
	const unsigned char flags = (packet.input.accelerate ? 1 : 0)
		| (packet.input.brake ? 2 : 0) | (packet.input.left ? 4 : 0) | (packet.input.right ? 8 : 0);
	bytes.push_back(static_cast<char>(flags));
	return bytes;
}

std::string serializePlayerReady(std::uint8_t colorIndex) {
	std::string payload;
	payload.push_back(1);
	payload.push_back(static_cast<char>(colorIndex));
	return payload;
}

std::optional<std::uint8_t> deserializePlayerReady(const std::string& payload) {
	if (payload.size() != 2 || static_cast<unsigned char>(payload[0]) != 1
		|| static_cast<unsigned char>(payload[1]) >= 8) {
		std::cerr << "[OnlineMatch] Invalid player ready packet" << std::endl;
		return std::nullopt;
	}
	return static_cast<std::uint8_t>(payload[1]);
}

std::optional<DrivingInputPacket> deserializeDrivingInput(const std::string& payload) {
	if (payload.size() != 6 || static_cast<unsigned char>(payload[0]) != 1
		|| (static_cast<unsigned char>(payload[5]) & 0xf0u) != 0) {
		std::cerr << "[OnlineMatch] Invalid driving input packet" << std::endl;
		return std::nullopt;
	}
	DrivingInputPacket packet;
	for (int i = 0; i < 4; ++i) {
		packet.sequence |= static_cast<std::uint32_t>(static_cast<unsigned char>(payload[1 + i])) << (i * 8);
	}
	const unsigned char flags = static_cast<unsigned char>(payload[5]);
	packet.input = { (flags & 1) != 0, (flags & 2) != 0, (flags & 4) != 0, (flags & 8) != 0 };
	return packet;
}

bool RemoteDrivingInput::accept(const DrivingInputPacket& packet) {
	const std::uint32_t difference = packet.sequence - sequence_;
	if (received_ && (difference == 0 || difference >= 0x80000000u)) return false;
	sequence_ = packet.sequence;
	input_ = packet.input;
	elapsed_ = 0.0f;
	received_ = true;
	return true;
}

void RemoteDrivingInput::update(float elapsedSeconds) {
	if (!received_ || elapsedSeconds <= 0.0f) return;
	elapsed_ += elapsedSeconds;
	if (elapsed_ >= TIMEOUT_SECONDS) input_ = {};
}

void RemoteDrivingInput::reset() {
	input_ = {};
	sequence_ = 0;
	elapsed_ = 0.0f;
	received_ = false;
}

bool HostDrivingInputs::addPlayer(std::uint64_t playerId) {
	if (playerId == 0) return false;
	if (players_.find(playerId) != players_.end()) return true;
	if (players_.size() >= MAX_REMOTE_PLAYERS) return false;
	players_.emplace(playerId, RemoteDrivingInput{});
	return true;
}

void HostDrivingInputs::removePlayer(std::uint64_t playerId) {
	players_.erase(playerId);
}

void HostDrivingInputs::retainPlayers(const std::vector<std::uint64_t>& playerIds) {
	for (auto it = players_.begin(); it != players_.end();) {
		if (std::find(playerIds.begin(), playerIds.end(), it->first) == playerIds.end()) {
			it = players_.erase(it);
		}
		else {
			++it;
		}
	}
}

bool HostDrivingInputs::accept(std::uint64_t senderId, const DrivingInputPacket& packet) {
	auto player = players_.find(senderId);
	return player != players_.end() && player->second.accept(packet);
}

std::optional<DrivingInput> HostDrivingInputs::input(std::uint64_t playerId) const {
	const auto player = players_.find(playerId);
	if (player == players_.end()) return std::nullopt;
	return player->second.input();
}

void HostDrivingInputs::update(float elapsedSeconds) {
	for (auto& player : players_) player.second.update(elapsedSeconds);
}

void HostDrivingInputs::clear() {
	players_.clear();
}

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
