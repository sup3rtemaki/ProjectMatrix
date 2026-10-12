#include "lightcyclesGame.h"

#include "res_path.h"

#include <algorithm>
#include <fstream>
#include <iostream>

namespace {
constexpr SDL_Color PLAYER_GLOW = { 0, 90, 135, 255 };
constexpr SDL_Color PLAYER_CORE = { 20, 235, 255, 255 };

struct EnemySpec {
	EnemyPersonality personality;
	SDL_Color glow;
	SDL_Color core;
	SDL_Point startPercent; // position in the arena, as a percentage of its size
	Direction direction;
};

const EnemySpec ENEMY_SPECS[] = {
	{ EnemyPersonality::Aggressor, { 105, 15, 30, 255 }, { 255, 45, 75, 255 }, { 90, 50 }, Direction::West },
	{ EnemyPersonality::Ambusher, { 105, 50, 5, 255 }, { 255, 155, 35, 255 }, { 10, 25 }, Direction::South },
	{ EnemyPersonality::Strategist, { 65, 20, 115, 255 }, { 185, 95, 255, 255 }, { 50, 75 }, Direction::North },
	{ EnemyPersonality::Coward, { 10, 95, 45, 255 }, { 60, 255, 130, 255 }, { 85, 70 }, Direction::West },
	{ EnemyPersonality::Erratic, { 110, 95, 5, 255 }, { 255, 235, 50, 255 }, { 85, 80 }, Direction::West }
};
}

LightcyclesGame::LightcyclesGame(SDL_Renderer* renderer)
	: renderer_(renderer),
	resPath_(getResourcePath()),
	player_(PLAYER_GLOW, PLAYER_CORE, &cycleAnimSet_, CELL_SIZE),
	occupied_(static_cast<std::size_t>(GRID_WIDTH * GRID_HEIGHT), 0) {
	for (const EnemySpec& spec : ENEMY_SPECS) {
		enemies_.emplace_back(spec.glow, spec.core, &cycleAnimSet_, CELL_SIZE);
		brains_.push_back(createEnemyBrain(spec.personality));
	}
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
	reset();
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
			reset();
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
	cameraController_.update(renderContext_.camera, WORLD_WIDTH * CELL_SIZE, WORLD_HEIGHT * CELL_SIZE);

	stepAccumulator_ += std::min(elapsedSeconds, 0.25f);
	while (result_ == GameResult::Running && stepAccumulator_ >= MOVE_STEP_SECONDS) {
		updateCycles();
		stepAccumulator_ -= MOVE_STEP_SECONDS;
	}
}

void LightcyclesGame::render() {
	SDL_SetRenderDrawColor(renderer_, 0, 0, 0, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(renderer_);

	const SDL_Rect& camera = renderContext_.camera;
	const SDL_Rect arena = { -camera.x, -camera.y, WORLD_WIDTH * CELL_SIZE, WORLD_HEIGHT * CELL_SIZE };
	SDL_SetRenderDrawColor(renderer_, 22, 32, 45, SDL_ALPHA_OPAQUE);
	SDL_RenderDrawRect(renderer_, &arena);

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
	return x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT;
}

bool LightcyclesGame::isOccupied(int x, int y) const {
	return !isInsideGrid(x, y) || occupied_[gridIndex(x, y)] != 0;
}

std::size_t LightcyclesGame::gridIndex(int x, int y) const {
	return static_cast<std::size_t>(y * GRID_WIDTH + x);
}

void LightcyclesGame::updateCycles() {
	const ArenaView arena{ GRID_WIDTH, GRID_HEIGHT, occupied_ };
	for (std::size_t index = 0; index < enemies_.size(); ++index) {
		if (enemies_[index].alive()) {
			enemies_[index].setDirection(brains_[index]->decide(enemies_[index], player_, arena));
		}
	}

	std::vector<PlannedMove> moves;
	if (player_.alive()) {
		moves.push_back({ &player_, player_.nextPosition(), false });
	}
	for (Cycle& enemy : enemies_) {
		if (enemy.alive()) {
			moves.push_back({ &enemy, enemy.nextPosition(), false });
		}
	}

	for (PlannedMove& move : moves) {
		move.crashed = isOccupied(move.destination.x, move.destination.y);
	}

	for (std::size_t first = 0; first < moves.size(); ++first) {
		for (std::size_t second = first + 1; second < moves.size(); ++second) {
			const SDL_Point firstPosition = moves[first].cycle->position();
			const SDL_Point secondPosition = moves[second].cycle->position();
			const bool sameCell = moves[first].destination.x == moves[second].destination.x
				&& moves[first].destination.y == moves[second].destination.y;
			const bool crossed = moves[first].destination.x == secondPosition.x
				&& moves[first].destination.y == secondPosition.y
				&& moves[second].destination.x == firstPosition.x
				&& moves[second].destination.y == firstPosition.y;
			if (sameCell || crossed) {
				moves[first].crashed = true;
				moves[second].crashed = true;
			}
		}
	}

	for (PlannedMove& move : moves) {
		if (move.crashed) {
			move.cycle->crash();
			continue;
		}
		move.cycle->advanceTo(move.destination);
		if (move.cycle->trail().size() > Cycle::MAX_TRAIL_LENGTH) {
			const SDL_Point expired = move.cycle->popOldestTrailPoint();
			occupied_[gridIndex(expired.x, expired.y)] = 0;
		}
		occupied_[gridIndex(move.destination.x, move.destination.y)] = 1;
	}

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
	std::fill(occupied_.begin(), occupied_.end(), 0);
	const SDL_Point playerStart = { GRID_WIDTH / 4, GRID_HEIGHT / 2 };
	player_.reset(playerStart, Direction::East);
	occupied_[gridIndex(playerStart.x, playerStart.y)] = 1;

	for (std::size_t index = 0; index < enemies_.size(); ++index) {
		const EnemySpec& spec = ENEMY_SPECS[index];
		const SDL_Point start = { GRID_WIDTH * spec.startPercent.x / 100,
			GRID_HEIGHT * spec.startPercent.y / 100 };
		enemies_[index].reset(start, spec.direction);
		occupied_[gridIndex(start.x, start.y)] = 1;
	}
	renderContext_.camera.x = std::clamp(static_cast<int>(player_.x) - WINDOW_WIDTH / 2,
		0, WORLD_WIDTH * CELL_SIZE - WINDOW_WIDTH);
	renderContext_.camera.y = std::clamp(static_cast<int>(player_.y) - WINDOW_HEIGHT / 2,
		0, WORLD_HEIGHT * CELL_SIZE - WINDOW_HEIGHT);
	result_ = GameResult::Running;
	stepAccumulator_ = 0;
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
