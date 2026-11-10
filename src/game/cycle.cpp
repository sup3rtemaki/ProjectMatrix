#include "cycle.h"

#include "groupBox.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace {
constexpr float PI = 3.14159265358979323846f;

float directionAngle(Direction direction) {
	switch (direction) {
	case Direction::East: return 0.0f;
	case Direction::South: return 90.0f;
	case Direction::West: return 180.0f;
	case Direction::North: return 270.0f;
	}
	return 0.0f;
}
}

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
	x = static_cast<float>(start.x * cellSize_ + cellSize_ / 2);
	y = static_cast<float>(start.y * cellSize_ + cellSize_ / 2);
	direction_ = direction;
	headingDegrees_ = directionAngle(direction);
	currentSpeed_ = BASE_SPEED;
	turnGraceRemaining_ = 0.0f;
	turning_ = false;
	trail_.reset({ x, y });
	effects_.clear();
	alive_ = true;
	lives_ = INITIAL_LIVES;
	respawnTimer_ = 0.0f;
	changeAnimation(0, true);
}

void Cycle::respawn(SDL_Point start, Direction direction) {
	x = static_cast<float>(start.x * cellSize_ + cellSize_ / 2);
	y = static_cast<float>(start.y * cellSize_ + cellSize_ / 2);
	direction_ = direction;
	headingDegrees_ = directionAngle(direction);
	currentSpeed_ = BASE_SPEED;
	turnGraceRemaining_ = 0.0f;
	turning_ = false;
	trail_.reset({ x, y });
	effects_.clear();
	alive_ = true;
	respawnTimer_ = 0.0f;
	changeAnimation(0, true);
}

void Cycle::clearTrail() {
	trail_.clear();
}

void Cycle::setDirection(Direction direction) {
	if (direction != turnLeft(turnLeft(direction_)) && direction != direction_) {
		trail_.onTurn({ x, y });
		direction_ = direction;
		headingDegrees_ = directionAngle(direction);
		turnGraceRemaining_ = TURN_GRACE_DISTANCE;
		changeAnimation(0, true);
	}
}

void Cycle::changeAnimation(int newState, bool, string animName) {
	state = newState;
	if (animSet == nullptr) {
		refreshBounds();
		return;
	}
	currentAnim = animSet->getAnimation(animName.empty() ? animationName(direction_) : animName);
	currentFrame = currentAnim != nullptr ? currentAnim->getFrame(0) : nullptr;
	frameTimer = 0;
	refreshBounds();
}

// Without collisionBoxes (or a sprite) the cycle falls back to a one-cell square around its head.
void Cycle::refreshBounds() {
	const float half = cellSize_ / 2.0f;
	localBoxes_[0] = { -half, -half, static_cast<float>(cellSize_), static_cast<float>(cellSize_) };
	boxCount_ = 1;
	if (currentFrame == nullptr) {
		return;
	}

	const float size = static_cast<float>(cellSize_ * SPRITE_CELLS);
	const float scaleX = size / currentFrame->clip.w;
	const float scaleY = size / currentFrame->clip.h;
	std::size_t found = 0;
	for (const char* groupName : COLLISION_BOX_GROUPS) {
		for (Group* group : currentFrame->frameData) {
			if (group == nullptr || group->type.groupName != groupName) {
				continue;
			}
			const GroupBox* boxGroup = dynamic_cast<GroupBox*>(group);
			if (boxGroup == nullptr || boxGroup->data.empty()) {
				continue;
			}
			const SDL_Rect& box = boxGroup->data.front();
			localBoxes_[found++] = { (box.x - currentFrame->offset.x) * scaleX, (box.y - currentFrame->offset.y) * scaleY,
				box.w * scaleX, box.h * scaleY };
			break;
		}
	}
	if (found > 0) {
		boxCount_ = found;
	}
}

void Cycle::updateAnimation(float elapsedSeconds) {
	if (currentAnim == nullptr || currentFrame == nullptr || !alive_ || isRespawning()) {
		return;
	}

	const Frame* previous = currentFrame;
	frameTimer += elapsedSeconds;
	while (currentFrame->duration > 0 && frameTimer >= currentFrame->duration) {
		frameTimer -= currentFrame->duration;
		currentFrame = currentFrame->frameNumber != currentAnim->getEndFrameNumber()
			? currentAnim->getNextFrame(currentFrame)
			: currentAnim->getFrame(0);
	}
	if (currentFrame != previous) {
		refreshBounds();
	}
}

