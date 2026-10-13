#include "ui/hudUi.h"

#include "drawing_functions.h"

#include <algorithm>
#include <fstream>
#include <string>

HudUi::CachedText::~CachedText() {
	cleanup();
}

void HudUi::CachedText::cleanup() {
	if (texture != nullptr) {
		SDL_DestroyTexture(texture);
		texture = nullptr;
	}
	width = 0;
	height = 0;
	text.clear();
}

void HudUi::CachedText::update(const std::string& newText, const std::string& fontPath,
	SDL_Color newColor, int newSize, SDL_Renderer* renderer) {
	if (newText == text && newSize == fontSize
		&& newColor.r == color.r && newColor.g == color.g
		&& newColor.b == color.b && newColor.a == color.a
		&& texture != nullptr) {
		return;
	}

	cleanup();
	text = newText;
	color = newColor;
	fontSize = newSize;

	if (text.empty() || renderer == nullptr || fontPath.empty()) {
		return;
	}

	texture = renderText(text, fontPath, color, fontSize, renderer);
	if (texture != nullptr) {
		SDL_QueryTexture(texture, nullptr, nullptr, &width, &height);
	}
}

void HudUi::CachedText::draw(SDL_Renderer* renderer, int x, int y) const {
	if (texture != nullptr && renderer != nullptr) {
		const SDL_Rect dst = { x, y, width, height };
		SDL_RenderCopy(renderer, texture, nullptr, &dst);
	}
}

HudUi::HudUi() = default;

HudUi::~HudUi() = default;

void HudUi::setFont(const std::string& fontFile) {
	fontFile_ = fontFile;
}

std::string HudUi::resolveFontPath(const std::string& relative) const {
	const std::string full = resPath_ + relative;
	if (std::ifstream(full).good()) {
		return full;
	}
	const std::string fallback = resPath_ + fallbackFontFile_;
	if (std::ifstream(fallback).good()) {
		return fallback;
	}
	return "";
}

void HudUi::updateState(const HudState& state) {
	state_ = state;
}

void HudUi::drawLifePips(int x, int y, int lives, int maxLives) {
	if (renderer_ == nullptr) {
		return;
	}
	constexpr int PIP_WIDTH = 10;
	constexpr int PIP_HEIGHT = 14;
	constexpr int PIP_GAP = 5;

	for (int i = 0; i < maxLives; ++i) {
		const SDL_Rect pip = { x + i * (PIP_WIDTH + PIP_GAP), y, PIP_WIDTH, PIP_HEIGHT };
		if (i < lives) {
			SDL_SetRenderDrawColor(renderer_, 20, 235, 140, SDL_ALPHA_OPAQUE);
			SDL_RenderFillRect(renderer_, &pip);
			SDL_SetRenderDrawColor(renderer_, 200, 255, 230, SDL_ALPHA_OPAQUE);
			SDL_RenderDrawRect(renderer_, &pip);
		}
		else {
			SDL_SetRenderDrawColor(renderer_, 40, 50, 65, SDL_ALPHA_OPAQUE);
			SDL_RenderFillRect(renderer_, &pip);
			SDL_SetRenderDrawColor(renderer_, 70, 85, 105, SDL_ALPHA_OPAQUE);
			SDL_RenderDrawRect(renderer_, &pip);
		}
	}
}

