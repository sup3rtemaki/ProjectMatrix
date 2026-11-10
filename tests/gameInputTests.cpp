#define SDL_MAIN_HANDLED
#include "lightcyclesGame.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <random>
#ifdef _WIN32
#include <crtdbg.h>
#include <cstdlib>
#endif

struct LightcyclesGameInputTest {
	static void runWallIndex() {
		WallIndex index;
		index.reset(256, 256, 8);
		index.add({ 128.0f, 128.0f, 300.0f, 20.0f });
		index.add({ 512.0f, 128.0f, 8.0f, 128.0f });
		index.add({ 512.0f, 128.0f, 8.0f, 128.0f });
		std::mt19937 random(75431);
		std::uniform_real_distribution<float> position(0.0f, 2000.0f);
		std::uniform_real_distribution<float> size(1.0f, 40.0f);
		for (int i = 0; i < 750; ++i) index.add({ position(random), position(random), size(random), size(random) });

		Cycle self(SDL_Color{}, SDL_Color{}, nullptr, 8);
		Cycle other(SDL_Color{}, SDL_Color{}, nullptr, 8);
		const std::vector<PickupSighting> pickups;
		ArenaView reference{ 2048.0f, 2048.0f, index.boxes(),
			{ { 600.0f, 600.0f } },
			{ { { 100.0f, 100.0f }, { 500.0f, 100.0f }, &self },
				{ { 512.0f, 128.0f }, { 512.0f, 300.0f }, &other } }, {}, pickups };
		ArenaView accelerated = reference;
		accelerated.wallIndex = &index;
		for (const Vec2 p : { Vec2{ 512.0f, 0.0f }, Vec2{ -100.0f, 128.0f }, Vec2{ 100.0f, 90.0f } }) {
			for (const Vec2 dir : { Vec2{ 1.0f, 0.0f }, Vec2{ 0.0f, 1.0f } }) {
				for (float safe : { 0.0f, 24.0f }) {
					const RaycastHit expected = reference.raycast(p, dir, 800.0f, &self, safe);
					const RaycastHit actual = accelerated.raycast(p, dir, 800.0f, &self, safe);
					assert(actual.hit == expected.hit && actual.distance == expected.distance
						&& actual.hitCycle == expected.hitCycle);
				}
			}
		}
		std::uniform_real_distribution<float> origin(-250.0f, 2300.0f);
		std::uniform_real_distribution<float> angle(-3.14159265f, 3.14159265f);
		std::uniform_real_distribution<float> distance(0.0f, 1000.0f);
		for (int i = 0; i < 5000; ++i) {
			const Vec2 p{ origin(random), origin(random) };
			const float a = angle(random);
			const Vec2 dir{ std::cos(a), std::sin(a) };
			const float length = distance(random);
			const RaycastHit expected = reference.raycast(p, dir, length, &self);
			const RaycastHit actual = accelerated.raycast(p, dir, length, &self);
			assert(actual.hit == expected.hit && actual.distance == expected.distance
				&& actual.point.x == expected.point.x && actual.point.y == expected.point.y
				&& actual.hitCycle == expected.hitCycle);

			const OrientedBox box{ p, dir, { -dir.y, dir.x }, 12.0f, 6.0f };
			const Vec2 displacement = dir * (length / 10.0f);
			bool expectedCollision = false;
			for (const RectF& wall : index.boxes()) {
				if (geom::sweptBoxIntersectsBox(box, displacement, geom::orientedBox(wall, {}, 0.0f))) {
					expectedCollision = true;
					break;
				}
			}
			bool actualCollision = false;
			for (std::size_t wall : index.query(sweptBounds(box, displacement))) {
				if (geom::sweptBoxIntersectsBox(box, displacement, geom::orientedBox(index.boxes()[wall], {}, 0.0f))) {
					actualCollision = true;
					break;
				}
			}
			assert(actualCollision == expectedCollision);
		}
		const auto boundary = index.query({ 128.0f, 128.0f, 384.0f, 0.0f });
		assert(std::is_sorted(boundary.begin(), boundary.end()));
		assert(std::adjacent_find(boundary.begin(), boundary.end()) == boundary.end());
		assert(std::find(boundary.begin(), boundary.end(), 0) != boundary.end());
		assert(std::find(boundary.begin(), boundary.end(), 1) != boundary.end());
		assert(std::find(boundary.begin(), boundary.end(), 2) != boundary.end());
		index.reset(16, 16, 8);
		assert(index.boxes().empty() && index.query({ 0, 0, 128, 128 }).empty());
	}

