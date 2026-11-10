#include "lightcyclesGame.h"

#include "level/levelLoader.h"
#include "level/spawnPlanner.h"
#include "network/steamSession.h"
#include "randomNumber.h"
#include "res_path.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>

namespace {
constexpr SDL_Color PLAYER_GLOW = { 0, 90, 135, 255 };
constexpr SDL_Color PLAYER_CORE = { 20, 235, 255, 255 };
}

LightcyclesGame::LightcyclesGame(SDL_Renderer* renderer, SteamSessionService* steamSession)
	: renderer_(renderer),
	steamSession_(steamSession),
	resPath_(getResourcePath()),
	cycleSkins_(renderer, resPath_),
	player_(PLAYER_GLOW, PLAYER_CORE, cycleSkins_.forColor(PLAYER_CORE), CELL_SIZE),
	objects_(CELL_SIZE) {
	renderContext_.renderer = renderer_;
	renderContext_.camera = { 0, 0, CAMERA_WIDTH, CAMERA_HEIGHT };

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
	worldTexture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_TARGET, CAMERA_WIDTH, CAMERA_HEIGHT);
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

	cameraController_.isLerping = false;
	cameraController_.target = &player_;

	hudUi_.setRenderer(renderer_);
	hudUi_.setResourcePath(resPath_);
	objects_.setRenderer(renderer_);

	levelCount_ = countLevels();
	inMenu_ = true;
	exitRequested_ = false;
}

LightcyclesGame::~LightcyclesGame() {
	if (worldTexture_ != nullptr) {
		SDL_DestroyTexture(worldTexture_);
	}
}

std::vector<std::string> LightcyclesGame::getAvailableLevelNames() const {
	std::vector<std::string> names;
	for (std::size_t i = 0; i < levelCount_; ++i) {
		std::optional<LevelDefinition> def = LevelLoader::load(levelPath(i), CELL_SIZE);
		if (def && isPlayable(*def)) {
			names.push_back(def->name);
		}
		else {
			names.push_back("Level " + std::to_string(i + 1));
		}
	}
	if (names.empty()) {
		names.push_back("Default Arena");
	}
	return names;
}

bool LightcyclesGame::startMatch(const GameConfig& config) {
	activeConfig_ = config;
	onlineMatch_.reset();
	remoteInputs_.clear();
	inputSequence_ = 0;
	sessionHostId_ = 0;
	inputSendTimer_ = 0.0f;
	readySendTimer_ = 0.0f;
	timeWithoutFrame_ = 0.0f;
	readyPlayers_.clear();
	snapshotSendTimer_ = 0.0f;
	snapshotSendSequence_ = 0;
	frameAssembler_.reset();
	receivedFrameSequence_.reset();
	visualCycles_.clear();
	visualMotions_.clear();
	visualLocalId_ = 0;
	visualInterpolationTime_ = 0.0f;
	receivedFrame_ = false;
	matchStarted_ = false;
	if (config.onlineMode) {
		if (steamSession_ == nullptr || !steamSession_->lobbyInfo().available
			|| steamSession_->hostSteamId() == 0 || steamSession_->localSteamId() == 0) {
			std::cerr << "[Game] Cannot start without an active Steam lobby" << std::endl;
			return false;
		}
		sessionHostId_ = steamSession_->hostSteamId();
		onlineMatch_.beginSession(steamSession_->isHost(), 1,
			std::max(0, config.totalPlayers - 1));
	}

	std::size_t targetLevel = config.levelIndex;
	if (targetLevel >= levelCount_) {
		targetLevel = 0;
	}

	if (!loadLevel(targetLevel)) {
		std::cerr << "Failed to start match with level index " << targetLevel << std::endl;
		return false;
	}

	// Apply configured player color
	player_ = Cycle(config.playerGlow, config.playerCore, cycleSkins_.forColor(config.playerCore), CELL_SIZE);
	cameraController_.target = &player_;

	// Respect the configured number of AI opponents exactly. A zero-bot match must stay bot-free.
	const int desiredAiCount = std::max(0, config.aiCount);
	while (static_cast<int>(enemies_.size()) > desiredAiCount) {
		enemies_.pop_back();
		if (!brains_.empty()) {
			brains_.pop_back();
		}
	}
	if (static_cast<int>(enemies_.size()) < desiredAiCount) {
		const EnemyPersonality extraPersonalities[] = {
			EnemyPersonality::Aggressor,
			EnemyPersonality::Ambusher,
			EnemyPersonality::Strategist,
			EnemyPersonality::Coward,
			EnemyPersonality::Erratic
		};
		const SDL_Color extraColors[][2] = {
			{ { 110, 95, 5, 255 },  { 255, 235, 50, 255 } },
			{ { 120, 20, 90, 255 }, { 255, 90, 200, 255 } },
			{ { 20, 110, 90, 255 }, { 50, 255, 220, 255 } }
		};
		int extraIdx = 0;
		while (static_cast<int>(enemies_.size()) < desiredAiCount) {
			const auto& c = extraColors[extraIdx % 3];
			const EnemyPersonality p = extraPersonalities[extraIdx % 5];
			enemies_.emplace_back(c[0], c[1], cycleSkins_.forColor(c[1]), CELL_SIZE);
			brains_.push_back(createEnemyBrain(p));
			++extraIdx;
		}
	}

	inMenu_ = false;
	reset();
	return true;
}

void LightcyclesGame::returnToMenu() {
	inMenu_ = true;
	accelerating_ = braking_ = steeringLeft_ = steeringRight_ = false;
	if (onlineMatch_.enabled && steamSession_ != nullptr) steamSession_->leaveLobby();
	remoteInputs_.clear();
}

