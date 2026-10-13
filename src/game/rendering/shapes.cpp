#include "rendering/shapes.h"

#include <cmath>

namespace shapes {
void fillCircle(SDL_Renderer* renderer, int centerX, int centerY, int radius) {
	for (int dy = -radius; dy <= radius; ++dy) {
		const int dx = static_cast<int>(std::sqrt(static_cast<float>(radius * radius - dy * dy)));
		SDL_RenderDrawLine(renderer, centerX - dx, centerY + dy, centerX + dx, centerY + dy);
	}
}
}