	static void sendKey(LightcyclesGame& game, SDL_Scancode key, bool pressed) {
		SDL_Event event{};
		event.type = pressed ? SDL_KEYDOWN : SDL_KEYUP;
		event.key.keysym.scancode = key;
		game.handleEvent(event);
	}

	static void step(LightcyclesGame& game) {
		game.updateCycles(0.05f);
		assert(game.player_.alive());
		assert(game.result_ == GameResult::Running);
	}

	static void run(SDL_Renderer* renderer) {
		LightcyclesGame game(renderer);
		game.level_.width = 200;
		game.level_.height = 200;
		game.objects_.reset(game.level_, {}, {});
		game.player_.reset({ 100, 100 }, Direction::East);
		game.onlineMatch_.enabled = true;

		sendKey(game, SDL_SCANCODE_W, true);
		step(game);
		const float accelerated = game.player_.speed();
		assert(accelerated > Cycle::BASE_SPEED);
		sendKey(game, SDL_SCANCODE_W, false);
		step(game);
		assert(std::abs(game.player_.speed() - accelerated) < 0.001f);
		sendKey(game, SDL_SCANCODE_S, true);
		step(game);
		assert(game.player_.speed() < accelerated);
		sendKey(game, SDL_SCANCODE_S, false);

		sendKey(game, SDL_SCANCODE_D, true);
		step(game);
		const float rightHeading = game.player_.headingDegrees();
		assert(rightHeading > 0.0f && rightHeading < 180.0f);
		sendKey(game, SDL_SCANCODE_D, false);
		step(game);
		assert(std::abs(game.player_.headingDegrees() - rightHeading) < 0.001f);
		sendKey(game, SDL_SCANCODE_A, true);
		step(game);
		assert(game.player_.headingDegrees() < rightHeading);

		SDL_Event lostFocus{};
		lostFocus.type = SDL_WINDOWEVENT;
		lostFocus.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
		game.handleEvent(lostFocus);
		const float heading = game.player_.headingDegrees();
		step(game);
		assert(std::abs(game.player_.headingDegrees() - heading) < 0.001f);

		game.player_.reset({ 100, 50 }, Direction::East);
		game.enemies_.emplace_back(SDL_Color{ 0, 0, 0, 255 }, SDL_Color{ 255, 0, 0, 255 }, nullptr, 8);
		game.brains_.push_back(createEnemyBrain(EnemyPersonality::Aggressor));
		Cycle& enemy = game.enemies_.front();
		enemy.reset({ 100, 100 }, Direction::East);
		game.moveCycle(enemy, game.brains_.front().get(), 0, 0.05f);
		const float firstTurn = enemy.headingDegrees();
		assert(firstTurn > 0.1f && firstTurn < 359.9f);
		assert(std::abs(std::remainder(firstTurn, 90.0f)) > 1.0f);
		for (int i = 0; i < 5; ++i) game.moveCycle(enemy, game.brains_.front().get(), 0, 0.05f);
		assert(enemy.alive());
		assert(enemy.trail().getSegments(&enemy).size() > 1);
		game.brains_.front()->resetDriving();
		enemy.respawn({ 100, 100 }, Direction::East);
		assert(std::abs(enemy.headingDegrees()) < 0.001f);

		game.level_.placements.push_back({ ObjectType::Wall, { 109, 95, 1, 11 } });
		game.objects_.reset(game.level_, {}, std::vector<std::uint8_t>(200 * 200, 0));
		game.player_.reset({ 150, 100 }, Direction::East);
		game.brains_.front()->resetDriving();
		for (int i = 0; i < 50 && enemy.alive(); ++i) {
			game.moveCycle(enemy, game.brains_.front().get(), 0, 0.02f);
		}
		assert(enemy.alive());
		assert(std::abs(std::remainder(enemy.headingDegrees(), 360.0f)) > 10.0f);
	}

