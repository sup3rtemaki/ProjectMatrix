#include "lightcyclesGame.h"

#include "randomNumber.h"

#include <algorithm>

namespace {
constexpr SDL_Color PLAYER_GLOW = { 0, 90, 135, 255 };
constexpr SDL_Color PLAYER_CORE = { 20, 235, 255, 255 };
constexpr SDL_Color RED_GLOW = { 105, 15, 30, 255 };
constexpr SDL_Color RED_CORE = { 255, 45, 75, 255 };
constexpr SDL_Color ORANGE_GLOW = { 105, 50, 5, 255 };
constexpr SDL_Color ORANGE_CORE = { 255, 155, 35, 255 };
constexpr SDL_Color PURPLE_GLOW = { 65, 20, 115, 255 };
constexpr SDL_Color PURPLE_CORE = { 185, 95, 255, 255 };
}

LightcyclesGame::LightcyclesGame(SDL_Renderer* renderer)
	: renderer_(renderer),
	player_(PLAYER_GLOW, PLAYER_CORE),
	enemies_({ Cycle(RED_GLOW, RED_CORE), Cycle(ORANGE_GLOW, ORANGE_CORE),
		Cycle(PURPLE_GLOW, PURPLE_CORE) }),
	occupied_(static_cast<std::size_t>(GRID_WIDTH * GRID_HEIGHT), 0) {
	keyboard_.UP = SDL_SCANCODE_W;
	keyboard_.DOWN = SDL_SCANCODE_S;
	keyboard_.LEFT = SDL_SCANCODE_A;
	keyboard_.RIGHT = SDL_SCANCODE_D;
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

	stepAccumulator_ += std::min(elapsedSeconds, 0.25f);
	while (result_ == GameResult::Running && stepAccumulator_ >= MOVE_STEP_SECONDS) {
		updateCycles();
		stepAccumulator_ -= MOVE_STEP_SECONDS;
	}
}

void LightcyclesGame::render() const {
	SDL_SetRenderDrawColor(renderer_, 0, 0, 0, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(renderer_);

	const SDL_Rect arena = { 0, GRID_TOP, WINDOW_WIDTH, GRID_HEIGHT * CELL_SIZE };
	SDL_SetRenderDrawColor(renderer_, 22, 32, 45, SDL_ALPHA_OPAQUE);
	SDL_RenderDrawRect(renderer_, &arena);

	for (const Cycle& enemy : enemies_) {
		drawCycle(enemy);
	}
	drawCycle(player_);

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

bool LightcyclesGame::isSafeTurn(const Cycle& cycle, Direction direction) const {
	const SDL_Point next = cycle.nextPosition(direction);
	return !isOccupied(next.x, next.y);
}

void LightcyclesGame::chooseEnemyDirection(Cycle& enemy) {
	const Direction forward = enemy.direction();
	const Direction left = static_cast<Direction>((static_cast<int>(forward) + 3) % 4);
	const Direction right = static_cast<Direction>((static_cast<int>(forward) + 1) % 4);
	const bool forwardSafe = isSafeTurn(enemy, forward);
	const bool leftSafe = isSafeTurn(enemy, left);
	const bool rightSafe = isSafeTurn(enemy, right);

	if (forwardSafe && getRandomNumber(100) < 78) {
		return;
	}

	if (leftSafe && rightSafe) {
		enemy.setDirection(getRandomNumber(2) == 0 ? left : right);
	}
	else if (leftSafe) {
		enemy.setDirection(left);
	}
	else if (rightSafe) {
		enemy.setDirection(right);
	}
}

void LightcyclesGame::updateCycles() {
	for (Cycle& enemy : enemies_) {
		if (enemy.alive()) {
			chooseEnemyDirection(enemy);
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
	player_.reset({ GRID_WIDTH / 4, GRID_HEIGHT / 2 }, Direction::East);
	occupied_[gridIndex(GRID_WIDTH / 4, GRID_HEIGHT / 2)] = 1;

	const SDL_Point starts[] = {
		{ GRID_WIDTH * 3 / 4, GRID_HEIGHT / 2 },
		{ GRID_WIDTH / 2, GRID_HEIGHT / 4 },
		{ GRID_WIDTH / 2, GRID_HEIGHT * 3 / 4 }
	};
	const Direction directions[] = {
		Direction::West,
		Direction::South,
		Direction::North
	};

	for (std::size_t index = 0; index < enemies_.size(); ++index) {
		enemies_[index].reset(starts[index], directions[index]);
		occupied_[gridIndex(starts[index].x, starts[index].y)] = 1;
	}
	result_ = GameResult::Running;
	stepAccumulator_ = 0;
}

void LightcyclesGame::drawCycle(const Cycle& cycle) const {
	for (const SDL_Point point : cycle.trail()) {
		const SDL_Rect glow = { point.x * CELL_SIZE, GRID_TOP + point.y * CELL_SIZE,
			CELL_SIZE, CELL_SIZE };
		setDrawColor(cycle.glowColor());
		SDL_RenderFillRect(renderer_, &glow);

		const SDL_Rect core = { glow.x + 3, glow.y + 3, CELL_SIZE - 6, CELL_SIZE - 6 };
		setDrawColor(cycle.coreColor());
		SDL_RenderFillRect(renderer_, &core);
	}

	if (!cycle.alive()) {
		return;
	}

	const SDL_Point position = cycle.position();
	const SDL_Rect bike = { position.x * CELL_SIZE + 1,
		GRID_TOP + position.y * CELL_SIZE + 1, CELL_SIZE - 2, CELL_SIZE - 2 };
	setDrawColor(cycle.coreColor());
	SDL_RenderFillRect(renderer_, &bike);
}

void LightcyclesGame::setDrawColor(SDL_Color color) const {
	SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
}
