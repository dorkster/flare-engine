/*
Copyright © 2011-2012 Clint Bellanger
Copyright © 2012 Igor Paliychuk
Copyright © 2012 Stefan Beller
Copyright © 2013-2014 Henrik Andersson
Copyright © 2013 Kurt Rinnert
Copyright © 2012-2016 Justin Jacobs

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
 * class MenuInventory
 */

#include "Avatar.h"
#include "CommonIncludes.h"
#include "EffectManager.h"
#include "EngineSettings.h"
#include "EventManager.h"
#include "FileParser.h"
#include "FontEngine.h"
#include "GameSlotPreview.h"
#include "Hazard.h"
#include "ItemManager.h"
#include "Menu.h"
#include "MenuActionBar.h"
#include "MenuHUDLog.h"
#include "MenuInventory.h"
#include "MenuManager.h"
#include "MenuPowers.h"
#include "MessageEngine.h"
#include "PowerManager.h"
#include "Settings.h"
#include "SharedGameResources.h"
#include "SharedResources.h"
#include "SoundManager.h"
#include "StatBlock.h"
#include "TooltipManager.h"
#include "UtilsParsing.h"
#include "WidgetButton.h"
#include "WidgetSlot.h"

MenuInventory::MenuInventory()
	: closeButton(new WidgetButton("images/menus/buttons/button_x.png"))
	, equipmentSetPrevious(NULL)
	, equipmentSetNext(NULL)
	, equipmentSetLabel(NULL)
	, MAX_EQUIPPED(4)
	, MAX_CARRIED(64)
	, carried_cols(4)
	, carried_rows(4)
	, tap_to_activate_timer(settings->max_frames_per_sec / 3)
	, activated_slot(-1)
	, activated_item(0)
	, preview(NULL)
	, preview_enabled(false)
	, active_equipment_set(0)
	, max_equipment_set(0)
	, currency(0)
	, drag_prev_src(-1)
	, changed_equipment(true)
	, inv_ctrl(CTRL_NONE)
	, show_book("")
{
	visible = false;

	// raw data for equipment swap buttons
	std::map<unsigned, std::string> raw_set_button;
	std::string raw_previous;
	std::string raw_next;
	std::string raw_label;

	// Load config settings
	FileParser infile;
	// @CLASS MenuInventory|Description of menus/inventory.txt
	if (infile.open("menus/inventory.txt", FileParser::MOD_FILE, FileParser::ERROR_NORMAL)) {
		while(infile.next()) {
			if (parseMenuKey(infile.key, infile.val))
				continue;

			// @ATTR close|point|Position of the close button.
			if(infile.key == "close") {
				Point pos = Parse::toPoint(infile.val);
				closeButton->setBasePos(pos.x, pos.y, Utils::ALIGN_TOPLEFT);
			}
			// @ATTR set_button|int, int, int, filename : ID, Widget X, Widget Y, Image file|Set number, position and image filename for an equipment swap set button.
			else if(infile.key == "set_button") {
				unsigned id = static_cast<unsigned>(Parse::popFirstInt(infile.val));
				std::pair<unsigned, std::string> set_button(id, infile.val);
				raw_set_button.insert(set_button);
				raw_set_button[id] = infile.val;
			}
			// @ATTR set_previous|int, int, filename : Widget X, Widget Y, Image file|Position and image filename for an equipment swap set previous button.
			else if(infile.key == "set_previous") {
				raw_previous = infile.val;
			}
			// @ATTR set_next|int, int, filename : Widget X, Widget Y, Image file|Position and image filename for an equipment swap set next button.
			else if(infile.key == "set_next") {
				raw_next = infile.val;
			}
			// @ATTR label_equipment_set|label|Label showing the active equipment set.
			else if(infile.key == "label_equipment_set") {
				raw_label = infile.val;
			}
			// @ATTR equipment_slot|repeatable(int, int, string, int) : X, Y, Slot Type, Equipment set|Position, item type and equipment set number of an equipment slot. Equipment set number is "0" for shared items."
			else if(infile.key == "equipment_slot") {
				Rect area;
				Point pos;
				size_t slt_type;
				int eq_set;

				pos.x = area.x = Parse::popFirstInt(infile.val);
				pos.y = area.y = Parse::popFirstInt(infile.val);
				slt_type = items->getItemTypeIndexByString(Parse::popFirstString(infile.val));
				eq_set = Parse::popFirstInt(infile.val);
				area.w = area.h = eset->resolutions.icon_size;

				equipped_area.push_back(area);
				equipped_pos.push_back(pos);
				slot_type.push_back(slt_type);
				equipment_set.push_back(eq_set);
			}
			// @ATTR carried_area|point|Position of the first normal inventory slot.
			else if(infile.key == "carried_area") {
				Point pos;
				carried_pos.x = carried_area.x = Parse::popFirstInt(infile.val);
				carried_pos.y = carried_area.y = Parse::popFirstInt(infile.val);
			}
			// @ATTR carried_cols|int|The number of columns for the normal inventory.
			else if (infile.key == "carried_cols") carried_cols = std::max(1, Parse::toInt(infile.val));
			// @ATTR carried_rows|int|The number of rows for the normal inventory.
			else if (infile.key == "carried_rows") carried_rows = std::max(1, Parse::toInt(infile.val));
			// @ATTR label_title|label|Position of the "Inventory" label.
			else if (infile.key == "label_title") {
				label_inventory.setFromLabelInfo(Parse::popLabelInfo(infile.val));
			}
			// @ATTR currency|label|Position of the label that displays the total currency being carried.
			else if (infile.key == "currency") {
				label_currency.setFromLabelInfo(Parse::popLabelInfo(infile.val));
			}
			// @ATTR help|rectangle|A mouse-over area that displays some help text for inventory shortcuts.
			else if (infile.key == "help") help_pos = Parse::toRect(infile.val);

			// @ATTR preview_enabled|bool|When enabled, the player is drawn in the inventory menu as they appear in the game world. Disabled by default.
			else if (infile.key == "preview_enabled") preview_enabled = Parse::toBool(infile.val);
			// @ATTR preview_pos|point|Position of the preview image. The character is drawn so that this point should lie between their feet, or thereabouts.
			else if (infile.key == "preview_pos") preview_pos = Parse::toPoint(infile.val);

			else infile.error("MenuInventory: '%s' is not a valid key.", infile.key.c_str());
		}
		infile.close();
	}

	MAX_EQUIPPED = static_cast<int>(equipped_area.size());
	MAX_CARRIED = carried_cols * carried_rows;

	carried_area.w = carried_cols * eset->resolutions.icon_size;
	carried_area.h = carried_rows * eset->resolutions.icon_size;

	label_inventory.setText(msg->get("Inventory"));
	label_inventory.setColor(font->getColor(FontEngine::COLOR_MENU_NORMAL));

	label_currency.setColor(font->getColor(FontEngine::COLOR_MENU_NORMAL));

	inventory[EQUIPMENT].initFromList(MAX_EQUIPPED, equipped_area, slot_type);
	inventory[CARRIED].initGrid(MAX_CARRIED, carried_area, carried_cols);

	for (int i = 0; i < MAX_EQUIPPED; i++) {
		tablist.add(inventory[EQUIPMENT].slots[i]);
	}
	for (int i = 0; i < MAX_CARRIED; i++) {
		tablist.add(inventory[CARRIED].slots[i]);
	}

	for (size_t i=0; i<equipment_set.size(); i++) {
		if (equipment_set[i] > max_equipment_set) {
			max_equipment_set = equipment_set[i];
		}
	}

	// create equipment swap buttons
	std::map<unsigned, std::string>::iterator it;
	for (it = raw_set_button.begin(); it != raw_set_button.end(); ++it) {
		int px = Parse::popFirstInt(it->second);
		int py = Parse::popFirstInt(it->second);
		std::string icon = Parse::popFirstString(it->second);
		equipmentSetButton.push_back(new WidgetButton(icon));
		equipmentSetButton.back()->setBasePos(px, py, Utils::ALIGN_TOPLEFT);
		tablist.add(equipmentSetButton.back());
	}
	raw_set_button.clear();

	if (!raw_previous.empty()) {
		int px = Parse::popFirstInt(raw_previous);
		int py = Parse::popFirstInt(raw_previous);
		std::string icon = Parse::popFirstString(raw_previous);
		equipmentSetPrevious = new WidgetButton(icon);
		equipmentSetPrevious->setBasePos(px, py, Utils::ALIGN_TOPLEFT);
		tablist.add(equipmentSetPrevious);
	}

	if (!raw_next.empty()) {
		int px = Parse::popFirstInt(raw_next);
		int py = Parse::popFirstInt(raw_next);
		std::string icon = Parse::popFirstString(raw_next);
		equipmentSetNext = new WidgetButton(icon);
		equipmentSetNext->setBasePos(px, py, Utils::ALIGN_TOPLEFT);
		tablist.add(equipmentSetNext);
	}

	if (!raw_label.empty()) {
		equipmentSetLabel = new WidgetLabel;
		equipmentSetLabel->setFromLabelInfo(Parse::popLabelInfo(raw_label));

		std::stringstream label;
		label << active_equipment_set << "/" << max_equipment_set;
		equipmentSetLabel->setText(label.str());
		equipmentSetLabel->setColor(font->getColor(FontEngine::COLOR_MENU_NORMAL));
	}

	if (max_equipment_set > 0) {
		applyEquipmentSet(1);
	}

	if (!background)
		setBackground("images/menus/inventory.png");

	if (preview_enabled) {
		preview = new GameSlotPreview();
		preview->setStatBlock(&pc->stats);
		preview->setDirection(6); // face forward
		preview->loadDefaultGraphics();
		preview->loadGraphicsFromInventory(this);
	}

	align();
}

