#pragma once

#include "rendering/shapes.h"
#include "world/worldObject.h"

class BoostPickup : public WorldObject {
public:
	static constexpr float DURATION_SECONDS = 4.0f;

	using WorldObject::WorldObject;

	void onTouch(Cycle& cycle) override {
		cycle.effects().apply(EffectType::Boost, DURATION_SECONDS);
	}

	bool isPickup() const override { return true; }

	void draw(const RenderContext& context) override {
		SDL_SetRenderDrawColor(context.renderer, 40, 255, 90, SDL_ALPHA_OPAQUE);
		shapes::fillCircle(context.renderer, static_cast<int>(x) - context.camera.x,
			static_cast<int>(y) - context.camera.y, cellSize_);
	}
};
