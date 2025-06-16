/*
Copyright © 2011-2012 Clint Bellanger
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
 * class MenuManager
 */

#include "Avatar.h"
#include "EngineSettings.h"
#include "FontEngine.h"
#include "IconManager.h"
#include "InputState.h"
#include "Menu.h"
#include "MenuActionBar.h"
#include "MenuActiveEffects.h"
#include "MenuBook.h"
#include "MenuCharacter.h"
#include "MenuConfirm.h"
#include "MenuDevConsole.h"
#include "MenuEnemy.h"
#include "MenuExit.h"
#include "MenuGameOver.h"
#include "MenuHUDLog.h"
#include "MenuInventory.h"
#include "MenuLog.h"
#include "MenuManager.h"
#include "MenuMiniMap.h"
#include "MenuNumPicker.h"
#include "MenuPowers.h"
#include "MenuStash.h"
#include "MenuStatBar.h"
#include "MenuTalker.h"
#include "MenuTouchControls.h"
#include "MenuVendor.h"
#include "MessageEngine.h"
#include "ModManager.h"
#include "NPC.h"
#include "PowerManager.h"
#include "RenderDevice.h"
#include "Settings.h"
#include "SharedGameResources.h"
#include "SharedResources.h"
#include "SoundManager.h"
#include "Subtitles.h"
#include "TooltipManager.h"
#include "UtilsFileSystem.h"
#include "UtilsParsing.h"
#include "WidgetHorizontalList.h"
#include "WidgetSlot.h"
#include "WidgetTooltip.h"

MenuManager::MenuManager()
	: key_lock(false)
	, mouse_dragging(false)
	, keyboard_dragging(false)
	, sticky_dragging(false)
	, drag_stack()
	, drag_power(0)
	, drag_src(DRAG_SRC_NONE)
	, drag_icon(new WidgetSlot(WidgetSlot::NO_ICON, WidgetSlot::HIGHLIGHT_NORMAL))
	, done(false)
	, act_drag_hover(false)
	, keydrag_pos(Point())
	, action_src(ACTION_SRC_NONE)
	, drag_post_action(DRAG_POST_ACTION_NONE)
	, inv(NULL)
	, pow(NULL)
	, chr(NULL)
	, questlog(NULL)
	, hudlog(NULL)
	, act(NULL)
	, book(NULL)
	, hp(NULL)
	, mp(NULL)
	, xp(NULL)
	, resource_statbars()
	, mini(NULL)
	, num_picker(NULL)
	, enemy(NULL)
	, vendor(NULL)
	, talker(NULL)
	, exit(NULL)
	, effects(NULL)
	, stash(NULL)
	, game_over(NULL)
	, action_picker(NULL)
	, devconsole(NULL)
	, touch_controls(NULL)
	, subtitles(NULL)
	, pause(false)
	, menus_open(false) {

	hp = new MenuStatBar(MenuStatBar::TYPE_HP, 0);
	mp = new MenuStatBar(MenuStatBar::TYPE_MP, 0);
	xp = new MenuStatBar(MenuStatBar::TYPE_XP, 0);
	effects = new MenuActiveEffects();
	hudlog = new MenuHUDLog();
	act = new MenuActionBar();
	enemy = new MenuEnemy();
	vendor = new MenuVendor();
	talker = new MenuTalker();
	exit = new MenuExit();
	mini = new MenuMiniMap();
	chr = new MenuCharacter();
	inv = new MenuInventory();
	pow = new MenuPowers();
	questlog = new MenuLog();
	stash = new MenuStash();
	book = new MenuBook();
	num_picker = new MenuNumPicker();
	game_over = new MenuGameOver();
	action_picker = new MenuConfirm();

	resource_statbars.resize(eset->resource_stats.list.size());
	for (size_t i = 0; i < resource_statbars.size(); ++i) {
		resource_statbars[i] = new MenuStatBar(MenuStatBar::TYPE_RESOURCE_STAT, i);
	}

	menus.push_back(hp);
	menus.push_back(mp);
	menus.push_back(xp);
	for (size_t i = 0; i < resource_statbars.size(); ++i) {
		menus.push_back(resource_statbars[i]);
	}
	menus.push_back(effects);
	menus.push_back(hudlog);
	menus.push_back(act);
	menus.push_back(enemy);
	menus.push_back(vendor);
	menus.push_back(talker);
	menus.push_back(exit);
	menus.push_back(mini);
	menus.push_back(chr);
	menus.push_back(inv);
	menus.push_back(pow);
	menus.push_back(questlog);
	menus.push_back(stash);
	menus.push_back(book);
	menus.push_back(num_picker);
	menus.push_back(game_over);
	menus.push_back(action_picker);

	if (settings->dev_mode) {
		devconsole = new MenuDevConsole();
	}

	touch_controls = new MenuTouchControls();

	subtitles = new Subtitles();

	closeAll(); // make sure all togglable menus start closed

	settings->show_hud = true;

	drag_icon->enabled = false;
	drag_icon->show_disabled_overlay = false;

	action_picker->setTitle(msg->get("Choose an action:"));

	// enabled menu buttons on actionbar
	act->menus[MenuActionBar::MENU_CHARACTER]->enabled = chr->enabled;
	act->menus[MenuActionBar::MENU_INVENTORY]->enabled = inv->enabled;
	act->menus[MenuActionBar::MENU_POWERS]->enabled = pow->enabled;
	act->menus[MenuActionBar::MENU_LOG]->enabled = questlog->enabled;
}

void MenuManager::alignAll() {
	for (size_t i=0; i<menus.size(); i++) {
		menus[i]->align();
	}

	if (settings->dev_mode) {
		devconsole->align();
	}

	touch_controls->align();
}

void MenuManager::renderIcon(int x, int y) {
	if (drag_icon->getIcon() != WidgetSlot::NO_ICON) {
		drag_icon->setPos(x,y);
		drag_icon->render();
	}
}

void MenuManager::setDragIcon(int icon_id, int overlay_id) {
	drag_icon->setIcon(icon_id, overlay_id);
	drag_icon->setAmount(0, 0);
}

void MenuManager::setDragIconItem(ItemStack stack) {
	if (stack.empty() || !items->isValid(stack.item)) {
		drag_icon->setIcon(WidgetSlot::NO_ICON, WidgetSlot::NO_OVERLAY);
		drag_icon->setAmount(0, 0);
	}
	else {
		drag_icon->setIcon(items->items[stack.item]->icon, items->getItemIconOverlay(stack.item));
		drag_icon->setAmount(stack.quantity, items->items[stack.item]->max_quantity);
	}
}

void MenuManager::handleKeyboardNavigation() {

	for (size_t i = 0; i < stash->tabs.size(); ++i) {
		stash->tabs[i].tablist.setNextTabList(NULL);
	}
	vendor->tablist_buy.setNextTabList(NULL);
	vendor->tablist_sell.setNextTabList(NULL);
	chr->tablist.setNextTabList(NULL);
	questlog->setNextTabList(NULL);
	inv->tablist.setPrevTabList(NULL);
	pow->setNextTabList(NULL);

	// unlock menus if only one side is showing
	if (!inv->visible && !pow->visible) {
		stash->tablist.unlock();
		vendor->tablist.unlock();
		chr->tablist.unlock();
		if (!questlog->getCurrentTabList())
			questlog->tablist.unlock();

	}
	else if (!vendor->visible && !stash->visible && !chr->visible && !questlog->visible) {
		if (inv->visible)
			inv->tablist.unlock();
		else if (pow->visible && !pow->getCurrentTabList())
			pow->tablist.unlock();
	}

	if (drag_src == DRAG_SRC_NONE) {
		if (inv->visible) {
			for (size_t i = 0; i < stash->tabs.size(); ++i) {
				stash->tabs[i].tablist.setNextTabList(&inv->tablist);
			}
			vendor->tablist_buy.setNextTabList(&inv->tablist);
			vendor->tablist_sell.setNextTabList(&inv->tablist);
			chr->tablist.setNextTabList(&inv->tablist);
			questlog->setNextTabList(&inv->tablist);

			if (stash->visible) {
				inv->tablist.setPrevTabList(&stash->tablist);
			}
			else if (vendor->visible) {
				inv->tablist.setPrevTabList(&vendor->tablist);
			}
			else if (chr->visible) {
				inv->tablist.setPrevTabList(&chr->tablist);
			}
			else if (questlog->visible) {
				inv->tablist.setPrevTabList(questlog->getVisibleChildTabList());
			}
		}
		else if (pow->visible) {
			for (size_t i = 0; i < stash->tabs.size(); ++i) {
				stash->tabs[i].tablist.setNextTabList(&pow->tablist);
			}
			vendor->tablist_buy.setNextTabList(&pow->tablist);
			vendor->tablist_sell.setNextTabList(&pow->tablist);
			chr->tablist.setNextTabList(&pow->tablist);
			questlog->setNextTabList(&pow->tablist);

			// NOTE stash and vendor are only visible with inventory, so we don't need to handle them here
			if (chr->visible) {
				pow->tablist.setPrevTabList(&chr->tablist);
			}
			else if (questlog->visible) {
				pow->tablist.setPrevTabList(questlog->getVisibleChildTabList());
			}
		}
	}

	// stash and vendor always start locked
	if (!stash->visible) {
		stash->tablist.lock();
		for (size_t i = 0; i < stash->tabs.size(); ++i) {
			stash->tabs[i].tablist.lock();
		}
	}
	if (!vendor->visible) {
		vendor->tablist.lock();
		vendor->tablist_buy.lock();
		vendor->tablist_sell.lock();
	}

	// inventory always starts unlocked
	if (!inv->visible) inv->tablist.unlock();

	if (act->getCurrentTabList()) {
		inv->tablist.lock();
	}
	else {
		act->tablist.lock();
	}
}

