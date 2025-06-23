/*
Copyright © 2014-2016 Justin Jacobs

This file is part of FLARE.

FLARE is free software: you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.

FLARE is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
FLARE.  If not, see http://www.gnu.org/licenses/
*/

/**
 * class MenuDevConsole
 */

#ifndef MENU_DEVCONSOLE_H
#define MENU_DEVCONSOLE_H

#include "CommonIncludes.h"
#include "Menu.h"
#include "Utils.h"

class WidgetButton;
class WidgetInput;
class WidgetLog;

class MenuDevConsole : public Menu {
protected:
	void loadGraphics();
	void execute();
	void getPlayerInfo();
	void getTileInfo();
	void getEntityInfo();
	void reset();

	WidgetButton *button_close;
	WidgetInput *input_box;
	WidgetLog *log_history;

	Rect history_area;

	bool first_open;

	size_t input_scrollback_pos;
	std::vector<std::string> input_scrollback;

	int set_shortcut_slot;

	float console_height;
	Rect resize_area;
	Sprite* resize_handle;
	bool dragging_resize;

	int scrollbar_w;

	std::string font_name;
	std::string font_bold_name;
	Color background_color;
	Color resize_handle_color;

public:
	MenuDevConsole();
	~MenuDevConsole();
	void align();
	void closeWindow();

	void logic();
	virtual void render();

	bool inputFocus();

	FPoint target;
	Timer distance_timer;
};

#endif