void Cycle::draw(const RenderContext& context) {
	if (!alive_ || isRespawning()) {
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
		static_cast<int>(std::lround(x)) - context.camera.x - currentFrame->offset.x * size / clip.w,
		static_cast<int>(std::lround(y)) - context.camera.y - currentFrame->offset.y * size / clip.h,
		size, size };
	const SDL_Point pivot = { currentFrame->offset.x * size / clip.w, currentFrame->offset.y * size / clip.h };
	SDL_RenderCopyEx(context.renderer, animSet->spriteSheet, &clip, &dest,
		headingDegrees_ - directionAngle(direction_), &pivot, SDL_FLIP_NONE);
}

Vec2 Cycle::headingVector() const {
	const float radians = headingDegrees_ * PI / 180.0f;
	return { std::cos(radians), std::sin(radians) };
}

OrientedBox Cycle::boxAt(std::size_t index, Vec2 head) const {
	return geom::orientedBox(localBoxes_[index], head, (headingDegrees_ - directionAngle(direction_)) * PI / 180.0f);
}

OrientedBox Cycle::terrainBoxAt(Vec2 head) const {
	if (turnGraceRemaining_ > 0.0f) {
		return geom::orientedBox({ -0.5f, -0.5f, 1.0f, 1.0f }, head, 0.0f);
	}
	return boxAt(boxCount_ > 1 ? 1 : 0, head);
}

bool Cycle::overlaps(const RectF& rect) const {
	const OrientedBox other = geom::orientedBox(rect, { 0.0f, 0.0f }, 0.0f);
	for (std::size_t i = 0; i < boxCount_; ++i) {
		if (geom::sweptBoxIntersectsBox(boxAt(i, center()), { 0.0f, 0.0f }, other)) return true;
	}
	return false;
}

bool Cycle::overlaps(const Cycle& other) const {
	for (std::size_t i = 0; i < boxCount_; ++i) {
		for (std::size_t j = 0; j < other.boxCount_; ++j) {
			if (geom::sweptBoxIntersectsBox(boxAt(i, center()), { 0.0f, 0.0f },
				other.boxAt(j, other.center()))) return true;
		}
	}
	return false;
}