void MenuManager::logic() {
	ItemStack stack;

	subtitles->logic(snd->getLastPlayedSID());

	// refresh statbar values from player statblock
	hp->update();
	mp->update();
	xp->update();
	for (size_t i = 0; i < resource_statbars.size(); ++i) {
		resource_statbars[i]->update();
	}

	// close talker/vendor menu if player is attacked
	if (pc->stats.abort_npc_interact && eset->misc.combat_aborts_npc_interact) {
		talker->setNPC(NULL);
		vendor->setNPC(NULL);
	}
	pc->stats.abort_npc_interact = false;

	if (act->tablist.getCurrent() != -1) {
		// books/npcs can be activated by powers in the actionbar, so we need to defocus the bar if one of those is opened
		if (talker->visible || book->visible) {
			act->tablist.defocus();
		}

		// vendor/stash items can't be placed on the action bar.
		// This restriction is actually handled during the drop, but clearing the drag contents is a quicker indication to the player
		if (drag_src == DRAG_SRC_VENDOR || drag_src == DRAG_SRC_STASH) {
			resetDrag();
		}
	}

	// when selecting item quantities, don't process other menus
	if (num_picker->visible) {
		num_picker->logic();

		if (num_picker->confirm_clicked) {
			// start dragging items
			// removes the desired quantity from the source stack

			if (drag_src == DRAG_SRC_INVENTORY) {
				drag_stack.quantity = num_picker->getValue();
				inv->removeFromPrevSlot(drag_stack.quantity);
			}
			else if (drag_src == DRAG_SRC_VENDOR) {
				drag_stack.quantity = num_picker->getValue();
				vendor->removeFromPrevSlot(drag_stack.quantity);
			}
			else if (drag_src == DRAG_SRC_STASH) {
				drag_stack.quantity = num_picker->getValue();
				stash->removeFromPrevSlot(drag_stack.quantity);
			}

			num_picker->confirm_clicked = false;
			num_picker->visible = false;
			if (inpt->usingMouse()) {
				sticky_dragging = true;
			}
			num_picker->tablist.defocus();
		}
		else if (num_picker->cancel_clicked) {
			// cancel item dragging
			drag_stack.clear();
			num_picker->closeWindow();
			resetDrag();
		}
		else {
			pause = true;
			return;
		}
	}
	else if (action_picker->visible) {
		action_picker->logic();
		if (action_picker->clicked_confirm) {
			unsigned action = action_picker_map.at(action_picker->action_list->getSelected());

			if (action_src == ACTION_SRC_INVENTORY) {
				if (action == ACTION_PICKER_INVENTORY_SELECT || action == ACTION_PICKER_INVENTORY_DROP || action == ACTION_PICKER_INVENTORY_SELL || action == ACTION_PICKER_INVENTORY_STASH) {
					drag_stack = inv->click(action_picker_target);
					if (!drag_stack.empty()) {
						actionPickerStartDrag();
						drag_src = DRAG_SRC_INVENTORY;
					}
					if (drag_stack.quantity > 1) {
						num_picker->setValueBounds(1, drag_stack.quantity);
						num_picker->visible = true;
					}

					if (action == ACTION_PICKER_INVENTORY_DROP)
						drag_post_action = DRAG_POST_ACTION_DROP;
					else if (action == ACTION_PICKER_INVENTORY_SELL)
						drag_post_action = DRAG_POST_ACTION_SELL;
					else if (action == ACTION_PICKER_INVENTORY_STASH)
						drag_post_action = DRAG_POST_ACTION_STASH;
				}
				else if (action == ACTION_PICKER_INVENTORY_ACTIVATE) {
					inv->activate(action_picker_target);
				}
			}
			else if (action_src == ACTION_SRC_STASH) {
				if (action == ACTION_PICKER_STASH_SELECT || action == ACTION_PICKER_STASH_TRANSFER) {
					drag_stack = stash->click(action_picker_target);
					if (!drag_stack.empty()) {
						actionPickerStartDrag();
						drag_src = DRAG_SRC_STASH;
					}
					if (drag_stack.quantity > 1) {
						num_picker->setValueBounds(1, drag_stack.quantity);
						num_picker->visible = true;
					}

					if (action == ACTION_PICKER_STASH_TRANSFER)
						drag_post_action = DRAG_POST_ACTION_STASH;
				}
			}
			else if (action_src == ACTION_SRC_VENDOR) {
				if (action == ACTION_PICKER_VENDOR_BUY) {
					drag_stack = vendor->click(action_picker_target);
					if (!drag_stack.empty()) {
						actionPickerStartDrag();
						drag_src = DRAG_SRC_VENDOR;
						vendor->lockTabControl();
					}
					if (drag_stack.quantity > 1) {
						int max_quantity = std::min(inv->getMaxPurchasable(drag_stack, vendor->getTab()), drag_stack.quantity);
						if (max_quantity >= 1) {
							num_picker->setValueBounds(1, max_quantity);
							num_picker->visible = true;
						}
						else {
							vendor->removeFromPrevSlot(drag_stack.quantity);
							// drag_stack.clear();
							// resetDrag();
						}
					}

					if (!drag_stack.empty()) {
						drag_post_action = DRAG_POST_ACTION_BUY;
					}
				}
			}
			else if (action_src == ACTION_SRC_POWERS) {
				if (action == ACTION_PICKER_POWERS_SELECT) {
					MenuPowersClick pow_click = pow->click(action_picker_target);
					drag_power = pow_click.drag;
					if (drag_power > 0) {
						actionPickerStartDrag();
						drag_src = DRAG_SRC_POWERS;

						if (inpt->mode != InputState::MODE_TOUCHSCREEN) {
							// move the cursor to the actionbar
							act->tablist.unlock();
							act->tablist.getNext(!TabList::GET_INNER, TabList::WIDGET_SELECT_AUTO);
							menu->defocusLeft();
							menu->defocusRight();
						}
					}
				}
				else if (action == ACTION_PICKER_POWERS_UPGRADE) {
					MenuPowersClick pow_click = pow->click(action_picker_target);
					if (pow_click.unlock > 0) {
						pow->clickUnlock(pow_click.unlock);
					}
				}
			}
			else if (action_src == ACTION_SRC_ACTIONBAR) {
				if (action == ACTION_PICKER_ACTIONBAR_SELECT) {
					drag_power = act->checkDrag(action_picker_target);
					if (drag_power > 0) {
						actionPickerStartDrag();
						drag_src = DRAG_SRC_ACTIONBAR;
					}
				}
				else if (action == ACTION_PICKER_ACTIONBAR_CLEAR) {
					act->remove(action_picker_target);
				}
				else if (action == ACTION_PICKER_ACTIONBAR_USE) {
					WidgetSlot* act_slot = act->getSlotFromPosition(action_picker_target);
					if (act_slot) {
						act->touch_slot = act_slot;
					}
				}
			}

			action_picker->visible = false;
			action_picker->tablist.defocus();
		}
		else if (action_picker->clicked_cancel) {
			action_picker->tablist.defocus();
		}
		else {
			pause = true;
			return;
		}
	}
	else {
		// Sometimes, an action from action_picker needs to show num_picker first.
		// num_picker will only start dragging the item stack, so we need to complete the selected action here

		// when using a touchscreen, MenuItemStorage::click() will highlight the selected slot. We can clear the highlight here
		if (drag_post_action != DRAG_POST_ACTION_NONE && inpt->mode == InputState::MODE_TOUCHSCREEN) {
			defocusLeft();
			defocusRight();
		}

		if (drag_post_action == DRAG_POST_ACTION_DROP) {
			if (drag_src == DRAG_SRC_INVENTORY) {
				// quest items cannot be dropped
				bool item_is_valid = items->isValid(drag_stack.item);
				if (!item_is_valid || (item_is_valid && !items->items[drag_stack.item]->quest_item)) {
					drop_stack.push(drag_stack);
				}
				else {
					pc->logMsg(msg->get("This item can not be dropped."), Avatar::MSG_NORMAL);
					items->playSound(drag_stack.item);

					inv->itemReturn(drag_stack);
				}
				if (inpt->mode == InputState::MODE_TOUCHSCREEN)
					inv->defocusTabLists();
			}
			else if (drag_src == DRAG_SRC_STASH) {
				drop_stack.push(drag_stack);
				stash->tabs[stash->getTab()].updated = true;
			}
			drag_src = DRAG_SRC_NONE;
			resetDrag();
		}
		else if (drag_post_action == DRAG_POST_ACTION_BUY) {
			if (!inv->buy(drag_stack, vendor->getTab(), !MenuInventory::IS_DRAGGING)) {
				vendor->itemReturn(inv->drop_stack.front());
				inv->drop_stack.pop();
			}
			drag_src = DRAG_SRC_NONE;
			resetDrag();
			vendor->resetDrag();
		}
		else if (drag_post_action == DRAG_POST_ACTION_SELL) {
			if (inv->sell(drag_stack)) {
				if (vendor->visible) {
					vendor->setTab(ItemManager::VENDOR_SELL);
					vendor->add(drag_stack);
				}
			}
			else {
				inv->itemReturn(drag_stack);
			}
			drag_src = DRAG_SRC_NONE;
			resetDrag();
		}
		else if (drag_post_action == DRAG_POST_ACTION_STASH) {
			if (drag_src == DRAG_SRC_INVENTORY) {
				if (!stash->add(drag_stack, MenuStash::NO_SLOT, MenuStash::ADD_PLAY_SOUND)) {
					inv->itemReturn(stash->drop_stack.front());
					stash->drop_stack.pop();
				}
			}
			else if (drag_src == DRAG_SRC_STASH) {
				if (!inv->add(drag_stack, MenuInventory::CARRIED, ItemStorage::NO_SLOT, MenuInventory::ADD_PLAY_SOUND, MenuInventory::ADD_AUTO_EQUIP)) {
					stash->itemReturn(inv->drop_stack.front());
					inv->drop_stack.pop();
				}
				stash->tabs[stash->getTab()].updated = true;
			}
			drag_src = DRAG_SRC_NONE;
			resetDrag();
		}

		drag_post_action = DRAG_POST_ACTION_NONE;
	}

	if (!inpt->usingMouse())
		handleKeyboardNavigation();

	// Check if the mouse is within any of the visible windows. Excludes the minimap and the exit/pause menu
	bool is_within_menus = (Utils::isWithinRect(act->window_area, inpt->mouse) ||
	(book->visible && Utils::isWithinRect(book->window_area, inpt->mouse)) ||
	(chr->visible && Utils::isWithinRect(chr->window_area, inpt->mouse)) ||
	(inv->visible && Utils::isWithinRect(inv->window_area, inpt->mouse)) ||
	(vendor->visible && Utils::isWithinRect(vendor->window_area, inpt->mouse)) ||
	(pow->visible && Utils::isWithinRect(pow->window_area, inpt->mouse)) ||
	(questlog->visible && Utils::isWithinRect(questlog->window_area, inpt->mouse)) ||
	(talker->visible && Utils::isWithinRect(talker->window_area, inpt->mouse)) ||
	(stash->visible && Utils::isWithinRect(stash->window_area, inpt->mouse)) ||
	(settings->dev_mode && devconsole->visible && Utils::isWithinRect(devconsole->window_area, inpt->mouse)));

	// Stop attacking if the cursor is inside an interactable menu
	if ((pc->using_main1 || pc->using_main2) && is_within_menus) {
		inpt->pressing[Input::MAIN1] = false;
		inpt->pressing[Input::MAIN2] = false;
	}

	stash->lock_tab_control = (mouse_dragging || keyboard_dragging);

	if (settings->dev_mode) {
		devconsole->logic();
	}

	if (!exit->visible && !is_within_menus)
		mini->logic();

	book->logic();
	effects->logic();

	if (!exit->visible && !game_over->visible)
		act->logic();

	hudlog->logic();
	enemy->logic();
	chr->logic();
	inv->logic();
	vendor->logic();
	pow->logic();
	questlog->logic();
	talker->logic();
	stash->logic();
	game_over->logic();

	touch_controls->logic();

	if (chr->checkUpgrade() || pc->stats.level_up) {
		// apply equipment and max hp/mp
		inv->applyEquipment();
		pc->stats.hp = pc->stats.get(Stats::HP_MAX);
		pc->stats.mp = pc->stats.get(Stats::MP_MAX);
		pc->stats.level_up = false;
	}

	// only allow the vendor window to be open if the inventory is open
	if (vendor->visible && !(inv->visible)) {
		snd->play(vendor->sfx_close, snd->DEFAULT_CHANNEL, snd->NO_POS, !snd->LOOP);
		closeAll();
	}

	// context-sensistive help tooltip in inventory menu
	if (inv->visible && vendor->visible) {
		inv->inv_ctrl = MenuInventory::CTRL_VENDOR;
	}
	else if (inv->visible && stash->visible) {
		inv->inv_ctrl = MenuInventory::CTRL_STASH;
	}
	else {
		inv->inv_ctrl = MenuInventory::CTRL_NONE;
	}

	if (!inpt->pressing[Input::INVENTORY] && !inpt->pressing[Input::POWERS] && !inpt->pressing[Input::CHARACTER] && !inpt->pressing[Input::LOG])
		key_lock = false;

	if (settings->dev_mode && devconsole->inputFocus())
		key_lock = true;

	// stop dragging with cancel key
	if (!key_lock && inpt->pressing[Input::CANCEL] && !inpt->lock[Input::CANCEL] && !pc->stats.corpse) {
		if (keyboard_dragging || mouse_dragging) {
			inpt->lock[Input::CANCEL] = true;
			resetDrag();
		}
	}

	// exit menu toggle
	if (!key_lock && !mouse_dragging && !keyboard_dragging) {
		if (inpt->pressing[Input::CANCEL] && !inpt->lock[Input::CANCEL]) {
			key_lock = true;
			if (act->twostep_slot != -1) {
				inpt->lock[Input::CANCEL] = true;
				act->twostep_slot = -1;
			}
			else if (settings->dev_mode && devconsole->visible) {
				inpt->lock[Input::CANCEL] = true;
				devconsole->closeWindow();
			}
			else if (act->tablist.getCurrent() != -1) {
				inpt->lock[Input::CANCEL] = true;
				act->tablist.defocus();
			}
			else if (menus_open) {
				inpt->lock[Input::CANCEL] = true;
				closeAll();
			}
			else if (!game_over->visible && (inpt->mode != InputState::MODE_JOYSTICK || exit->visible)) {
				// CANCEL (which is usually mapped to keyboard Escape) can open the pause menu if there are no other actions
				// we make an exception for opening the pause menu with joysticks, which usually have CANCEL mapped to a face button with a separate mapping for PAUSE (i.e. "Start")
				// closing the menu can always be done with either mapping
				inpt->lock[Input::CANCEL] = true;
				act->defocusTabLists();
				exit->handleCancel();
			}
		}

		// handle opening the pause menu with the dedicated binding
		if (inpt->pressing[Input::PAUSE] && !inpt->lock[Input::PAUSE] && !inpt->lock[Input::CANCEL]) {
			inpt->lock[Input::PAUSE] = true;

			// perform all "cancel" actions
			resetDrag();
			act->twostep_slot = -1;
			if (menus_open) {
				closeAll();
			}

			if (!game_over->visible) {
				act->defocusTabLists();
				exit->handleCancel();
			}
		}
	}
	if (mini->clicked_config) {
		mini->clicked_config = false;
		showExitMenu();
	}

	if (exit->visible) {
		exit->logic();
		if (exit->isExitRequested()) {
			done = true;
		}
		// if dpi scaling is changed, we need to realign the menus
		if (inpt->window_resized) {
			alignAll();
		}
	}
	else if (game_over->visible) {
		if (game_over->exit_clicked) {
			game_over->close();
			done = true;
		}
	}
	else {
		bool clicking_character = false;
		bool clicking_inventory = false;
		bool clicking_powers = false;
		bool clicking_log = false;

		// check if mouse-clicking a menu button
		act->checkMenu(clicking_character, clicking_inventory, clicking_powers, clicking_log);

		// inventory menu toggle
		if (menu->inv->enabled && ((inpt->pressing[Input::INVENTORY] && !key_lock && !mouse_dragging && !keyboard_dragging) || clicking_inventory)) {
			key_lock = true;
			if (inv->visible) {
				snd->play(inv->sfx_close, snd->DEFAULT_CHANNEL, snd->NO_POS, !snd->LOOP);
				closeRight();
			}
			else {
				closeRight();
				defocusLeft();
				act->requires_attention[MenuActionBar::MENU_INVENTORY] = false;
				inv->visible = true;
				snd->play(inv->sfx_open, snd->DEFAULT_CHANNEL, snd->NO_POS, !snd->LOOP);
			}

		}

		// powers menu toggle
		if (menu->pow->enabled && (((inpt->pressing[Input::POWERS] && !key_lock && !mouse_dragging && !keyboard_dragging) || clicking_powers) && !pc->stats.transformed)) {
			key_lock = true;
			if (pow->visible) {
				snd->play(pow->sfx_close, snd->DEFAULT_CHANNEL, snd->NO_POS, !snd->LOOP);
				closeRight();
			}
			else {
				closeRight();
				defocusLeft();
				act->requires_attention[MenuActionBar::MENU_POWERS] = false;
				pow->visible = true;
				snd->play(pow->sfx_open, snd->DEFAULT_CHANNEL, snd->NO_POS, !snd->LOOP);
			}
		}

		// character menu toggleggle
		if (menu->chr->enabled && ((inpt->pressing[Input::CHARACTER] && !key_lock && !mouse_dragging && !keyboard_dragging) || clicking_character)) {
			key_lock = true;
			if (chr->visible) {
				snd->play(chr->sfx_close, snd->DEFAULT_CHANNEL, snd->NO_POS, !snd->LOOP);
				closeLeft();
			}
			else {
				closeLeft();
				defocusRight();
				act->requires_attention[MenuActionBar::MENU_CHARACTER] = false;
				chr->visible = true;
				snd->play(chr->sfx_open, snd->DEFAULT_CHANNEL, snd->NO_POS, !snd->LOOP);
				// Make sure the stat list isn't scrolled when we open the character menu
				inpt->resetScroll();
			}
		}

		// log menu toggle
		if (menu->questlog->enabled && ((inpt->pressing[Input::LOG] && !key_lock && !mouse_dragging && !keyboard_dragging) || clicking_log)) {
			key_lock = true;
			if (questlog->visible) {
				snd->play(questlog->sfx_close, snd->DEFAULT_CHANNEL, snd->NO_POS, !snd->LOOP);
				closeLeft();
			}
			else {
				closeLeft();
				defocusRight();
				act->requires_attention[MenuActionBar::MENU_LOG] = false;
				questlog->visible = true;
				snd->play(questlog->sfx_open, snd->DEFAULT_CHANNEL, snd->NO_POS, !snd->LOOP);
				// Make sure the log isn't scrolled when we open the log menu
				inpt->resetScroll();
			}
		}

		//developer console
		if (settings->dev_mode && inpt->pressing[Input::DEVELOPER_MENU] && !inpt->lock[Input::DEVELOPER_MENU] && !mouse_dragging && !keyboard_dragging) {
			inpt->lock[Input::DEVELOPER_MENU] = true;
			if (devconsole->visible) {
				closeAll();
				key_lock = false;
			}
			else {
				closeAll();
				devconsole->visible = true;
			}
		}
	}

	bool console_open = settings->dev_mode && devconsole->visible;
	menus_open = (inv->visible || pow->visible || chr->visible || questlog->visible || vendor->visible || talker->visible || book->visible || console_open);
	pause = (eset->misc.menus_pause && menus_open) || exit->visible || console_open || book->visible;

	touch_controls->visible = !menus_open && !exit->visible && inpt->usingTouchscreen();

	if (pc->stats.alive && !inpt->usingMouse()) {
		if (inpt->pressing[Input::MAIN1]) inpt->lock[Input::MAIN1] = true;
		if (inpt->pressing[Input::MAIN2]) inpt->lock[Input::MAIN2] = true;

		dragAndDropWithKeyboard();
	}
	else if (pc->stats.alive) {
		// handle right-click
		if (!mouse_dragging && inpt->pressing[Input::MAIN2]) {
			// exit menu
			if (exit->visible && Utils::isWithinRect(exit->window_area, inpt->mouse)) {
				inpt->lock[Input::MAIN2] = true;
				inpt->pressing[Input::MAIN2] = false;
			}

			// book menu
			else if (book->visible && Utils::isWithinRect(book->window_area, inpt->mouse)) {
				inpt->lock[Input::MAIN2] = true;
				inpt->pressing[Input::MAIN2] = false;
			}

			// inventory
			else if (inv->visible && Utils::isWithinRect(inv->window_area, inpt->mouse)) {
				if (!inpt->lock[Input::MAIN2] && Utils::isWithinRect(inv->carried_area, inpt->mouse)) {
					// activate inventory item
					inv->activate(inpt->mouse);
				}
				inpt->lock[Input::MAIN2] = true;
				inpt->pressing[Input::MAIN2] = false;
			}

			// other menus
			else if (talker->visible && Utils::isWithinRect(talker->window_area, inpt->mouse)) {
				inpt->lock[Input::MAIN2] = true;
				inpt->pressing[Input::MAIN2] = false;
			}
			else if (pow->visible && Utils::isWithinRect(pow->window_area, inpt->mouse)) {
				inpt->lock[Input::MAIN2] = true;
				inpt->pressing[Input::MAIN2] = false;
			}
			else if (chr->visible && Utils::isWithinRect(chr->window_area, inpt->mouse)) {
				inpt->lock[Input::MAIN2] = true;
				inpt->pressing[Input::MAIN2] = false;
			}
			else if (questlog->visible && Utils::isWithinRect(questlog->window_area, inpt->mouse)) {
				inpt->lock[Input::MAIN2] = true;
				inpt->pressing[Input::MAIN2] = false;
			}
			else if (vendor->visible && Utils::isWithinRect(vendor->window_area, inpt->mouse)) {
				inpt->lock[Input::MAIN2] = true;
				inpt->pressing[Input::MAIN2] = false;
			}
			else if (stash->visible && Utils::isWithinRect(stash->window_area, inpt->mouse)) {
				inpt->lock[Input::MAIN2] = true;
				inpt->pressing[Input::MAIN2] = false;
			}
		}

		// handle left-click for book menu first
		if (!mouse_dragging && inpt->pressing[Input::MAIN1] && !inpt->lock[Input::MAIN1]) {
			if (book->visible && Utils::isWithinRect(book->window_area, inpt->mouse)) {
				inpt->lock[Input::MAIN1] = true;
			}
		}

		// handle left-click
		if (!mouse_dragging && inpt->pressing[Input::MAIN1] && !inpt->lock[Input::MAIN1]) {
			resetDrag();

			for (size_t i=0; i<menus.size(); ++i) {
				if (!menus[i]->visible || !Utils::isWithinRect(menus[i]->window_area, inpt->mouse)) {
					menus[i]->defocusTabLists();
				}
			}

			// exit menu
			if (exit->visible && Utils::isWithinRect(exit->window_area, inpt->mouse)) {
				inpt->lock[Input::MAIN1] = true;
			}


			if (chr->visible && Utils::isWithinRect(chr->window_area, inpt->mouse)) {
				inpt->lock[Input::MAIN1] = true;
			}

			if (vendor->visible && Utils::isWithinRect(vendor->window_area,inpt->mouse)) {
				inpt->lock[Input::MAIN1] = true;
				if (inpt->pressing[Input::CTRL]) {
					// buy item from a vendor
					stack = vendor->click(inpt->mouse);
					if (!inv->buy(stack, vendor->getTab(), !MenuInventory::IS_DRAGGING)) {
						vendor->itemReturn(inv->drop_stack.front());
						inv->drop_stack.pop();
					}
				}
				else {
					if (inpt->touch_locked) {
						showActionPicker(vendor, inpt->mouse);
					}
					else {
						// start dragging a vendor item
						drag_stack = vendor->click(inpt->mouse);
						if (!drag_stack.empty()) {
							mouse_dragging = true;
							drag_src = DRAG_SRC_VENDOR;
						}
						if (drag_stack.quantity > 1 && inpt->pressing[Input::SHIFT]) {
							int max_quantity = std::min(inv->getMaxPurchasable(drag_stack, vendor->getTab()), drag_stack.quantity);
							if (max_quantity >= 1) {
								num_picker->setValueBounds(1, max_quantity);
								num_picker->visible = true;
							}
							else {
								drag_stack.clear();
								resetDrag();
							}
						}
					}
				}
			}

			if (stash->visible && Utils::isWithinRect(stash->window_area,inpt->mouse)) {
				inpt->lock[Input::MAIN1] = true;
				if (inpt->pressing[Input::CTRL]) {
					// take an item from the stash
					stack = stash->click(inpt->mouse);
					if (!inv->add(stack, MenuInventory::CARRIED, ItemStorage::NO_SLOT, MenuInventory::ADD_PLAY_SOUND, MenuInventory::ADD_AUTO_EQUIP)) {
						stash->itemReturn(inv->drop_stack.front());
						inv->drop_stack.pop();
					}
					stash->tabs[stash->getTab()].updated = true;
				}
				else {
					if (inpt->touch_locked) {
						showActionPicker(stash, inpt->mouse);
					}
					else {
						// start dragging a stash item
						drag_stack = stash->click(inpt->mouse);
						if (!drag_stack.empty()) {
							mouse_dragging = true;
							drag_src = DRAG_SRC_STASH;
						}
						if (drag_stack.quantity > 1 && inpt->pressing[Input::SHIFT]) {
							num_picker->setValueBounds(1, drag_stack.quantity);
							num_picker->visible = true;
						}
					}
				}
			}

			if (questlog->visible && Utils::isWithinRect(questlog->window_area,inpt->mouse)) {
				inpt->lock[Input::MAIN1] = true;
			}

			// pick up an inventory item
			if (inv->visible && Utils::isWithinRect(inv->window_area,inpt->mouse)) {
				if (inpt->pressing[Input::CTRL]) {
					inpt->lock[Input::MAIN1] = true;
					stack = inv->click(inpt->mouse);
					if (stash->visible) {
						if (!stash->add(stack, MenuStash::NO_SLOT, MenuStash::ADD_PLAY_SOUND)) {
							inv->itemReturn(stash->drop_stack.front());
							stash->drop_stack.pop();
						}
					}
					else {
						// The vendor could have a limited amount of currency in the future. It will be tested here.
						if ((eset->misc.sell_without_vendor || vendor->visible) && inv->sell(stack)) {
							if (vendor->visible) {
								vendor->setTab(ItemManager::VENDOR_SELL);
								vendor->add(stack);
							}
						}
						else {
							inv->itemReturn(stack);
						}
					}
				}
				else {
					inpt->lock[Input::MAIN1] = true;

					if (inpt->touch_locked) {
						showActionPicker(inv, inpt->mouse);
					}
					else {
						drag_stack = inv->click(inpt->mouse);
						if (!drag_stack.empty()) {
							mouse_dragging = true;
							drag_src = DRAG_SRC_INVENTORY;
						}
						if (drag_stack.quantity > 1 && inpt->pressing[Input::SHIFT]) {
							num_picker->setValueBounds(1, drag_stack.quantity);
							num_picker->visible = true;
						}
					}
				}
			}
			// pick up a power
			if (pow->visible && Utils::isWithinRect(pow->window_area,inpt->mouse)) {
				inpt->lock[Input::MAIN1] = true;

				if (inpt->touch_locked) {
					showActionPicker(pow, inpt->mouse);
				}
				else {
					// check for unlock/dragging
					MenuPowersClick pow_click = pow->click(inpt->mouse);
					drag_power = pow_click.drag;
					if (drag_power > 0) {
						mouse_dragging = true;
						keyboard_dragging = false;
						drag_src = DRAG_SRC_POWERS;
					}
					else if (pow_click.unlock > 0) {
						pow->clickUnlock(pow_click.unlock);
					}
				}
			}
			// action bar
			if (!exit->visible && (act->isWithinSlots(inpt->mouse) || act->isWithinMenus(inpt->mouse))) {
				inpt->lock[Input::MAIN1] = true;

				// ctrl-click action bar to clear that slot
				if (inpt->pressing[Input::CTRL]) {
					act->remove(inpt->mouse);
				}
				// allow drag-to-rearrange action bar
				else if (!act->isWithinMenus(inpt->mouse)) {
					if (inpt->touch_locked) {
						WidgetSlot* act_slot = act->getSlotFromPosition(inpt->mouse);
						if (act_slot) {
							act->tablist.setCurrent(act_slot);
							keydrag_pos = Point(act_slot->pos.x, act_slot->pos.y);
						}
						showActionPicker(act, inpt->mouse);
					}
					else {
						drag_power = act->checkDrag(inpt->mouse);
						if (drag_power > 0) {
							mouse_dragging = true;
							drag_src = DRAG_SRC_ACTIONBAR;
						}
					}
				}

				// else, clicking action bar to use a power?
				// this check is done by GameEngine when calling Avatar::logic()


			}
		}

		// highlight matching inventory slots based on what we're dragging
		if (inv->visible && (mouse_dragging || keyboard_dragging)) {
			inv->inventory[MenuInventory::EQUIPMENT].highlightMatching(drag_stack.item);
		}

		// handle dropping
		if (mouse_dragging && ((sticky_dragging && inpt->pressing[Input::MAIN1] && !inpt->lock[Input::MAIN1]) || (!sticky_dragging && !inpt->pressing[Input::MAIN1]))) {
			if (sticky_dragging) {
				inpt->lock[Input::MAIN1] = true;
				sticky_dragging = false;
			}

			// putting a power on the Action Bar
			if (drag_src == DRAG_SRC_POWERS) {
				if (act->isWithinSlots(inpt->mouse)) {
					act->drop(inpt->mouse, drag_power, !MenuActionBar::REORDER);
				}
			}

			// rearranging the action bar
			else if (drag_src == DRAG_SRC_ACTIONBAR) {
				if (act->isWithinSlots(inpt->mouse)) {
					act->drop(inpt->mouse, drag_power, MenuActionBar::REORDER);
					// for locked slots forbid power dropping
				}
				else if (act->locked[act->drag_prev_slot]) {
					act->hotkeys[act->drag_prev_slot] = drag_power;
				}
				drag_power = 0;
			}

			// rearranging inventory or dropping items
			else if (drag_src == DRAG_SRC_INVENTORY) {

				if (inv->visible && Utils::isWithinRect(inv->window_area, inpt->mouse)) {
					inv->drop(inpt->mouse, drag_stack);
				}
				else if (act->isWithinSlots(inpt->mouse)) {
					// The action bar is not storage!
					inv->itemReturn(drag_stack);
					inv->applyEquipment();

					// put an item with a power on the action bar
					if (items->isValid(drag_stack.item) && items->items[drag_stack.item]->power != 0) {
						act->drop(inpt->mouse, items->items[drag_stack.item]->power, !MenuActionBar::REORDER);
					}
				}
				else if (vendor->visible && Utils::isWithinRect(vendor->window_area, inpt->mouse)) {
					if (inv->sell( drag_stack)) {
						vendor->setTab(ItemManager::VENDOR_SELL);
						vendor->add( drag_stack);
					}
					else {
						inv->itemReturn(drag_stack);
					}
				}
				else if (stash->visible && Utils::isWithinRect(stash->window_area, inpt->mouse)) {
					for (size_t i = 0; i < stash->tabs.size(); ++i) {
						stash->tabs[i].stock.drag_prev_slot = -1;
					}
					if (!stash->drop(inpt->mouse, drag_stack)) {
						inv->itemReturn(stash->drop_stack.front());
						stash->drop_stack.pop();
					}
				}
				else {
					// if dragging and the source was inventory, drop item to the floor

					// quest items cannot be dropped
					bool item_is_valid = items->isValid(drag_stack.item);
					if (!item_is_valid || (item_is_valid && !items->items[drag_stack.item]->quest_item)) {
						drop_stack.push(drag_stack);
					}
					else {
						pc->logMsg(msg->get("This item can not be dropped."), Avatar::MSG_NORMAL);
						items->playSound(drag_stack.item);

						inv->itemReturn(drag_stack);
					}
				}
				inv->clearHighlight();
			}

			else if (drag_src == DRAG_SRC_VENDOR) {

				// dropping an item from vendor (we only allow to drop into the carried area)
				if (inv->visible && Utils::isWithinRect(inv->window_area, inpt->mouse)) {
					if (!inv->buy(drag_stack, vendor->getTab(), MenuInventory::IS_DRAGGING)) {
						vendor->itemReturn(inv->drop_stack.front());
						inv->drop_stack.pop();
					}
				}
				else {
					items->playSound(drag_stack.item);
					vendor->itemReturn(drag_stack);
				}
			}

			else if (drag_src == DRAG_SRC_STASH) {

				// dropping an item from stash (we only allow to drop into the carried area)
				if (inv->visible && Utils::isWithinRect(inv->window_area, inpt->mouse)) {
					if (!inv->drop(inpt->mouse, drag_stack)) {
						stash->itemReturn(inv->drop_stack.front());
						inv->drop_stack.pop();
					}
					stash->tabs[stash->getTab()].updated = true;
				}
				else if (stash->visible && Utils::isWithinRect(stash->window_area, inpt->mouse)) {
					if (!stash->drop(inpt->mouse,drag_stack)) {
						drop_stack.push(stash->drop_stack.front());
						stash->drop_stack.pop();
					}
				}
				else {
					drop_stack.push(drag_stack);
					stash->tabs[stash->getTab()].updated = true;
				}
			}

			drag_stack.clear();
			drag_power = 0;
			drag_src = DRAG_SRC_NONE;
			mouse_dragging = false;
		}
	}
	else {
		if (mouse_dragging || keyboard_dragging) {
			resetDrag();
		}
	}

	// return items that are currently begin dragged when returning to title screen or exiting game
	if (done || inpt->done) {
		resetDrag();
	}

	// handle equipment changes affecting hero stats
	if (inv->changed_equipment) {
		inv->applyEquipment();
		// the equipment flags get reset in GameStatePlay
	}

	if (!(mouse_dragging || keyboard_dragging)) {
		setDragIcon(WidgetSlot::NO_ICON, WidgetSlot::NO_OVERLAY);
	}

	// auto-select tablists when using keyboard/gamepad
	if (!inpt->usingMouse()) {
		if (game_over->visible && !game_over->tablist.isLocked() && game_over->tablist.getCurrent() == -1) {
			game_over->tablist.getNext(!TabList::GET_INNER, TabList::WIDGET_SELECT_AUTO);
		}
		else if (num_picker->visible && !num_picker->tablist.isLocked() && num_picker->tablist.getCurrent() == -1) {
			num_picker->tablist.getNext(!TabList::GET_INNER, TabList::WIDGET_SELECT_AUTO);
		}
		else if (action_picker->visible && !action_picker->tablist.isLocked() && action_picker->tablist.getCurrent() == -1) {
			action_picker->tablist.getNext(!TabList::GET_INNER, TabList::WIDGET_SELECT_AUTO);
		}
		else if (book->visible && !book->tablist.isLocked() && book->tablist.getCurrent() == -1) {
			book->tablist.getNext(!TabList::GET_INNER, TabList::WIDGET_SELECT_AUTO);
		}
		else if (!isTabListSelected()) {
			inv->tablist.lock();
			pow->tablist.lock();
			chr->tablist.lock();
			questlog->tablist.lock();
			stash->tablist.lock();
			vendor->tablist.lock();

			if (inv->visible) {
				inv->tablist.unlock();
				inv->tablist.getNext(!TabList::GET_INNER, TabList::WIDGET_SELECT_AUTO);
			}
			else if (pow->visible) {
				pow->tablist.unlock();
				pow->tablist.getNext(!TabList::GET_INNER, TabList::WIDGET_SELECT_AUTO);
			}
			else if (chr->visible) {
				chr->tablist.unlock();
				chr->tablist.getNext(!TabList::GET_INNER, TabList::WIDGET_SELECT_AUTO);
			}
			else if (questlog->visible) {
				questlog->tablist.unlock();
				questlog->tablist.getNext(!TabList::GET_INNER, TabList::WIDGET_SELECT_AUTO);
			}
		}
	}
}

