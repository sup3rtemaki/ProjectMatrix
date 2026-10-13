#pragma once

#include "SDL.h"

namespace shapes {
// Filled circle in the renderer's current draw color.
void fillCircle(SDL_Renderer* renderer, int centerX, int centerY, int radius);
}
