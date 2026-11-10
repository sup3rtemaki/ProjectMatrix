#include "world/obstacles/tower.h"

#include "groupBox.h"
#include "rendering/shapes.h"

#include <algorithm>
#include <list>

const int Tower::TOWER_STATE_IDLE = 0;
const int Tower::TOWER_STATE_ATTACK = 1;

const std::string Tower::TOWER_ANIM_IDLE = "idle";
const std::string Tower::TOWER_ANIM_ATTACK = "attack";

namespace {
const char* const COLLISION_BOX_GROUP = "collisionBox1";
}

Tower::Tower(SDL_Rect cells, int cellSize, SDL_Renderer* renderer, const std::string& animFile)
	: WorldObject(cells, cellSize) {
	std::list<DataGroupType> dataGroupTypes;
	DataGroupType collisionBoxType;
	collisionBoxType.groupName = COLLISION_BOX_GROUP;
	collisionBoxType.dataType = DataGroupType::DATATYPE_BOX;
	dataGroupTypes.push_back(collisionBoxType);

	localAnimSet_ = std::make_unique<AnimationSet>();
	localAnimSet_->loadAnimationSet(animFile, dataGroupTypes, renderer);
	projectileFactory_ = std::make_unique<ProjectileFactory>(renderer);
	animSet = localAnimSet_.get();
	state = TOWER_STATE_IDLE;
	solid = true;
	collideWithSolids = false;
	collisionBoxW = 0;
	collisionBoxH = 0;
	collisionBox = { 0, 0, 0, 0 };

	// Put the sprite's top-left corner on the Tiled object by moving the pivot to the frame offset.
	Animation* idle = animSet->getAnimation(TOWER_ANIM_IDLE);
	Frame* firstFrame = (idle == nullptr) ? nullptr : idle->getFrame(0);
	if (firstFrame == nullptr) {
		return;
	}
	x = static_cast<float>(cells.x * cellSize + firstFrame->offset.x);
	y = static_cast<float>(cells.y * cellSize + firstFrame->offset.y);
	changeAnimation(TOWER_STATE_IDLE, true);

	if (collisionBoxW > 0 && collisionBoxH > 0) {
		const int left = collisionBox.x / cellSize;
		const int top = collisionBox.y / cellSize;
		cells_ = { left, top, (collisionBox.x + collisionBox.w + cellSize - 1) / cellSize - left,
			(collisionBox.y + collisionBox.h + cellSize - 1) / cellSize - top };
	}
}

void Tower::tick(float elapsedSeconds, const std::vector<Cycle*>& cycles) {
	deltaTime = elapsedSeconds;
	if (target != nullptr && std::find(cycles.begin(), cycles.end(), target) == cycles.end()) {
		target = nullptr;
	}
	update();
	updateAttack(cycles);
	updateProjectiles(cycles);
}

void Tower::update() {
	updateCollisionBox();
	updateAnimation();
}

void Tower::drawProjectiles(const RenderContext& context) {
	for (const std::unique_ptr<Projectile>& projectile : projectiles_) {
		projectile->draw(context);
	}
}

SnapshotTower Tower::snapshot(const SDL_Rect& placement) const {
	SnapshotTower result;
	result.placement = { static_cast<std::uint32_t>(placement.x), static_cast<std::uint32_t>(placement.y),
		static_cast<std::uint32_t>(placement.w), static_cast<std::uint32_t>(placement.h) };
	result.attacking = state == TOWER_STATE_ATTACK;
	result.animationFrame = currentFrame == nullptr ? 0 : static_cast<std::uint32_t>(currentFrame->frameNumber);
	result.projectiles.reserve(projectiles_.size());
	for (const auto& projectile : projectiles_) {
		if (!projectile->expired()) {
			result.projectiles.push_back({ { projectile->x, projectile->y },
				projectile->currentFrame == nullptr ? 0u : static_cast<std::uint32_t>(projectile->currentFrame->frameNumber) });
		}
	}
	return result;
}

bool Tower::canApplySnapshot(const SnapshotTower& snapshot) {
	Animation* animation = animSet == nullptr ? nullptr
		: animSet->getAnimation(snapshot.attacking ? TOWER_ANIM_ATTACK : TOWER_ANIM_IDLE);
	if (animation == nullptr || snapshot.animationFrame >= animation->frames.size()) return false;
	if (!snapshot.projectiles.empty()) {
		std::unique_ptr<Projectile> probe;
		const Projectile* sample = projectiles_.empty() ? nullptr : projectiles_.front().get();
		if (sample == nullptr) {
			probe = projectileFactory_->create(ProjectileType::Triangle, { 0.0f, 0.0f }, { 0.0f, 0.0f });
			sample = probe.get();
		}
		if (sample->currentAnim == nullptr) return false;
		for (const SnapshotProjectile& projectile : snapshot.projectiles) {
			if (projectile.animationFrame >= sample->currentAnim->frames.size()) return false;
		}
	}
	return true;
}