	static void runSpectator(SDL_Renderer* renderer) {
		LightcyclesGame game(renderer);
		game.level_.width = 200;
		game.level_.height = 200;
		game.objects_.reset(game.level_, {}, {});
		game.player_.reset({ 20, 20 }, Direction::East);
		game.player_.setLives(0);
		game.onlineMatch_.enabled = false;
		for (int i = 0; i < 3; ++i) {
			game.enemies_.emplace_back(SDL_Color{ 0, 0, 0, 255 }, SDL_Color{ 255, 0, 0, 255 }, nullptr, 8);
			game.enemies_.back().reset({ 40 + i * 30, 60 }, Direction::East);
			game.brains_.push_back(createEnemyBrain(EnemyPersonality::Aggressor));
		}

		game.updateResult();
		assert(game.result_ == GameResult::Running);
		game.updateSpectatorCamera();
		assert(game.isSpectating_);
		assert((game.spectatorTarget_ == LightcyclesGame::CycleNameKey{ SnapshotCycleKind::Bot, 1 }));
		const int firstCameraX = game.renderContext_.camera.x;

		game.enemies_[0].setLives(0);
		game.updateResult();
		assert(game.result_ == GameResult::Running);
		game.updateSpectatorCamera();
		assert((game.spectatorTarget_ == LightcyclesGame::CycleNameKey{ SnapshotCycleKind::Bot, 2 }));
		assert(game.renderContext_.camera.x != firstCameraX);

		game.enemies_[1].setLives(0);
		game.updateResult();
		assert(game.result_ == GameResult::PlayerLost);
		game.updateSpectatorCamera();
		assert(game.isSpectating_);
		assert((game.spectatorTarget_ == LightcyclesGame::CycleNameKey{ SnapshotCycleKind::Bot, 3 }));
	}

