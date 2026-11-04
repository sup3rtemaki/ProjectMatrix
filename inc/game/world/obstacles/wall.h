#pragma once

#include "world/worldObject.h"

class Wall : public WorldObject {
public:
	using WorldObject::WorldObject;

	void onTouch(Cycle& cycle) override { cycle.crash(); }
	bool isDangerous() const override { return true; }
	bool isConsumedOnTouch() const override { return false; }
	bool blocksProjectiles() const override { return true; }

	// The Walls tile layer draws the art; this entity is only the collision.
	void draw(const RenderContext& context) override {
		if (!context.debugging) {
			return;
		}
		const SDL_Rect bounds = { cells_.x * cellSize_ - context.camera.x,
			cells_.y * cellSize_ - context.camera.y, cells_.w * cellSize_, cells_.h * cellSize_ };
		SDL_SetRenderDrawColor(context.renderer, 55, 65, 85, SDL_ALPHA_OPAQUE);
		SDL_RenderFillRect(context.renderer, &bounds);
		SDL_SetRenderDrawColor(context.renderer, 120, 140, 175, SDL_ALPHA_OPAQUE);
		SDL_RenderDrawRect(context.renderer, &bounds);
	}
};
