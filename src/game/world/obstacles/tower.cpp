#include "world/obstacles/tower.h"

#include "groupBox.h"
#include "rendering/shapes.h"

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
	update();
	updateAttack(cycles);
	updateProjectiles(cycles);
}

void Tower::update() {
	updateCollisionBox();
	updateAnimation();
}

void Tower::draw(const RenderContext& context) {
	Entity::draw(context);

	SDL_SetRenderDrawColor(context.renderer, 255, 120, 30, SDL_ALPHA_OPAQUE);
	for (const Projectile& projectile : projectiles_) {
		shapes::fillCircle(context.renderer, static_cast<int>(projectile.position.x) - context.camera.x,
			static_cast<int>(projectile.position.y) - context.camera.y, PROJECTILE_DRAW_RADIUS);
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
		fireAt(*target);
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
	const float scale = PROJECTILE_SPEED / distance;
	const Vec2 toTarget = target.center() - origin;
	projectiles_.push_back({ origin, { toTarget.x * scale, toTarget.y * scale }, PROJECTILE_LIFETIME });
	changeAnimation(TOWER_STATE_IDLE, true);
}

void Tower::updateProjectiles(const std::vector<Cycle*>& cycles) {
	for (std::size_t index = 0; index < projectiles_.size();) {
		Projectile& projectile = projectiles_[index];
		projectile.position.x += projectile.velocity.x * deltaTime;
		projectile.position.y += projectile.velocity.y * deltaTime;
		projectile.remainingSeconds -= deltaTime;

		bool spent = projectile.remainingSeconds <= 0.0f;
		for (Cycle* cycle : cycles) {
			if (cycle != nullptr && isTargetable(*cycle)
				&& geom::distance(projectile.position, cycle->center()) <= PROJECTILE_HIT_RADIUS) {
				cycle->crash();
				spent = true;
				break;
			}
		}

		if (spent) {
			projectiles_.erase(projectiles_.begin() + static_cast<std::ptrdiff_t>(index));
		}
		else {
			++index;
		}
	}
}
