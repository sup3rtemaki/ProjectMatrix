#pragma once

#include "level/levelDefinition.h"
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
	void draw(const RenderContext& context) const;

private:
	void clear();

	std::vector<SDL_Texture*> textures_;
	std::vector<TileSprite> tiles_;
};
