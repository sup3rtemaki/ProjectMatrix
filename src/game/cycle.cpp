#include "cycle.h"

#include <cmath>

Cycle::Cycle(SDL_Color glowColor, SDL_Color coreColor, AnimationSet* animSet, int cellSize)
	: glowColor_(glowColor), coreColor_(coreColor), cellSize_(cellSize) {
	this->animSet = animSet;
	currentAnim = nullptr;
	currentFrame = nullptr;
	solid = false;
	collideWithSolids = false;
	x = 0;
	y = 0;
	frameTimer = 0;
	changeAnimation(0, true);
}

void Cycle::reset(SDL_Point start, Direction direction) {
	position_ = start;
	direction_ = direction;
	trail_.clear();
	trail_.push_back(start);
	alive_ = true;
	updatePixelPosition();
	changeAnimation(0, true);
}

void Cycle::setDirection(Direction direction) {
	if (direction != turnLeft(turnLeft(direction_)) && direction != direction_) {
		direction_ = direction;
		changeAnimation(0, true);
	}
}

void Cycle::changeAnimation(int newState, bool, string animName) {
	state = newState;
	if (animSet == nullptr) {
		return;
	}
	currentAnim = animSet->getAnimation(animName.empty() ? animationName(direction_) : animName);
	currentFrame = currentAnim != nullptr ? currentAnim->getFrame(0) : nullptr;
}

void Cycle::draw(const RenderContext& context) {
	if (!alive_) {
		return;
	}

	const int size = cellSize_ * SPRITE_CELLS;
	if (currentFrame == nullptr) {
		// Animation set missing: keep the cycle visible as a plain square.
		const SDL_Rect fallback = { static_cast<int>(x) - context.camera.x - cellSize_ / 2,
			static_cast<int>(y) - context.camera.y - cellSize_ / 2, cellSize_, cellSize_ };
		SDL_SetRenderDrawColor(context.renderer, coreColor_.r, coreColor_.g, coreColor_.b, coreColor_.a);
		SDL_RenderFillRect(context.renderer, &fallback);
		return;
	}

	const SDL_Rect& clip = currentFrame->clip;
	const SDL_Rect dest = {
		static_cast<int>(x) - context.camera.x - currentFrame->offset.x * size / clip.w,
		static_cast<int>(y) - context.camera.y - currentFrame->offset.y * size / clip.h,
		size, size };
	SDL_RenderCopy(context.renderer, animSet->spriteSheet, &clip, &dest);
}

void Cycle::setDirectionFromAngle(float angle) {
	const int degrees = static_cast<int>(std::lround(angle)) % 360;
	switch (degrees) {
	case 0: setDirection(Direction::East); break;
	case 90: setDirection(Direction::South); break;
	case 180: setDirection(Direction::West); break;
	case 270: setDirection(Direction::North); break;
	default: break;
	}
}

void Cycle::advanceTo(SDL_Point destination) {
	position_ = destination;
	trail_.push_back(destination);
	updatePixelPosition();
}

SDL_Point Cycle::popOldestTrailPoint() {
	const SDL_Point oldest = trail_.front();
	trail_.pop_front();
	return oldest;
}

void Cycle::crash() {
	alive_ = false;
}

SDL_Point Cycle::position() const {
	return position_;
}

Direction Cycle::direction() const {
	return direction_;
}

SDL_Point Cycle::nextPosition() const {
	return nextPosition(direction_);
}

SDL_Point Cycle::nextPosition(Direction direction) const {
	const SDL_Point offset = directionOffset(direction);
	return { position_.x + offset.x, position_.y + offset.y };
}

const std::deque<SDL_Point>& Cycle::trail() const {
	return trail_;
}

SDL_Color Cycle::glowColor() const {
	return glowColor_;
}

SDL_Color Cycle::coreColor() const {
	return coreColor_;
}

bool Cycle::alive() const {
	return alive_;
}

SDL_Point Cycle::directionOffset(Direction direction) {
	switch (direction) {
	case Direction::North: return { 0, -1 };
	case Direction::East: return { 1, 0 };
	case Direction::South: return { 0, 1 };
	case Direction::West: return { -1, 0 };
	}
	return { 0, 0 };
}

Direction Cycle::turnLeft(Direction direction) {
	return static_cast<Direction>((static_cast<int>(direction) + 3) % 4);
}

Direction Cycle::turnRight(Direction direction) {
	return static_cast<Direction>((static_cast<int>(direction) + 1) % 4);
}

const char* Cycle::animationName(Direction direction) {
	switch (direction) {
	case Direction::North: return "up";
	case Direction::East: return "right";
	case Direction::South: return "down";
	case Direction::West: return "left";
	}
	return "right";
}

void Cycle::updatePixelPosition() {
	x = static_cast<float>(position_.x * cellSize_ + cellSize_ / 2);
	y = static_cast<float>(position_.y * cellSize_ + cellSize_ / 2);
}
