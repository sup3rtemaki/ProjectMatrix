#pragma once

#include "level/levelDefinition.h"
#include "rendering/depthQueue.h"
#include "renderContext.h"

#include <string>
#include <vector>

// Draws the tile layers of a level, skipping tiles outside the camera.
class TileMapRenderer {
public:
	TileMapRenderer() = default;
	TileMapRenderer(const TileMapRenderer&) = delete;
	TileMapRenderer& operator=(const TileMapRenderer&) = delete;
	~TileMapRenderer();

	void load(const LevelDefinition& level, SDL_Renderer* renderer, const std::string& texturesPath);
	// Draws the tiles of one layer right away.
	void drawLayer(const RenderContext& context, int layer) const;
	// Queues each tile of one layer sorted by its bottom edge. The context must outlive queue.flush().
	void queueLayer(const RenderContext& context, int layer, DepthQueue& queue) const;

private:
	bool isVisible(const TileSprite& tile, const SDL_Rect& camera) const;
	void drawTile(const RenderContext& context, const TileSprite& tile) const;

	void clear();

	std::vector<SDL_Texture*> textures_;
	std::vector<TileSprite> tiles_;
};
