#include "cycle.h"

#include <cmath>

Cycle::Cycle(SDL_Color glowColor, SDL_Color coreColor)
	: glowColor_(glowColor), coreColor_(coreColor) {
}

void Cycle::reset(SDL_Point start, Direction direction) {
	position_ = start;
	direction_ = direction;
	trail_.clear();
	trail_.push_back(start);
	alive_ = true;
}

void Cycle::setDirection(Direction direction) {
	if (direction != turnLeft(turnLeft(direction_))) {
		direction_ = direction;
	}
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

const std::vector<SDL_Point>& Cycle::trail() const {
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