void MenuInventory::align() {
	Menu::align();

	for (int i=0; i<MAX_EQUIPPED; i++) {
		equipped_area[i].x = equipped_pos[i].x + window_area.x;
		equipped_area[i].y = equipped_pos[i].y + window_area.y;
	}

	carried_area.x = carried_pos.x + window_area.x;
	carried_area.y = carried_pos.y + window_area.y;

	inventory[EQUIPMENT].setPos(window_area.x, window_area.y);
	inventory[CARRIED].setPos(window_area.x, window_area.y);

	closeButton->setPos(window_area.x, window_area.y);

	if (!equipmentSetButton.empty()) {
		for (size_t i=0; i<equipmentSetButton.size(); i++) {
			equipmentSetButton[i]->setPos(window_area.x, window_area.y);
		}
	}

	if (equipmentSetPrevious) equipmentSetPrevious->setPos(window_area.x, window_area.y);
	if (equipmentSetNext) equipmentSetNext->setPos(window_area.x, window_area.y);
	if (equipmentSetLabel) equipmentSetLabel->setPos(window_area.x, window_area.y);

	label_inventory.setPos(window_area.x, window_area.y);
	label_currency.setPos(window_area.x, window_area.y);

	if (preview)
		preview->setPos(Point(window_area.x + preview_pos.x, window_area.y + preview_pos.y));
}

void MenuInventory::logic() {

	// if the player has just died, the penalty is half his current currency.
	if (pc->stats.death_penalty && eset->death_penalty.enabled) {
		std::string death_message = "";

		// remove a % of currency
		if (eset->death_penalty.currency > 0) {
			if (currency > 0)
				removeCurrency(static_cast<int>((static_cast<float>(currency) * eset->death_penalty.currency) / 100.f));
			death_message += msg->getv("Lost %s%% of %s.", Utils::floatToString(eset->death_penalty.currency, eset->number_format.death_penalty).c_str(), eset->loot.currency.c_str()) + ' ';
		}

		// remove a % of either total xp or xp since the last level
		if (eset->death_penalty.xp > 0) {
			if (pc->stats.xp > 0)
				pc->stats.xp -= static_cast<int>((static_cast<float>(pc->stats.xp) * eset->death_penalty.xp) / 100.f);
			death_message += msg->getv("Lost %s%% of total XP.", Utils::floatToString(eset->death_penalty.xp, eset->number_format.death_penalty).c_str()) + ' ';
		}
		else if (eset->death_penalty.xp_current > 0) {
			if (pc->stats.xp - eset->xp.getLevelXP(pc->stats.level) > 0)
				pc->stats.xp -= static_cast<int>((static_cast<float>(pc->stats.xp - eset->xp.getLevelXP(pc->stats.level)) * eset->death_penalty.xp_current) / 100.f);
			death_message += msg->getv("Lost %s%% of current level XP.", Utils::floatToString(eset->death_penalty.xp_current, eset->number_format.death_penalty).c_str()) + ' ';
		}

		// prevent down-leveling from removing too much xp
		if (pc->stats.xp < eset->xp.getLevelXP(pc->stats.level))
			pc->stats.xp = eset->xp.getLevelXP(pc->stats.level);

		// remove a random carried item
		if (eset->death_penalty.item) {
			std::vector<ItemID> removable_items;
			removable_items.clear();
			for (int i=0; i < MAX_EQUIPPED; i++) {
				if (!inventory[EQUIPMENT][i].empty() && items->isValid(inventory[EQUIPMENT][i].item)) {
					if (!items->items[inventory[EQUIPMENT][i].item]->quest_item)
						removable_items.push_back(inventory[EQUIPMENT][i].item);
				}
			}
			for (int i=0; i < MAX_CARRIED; i++) {
				if (!inventory[CARRIED][i].empty() && items->isValid(inventory[CARRIED][i].item)) {
					if (!items->items[inventory[CARRIED][i].item]->quest_item)
						removable_items.push_back(inventory[CARRIED][i].item);
				}
			}
			if (!removable_items.empty()) {
				size_t random_item = static_cast<size_t>(rand()) % removable_items.size();
				remove(removable_items[random_item], 1);
				death_message += msg->getv("Lost %s.",items->getItemName(removable_items[random_item]).c_str());
			}
		}

		pc->logMsg(death_message, Avatar::MSG_NORMAL);

		pc->stats.death_penalty = false;
	}

	// a copy of currency is kept in stats, to help with various situations
	pc->stats.currency = currency = inventory[CARRIED].count(eset->misc.currency_id);

	if (visible) {
		tablist.logic();

		// check close button
		if (closeButton->checkClick()) {
			visible = false;
			snd->play(sfx_close, snd->DEFAULT_CHANNEL, snd->NO_POS, !snd->LOOP);
		}

		if (drag_prev_src == -1) {
			clearHighlight();
		}

		//check equipment set buttons
		if (!equipmentSetButton.empty()) {
			for (size_t i=0; i<equipmentSetButton.size(); i++) {
				if(equipmentSetButton[i]->checkClick()) {
					applyEquipmentSet(static_cast<unsigned>(i)+1);
					applyEquipment();
				}
			}
		}

		if (equipmentSetNext) {
			if (equipmentSetNext->checkClick()) {
				applyNextEquipmentSet();
				applyEquipment();
			}
		}

		if (equipmentSetPrevious) {
			if (equipmentSetPrevious->checkClick()) {
				applyPreviousEquipmentSet();
				applyEquipment();
			}
		}
	}

	if (max_equipment_set > 0) {
		if (inpt->pressing[Input::EQUIPMENT_SWAP] && !inpt->lock[Input::EQUIPMENT_SWAP]) {
			inpt->lock[Input::EQUIPMENT_SWAP] = true;
			applyNextEquipmentSet();
			applyEquipment();
			clearHighlight();
		}
		else if (inpt->pressing[Input::EQUIPMENT_SWAP_PREV] && !inpt->lock[Input::EQUIPMENT_SWAP_PREV]) {
			inpt->lock[Input::EQUIPMENT_SWAP_PREV] = true;
			applyPreviousEquipmentSet();
			applyEquipment();
			clearHighlight();
		}
	}

	tap_to_activate_timer.tick();

	if (preview)
		preview->logic();
}

