#pragma once

#include <cstddef>
#include <vector>

enum class EffectType {
	Boost,     // faster
	LongTrail, // longer light trail
	Slow       // slower
};

// Timed status effects carried by a cycle.
class CycleEffects {
public:
	static constexpr float BOOST_SPEED_MULTIPLIER = 1.7f;
	static constexpr float SLOW_SPEED_MULTIPLIER = 0.5f;
	static constexpr std::size_t LONG_TRAIL_BONUS = 40;

	// Restarts the timer when the effect is already active.
	void apply(EffectType type, float seconds);
	void update(float elapsedSeconds);
	void clear();

	bool isActive(EffectType type) const;
	float speedMultiplier() const;
	std::size_t trailBonus() const;

private:
	struct ActiveEffect {
		EffectType type;
		float remaining;
	};

	std::vector<ActiveEffect> active_;
};