std::string LightcyclesGame::levelPath(std::size_t index) const {
	return resPath_ + LEVELS_FOLDER + LEVEL_FILES[index];
}

std::size_t LightcyclesGame::countLevels() const {
	std::size_t count = 0;
	while (count < LEVEL_FILES.size() && std::ifstream(levelPath(count)).good()) {
		++count;
	}
	return count;
}

bool LightcyclesGame::isPlayable(const LevelDefinition& level) const {
	if (level.width < MIN_GRID_WIDTH || level.height < MIN_GRID_HEIGHT) {
		std::cerr << "Level " << level.name << " is smaller than the window ("
			<< MIN_GRID_WIDTH << "x" << MIN_GRID_HEIGHT << " cells minimum)" << std::endl;
		return false;
	}
	if (level.enemies.empty()) {
		std::cerr << "Level " << level.name << " has no enemies" << std::endl;
		return false;
	}

	std::vector<SpawnPoint> spawns = { level.player };
	for (const EnemyDefinition& enemy : level.enemies) {
		spawns.push_back(enemy.spawn);
	}
	for (const SpawnPoint& spawn : spawns) {
		if (spawn.cell && (spawn.cell->x < 1 || spawn.cell->x >= level.width - 1
			|| spawn.cell->y < 1 || spawn.cell->y >= level.height - 1)) {
			std::cerr << "Level " << level.name << " has a spawn outside the arena" << std::endl;
			return false;
		}
	}
	return true;
}

bool LightcyclesGame::loadLevel(std::size_t index) {
	std::optional<LevelDefinition> loaded = LevelLoader::load(levelPath(index), CELL_SIZE);
	if (!loaded || !isPlayable(*loaded)) {
		return false;
	}

	level_ = std::move(*loaded);
	levelIndex_ = index;
	tileMap_.load(level_, renderer_, resPath_ + TEXTURES_FOLDER);
	enemies_.clear();
	brains_.clear();
	for (const EnemyDefinition& definition : level_.enemies) {
		enemies_.emplace_back(definition.glow, definition.core, cycleSkins_.forColor(definition.core), CELL_SIZE);
		brains_.push_back(createEnemyBrain(definition.personality));
	}
	reset();
	return true;
}

bool LightcyclesGame::isRunning() const {
	return running_;
}

void LightcyclesGame::handleEvent(SDL_Event& event) {
	if (event.type == SDL_QUIT) {
		running_ = false;
		exitRequested_ = true;
		return;
	}
	if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
		accelerating_ = braking_ = steeringLeft_ = steeringRight_ = false;
	}
	if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
		const bool pressed = event.type == SDL_KEYDOWN;
		switch (event.key.keysym.scancode) {
		case SDL_SCANCODE_W: accelerating_ = pressed; break;
		case SDL_SCANCODE_S: braking_ = pressed; break;
		case SDL_SCANCODE_A: steeringLeft_ = pressed; break;
		case SDL_SCANCODE_D: steeringRight_ = pressed; break;
		default: break;
		}
	}

	if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
		if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
			returnToMenu();
			return;
		}
		if (event.key.keysym.scancode == SDL_SCANCODE_RETURN
			&& result_ != GameResult::Running) {
			if (onlineMatch_.enabled) {
				returnToMenu();
				return;
			}
			if (result_ == GameResult::PlayerWon) {
				const int savedScore = player_.score();
				if (loadLevel((levelIndex_ + 1) % levelCount_)) {
					player_.setScore(savedScore);
				}
				else {
					reset();
				}
			}
			else {
				player_.setScore(0);
				reset();
			}
			return;
		}
	}

}

void LightcyclesGame::update(float elapsedSeconds) {
	if (!onlineMatch_.enabled && result_ != GameResult::Running) return;
	if (onlineMatch_.enabled) {
		updateOnlineInputs(elapsedSeconds);
		if (inMenu_) return;
		if (onlineMatch_.host) {
			if (!matchStarted_) {
				const auto connected = std::count_if(remoteCycles_.begin(), remoteCycles_.end(),
					[](const auto& entry) { return entry.second.connected; });
				if (connected + 1 >= std::max(1, activeConfig_.totalPlayers)) {
					if (!steamSession_->setLobbyJoinable(false)) {
						returnToMenu();
						return;
					}
					matchStarted_ = true;
				}
			}
			if (matchStarted_ && result_ == GameResult::Running) updateCycles(std::min(elapsedSeconds, 0.25f));
			sendOnlineFrame(elapsedSeconds);
		}
		else {
			updateVisualInterpolation(elapsedSeconds);
		}
	}
	else if (result_ == GameResult::Running) {
		updateCycles(std::min(elapsedSeconds, 0.25f));
	}

	cameraController_.deltaTime = std::min(elapsedSeconds, 0.25f);
	cameraController_.update(renderContext_.camera, level_.width * CELL_SIZE, level_.height * CELL_SIZE);
}

