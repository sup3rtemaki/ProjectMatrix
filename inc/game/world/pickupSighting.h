#pragma once

#include "world/objectType.h"

#include "SDL.h"

// A power-up an enemy can see and may go after.
struct PickupSighting {
	ObjectType type;
	SDL_Point cell;
};
