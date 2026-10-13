#pragma once

#include "rendering/shapes.h"
#include "world/worldObject.h"

class MineTrap : public WorldObject {
public:
	using WorldObject::WorldObject;

	void onTouch(Cycle& cycle) override { cycle.crash(); }
	bool isDangerous() const override { return true; }

	void draw(const RenderContext& context) override {
		SDL_SetRenderDrawColor(context.renderer, 255, 40, 40, SDL_ALPHA_OPAQUE);
		shapes::fillCircle(context.renderer, static_cast<int>(x) - context.camera.x,
			static_cast<int>(y) - context.camera.y, cellSize_);
	}
};