void LightcyclesGame::updateOnlineInputs(float elapsedSeconds) {
	if (steamSession_ == nullptr || !steamSession_->lobbyInfo().available) {
		std::cerr << "[Game] Steam lobby is unavailable; returning to menu" << std::endl;
		returnToMenu();
		return;
	}
	const std::uint64_t owner = steamSession_->hostSteamId();
	if (owner == 0 || owner != sessionHostId_
		|| (owner == steamSession_->localSteamId()) != onlineMatch_.host) {
		std::cerr << "[Game] Lobby host changed or is unavailable; returning to menu" << std::endl;
		returnToMenu();
		return;
	}
	remoteInputs_.update(elapsedSeconds);
	if (steamSession_->isHost()) {
		if (!steamSession_->synchronizeDrivingPlayers(remoteInputs_)) return;
		const auto members = steamSession_->lobbyMemberIds();
		for (auto it = readyPlayers_.begin(); it != readyPlayers_.end();) {
			if (std::find(members.begin(), members.end(), it->first) == members.end()) it = readyPlayers_.erase(it);
			else ++it;
		}
		for (const SteamPeerMessage& message : steamSession_->receivePeerMessages()) {
			if (message.type == SteamMessageType::Input) {
				steamSession_->acceptDrivingInput(message, remoteInputs_);
			}
			else if (message.type == SteamMessageType::Ready && message.senderId != owner) {
				const auto color = deserializePlayerReady(message.payload);
				const auto existing = readyPlayers_.find(message.senderId);
				if (color && ((existing == readyPlayers_.end() && !matchStarted_)
					|| (existing != readyPlayers_.end() && existing->second == *color))) {
					readyPlayers_[message.senderId] = *color;
				}
				else if (color) std::cerr << "[Game] Rejected late or changed player readiness" << std::endl;
			}
			else {
				std::cerr << "[Game] Unexpected message received by host" << std::endl;
			}
		}
		synchronizeRemoteCycles(members, elapsedSeconds);
	}
	else {
		timeWithoutFrame_ += std::max(0.0f, elapsedSeconds);
		for (const SteamPeerMessage& message : steamSession_->receivePeerMessages()) {
			if (message.senderId != owner || message.type != SteamMessageType::Snapshot) {
				std::cerr << "[Game] Rejected message from non-host or unexpected type" << std::endl;
				continue;
			}
			const auto frame = frameAssembler_.accept(message.payload);
			if (frame && applyOnlineFrame(*frame, steamSession_->localSteamId(), owner, steamSession_->lobbyMemberIds())) {
				timeWithoutFrame_ = 0.0f;
			}
		}
		if (timeWithoutFrame_ >= 15.0f) {
			std::cerr << "[Game] No valid authoritative frame for 15 seconds; returning to menu" << std::endl;
			returnToMenu();
			return;
		}
		readySendTimer_ -= elapsedSeconds;
		if (!matchStarted_ && readySendTimer_ <= 0.0f) {
			readySendTimer_ = 0.5f;
			if (activeConfig_.colorIndex < 0 || static_cast<std::size_t>(activeConfig_.colorIndex) >= NUM_PLAYER_COLOR_OPTIONS) {
				std::cerr << "[Game] Invalid player color selection" << std::endl;
				returnToMenu();
				return;
			}
			steamSession_->sendPeerMessage(owner, SteamMessageType::Ready,
				serializePlayerReady(static_cast<std::uint8_t>(activeConfig_.colorIndex)));
		}
		inputSendTimer_ -= elapsedSeconds;
		if (inputSendTimer_ <= 0.0f) {
			inputSendTimer_ = 1.0f / 30.0f;
			const DrivingInputPacket packet{ ++inputSequence_,
				{ accelerating_, braking_, steeringLeft_, steeringRight_ } };
			steamSession_->sendDrivingInput(steamSession_->hostSteamId(), packet);
		}
	}
}

void LightcyclesGame::sendOnlineFrame(float elapsedSeconds) {
	snapshotSendTimer_ -= elapsedSeconds;
	if (snapshotSendTimer_ > 0.0f) return;
	snapshotSendTimer_ = 0.1f;
	const std::uint64_t localId = steamSession_->localSteamId();
	const auto members = steamSession_->lobbyMemberIds();
	if (members.size() < 2) return;
	const std::uint32_t sequence = ++snapshotSendSequence_;
	OnlineFrame frame;
	frame.levelIndex = static_cast<std::uint16_t>(levelIndex_);
	frame.phase = !matchStarted_ ? OnlinePhase::Waiting
		: result_ == GameResult::Running ? OnlinePhase::Running : OnlinePhase::Finished;
	frame.cycles = captureCycleSnapshot(localId, sequence);
	frame.world = objects_.snapshot(sequence);
	const auto parts = serializeOnlineFrame(frame);
	if (!parts) {
		std::cerr << "[Game] Failed to encode authoritative frame" << std::endl;
		return;
	}
	for (const std::uint64_t id : members) {
		if (id == localId) continue;
		if (!steamSession_->sendPeerMessage(id, SteamMessageType::Snapshot, (*parts)[0])) continue;
		steamSession_->sendPeerMessage(id, SteamMessageType::Snapshot, (*parts)[1]);
	}
}

