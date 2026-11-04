#pragma once

#include "world/projectiles/projectileFactory.h"
#include "world/worldObject.h"

#include <memory>
#include <string>
#include <vector>

// Static tower; cells is where its Tiled tile object sits, and the fdset gives the sprite pivot and collision box.
class Tower : public WorldObject {
public:
	static const int TOWER_STATE_IDLE;
	static const int TOWER_STATE_ATTACK;

	static const std::string TOWER_ANIM_IDLE;
	static const std::string TOWER_ANIM_ATTACK;

	static constexpr float FIRE_RANGE = 320.0f;           // pixels
	static constexpr float FIRE_INTERVAL_SECONDS = 3.0f;  // from a cycle entering the range to the shot, and between shots

	Tower(SDL_Rect cells, int cellSize, SDL_Renderer* renderer, const std::string& animFile = "animations/tower_1.fdset");

	void onTouch(Cycle& cycle) override { cycle.crash(); }
	bool isDangerous() const override { return true; }
	bool isConsumedOnTouch() const override { return false; }

	void tick(float elapsedSeconds, const std::vector<Cycle*>& cycles) override;
	void update() override;
	void draw(const RenderContext& context) override { Entity::draw(context); }
	void drawProjectiles(const RenderContext& context) override;
	void changeAnimation(int newState, bool resetFrameToBeginning, string animName = "") override;
	void updateAnimation();
	void updateCollisionBox() override;

private:
	static bool isTargetable(const Cycle& cycle);

	void applyCollisionBoxFromCurrentFrame();
	Vec2 muzzle() const;
	Cycle* nearestCycleInRange(const std::vector<Cycle*>& cycles) const;
	void updateAttack(const std::vector<Cycle*>& cycles);
	void fireAt(const Cycle& target);
	void updateProjectiles(const std::vector<Cycle*>& cycles);

	std::unique_ptr<AnimationSet> localAnimSet_;
	int boxOffsetX_ = 0; // collision box relative to the pivot
	int boxOffsetY_ = 0;
	float chargeSeconds_ = 0.0f;
	std::unique_ptr<ProjectileFactory> projectileFactory_;
	std::vector<std::unique_ptr<Projectile>> projectiles_;
	Cycle* target = nullptr;
};
