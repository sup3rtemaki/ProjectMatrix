#include "network/onlineMatchState.h"

#include <cassert>
#include <cstdint>
#include <string>

int main() {
	for (std::uint8_t color = 0; color < 8; ++color) {
		const auto ready = deserializePlayerReady(serializePlayerReady(color));
		assert(ready && *ready == color);
	}
	assert(!deserializePlayerReady(""));
	assert(!deserializePlayerReady(serializePlayerReady(8)));
	assert(!deserializePlayerReady(std::string("\2\0", 2)));
	assert(!deserializePlayerReady(serializePlayerReady(0) + '\0'));
	DrivingInputPacket source{ 0x12345678u, { true, false, false, true } };
	const std::string wire = serializeDrivingInput(source);
	assert(wire.size() == 6);
	assert(static_cast<unsigned char>(wire[0]) == 1);
	assert(static_cast<unsigned char>(wire[1]) == 0x78);
	assert(static_cast<unsigned char>(wire[4]) == 0x12);
	assert(static_cast<unsigned char>(wire[5]) == 9);
	const auto decoded = deserializeDrivingInput(wire);
	assert(decoded && decoded->sequence == source.sequence);
	assert(decoded->input.throttle() == 1.0f && decoded->input.steering() == 1.0f);
	for (std::size_t length = 0; length < wire.size(); ++length) {
		assert(!deserializeDrivingInput(wire.substr(0, length)));
	}
	assert(!deserializeDrivingInput(wire + '\0'));
	std::string malformed = wire;
	malformed[0] = 2;
	assert(!deserializeDrivingInput(malformed));
	malformed = wire;
	malformed[5] = static_cast<char>(0x80);
	assert(!deserializeDrivingInput(malformed));

	const std::uint64_t first = 0x0110000100000001ull;
	const std::uint64_t second = 0x0110000200000001ull;
	HostDrivingInputs host;
	assert(!host.addPlayer(0));
	assert(host.addPlayer(first) && host.addPlayer(second));
	assert(!host.accept(123, source));
	assert(host.accept(first, source));
	assert(!host.accept(first, source));
	assert(!host.accept(first, { source.sequence - 1, {} }));
	assert(host.input(first)->steering() == 1.0f);
	assert(host.input(second)->steering() == 0.0f);
	host.update(0.49f);
	assert(host.input(first)->steering() == 1.0f);
	host.update(0.01f);
	assert(host.input(first)->steering() == 0.0f);
	assert(!host.accept(first, source));
	assert(host.accept(first, { source.sequence + 1, { false, true, true, false } }));
	assert(host.input(first)->throttle() == -1.0f && host.input(first)->steering() == -1.0f);
	host.removePlayer(first);
	assert(!host.input(first));
	assert(host.addPlayer(first));
	assert(host.accept(first, { 0xffffffffu, {} }));
	assert(host.accept(first, { 0, { true, true, true, true } }));
	assert(host.input(first)->steering() == 0.0f && host.input(first)->throttle() == 0.0f);
	assert(!host.accept(first, { 0xffffffffu, {} }));
	host.retainPlayers({ first });
	assert(host.input(first) && !host.input(second));
	for (std::uint64_t id = 1; id <= 6; ++id) assert(host.addPlayer(id));
	assert(!host.addPlayer(7));
	assert(host.addPlayer(first));
	host.clear();
	assert(!host.input(first));
}