	static void runRemote(SDL_Renderer* renderer, int enemyCount) {
		LightcyclesGame game(renderer);
		game.level_.width = 200;
		game.level_.height = 200;
		game.objects_.reset(game.level_, {}, {});
		game.player_.reset({ 30, 30 }, Direction::East);
		game.onlineMatch_.enabled = true;
		game.onlineMatch_.host = true;
		for (int i = 0; i < enemyCount; ++i) {
			game.enemies_.emplace_back(SDL_Color{ 0, 0, 0, 255 }, SDL_Color{ 255, 0, 0, 255 }, nullptr, 8);
			game.enemies_.back().reset({ 30 + i * 120, 150 }, Direction::East);
			game.brains_.push_back(createEnemyBrain(EnemyPersonality::Aggressor));
		}
		const std::uint64_t id = 0x110000100000001ull;
		LightcyclesGame::RemoteCycle remote;
		remote.cycle = std::make_unique<Cycle>(SDL_Color{ 0, 0, 0, 255 },
			SDL_Color{ 0, 255, 0, 255 }, nullptr, 8);
		remote.cycle->reset({ 100, 100 }, Direction::East);
		remote.connected = true;
		Cycle* cycle = remote.cycle.get();
		game.remoteCycles_.emplace(id, std::move(remote));
		assert(game.remoteInputs_.addPlayer(id));
		const auto packet = deserializeDrivingInput(serializeDrivingInput({ 1, { true, false, false, true } }));
		assert(packet && game.remoteInputs_.accept(id, *packet));
		game.updateCycles(0.05f);
		assert(cycle->alive());
		assert(std::abs(cycle->headingDegrees() - 9.0f) < 0.01f);
		assert(std::abs(cycle->baseSpeed() - (Cycle::BASE_SPEED + Cycle::ACCELERATION * 0.05f)) < 0.01f);
		assert(game.buildArenaView().chaseTargets.size() == static_cast<std::size_t>(enemyCount + 2));
		cycle->effects().apply(EffectType::Boost, 2.0f);
		const auto captured = game.captureCycleSnapshot(id + 10, 42);
		assert(captured.cycles.size() == static_cast<std::size_t>(enemyCount + 2));
		const auto encoded = serializeCycleSnapshot(captured);
		assert(encoded);
		const auto decoded = deserializeCycleSnapshot(*encoded);
		assert(decoded && decoded->sequence == 42 && decoded->cycles[1].id == id);
		assert(decoded->cycles[1].position.x == cycle->center().x);
		assert(decoded->cycles[1].heading == cycle->headingDegrees());
		assert(decoded->cycles[1].speed == cycle->baseSpeed() && decoded->cycles[1].boost);
		assert(decoded->cycles[1].core[1] == cycle->coreColor().g);
		assert(decoded->cycles[1].trail.size() == cycle->trail().vertices().size());
		for (int i = 0; i < enemyCount; ++i) {
			assert(decoded->cycles[i + 2].kind == SnapshotCycleKind::Bot);
			assert(decoded->cycles[i + 2].id == static_cast<std::uint64_t>(i + 1));
		}
		game.remoteInputs_.update(RemoteDrivingInput::TIMEOUT_SECONDS);
		game.updateCycles(0.05f);
		assert(std::abs(cycle->headingDegrees() - 9.0f) < 0.01f);
		assert(std::abs(cycle->baseSpeed() - (Cycle::BASE_SPEED + Cycle::ACCELERATION * 0.05f)) < 0.01f);

		const std::uint64_t secondId = id + 1;
		LightcyclesGame::RemoteCycle second;
		second.cycle = std::make_unique<Cycle>(SDL_Color{ 0, 0, 0, 255 },
			SDL_Color{ 0, 0, 255, 255 }, nullptr, 8);
		second.cycle->reset(cycle->position(), Direction::East);
		second.connected = true;
		Cycle* secondCycle = second.cycle.get();
		game.remoteCycles_.emplace(secondId, std::move(second));
		game.crashCollidingHeads();
		assert(cycle->lives() == Cycle::INITIAL_LIVES - 1);
		assert(secondCycle->lives() == Cycle::INITIAL_LIVES - 1);

		game.player_.setLives(0);
		game.updateResult();
		assert(game.result_ == GameResult::Running);
		game.remoteCycles_.at(id).connected = false;
		game.remoteCycles_.at(id).retired = true;
		game.remoteCycles_.at(secondId).connected = false;
		game.remoteCycles_.at(secondId).retired = true;
		game.updateCycles(0.01f);
		assert(game.remoteCycles_.empty());
	}