void MenuInventory::render() {
	if (!visible) return;

	// background
	Menu::render();

	// close button
	closeButton->render();

	// equipment set buttons
	if (!equipmentSetButton.empty()) {
		for (size_t i=0; i<equipmentSetButton.size(); i++) {
			equipmentSetButton[i]->render();
		}
	}
	if (equipmentSetPrevious) equipmentSetPrevious->render();
	if (equipmentSetNext) equipmentSetNext->render();
	if (equipmentSetLabel) equipmentSetLabel->render();

	// text overlay
	label_inventory.render();

	if (!label_currency.isHidden()) {
		label_currency.setText(msg->getv("%d %s", currency, eset->loot.currency.c_str()));
		label_currency.render();
	}

	inventory[EQUIPMENT].render();
	inventory[CARRIED].render();

	if (preview)
		preview->render();
}

int MenuInventory::areaOver(const Point& position) {
	if (Utils::isWithinRect(carried_area, position)) {
		return CARRIED;
	}
	else {
		for (unsigned i=0; i<equipped_area.size(); i++) {
			if (Utils::isWithinRect(equipped_area[i], position)) {
				return EQUIPMENT;
			}
		}
	}

	// point is inside the inventory menu, but not over a slot
	if (Utils::isWithinRect(window_area, position)) {
		return NO_AREA;
	}

	return -2;
}

/**
 * If mousing-over an item with a tooltip, return that tooltip data.
 *
 * @param mouse The x,y screen coordinates of the mouse cursor
 */
void MenuInventory::renderTooltips(const Point& position) {
	if (!visible || !Utils::isWithinRect(window_area, position))
		return;

	int area = areaOver(position);
	int slot = -1;
	TooltipData tip_data;

	if (area < 0) {
		if (position.x >= window_area.x + help_pos.x && position.y >= window_area.y+help_pos.y && position.x < window_area.x+help_pos.x+help_pos.w && position.y < window_area.y+help_pos.y+help_pos.h) {
			tip_data.addText(msg->get("Pick up item(s):") + " " + inpt->getBindingString(Input::MAIN1));
			tip_data.addText(msg->get("Use or equip item:") + " " + inpt->getBindingString(Input::MAIN2) + "\n");
			tip_data.addText(msg->getv("%s modifiers", inpt->getBindingString(Input::MAIN1).c_str()));
			tip_data.addText(msg->get("Select a quantity of item:") + " " + inpt->getBindingString(Input::SHIFT));

			if (inv_ctrl == CTRL_STASH)
				tip_data.addText(msg->get("Stash item stack:") + " " + inpt->getBindingString(Input::CTRL));
			else if (inv_ctrl == CTRL_VENDOR || eset->misc.sell_without_vendor)
				tip_data.addText(msg->get("Sell item stack:") + " " + inpt->getBindingString(Input::CTRL));
		}
		tooltipm->push(tip_data, position, TooltipData::STYLE_FLOAT);
	}
	else {
		slot = inventory[area].slotOver(position);
	}

	if (slot == -1)
		return;

	tip_data.clear();

	if (area == EQUIPMENT)
		if (!isActive(slot))
			return;

	if (inventory[area][slot].item > 0) {
		tip_data = inventory[area].checkTooltip(position, &pc->stats, ItemManager::PLAYER_INV, ItemManager::TOOLTIP_INPUT_HINT);
	}
	else if (area == EQUIPMENT && inventory[area][slot].empty()) {
		tip_data.addText(msg->get(items->getItemType(slot_type[slot]).name));
	}

	tooltipm->push(tip_data, position, TooltipData::STYLE_FLOAT);
}

/**
 * Click-start dragging in the inventory
 */
ItemStack MenuInventory::click(const Point& position) {
	ItemStack item;

	drag_prev_src = areaOver(position);
	if (drag_prev_src > -1) {
		item = inventory[drag_prev_src].click(position);

		if (inpt->usingTouchscreen()) {
			tablist.setCurrent(inventory[drag_prev_src].current_slot);
			tap_to_activate_timer.reset(Timer::BEGIN);
		}

		if (item.empty()) {
			drag_prev_src = -1;
			return item;
		}

		// if dragging equipment, prepare to change stats/sprites
		if (drag_prev_src == EQUIPMENT) {
			if (pc->stats.humanoid) {
				updateEquipment(inventory[EQUIPMENT].drag_prev_slot);
			}
			else {
				itemReturn(item);
				item.clear();
			}
		}
	}

	return item;
}

/**
 * Return dragged item to previous slot
 */
void MenuInventory::itemReturn(ItemStack stack) {
	if (drag_prev_src == -1) {
		add(stack, CARRIED, ItemStorage::NO_SLOT, !ADD_PLAY_SOUND, !ADD_AUTO_EQUIP);
	}
	else {
		int prev_slot = inventory[drag_prev_src].drag_prev_slot;
		inventory[drag_prev_src].itemReturn(stack);
		// if returning equipment, prepare to change stats/sprites
		if (drag_prev_src == EQUIPMENT) {
			updateEquipment(prev_slot);
		}
	}
	drag_prev_src = -1;
}

/**
 * Dragging and dropping an item can be used to rearrange the inventory
 * and equip items
 */
