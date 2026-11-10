#pragma once

#include "cycle.h"
#include "entity.h"
#include "world/solidQuery.h"

#include <string>
#include <vector>

// Shot that flies straight until it hits a cycle, a solid or its lifetime ends. Type specifics come from ProjectileConfig.
struct ProjectileConfig {
	std::string animFile;
	std::string animName = "idle";
	float speed = 220.0f;       // pixels per second
	float lifetime = 2.0f;      // seconds
};

class Projectile : public Entity {
public:
	static const char* const COLLISION_BOX_GROUP;

	// animSet is shared and owned by the factory.
	Projectile(const ProjectileConfig& config, AnimationSet* animSet, Vec2 origin, Vec2 direction);

	// Moves in small steps so a fast shot cannot skip over a thin wall.
	void tick(float elapsedSeconds, const std::vector<Cycle*>& cycles, const SolidQuery& isSolid);
	void update() override;
	void updateCollisionBox() override;
	void changeAnimation(int newState, bool resetFrameToBeginning, string animName = "") override;

	bool expired() const { return !active; }

private:
	friend class Tower;
	static constexpr float MAX_STEP_PIXELS = 4.0f;

	void applySnapshot(const SnapshotProjectile& snapshot);
	void updateAnimation();
	void applyCollisionBoxFromCurrentFrame();
	bool hits(const Cycle& cycle) const;

	ProjectileConfig config_;
	Vec2 velocity_;
	float remainingSeconds_;
	int boxOffsetX_ = 0; // collision box relative to the pivot
	int boxOffsetY_ = 0;
};