	static void runWorldSnapshot(SDL_Renderer* renderer) {
		LevelDefinition level;
		level.width = 100;
		level.height = 100;
		level.placements = {
			{ ObjectType::Wall, { 10, 10, 2, 2 } },
			{ ObjectType::Tower, { 25, 25, 8, 8 } },
			{ ObjectType::BoostPickup, { 60, 60, 2, 2 } },
			{ ObjectType::MineTrap, { 70, 70, 2, 2 } }
		};
		const std::vector<std::uint8_t> occupied(100 * 100, 0);
		ArenaObjects host(8);
		ArenaObjects client(8);
		host.setRenderer(renderer);
		client.setRenderer(renderer);
		host.reset(level, {}, occupied);
		client.reset(level, {}, occupied);
		WorldSnapshot state = host.snapshot(1);
		assert(state.objects.size() == 2 && state.towers.size() == 1);
		assert(state.towers[0].placement.x == 25 && state.towers[0].placement.y == 25);
		Cycle target({ 0, 0, 0, 255 }, { 255, 0, 0, 255 }, nullptr, 8);
		target.reset({ 45, 32 }, Direction::East);
		bool fired = false;
		for (int i = 0; i < 30 && !fired; ++i) {
			host.update(0.1f, occupied, { &target });
			state = host.snapshot(2);
			fired = !state.towers[0].projectiles.empty();
		}
		assert(fired);
		assert(state.towers[0].projectiles[0].position.x > 0.0f);
		host.touch(target, { 60, 60 });
		state = host.snapshot(3);
		assert(state.objects.size() == 1);
		const auto wire = serializeWorldSnapshot(state);
		assert(wire);
		const auto decoded = deserializeWorldSnapshot(*wire);
		assert(decoded && client.applySnapshot(*decoded));
		const WorldSnapshot applied = client.snapshot(3);
		assert(applied.objects.size() == 1 && applied.objects[0].type == SnapshotObjectType::Mine);
		assert(applied.towers[0].attacking == state.towers[0].attacking);
		assert(applied.towers[0].animationFrame == state.towers[0].animationFrame);
		assert(applied.towers[0].projectiles.size() == state.towers[0].projectiles.size());
		assert(applied.towers[0].projectiles[0].position.x == state.towers[0].projectiles[0].position.x);
		assert(client.getWallBoxes().size() == 1);
		assert(client.pickups().empty());
		WorldSnapshot invalid = state;
		invalid.objects[0].cells = { 10, 10, 2, 2 };
		assert(!client.applySnapshot(invalid));
		assert(client.snapshot(3).objects[0].cells.x == state.objects[0].cells.x);
		invalid = state;
		invalid.towers[0].animationFrame = 1000;
		assert(!client.applySnapshot(invalid));
		assert(client.snapshot(3).towers[0].animationFrame == state.towers[0].animationFrame);
		invalid = state;
		invalid.towers[0].projectiles[0].animationFrame = 1000;
		assert(!client.applySnapshot(invalid));
		assert(client.snapshot(3).towers[0].projectiles[0].position.x == state.towers[0].projectiles[0].position.x);
		invalid = state;
		invalid.objects[0].cells = { 99, 99, 2, 2 };
		assert(!client.applySnapshot(invalid));
		invalid = state;
		invalid.towers.clear();
		assert(!client.applySnapshot(invalid));
		state.towers[0].projectiles.clear();
		state.objects.push_back({ SnapshotObjectType::LongTrail, { 55, 55, 2, 2 } });
		assert(client.applySnapshot(state));
		assert(client.snapshot(3).towers[0].projectiles.empty());
		assert(client.pickups().size() == 1);
	}

