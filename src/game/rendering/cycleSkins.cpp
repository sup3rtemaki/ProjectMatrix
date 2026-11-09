#include "rendering/cycleSkins.h"

#include "cycle.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <list>

namespace {
struct Skin {
	const char* name;
	SDL_Color core; // the trail core color this skin stands for
};

const Skin SKINS[] = {
	{ "cyan", { 20, 235, 255, 255 } },
	{ "blue", { 60, 130, 255, 255 } },
	{ "green", { 60, 255, 130, 255 } },
	{ "yellow", { 255, 235, 50, 255 } },
	{ "orange", { 255, 155, 35, 255 } },
	{ "red", { 255, 45, 75, 255 } },
	{ "magenta", { 255, 90, 200, 255 } },
	{ "white", { 240, 245, 255, 255 } }
};

int colorDistance(SDL_Color a, SDL_Color b) {
	const int dr = a.r - b.r;
	const int dg = a.g - b.g;
	const int db = a.b - b.b;
	return dr * dr + dg * dg + db * db;
}
}

AnimationSet* CycleSkins::forColor(SDL_Color trailCore) {
	const Skin* best = std::begin(SKINS);
	for (const Skin& skin : SKINS) {
		if (colorDistance(skin.core, trailCore) < colorDistance(best->core, trailCore)) {
			best = &skin;
		}
	}
	return load(best->name);
}

AnimationSet* CycleSkins::load(const std::string& name) {
	const auto found = loaded_.find(name);
	if (found != loaded_.end()) {
		return found->second.get();
	}

	const std::string file = "animations/cycle_" + name + ".fdset";
	if (!std::ifstream(resPath_ + file).good()) {
		std::cerr << "Animation set not found: " << resPath_ + file << std::endl;
		loaded_[name] = nullptr;
		return nullptr;
	}
	std::list<DataGroupType> dataGroupTypes;
	for (const char* groupName : Cycle::COLLISION_BOX_GROUPS) {
		DataGroupType collisionBoxType;
		collisionBoxType.groupName = groupName;
		collisionBoxType.dataType = DataGroupType::DATATYPE_BOX;
		dataGroupTypes.push_back(collisionBoxType);
	}
	auto animSet = std::make_unique<AnimationSet>();
	animSet->loadAnimationSet(file, dataGroupTypes, renderer_);
	AnimationSet* raw = animSet.get();
	loaded_[name] = std::move(animSet);
	return raw;
}