void MenuManager::dragAndDropWithKeyboard() {
	// inventory menu

	if (inv->visible && inv->getCurrentTabList() && drag_src != DRAG_SRC_ACTIONBAR) {
		int slot_index = inv->getCurrentTabList()->getCurrent();
		Point src_slot;
		WidgetSlot * inv_slot;

		if (slot_index < inv->getEquippedCount())
			inv_slot = inv->inventory[MenuInventory::EQUIPMENT].slots[slot_index];
		else if (slot_index < inv->getTotalSlotCount())
			inv_slot = inv->inventory[MenuInventory::CARRIED].slots[slot_index - inv->getEquippedCount()];
		else
			inv_slot = NULL;

		if (inv_slot) {
			src_slot.x = inv_slot->pos.x;
			src_slot.y = inv_slot->pos.y;

			WidgetSlot::CLICK_TYPE slotClick = inv_slot->checkClick();

			// pick up item
			if (slotClick == WidgetSlot::DRAG && drag_stack.empty()) {
				showActionPicker(inv, src_slot);
			}
			// rearrange item
			else if (slotClick == WidgetSlot::DRAG && !drag_stack.empty()) {
				inv->drop(src_slot, drag_stack);
				drag_src = DRAG_SRC_NONE;
				drag_stack.clear();
				keyboard_dragging = false;
				sticky_dragging = false;
			}
			else if (slotClick == WidgetSlot::ACTIVATE && drag_stack.empty()) {
				inv->activate(src_slot);
			}
		}
	}

	// vendor menu
	if (vendor->visible && vendor->getCurrentTabList() && vendor->getCurrentTabList() != (&vendor->tablist) && drag_src != DRAG_SRC_ACTIONBAR) {
		int slot_index = vendor->getCurrentTabList()->getCurrent();
		Point src_slot;
		WidgetSlot * vendor_slot;

		if (vendor->getTab() == ItemManager::VENDOR_SELL)
			vendor_slot = vendor->stock[ItemManager::VENDOR_SELL].slots[slot_index];
		else
			vendor_slot = vendor->stock[ItemManager::VENDOR_BUY].slots[slot_index];

		src_slot.x = vendor_slot->pos.x;
		src_slot.y = vendor_slot->pos.y;

		WidgetSlot::CLICK_TYPE slotClick = vendor_slot->checkClick();

		// pick up item
		if (slotClick == WidgetSlot::DRAG && drag_stack.empty()) {
			showActionPicker(vendor, src_slot);
		}
	}

	// stash menu
	if (stash->visible && stash->getCurrentTabList() && drag_src != DRAG_SRC_ACTIONBAR) {
		int slot_index = stash->getCurrentTabList()->getCurrent();
		size_t tab = stash->getTab();
		WidgetSlot::CLICK_TYPE slotClick = stash->tabs[tab].stock.slots[slot_index]->checkClick();
		Point src_slot(stash->tabs[tab].stock.slots[slot_index]->pos.x, stash->tabs[tab].stock.slots[slot_index]->pos.y);

		// pick up item
		if (slotClick == WidgetSlot::DRAG && drag_stack.empty()) {
			showActionPicker(stash, src_slot);
		}
		// rearrange item
		else if (slotClick == WidgetSlot::DRAG && !drag_stack.empty()) {
			if (!stash->drop(src_slot, drag_stack)) {
				drop_stack.push(stash->drop_stack.front());
				stash->drop_stack.pop();
			}
			inv->clearHighlight();
			drag_src = DRAG_SRC_NONE;
			drag_stack.clear();
			keyboard_dragging = false;
			sticky_dragging = false;
		}
	}

	// powers menu
	if (pow->visible && pow->isTabListSelected() && drag_src != DRAG_SRC_ACTIONBAR) {
		int slot_index = pow->getSelectedCellIndex();
		Point src_slot(pow->slots[slot_index]->pos.x, pow->slots[slot_index]->pos.y);

		// save slot enable state
		bool slot_enabled = pow->slots[slot_index]->enabled;

		// temporarily enable slot if the power can be upgraded
		if (pow->click(src_slot).unlock > 0) {
			pow->slots[slot_index]->enabled = true;
		}

		WidgetSlot::CLICK_TYPE slotClick = pow->slots[slot_index]->checkClick();

		// restore slot enable state
		pow->slots[slot_index]->enabled = slot_enabled;

		if (slotClick == WidgetSlot::DRAG) {
			showActionPicker(pow, src_slot);
		}
	}

	// actionbar
	if (act->getCurrentTabList() && static_cast<unsigned>(act->getCurrentTabList()->getCurrent()) < act->slots.size()) {
		size_t slot_index = act->getCurrentSlotIndexFromTablist();
		if (slot_index < act->slots.size() + MenuActionBar::MENU_COUNT && act->slots[slot_index]) {
			WidgetSlot::CLICK_TYPE slotClick = act->slots[slot_index]->checkClick();
			Point dest_slot = act->getSlotPos(slot_index);

			// pick up power
			if (slotClick == WidgetSlot::DRAG && drag_stack.empty() && drag_power == 0) {
				showActionPicker(act, dest_slot);
			}
			// drop power/item from other menu
			else if (slotClick == WidgetSlot::DRAG && drag_src != DRAG_SRC_ACTIONBAR && (!drag_stack.empty() || drag_power > 0)) {
				if (drag_src == DRAG_SRC_POWERS) {
					act->drop(dest_slot, drag_power, !MenuActionBar::REORDER);
				}
				else if (drag_src == DRAG_SRC_INVENTORY) {
					if (items->isValid(drag_stack.item) && items->items[drag_stack.item]->power != 0) {
						act->drop(dest_slot, items->items[drag_stack.item]->power, !MenuActionBar::REORDER);
					}
				}
				resetDrag();
				inv->applyEquipment();
			}
			// rearrange actionbar
			else if (slotClick == WidgetSlot::DRAG && drag_src == DRAG_SRC_ACTIONBAR && drag_power > 0) {
				act->drop(dest_slot, drag_power, MenuActionBar::REORDER);
				drag_src = DRAG_SRC_NONE;
				drag_power = 0;
				keyboard_dragging = false;
			}
		}
	}
}