	static void runOnlineFrame(SDL_Renderer* renderer) {
		const std::uint64_t hostId = 0x110000100000001ull;
		const std::uint64_t clientId = hostId + 1;
		LightcyclesGame host(renderer);
		LightcyclesGame client(renderer);
		host.level_.width = client.level_.width = 200;
		host.level_.height = client.level_.height = 200;
		host.objects_.reset(host.level_, {}, {});
		client.objects_.reset(client.level_, {}, {});
		host.onlineMatch_.enabled = true;
		host.onlineMatch_.host = true;
		host.player_.reset({ 30, 30 }, Direction::East);
		LightcyclesGame::RemoteCycle remote;
		const SDL_Color glow{ 10, 95, 45, 255 };
		const SDL_Color core{ 60, 255, 130, 255 };
		remote.cycle = std::make_unique<Cycle>(glow, core, host.cycleSkins_.forColor(core), 8);
		remote.cycle->reset({ 100, 100 }, Direction::East);
		remote.cycle->effects().apply(EffectType::Slow, 3.0f);
		remote.cycle->addScore(7);
		remote.connected = true;
		host.remoteCycles_.emplace(clientId, std::move(remote));
		host.enemies_.emplace_back(glow, core, host.cycleSkins_.forColor(core), 8);
		host.enemies_.back().reset({ 160, 160 }, Direction::North);
		host.brains_.push_back(createEnemyBrain(EnemyPersonality::Aggressor));
		assert(host.remoteInputs_.addPlayer(clientId));
		const auto input = deserializeDrivingInput(serializeDrivingInput({ 1, { true, false, false, true } }));
		assert(input && host.remoteInputs_.accept(clientId, *input));
		host.updateCycles(0.1f);
		assert(host.remoteCycles_.at(clientId).cycle->alive());
		OnlineFrame frame;
		frame.phase = OnlinePhase::Running;
		frame.cycles = host.captureCycleSnapshot(hostId, 5);
		frame.world = host.objects_.snapshot(5);
		const auto wire = serializeOnlineFrame(frame);
		assert(wire);
		OnlineFrameAssembler assembler;
		assert(!assembler.accept((*wire)[0]));
		const auto received = assembler.accept((*wire)[1]);
		assert(received && client.applyOnlineFrame(*received, clientId, hostId, { hostId, clientId }));
		assert(client.receivedFrame_ && client.matchStarted_);
		assert(client.player_.center().x == frame.cycles.cycles[1].position.x);
		assert(std::abs(client.player_.headingDegrees() - 18.0f) < 0.01f);
		assert(client.player_.score() == 7 && client.player_.effects().isActive(EffectType::Slow));
		assert(client.player_.trail().vertices().size() == frame.cycles.cycles[1].trail.size());
		assert(client.visualCycles_.size() == 2);
		assert(client.visualCycles_.at({ SnapshotCycleKind::Human, hostId })->center().x == host.player_.center().x);
		assert(client.visualCycles_.at({ SnapshotCycleKind::Bot, 1 })->direction() == Direction::North);
		assert(!client.applyOnlineFrame(*received, clientId, hostId, { hostId, clientId }));

		OnlineFrame invalid = *received;
		invalid.cycles.sequence = invalid.world.sequence = 6;
		invalid.cycles.cycles[1].position.x += 50.0f;
		invalid.world.towers.push_back({ { 1, 1, 1, 1 }, false, 0, {} });
		const float before = client.player_.center().x;
		assert(!client.applyOnlineFrame(invalid, clientId, hostId, { hostId, clientId }));
		assert(client.player_.center().x == before);
		invalid = *received;
		invalid.cycles.sequence = invalid.world.sequence = 6;
		invalid.cycles.cycles[1].animationFrame = 65535;
		assert(!client.applyOnlineFrame(invalid, clientId, hostId, { hostId, clientId }));
		assert(client.player_.center().x == before);
		invalid = *received;
		invalid.cycles.sequence = invalid.world.sequence = 6;
		assert(!client.applyOnlineFrame(invalid, clientId, hostId, { clientId }));
		invalid.cycles.cycles[1].position.x += 20.0f;
		invalid.cycles.cycles[1].heading = 28.0f;
		assert(client.applyOnlineFrame(invalid, clientId, hostId, { hostId, clientId }));
		assert(client.player_.center().x == before && std::abs(client.player_.headingDegrees() - 18.0f) < 0.01f);
		client.updateVisualInterpolation(0.05f);
		assert(std::abs(client.player_.center().x - (before + 10.0f)) < 0.001f);
		assert(std::abs(client.player_.headingDegrees() - 23.0f) < 0.01f);
		assert(std::abs(client.player_.trail().vertices().back().x - client.player_.center().x) < 0.001f);
		client.updateVisualInterpolation(0.05f);
		assert(std::abs(client.player_.center().x - (before + 20.0f)) < 0.001f);
		invalid.cycles.sequence = invalid.world.sequence = 7;
		invalid.phase = OnlinePhase::Finished;
		invalid.cycles.cycles[1].alive = false;
		invalid.cycles.cycles[1].lives = 0;
		invalid.cycles.cycles[2].alive = false;
		invalid.cycles.cycles[2].lives = 0;
		assert(client.applyOnlineFrame(invalid, clientId, hostId, { hostId, clientId }));
		assert(client.result_ == GameResult::PlayerLost);
		invalid.cycles.sequence = invalid.world.sequence = 8;
		invalid.phase = OnlinePhase::Running;
		assert(!client.applyOnlineFrame(invalid, clientId, hostId, { hostId, clientId }));
	}

