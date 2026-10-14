#pragma once

#include "collision/geometry2D.h"
#include "SDL.h"

#include <deque>
#include <vector>

class ContinuousTrail {
public:
	static constexpr float DEFAULT_THICKNESS = 6.0f;
	static constexpr float CORE_THICKNESS = 2.0f;

	ContinuousTrail() = default;

	void reset(Vec2 startPos);
	void updateHead(Vec2 currentPos);
	void onTurn(Vec2 cornerPos);
	void trim(float maxAllowedLength);
	void clear();

	float getLength() const;
	bool empty() const { return vertices_.size() < 2; }
	const std::deque<Vec2>& vertices() const { return vertices_; }

	// Returns all line segments. If ignoreRecentHead is true (for self-collision),
	// segments within safeHeadDist of the head are excluded so a cycle does not hit its own tail immediately.
	std::vector<LineSegment> getSegments(const Cycle* owner, bool ignoreRecentHead = false, float safeHeadDist = 24.0f) const;

	void draw(SDL_Renderer* renderer, const SDL_Rect& camera, int hudOffsetY,
		SDL_Color glowColor, SDL_Color coreColor) const;

private:
	std::deque<Vec2> vertices_;

	static void drawThickSegment(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int thickness, SDL_Color color);
	static void drawJointCap(SDL_Renderer* renderer, int cx, int cy, int radius, SDL_Color color);
};