void MenuManager::resetDrag() {
	if (drag_src == DRAG_SRC_VENDOR) {
		vendor->itemReturn(drag_stack);
		vendor->unlockTabControl();
		inv->clearHighlight();
	}
	else if (drag_src == DRAG_SRC_STASH) {
		stash->itemReturn(drag_stack);
		inv->clearHighlight();
	}
	else if (drag_src == DRAG_SRC_INVENTORY) {
		inv->itemReturn(drag_stack);
		inv->clearHighlight();
	}
	else if (drag_src == DRAG_SRC_ACTIONBAR) {
		act->actionReturn(drag_power);
	}

	drag_src = DRAG_SRC_NONE;
	drag_stack.clear();
	drag_power = 0;

	setDragIcon(WidgetSlot::NO_ICON, WidgetSlot::NO_OVERLAY);

	vendor->stock[ItemManager::VENDOR_BUY].drag_prev_slot = -1;
	vendor->stock[ItemManager::VENDOR_SELL].drag_prev_slot = -1;
	for (size_t i = 0; i < stash->tabs.size(); ++i) {
		stash->tabs[i].stock.drag_prev_slot = -1;
	}
	inv->drag_prev_src = -1;
	inv->inventory[MenuInventory::EQUIPMENT].drag_prev_slot = -1;
	inv->inventory[MenuInventory::CARRIED].drag_prev_slot = -1;

	keyboard_dragging = false;
	mouse_dragging = false;
	sticky_dragging = false;
}

