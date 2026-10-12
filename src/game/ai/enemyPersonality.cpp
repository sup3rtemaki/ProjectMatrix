#include "ai/enemyPersonality.h"

#include "ai/personalities/aggressorBrain.h"
#include "ai/personalities/ambusherBrain.h"
#include "ai/personalities/cowardBrain.h"
#include "ai/personalities/erraticBrain.h"
#include "ai/personalities/strategistBrain.h"

std::unique_ptr<EnemyBrain> createEnemyBrain(EnemyPersonality personality) {
	switch (personality) {
	case EnemyPersonality::Aggressor: return std::make_unique<AggressorBrain>();
	case EnemyPersonality::Ambusher: return std::make_unique<AmbusherBrain>();
	case EnemyPersonality::Strategist: return std::make_unique<StrategistBrain>();
	case EnemyPersonality::Coward: return std::make_unique<CowardBrain>();
	case EnemyPersonality::Erratic: return std::make_unique<ErraticBrain>();
	}
	return std::make_unique<ErraticBrain>();
}
