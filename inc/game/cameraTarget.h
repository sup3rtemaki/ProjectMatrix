#pragma once

#include "entity.h"

// CameraController follows an Entity; this carries only the position of the followed cycle.
class CameraTarget : public Entity {
public:
	CameraTarget() {
		x = 0;
		y = 0;
	}

	void changeAnimation(int, bool, string = "") override {}
};
