#pragma once

#include "rendering/shapes.h"
#include "world/worldObject.h"

class LongTrailPickup : public WorldObject {
public:
	static constexpr float DURATION_SECONDS = 8.0f;

	using WorldObject::WorldObject;

	void onTouch(Cycle& cycle) override {
		cycle.effects().apply(EffectType::LongTrail, DURATION_SECONDS);
	}

	bool isPickup() const override { return true; }

	void draw(const RenderContext& context) override {
		SDL_SetRenderDrawColor(context.renderer, 150, 255, 40, SDL_ALPHA_OPAQUE);
		shapes::fillCircle(context.renderer, static_cast<int>(x) - context.camera.x,
			static_cast<int>(y) - context.camera.y, cellSize_);
	}
};