bool MenuInventory::drop(const Point& position, ItemStack stack) {
	items->playSound(stack.item);

	bool success = true;

	int area = areaOver(position);
	if (area < 0) {
		if (drag_prev_src == -1) {
			success = add(stack, CARRIED, ItemStorage::NO_SLOT, !ADD_PLAY_SOUND, ADD_AUTO_EQUIP);
		}
		else {
			// not dropped into a slot. Just return it to the previous slot.
			itemReturn(stack);
		}
		return success;
	}

	int slot = inventory[area].slotOver(position);
	if (slot == -1) {
		if (drag_prev_src == -1) {
			success = add(stack, CARRIED, ItemStorage::NO_SLOT, !ADD_PLAY_SOUND, ADD_AUTO_EQUIP);
		}
		else {
			// not dropped into a slot. Just return it to the previous slot.
			itemReturn(stack);
		}
		return success;
	}

	int drag_prev_slot = -1;
	if (drag_prev_src != -1)
		drag_prev_slot = inventory[drag_prev_src].drag_prev_slot;

	if (area == EQUIPMENT) { // dropped onto equipped item

		// make sure the item is going to the correct slot
		// we match slot_type to stack.item's type to place items in the proper slots
		// also check to see if the hero meets the requirements
		if (items->isValid(stack.item) && slot_type[slot] == items->items[stack.item]->type && items->requirementsMet(&pc->stats, stack.item) && pc->stats.humanoid && inventory[EQUIPMENT].slots[slot]->enabled) {
			if (inventory[area][slot].item == stack.item) {
				// Merge the stacks
				success = add(stack, area, slot, !ADD_PLAY_SOUND, !ADD_AUTO_EQUIP);
			}
			else {
				// Swap the two stacks
				if (!inventory[area][slot].empty())
					itemReturn(inventory[area][slot]);
				inventory[area][slot] = stack;
				updateEquipment(slot);
				applyEquipment();

				// if this item has a power, place it on the action bar if possible
				if (items->items[stack.item]->power > 0) {
					menu_act->addPower(items->items[stack.item]->power, 0);
				}
			}
		}
		else {
			// equippable items only belong to one slot, for the moment
			itemReturn(stack); // cancel
			updateEquipment(slot);
			applyEquipment();
		}
	}
	else if (area == CARRIED) {
		// dropped onto carried item

		if (drag_prev_src == CARRIED) {
			if (slot != drag_prev_slot) {
				if (inventory[area][slot].item == stack.item) {
					// Merge the stacks
					success = add(stack, area, slot, !ADD_PLAY_SOUND, !ADD_AUTO_EQUIP);
				}
				else if (inventory[area][slot].empty()) {
					// Drop the stack
					inventory[area][slot] = stack;
				}
				else if (drag_prev_slot != -1 && inventory[drag_prev_src][drag_prev_slot].empty()) {
					// Check if the previous slot is free (could still be used if SHIFT was used).
					// Swap the two stacks
					itemReturn( inventory[area][slot]);
					inventory[area][slot] = stack;
				}
				else {
					itemReturn( stack);
				}
			}
			else {
				itemReturn( stack); // cancel

				// allow reading books on touchscreen devices
				// since touch screens don't have right-click, we use a "tap" (drop on same slot quickly) to activate
				// NOTE: the quantity must be 1, since the number picker appears when tapping on a stack of more than 1 item
				// NOTE: we only support activating books since equipment activation doesn't work for some reason
				// NOTE: Consumables are usually in stacks > 1, so we ignore those as well for consistency
				if (inpt->usingTouchscreen() && !tap_to_activate_timer.isEnd() && stack.quantity == 1 && items->isValid(stack.item) && !items->items[stack.item]->book.empty()) {
					activate(position);
				}
			}
		}
		else {
			if (inventory[area][slot].item == stack.item || drag_prev_src == -1) {
				// Merge the stacks
				success = add(stack, area, slot, !ADD_PLAY_SOUND, !ADD_AUTO_EQUIP);
			}
			else if (inventory[area][slot].empty()) {
				// Drop the stack
				inventory[area][slot] = stack;
			}
			else if (
				inventory[EQUIPMENT][drag_prev_slot].empty()
				&& inventory[CARRIED][slot].item != stack.item
				&& items->isValid(inventory[CARRIED][slot].item)
				&& items->items[inventory[CARRIED][slot].item]->type == slot_type[drag_prev_slot]
				&& items->requirementsMet(&pc->stats, inventory[CARRIED][slot].item)
			)
			{
				// The whole equipped stack is dropped on an empty carried slot or on a wearable item
				// Swap the two stacks
				itemReturn(inventory[area][slot]);
				updateEquipment(drag_prev_slot);

				// if this item has a power, place it on the action bar if possible
				if (items->items[inventory[EQUIPMENT][drag_prev_slot].item]->power > 0) {
					menu_act->addPower(items->items[inventory[EQUIPMENT][drag_prev_slot].item]->power, 0);
				}

				inventory[area][slot] = stack;

				applyEquipment();
			}
			else {
				itemReturn(stack); // cancel
			}
		}
	}

	drag_prev_src = -1;

	return success;
}

/**
 * Right-clicking on a usable item in the inventory causes it to activate.
 * e.g. drink a potion
 * e.g. equip an item
 */
void MenuInventory::activate(const Point& position) {
	FPoint nullpt;
	nullpt.x = nullpt.y = 0;

	// clicked a carried item
	int slot = inventory[CARRIED].slotOver(position);
	if (slot == -1)
		return;

	ItemStack& stack = inventory[CARRIED][slot];

	// empty slot
	if (stack.empty() || !items->isValid(stack.item))
		return;

	// run the item's script if it has one
	if (!items->items[stack.item]->script.empty()) {
		EventManager::executeScript(items->items[stack.item]->script, pc->stats.pos.x, pc->stats.pos.y);
	}
	// if the item is a book, open it
	else if (!items->items[stack.item]->book.empty()) {
		show_book = items->items[stack.item]->book;
	}
	// use a power attached to a non-equipment item
	else if (powers->isValid(items->items[stack.item]->power) && getEquipSlotFromItem(stack.item, !ONLY_EMPTY_SLOTS) == -1) {
		PowerID power_id = items->items[stack.item]->power;
		Power* item_power = powers->powers[power_id];

		// equipment might want to replace powers, so do it here
		for (int i = 0; i < inventory[EQUIPMENT].getSlotNumber(); ++i) {
			ItemID id = inventory[EQUIPMENT][i].item;
			if (id == 0 || !items->items[id])
				continue;

			for (size_t j = 0; j < items->items[id]->replace_power.size(); ++j) {
				if (power_id == items->items[id]->replace_power[j].first) {
					power_id = items->items[id]->replace_power[j].second;
					break;
				}
			}
		}

		// if the power consumes items, make sure we have enough
		for (size_t i = 0; i < item_power->required_items.size(); ++i) {
			if (item_power->required_items[i].id > 0 &&
			    item_power->required_items[i].quantity > inventory[CARRIED].count(item_power->required_items[i].id))
			{
				pc->logMsg(msg->get("You don't have enough of the required item."), Avatar::MSG_NORMAL);
				return;
			}

			if (item_power->required_items[i].id == stack.item) {
				activated_slot = slot;
				activated_item = stack.item;
			}
		}

		// check power & item requirements
		if (!pc->stats.canUsePower(power_id, !StatBlock::CAN_USE_PASSIVE) || !pc->power_cooldown_timers[power_id]->isEnd()) {
			pc->logMsg(msg->get("You can't use this item right now."), Avatar::MSG_NORMAL);
			return;
		}

		// if this item requires targeting it can't be used this way
		if (!item_power->requires_targeting) {
			ActionData action_data;
			action_data.power = power_id;
			action_data.activated_from_inventory = true;

			action_data.target = Utils::calcVector(pc->stats.pos, pc->stats.direction, pc->stats.melee_range);

			if (item_power->new_state == Power::STATE_INSTANT) {
				for (size_t j = 0; j < item_power->required_items.size(); ++j) {
					if (item_power->required_items[j].id > 0 && !item_power->required_items[j].equipped) {
						action_data.instant_item = true;
						break;
					}
				}
			}

			pc->action_queue.push_back(action_data);
		}
		else {
			// let player know this can only be used from the action bar
			pc->logMsg(msg->get("This item can only be used from the action bar."), Avatar::MSG_NORMAL);
		}

	}
	// equip an item
	else if (pc->stats.humanoid && !items->getItemType(items->items[stack.item]->type).name.empty()) {
		int equip_slot = getEquipSlotFromItem(inventory[CARRIED].storage[slot].item, !ONLY_EMPTY_SLOTS);

		if (equip_slot >= 0) {
			ItemStack active_stack = click(position);

			if (inventory[EQUIPMENT][equip_slot].item == active_stack.item) {
				// Merge the stacks
				add(active_stack, EQUIPMENT, equip_slot, !ADD_PLAY_SOUND, !ADD_AUTO_EQUIP);
			}
			else if (inventory[EQUIPMENT][equip_slot].empty()) {
				// Drop the stack
				inventory[EQUIPMENT][equip_slot] = active_stack;
			}
			else {
				if (stack.empty()) { // Don't forget this slot may have been emptied by the click()
					// Swap the two stacks
					itemReturn(inventory[EQUIPMENT][equip_slot]);
				}
				else {
					// Drop the equipped item anywhere
					add(inventory[EQUIPMENT][equip_slot], CARRIED, ItemStorage::NO_SLOT, ADD_PLAY_SOUND, !ADD_AUTO_EQUIP);
				}
				inventory[EQUIPMENT][equip_slot] = active_stack;
			}

			updateEquipment(equip_slot);
			items->playSound(inventory[EQUIPMENT][equip_slot].item);

			// if this item has a power, place it on the action bar if possible
			if (items->items[active_stack.item]->power > 0) {
				menu_act->addPower(items->items[active_stack.item]->power, 0);
			}

			applyEquipment();
		}
		else if (equip_slot == -1) {
			Utils::logError("MenuInventory: Can't find equip slot, corresponding to type %s", items->getItemType(items->items[stack.item]->type).id.c_str());
		}
	}

	drag_prev_src = -1;
}

