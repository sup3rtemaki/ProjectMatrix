#include "systems/mainMenuSystem.h"

MainMenuSystem::MainMenuSystem(MainMenu* menu, RenderContext& context)
	: menu_(menu), context_(context) {
}

MainMenuSystem::~MainMenuSystem() = default;

bool MainMenuSystem::update(SDL_Event& event) {
	if (event.type == SDL_QUIT) {
		if (quitCallback_) quitCallback_();
		return false;
	}

	if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
		handleKeyDown(event.key.keysym.scancode);
	}

	return false;
}

void MainMenuSystem::handleKeyDown(SDL_Keycode scancode) {
	if (menu_ == nullptr) return;

	switch (scancode) {
	case SDL_SCANCODE_UP:
	case SDL_SCANCODE_W:
		menu_->onIndexUp();
		break;
	case SDL_SCANCODE_DOWN:
	case SDL_SCANCODE_S:
		menu_->onIndexDown();
		break;
	case SDL_SCANCODE_LEFT:
	case SDL_SCANCODE_A:
		menu_->onIndexLeft();
		break;
	case SDL_SCANCODE_RIGHT:
	case SDL_SCANCODE_D:
		menu_->onIndexRight();
		break;
	case SDL_SCANCODE_RETURN:
	case SDL_SCANCODE_SPACE:
		menu_->onSelect();
		break;
	case SDL_SCANCODE_ESCAPE:
		menu_->onBack();
		break;
	default:
		break;
	}
}

void MainMenuSystem::render() {
	if (menu_ == nullptr || context_.renderer == nullptr) return;
	menu_->draw();
}

void MainMenuSystem::setStartGameCallback(std::function<void(const GameConfig&)> cb) {
	startGameCallback_ = cb;
	if (menu_) menu_->setStartGameCallback(cb);
}

void MainMenuSystem::setQuitCallback(std::function<void()> cb) {
	quitCallback_ = cb;
	if (menu_) menu_->setQuitCallback(cb);
}
