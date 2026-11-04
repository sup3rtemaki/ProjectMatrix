#include "world/projectiles/projectileFactory.h"

#include <list>

ProjectileConfig ProjectileFactory::configFor(ProjectileType type) {
	ProjectileConfig config;
	switch (type) {
	case ProjectileType::Triangle:
		config.animFile = "animations/projectile_tri.fdset";
		break;
	}
	return config;
}

AnimationSet* ProjectileFactory::animSetFor(const ProjectileConfig& config) {
	auto found = animSets_.find(config.animFile);
	if (found != animSets_.end()) {
		return found->second.get();
	}
	std::list<DataGroupType> dataGroupTypes;
	DataGroupType collisionBoxType;
	collisionBoxType.groupName = Projectile::COLLISION_BOX_GROUP;
	collisionBoxType.dataType = DataGroupType::DATATYPE_BOX;
	dataGroupTypes.push_back(collisionBoxType);
	auto animSet = std::make_unique<AnimationSet>();
	animSet->loadAnimationSet(config.animFile, dataGroupTypes, renderer_);
	AnimationSet* raw = animSet.get();
	animSets_[config.animFile] = std::move(animSet);
	return raw;
}

std::unique_ptr<Projectile> ProjectileFactory::create(ProjectileType type, Vec2 origin, Vec2 direction) {
	const ProjectileConfig config = configFor(type);
	return std::make_unique<Projectile>(config, animSetFor(config), origin, direction);
}
