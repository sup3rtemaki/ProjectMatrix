#include "lightcyclesGame.h"

#include "level/levelLoader.h"
#include "level/spawnPlanner.h"
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

LightcyclesGame::LightcyclesGame(SDL_Renderer* renderer)
	: renderer_(renderer),
	resPath_(getResourcePath()),
	player_(PLAYER_GLOW, PLAYER_CORE, &cycleAnimSet_, CELL_SIZE),
	objects_(CELL_SIZE) {
	renderContext_.renderer = renderer_;
	renderContext_.camera = { 0, 0, WINDOW_WIDTH, VIEWPORT_HEIGHT };

	if (std::ifstream(resPath_ + CYCLE_ANIMATION_SET).good()) {
		std::list<DataGroupType> dataGroupTypes;
		cycleAnimSet_.loadAnimationSet(CYCLE_ANIMATION_SET, dataGroupTypes, renderer_);
	}
	else {
		std::cerr << "Animation set not found: " << resPath_ + CYCLE_ANIMATION_SET << std::endl;
	}

	keyboard_.UP = SDL_SCANCODE_W;
	keyboard_.DOWN = SDL_SCANCODE_S;
	keyboard_.LEFT = SDL_SCANCODE_A;
	keyboard_.RIGHT = SDL_SCANCODE_D;
	cameraController_.isLerping = true;
	cameraController_.lerp = 10.0f;
	cameraController_.target = &player_;

	hudUi_.setRenderer(renderer_);
	hudUi_.setResourcePath(resPath_);

	levelCount_ = countLevels();
	inMenu_ = true;
	exitRequested_ = false;
}