void Cycle::steer(float steering, float throttle, float elapsedSeconds) {
	if (throttle > 0.0f) currentSpeed_ = std::min(MAX_SPEED, currentSpeed_ + ACCELERATION * elapsedSeconds);
	if (throttle < 0.0f) currentSpeed_ = std::max(MIN_SPEED, currentSpeed_ - BRAKING * elapsedSeconds);
	if (steering == 0.0f) {
		if (turning_ && geom::distanceSq(lastCurveSample_, center()) > 0.0001f) trail_.onTurn(center());
		turning_ = false;
		return;
	}

	if (!turning_ || geom::distanceSq(lastCurveSample_, center())
		>= TRAIL_CURVE_SAMPLE_DISTANCE * TRAIL_CURVE_SAMPLE_DISTANCE) {
		trail_.onTurn(center());
		lastCurveSample_ = center();
		turning_ = true;
	}
	headingDegrees_ = std::fmod(headingDegrees_ + steering * TURN_SPEED * elapsedSeconds + 360.0f, 360.0f);
	const int quadrant = static_cast<int>(std::floor((headingDegrees_ + 45.0f) / 90.0f)) % 4;
	const Direction directions[] = { Direction::East, Direction::South, Direction::West, Direction::North };
	if (directions[quadrant] != direction_) {
		const int frame = currentFrame != nullptr ? currentFrame->frameNumber : 0;
		const float timer = frameTimer;
		direction_ = directions[quadrant];
		changeAnimation(state, true);
		if (currentAnim != nullptr) {
			Frame* matchingFrame = currentAnim->getFrame(frame);
			if (matchingFrame != nullptr) currentFrame = matchingFrame;
			frameTimer = timer;
			refreshBounds();
		}
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

void Cycle::advance(float distance) {
	const Vec2 forward = headingVector();
	x += forward.x * distance;
	y += forward.y * distance;
	turnGraceRemaining_ = std::max(0.0f, turnGraceRemaining_ - distance);
	trail_.updateHead({ x, y });
	trail_.trim(maxPixelTrailLength());
}

void Cycle::crash() {
	alive_ = false;
}

SDL_Point Cycle::position() const {
	return { static_cast<int>(std::floor(x / cellSize_)), static_cast<int>(std::floor(y / cellSize_)) };
}

Direction Cycle::direction() const {
	return direction_;
}

SDL_Point Cycle::nextPosition() const {
	return nextPosition(direction_);
}

SDL_Point Cycle::nextPosition(Direction direction) const {
	const SDL_Point offset = directionOffset(direction);
	const SDL_Point cell = position();
	return { cell.x + offset.x, cell.y + offset.y };
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

SnapshotCycle Cycle::snapshot(SnapshotCycleKind kind, std::uint64_t id) const {
	SnapshotCycle result;
	result.kind = kind;
	result.id = id;
	result.position = { x, y };
	result.heading = headingDegrees_;
	result.speed = currentSpeed_;
	result.score = static_cast<std::uint32_t>(score_);
	result.lives = static_cast<std::uint32_t>(lives_);
	result.alive = alive_;
	result.respawning = isRespawning();
	result.boost = effects_.isActive(EffectType::Boost);
	result.longTrail = effects_.isActive(EffectType::LongTrail);
	result.slow = effects_.isActive(EffectType::Slow);
	result.glow = { glowColor_.r, glowColor_.g, glowColor_.b, glowColor_.a };
	result.core = { coreColor_.r, coreColor_.g, coreColor_.b, coreColor_.a };
	result.animationFrame = currentFrame == nullptr ? 0 : static_cast<std::uint32_t>(currentFrame->frameNumber);
	result.trail.reserve(trail_.vertices().size());
	for (const Vec2 point : trail_.vertices()) result.trail.push_back({ point.x, point.y });
	return result;
}

bool Cycle::applyVisualSnapshot(const SnapshotCycle& snapshot) {
	if (!validateCycleSnapshot({ 0, { snapshot } })) {
		std::cerr << "[Cycle] Invalid visual snapshot" << std::endl;
		return false;
	}
	const int quadrant = static_cast<int>(std::floor((snapshot.heading + 45.0f) / 90.0f)) % 4;
	const Direction directions[] = { Direction::East, Direction::South, Direction::West, Direction::North };
	const Direction newDirection = directions[quadrant];
	Animation* animation = animSet == nullptr ? nullptr : animSet->getAnimation(animationName(newDirection));
	if (animSet != nullptr && (animation == nullptr || snapshot.animationFrame >= animation->frames.size())) {
		std::cerr << "[Cycle] Snapshot animation frame is unavailable" << std::endl;
		return false;
	}
	if (animSet == nullptr && snapshot.animationFrame != 0) {
		std::cerr << "[Cycle] Snapshot requires a missing animation" << std::endl;
		return false;
	}
	std::vector<Vec2> vertices;
	vertices.reserve(snapshot.trail.size());
	for (const SnapshotPoint& point : snapshot.trail) vertices.push_back({ point.x, point.y });
	x = snapshot.position.x;
	y = snapshot.position.y;
	direction_ = newDirection;
	headingDegrees_ = snapshot.heading;
	currentSpeed_ = snapshot.speed;
	lives_ = static_cast<int>(snapshot.lives);
	score_ = static_cast<int>(snapshot.score);
	alive_ = snapshot.alive;
	respawnTimer_ = snapshot.respawning ? 1.0f : 0.0f;
	glowColor_ = { snapshot.glow[0], snapshot.glow[1], snapshot.glow[2], snapshot.glow[3] };
	coreColor_ = { snapshot.core[0], snapshot.core[1], snapshot.core[2], snapshot.core[3] };
	effects_.clear();
	if (snapshot.boost) effects_.apply(EffectType::Boost, std::numeric_limits<float>::max());
	if (snapshot.longTrail) effects_.apply(EffectType::LongTrail, std::numeric_limits<float>::max());
	if (snapshot.slow) effects_.apply(EffectType::Slow, std::numeric_limits<float>::max());
	trail_.setVertices(vertices);
	currentAnim = animation;
	currentFrame = animation == nullptr ? nullptr : animation->getFrame(static_cast<int>(snapshot.animationFrame));
	frameTimer = 0.0f;
	refreshBounds();
	return true;
}

void Cycle::setVisualPose(Vec2 position, float heading) {
	x = position.x;
	y = position.y;
	headingDegrees_ = std::fmod(heading + 360.0f, 360.0f);
	const int quadrant = static_cast<int>(std::floor((headingDegrees_ + 45.0f) / 90.0f)) % 4;
	const Direction directions[] = { Direction::East, Direction::South, Direction::West, Direction::North };
	if (directions[quadrant] != direction_) {
		const int frame = currentFrame == nullptr ? 0 : currentFrame->frameNumber;
		direction_ = directions[quadrant];
		changeAnimation(state, true);
		if (currentAnim != nullptr) {
			currentFrame = currentAnim->getFrame(frame);
			refreshBounds();
		}
	}
	if (!trail_.vertices().empty()) trail_.updateHead(position);
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
