#pragma once

#include "ui/ui.h"

#include <string>
#include <vector>

enum class MenuState {
	Active,
	Background,
	Inactive
};

class Menu : public Ui {
public:
	int index = 0;
	MenuState menuState = MenuState::Active;

	virtual void onIndexUp() = 0;
	virtual void onIndexDown() = 0;
	virtual void onIndexLeft() {}
	virtual void onIndexRight() {}
	virtual void onSelect() {}
	virtual void onBack() {}

protected:
	std::vector<std::string> menuItems;

	virtual void drawMenuBackground() {}
	virtual void drawText() = 0;
	virtual void drawSelectionBox() {}
};