	static void runActualMapFrame(SDL_Renderer* renderer) {
		const std::uint64_t hostId = 0x110000100000011ull;
		const std::uint64_t clientId = hostId + 1;
		LightcyclesGame host(renderer);
		LightcyclesGame client(renderer);
		GameConfig config;
		config.aiCount = 5;
		assert(host.startMatch(config));
		assert(client.startMatch(config));
		assert(host.cycleDisplayName(SnapshotCycleKind::Human, 0) == "Player");
		for (std::uint64_t id = 1; id <= 5; ++id) {
			assert(host.cycleDisplayName(SnapshotCycleKind::Bot, id) == "CPU" + std::to_string(id));
		}
		const auto& color = PLAYER_COLOR_OPTIONS[1];
		LightcyclesGame::RemoteCycle remote;
		remote.cycle = std::make_unique<Cycle>(color.glow, color.core, host.cycleSkins_.forColor(color.core), 8);
		remote.cycle->reset({ 200, 200 }, Direction::East);
		remote.connected = true;
		host.remoteCycles_.emplace(clientId, std::move(remote));
		OnlineFrame frame;
		frame.phase = OnlinePhase::Running;
		frame.cycles = host.captureCycleSnapshot(hostId, 123);
		frame.world = host.objects_.snapshot(123);
		assert(frame.world.towers.size() == 4 && frame.cycles.cycles.size() == 7);
		const auto parts = serializeOnlineFrame(frame);
		assert(parts);
		OnlineFrameAssembler assembler;
		assert(!assembler.accept((*parts)[0]));
		const auto received = assembler.accept((*parts)[1]);
		assert(received && client.applyOnlineFrame(*received, clientId, hostId, { hostId, clientId }));
		assert(client.objects_.getWallBoxes().size() == 750);
		assert(client.visualCycles_.size() == 6);
		assert(client.objects_.snapshot(123).objects.size() == frame.world.objects.size());
		ArenaView indexed = host.buildArenaView();
		ArenaView reference = indexed;
		reference.wallIndex = nullptr;
		std::mt19937 random(9876);
		std::uniform_real_distribution<float> x(-100.0f, indexed.worldWidth + 100.0f);
		std::uniform_real_distribution<float> y(-100.0f, indexed.worldHeight + 100.0f);
		std::uniform_real_distribution<float> angle(-3.14159265f, 3.14159265f);
		for (int i = 0; i < 1000; ++i) {
			const Vec2 origin{ x(random), y(random) };
			const float a = angle(random);
			const Vec2 dir{ std::cos(a), std::sin(a) };
			const RaycastHit expected = reference.raycast(origin, dir, 500.0f, &host.player_);
			const RaycastHit actual = indexed.raycast(origin, dir, 500.0f, &host.player_);
			assert(actual.hit == expected.hit && actual.distance == expected.distance
				&& actual.point.x == expected.point.x && actual.point.y == expected.point.y
				&& actual.hitCycle == expected.hitCycle);
		}
		for (int i = 0; i < 10; ++i) host.updateCycles(1.0f / 60.0f);
		assert(host.result_ == GameResult::Running);
		host.onlineMatch_.enabled = true;
		host.onlineMatch_.host = true;
		for (int i = 0; i < 10; ++i) host.updateCycles(1.0f / 60.0f);
		assert(host.objects_.getWallBoxes().size() == 750);
	}
};

int main() {
#ifdef _WIN32
	_set_error_mode(_OUT_TO_STDERR);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
	assert(SDL_setenv("SDL_VIDEODRIVER", "dummy", 1) == 0);
	assert(SDL_Init(SDL_INIT_VIDEO) == 0);
	SDL_Surface* surface = SDL_CreateRGBSurface(0, 64, 64, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
	assert(surface != nullptr);
	SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
	assert(renderer != nullptr);
	LightcyclesGameInputTest::run(renderer);
	LightcyclesGameInputTest::runSpectator(renderer);
	LightcyclesGameInputTest::runWallIndex();
	LightcyclesGameInputTest::runRemote(renderer, 0);
	LightcyclesGameInputTest::runRemote(renderer, 2);
	LightcyclesGameInputTest::runWorldSnapshot(renderer);
	LightcyclesGameInputTest::runOnlineFrame(renderer);
	LightcyclesGameInputTest::runActualMapFrame(renderer);
	SDL_DestroyRenderer(renderer);
	SDL_FreeSurface(surface);
	SDL_Quit();
	std::cout << "Game input tests passed\n";
}
