#pragma once

#include "collision/geometry2D.h"
#include "effects/cycleEffects.h"
#include "livingEntity.h"
#include "trail/continuousTrail.h"

#include <cstddef>

enum class Direction {
	North,
	East,
	South,
	West
};

class Cycle : public LivingEntity {
public:
	static constexpr std::size_t MAX_TRAIL_LENGTH = 40;
	static constexpr int SPRITE_CELLS = 10; // sprite and its collisionBox are scaled to this many grid cells wide
	static constexpr const char* COLLISION_BOX_GROUPS[] = { "collisionBox1", "collisionBox2" };
	static constexpr int INITIAL_LIVES = 3;
	static constexpr float BASE_SPEED = 160.0f; // pixels per second, before boosts
	static constexpr float MIN_SPEED = 90.0f;
	static constexpr float MAX_SPEED = 220.0f;
	static constexpr float ACCELERATION = 100.0f;
	static constexpr float BRAKING = 140.0f;
	static constexpr float TURN_SPEED = 180.0f; // degrees per second
	static constexpr float TRAIL_CURVE_SAMPLE_DISTANCE = 4.0f;
	static constexpr float TURN_GRACE_DISTANCE = 12.0f; // pixels travelled after a turn before the nose box counts for terrain

	// animSet is shared and owned by the caller.
	Cycle(SDL_Color glowColor, SDL_Color coreColor, AnimationSet* animSet, int cellSize);

	void reset(SDL_Point start, Direction direction);
	void respawn(SDL_Point start, Direction direction);
	void setDirection(Direction direction);
	void setDirectionFromAngle(float angle);
	void steer(float steering, float throttle, float elapsedSeconds);
	void advance(float distance); // moves along the current direction, in pixels
	void clearTrail();
	void crash();

	void draw(const RenderContext& context) override;
	void changeAnimation(int newState, bool resetFrameToBeginning, string animName = "") override;
	void updateAnimation(float elapsedSeconds); // advances and loops the current animation
	void updateDamages() override {}
	void die() override { crash(); }

	SDL_Point position() const; // grid cell under the cycle's head
	Vec2 center() const { return { x, y }; }
	// World-space collisionBoxes of the current frame (collisionBox1, collisionBox2), scaled like the sprite.
	static constexpr std::size_t MAX_BOXES = 2;
	std::size_t boxCount() const { return boxCount_; }
	OrientedBox boxAt(std::size_t index, Vec2 head) const;
	OrientedBox terrainBoxAt(Vec2 head) const;
	bool overlaps(const RectF& rect) const;
	bool overlaps(const Cycle& other) const;
	float headingDegrees() const { return headingDegrees_; }
	Vec2 headingVector() const;
	Direction direction() const;
	SDL_Point nextPosition() const;
	SDL_Point nextPosition(Direction direction) const;
	const ContinuousTrail& trail() const { return trail_; }
	ContinuousTrail& trail() { return trail_; }
	SDL_Color glowColor() const;
	SDL_Color coreColor() const;
	bool alive() const;

	int lives() const { return lives_; }
	void setLives(int lives) { lives_ = lives; }
	void loseLife() { if (lives_ > 0) --lives_; }
	bool isEliminated() const { return lives_ <= 0; }

	int score() const { return score_; }
	void addScore(int points = 1) { score_ += points; }
	void setScore(int score) { score_ = score; }

	bool isRespawning() const { return respawnTimer_ > 0.0f; }
	void startRespawn(float seconds) { respawnTimer_ = seconds; }
	void updateRespawn(float dt) { if (respawnTimer_ > 0.0f) respawnTimer_ -= dt; }

	// Current speed in pixels per second, including active boosts.
	float speed() const { return currentSpeed_ * effects_.speedMultiplier(); }
	float baseSpeed() const { return currentSpeed_; }

	CycleEffects& effects() { return effects_; }
	float maxPixelTrailLength() const { return static_cast<float>((MAX_TRAIL_LENGTH + effects_.trailBonus()) * cellSize_); }

private:
	static SDL_Point directionOffset(Direction direction);
	static Direction turnLeft(Direction direction);
	static Direction turnRight(Direction direction);
	static const char* animationName(Direction direction);
	void refreshBounds();

	RectF localBoxes_[MAX_BOXES]{}; // collisionBoxes relative to the head, in pixels
	std::size_t boxCount_ = 0;
	float turnGraceRemaining_ = 0.0f;
	float headingDegrees_ = 0.0f;
	float currentSpeed_ = BASE_SPEED;
	Vec2 lastCurveSample_{};
	bool turning_ = false;
	Direction direction_ = Direction::East;
	ContinuousTrail trail_;
	CycleEffects effects_;
	SDL_Color glowColor_{};
	SDL_Color coreColor_{};
	int cellSize_;
	int lives_ = INITIAL_LIVES;
	int score_ = 0;
	float respawnTimer_ = 0.0f;
	bool alive_ = true;
};
