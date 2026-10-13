#pragma once

#include "effects/cycleEffects.h"
#include "livingEntity.h"

#include <cstddef>
#include <deque>

enum class Direction {
	North,
	East,
	South,
	West
};

class Cycle : public LivingEntity {
public:
	static constexpr std::size_t MAX_TRAIL_LENGTH = 40;
	static constexpr int SPRITE_CELLS = 6; // sprite is drawn this many grid cells wide

	// animSet is shared and owned by the caller.
	Cycle(SDL_Color glowColor, SDL_Color coreColor, AnimationSet* animSet, int cellSize);

	void reset(SDL_Point start, Direction direction);
	void setDirection(Direction direction);
	void setDirectionFromAngle(float angle);
	void advance(float distance); // moves along the current direction, in pixels
	void addTrailCell(SDL_Point cell);
	SDL_Point popOldestTrailPoint();
	void crash();

	void draw(const RenderContext& context) override;
	void changeAnimation(int newState, bool resetFrameToBeginning, string animName = "") override;
	void updateDamages() override {}
	void die() override { crash(); }

	SDL_Point position() const; // grid cell under the cycle's head
	Direction direction() const;
	SDL_Point nextPosition() const;
	SDL_Point nextPosition(Direction direction) const;
	const std::deque<SDL_Point>& trail() const;
	SDL_Color glowColor() const;
	SDL_Color coreColor() const;
	bool alive() const;

	CycleEffects& effects() { return effects_; }
	std::size_t maxTrailLength() const { return MAX_TRAIL_LENGTH + effects_.trailBonus(); }

private:
	static SDL_Point directionOffset(Direction direction);
	static Direction turnLeft(Direction direction);
	static Direction turnRight(Direction direction);
	static const char* animationName(Direction direction);

	Direction direction_ = Direction::East;
	std::deque<SDL_Point> trail_;
	CycleEffects effects_;
	SDL_Color glowColor_{};
	SDL_Color coreColor_{};
	int cellSize_;
	bool alive_ = true;
};