void MenuManager::render() {
	if (!settings->show_hud) {
		// if the hud is disabled, only show a few necessary menus

		// exit menu
		exit->render();

		// dev console
		if (settings->dev_mode)
			devconsole->render();

		return;
	}

	bool hudlog_overlapped = false;
	if (chr->visible && Utils::rectsOverlap(hudlog->window_area, chr->window_area)) {
		hudlog_overlapped = true;
	}
	if (questlog->visible && Utils::rectsOverlap(hudlog->window_area, questlog->window_area)) {
		hudlog_overlapped = true;
	}
	if (inv->visible && Utils::rectsOverlap(hudlog->window_area, inv->window_area)) {
		hudlog_overlapped = true;
	}
	if (pow->visible && Utils::rectsOverlap(hudlog->window_area, pow->window_area)) {
		hudlog_overlapped = true;
	}
	if (vendor->visible && Utils::rectsOverlap(hudlog->window_area, vendor->window_area)) {
		hudlog_overlapped = true;
	}
	if (stash->visible && Utils::rectsOverlap(hudlog->window_area, stash->window_area)) {
		hudlog_overlapped = true;
	}
	if (talker->visible && Utils::rectsOverlap(hudlog->window_area, talker->window_area)) {
		hudlog_overlapped = true;
	}

	for (size_t i=0; i<menus.size(); i++) {
		if (menus[i] == hudlog && hudlog_overlapped && !hudlog->hide_overlay) {
			continue;
		}

		menus[i]->render();
	}

	if (hudlog_overlapped && !hudlog->hide_overlay) {
		hudlog->renderOverlay();
	}

	subtitles->render();

	touch_controls->render();

	if (!num_picker->visible && !action_picker->visible && !mouse_dragging && !sticky_dragging) {
		if (!inpt->usingMouse() || inpt->usingTouchscreen())
			handleKeyboardTooltips();
		else {
			// Find tooltips depending on mouse position
			if (!book->visible) {
				pushMatchingItemsOf(inpt->mouse);

				chr->renderTooltips(inpt->mouse);
				vendor->renderTooltips(inpt->mouse);
				stash->renderTooltips(inpt->mouse);
				pow->renderTooltips(inpt->mouse);
				inv->renderTooltips(inpt->mouse);
			}
			if (!exit->visible) {
				effects->renderTooltips(inpt->mouse);
				act->renderTooltips(inpt->mouse);
			}
		}
	}

	// draw icon under cursor if dragging
	if (mouse_dragging && !num_picker->visible) {
		if (drag_src == DRAG_SRC_INVENTORY || drag_src == DRAG_SRC_VENDOR || drag_src == DRAG_SRC_STASH)
			setDragIconItem(drag_stack);
		else if (drag_src == DRAG_SRC_POWERS || drag_src == DRAG_SRC_ACTIONBAR)
			setDragIcon(powers->powers[drag_power]->icon, -1);

		if (inpt->usingTouchscreen() && sticky_dragging)
			renderIcon(keydrag_pos.x - eset->resolutions.icon_size/2, keydrag_pos.y - eset->resolutions.icon_size/2);
		else
			renderIcon(inpt->mouse.x - eset->resolutions.icon_size/2, inpt->mouse.y - eset->resolutions.icon_size/2);
	}
	else if (keyboard_dragging && !num_picker->visible) {
		if (drag_src == DRAG_SRC_INVENTORY || drag_src == DRAG_SRC_VENDOR || drag_src == DRAG_SRC_STASH)
			setDragIconItem(drag_stack);
		else if (drag_src == DRAG_SRC_POWERS || drag_src == DRAG_SRC_ACTIONBAR)
			setDragIcon(powers->powers[drag_power]->icon, -1);

		renderIcon(keydrag_pos.x - eset->resolutions.icon_size/2, keydrag_pos.y - eset->resolutions.icon_size/2);
	}

	// render the dev console above everything else
	if (settings->dev_mode) {
		devconsole->render();
	}
}

