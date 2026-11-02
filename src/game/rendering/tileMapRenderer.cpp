#include "rendering/tileMapRenderer.h"

#include "drawing_functions.h"

namespace {
// Callers may shift the camera a little, so draw slightly beyond it.
constexpr int CULL_MARGIN = 64;
}

TileMapRenderer::~TileMapRenderer() {
	clear();
}

void TileMapRenderer::clear() {
	for (SDL_Texture* texture : textures_) {
		if (texture != nullptr) {
			SDL_DestroyTexture(texture);
		}
	}
	textures_.clear();
	tiles_.clear();
}

void TileMapRenderer::load(const LevelDefinition& level, SDL_Renderer* renderer, const std::string& texturesPath) {
	clear();
	for (const std::string& image : level.tileImages) {
		textures_.push_back(loadTexture(texturesPath + image, renderer));
	}
	tiles_ = level.tiles;
}

void TileMapRenderer::draw(const RenderContext& context) const {
	const SDL_Rect& camera = context.camera;
	for (const TileSprite& tile : tiles_) {
		const SDL_Rect& world = tile.destination;
		if (world.x + world.w < camera.x - CULL_MARGIN || world.x > camera.x + camera.w + CULL_MARGIN
			|| world.y + world.h < camera.y - CULL_MARGIN || world.y > camera.y + camera.h + CULL_MARGIN) {
			continue;
		}
		SDL_Texture* texture = textures_[static_cast<std::size_t>(tile.image)];
		if (texture == nullptr) {
			continue;
		}
		const SDL_Rect screen = { world.x - camera.x, world.y - camera.y, world.w, world.h };
		SDL_RenderCopy(context.renderer, texture, &tile.source, &screen);
	}
}
