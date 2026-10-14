#include "trail/continuousTrail.h"
#include "rendering/shapes.h"

#include <algorithm>
#include <cmath>

void ContinuousTrail::reset(Vec2 startPos) {
	vertices_.clear();
	vertices_.push_back(startPos);
	vertices_.push_back(startPos);
}

void ContinuousTrail::updateHead(Vec2 currentPos) {
	if (vertices_.empty()) {
		vertices_.push_back(currentPos);
		vertices_.push_back(currentPos);
		return;
	}
	vertices_.back() = currentPos;
}

void ContinuousTrail::onTurn(Vec2 cornerPos) {
	if (vertices_.empty()) {
		vertices_.push_back(cornerPos);
		vertices_.push_back(cornerPos);
		return;
	}

	// Lock the current position as a fixed corner vertex, and push a new head point
	vertices_.back() = cornerPos;
	vertices_.push_back(cornerPos);
}

void ContinuousTrail::trim(float maxAllowedLength) {
	if (vertices_.size() < 2) return;

	float total = getLength();
	while (vertices_.size() >= 2 && total > maxAllowedLength) {
		const float excess = total - maxAllowedLength;
		const float segLen = geom::distance(vertices_[0], vertices_[1]);

		if (segLen <= excess + 0.001f) {
			total -= segLen;
			vertices_.pop_front();
		}
		else {
			const float ratio = excess / segLen;
			vertices_[0] = vertices_[0] + (vertices_[1] - vertices_[0]) * ratio;
			break;
		}
	}
}

void ContinuousTrail::clear() {
	vertices_.clear();
}

float ContinuousTrail::getLength() const {
	if (vertices_.size() < 2) return 0.0f;
	float len = 0.0f;
	for (std::size_t i = 0; i + 1 < vertices_.size(); ++i) {
		len += geom::distance(vertices_[i], vertices_[i + 1]);
	}
	return len;
}

std::vector<LineSegment> ContinuousTrail::getSegments(const Cycle* owner, bool ignoreRecentHead, float safeHeadDist) const {
	std::vector<LineSegment> result;
	if (vertices_.size() < 2) return result;

	float distFromHead = 0.0f;
	const int count = static_cast<int>(vertices_.size());

	for (int i = count - 2; i >= 0; --i) {
		Vec2 pA = vertices_[static_cast<std::size_t>(i)];
		Vec2 pB = vertices_[static_cast<std::size_t>(i + 1)];
		const float segLen = geom::distance(pA, pB);

		if (ignoreRecentHead) {
			if (distFromHead + segLen <= safeHeadDist) {
				distFromHead += segLen;
				continue; // Completely within safe head zone
			}
			if (distFromHead < safeHeadDist) {
				// Partially within safe zone: trim pB towards pA
				const float trimDist = safeHeadDist - distFromHead;
				const float ratio = (segLen - trimDist) / segLen;
				pB = pA + (pB - pA) * ratio;
				distFromHead = safeHeadDist;
			}
		}

		if (geom::distanceSq(pA, pB) > 0.01f) {
			result.push_back({ pA, pB, owner });
		}
		distFromHead += segLen;
	}

	return result;
}

void ContinuousTrail::drawThickSegment(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int thickness, SDL_Color color) {
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

	if (x1 == x2) {
		// Vertical segment
		const int minY = std::min(y1, y2);
		const int maxY = std::max(y1, y2);
		const int half = thickness / 2;
		const SDL_Rect rect = { x1 - half, minY, thickness, (maxY - minY) + 1 };
		SDL_RenderFillRect(renderer, &rect);
	}
	else if (y1 == y2) {
		// Horizontal segment
		const int minX = std::min(x1, x2);
		const int maxX = std::max(x1, x2);
		const int half = thickness / 2;
		const SDL_Rect rect = { minX, y1 - half, (maxX - minX) + 1, thickness };
		SDL_RenderFillRect(renderer, &rect);
	}
	else {
		// Arbitrary angle segment
		const float dx = static_cast<float>(x2 - x1);
		const float dy = static_cast<float>(y2 - y1);
		const float len = std::sqrt(dx * dx + dy * dy);
		if (len < 0.001f) return;

		const float perpX = -dy / len;
		const float perpY = dx / len;
		const int half = thickness / 2;

		for (int offset = -half; offset <= half; ++offset) {
			const int ox = static_cast<int>(std::round(perpX * offset));
			const int oy = static_cast<int>(std::round(perpY * offset));
			SDL_RenderDrawLine(renderer, x1 + ox, y1 + oy, x2 + ox, y2 + oy);
		}
	}
}

void ContinuousTrail::drawJointCap(SDL_Renderer* renderer, int cx, int cy, int radius, SDL_Color color) {
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
	shapes::fillCircle(renderer, cx, cy, radius);
}

void ContinuousTrail::draw(SDL_Renderer* renderer, const SDL_Rect& camera, int hudOffsetY,
	SDL_Color glowColor, SDL_Color coreColor) const {
	if (vertices_.size() < 2 || renderer == nullptr) return;

	// Pass 1: Draw outer glow along all segments and joint caps
	const int glowThickness = static_cast<int>(DEFAULT_THICKNESS);
	for (std::size_t i = 0; i + 1 < vertices_.size(); ++i) {
		const int x1 = static_cast<int>(std::round(vertices_[i].x - camera.x));
		const int y1 = static_cast<int>(std::round(vertices_[i].y - camera.y + hudOffsetY));
		const int x2 = static_cast<int>(std::round(vertices_[i + 1].x - camera.x));
		const int y2 = static_cast<int>(std::round(vertices_[i + 1].y - camera.y + hudOffsetY));

		drawThickSegment(renderer, x1, y1, x2, y2, glowThickness, glowColor);
		drawJointCap(renderer, x1, y1, glowThickness / 2, glowColor);
	}

	// Pass 2: Draw bright inner core along all segments and joint caps
	const int coreThickness = static_cast<int>(CORE_THICKNESS);
	for (std::size_t i = 0; i + 1 < vertices_.size(); ++i) {
		const int x1 = static_cast<int>(std::round(vertices_[i].x - camera.x));
		const int y1 = static_cast<int>(std::round(vertices_[i].y - camera.y + hudOffsetY));
		const int x2 = static_cast<int>(std::round(vertices_[i + 1].x - camera.x));
		const int y2 = static_cast<int>(std::round(vertices_[i + 1].y - camera.y + hudOffsetY));

		drawThickSegment(renderer, x1, y1, x2, y2, coreThickness, coreColor);
		drawJointCap(renderer, x1, y1, coreThickness / 2, coreColor);
	}
}
