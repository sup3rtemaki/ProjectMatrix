#include "world/projectiles/projectile.h"

#include "groupBox.h"

#include <algorithm>
#include <cmath>

const char* const Projectile::COLLISION_BOX_GROUP = "collisionBox1";

Projectile::Projectile(const ProjectileConfig& config, AnimationSet* animSet, Vec2 origin, Vec2 direction)
	: config_(config), velocity_{ direction.x * config.speed, direction.y * config.speed },
	remainingSeconds_(config.lifetime) {
	this->animSet = animSet;
	currentAnim = nullptr;
	currentFrame = nullptr;
	frameTimer = 0;
	solid = false;
	collideWithSolids = false;
	type = "projectile";
	x = origin.x;
	y = origin.y;
	collisionBox = { 0, 0, 0, 0 };
	collisionBoxW = 0;
	collisionBoxH = 0;
	changeAnimation(0, true);
}

void Projectile::tick(float elapsedSeconds, const std::vector<Cycle*>& cycles, const SolidQuery& isSolid) {
	const float speed = std::sqrt(velocity_.x * velocity_.x + velocity_.y * velocity_.y);
	const int steps = std::max(1, static_cast<int>(std::ceil(speed * elapsedSeconds / MAX_STEP_PIXELS)));
	deltaTime = elapsedSeconds / steps;

	for (int step = 0; step < steps && active; ++step) {
		update();
		if (!active) {
			return;
		}

		if (isSolid && isSolid(collisionBox)) {
			active = false;
			return;
		}
		for (Cycle* cycle : cycles) {
			if (cycle != nullptr && cycle->alive() && !cycle->isRespawning() && !cycle->isEliminated()
				&& hits(*cycle)) {
				cycle->crash();
				active = false;
				return;
			}
		}
	}
}

void Projectile::update() {
	x += velocity_.x * deltaTime;
	y += velocity_.y * deltaTime;
	remainingSeconds_ -= deltaTime;
	if (remainingSeconds_ <= 0.0f) {
		active = false;
		return;
	}
	updateAnimation();
	updateCollisionBox();
}

void Projectile::updateCollisionBox() {
	collisionBox.x = static_cast<int>(std::lround(x)) + boxOffsetX_;
	collisionBox.y = static_cast<int>(std::lround(y)) + boxOffsetY_;
	collisionBox.w = collisionBoxW;
	collisionBox.h = collisionBoxH;
}

void Projectile::applyCollisionBoxFromCurrentFrame() {
	collisionBoxW = 0;
	collisionBoxH = 0;
	boxOffsetX_ = 0;
	boxOffsetY_ = 0;

	if (currentFrame != nullptr) {
		for (Group* group : currentFrame->frameData) {
			if (group == nullptr || group->type.groupName != COLLISION_BOX_GROUP) {
				continue;
			}
			GroupBox* boxGroup = dynamic_cast<GroupBox*>(group);
			if (boxGroup == nullptr || boxGroup->data.empty()) {
				continue;
			}
			const SDL_Rect& box = boxGroup->data.front();
			collisionBoxW = box.w;
			collisionBoxH = box.h;
			boxOffsetX_ = box.x - currentFrame->offset.x;
			boxOffsetY_ = box.y - currentFrame->offset.y;
			break;
		}
	}
	updateCollisionBox();
}

void Projectile::changeAnimation(int newState, bool resetFrameToBeginning, string animName) {
	state = newState;
	if (animSet == nullptr) {
		return;
	}
	currentAnim = animSet->getAnimation(animName.empty() ? config_.animName : animName);
	if (currentAnim == nullptr) {
		currentFrame = nullptr;
		return;
	}
	if (resetFrameToBeginning || currentFrame == nullptr) {
		currentFrame = currentAnim->getFrame(0);
		frameTimer = 0;
	}
	applyCollisionBoxFromCurrentFrame();
}

void Projectile::updateAnimation() {
	if (currentFrame == nullptr || currentAnim == nullptr) {
		return;
	}
	frameTimer += deltaTime;
	if (frameTimer < currentFrame->duration) {
		return;
	}
	currentFrame = currentFrame->frameNumber == currentAnim->getEndFrameNumber()
		? currentAnim->getFrame(0)
		: currentAnim->getNextFrame(currentFrame);
	frameTimer = 0;
	applyCollisionBoxFromCurrentFrame();
}

// A cycle is hit when its head is inside the projectile's collision box.
bool Projectile::hits(const Cycle& cycle) const {
	const SDL_Point head = { static_cast<int>(std::lround(cycle.center().x)),
		static_cast<int>(std::lround(cycle.center().y)) };
	return SDL_PointInRect(&head, &collisionBox) == SDL_TRUE;
}