void Tower::applySnapshot(const SnapshotTower& snapshot) {
	changeAnimation(snapshot.attacking ? TOWER_STATE_ATTACK : TOWER_STATE_IDLE, true);
	currentFrame = currentAnim->getFrame(static_cast<int>(snapshot.animationFrame));
	frameTimer = 0.0f;
	target = nullptr;
	applyCollisionBoxFromCurrentFrame();
	while (projectiles_.size() > snapshot.projectiles.size()) projectiles_.pop_back();
	while (projectiles_.size() < snapshot.projectiles.size()) {
		projectiles_.push_back(projectileFactory_->create(ProjectileType::Triangle, { 0.0f, 0.0f }, { 0.0f, 0.0f }));
	}
	for (std::size_t i = 0; i < projectiles_.size(); ++i) {
		projectiles_[i]->applySnapshot(snapshot.projectiles[i]);
	}
}

void Tower::changeAnimation(int newState, bool resetFrameToBeginning, string animName) {
	state = newState;
	if (animSet == nullptr) {
		return;
	}

	if (!animName.empty()) {
		currentAnim = animSet->getAnimation(animName);
	}
	else if (state == TOWER_STATE_ATTACK) {
		currentAnim = animSet->getAnimation(TOWER_ANIM_ATTACK);
	}
	else {
		currentAnim = animSet->getAnimation(TOWER_ANIM_IDLE);
	}

	if (currentAnim == nullptr) {
		currentFrame = nullptr;
		return;
	}

	if (resetFrameToBeginning || currentFrame == nullptr) {
		currentFrame = currentAnim->getFrame(0);
		frameTimer = 0;
	}
	else {
		currentFrame = currentAnim->getFrame(currentFrame->frameNumber);
	}
	applyCollisionBoxFromCurrentFrame();
}

void Tower::updateAnimation() {
	if (currentFrame == nullptr || currentAnim == nullptr) {
		return;
	}

	frameTimer += deltaTime;
	if (frameTimer < currentFrame->duration) {
		return;
	}

	if (currentFrame->frameNumber != currentAnim->getEndFrameNumber()) {
		currentFrame = currentAnim->getNextFrame(currentFrame);
	}
	else if (state == TOWER_STATE_ATTACK) {
		if (target != nullptr) {
			fireAt(*target);
		}
		changeAnimation(TOWER_STATE_IDLE, true);
	}
	else {
		currentFrame = currentAnim->getFrame(0);
	}
	frameTimer = 0;
	applyCollisionBoxFromCurrentFrame();
}

void Tower::updateCollisionBox() {
	collisionBox.x = static_cast<int>(x) + boxOffsetX_;
	collisionBox.y = static_cast<int>(y) + boxOffsetY_;
	collisionBox.w = collisionBoxW;
	collisionBox.h = collisionBoxH;
}

void Tower::applyCollisionBoxFromCurrentFrame() {
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

bool Tower::isTargetable(const Cycle& cycle) {
	return cycle.alive() && !cycle.isRespawning() && !cycle.isEliminated();
}

// Shots leave from the middle of the sprite.
Vec2 Tower::muzzle() const {
	if (currentFrame == nullptr) {
		return { x, y };
	}
	return { x - currentFrame->offset.x + currentFrame->clip.w / 2.0f,
		y - currentFrame->offset.y + currentFrame->clip.h / 2.0f };
}

Cycle* Tower::nearestCycleInRange(const std::vector<Cycle*>& cycles) const {
	const Vec2 origin = muzzle();
	Cycle* nearest = nullptr;
	float nearestDistance = FIRE_RANGE;
	for (Cycle* cycle : cycles) {
		if (cycle == nullptr || !isTargetable(*cycle)) {
			continue;
		}
		const float distance = geom::distance(origin, cycle->center());
		if (distance <= nearestDistance) {
			nearest = cycle;
			nearestDistance = distance;
		}
	}
	return nearest;
}

void Tower::updateAttack(const std::vector<Cycle*>& cycles) {
	if (currentAnim->name != TOWER_ANIM_ATTACK) {
		target = nearestCycleInRange(cycles);
		if (target == nullptr) {
			chargeSeconds_ = 0.0f;
			return;
		}
	}

	chargeSeconds_ += deltaTime;
	if (chargeSeconds_ >= FIRE_INTERVAL_SECONDS) {
		chargeSeconds_ = 0.0f;
		changeAnimation(TOWER_STATE_ATTACK, true);
		//fireAt(*target);
	}
}

// The shot flies straight at where the target is now, so a moving cycle can dodge it.
void Tower::fireAt(const Cycle& target) {
	const Vec2 origin = muzzle();
	const float distance = geom::distance(origin, target.center());
	if (distance <= 0.0f) {
		return;
	}
	const Vec2 toTarget = target.center() - origin;
	projectiles_.push_back(projectileFactory_->create(ProjectileType::Triangle, origin,
		{ toTarget.x / distance, toTarget.y / distance }));
}

void Tower::updateProjectiles(const std::vector<Cycle*>& cycles) {
	for (const std::unique_ptr<Projectile>& projectile : projectiles_) {
		projectile->tick(deltaTime, cycles, solidQuery_);
	}
	projectiles_.erase(std::remove_if(projectiles_.begin(), projectiles_.end(),
		[](const std::unique_ptr<Projectile>& projectile) { return projectile->expired(); }), projectiles_.end());
}