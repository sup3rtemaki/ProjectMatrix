#include "effects/cycleEffects.h"

#include <algorithm>

void CycleEffects::apply(EffectType type, float seconds) {
	for (ActiveEffect& effect : active_) {
		if (effect.type == type) {
			effect.remaining = seconds;
			return;
		}
	}
	active_.push_back({ type, seconds });
}

void CycleEffects::update(float elapsedSeconds) {
	for (ActiveEffect& effect : active_) {
		effect.remaining -= elapsedSeconds;
	}
	active_.erase(std::remove_if(active_.begin(), active_.end(),
		[](const ActiveEffect& effect) { return effect.remaining <= 0; }), active_.end());
}

void CycleEffects::clear() {
	active_.clear();
}

bool CycleEffects::isActive(EffectType type) const {
	return std::any_of(active_.begin(), active_.end(),
		[type](const ActiveEffect& effect) { return effect.type == type; });
}

float CycleEffects::speedMultiplier() const {
	float multiplier = 1.0f;
	if (isActive(EffectType::Boost)) {
		multiplier *= BOOST_SPEED_MULTIPLIER;
	}
	if (isActive(EffectType::Slow)) {
		multiplier *= SLOW_SPEED_MULTIPLIER;
	}
	return multiplier;
}

std::size_t CycleEffects::trailBonus() const {
	return isActive(EffectType::LongTrail) ? LONG_TRAIL_BONUS : 0;
}
