#pragma once

#include "ui/ui.h"

#include <memory>
#include <string>

enum class HudGameStatus {
	Running,
	PlayerWon,
	PlayerLost,
	Draw
};

struct HudState {
	int score = 0;
	int lives = 3;
	int maxLives = 3;
	int enemiesAlive = 0;
	int totalEnemies = 0;
	std::string levelName;
	HudGameStatus status = HudGameStatus::Running;
	bool isRespawning = false;
	bool hasBoost = false;
	bool hasLongTrail = false;
	bool hasSlow = false;
};

class HudUi : public Ui {
public:
	static constexpr int HEIGHT = 36;

	HudUi();
	~HudUi() override;

	void setFont(const std::string& fontFile);
	void updateState(const HudState& state);
	void draw() override;

private:
	struct CachedText {
		std::string text;
		SDL_Color color{ 255, 255, 255, 255 };
		int fontSize = 0;
		SDL_Texture* texture = nullptr;
		int width = 0;
		int height = 0;

		~CachedText();
		void update(const std::string& newText, const std::string& fontPath,
			SDL_Color newColor, int newSize, SDL_Renderer* renderer);
		void draw(SDL_Renderer* renderer, int x, int y) const;
		void cleanup();
	};

	std::string fontFile_ = "fonts/vermin_vibes_1989.ttf";
	std::string fallbackFontFile_ = "fonts/Berylium.ttf";
	HudState state_;

	CachedText labelScore_;
	CachedText labelLives_;
	CachedText labelEnemies_;
	CachedText labelLevel_;
	CachedText labelStatusBanner_;
	CachedText labelStatusSub_;
	CachedText labelPowerupBoost_;
	CachedText labelPowerupTrail_;
	CachedText labelPowerupSlow_;

	std::string resolveFontPath(const std::string& relative) const;
	void drawLifePips(int x, int y, int lives, int maxLives);
};
