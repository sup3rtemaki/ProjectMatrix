#include "ai/arenaView.h"

#include <deque>

bool ArenaView::isBlocked(int x, int y) const {
	return x < 0 || x >= width || y < 0 || y >= height
		|| occupied[static_cast<std::size_t>(y * width + x)] != 0
		|| hazards[static_cast<std::size_t>(y * width + x)] != 0;
}

int ArenaView::reachableCells(SDL_Point start, int limit) const {
	if (isBlocked(start.x, start.y)) {
		return 0;
	}

	std::vector<std::uint8_t> visited(occupied.size(), 0);
	std::deque<SDL_Point> pending{ start };
	visited[static_cast<std::size_t>(start.y * width + start.x)] = 1;

	int count = 0;
	while (!pending.empty() && count < limit) {
		const SDL_Point cell = pending.front();
		pending.pop_front();
		++count;

		const SDL_Point neighbours[] = {
			{ cell.x + 1, cell.y }, { cell.x - 1, cell.y },
			{ cell.x, cell.y + 1 }, { cell.x, cell.y - 1 } };
		for (const SDL_Point neighbour : neighbours) {
			if (isBlocked(neighbour.x, neighbour.y)) {
				continue;
			}
			std::uint8_t& seen = visited[static_cast<std::size_t>(neighbour.y * width + neighbour.x)];
			if (seen == 0) {
				seen = 1;
				pending.push_back(neighbour);
			}
		}
	}
	return count;
}