bool LightcyclesGame::applyOnlineFrame(const OnlineFrame& frame, std::uint64_t localId, std::uint64_t hostId,
	const std::vector<std::uint64_t>& members) {
	const auto reject = []() {
		std::cerr << "[Game] Rejected incompatible authoritative frame" << std::endl;
		return false;
	};
	if (localId == 0 || hostId == 0 || localId == hostId || frame.levelIndex != levelIndex_
		|| frame.cycles.sequence != frame.world.sequence || !validateCycleSnapshot(frame.cycles)
		|| static_cast<unsigned int>(frame.phase) > static_cast<unsigned int>(OnlinePhase::Finished)
		|| (matchStarted_ && frame.phase == OnlinePhase::Waiting)
		|| (result_ != GameResult::Running && frame.phase != OnlinePhase::Finished)) return reject();
	SnapshotSequence nextSequence = receivedFrameSequence_;
	if (!nextSequence.accept(frame.cycles.sequence)) return reject();
	std::unique_ptr<Cycle> local;
	std::map<std::pair<SnapshotCycleKind, std::uint64_t>, std::unique_ptr<Cycle>> others;
	std::map<std::pair<SnapshotCycleKind, std::uint64_t>, VisualMotion> motions;
	bool hasHost = false;
	int aliveCount = 0;
	for (const SnapshotCycle& state : frame.cycles.cycles) {
		if (state.kind == SnapshotCycleKind::Human) {
			if (std::find(members.begin(), members.end(), state.id) == members.end()) return reject();
			if (state.id == hostId) hasHost = true;
		}
		else if (state.id > CycleSnapshot::MAX_BOTS) return reject();
		const SDL_Color glow{ state.glow[0], state.glow[1], state.glow[2], state.glow[3] };
		const SDL_Color core{ state.core[0], state.core[1], state.core[2], state.core[3] };
		auto cycle = std::make_unique<Cycle>(glow, core, cycleSkins_.forColor(core), CELL_SIZE);
		if (!cycle->applyVisualSnapshot(state)) return reject();
		const auto key = std::make_pair(state.kind, state.id);
		const Cycle* previous = nullptr;
		if (receivedFrame_) {
			if (state.kind == SnapshotCycleKind::Human && state.id == localId) previous = &player_;
			else {
				const auto found = visualCycles_.find(key);
				if (found != visualCycles_.end()) previous = found->second.get();
			}
		}
		if (previous != nullptr && previous->alive() && cycle->alive()
			&& !previous->isRespawning() && !cycle->isRespawning()
			&& geom::distanceSq(previous->center(), cycle->center()) < 128.0f * 128.0f) {
			const VisualMotion motion{ previous->center(), cycle->center(), previous->headingDegrees(),
				std::remainder(cycle->headingDegrees() - previous->headingDegrees(), 360.0f) };
			motions.emplace(key, motion);
			cycle->setVisualPose(motion.from, motion.fromHeading);
		}
		if (!cycle->isEliminated()) ++aliveCount;
		if (state.kind == SnapshotCycleKind::Human && state.id == localId) local = std::move(cycle);
		else others.emplace(key, std::move(cycle));
	}
	if (!local || !hasHost || (frame.phase == OnlinePhase::Finished && aliveCount > 1)
		|| !objects_.applySnapshot(frame.world)) return reject();
	player_ = std::move(*local);
	visualCycles_ = std::move(others);
	visualMotions_ = std::move(motions);
	visualLocalId_ = localId;
	visualInterpolationTime_ = 0.0f;
	receivedFrameSequence_ = nextSequence;
	receivedFrame_ = true;
	matchStarted_ = frame.phase != OnlinePhase::Waiting;
	if (frame.phase == OnlinePhase::Finished) {
		result_ = aliveCount == 0 ? GameResult::Draw
			: player_.isEliminated() ? GameResult::PlayerLost : GameResult::PlayerWon;
	}
	else {
		result_ = GameResult::Running;
	}
	return true;
}

void LightcyclesGame::updateVisualInterpolation(float elapsedSeconds) {
	constexpr float SNAPSHOT_INTERVAL = 0.1f;
	visualInterpolationTime_ = std::min(SNAPSHOT_INTERVAL, visualInterpolationTime_ + std::max(0.0f, elapsedSeconds));
	const float alpha = visualInterpolationTime_ / SNAPSHOT_INTERVAL;
	for (const auto& entry : visualMotions_) {
		Cycle* cycle = nullptr;
		if (entry.first.first == SnapshotCycleKind::Human && entry.first.second == visualLocalId_) cycle = &player_;
		else {
			const auto found = visualCycles_.find(entry.first);
			if (found != visualCycles_.end()) cycle = found->second.get();
		}
		if (cycle == nullptr) continue;
		const VisualMotion& motion = entry.second;
		cycle->setVisualPose(motion.from + (motion.to - motion.from) * alpha,
			motion.fromHeading + motion.headingDelta * alpha);
	}
}

CycleSnapshot LightcyclesGame::captureCycleSnapshot(std::uint64_t hostId, std::uint32_t sequence) const {
	CycleSnapshot snapshot;
	snapshot.sequence = sequence;
	snapshot.cycles.push_back(player_.snapshot(SnapshotCycleKind::Human, hostId));
	for (const auto& remote : remoteCycles_) {
		if (remote.second.connected) {
			snapshot.cycles.push_back(remote.second.cycle->snapshot(SnapshotCycleKind::Human, remote.first));
		}
	}
	for (std::size_t i = 0; i < enemies_.size(); ++i) {
		snapshot.cycles.push_back(enemies_[i].snapshot(SnapshotCycleKind::Bot, i + 1));
	}
	return snapshot;
}

bool LightcyclesGame::spawnRemoteCycle(Cycle& cycle) {
	for (int attempt = 0; attempt < 20; ++attempt) {
		SDL_Point cell;
		Direction direction;
		if (!findSafeRespawn(cell, direction, false)) break;
		const Vec2 position = { static_cast<float>(cell.x * CELL_SIZE + CELL_SIZE / 2),
			static_cast<float>(cell.y * CELL_SIZE + CELL_SIZE / 2) };
		const auto tooClose = [&](const Cycle& other) {
			return &other != &cycle && other.alive() && !other.isRespawning()
				&& geom::distanceSq(position, other.center()) < 80.0f * 80.0f;
		};
		if (tooClose(player_) || std::any_of(enemies_.begin(), enemies_.end(), tooClose)) continue;
		bool occupied = false;
		for (const auto& remote : remoteCycles_) {
			if (remote.second.connected && tooClose(*remote.second.cycle)) {
				occupied = true;
				break;
			}
		}
		if (occupied) continue;
		cycle.reset(cell, direction);
		return true;
	}
	std::cerr << "[Game] Could not find a safe spawn for a remote player" << std::endl;
	return false;
}

