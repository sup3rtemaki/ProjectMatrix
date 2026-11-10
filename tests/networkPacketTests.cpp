#define SDL_MAIN_HANDLED
#include "network/steamSession.h"

#include <cassert>
#include <cstring>
#include <limits>

int main() {
	SteamSessionService session;
	OnlineInputPacket input{ 7, 42, 33.5f, true };
	const std::string inputBytes = session.serializeInputPacket(input);
	const auto decodedInput = session.deserializeInputPacket(inputBytes);
	assert(decodedInput && decodedInput->sequence == 7 && decodedInput->playerId == 42
		&& decodedInput->angle == 33.5f && decodedInput->turnQueued);
	for (std::size_t i = 0; i < inputBytes.size(); ++i) {
		assert(!session.deserializeInputPacket(inputBytes.substr(0, i)));
	}
	std::string badInput = inputBytes;
	badInput.back() = 2;
	assert(!session.deserializeInputPacket(badInput));
	input.angle = std::numeric_limits<float>::quiet_NaN();
	assert(!session.deserializeInputPacket(session.serializeInputPacket(input)));

	OnlineMatchSnapshot snapshot;
	snapshot.tick = 123;
	snapshot.hostPlayerId = 42;
	OnlinePlayerState player;
	player.playerId = 7;
	player.name = "test";
	player.x = 16.0f;
	player.y = 25.0f;
	snapshot.players.push_back(player);
	const std::string bytes = session.serializeSnapshot(snapshot);
	const auto decoded = session.deserializeSnapshot(bytes);
	assert(decoded && decoded->tick == 123 && decoded->players.size() == 1
		&& decoded->players[0].name == "test" && decoded->players[0].x == 16.0f);
	for (std::size_t i = 0; i < bytes.size(); ++i) {
		assert(!session.deserializeSnapshot(bytes.substr(0, i)));
	}
	std::string badCount = bytes;
	const std::uint32_t count = 0xffffffffu;
	std::memcpy(&badCount[8], &count, sizeof(count));
	assert(!session.deserializeSnapshot(badCount));
	assert(!session.deserializeSnapshot(bytes + 'x'));
}
