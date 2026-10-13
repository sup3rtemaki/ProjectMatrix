#pragma once

#include "rendering/shapes.h"
#include "world/worldObject.h"

class SlowTrap : public WorldObject {
public:
	static constexpr float DURATION_SECONDS = 3.0f;

	using WorldObject::WorldObject;

	void onTouch(Cycle& cycle) override {
		cycle.effects().apply(EffectType::Slow, DURATION_SECONDS);
	}

	void draw(const RenderContext& context) override {
		SDL_SetRenderDrawColor(context.renderer, 255, 150, 30, SDL_ALPHA_OPAQUE);
		shapes::fillCircle(context.renderer, static_cast<int>(x) - context.camera.x,
			static_cast<int>(y) - context.camera.y, cellSize_);
	}
};
