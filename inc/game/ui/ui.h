#pragma once

#include "SDL.h"

#include <string>

class Ui {
public:
	Ui() = default;
	virtual ~Ui() = default;

	virtual void setRenderer(SDL_Renderer* renderer) {
		renderer_ = renderer;
	}

	virtual void setResourcePath(const std::string& path) {
		resPath_ = path;
	}

	virtual void update(float dt) {
		deltaTime_ = dt;
	}

	virtual void draw() = 0;

protected:
	SDL_Renderer* renderer_ = nullptr;
	std::string resPath_;
	float deltaTime_ = 0.0f;
};
