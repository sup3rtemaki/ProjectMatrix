#define SDL_MAIN_HANDLED
#include "cycle.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {
bool close(float a, float b, float tolerance = 0.01f) {
	return std::abs(a - b) < tolerance;
}

void testMovement() {
	Cycle cycle({ 0, 0, 0, 255 }, { 0, 0, 0, 255 }, nullptr, 8);
	cycle.reset({ 30, 30 }, Direction::East);
	const Vec2 initial = cycle.center();
	cycle.steer(0.0f, 1.0f, 10.0f);
	assert(close(cycle.speed(), Cycle::MAX_SPEED));
	cycle.steer(0.0f, -1.0f, 10.0f);
	assert(close(cycle.speed(), Cycle::MIN_SPEED));
	cycle.steer(0.0f, 0.0f, 1.0f);
	assert(close(cycle.speed(), Cycle::MIN_SPEED));

	for (int i = 0; i < 10; ++i) {
		cycle.steer(1.0f, 0.0f, 0.025f);
		cycle.advance(cycle.speed() * 0.025f);
	}
	assert(close(cycle.headingDegrees(), 45.0f));
	assert(cycle.center().x > initial.x && cycle.center().y > initial.y);
	assert(cycle.trail().getSegments(&cycle).size() > 1);
	cycle.steer(-1.0f, 0.0f, 0.25f);
	assert(close(cycle.headingDegrees(), 0.0f));
	cycle.respawn({ 30, 30 }, Direction::North);
	assert(close(cycle.speed(), Cycle::BASE_SPEED));
	assert(close(cycle.headingDegrees(), 270.0f));
}

void testRotatedCollisions() {
	const float radians = 3.14159265358979323846f / 4.0f;
	const OrientedBox bike = geom::orientedBox({ -10, -1, 20, 2 }, { 0, 0 }, radians);
	const OrientedBox wall = geom::orientedBox({ 5, 5, 2, 2 }, { 0, 0 }, 0.0f);
	const OrientedBox emptyCorner = geom::orientedBox({ -7, 5, 1, 1 }, { 0, 0 }, 0.0f);
	assert(geom::sweptBoxIntersectsBox(bike, { 0, 0 }, wall));
	assert(!geom::sweptBoxIntersectsBox(bike, { 0, 0 }, emptyCorner));
	assert(geom::sweptBoxIntersectsSegment(bike, { 0, 0 }, { 0, -2 }, { 0, 2 }));
	assert(!geom::sweptBoxIntersectsSegment(bike, { 0, 0 }, { -7, 5 }, { -6, 6 }));

	const OrientedBox small = geom::orientedBox({ -1, -1, 2, 2 }, { 0, 0 }, 0.0f);
	const OrientedBox target = geom::orientedBox({ 4, -1, 1, 2 }, { 0, 0 }, 0.0f);
	assert(geom::sweptBoxIntersectsBox(small, { 8, 0 }, target));
	assert(!geom::sweptBoxIntersectsBox(small, { 0, 8 }, target));
	assert(geom::sweptBoxIntersectsSegment(small, { 8, 0 }, { 4, -2 }, { 4, 2 }));
	assert(!geom::sweptBoxIntersectsSegment(small, { 0, 8 }, { 4, -2 }, { 4, 2 }));
	assert(geom::boxInsideRect(small, { -2, -2, 4, 4 }));
	assert(!geom::boxInsideRect(bike, { -5, -5, 10, 10 }));
}

}

int main() {
	testMovement();
	testRotatedCollisions();
	std::cout << "Movement and rotated collision tests passed\n";
}