/**
 * Insert item into first available carried slot, preferably in the optionnal specified slot
 *
 * @param ItemStack Stack of items
 * @param area Area number where it will try to store the item
 * @param slot Slot number where it will try to store the item
 */
bool MenuInventory::add(ItemStack stack, int area, int slot, bool play_sound, bool auto_equip) {
	if (stack.empty())
		return true;

	if (!items->isValid(stack.item))
		return false;

	bool success = true;

	if (play_sound)
		items->playSound(stack.item);

	if (auto_equip && settings->auto_equip) {
		int equip_slot = getEquipSlotFromItem(stack.item, ONLY_EMPTY_SLOTS);
		bool disabled_slots_empty = true;

		// if this item would disable non-empty slots, don't auto-equip it
		for (size_t i = 0; i < items->items[stack.item]->disable_slots.size(); ++i) {
			for (int j = 0; j < MAX_EQUIPPED; ++j) {
				if (!inventory[EQUIPMENT].storage[j].empty() && slot_type[j] == items->items[stack.item]->disable_slots[i]) {
					disabled_slots_empty = false;
				}
			}
		}

		if (equip_slot >= 0 && inventory[EQUIPMENT].slots[equip_slot]->enabled && disabled_slots_empty) {
			area = EQUIPMENT;
			slot = equip_slot;
		}

	}

	if (area == CARRIED) {
		ItemStack leftover = inventory[CARRIED].add(stack, slot);
		if (!leftover.empty()) {
			if (items->items[stack.item]->quest_item) {
				// quest items can't be dropped, so find a non-quest item in the inventory to drop
				const int max_q = items->items[stack.item]->max_quantity;
				int slots_to_clear = 1;
				if (max_q > 0)
					slots_to_clear = leftover.quantity + (leftover.quantity % max_q) / max_q;

				for (int i = MAX_CARRIED-1; i >=0; --i) {
					if (items->items[inventory[CARRIED].storage[i].item]->quest_item)
						continue;

					drop_stack.push(inventory[CARRIED].storage[i]);
					inventory[CARRIED].storage[i].clear();

					slots_to_clear--;
					if (slots_to_clear <= 0)
						break;
				}

				if (slots_to_clear > 0) {
					// inventory is full of quest items! we have to drop this now...
					drop_stack.push(leftover);
				}
				else {
					add(leftover, CARRIED, slot, !ADD_PLAY_SOUND, !ADD_AUTO_EQUIP);
				}
			}
			else {
				drop_stack.push(leftover);
			}
			pc->logMsg(msg->get("Inventory is full."), Avatar::MSG_NORMAL);
			success = false;
		}
	}
	else if (area == EQUIPMENT) {
		ItemStack &dest = inventory[EQUIPMENT].storage[slot];
		ItemStack leftover;
		leftover.item = stack.item;

		if (!dest.empty() && dest.item != stack.item) {
			// items don't match, so just add the stack to the carried area
			leftover.quantity = stack.quantity;
		}
		else if (dest.quantity + stack.quantity > items->items[stack.item]->max_quantity) {
			// items match, so attempt to merge the stacks. Any leftover will be added to the carried area
			leftover.quantity = dest.quantity + stack.quantity - items->items[stack.item]->max_quantity;
			stack.quantity = items->items[stack.item]->max_quantity - dest.quantity;
			if (stack.quantity > 0) {
				add(stack, EQUIPMENT, slot, !ADD_PLAY_SOUND, !ADD_AUTO_EQUIP);
			}
		}
		else {
			// put the item in the appropriate equipment slot
			inventory[EQUIPMENT].add(stack, slot);
			updateEquipment(slot);
			leftover.clear();
		}

		if (!leftover.empty()) {
			add(leftover, CARRIED, ItemStorage::NO_SLOT, !ADD_PLAY_SOUND, !ADD_AUTO_EQUIP);
		}

		applyEquipment();
	}

	// if this item has a power, place it on the action bar if possible
	if (success && items->getItemType(items->items[stack.item]->type).id == "consumable" && items->items[stack.item]->power > 0) {
		menu_act->addPower(items->items[stack.item]->power, 0);
	}

	drag_prev_src = -1;

	return success;
}

/**
 * Remove one given item from the player's inventory.
 */
bool MenuInventory::remove(ItemID item, int quantity) {
	if (activated_item != 0 && activated_slot != -1 && item == activated_item) {
		inventory[CARRIED].subtract(activated_slot, 1);
		activated_item = 0;
		activated_slot = -1;
	}
	else if(!inventory[CARRIED].remove(item, quantity)) {
		if (!inventory[EQUIPMENT].remove(item, quantity)) {
			return false;
		}
		else {
			applyEquipment();
		}
	}

	return true;
}

void MenuInventory::removeFromPrevSlot(int quantity) {
	if (drag_prev_src > -1 && inventory[drag_prev_src].drag_prev_slot > -1) {
		int drag_prev_slot = inventory[drag_prev_src].drag_prev_slot;
		inventory[drag_prev_src].subtract(drag_prev_slot, quantity);
		if (inventory[drag_prev_src].storage[drag_prev_slot].empty()) {
			if (drag_prev_src == EQUIPMENT)
				updateEquipment(inventory[EQUIPMENT].drag_prev_slot);
		}
	}
}

/**
 * Add currency item
 */
void MenuInventory::addCurrency(int count) {
	if (count > 0) {
		ItemStack stack;
		stack.item = eset->misc.currency_id;
		stack.quantity = count;
		add(stack, CARRIED, ItemStorage::NO_SLOT, !ADD_PLAY_SOUND, !ADD_AUTO_EQUIP);
	}
}

/**
 * Remove currency item
 */
void MenuInventory::removeCurrency(int count) {
	inventory[CARRIED].remove(eset->misc.currency_id, count);
}

/**
 * Check if there is enough currency to buy the given stack, and if so remove it from the current total and add the stack.
 * (Handle the drop into the equipment area, but add() don't handle it well in all circonstances. MenuManager::logic() allow only into the carried area.)
 */