void LightcyclesGame::synchronizeRemoteCycles(const std::vector<std::uint64_t>& members, float elapsedSeconds) {
	const std::uint64_t localId = steamSession_->localSteamId();
	for (auto& remote : remoteCycles_) {
		if (std::find(members.begin(), members.end(), remote.first) == members.end()) {
			remote.second.connected = false;
			remote.second.retired = true;
			remote.second.cycle->crash();
			remote.second.cycle->clearTrail();
		}
	}
	for (const std::uint64_t id : members) {
		if (id == 0 || id == localId) continue;
		const auto ready = readyPlayers_.find(id);
		if (ready == readyPlayers_.end()) continue;
		auto found = remoteCycles_.find(id);
		if (found == remoteCycles_.end()) {
			const auto& color = PLAYER_COLOR_OPTIONS[ready->second];
			RemoteCycle remote;
			remote.cycle = std::make_unique<Cycle>(color.glow, color.core, cycleSkins_.forColor(color.core), CELL_SIZE);
			found = remoteCycles_.emplace(id, std::move(remote)).first;
		}
		RemoteCycle& remote = found->second;
		remote.retired = false;
		if (!remote.connected) {
			remote.spawnRetrySeconds -= elapsedSeconds;
			if (remote.spawnRetrySeconds <= 0.0f) {
				remote.connected = spawnRemoteCycle(*remote.cycle);
				remote.spawnRetrySeconds = 1.0f;
			}
		}
	}
}

