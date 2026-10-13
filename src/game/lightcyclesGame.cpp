#include "lightcyclesGame.h"

#include "level/levelLoader.h"
#include "level/spawnPlanner.h"
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
	renderContext_.camera = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };

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

	levelCount_ = countLevels();
	if (levelCount_ == 0 || !loadLevel(0)) {
		std::cerr << "No playable level in " << resPath_ + LEVELS_FOLDER << std::endl;
		running_ = false;
	}
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
		return;
	}

	if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
		if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
			running_ = false;
			return;
		}
		if (event.key.keysym.scancode == SDL_SCANCODE_RETURN
			&& result_ != GameResult::Running) {
			const bool advanced = result_ == GameResult::PlayerWon
				&& loadLevel((levelIndex_ + 1) % levelCount_);
			if (!advanced) {
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

	const SDL_Rect& camera = renderContext_.camera;
	const SDL_Rect arena = { -camera.x, -camera.y, level_.width * CELL_SIZE, level_.height * CELL_SIZE };
	SDL_SetRenderDrawColor(renderer_, 22, 32, 45, SDL_ALPHA_OPAQUE);
	SDL_RenderDrawRect(renderer_, &arena);

	objects_.draw(renderContext_);
	drawTrail(player_);
	for (const Cycle& enemy : enemies_) {
		drawTrail(enemy);
	}
	for (Cycle& enemy : enemies_) {
		enemy.draw(renderContext_);
	}
	player_.draw(renderContext_);

	SDL_Rect statusStrip = { 0, 0, WINDOW_WIDTH, 3 };
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
		SDL_SetRenderDrawColor(renderer_, 0, 105, 135, SDL_ALPHA_OPAQUE);
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
	float fastest = 1.0f;
	player_.effects().update(elapsedSeconds);
	fastest = std::max(fastest, player_.effects().speedMultiplier());
	for (Cycle& enemy : enemies_) {
		enemy.effects().update(elapsedSeconds);
		fastest = std::max(fastest, enemy.effects().speedMultiplier());
	}
	objects_.update(elapsedSeconds, occupied_);

	// Sub-steps of at most half a cell so no grid cell is skipped.
	const float maxDistance = CYCLE_SPEED * fastest * elapsedSeconds;
	const int steps = std::max(1, static_cast<int>(std::ceil(maxDistance / (CELL_SIZE / 2.0f))));

	for (int step = 0; step < steps && result_ == GameResult::Running; ++step) {
		const float stepSeconds = elapsedSeconds / steps;
		moveCycle(player_, nullptr, stepSeconds);
		for (std::size_t index = 0; index < enemies_.size(); ++index) {
			moveCycle(enemies_[index], brains_[index].get(), stepSeconds);
		}
		crashCollidingHeads();
		updateResult();
	}
}

void LightcyclesGame::moveCycle(Cycle& cycle, EnemyBrain* brain, float elapsedSeconds) {
	if (!cycle.alive()) {
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
		cycle.crash();
		return;
	}

	objects_.touch(cycle, after);
	if (!cycle.alive()) {
		return;
	}

	if (brain != nullptr) {
		const ArenaView arena{ level_.width, level_.height, occupied_, objects_.dangerGrid(),
			objects_.pickups() };
		cycle.setDirection(brain->decide(cycle, player_, arena));
	}
}

void LightcyclesGame::lightCell(Cycle& cycle, SDL_Point cell) {
	occupied_[gridIndex(cell.x, cell.y)] = 1;
	cycle.addTrailCell(cell);
	while (cycle.trail().size() > cycle.maxTrailLength()) {
		const SDL_Point expired = cycle.popOldestTrailPoint();
		occupied_[gridIndex(expired.x, expired.y)] = 0;
	}
}

void LightcyclesGame::crashCollidingHeads() {
	std::vector<Cycle*> alive;
	if (player_.alive()) {
		alive.push_back(&player_);
	}
	for (Cycle& enemy : enemies_) {
		if (enemy.alive()) {
			alive.push_back(&enemy);
		}
	}

	for (std::size_t first = 0; first < alive.size(); ++first) {
		for (std::size_t second = first + 1; second < alive.size(); ++second) {
			const SDL_Point a = alive[first]->position();
			const SDL_Point b = alive[second]->position();
			if (a.x == b.x && a.y == b.y) {
				alive[first]->crash();
				alive[second]->crash();
			}
		}
	}
}

void LightcyclesGame::updateResult() {
	const bool anyEnemyAlive = std::any_of(enemies_.begin(), enemies_.end(),
		[](const Cycle& enemy) { return enemy.alive(); });
	if (!player_.alive()) {
		result_ = anyEnemyAlive ? GameResult::PlayerLost : GameResult::Draw;
	}
	else if (!anyEnemyAlive) {
		result_ = GameResult::PlayerWon;
	}
}

void LightcyclesGame::reset() {
	occupied_.assign(static_cast<std::size_t>(level_.width * level_.height), 0);

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
	renderContext_.camera.y = std::clamp(static_cast<int>(player_.y) - WINDOW_HEIGHT / 2,
		0, level_.height * CELL_SIZE - WINDOW_HEIGHT);
	result_ = GameResult::Running;
}

void LightcyclesGame::drawTrail(const Cycle& cycle) const {
	const SDL_Rect& camera = renderContext_.camera;
	for (const SDL_Point point : cycle.trail()) {
		const SDL_Rect glow = { point.x * CELL_SIZE - camera.x, point.y * CELL_SIZE - camera.y,
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