void MenuManager::handleKeyboardTooltips() {
	if (book->visible)
		return;

	if (vendor->visible) {
		TabList* cur_tablist = vendor->getCurrentTabList();
		if (cur_tablist && cur_tablist != &(vendor->tablist)) {
			int slot_index = cur_tablist->getCurrent();

			if (vendor->getTab() == ItemManager::VENDOR_BUY) {
				keydrag_pos.x = vendor->stock[ItemManager::VENDOR_BUY].slots[slot_index]->pos.x;
				keydrag_pos.y = vendor->stock[ItemManager::VENDOR_BUY].slots[slot_index]->pos.y;
			}
			else if (vendor->getTab() == ItemManager::VENDOR_SELL) {
				keydrag_pos.x = vendor->stock[ItemManager::VENDOR_SELL].slots[slot_index]->pos.x;
				keydrag_pos.y = vendor->stock[ItemManager::VENDOR_SELL].slots[slot_index]->pos.y;
			}

			Point tooltip_pos = keydrag_pos;
			tooltip_pos.x += eset->resolutions.icon_size / 2;
			tooltip_pos.y += eset->resolutions.icon_size / 2;

			pushMatchingItemsOf(tooltip_pos);
			vendor->renderTooltips(tooltip_pos);
		}
	}

	if (stash->visible) {
		TabList* cur_tablist = stash->getCurrentTabList();
		if (cur_tablist && cur_tablist != &(stash->tablist)) {
			int slot_index = stash->getCurrentTabList()->getCurrent();
			size_t tab = stash->getTab();

			keydrag_pos.x = stash->tabs[tab].stock.slots[slot_index]->pos.x;
			keydrag_pos.y = stash->tabs[tab].stock.slots[slot_index]->pos.y;

			Point tooltip_pos = keydrag_pos;
			tooltip_pos.x += eset->resolutions.icon_size / 2;
			tooltip_pos.y += eset->resolutions.icon_size / 2;

			pushMatchingItemsOf(tooltip_pos);
			stash->renderTooltips(tooltip_pos);
		}
	}

	if (pow->visible && pow->isTabListSelected()) {
		int slot_index = pow->getSelectedCellIndex();

		keydrag_pos.x = pow->slots[slot_index]->pos.x;
		keydrag_pos.y = pow->slots[slot_index]->pos.y;

		Point tooltip_pos = keydrag_pos;
		tooltip_pos.x += eset->resolutions.icon_size / 2;
		tooltip_pos.y += eset->resolutions.icon_size / 2;

		pow->renderTooltips(tooltip_pos);
	}

	if (inv->visible && inv->getCurrentTabList()) {
		int slot_index = inv->getCurrentTabList()->getCurrent();

		if (slot_index < inv->getEquippedCount()) {
			keydrag_pos.x = inv->inventory[MenuInventory::EQUIPMENT].slots[slot_index]->pos.x;
			keydrag_pos.y = inv->inventory[MenuInventory::EQUIPMENT].slots[slot_index]->pos.y;
		}
		else if (slot_index < inv->getTotalSlotCount()) {
			keydrag_pos.x = inv->inventory[MenuInventory::CARRIED].slots[slot_index - inv->getEquippedCount()]->pos.x;
			keydrag_pos.y = inv->inventory[MenuInventory::CARRIED].slots[slot_index - inv->getEquippedCount()]->pos.y;
		}
		else {
			Widget *temp_widget = inv->getCurrentTabList()->getWidgetByIndex(slot_index);
			keydrag_pos.x = temp_widget->pos.x;
			keydrag_pos.y = temp_widget->pos.y;
		}

		Point tooltip_pos = keydrag_pos;
		tooltip_pos.x += eset->resolutions.icon_size / 2;
		tooltip_pos.y += eset->resolutions.icon_size / 2;

		pushMatchingItemsOf(tooltip_pos);
		inv->renderTooltips(tooltip_pos);
	}

	if (act->getCurrentTabList()) {
		size_t slot_index = act->getCurrentSlotIndexFromTablist();
		if (slot_index < act->slots.size() + MenuActionBar::MENU_COUNT) {
			keydrag_pos = act->getSlotPos(slot_index);

			Point tooltip_pos = keydrag_pos;
			tooltip_pos.x += eset->resolutions.icon_size / 2;
			tooltip_pos.y += eset->resolutions.icon_size / 2;

			act->renderTooltips(tooltip_pos);
		}
	}
}

