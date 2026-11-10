#include "network/onlineMatchState.h"

#include <cassert>
#include <cstdint>
#include <limits>
#include <string>

int main() {
	SnapshotCycle human;
	human.id = 0x0110000200000001ull;
	human.position = { 123.5f, 987.25f };
	human.heading = 271.5f;
	human.speed = 195.0f;
	human.score = 123;
	human.lives = 2;
	human.alive = true;
	human.boost = true;
	human.slow = true;
	human.glow = { 1, 2, 3, 4 };
	human.core = { 5, 6, 7, 8 };
	human.animationFrame = 12;
	human.trail = { { 1.0f, 2.0f }, { 3.0f, 4.0f }, human.position };

	SnapshotCycle bot = human;
	bot.kind = SnapshotCycleKind::Bot;
	bot.id = 1;
	bot.alive = false;
	bot.respawning = true;
	bot.longTrail = true;
	CycleSnapshot snapshot{ 0x12345678u, { human, bot } };
	const auto wire = serializeCycleSnapshot(snapshot);
	assert(wire && static_cast<unsigned char>((*wire)[0]) == 1);
	assert(static_cast<unsigned char>((*wire)[1]) == 0x78);
	assert(static_cast<unsigned char>((*wire)[4]) == 0x12);
	const auto decoded = deserializeCycleSnapshot(*wire);
	assert(decoded && decoded->sequence == snapshot.sequence && decoded->cycles.size() == 2);
	const SnapshotCycle& result = decoded->cycles[0];
	assert(result.id == human.id && result.kind == human.kind);
	assert(result.position.x == human.position.x && result.position.y == human.position.y);
	assert(result.heading == human.heading && result.speed == human.speed);
	assert(result.score == human.score && result.lives == human.lives && result.alive);
	assert(result.boost && result.slow && !result.longTrail && !result.respawning);
	assert(result.glow == human.glow && result.core == human.core && result.animationFrame == 12);
	assert(result.trail.size() == 3 && result.trail[2].x == human.position.x);
	assert(decoded->cycles[1].kind == SnapshotCycleKind::Bot && decoded->cycles[1].respawning);

	for (std::size_t length = 0; length < wire->size(); ++length) {
		assert(!deserializeCycleSnapshot(wire->substr(0, length)));
	}
	assert(!deserializeCycleSnapshot(*wire + '\0'));
	std::string malformed = *wire;
	malformed[0] = 2;
	assert(!deserializeCycleSnapshot(malformed));
	malformed = *wire;
	malformed[5] = 14;
	assert(!deserializeCycleSnapshot(malformed));
	malformed = *wire;
	malformed[6] = 2;
	assert(!deserializeCycleSnapshot(malformed));
	malformed = *wire;
	malformed[36] = static_cast<char>(0x80);
	assert(!deserializeCycleSnapshot(malformed));
	malformed = *wire;
	malformed[47] = 1;
	malformed[48] = 2;
	assert(!deserializeCycleSnapshot(malformed));
	malformed = *wire;
	malformed[15] = '\0'; malformed[16] = '\0';
	malformed[17] = static_cast<char>(0xc0); malformed[18] = static_cast<char>(0x7f);
	assert(!deserializeCycleSnapshot(malformed));
	malformed = *wire;
	const std::size_t secondCycleOffset = 6 + (wire->size() - 6) / 2;
	malformed.replace(secondCycleOffset, 9, wire->substr(6, 9));
	assert(!deserializeCycleSnapshot(malformed));
	assert(!deserializeCycleSnapshot(std::string(CycleSnapshot::MAX_BYTES + 1, '\0')));

	CycleSnapshot invalid = snapshot;
	invalid.cycles.push_back(human);
	assert(!serializeCycleSnapshot(invalid));
	invalid = snapshot;
	invalid.cycles[0].position.x = std::numeric_limits<float>::infinity();
	assert(!serializeCycleSnapshot(invalid));
	invalid = snapshot;
	invalid.cycles[0].heading = 360.0f;
	assert(!serializeCycleSnapshot(invalid));
	invalid = snapshot;
	invalid.cycles[0].alive = true;
	invalid.cycles[0].respawning = true;
	assert(!serializeCycleSnapshot(invalid));
	invalid = snapshot;
	invalid.cycles[0].trail.resize(CycleSnapshot::MAX_TRAIL_POINTS + 1);
	assert(!serializeCycleSnapshot(invalid));
	invalid = snapshot;
	invalid.cycles[0].lives = 256;
	assert(!serializeCycleSnapshot(invalid));
	invalid = snapshot;
	invalid.cycles[0].animationFrame = 65536;
	assert(!serializeCycleSnapshot(invalid));

	CycleSnapshot maximum;
	for (std::size_t i = 0; i < CycleSnapshot::MAX_HUMANS + CycleSnapshot::MAX_BOTS; ++i) {
		SnapshotCycle cycle = human;
		cycle.kind = i < CycleSnapshot::MAX_HUMANS ? SnapshotCycleKind::Human : SnapshotCycleKind::Bot;
		cycle.id = i + 1;
		cycle.trail.resize(CycleSnapshot::MAX_TRAIL_POINTS, { 10.0f, 20.0f });
		maximum.cycles.push_back(std::move(cycle));
	}
	const auto maxWire = serializeCycleSnapshot(maximum);
	assert(maxWire && maxWire->size() <= CycleSnapshot::MAX_BYTES);
	const auto maxDecoded = deserializeCycleSnapshot(*maxWire);
	assert(maxDecoded && maxDecoded->cycles.size() == 13 && maxDecoded->cycles.back().trail.size() == 512);
	maximum.cycles.back().kind = SnapshotCycleKind::Human;
	assert(!serializeCycleSnapshot(maximum));

	SnapshotSequence sequence;
	assert(sequence.accept(0xfffffffeu));
	assert(sequence.accept(0xffffffffu));
	assert(sequence.accept(0));
	assert(!sequence.accept(0));
	assert(!sequence.accept(0xffffffffu));
	assert(!sequence.accept(0x80000000u));
	sequence.reset();
	assert(sequence.accept(0xffffffffu));

	OnlineFrame frame{ 17, OnlinePhase::Running, snapshot, {} };
	frame.world.sequence = snapshot.sequence;
	const auto parts = serializeOnlineFrame(frame);
	assert(parts);
	OnlineFrameAssembler assembler;
	assert(!assembler.accept((*parts)[1]));
	auto assembled = assembler.accept((*parts)[0]);
	assert(assembled && assembled->levelIndex == 17 && assembled->phase == OnlinePhase::Running);
	assert(assembled->cycles.cycles.size() == 2 && assembled->world.sequence == snapshot.sequence);
	assert(!assembler.accept((*parts)[0]));
	OnlineFrame newer = frame;
	newer.cycles.sequence++;
	newer.world.sequence++;
	const auto newerParts = serializeOnlineFrame(newer);
	assert(newerParts && !assembler.accept((*newerParts)[1]));
	assembled = assembler.accept((*newerParts)[0]);
	assert(assembled && assembled->cycles.sequence == newer.cycles.sequence);
	assert(!assembler.accept((*parts)[0]));
	std::string wrongPhase = (*parts)[1];
	wrongPhase[4] = static_cast<char>(OnlinePhase::Finished);
	assert(!assembler.accept(wrongPhase));
	assert(!assembler.accept((*parts)[1]));
	assert(assembler.accept((*parts)[0]));
	assert(!assembler.accept(std::string(5, '\0')));
	newer.world.sequence++;
	assert(!serializeOnlineFrame(newer));
}