bool MenuInventory::buy(ItemStack stack, int tab, bool dragging) {
	if (stack.empty()) {
		return true;
	}

	if (!items->isValid(stack.item))
		return false;

	int value_each;
	if (tab == ItemManager::VENDOR_BUY) value_each = items->items[stack.item]->getPrice(ItemManager::USE_VENDOR_RATIO);
	else value_each = items->items[stack.item]->getSellPrice(stack.can_buyback);

	int count = value_each * stack.quantity;
	if( inventory[CARRIED].count(eset->misc.currency_id) >= count) {
		stack.can_buyback = false;

		if (dragging) {
			drop(inpt->mouse, stack);
		}
		else {
			add(stack, CARRIED, ItemStorage::NO_SLOT, ADD_PLAY_SOUND, ADD_AUTO_EQUIP);
		}

		removeCurrency(count);
		items->playSound(eset->misc.currency_id);
		return true;
	}
	else {
		pc->logMsg(msg->getv("Not enough %s.", eset->loot.currency.c_str()), Avatar::MSG_NORMAL);
		drop_stack.push(stack);
		return false;
	}
}

/**
 * Sell a specific stack of items
 */
bool MenuInventory::sell(ItemStack stack) {
	if (stack.empty() || !items->isValid(stack.item)) {
		return false;
	}

	// can't sell currency
	if (stack.item == eset->misc.currency_id) return false;

	// items that have no price cannot be sold
	if (items->items[stack.item]->getPrice(ItemManager::USE_VENDOR_RATIO) == 0) {
		items->playSound(stack.item);
		pc->logMsg(msg->get("This item can not be sold."), Avatar::MSG_NORMAL);
		return false;
	}

	// quest items can not be sold
	if (items->items[stack.item]->quest_item) {
		items->playSound(stack.item);
		pc->logMsg(msg->get("This item can not be sold."), Avatar::MSG_NORMAL);
		return false;
	}

	int value_each = items->items[stack.item]->getSellPrice(ItemManager::DEFAULT_SELL_PRICE);
	int value = value_each * stack.quantity;
	addCurrency(value);
	items->playSound(eset->misc.currency_id);
	drag_prev_src = -1;
	return true;
}

void MenuInventory::updateEquipment(int slot) {

	if (slot == -1) {
		// This should never happen, but ignore it if it does
		return;
	}
	else {
		changed_equipment = true;
	}
}

/**
 * Given the equipped items, calculate the hero's stats
 */
void MenuInventory::applyEquipment() {
	if (items->items.empty())
		return;

	ItemID item_id;
	std::vector<ItemSetID> active_sets;
	std::vector<int> active_set_quantities;

	// calculate bonuses to basic stats, added by items
	bool checkRequired = true;
	while(checkRequired) {
		checkRequired = false;
		active_sets.clear();
		active_set_quantities.clear();

		for (size_t j = 0; j < eset->primary_stats.list.size(); ++j) {
			pc->stats.primary_additional[j] = 0;
		}

		for (int i = 0; i < MAX_EQUIPPED; i++) {
			if (isActive(i)) {
				item_id = inventory[EQUIPMENT].storage[i].item;
				if (!items->isValid(item_id))
					continue;

				Item* item = items->items[item_id];
				unsigned bonus_counter = 0;
				while (bonus_counter < item->bonus.size()) {
					for (size_t j = 0; j < eset->primary_stats.list.size(); ++j) {
						if (item->bonus[bonus_counter].type == BonusData::PRIMARY_STAT && item->bonus[bonus_counter].index == j)
							pc->stats.primary_additional[j] += static_cast<int>(item->bonus[bonus_counter].value.get());
					}

					bonus_counter++;
				}
			}
		}

		// determine which item sets are active and count the number of items for each active set
		std::vector<ItemSetID>::iterator it;
		for (int i=0; i<MAX_EQUIPPED; i++) {
			ItemStack& stack = inventory[EQUIPMENT].storage[i];

			if (items->isValid(stack.item) && isActive(i) && items->items[stack.item]->set > 0) {
				it = std::find(active_sets.begin(), active_sets.end(), items->items[stack.item]->set);
				if (it != active_sets.end()) {
					active_set_quantities[std::distance(active_sets.begin(), it)] += 1;
				}
				else {
					active_sets.push_back(items->items[stack.item]->set);
					active_set_quantities.push_back(1);
				}
			}
		}

		// calculate bonuses to basic stats, added by item sets
		for (size_t k = 0; k < active_sets.size(); ++k) {
			if (!items->isValidSet(active_sets[k]))
				continue;

			ItemSet* item_set = items->item_sets[active_sets[k]];
			for (size_t bonus_counter = 0; bonus_counter < item_set->bonus.size(); ++bonus_counter) {
				if (item_set->bonus[bonus_counter].requirement != active_set_quantities[k])
					continue;

				for (size_t j = 0; j < eset->primary_stats.list.size(); ++j) {
					if (item_set->bonus[bonus_counter].type == BonusData::PRIMARY_STAT && item_set->bonus[bonus_counter].index == j)
						pc->stats.primary_additional[j] += static_cast<int>(item_set->bonus[bonus_counter].value.get());
				}
			}
		}

		// check that each equipped item fit requirements and is in the proper type of slot
		for (int i = 0; i < MAX_EQUIPPED; i++) {
			ItemStack& stack = inventory[EQUIPMENT].storage[i];

			if (items->isValid(stack.item)) {
				if ((isActive(i) && !items->requirementsMet(&pc->stats, stack.item)) || (!stack.empty() && slot_type[i] != items->items[stack.item]->type)) {
					add(stack, CARRIED, ItemStorage::NO_SLOT, ADD_PLAY_SOUND, !ADD_AUTO_EQUIP);
					stack.clear();
					checkRequired = true;
				}
			}
		}
	}

	// defaults
	for (unsigned i=0; i<pc->stats.powers_list_items.size(); ++i) {
		PowerID id = pc->stats.powers_list_items[i];
		// pc->stats.hp > 0 is hack to keep on_death revive passives working
		if (powers->powers[id]->passive && pc->stats.hp > 0 && !powers->powers[id]->passive_effects_persist) {
			pc->stats.effects.removeEffectPassive(id);
		}
	}
	pc->stats.powers_list_items.clear();

	// reset wielding vars
	pc->stats.equip_flags.clear();

	// remove all effects and bonuses added by items
	pc->stats.effects.clearItemEffects();

	// reset power level bonuses
	menu->pow->clearBonusLevels();

	applyItemStats();
	applyItemSetBonuses(active_sets, active_set_quantities);


	// enable all slots by default
	for (int i = 0; i < MAX_EQUIPPED; ++i) {
		inventory[EQUIPMENT].slots[i]->enabled = true;
	}
	// disable any incompatible slots, unequipping items if neccessary
	for (int i = 0; i < MAX_EQUIPPED; ++i) {
		item_id = inventory[EQUIPMENT][i].item;

		if (items->isValid(item_id) && isActive(i)) {
			for (size_t j = 0; j < items->items[item_id]->disable_slots.size(); ++j) {
				disableEquipmentSlot(items->items[item_id]->disable_slots[j]);
			}
		}
	}

	// disable equipment slots via passive powers
	for (size_t i = 0; i < pc->stats.powers_passive.size(); ++i) {
		PowerID id = pc->stats.powers_passive[i];
		if (!powers->powers[id]->passive)
			continue;

		for (size_t j = 0; j < powers->powers[id]->disable_equip_slots.size(); ++j) {
			disableEquipmentSlot(powers->powers[id]->disable_equip_slots[j]);
		}
	}
	for (size_t i = 0; i < pc->stats.powers_list_items.size(); ++i) {
		PowerID id = pc->stats.powers_list_items[i];
		if (!powers->powers[id]->passive)
			continue;

		for (size_t j = 0; j < powers->powers[id]->disable_equip_slots.size(); ++j) {
			disableEquipmentSlot(powers->powers[id]->disable_equip_slots[j]);
		}
	}

	// update stat display
	pc->stats.refresh_stats = true;

	if (preview)
		preview->loadGraphicsFromInventory(this);
}