std::vector<std::string> LightcyclesGame::getAvailableLevelNames() const {
	std::vector<std::string> names;
	for (std::size_t i = 0; i < levelCount_; ++i) {
		std::optional<LevelDefinition> def = LevelLoader::load(levelPath(i));
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

	std::size_t targetLevel = config.levelIndex;
	if (targetLevel >= levelCount_) {
		targetLevel = 0;
	}

	if (!loadLevel(targetLevel)) {
		std::cerr << "Failed to start match with level index " << targetLevel << std::endl;
		return false;
	}

	// Apply configured player color
	player_ = Cycle(config.playerGlow, config.playerCore, &cycleAnimSet_, CELL_SIZE);
	cameraController_.target = &player_;

	// Adjust AI count according to config
	if (config.aiCount > 0 && config.aiCount < static_cast<int>(enemies_.size())) {
		while (static_cast<int>(enemies_.size()) > config.aiCount) {
			enemies_.pop_back();
			brains_.pop_back();
		}
	}
	else if (config.aiCount > static_cast<int>(enemies_.size())) {
		// Fill additional AI opponents with standard personalities
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
		while (static_cast<int>(enemies_.size()) < config.aiCount) {
			const auto& c = extraColors[extraIdx % 3];
			EnemyPersonality p = extraPersonalities[extraIdx % 5];
			enemies_.emplace_back(c[0], c[1], &cycleAnimSet_, CELL_SIZE);
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
}

std::string LightcyclesGame::levelPath(std::size_t index) const {
	return resPath_ + LEVELS_FOLDER + "level" + std::to_string(index + 1) + ".json";
}

std::size_t LightcyclesGame::countLevels() const {
	std::size_t count = 0;
	while (std::ifstream(levelPath(count)).good()) {
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
	std::optional<LevelDefinition> loaded = LevelLoader::load(levelPath(index));
	if (!loaded || !isPlayable(*loaded)) {
		return false;
	}

	level_ = std::move(*loaded);
	levelIndex_ = index;
	enemies_.clear();
	brains_.clear();
	for (const EnemyDefinition& definition : level_.enemies) {
		enemies_.emplace_back(definition.glow, definition.core, &cycleAnimSet_, CELL_SIZE);
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

	if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
		if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
			returnToMenu();
			return;
		}
		if (event.key.keysym.scancode == SDL_SCANCODE_RETURN
			&& result_ != GameResult::Running) {
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

	if (result_ != GameResult::Running) {
		return;
	}

	for (const InputCommand& command : keyboard_.update(&event)) {
		if (command.type == InputCommandType::Move) {
			player_.setDirectionFromAngle(command.angle);
		}
	}
}

void LightcyclesGame::update(float elapsedSeconds) {
	if (result_ != GameResult::Running) {
		return;
	}

	cameraController_.deltaTime = std::min(elapsedSeconds, 0.25f);
	cameraController_.update(renderContext_.camera, level_.width * CELL_SIZE, level_.height * CELL_SIZE);

	updateCycles(std::min(elapsedSeconds, 0.25f));
}

void LightcyclesGame::render() {
	SDL_SetRenderDrawColor(renderer_, 0, 0, 0, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(renderer_);

	RenderContext drawContext = renderContext_;
	drawContext.camera.y = renderContext_.camera.y - HUD_HEIGHT;

	const SDL_Rect& camera = renderContext_.camera;
	const SDL_Rect arena = { -camera.x, HUD_HEIGHT - camera.y, level_.width * CELL_SIZE, level_.height * CELL_SIZE };
	SDL_SetRenderDrawColor(renderer_, 22, 32, 45, SDL_ALPHA_OPAQUE);
	SDL_RenderDrawRect(renderer_, &arena);

	objects_.draw(drawContext);
	drawTrail(player_);
	for (const Cycle& enemy : enemies_) {
		drawTrail(enemy);
	}
	for (Cycle& enemy : enemies_) {
		enemy.draw(drawContext);
	}
	player_.draw(drawContext);

	HudState hudState;
	hudState.score = player_.score();
	hudState.lives = player_.lives();
	hudState.maxLives = Cycle::INITIAL_LIVES;
	hudState.totalEnemies = static_cast<int>(enemies_.size());
	hudState.enemiesAlive = static_cast<int>(std::count_if(enemies_.begin(), enemies_.end(),
		[](const Cycle& enemy) { return !enemy.isEliminated(); }));
	hudState.levelName = level_.name;
	hudState.isRespawning = player_.isRespawning();
	hudState.hasBoost = player_.effects().isActive(EffectType::Boost);
	hudState.hasLongTrail = player_.effects().isActive(EffectType::LongTrail);
	hudState.hasSlow = player_.effects().isActive(EffectType::Slow);

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

bool LightcyclesGame::isInsideGrid(int x, int y) const {
	return x >= 0 && x < level_.width && y >= 0 && y < level_.height;
}

bool LightcyclesGame::isOccupied(int x, int y) const {
	return !isInsideGrid(x, y) || occupied_[gridIndex(x, y)] != 0;
}

std::size_t LightcyclesGame::gridIndex(int x, int y) const {
	return static_cast<std::size_t>(y * level_.width + x);
}

void LightcyclesGame::updateCycles(float elapsedSeconds) {
	updateCycleRespawn(player_, elapsedSeconds);
	for (Cycle& enemy : enemies_) {
		updateCycleRespawn(enemy, elapsedSeconds);
	}

	float fastest = 1.0f;
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
	objects_.update(elapsedSeconds, occupied_);

	// Sub-steps of at most half a cell so no grid cell is skipped.
	const float maxDistance = CYCLE_SPEED * fastest * elapsedSeconds;
	const int steps = std::max(1, static_cast<int>(std::ceil(maxDistance / (CELL_SIZE / 2.0f))));

	for (int step = 0; step < steps && result_ == GameResult::Running; ++step) {
		const float stepSeconds = elapsedSeconds / steps;
		if (player_.alive() && !player_.isRespawning()) {
			moveCycle(player_, nullptr, stepSeconds);
		}
		for (std::size_t index = 0; index < enemies_.size(); ++index) {
			if (enemies_[index].alive() && !enemies_[index].isRespawning()) {
				moveCycle(enemies_[index], brains_[index].get(), stepSeconds);
			}
		}
		crashCollidingHeads();
		updateResult();
	}
}

void LightcyclesGame::moveCycle(Cycle& cycle, EnemyBrain* brain, float elapsedSeconds) {
	if (!cycle.alive() || cycle.isRespawning()) {
		return;
	}

	const SDL_Point before = cycle.position();
	cycle.advance(CYCLE_SPEED * cycle.effects().speedMultiplier() * elapsedSeconds);
	const SDL_Point after = cycle.position();
	if (before.x == after.x && before.y == after.y) {
		return;
	}

	lightCell(cycle, before);
	if (isOccupied(after.x, after.y)) {
		if (isInsideGrid(after.x, after.y)) {
			Cycle* killer = trailOwner_[gridIndex(after.x, after.y)];
			if (killer != nullptr && killer != &cycle) {
				killer->addScore(1);
			}
		}
		handleCycleCrash(cycle);
		return;
	}

	objects_.touch(cycle, after);
	if (!cycle.alive()) {
		handleCycleCrash(cycle);
		return;
	}

	if (brain != nullptr) {
		const ArenaView arena{ level_.width, level_.height, occupied_, objects_.dangerGrid(),
			objects_.pickups() };
		cycle.setDirection(brain->decide(cycle, player_, arena));
	}
}

void LightcyclesGame::lightCell(Cycle& cycle, SDL_Point cell) {
	const std::size_t idx = gridIndex(cell.x, cell.y);
	occupied_[idx] = 1;
	trailOwner_[idx] = &cycle;
	cycle.addTrailCell(cell);
	while (cycle.trail().size() > cycle.maxTrailLength()) {
		const SDL_Point expired = cycle.popOldestTrailPoint();
		const std::size_t expIdx = gridIndex(expired.x, expired.y);
		occupied_[expIdx] = 0;
		trailOwner_[expIdx] = nullptr;
	}
}

void LightcyclesGame::clearCycleTrail(Cycle& cycle) {
	for (const SDL_Point point : cycle.trail()) {
		if (isInsideGrid(point.x, point.y)) {
			const std::size_t idx = gridIndex(point.x, point.y);
			occupied_[idx] = 0;
			trailOwner_[idx] = nullptr;
		}
	}
	cycle.clearTrail();
}

void LightcyclesGame::handleCycleCrash(Cycle& cycle) {
	if (cycle.isRespawning() || cycle.isEliminated()) {
		return;
	}
	cycle.crash();
	cycle.loseLife();
	clearCycleTrail(cycle);

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

bool LightcyclesGame::findSafeRespawn(SDL_Point& outCell, Direction& outDir) {
	const SDL_Point offsets[] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };
	std::vector<Cycle*> aliveCycles;
	if (player_.alive() && !player_.isRespawning()) {
		aliveCycles.push_back(&player_);
	}
	for (Cycle& enemy : enemies_) {
		if (enemy.alive() && !enemy.isRespawning()) {
			aliveCycles.push_back(&enemy);
		}
	}

	for (int attempt = 0; attempt < 200; ++attempt) {
		const SDL_Point candidate = {
			10 + getRandomNumber(level_.width - 20),
			10 + getRandomNumber(level_.height - 20)
		};
		if (isOccupied(candidate.x, candidate.y)
			|| objects_.dangerGrid()[gridIndex(candidate.x, candidate.y)] != 0) {
			continue;
		}
		bool farFromOthers = true;
		for (Cycle* other : aliveCycles) {
			if (std::abs(other->position().x - candidate.x) < 15
				&& std::abs(other->position().y - candidate.y) < 15) {
				farFromOthers = false;
				break;
			}
		}
		if (!farFromOthers) {
			continue;
		}
		for (int d = 0; d < 4; ++d) {
			bool open = true;
			for (int step = 1; step <= 8; ++step) {
				const int cx = candidate.x + offsets[d].x * step;
				const int cy = candidate.y + offsets[d].y * step;
				if (!isInsideGrid(cx, cy) || isOccupied(cx, cy)
					|| objects_.dangerGrid()[gridIndex(cx, cy)] != 0) {
					open = false;
					break;
				}
			}
			if (open) {
				outCell = candidate;
				outDir = static_cast<Direction>(d);
				return true;
			}
		}
	}
	outCell = { level_.width / 2, level_.height / 2 };
	outDir = Direction::East;
	return true;
}

void LightcyclesGame::respawnCycle(Cycle& cycle) {
	SDL_Point cell;
	Direction dir;
	findSafeRespawn(cell, dir);
	cycle.respawn(cell, dir);
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

	for (std::size_t first = 0; first < alive.size(); ++first) {
		for (std::size_t second = first + 1; second < alive.size(); ++second) {
			const SDL_Point a = alive[first]->position();
			const SDL_Point b = alive[second]->position();
			if (a.x == b.x && a.y == b.y) {
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
	occupied_.assign(static_cast<std::size_t>(level_.width * level_.height), 0);
	trailOwner_.assign(static_cast<std::size_t>(level_.width * level_.height), nullptr);

	const std::vector<ResolvedSpawn> spawns = resolveSpawns(level_);
	std::vector<SDL_Point> startCells;
	for (const ResolvedSpawn& spawn : spawns) {
		startCells.push_back(spawn.cell);
	}
	player_.reset(spawns[0].cell, spawns[0].direction);
	for (std::size_t index = 0; index < enemies_.size(); ++index) {
		enemies_[index].reset(spawns[index + 1].cell, spawns[index + 1].direction);
	}
	objects_.reset(level_, startCells, occupied_);

	renderContext_.camera.x = std::clamp(static_cast<int>(player_.x) - WINDOW_WIDTH / 2,
		0, level_.width * CELL_SIZE - WINDOW_WIDTH);
	renderContext_.camera.y = std::clamp(static_cast<int>(player_.y) - VIEWPORT_HEIGHT / 2,
		0, level_.height * CELL_SIZE - VIEWPORT_HEIGHT);
	result_ = GameResult::Running;
}

void LightcyclesGame::drawTrail(const Cycle& cycle) const {
	const SDL_Rect& camera = renderContext_.camera;
	for (const SDL_Point point : cycle.trail()) {
		const SDL_Rect glow = { point.x * CELL_SIZE - camera.x, HUD_HEIGHT + point.y * CELL_SIZE - camera.y,
			CELL_SIZE, CELL_SIZE };
		setDrawColor(cycle.glowColor());
		SDL_RenderFillRect(renderer_, &glow);

		const SDL_Rect core = { glow.x + 3, glow.y + 3, CELL_SIZE - 6, CELL_SIZE - 6 };
		setDrawColor(cycle.coreColor());
		SDL_RenderFillRect(renderer_, &core);
	}
}

void LightcyclesGame::setDrawColor(SDL_Color color) const {
	SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
}
