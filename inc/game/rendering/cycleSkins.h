#pragma once

#include "animationSet.h"

#include "SDL.h"

#include <map>
#include <memory>
#include <string>

// One cycle spritesheet per color (animations/cycle_<name>.fdset), loaded the first time it is needed.
// A trail color uses the skin whose color is closest to it.
class CycleSkins {
public:
	explicit CycleSkins(SDL_Renderer* renderer, std::string resPath)
		: renderer_(renderer), resPath_(std::move(resPath)) {}

	// Null when the skin's files are missing; Cycle then draws a plain square.
	AnimationSet* forColor(SDL_Color trailCore);

private:
	AnimationSet* load(const std::string& name);

	SDL_Renderer* renderer_;
	std::string resPath_;
	std::map<std::string, std::unique_ptr<AnimationSet>> loaded_;
};
