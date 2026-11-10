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

void HudUi::setScreenSize(int width, int height) {
	screenWidth_ = std::max(1, width);
	screenHeight_ = std::max(1, height);
	scale_ = screenHeight_ / 640.0f;
	height_ = px(HEIGHT);
}

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
	const int PIP_WIDTH = px(10);
	const int PIP_HEIGHT = px(14);
	const int PIP_GAP = px(5);

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

	const int W = screenWidth_;
	const int H = screenHeight_;
	const int bar = height_;

	// Opaque top bar
	SDL_SetRenderDrawColor(renderer_, 8, 14, 22, SDL_ALPHA_OPAQUE);
	const SDL_Rect topBar = { 0, 0, W, bar };
	SDL_RenderFillRect(renderer_, &topBar);
	SDL_SetRenderDrawColor(renderer_, 30, 45, 65, SDL_ALPHA_OPAQUE);
	SDL_RenderDrawLine(renderer_, 0, bar - 1, W, bar - 1);

	// Score
	const std::string scoreText = "SCORE: " + std::to_string(state_.score);
	labelScore_.update(scoreText, font, { 20, 235, 255, 255 }, px(18), renderer_);
	const int textY = std::max(0, (bar - labelScore_.height) / 2);
	labelScore_.draw(renderer_, px(14), textY);

	// Lives
	const int livesX = px(14) + std::max(labelScore_.width, px(150)) + px(16);
	labelLives_.update("LIVES:", font, { 220, 235, 245, 255 }, px(18), renderer_);
	labelLives_.draw(renderer_, livesX, textY);
	const int pipsX = livesX + labelLives_.width + px(8);
	drawLifePips(pipsX, (bar - px(14)) / 2, state_.lives, state_.maxLives);

	// Power-up indicators
	int badgeX = pipsX + state_.maxLives * (px(10) + px(5)) + px(24);
	const int badgeY = textY + px(1);
	if (state_.hasBoost) {
		labelPowerupBoost_.update("[BOOST]", font, { 40, 255, 90, 255 }, px(16), renderer_);
		labelPowerupBoost_.draw(renderer_, badgeX, badgeY);
		badgeX += labelPowerupBoost_.width + px(10);
	}
	else {
		labelPowerupBoost_.cleanup();
	}
	if (state_.hasLongTrail) {
		labelPowerupTrail_.update("[TRAIL+]", font, { 170, 255, 40, 255 }, px(16), renderer_);
		labelPowerupTrail_.draw(renderer_, badgeX, badgeY);
		badgeX += labelPowerupTrail_.width + px(10);
	}
	else {
		labelPowerupTrail_.cleanup();
	}
	if (state_.hasSlow) {
		labelPowerupSlow_.update("[SLOW]", font, { 255, 150, 30, 255 }, px(16), renderer_);
		labelPowerupSlow_.draw(renderer_, badgeX, badgeY);
	}
	else {
		labelPowerupSlow_.cleanup();
	}

	// Level info & enemies, right-aligned
	const std::string enemiesText = "ENEMIES: " + std::to_string(state_.enemiesAlive) + "/" + std::to_string(state_.totalEnemies);
	labelEnemies_.update(enemiesText, font, { 255, 90, 110, 255 }, px(18), renderer_);
	const int enemiesX = W - labelEnemies_.width - px(14);
	labelEnemies_.draw(renderer_, enemiesX, textY);

	labelLevel_.update(state_.levelName, font, { 140, 175, 205, 255 }, px(16), renderer_);
	if (labelLevel_.width > 0) {
		labelLevel_.draw(renderer_, enemiesX - px(30) - labelLevel_.width, badgeY);
	}

	const auto drawBox = [&](const SDL_Rect& box, Uint8 alpha, SDL_Color border) {
		SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(renderer_, 10, 15, 25, alpha);
		SDL_RenderFillRect(renderer_, &box);
		SDL_SetRenderDrawColor(renderer_, border.r, border.g, border.b, SDL_ALPHA_OPAQUE);
		SDL_RenderDrawRect(renderer_, &box);
	};
	const SDL_Color yellow{ 255, 235, 50, 255 };
	const auto drawSingleBanner = [&](const char* text) {
		labelStatusBanner_.update(text, font, yellow, px(24), renderer_);
		const int bx = (W - labelStatusBanner_.width) / 2;
		const int by = H * 2 / 5;
		drawBox({ bx - px(16), by - px(6), labelStatusBanner_.width + px(32), labelStatusBanner_.height + px(12) },
			200, yellow);
		labelStatusBanner_.draw(renderer_, bx, by);
	};

	if (state_.waiting && state_.status == HudGameStatus::Running) {
		drawSingleBanner("WAITING FOR PLAYERS...");
	}
	else if (state_.spectating && state_.status == HudGameStatus::Running) {
		labelStatusBanner_.update("SPECTATING", font, yellow, px(24), renderer_);
		labelStatusSub_.update(state_.spectatorTarget.empty() ? "FOLLOWING A CYCLE"
			: "FOLLOWING: " + state_.spectatorTarget, font, { 230, 240, 255, 255 }, px(18), renderer_);
		const int boxW = std::max(labelStatusBanner_.width, labelStatusSub_.width) + px(32);
		const int bx = (W - boxW) / 2;
		const int by = bar + px(19);
		drawBox({ bx, by, boxW, labelStatusBanner_.height + labelStatusSub_.height + px(14) }, 200, yellow);
		labelStatusBanner_.draw(renderer_, (W - labelStatusBanner_.width) / 2, by + px(3));
		labelStatusSub_.draw(renderer_, (W - labelStatusSub_.width) / 2,
			by + labelStatusBanner_.height + px(5));
	}
	else if (state_.isRespawning && state_.status == HudGameStatus::Running) {
		drawSingleBanner("RESPAWNING...");
	}
	else if (state_.status != HudGameStatus::Running) {
		// End of game banners
		std::string mainText;
		std::string subText;
		SDL_Color bannerColor{ 255, 255, 255, 255 };

		switch (state_.status) {
		case HudGameStatus::PlayerWon:
			mainText = "LEVEL CLEAR!";
			subText = state_.online ? "PRESS ENTER TO RETURN TO MENU" : "PRESS ENTER FOR NEXT LEVEL";
			bannerColor = { 20, 255, 170, 255 };
			break;
		case HudGameStatus::PlayerLost:
			mainText = "GAME OVER";
			subText = state_.online ? "PRESS ENTER TO RETURN TO MENU" : "PRESS ENTER TO RETRY";
			bannerColor = { 255, 45, 75, 255 };
			break;
		case HudGameStatus::Draw:
			mainText = "DRAW!";
			subText = state_.online ? "PRESS ENTER TO RETURN TO MENU" : "PRESS ENTER TO RETRY";
			bannerColor = { 255, 190, 45, 255 };
			break;
		default:
			break;
		}

		labelStatusBanner_.update(mainText, font, bannerColor, px(36), renderer_);
		labelStatusSub_.update(subText, font, { 230, 240, 255, 255 }, px(18), renderer_);

		const int totalW = std::max(labelStatusBanner_.width, labelStatusSub_.width) + px(40);
		const int totalH = labelStatusBanner_.height + labelStatusSub_.height + px(30);
		const int boxX = (W - totalW) / 2;
		const int boxY = (H - totalH) / 2;

		drawBox({ boxX, boxY, totalW, totalH }, 230, bannerColor);

		labelStatusBanner_.draw(renderer_, (W - labelStatusBanner_.width) / 2, boxY + px(12));
		labelStatusSub_.draw(renderer_, (W - labelStatusSub_.width) / 2, boxY + px(16) + labelStatusBanner_.height);
	}
	else {
		labelStatusBanner_.cleanup();
		labelStatusSub_.cleanup();
	}
}