void MenuManager::closeAll() {
	closeLeft();
	closeRight();
}

void MenuManager::closeLeft() {
	if (num_picker->visible) {
		drag_stack.clear();
		num_picker->closeWindow();
	}

	resetDrag();
	chr->visible = false;
	questlog->visible = false;
	exit->visible = false;
	stash->visible = false;
	book->setBookFilename("");

	talker->setNPC(NULL);
	vendor->setNPC(NULL);

	if (settings->dev_mode && devconsole->visible) {
		devconsole->closeWindow();
	}

	defocusLeft();
}

void MenuManager::closeRight() {
	if (num_picker->visible) {
		drag_stack.clear();
		num_picker->closeWindow();
	}

	resetDrag();
	inv->visible = false;
	pow->visible = false;
	exit->visible = false;
	book->setBookFilename("");

	talker->setNPC(NULL);

	if (settings->dev_mode && devconsole->visible) {
		devconsole->closeWindow();
	}

	defocusRight();
}

bool MenuManager::isDragging() {
	return drag_src != DRAG_SRC_NONE;
}

bool MenuManager::isNPCMenuVisible() {
	return talker->visible || vendor->visible;
}

void MenuManager::showExitMenu() {
	if (game_over->visible)
		return;

	pause = true;
	closeAll();
	if (exit) {
		// handleCancel() will show the menu, but only if it is already hidden
		// we use handleCancel() here because it will reset the menu to the "Exit" tab
		exit->visible = false;
		exit->handleCancel();
	}
}

