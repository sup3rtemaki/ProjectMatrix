#pragma once

#include "world/projectiles/projectile.h"

#include <map>
#include <memory>
#include <string>

enum class ProjectileType {
	Triangle
};

// Builds projectiles by type. Each type's animation set is loaded once and shared by its projectiles,
// so the factory must outlive them.
class ProjectileFactory {
public:
	explicit ProjectileFactory(SDL_Renderer* renderer) : renderer_(renderer) {}

	std::unique_ptr<Projectile> create(ProjectileType type, Vec2 origin, Vec2 direction);

	static ProjectileConfig configFor(ProjectileType type);

private:
	AnimationSet* animSetFor(const ProjectileConfig& config);

	SDL_Renderer* renderer_;
	std::map<std::string, std::unique_ptr<AnimationSet>> animSets_;
};