void LightcyclesGame::render() {
	SDL_SetRenderDrawColor(renderer_, 0, 0, 0, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(renderer_);

	const bool useTexture = worldTexture_ != nullptr
		&& SDL_SetRenderTarget(renderer_, worldTexture_) == 0;
	// Without the texture, fall back to drawing directly below the HUD.
	const int offsetY = useTexture ? 0 : HUD_HEIGHT;
	if (useTexture) {
		SDL_SetRenderDrawColor(renderer_, 0, 0, 0, SDL_ALPHA_OPAQUE);
		SDL_RenderClear(renderer_);
	}

	RenderContext drawContext = renderContext_;
	drawContext.camera.y = renderContext_.camera.y - offsetY;

	const SDL_Rect& camera = renderContext_.camera;
	const SDL_Rect arena = { -camera.x, offsetY - camera.y, level_.width * CELL_SIZE, level_.height * CELL_SIZE };
	SDL_SetRenderDrawColor(renderer_, 22, 32, 45, SDL_ALPHA_OPAQUE);
	SDL_RenderDrawRect(renderer_, &arena);

	// Layer 0: ground. Layer 1: walls, objects, trails and cycles sorted by Y. Layer 2: projectiles.
	tileMap_.drawLayer(drawContext, RenderLayer::Ground);

	tileMap_.queueLayer(drawContext, RenderLayer::Objects, depthQueue_);
	objects_.queueDraw(drawContext, depthQueue_);

	// Each trail segment is queued below its cycle, sorted by its own Y.
	const auto queueCycle = [&](Cycle& cycle) {
		const float sortY = cycle.center().y;
		depthQueue_.add(sortY, [&cycle, &drawContext]() { cycle.draw(drawContext); });
		cycle.trail().queueDraw(depthQueue_, renderer_, renderContext_.camera, offsetY, cycle.glowColor(),
			cycle.coreColor(), sortY);
	};
	if (onlineMatch_.enabled && !onlineMatch_.host) {
		for (auto& remote : visualCycles_) queueCycle(*remote.second);
		if (receivedFrame_) queueCycle(player_);
	}
	else {
		for (Cycle& enemy : enemies_) queueCycle(enemy);
		for (auto& remote : remoteCycles_) {
			if (remote.second.connected) queueCycle(*remote.second.cycle);
		}
		queueCycle(player_);
	}
	depthQueue_.flush();

	objects_.drawProjectiles(drawContext);
	if (useTexture) {
		SDL_SetRenderTarget(renderer_, nullptr);
		const SDL_Rect gameArea = { 0, HUD_HEIGHT, WINDOW_WIDTH, VIEWPORT_HEIGHT };
		SDL_RenderCopy(renderer_, worldTexture_, nullptr, &gameArea);
	}

	HudState hudState;
	hudState.score = player_.score();
	hudState.lives = player_.lives();
	hudState.maxLives = Cycle::INITIAL_LIVES;
	hudState.totalEnemies = static_cast<int>(enemies_.size());
	hudState.enemiesAlive = static_cast<int>(std::count_if(enemies_.begin(), enemies_.end(),
		[](const Cycle& enemy) { return !enemy.isEliminated(); }));
	if (onlineMatch_.enabled && !onlineMatch_.host) {
		hudState.totalEnemies = static_cast<int>(visualCycles_.size());
		hudState.enemiesAlive = static_cast<int>(std::count_if(visualCycles_.begin(), visualCycles_.end(),
			[](const auto& cycle) { return !cycle.second->isEliminated(); }));
	}
	else if (onlineMatch_.enabled) {
		for (const auto& remote : remoteCycles_) {
			if (remote.second.connected) {
				++hudState.totalEnemies;
				if (!remote.second.cycle->isEliminated()) ++hudState.enemiesAlive;
			}
		}
	}
	hudState.levelName = level_.name;
	hudState.isRespawning = player_.isRespawning();
	hudState.hasBoost = player_.effects().isActive(EffectType::Boost);
	hudState.hasLongTrail = player_.effects().isActive(EffectType::LongTrail);
	hudState.hasSlow = player_.effects().isActive(EffectType::Slow);
	hudState.online = onlineMatch_.enabled;
	hudState.waiting = onlineMatch_.enabled && !matchStarted_;

	switch (result_) {
	case GameResult::PlayerWon: hudState.status = HudGameStatus::PlayerWon; break;
	case GameResult::PlayerLost: hudState.status = HudGameStatus::PlayerLost; break;
	case GameResult::Draw: hudState.status = HudGameStatus::Draw; break;
	case GameResult::Running: hudState.status = HudGameStatus::Running; break;
	}

	hudUi_.updateState(hudState);
	hudUi_.draw();

	SDL_Rect statusStrip = { 0, HUD_HEIGHT - 2, WINDOW_WIDTH, 2 };
	switch (result_) {
	case GameResult::PlayerWon:
		SDL_SetRenderDrawColor(renderer_, 20, 255, 170, SDL_ALPHA_OPAQUE);
		break;
	case GameResult::PlayerLost:
		SDL_SetRenderDrawColor(renderer_, 255, 40, 75, SDL_ALPHA_OPAQUE);
		break;
	case GameResult::Draw:
		SDL_SetRenderDrawColor(renderer_, 255, 190, 45, SDL_ALPHA_OPAQUE);
		break;
	case GameResult::Running:
		SDL_SetRenderDrawColor(renderer_, 0, 140, 180, SDL_ALPHA_OPAQUE);
		break;
	}
	SDL_RenderFillRect(renderer_, &statusStrip);

	SDL_RenderPresent(renderer_);
}

ArenaView LightcyclesGame::buildArenaView() const {
	ArenaView arena{
		static_cast<float>(level_.width * CELL_SIZE),
		static_cast<float>(level_.height * CELL_SIZE),
		objects_.getWallBoxes(),
		objects_.getMinePositions(),
		{},
		{},
		objects_.pickups(),
		&objects_.wallIndex()
	};

	if (player_.alive() && !player_.isRespawning()) {
		arena.chaseTargets.push_back(player_.center());
		auto segs = player_.trail().getSegments(&player_, false);
		arena.segments.insert(arena.segments.end(), segs.begin(), segs.end());
	}
	for (const Cycle& enemy : enemies_) {
		if (enemy.alive() && !enemy.isRespawning()) {
			arena.chaseTargets.push_back(enemy.center());
			auto segs = enemy.trail().getSegments(&enemy, false);
			arena.segments.insert(arena.segments.end(), segs.begin(), segs.end());
		}
	}
	for (const auto& remote : remoteCycles_) {
		const Cycle& cycle = *remote.second.cycle;
		if (remote.second.connected && cycle.alive() && !cycle.isRespawning()) {
			arena.chaseTargets.push_back(cycle.center());
			auto segs = cycle.trail().getSegments(&cycle, false);
			arena.segments.insert(arena.segments.end(), segs.begin(), segs.end());
		}
	}
	return arena;
}

// The cycle's nose box, swept from where it was to where it is now, is tested against the arena,
// the walls and other cycles' trails. Steps are at most 4 px, so nothing is skipped over.
// Its own trail always touches the box, so that one is tested with the head's path only.
bool LightcyclesGame::checkCycleCollision(const Cycle& cycle, Vec2 before, Vec2 after, Cycle*& outKiller) {
	outKiller = nullptr;
	const float worldW = static_cast<float>(level_.width * CELL_SIZE);
	const float worldH = static_cast<float>(level_.height * CELL_SIZE);
	const OrientedBox start = cycle.terrainBoxAt(before);
	const OrientedBox end = cycle.terrainBoxAt(after);
	const Vec2 displacement = after - before;
	const RectF world = { 0.0f, 0.0f, worldW, worldH };

	// 1. Boundary walls
	if (!geom::boxInsideRect(start, world) || !geom::boxInsideRect(end, world)) {
		return true;
	}

	// 2. Obstacle walls
	const auto& wallBoxes = objects_.getWallBoxes();
	for (const std::size_t index : objects_.wallIndex().query(sweptBounds(start, displacement))) {
		const RectF& wall = wallBoxes[index];
		if (geom::sweptBoxIntersectsBox(start, displacement, geom::orientedBox(wall, { 0.0f, 0.0f }, 0.0f))) {
			return true;
		}
	}

	// 3. Trails
	const auto hitsTrail = [&](const Cycle& owner) {
		if (!owner.alive() || owner.isRespawning()) {
			return false;
		}
		const bool isSelf = (&cycle == &owner);
		for (const LineSegment& seg : owner.trail().getSegments(&owner, isSelf, 28.0f)) {
			const bool hit = isSelf ? geom::intersectSegments(before, after, seg.a, seg.b)
				: geom::sweptBoxIntersectsSegment(start, displacement, seg.a, seg.b);
			if (hit) {
				outKiller = const_cast<Cycle*>(&owner);
				return true;
			}
		}
		return false;
	};
	if (hitsTrail(player_)) {
		return true;
	}
	for (const Cycle& enemy : enemies_) {
		if (hitsTrail(enemy)) {
			return true;
		}
	}
	for (const auto& remote : remoteCycles_) {
		if (remote.second.connected && hitsTrail(*remote.second.cycle)) return true;
	}

	return false;
}
void LightcyclesGame::updateCycles(float elapsedSeconds) {
	updateCycleRespawn(player_, elapsedSeconds);
	for (Cycle& enemy : enemies_) {
		updateCycleRespawn(enemy, elapsedSeconds);
	}
	for (auto& remote : remoteCycles_) {
		if (remote.second.connected) updateCycleRespawn(*remote.second.cycle, elapsedSeconds);
	}

	float fastest = 2.0f;
	if (player_.alive() && !player_.isRespawning()) {
		player_.effects().update(elapsedSeconds);
		fastest = std::max(fastest, player_.effects().speedMultiplier());
	}
	for (Cycle& enemy : enemies_) {
		if (enemy.alive() && !enemy.isRespawning()) {
			enemy.effects().update(elapsedSeconds);
			fastest = std::max(fastest, enemy.effects().speedMultiplier());
		}
	}
	for (auto& remote : remoteCycles_) {
		Cycle& cycle = *remote.second.cycle;
		if (remote.second.connected && cycle.alive() && !cycle.isRespawning()) {
			cycle.effects().update(elapsedSeconds);
			fastest = std::max(fastest, cycle.effects().speedMultiplier());
		}
	}
	std::vector<std::uint8_t> dummyOccupied(level_.width * level_.height, 0);
	std::vector<Cycle*> cycles = { &player_ };
	for (Cycle& enemy : enemies_) {
		cycles.push_back(&enemy);
	}
	for (auto& remote : remoteCycles_) {
		if (remote.second.connected) cycles.push_back(remote.second.cycle.get());
	}
	for (Cycle* cycle : cycles) {
		cycle->updateAnimation(elapsedSeconds);
	}
	objects_.update(elapsedSeconds, dummyOccupied, cycles);
	for (auto it = remoteCycles_.begin(); it != remoteCycles_.end();) {
		if (it->second.retired) it = remoteCycles_.erase(it);
		else ++it;
	}
	// Objects such as towers can crash a cycle outside its own movement.
	for (Cycle* cycle : cycles) {
		if (!cycle->alive()) {
			handleCycleCrash(*cycle);
		}
	}

	// Sub-steps of at most 4 pixels so no segment passes through another
	const float maxDistance = Cycle::MAX_SPEED * fastest * elapsedSeconds;
	const int steps = std::max(1, static_cast<int>(std::ceil(maxDistance / 4.0f)));

	for (int step = 0; step < steps && result_ == GameResult::Running; ++step) {
		const float stepSeconds = elapsedSeconds / steps;
		if (player_.alive() && !player_.isRespawning()) {
			moveCycle(player_, nullptr, 0, stepSeconds);
		}
		for (std::size_t index = 0; index < enemies_.size(); ++index) {
			if (enemies_[index].alive() && !enemies_[index].isRespawning()) {
				moveCycle(enemies_[index], brains_[index].get(), index, stepSeconds);
			}
		}
		for (auto& remote : remoteCycles_) {
			Cycle& cycle = *remote.second.cycle;
			if (remote.second.connected && cycle.alive() && !cycle.isRespawning()) {
				const auto input = remoteInputs_.input(remote.first);
				if (input) moveCycle(cycle, nullptr, 0, stepSeconds, &*input);
			}
		}
		crashCollidingHeads();
		updateResult();
	}
}

void LightcyclesGame::moveCycle(Cycle& cycle, EnemyBrain* brain, std::size_t enemyIndex, float elapsedSeconds,
	const DrivingInput* remoteInput) {
	if (!cycle.alive() || cycle.isRespawning()) {
		return;
	}

	if (brain != nullptr && enemyIndex < brains_.size()) {
		const ArenaView arena = buildArenaView();
		brain->drive(cycle, player_, arena, elapsedSeconds);
	}
	else if (&cycle == &player_) {
		const float steering = static_cast<float>(steeringRight_) - static_cast<float>(steeringLeft_);
		const float throttle = static_cast<float>(accelerating_) - static_cast<float>(braking_);
		cycle.steer(steering, throttle, elapsedSeconds);
	}
	else if (remoteInput != nullptr) {
		cycle.steer(remoteInput->steering(), remoteInput->throttle(), elapsedSeconds);
	}

	const Vec2 before = cycle.center();
	cycle.advance(cycle.speed() * elapsedSeconds);
	const Vec2 after = cycle.center();

	Cycle* killer = nullptr;
	if (checkCycleCollision(cycle, before, after, killer)) {
		if (killer != nullptr && killer != &cycle) {
			killer->addScore(1);
		}
		handleCycleCrash(cycle);
		return;
	}

	objects_.touch(cycle, cycle.position());
	if (!cycle.alive()) {
		handleCycleCrash(cycle);
		return;
	}
}

void LightcyclesGame::handleCycleCrash(Cycle& cycle) {
	if (cycle.isRespawning() || cycle.isEliminated()) {
		return;
	}
	cycle.crash();
	cycle.loseLife();
	cycle.clearTrail();

	if (cycle.lives() > 0) {
		cycle.startRespawn(1.5f);
	}
}

void LightcyclesGame::updateCycleRespawn(Cycle& cycle, float elapsedSeconds) {
	if (!cycle.isRespawning()) {
		return;
	}
	cycle.updateRespawn(elapsedSeconds);
	if (!cycle.isRespawning() && cycle.lives() > 0) {
		respawnCycle(cycle);
	}
}

bool LightcyclesGame::findSafeRespawn(SDL_Point& outCell, Direction& outDir, bool allowFallback) {
	const ArenaView arena = buildArenaView();

	for (int attempt = 0; attempt < 100; ++attempt) {
		const SDL_Point candidate = {
			10 + getRandomNumber(level_.width - 20),
			10 + getRandomNumber(level_.height - 20)
		};
		const Vec2 candPos = {
			static_cast<float>(candidate.x * CELL_SIZE + CELL_SIZE / 2),
			static_cast<float>(candidate.y * CELL_SIZE + CELL_SIZE / 2)
		};

		// The facing is not known yet, so keep a margin of three cells around the spawn point clear of walls
		const float margin = static_cast<float>(CELL_SIZE * 3);
		const RectF spawnArea = { candPos.x - margin, candPos.y - margin, margin * 2.0f, margin * 2.0f };
		bool insideWall = false;
		for (const RectF& wall : arena.walls) {
			if (wall.intersects(spawnArea)) {
				insideWall = true;
				break;
			}
		}
		if (insideWall) continue;

		for (int d = 0; d < 4; ++d) {
			const Direction dir = static_cast<Direction>(d);
			const Vec2 dirVec = EnemyBrain::directionVector(dir);
			const float clearance = arena.evaluateClearance(candPos, dirVec, 80.0f, nullptr);
			if (clearance >= 60.0f) {
				outCell = candidate;
				outDir = dir;
				return true;
			}
		}
	}

	if (!allowFallback) return false;
	outCell = { level_.width / 2, level_.height / 2 };
	outDir = Direction::East;
	return true;
}

void LightcyclesGame::respawnCycle(Cycle& cycle) {
	SDL_Point cell;
	Direction dir;
	findSafeRespawn(cell, dir);
	cycle.respawn(cell, dir);
	for (std::size_t i = 0; i < enemies_.size(); ++i) {
		if (&cycle == &enemies_[i]) {
			brains_[i]->resetDriving();
			break;
		}
	}
	if (&cycle == &player_) {
		renderContext_.camera.x = std::clamp(static_cast<int>(player_.x) - WINDOW_WIDTH / 2,
			0, level_.width * CELL_SIZE - WINDOW_WIDTH);
		renderContext_.camera.y = std::clamp(static_cast<int>(player_.y) - VIEWPORT_HEIGHT / 2,
			0, level_.height * CELL_SIZE - VIEWPORT_HEIGHT);
	}
}

void LightcyclesGame::crashCollidingHeads() {
	std::vector<Cycle*> alive;
	if (player_.alive() && !player_.isRespawning()) {
		alive.push_back(&player_);
	}
	for (Cycle& enemy : enemies_) {
		if (enemy.alive() && !enemy.isRespawning()) {
			alive.push_back(&enemy);
		}
	}
	for (auto& remote : remoteCycles_) {
		Cycle& cycle = *remote.second.cycle;
		if (remote.second.connected && cycle.alive() && !cycle.isRespawning()) alive.push_back(&cycle);
	}

	for (std::size_t first = 0; first < alive.size(); ++first) {
		for (std::size_t second = first + 1; second < alive.size(); ++second) {
			if (alive[first]->overlaps(*alive[second])) {
				handleCycleCrash(*alive[first]);
				handleCycleCrash(*alive[second]);
			}
		}
	}
}

void LightcyclesGame::updateResult() {
	if (result_ != GameResult::Running) {
		return;
	}

	if (onlineMatch_.enabled) {
		int participants = 1 + static_cast<int>(enemies_.size());
		int survivors = player_.isEliminated() ? 0 : 1;
		for (const Cycle& enemy : enemies_) {
			if (!enemy.isEliminated()) ++survivors;
		}
		for (const auto& remote : remoteCycles_) {
			if (remote.second.connected) {
				++participants;
				if (!remote.second.cycle->isEliminated()) ++survivors;
			}
		}
		if ((participants > 1 || (matchStarted_ && activeConfig_.totalPlayers > 1)) && survivors <= 1) {
			result_ = survivors == 0 ? GameResult::Draw
				: player_.isEliminated() ? GameResult::PlayerLost : GameResult::PlayerWon;
		}
		return;
	}

	const bool playerEliminated = player_.isEliminated();
	const bool allEnemiesEliminated = std::all_of(enemies_.begin(), enemies_.end(),
		[](const Cycle& enemy) { return enemy.isEliminated(); });

	if (playerEliminated && allEnemiesEliminated) {
		result_ = GameResult::Draw;
	}
	else if (playerEliminated) {
		result_ = GameResult::PlayerLost;
	}
	else if (allEnemiesEliminated) {
		result_ = GameResult::PlayerWon;
	}
}

void LightcyclesGame::reset() {
	const std::vector<ResolvedSpawn> spawns = resolveSpawns(level_);
	std::vector<SDL_Point> startCells;
	for (const ResolvedSpawn& spawn : spawns) {
		startCells.push_back(spawn.cell);
	}
	player_.reset(spawns[0].cell, spawns[0].direction);
	for (std::size_t index = 0; index < enemies_.size(); ++index) {
		enemies_[index].reset(spawns[index + 1].cell, spawns[index + 1].direction);
		brains_[index]->resetDriving();
	}

	std::vector<std::uint8_t> dummyOccupied(level_.width * level_.height, 0);
	objects_.reset(level_, startCells, dummyOccupied);
	remoteCycles_.clear();
	remoteInputs_.clear();

	renderContext_.camera.x = std::clamp(static_cast<int>(player_.x) - renderContext_.camera.w / 2,
		0, level_.width * CELL_SIZE - renderContext_.camera.w);
	renderContext_.camera.y = std::clamp(static_cast<int>(player_.y) - renderContext_.camera.h / 2,
		0, level_.height * CELL_SIZE - renderContext_.camera.h);
	result_ = GameResult::Running;
}

void LightcyclesGame::setDrawColor(SDL_Color color) const {
	SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
}
