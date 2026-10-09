#pragma once

#include "SDL.h"

#include <vector>

enum class Direction {
	North,
	East,
	South,
	West
};

class Cycle {
public:
	Cycle(SDL_Color glowColor, SDL_Color coreColor);

	void reset(SDL_Point start, Direction direction);
	void setDirection(Direction direction);
	void setDirectionFromAngle(float angle);
	void advanceTo(SDL_Point destination);
	void crash();

	SDL_Point position() const;
	Direction direction() const;
	SDL_Point nextPosition() const;
	SDL_Point nextPosition(Direction direction) const;
	const std::vector<SDL_Point>& trail() const;
	SDL_Color glowColor() const;
	SDL_Color coreColor() const;
	bool alive() const;

private:
	static SDL_Point directionOffset(Direction direction);
	static Direction turnLeft(Direction direction);
	static Direction turnRight(Direction direction);

	SDL_Point position_{};
	Direction direction_ = Direction::East;
	std::vector<SDL_Point> trail_;
	SDL_Color glowColor_{};
	SDL_Color coreColor_{};
	bool alive_ = true;
};