void MenuManager::pushMatchingItemsOf(const Point& hov_pos) {
	if (!settings->item_compare_tips)
		return;

	int area;
	ItemStack hov_stack;

	if (inv->visible && Utils::isWithinRect(inv->window_area, hov_pos)) {
		area = inv->areaOver(hov_pos);
		if (area == MenuInventory::CARRIED)
			hov_stack = inv->inventory[area].getItemStackAtPos(hov_pos);
	}
	else if (vendor->visible && Utils::isWithinRect(vendor->window_area, hov_pos)) {
		area = vendor->getTab();
		if (area >= 0)
			hov_stack = vendor->stock[area].getItemStackAtPos(hov_pos);
	}
	else if (stash->visible && Utils::isWithinRect(stash->window_area, hov_pos)) {
		area = static_cast<int>(stash->getTab());
		if (area >= 0)
			hov_stack = stash->tabs[area].stock.getItemStackAtPos(hov_pos);
	}

	// we assume that a non-empty item type means that there is a primary tooltip
	if (items->isValid(hov_stack.item) && !items->getItemType(items->items[hov_stack.item]->type).name.empty()) {
		size_t tip_index = 1;

		//get equipped items of the same type
		for (size_t i = 0; i < inv->equipped_area.size(); i++) {
			if (tip_index >= eset->tooltips.visible_max)
				break; // can't show any more tooltips

			if (inv->isActive(i)){
				if (inv->slot_type[i] == items->items[hov_stack.item]->type) {
					if (!inv->inventory[MenuInventory::EQUIPMENT].storage[i].empty()) {
						Point match_pos(inv->equipped_area[i].x, inv->equipped_area[i].y);

						TooltipData match = inv->inventory[MenuInventory::EQUIPMENT].checkTooltip(match_pos, &pc->stats, ItemManager::PLAYER_INV, !ItemManager::TOOLTIP_INPUT_HINT);
						match.addColoredText(msg->get("Equipped"), font->getColor(FontEngine::COLOR_ITEM_FLAVOR));

						tooltipm->push(match, hov_pos, TooltipData::STYLE_FLOAT, tip_index);
						tip_index++;
					}
				}
			}
		}
	}
}

void MenuManager::defocusLeft() {
	chr->defocusTabLists();
	questlog->defocusTabLists();
	stash->defocusTabLists();
	vendor->defocusTabLists();
}

void MenuManager::defocusRight() {
	inv->defocusTabLists();
	pow->defocusTabLists();
}

bool MenuManager::isTabListSelected() {
	for (size_t i = 0; i < menus.size(); ++i) {
		if (menus[i]->getCurrentTabList())
			return true;
	}

	if (devconsole && devconsole->tablist.getCurrent() != -1)
		return true;

	return false;
}

void MenuManager::showActionPicker(Menu* src_menu, const Point& target) {
	action_picker->action_list->clear();
	action_picker_map.clear();

	if (src_menu == act) {
		PowerID slot_power = act->checkDrag(target);
		act->actionReturn(slot_power);

		if (slot_power > 0) {
			action_src = ACTION_SRC_ACTIONBAR;
			action_picker_target = target;

			if (inpt->mode == InputState::MODE_TOUCHSCREEN) {
				action_picker->action_list->append(msg->get("Use"), "");
				action_picker_map[action_picker_map.size()] = ACTION_PICKER_ACTIONBAR_USE;
			}

			action_picker->action_list->append(msg->get("Move"), "");
			action_picker_map[action_picker_map.size()] = ACTION_PICKER_ACTIONBAR_SELECT;

			action_picker->action_list->append(msg->get("Clear"), "");
			action_picker_map[action_picker_map.size()] = ACTION_PICKER_ACTIONBAR_CLEAR;
		}
	}
	else if (src_menu == pow) {
		MenuPowersClick pow_click = pow->click(target);

		if (pow_click.drag > 0 || pow_click.unlock > 0) {
			action_src = ACTION_SRC_POWERS;
			action_picker_target = target;
		}

		if (pow_click.drag > 0) {
			action_picker->action_list->append(msg->get("Equip"), "");
			action_picker_map[0] = ACTION_PICKER_POWERS_SELECT;
		}
		if (pow_click.unlock > 0) {
			action_picker->action_list->append(msg->get("Upgrade"), "");
			action_picker_map[action_picker_map.size()] = ACTION_PICKER_POWERS_UPGRADE;
		}
	}
	else if (src_menu == inv) {
		ItemStack inv_click = inv->click(target);
		if (inv_click.quantity == 1) {
			inv->itemReturn(inv_click);
		}

		if (!inv_click.empty()) {
			action_src = ACTION_SRC_INVENTORY;
			action_picker_target = target;

			if (vendor->visible) {
				action_picker->action_list->append(msg->get("Sell"), "");
				action_picker_map[action_picker_map.size()] = ACTION_PICKER_INVENTORY_SELL;
			}
			else if (stash->visible) {
				action_picker->action_list->append(msg->get("Transfer"), "");
				action_picker_map[action_picker_map.size()] = ACTION_PICKER_INVENTORY_STASH;
			}

			action_picker->action_list->append(msg->get("Move"), "");
			action_picker_map[action_picker_map.size()] = ACTION_PICKER_INVENTORY_SELECT;

			if (inv->canUseItem(target)) {
				action_picker->action_list->append(msg->get("Use"), "");
				action_picker_map[action_picker_map.size()] = ACTION_PICKER_INVENTORY_ACTIVATE;
			}
			else if (inv->canEquipItem(target)) {
				action_picker->action_list->append(msg->get("Equip"), "");
				action_picker_map[action_picker_map.size()] = ACTION_PICKER_INVENTORY_ACTIVATE;
			}

			action_picker->action_list->append(msg->get("Drop"), "");
			action_picker_map[action_picker_map.size()] = ACTION_PICKER_INVENTORY_DROP;

			if (!vendor->visible && eset->misc.sell_without_vendor) {
				action_picker->action_list->append(msg->get("Sell"), "");
				action_picker_map[action_picker_map.size()] = ACTION_PICKER_INVENTORY_SELL;
			}
		}
	}
	else if (src_menu == stash) {
		ItemStack stash_click = stash->click(target);
		if (stash_click.quantity == 1) {
			stash->itemReturn(stash_click);
		}

		if (!stash_click.empty()) {
			action_src = ACTION_SRC_STASH;
			action_picker_target = target;

			action_picker->action_list->append(msg->get("Move"), "");
			action_picker_map[action_picker_map.size()] = ACTION_PICKER_STASH_SELECT;

			action_picker->action_list->append(msg->get("Transfer"), "");
			action_picker_map[action_picker_map.size()] = ACTION_PICKER_STASH_TRANSFER;
		}
	}
	else if (src_menu == vendor) {
		ItemStack vendor_click = vendor->click(target);
		if (vendor_click.quantity == 1) {
			vendor->itemReturn(vendor_click);
		}

		if (!vendor_click.empty()) {
			action_src = ACTION_SRC_VENDOR;
			action_picker_target = target;

			action_picker->action_list->append(msg->get("Buy"), "");
			action_picker_map[action_picker_map.size()] = ACTION_PICKER_VENDOR_BUY;
		}
	}

	if (!action_picker_map.empty())
		action_picker->show();
}

void MenuManager::actionPickerStartDrag() {
	if (inpt->usingMouse()) {
		mouse_dragging = true;
		sticky_dragging = true;
	}
	else {
		keyboard_dragging = true;
	}
}

MenuManager::~MenuManager() {
	for (size_t i = 0; i < resource_statbars.size(); ++i) {
		delete resource_statbars[i];
	}

	delete hp;
	delete mp;
	delete xp;
	delete mini;
	delete inv;
	delete pow;
	delete chr;
	delete hudlog;
	delete questlog;
	delete act;
	delete vendor;
	delete talker;
	delete exit;
	delete enemy;
	delete effects;
	delete stash;
	delete book;
	delete num_picker;
	delete game_over;
	delete action_picker;

	if (settings->dev_mode) {
		delete devconsole;
	}

	delete touch_controls;

	delete subtitles;

	delete drag_icon;
}
