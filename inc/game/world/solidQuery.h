#pragma once

#include "SDL.h"

#include <functional>

// Tells whether a pixel-space box overlaps something that stops projectiles (walls, arena edge).
using SolidQuery = std::function<bool(const SDL_Rect&)>;
