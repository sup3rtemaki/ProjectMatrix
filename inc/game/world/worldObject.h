#pragma once

#include "cycle.h"

// Anything placed in the arena that a cycle can run into. Drawn with primitives until sprites exist.
class WorldObject : public Entity {
public:
	// cells is the occupied area in grid cells.
	WorldObject(SDL_Rect cells, int cellSize) : cells_(cells), cellSize_(cellSize) {
		x = (cells.x + cells.w / 2.0f) * cellSize;
		y = (cells.y + cells.h / 2.0f) * cellSize;
		animSet = nullptr;
		currentAnim = nullptr;
		currentFrame = nullptr;
		frameTimer = 0;
		solid = false;
		collideWithSolids = false;
	}

	virtual void onTouch(Cycle& cycle) = 0;
	// Enemy AI steers around dangerous cells.
	virtual bool isDangerous() const { return false; }
	virtual bool isConsumedOnTouch() const { return true; }
	// Power-ups the enemy AI may go after.
	virtual bool isPickup() const { return false; }

	void changeAnimation(int, bool, string = "") override {}

	const SDL_Rect& cells() const { return cells_; }

protected:
	SDL_Rect cells_;
	int cellSize_;
};