void MenuInventory::applyItemStats() {
	if (items->items.empty())
		return;

	// reset additional values
	for (size_t i = 0; i < eset->damage_types.list.size(); ++i) {
		pc->stats.item_base_dmg[i].min = pc->stats.item_base_dmg[i].max = 0;
	}
	pc->stats.item_base_abs.min = pc->stats.item_base_abs.max = 0;

	// apply stats from all items
	for (int i=0; i<MAX_EQUIPPED; i++) {
		if (isActive(i)) {
			ItemID item_id = inventory[EQUIPMENT].storage[i].item;
			if (!items->isValid(item_id))
				continue;

			Item* item = items->items[item_id];

			// apply base stats
			for (size_t j = 0; j < eset->damage_types.list.size(); ++j) {
				pc->stats.item_base_dmg[j].min += item->base_dmg[j].min;
				pc->stats.item_base_dmg[j].max += item->base_dmg[j].max;
			}

			// set equip flags
			for (unsigned j=0; j<item->equip_flags.size(); ++j) {
				pc->stats.equip_flags.insert(item->equip_flags[j]);
			}

			// apply absorb bonus
			pc->stats.item_base_abs.min += item->base_abs.min;
			pc->stats.item_base_abs.max += item->base_abs.max;

			// apply various bonuses
			unsigned bonus_counter = 0;
			while (bonus_counter < item->bonus.size()) {
				applyBonus(&item->bonus[bonus_counter]);
				bonus_counter++;
			}

			// add item powers
			if (item->power > 0) {
				pc->stats.powers_list_items.push_back(item->power);
				if (pc->stats.effects.triggered_others)
					powers->activateSinglePassive(&pc->stats, item->power);
			}
		}
	}
}

void MenuInventory::applyItemSetBonuses(std::vector<ItemSetID> &active_sets, std::vector<int> &active_set_quantities) {
	// apply item set bonuses
	for (size_t i = 0; i < active_sets.size(); ++i) {
		if (!items->isValidSet(active_sets[i]))
			continue;

		ItemSet* item_set = items->item_sets[active_sets[i]];

		for (size_t j = 0; j < item_set->bonus.size(); ++j) {
			if (item_set->bonus[j].requirement > active_set_quantities[i])
				continue;

			applyBonus(&(item_set->bonus[j]));
		}
	}
}

void MenuInventory::applyBonus(const BonusData* bdata) {
	EffectDef ed;

	if (bdata->type == BonusData::SPEED) {
		ed.id = "speed";
	}
	else if (bdata->type == BonusData::ATTACK_SPEED) {
		ed.id = "attack_speed";
	}
	else if (bdata->type == BonusData::STAT) {
		ed.id = Stats::KEY[bdata->index];
	}
	else if (bdata->type == BonusData::DAMAGE_MIN) {
		ed.id = eset->damage_types.list[bdata->index].min;
	}
	else if (bdata->type == BonusData::DAMAGE_MAX) {
		ed.id = eset->damage_types.list[bdata->index].max;
	}
	else if (bdata->type == BonusData::RESIST_ELEMENT) {
		ed.id = eset->damage_types.list[bdata->index].resist;
	}
	else if (bdata->type == BonusData::PRIMARY_STAT) {
		ed.id = eset->primary_stats.list[bdata->index].id;
	}
	else if (bdata->power_id > 0) {
		menu->pow->addBonusLevels(bdata->power_id, static_cast<int>(bdata->value.get()));
		return; // don't add item effect
	}
	else if (bdata->type == BonusData::RESOURCE_STAT) {
		ed.id = eset->resource_stats.list[bdata->index].ids[bdata->sub_index];
	}

	ed.type = Effect::getTypeFromString(ed.id);

	EffectParams ep;
	ep.magnitude = bdata->value.get();
	ep.is_multiplier = bdata->is_multiplier;
	ep.source_type = Power::SOURCE_TYPE_HERO;
	ep.is_from_item = true;

	pc->stats.effects.addEffect(&pc->stats, ed, ep);
}

void MenuInventory::applyEquipmentSet(unsigned set) {
	unsigned prev_equipment_set = active_equipment_set;

	if (set > 0 && set <= max_equipment_set) {
		active_equipment_set = set;
		updateEquipmentSetWidgets();
	}

	if (active_equipment_set > 0 && prev_equipment_set != active_equipment_set) {
		if (!visible && menu && menu->hudlog) {
			menu->hudlog->add(msg->getv("Equipped set %d.", static_cast<int>(active_equipment_set)), MenuHUDLog::MSG_NORMAL);
		}
	}
}

void MenuInventory::applyNextEquipmentSet() {
	if (active_equipment_set < max_equipment_set) {
		applyEquipmentSet(active_equipment_set+1);
	}
	else {
		applyEquipmentSet(1);
	}
}

void MenuInventory::applyPreviousEquipmentSet() {
	if (active_equipment_set > 1) {
		applyEquipmentSet(active_equipment_set-1);
	}
	else {
		applyEquipmentSet(max_equipment_set);
	}
}

void MenuInventory::updateEquipmentSetWidgets() {
	Widget* first_active_slot = NULL;
	Widget* current_tablist_widget = tablist.getWidgetByIndex(tablist.getCurrent());
	bool reset_tablist_cursor = false;

	for (int i=0; i<MAX_EQUIPPED; i++) {
		if (isActive(i)) {
			if (!first_active_slot) {
				first_active_slot = inventory[EQUIPMENT].slots[i];
			}
			inventory[EQUIPMENT].slots[i]->visible = true;
			inventory[EQUIPMENT].slots[i]->enable_tablist_nav = true;
		}
		else {
			inventory[EQUIPMENT].slots[i]->visible = false;
			inventory[EQUIPMENT].slots[i]->enable_tablist_nav = false;
			if (current_tablist_widget && inventory[EQUIPMENT].slots[i] == current_tablist_widget) {
				reset_tablist_cursor = true;
			}
		}
	}

	if (!equipmentSetButton.empty()) {
		for (size_t i=0; i<equipmentSetButton.size(); i++) {
			if (active_equipment_set > 0) {
				if (i == active_equipment_set-1) {
					equipmentSetButton[i]->enabled = false;
					if (current_tablist_widget && equipmentSetButton[i] == current_tablist_widget) {
						reset_tablist_cursor = true;
					}

				}
				else {
					equipmentSetButton[i]->enabled = true;
				}
			}
		}
	}

	if (reset_tablist_cursor && first_active_slot) {
		tablist.setCurrent(first_active_slot);
	}

	if (equipmentSetLabel) {
		std::stringstream label;
		label << active_equipment_set << "/" << max_equipment_set;
		equipmentSetLabel->setText(label.str());
		equipmentSetLabel->setColor(font->getColor(FontEngine::COLOR_MENU_NORMAL));
	}

	changed_equipment = true;
}


