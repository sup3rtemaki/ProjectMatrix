#include "network/onlineMatchState.h"

#include <cassert>
#include <limits>
#include <string>

int main() {
	WorldSnapshot state;
	state.sequence = 0x12345678;
	state.objects = {
		{ SnapshotObjectType::Boost, { 10, 20, 2, 2 } },
		{ SnapshotObjectType::Mine, { 30, 40, 2, 2 } }
	};
	state.towers = {
		{ { 100, 120, 10, 10 }, true, 3, { { { 100.5f, 20.25f }, 2 }, { { 12, 14 }, 0 } } }
	};
	const auto wire = serializeWorldSnapshot(state);
	assert(wire && wire->size() == 60);
	assert(static_cast<unsigned char>((*wire)[0]) == 1 && static_cast<unsigned char>((*wire)[1]) == 0x78);
	const auto decoded = deserializeWorldSnapshot(*wire);
	assert(decoded && decoded->sequence == state.sequence);
	assert(decoded->objects.size() == 2 && decoded->towers.size() == 1);
	assert(decoded->objects[0].type == SnapshotObjectType::Boost && decoded->objects[1].cells.y == 40);
	assert(decoded->towers[0].placement.x == 100 && decoded->towers[0].attacking);
	assert(decoded->towers[0].animationFrame == 3 && decoded->towers[0].projectiles.size() == 2);
	assert(decoded->towers[0].projectiles[0].position.x == 100.5f
		&& decoded->towers[0].projectiles[0].animationFrame == 2);

	for (std::size_t length = 0; length < wire->size(); ++length) {
		assert(!deserializeWorldSnapshot(wire->substr(0, length)));
	}
	assert(!deserializeWorldSnapshot(*wire + '\0'));
	std::string malformed = *wire;
	malformed[0] = 2;
	assert(!deserializeWorldSnapshot(malformed));
	malformed = *wire;
	malformed[9] = 4;
	assert(!deserializeWorldSnapshot(malformed));
	malformed = *wire;
	malformed[5] = static_cast<char>(0xff);
	malformed[6] = static_cast<char>(0xff);
	assert(!deserializeWorldSnapshot(malformed));
	malformed = *wire;
	malformed[35] = 2;
	assert(!deserializeWorldSnapshot(malformed));
	malformed = *wire;
	malformed[38] = static_cast<char>(0xff);
	malformed[39] = static_cast<char>(0xff);
	assert(!deserializeWorldSnapshot(malformed));
	assert(!deserializeWorldSnapshot(std::string(WorldSnapshot::MAX_BYTES + 1, '\0')));

	WorldSnapshot invalid = state;
	invalid.objects[0].cells.w = 0;
	assert(!serializeWorldSnapshot(invalid));
	invalid = state;
	invalid.objects[0].cells.x = 65535;
	assert(!serializeWorldSnapshot(invalid));
	invalid = state;
	invalid.objects[1].cells = invalid.objects[0].cells;
	assert(!serializeWorldSnapshot(invalid));
	invalid = state;
	invalid.objects[0].cells.x = invalid.towers[0].placement.x;
	invalid.objects[0].cells.y = invalid.towers[0].placement.y;
	assert(serializeWorldSnapshot(invalid));
	invalid = state;
	invalid.towers[0].projectiles[0].position.x = std::numeric_limits<float>::quiet_NaN();
	assert(!serializeWorldSnapshot(invalid));
	invalid = state;
	invalid.towers[0].animationFrame = 65536;
	assert(!serializeWorldSnapshot(invalid));

	WorldSnapshot maximum;
	for (std::size_t i = 0; i < WorldSnapshot::MAX_OBJECTS; ++i) {
		maximum.objects.push_back({ SnapshotObjectType::Slow, { static_cast<std::uint32_t>(i), 0, 1, 1 } });
	}
	for (std::size_t i = 0; i < WorldSnapshot::MAX_TOWERS; ++i) {
		SnapshotTower tower;
		tower.placement = { static_cast<std::uint32_t>(i), 10, 1, 1 };
		tower.projectiles.resize(WorldSnapshot::MAX_PROJECTILES / WorldSnapshot::MAX_TOWERS);
		maximum.towers.push_back(std::move(tower));
	}
	const auto maxWire = serializeWorldSnapshot(maximum);
	assert(maxWire && maxWire->size() <= WorldSnapshot::MAX_BYTES);
	const auto maxDecoded = deserializeWorldSnapshot(*maxWire);
	assert(maxDecoded && maxDecoded->objects.size() == WorldSnapshot::MAX_OBJECTS
		&& maxDecoded->towers.size() == WorldSnapshot::MAX_TOWERS
		&& maxDecoded->towers[0].projectiles.size() == 8);
	maximum.towers[0].projectiles.push_back({});
	assert(!serializeWorldSnapshot(maximum));
}
