#pragma once

#include "level/levelDefinition.h"

#include <optional>
#include <string>

namespace LevelLoader {
// Reads a Tiled map (.json) as a level; reports the problem on stderr and returns nothing on failure.
std::optional<LevelDefinition> load(const std::string& path, int cellSize);
}