void HudUi::draw() {
	if (renderer_ == nullptr) {
		return;
	}

	const std::string font = resolveFontPath(fontFile_);

	// Opaque top bar
	SDL_SetRenderDrawColor(renderer_, 8, 14, 22, SDL_ALPHA_OPAQUE);
	const SDL_Rect topBar = { 0, 0, 960, HEIGHT };
	SDL_RenderFillRect(renderer_, &topBar);
	SDL_SetRenderDrawColor(renderer_, 30, 45, 65, SDL_ALPHA_OPAQUE);
	SDL_RenderDrawLine(renderer_, 0, HEIGHT - 1, 960, HEIGHT - 1);

	// Score
	const std::string scoreText = "SCORE: " + std::to_string(state_.score);
	labelScore_.update(scoreText, font, { 20, 235, 255, 255 }, 18, renderer_);
	labelScore_.draw(renderer_, 14, 8);

	// Lives
	const std::string livesText = "LIVES:";
	labelLives_.update(livesText, font, { 220, 235, 245, 255 }, 18, renderer_);
	labelLives_.draw(renderer_, 170, 8);
	const int pipsX = labelLives_.width > 0 ? 170 + labelLives_.width + 8 : 240;
	drawLifePips(pipsX, 10, state_.lives, state_.maxLives);

	// Power-up indicators
	int badgeX = pipsX + state_.maxLives * 15 + 24;
	if (state_.hasBoost) {
		labelPowerupBoost_.update("[BOOST]", font, { 40, 255, 90, 255 }, 16, renderer_);
		labelPowerupBoost_.draw(renderer_, badgeX, 9);
		badgeX += labelPowerupBoost_.width + 10;
	}
	else {
		labelPowerupBoost_.cleanup();
	}
	if (state_.hasLongTrail) {
		labelPowerupTrail_.update("[TRAIL+]", font, { 170, 255, 40, 255 }, 16, renderer_);
		labelPowerupTrail_.draw(renderer_, badgeX, 9);
		badgeX += labelPowerupTrail_.width + 10;
	}
	else {
		labelPowerupTrail_.cleanup();
	}
	if (state_.hasSlow) {
		labelPowerupSlow_.update("[SLOW]", font, { 255, 150, 30, 255 }, 16, renderer_);
		labelPowerupSlow_.draw(renderer_, badgeX, 9);
	}
	else {
		labelPowerupSlow_.cleanup();
	}

	// Level info & enemies
	const std::string levelText = state_.levelName.empty() ? "" : state_.levelName;
	labelLevel_.update(levelText, font, { 140, 175, 205, 255 }, 16, renderer_);
	if (labelLevel_.width > 0) {
		labelLevel_.draw(renderer_, 720 - labelLevel_.width, 9);
	}

	const std::string enemiesText = "ENEMIES: " + std::to_string(state_.enemiesAlive) + "/" + std::to_string(state_.totalEnemies);
	labelEnemies_.update(enemiesText, font, { 255, 90, 110, 255 }, 18, renderer_);
	labelEnemies_.draw(renderer_, 780, 8);

	// Respawning banner
	if (state_.isRespawning && state_.status == HudGameStatus::Running) {
		labelStatusBanner_.update("RESPAWNING...", font, { 255, 235, 50, 255 }, 24, renderer_);
		const int bx = (960 - labelStatusBanner_.width) / 2;
		const int by = 260;
		SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(renderer_, 10, 15, 25, 200);
		const SDL_Rect bg = { bx - 16, by - 6, labelStatusBanner_.width + 32, labelStatusBanner_.height + 12 };
		SDL_RenderFillRect(renderer_, &bg);
		SDL_SetRenderDrawColor(renderer_, 255, 235, 50, SDL_ALPHA_OPAQUE);
		SDL_RenderDrawRect(renderer_, &bg);
		labelStatusBanner_.draw(renderer_, bx, by);
	}
	else if (state_.status != HudGameStatus::Running) {
		// End of game banners
		std::string mainText;
		std::string subText;
		SDL_Color bannerColor{ 255, 255, 255, 255 };

		switch (state_.status) {
		case HudGameStatus::PlayerWon:
			mainText = "LEVEL CLEAR!";
			subText = "PRESS ENTER FOR NEXT LEVEL";
			bannerColor = { 20, 255, 170, 255 };
			break;
		case HudGameStatus::PlayerLost:
			mainText = "GAME OVER";
			subText = "PRESS ENTER TO RETRY";
			bannerColor = { 255, 45, 75, 255 };
			break;
		case HudGameStatus::Draw:
			mainText = "DRAW!";
			subText = "PRESS ENTER TO RETRY";
			bannerColor = { 255, 190, 45, 255 };
			break;
		default:
			break;
		}

		labelStatusBanner_.update(mainText, font, bannerColor, 36, renderer_);
		labelStatusSub_.update(subText, font, { 230, 240, 255, 255 }, 18, renderer_);

		const int totalW = std::max(labelStatusBanner_.width, labelStatusSub_.width) + 40;
		const int totalH = labelStatusBanner_.height + labelStatusSub_.height + 30;
		const int boxX = (960 - totalW) / 2;
		const int boxY = (640 - totalH) / 2;

		SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(renderer_, 8, 12, 20, 230);
		const SDL_Rect modal = { boxX, boxY, totalW, totalH };
		SDL_RenderFillRect(renderer_, &modal);
		SDL_SetRenderDrawColor(renderer_, bannerColor.r, bannerColor.g, bannerColor.b, SDL_ALPHA_OPAQUE);
		SDL_RenderDrawRect(renderer_, &modal);

		labelStatusBanner_.draw(renderer_, (960 - labelStatusBanner_.width) / 2, boxY + 12);
		labelStatusSub_.draw(renderer_, (960 - labelStatusSub_.width) / 2, boxY + 16 + labelStatusBanner_.height);
	}
	else {
		labelStatusBanner_.cleanup();
		labelStatusSub_.cleanup();
	}
}
