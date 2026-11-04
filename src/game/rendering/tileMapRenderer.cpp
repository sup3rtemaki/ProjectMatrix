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

bool TileMapRenderer::isVisible(const TileSprite& tile, const SDL_Rect& camera) const {
	const SDL_Rect& world = tile.destination;
	return !(world.x + world.w < camera.x - CULL_MARGIN || world.x > camera.x + camera.w + CULL_MARGIN
		|| world.y + world.h < camera.y - CULL_MARGIN || world.y > camera.y + camera.h + CULL_MARGIN);
}

void TileMapRenderer::drawTile(const RenderContext& context, const TileSprite& tile) const {
	SDL_Texture* texture = textures_[static_cast<std::size_t>(tile.image)];
	if (texture == nullptr) {
		return;
	}
	const SDL_Rect& world = tile.destination;
	const SDL_Rect screen = { world.x - context.camera.x, world.y - context.camera.y, world.w, world.h };
	SDL_RenderCopy(context.renderer, texture, &tile.source, &screen);
}

void TileMapRenderer::drawLayer(const RenderContext& context, int layer) const {
	for (const TileSprite& tile : tiles_) {
		if (tile.layer == layer && isVisible(tile, context.camera)) {
			drawTile(context, tile);
		}
	}
}

void TileMapRenderer::queueLayer(const RenderContext& context, int layer, DepthQueue& queue) const {
	for (const TileSprite& tile : tiles_) {
		if (tile.layer == layer && isVisible(tile, context.camera)) {
			queue.add(static_cast<float>(tile.destination.y + tile.destination.h),
				[this, &context, &tile]() { drawTile(context, tile); });
		}
	}
}