bool MenuInventory::isActive(size_t equipped) {
	if (equipment_set[equipped] == 0 || equipment_set[equipped]==active_equipment_set) {
		return true;
	}

	return false;
}

int MenuInventory::getEquippedCount() {
	return static_cast<int>(equipped_area.size());
}

int MenuInventory::getTotalSlotCount() {
	return MAX_CARRIED + MAX_EQUIPPED;
}

void MenuInventory::clearHighlight() {
	inventory[EQUIPMENT].highlightClear();
	inventory[CARRIED].highlightClear();
}

/**
 * Sort equipment storage array, so items order matches slots order
 */
void MenuInventory::fillEquipmentSlots() {
	// create temporary array
	ItemStack *equip_stack = new ItemStack[MAX_EQUIPPED];

	for (int i = 0; i < MAX_EQUIPPED; ++i) {
		// initialize array
		// if an item is set, ensure the quantity is >= 1
		equip_stack[i] = inventory[EQUIPMENT].storage[i];
		if (equip_stack[i].item > 0)
			equip_stack[i].quantity = std::max(1, equip_stack[i].quantity);
		else
			equip_stack[i].clear();

		// clean up storage[]
		inventory[EQUIPMENT].storage[i].clear();

		// if items were in the correct slot, put them back
		if (!equip_stack[i].empty() && inventory[EQUIPMENT].storage[i].empty() && items->isValid(equip_stack[i].item) && items->items[equip_stack[i].item]->type == slot_type[i]) {
			inventory[EQUIPMENT].storage[i] = equip_stack[i];
			equip_stack[i].clear();
		}
	}

	// for items that weren't in a matching slot, try to find one
	// if all else fails, add them to the inventory
	for (int i = 0; i < MAX_EQUIPPED; ++i) {
		if (equip_stack[i].empty() || !items->isValid(equip_stack[i].item))
			continue;

		bool found_slot = false;
		for (int j = 0; j < MAX_EQUIPPED; ++j) {
			// search for empty slot with needed type
			if (inventory[EQUIPMENT].storage[j].empty()) {
				if (items->items[equip_stack[i].item]->type == slot_type[j]) {
					inventory[EQUIPMENT].storage[j] = equip_stack[i];
					found_slot = true;
					break;
				}
			}
		}

		// couldn't find a slot, adding to inventory
		if (!found_slot) {
			add(equip_stack[i], CARRIED, ItemStorage::NO_SLOT, !ADD_PLAY_SOUND, !ADD_AUTO_EQUIP);
		}
	}

	delete [] equip_stack;
}

int MenuInventory::getMaxPurchasable(ItemStack item, int vendor_tab) {
	if (!items->isValid(item.item))
		return 0;

	if (vendor_tab == ItemManager::VENDOR_BUY)
		return currency / items->items[item.item]->getPrice(ItemManager::USE_VENDOR_RATIO);
	else if (vendor_tab == ItemManager::VENDOR_SELL)
		return currency / items->items[item.item]->getSellPrice(item.can_buyback);
	else
		return 0;
}

int MenuInventory::getEquipSlotFromItem(ItemID item, bool only_empty_slots) {
	if (!items->isValid(item) || !items->requirementsMet(&pc->stats, item))
		return -2;

	int equip_slot = -1;

	// find first empty(or just first) slot for item to equip
	for (int i = 0; i < MAX_EQUIPPED; i++) {
		if (!isActive(i))
			continue;

		if (slot_type[i] == items->items[item]->type) {
			if (inventory[EQUIPMENT].storage[i].empty()) {
				// empty and matching, no need to search more
				equip_slot = i;
				break;
			}
			else if (!only_empty_slots && equip_slot == -1) {
				// non-empty and matching
				equip_slot = i;
			}
		}
	}

	return equip_slot;
}

PowerID MenuInventory::getPowerMod(PowerID meta_power) {
	for (int i = 0; i < inventory[EQUIPMENT].getSlotNumber(); ++i) {
		ItemID id = inventory[EQUIPMENT][i].item;
		if (!items->isValid(id))
			continue;

		for (size_t j=0; j<items->items[id]->replace_power.size(); j++) {
			if (items->items[id]->replace_power[j].first == meta_power && items->items[id]->replace_power[j].second != meta_power) {
				return items->items[id]->replace_power[j].second;
			}
		}
	}

	return 0;
}

void MenuInventory::disableEquipmentSlot(size_t disable_slot_type) {
	for (int i=0; i<MAX_EQUIPPED; ++i) {
		if (isActive(i) && slot_type[i] == disable_slot_type) {
			if (!inventory[EQUIPMENT].storage[i].empty()) {
				add(inventory[EQUIPMENT].storage[i], CARRIED, ItemStorage::NO_SLOT, ADD_PLAY_SOUND, !ADD_AUTO_EQUIP);
				inventory[EQUIPMENT].storage[i].clear();
				updateEquipment(i);
				applyEquipment();
			}
			inventory[EQUIPMENT].slots[i]->enabled = false;
		}
	}
}

bool MenuInventory::canActivateItem(ItemID item) {
	if (!items->isValid(item))
		return false;

	if (!items->items[item]->script.empty())
		return true;
	if (!items->items[item]->book.empty())
		return true;
	if (items->items[item]->power > 0 && getEquipSlotFromItem(item, !ONLY_EMPTY_SLOTS) == -1)
		return true;

	return false;
}

int MenuInventory::getEquippedSetCount(size_t set_id) {
	int quantity = 0;
	for (int i=0; i<MAX_EQUIPPED; i++) {
		ItemID item_id = inventory[EQUIPMENT].storage[i].item;
		if (items->isValid(item_id) && isActive(i)) {
			if (items->items[item_id]->set == set_id) {
				quantity++;
			}
		}
	}
	return quantity;
}

bool MenuInventory::canEquipItem(const Point& position) {
	// clicked a carried item
	int slot = inventory[CARRIED].slotOver(position);
	if (slot == -1)
		return false;

	ItemID item_id = inventory[CARRIED][slot].item;

	// empty slot
	if (inventory[CARRIED][slot].empty() || !items->isValid(item_id))
		return false;

	return (pc->stats.humanoid && !items->getItemType(items->items[item_id]->type).name.empty() && getEquipSlotFromItem(item_id, !ONLY_EMPTY_SLOTS) >= 0);
}

bool MenuInventory::canUseItem(const Point& position) {
	// clicked a carried item
	int slot = inventory[CARRIED].slotOver(position);
	if (slot == -1)
		return false;

	// empty slot
	if (inventory[CARRIED][slot].empty())
		return false;

	ItemID item_id = inventory[CARRIED][slot].item;

	return canActivateItem(item_id);
}

// same as ItemStorage::contain(), but accounts for active equipment set
bool MenuInventory::equipmentContain(ItemID item, int quantity) {
	int total_quantity = 0;
	for (int i = 0; i < MAX_EQUIPPED; ++i) {
		if (!isActive(i))
			continue;

		if (inventory[EQUIPMENT][i].item == item)
			total_quantity += inventory[EQUIPMENT][i].quantity;

		if (total_quantity >= quantity)
			return true;
	}
	return false;
}

MenuInventory::~MenuInventory() {
	delete closeButton;
	for (size_t i=0; i<equipmentSetButton.size(); i++) {
		delete equipmentSetButton[i];
	}

	delete equipmentSetNext;
	delete equipmentSetPrevious;
	delete equipmentSetLabel;

	if (preview)
		delete preview;
}
