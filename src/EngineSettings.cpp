/*
Copyright © 2018 Justin Jacobs

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
 * class EngineSettings
 */

#include "EngineSettings.h"
#include "FileParser.h"
#include "FontEngine.h"
#include "MenuActionBar.h"
#include "MessageEngine.h"
#include "ModManager.h"
#include "Settings.h"
#include "SharedResources.h"
#include "Utils.h"
#include "UtilsMath.h"
#include "UtilsParsing.h"
#include <sstream>

void EngineSettings::load() {
	misc.load();
	resolutions.load();
	gameplay.load();
	combat.load();
	equip_flags.load();
	primary_stats.load();
	hero_classes.load(); // depends on primary_stats
	damage_types.load();
	death_penalty.load();
	tooltips.load();
	loot.load(); // depends on misc
	tileset.load();
	widgets.load();
	xp.load();
	number_format.load();
	resource_stats.load();
}

void EngineSettings::Misc::load() {
	// reset to defaults
	save_hpmp = false;
	corpse_timeout = 60 * settings->max_frames_per_sec;
	corpse_timeout_enabled = true;
	sell_without_vendor = true;
	aim_assist = 0;
	window_title = "Flare";
	save_prefix = "";
	sound_falloff = 15;
	party_exp_percentage = 100;
	enable_ally_collision = true;
	enable_ally_collision_ai = true;
	currency_id = 1;
	interact_range = 3;
	menus_pause = false;
	save_onload = true;
	save_onexit = true;
	save_pos_onexit = false;
	save_oncutscene = true;
	save_onstash = SAVE_ONSTASH_ALL;
	save_anywhere = false;
	camera_speed = 10.f * (static_cast<float>(settings->max_frames_per_sec) / Settings::LOGIC_FPS);
	save_buyback = true;
	keep_buyback_on_map_change = true;
	sfx_unable_to_cast = "";
	combat_aborts_npc_interact = true;
	fogofwar = 0;
	save_fogofwar = false;
	mouse_move_deadzone_moving = 0.25f;
	mouse_move_deadzone_not_moving = 0.75f;
	passive_trigger_effect_stacking = false;

	FileParser infile;
	// @CLASS EngineSettings: Misc|Description of engine/misc.txt
	if (infile.open("engine/misc.txt", FileParser::MOD_FILE, FileParser::ERROR_NORMAL)) {
		while (infile.next()) {
			// @ATTR save_hpmp|bool|When saving the game, keep the hero's current HP and MP.
			if (infile.key == "save_hpmp")
				save_hpmp = Parse::toBool(infile.val);
			// @ATTR corpse_timeout|duration|Duration that a corpse can exist on the map in 'ms' or 's'. Use 0 to keep corpses indefinitely.
			else if (infile.key == "corpse_timeout")
				corpse_timeout = Parse::toDuration(infile.val);
			// @ATTR sell_without_vendor|bool|Allows selling items when not at a vendor via CTRL-Click.
			else if (infile.key == "sell_without_vendor")
				sell_without_vendor = Parse::toBool(infile.val);
			// @ATTR aim_assist|int|The pixel offset for powers that use aim_assist.
			else if (infile.key == "aim_assist")
				aim_assist = Parse::toInt(infile.val);
			// @ATTR window_title|string|Sets the text in the window's titlebar.
			else if (infile.key == "window_title")
				window_title = infile.val;
			// @ATTR save_prefix|string|A string that's prepended to save filenames to prevent conflicts between mods.
			else if (infile.key == "save_prefix")
				save_prefix = infile.val;
			// @ATTR sound_falloff|int|The maximum radius in tiles that any single sound is audible.
			else if (infile.key == "sound_falloff")
				sound_falloff = Parse::toInt(infile.val);
			// @ATTR party_exp_percentage|float|The percentage of XP given to allies.
			else if (infile.key == "party_exp_percentage")
				party_exp_percentage = Parse::toFloat(infile.val);
			// @ATTR enable_ally_collision|bool|Allows allies to block the player's path.
			else if (infile.key == "enable_ally_collision")
				enable_ally_collision = Parse::toBool(infile.val);
			// @ATTR enable_ally_collision_ai|bool|Allows allies to block the path of other AI creatures.
			else if (infile.key == "enable_ally_collision_ai")
				enable_ally_collision_ai = Parse::toBool(infile.val);
			else if (infile.key == "currency_id") {
				// @ATTR currency_id|item_id|An item id that will be used as currency.
				currency_id = Parse::toItemID(infile.val);
				if (currency_id < 1) {
					currency_id = 1;
					Utils::logError("EngineSettings: Currency ID below the minimum allowed value. Resetting it to %d", currency_id);
				}
			}
			// @ATTR interact_range|float|Distance where the player can interact with objects and NPCs.
			else if (infile.key == "interact_range")
				interact_range = Parse::toFloat(infile.val);
			// @ATTR menus_pause|bool|Opening any menu will pause the game.
			else if (infile.key == "menus_pause")
				menus_pause = Parse::toBool(infile.val);
			// @ATTR save_onload|bool|Save the game upon changing maps.
			else if (infile.key == "save_onload")
				save_onload = Parse::toBool(infile.val);
			// @ATTR save_onexit|bool|Save the game upon quitting to the title screen or desktop.
			else if (infile.key == "save_onexit")
				save_onexit = Parse::toBool(infile.val);
			// @ATTR save_pos_onexit|bool|If the game gets saved on exiting, store the player's current position instead of the map spawn position.
			else if (infile.key == "save_pos_onexit")
				save_pos_onexit = Parse::toBool(infile.val);
			// @ATTR save_oncutscene|bool|Saves the game when triggering any cutscene via an Event.
			else if (infile.key == "save_oncutscene")
				save_oncutscene = Parse::toBool(infile.val);
			// @ATTR save_onstash|[bool, "private", "shared"]|Saves the game when changing the contents of a stash. The default is true (i.e. save when using both stash types). Use caution with the values "private" and false, since not saving shared stashes exposes an item duplication exploit.
			else if (infile.key == "save_onstash") {
				if (infile.val == "private")
					save_onstash = SAVE_ONSTASH_PRIVATE;
				else if (infile.val == "shared")
					save_onstash = SAVE_ONSTASH_SHARED;
				else {
					if (Parse::toBool(infile.val))
						save_onstash = SAVE_ONSTASH_ALL;
					else
						save_onstash = SAVE_ONSTASH_NONE;
				}
			}
			// @ATTR save_anywhere|bool|Enables saving the game with a button in the pause menu.
			else if (infile.key == "save_anywhere")
				save_anywhere = Parse::toBool(infile.val);
			// @ATTR camera_speed|float|Modifies how fast the camera moves to recenter on the player. Larger values mean a slower camera. Default value is 10.
			else if (infile.key == "camera_speed") {
				camera_speed = Parse::toFloat(infile.val);
				if (camera_speed <= 0)
					camera_speed = 1;
			}
			// @ATTR save_buyback|bool|Saves the vendor buyback stock whenever the game is saved.
			else if (infile.key == "save_buyback")
				save_buyback = Parse::toBool(infile.val);
			// @ATTR keep_buyback_on_map_change|bool|If true, NPC buyback stocks will persist when the map changes. If false, save_buyback is disabled.
			else if (infile.key == "keep_buyback_on_map_change")
				keep_buyback_on_map_change = Parse::toBool(infile.val);
			// @ATTR sfx_unable_to_cast|filename|Sound to play when the player lacks the MP to cast a power.
			else if (infile.key == "sfx_unable_to_cast")
				sfx_unable_to_cast = infile.val;
			// @ATTR combat_aborts_npc_interact|bool|If true, the NPC dialog and vendor menus will be closed if the player is attacked.
			else if (infile.key == "combat_aborts_npc_interact")
				combat_aborts_npc_interact = Parse::toBool(infile.val);
			// @ATTR fogofwar|int|Set the fog of war type. 0-disabled, 1-minimap, 2-tint, 3-overlay.
			else if (infile.key == "fogofwar")
				fogofwar = static_cast<unsigned short>(Parse::toInt(infile.val));
			// @ATTR save_fogofwar|bool|If true, the fog of war layer keeps track of the progress.
			else if (infile.key == "save_fogofwar")
				save_fogofwar = Parse::toBool(infile.val);

			// @ATTR mouse_move_deadzone|float, float : Deadzone while moving, Deadzone while not moving|Adds a deadzone circle around the player to prevent erratic behavior when using mouse movement. Ideally, the deadzone when moving should be less than the deadzone when not moving. Defaults are 0.25 and 0.75 respectively.
			else if (infile.key == "mouse_move_deadzone") {
				mouse_move_deadzone_moving = Parse::popFirstFloat(infile.val);
				mouse_move_deadzone_not_moving = Parse::popFirstFloat(infile.val);
			}

			// @ATTR passive_trigger_effect_stacking|bool|For backwards compatibility. Allows adding multiple of the same effect from a power with the same passive trigger. False by default.
			else if (infile.key == "passive_trigger_effect_stacking") {
				passive_trigger_effect_stacking = Parse::toBool(infile.val);
			}

			else infile.error("EngineSettings: '%s' is not a valid key.", infile.key.c_str());
		}
		infile.close();
	}

	if (save_prefix == "") {
		Utils::logError("EngineSettings: save_prefix not found in engine/misc.txt, setting to 'default'. This may cause save file conflicts between games that have no save_prefix.");
		save_prefix = "default";
	}

	if (save_buyback && !keep_buyback_on_map_change) {
		Utils::logError("EngineSettings: Warning, save_buyback=true is ignored when keep_buyback_on_map_change=false.");
		save_buyback = false;
	}

	if (corpse_timeout <= 0) {
		corpse_timeout_enabled = false;
		corpse_timeout = settings->max_frames_per_sec + 1;
	}
}

void EngineSettings::Resolutions::load() {
	// reset to defaults
	frame_w = 0;
	frame_h = 0;
	icon_size = 0;
	min_screen_w = 640;
	min_screen_h = 480;
	virtual_heights.clear();
	virtual_dpi = 0;
	ignore_texture_filter = false;

	FileParser infile;
	// @CLASS EngineSettings: Resolution|Description of engine/resolutions.txt
	if (infile.open("engine/resolutions.txt", FileParser::MOD_FILE, FileParser::ERROR_NORMAL)) {
		while (infile.next()) {
			// @ATTR menu_frame_width|int|Width of frame for New Game, Configuration, etc. menus.
			if (infile.key == "menu_frame_width")
				frame_w = static_cast<unsigned short>(Parse::toInt(infile.val));
			// @ATTR menu_frame_height|int|Height of frame for New Game, Configuration, etc. menus.
			else if (infile.key == "menu_frame_height")
				frame_h = static_cast<unsigned short>(Parse::toInt(infile.val));
			// @ATTR icon_size|int|Size of icons.
			else if (infile.key == "icon_size")
				icon_size = static_cast<unsigned short>(Parse::toInt(infile.val));
			// @ATTR required_width|int|Minimum window/screen resolution width.
			else if (infile.key == "required_width") {
				min_screen_w = static_cast<unsigned short>(Parse::toInt(infile.val));
			}
			// @ATTR required_height|int|Minimum window/screen resolution height.
			else if (infile.key == "required_height") {
				min_screen_h = static_cast<unsigned short>(Parse::toInt(infile.val));
			}
			// @ATTR virtual_height|list(int)|A list of heights (in pixels) that the game can use for its actual rendering area. The virtual height chosen is based on the current window height. The width will be resized to match the window's aspect ratio, and everything will be scaled up to fill the window.
			else if (infile.key == "virtual_height") {
				virtual_heights.clear();
				std::string v_height = Parse::popFirstString(infile.val);
				while (!v_height.empty()) {
					int test_v_height = Parse::toInt(v_height);
					if (test_v_height <= 0) {
						Utils::logError("EngineSettings: virtual_height must be greater than zero.");
					}
					else {
						virtual_heights.push_back(static_cast<unsigned short>(test_v_height));
					}
					v_height = Parse::popFirstString(infile.val);
				}

				std::sort(virtual_heights.begin(), virtual_heights.end());

				if (!virtual_heights.empty()) {
					settings->view_h = virtual_heights.back();
				}

				settings->view_h_half = settings->view_h / 2;
			}
			// @ATTR virtual_dpi|float|A target diagonal screen DPI used to determine how much to scale the internal render resolution.
			else if (infile.key == "virtual_dpi") {
				virtual_dpi = Parse::toFloat(infile.val);
			}
			// @ATTR ignore_texture_filter|bool|If true, this ignores the "Texture Filtering" video setting and uses only nearest-neighbor scaling. This is good for games that use pixel art assets.
			else if (infile.key == "ignore_texture_filter") {
				ignore_texture_filter = Parse::toBool(infile.val);
			}
			else infile.error("EngineSettings: '%s' is not a valid key.", infile.key.c_str());
		}
		infile.close();
	}

	// prevent the window from being too small
	if (settings->screen_w < min_screen_w) settings->screen_w = min_screen_w;
	if (settings->screen_h < min_screen_h) settings->screen_h = min_screen_h;

	// icon size can not be zero, so we set a default of 32x32, which is fantasycore's icon size
	if (icon_size == 0) {
		Utils::logError("EngineSettings: icon_size is undefined. Setting it to 32.");
		icon_size = 32;
	}
}

void EngineSettings::Gameplay::load() {
	// reset to defaults
	enable_playgame = false;

	FileParser infile;
	// @CLASS EngineSettings: Gameplay|Description of engine/gameplay.txt
	if (infile.open("engine/gameplay.txt", FileParser::MOD_FILE, FileParser::ERROR_NORMAL)) {
		while (infile.next()) {
			if (infile.key == "enable_playgame") {
				// @ATTR enable_playgame|bool|Enables the "Play Game" button on the main menu.
				enable_playgame = Parse::toBool(infile.val);
			}
			else infile.error("EngineSettings: '%s' is not a valid key.", infile.key.c_str());
		}
		infile.close();
	}
}

void EngineSettings::Combat::load() {
	// reset to defaults
	min_absorb = 0;
	max_absorb = 100;
	min_resist = 0;
	max_resist = 100;
	min_block = 0;
	max_block = 100;
	min_avoidance = 0;
	max_avoidance = 100;
	min_miss_damage = 0;
	max_miss_damage = 0;
	min_crit_damage = 200;
	max_crit_damage = 200;
	min_overhit_damage = 100;
	max_overhit_damage = 100;
	resource_round_method = EngineSettings::Combat::RESOURCE_ROUND_METHOD_ROUND;
	offscreen_enemy_encounters = false;

	FileParser infile;
	// @CLASS EngineSettings: Combat|Description of engine/combat.txt
	if (infile.open("engine/combat.txt", FileParser::MOD_FILE, FileParser::ERROR_NORMAL)) {
		while (infile.next()) {
			if (infile.key == "absorb_percent") {
				// @ATTR absorb_percent|float, float : Minimum, Maximum|Limits the percentage of damage that can be absorbed. A max value less than 100 will ensure that the target always takes at least 1 damage from non-elemental attacks.
				min_absorb = Parse::popFirstFloat(infile.val);
				max_absorb = Parse::popFirstFloat(infile.val);
				max_absorb = std::max(max_absorb, min_absorb);
			}
			else if (infile.key == "resist_percent") {
				// @ATTR resist_percent|float, float : Minimum, Maximum|Limits the percentage of damage that can be resisted. A max value less than 100 will ensure that the target always takes at least 1 damage from elemental attacks.
				min_resist = Parse::popFirstFloat(infile.val);
				max_resist = Parse::popFirstFloat(infile.val);
				max_resist = std::max(max_resist, min_resist);
			}
			else if (infile.key == "block_percent") {
				// @ATTR block_percent|float, float : Minimum, Maximum|Limits the percentage of damage that can be absorbed when the target is in the 'block' animation state. A max value less than 100 will ensure that the target always takes at least 1 damage from non-elemental attacks.
				min_block = Parse::popFirstFloat(infile.val);
				max_block = Parse::popFirstFloat(infile.val);
				max_block = std::max(max_block, min_block);
			}
			else if (infile.key == "avoidance_percent") {
				// @ATTR avoidance_percent|float, float : Minimum, Maximum|Limits the percentage chance that damage will be avoided.
				min_avoidance = Parse::popFirstFloat(infile.val);
				max_avoidance = Parse::popFirstFloat(infile.val);
				max_avoidance = std::max(max_avoidance, min_avoidance);
			}
			// @ATTR miss_damage_percent|float, float : Minimum, Maximum|The percentage of damage dealt when a miss occurs.
			else if (infile.key == "miss_damage_percent") {
				min_miss_damage = Parse::popFirstFloat(infile.val);
				max_miss_damage = Parse::popFirstFloat(infile.val);
				max_miss_damage = std::max(max_miss_damage, min_miss_damage);
			}
			// @ATTR crit_damage_percent|float, float : Minimum, Maximum|The percentage of damage dealt when a critical hit occurs.
			else if (infile.key == "crit_damage_percent") {
				min_crit_damage = Parse::popFirstFloat(infile.val);
				max_crit_damage = Parse::popFirstFloat(infile.val);
				max_crit_damage = std::max(max_crit_damage, min_crit_damage);
			}
			// @ATTR overhit_damage_percent|float, float : Minimum, Maximum|The percentage of damage dealt when an overhit occurs.
			else if (infile.key == "overhit_damage_percent") {
				min_overhit_damage = Parse::popFirstFloat(infile.val);
				max_overhit_damage = Parse::popFirstFloat(infile.val);
				max_overhit_damage = std::max(max_overhit_damage, min_overhit_damage);
			}
			// @ATTR resource_round_method|['none', 'round', 'floor', 'ceil']|Rounds the numbers for most combat events that affect HP/MP. For example: damage taken, HP healed, MP consumed. Defaults to 'round'.
			else if (infile.key == "resource_round_method") {
				if (infile.val == "none")
					resource_round_method = EngineSettings::Combat::RESOURCE_ROUND_METHOD_NONE;
				else if (infile.val == "round")
					resource_round_method = EngineSettings::Combat::RESOURCE_ROUND_METHOD_ROUND;
				else if (infile.val == "floor")
					resource_round_method = EngineSettings::Combat::RESOURCE_ROUND_METHOD_FLOOR;
				else if (infile.val == "ceil")
					resource_round_method = EngineSettings::Combat::RESOURCE_ROUND_METHOD_CEIL;
				else
					infile.error("EngineSettings: '%s' is not a valid resource rounding method.", infile.val.c_str());
			}
			// @ATTR offscreen_enemy_encounters|bool|If true, enemies can enter combat even if they are off-screen. Defaults to false.
			else if (infile.key == "offscreen_enemy_encounters") {
				offscreen_enemy_encounters = Parse::toBool(infile.val);
			}

			else infile.error("EngineSettings: '%s' is not a valid key.", infile.key.c_str());
		}
		infile.close();
	}
}

float EngineSettings::Combat::resourceRound(const float resource_val) {
	if (resource_round_method == EngineSettings::Combat::RESOURCE_ROUND_METHOD_ROUND) {
		return roundf(resource_val);
	}
	else if (resource_round_method == EngineSettings::Combat::RESOURCE_ROUND_METHOD_FLOOR) {
		return floorf(resource_val);
	}
	else if (resource_round_method == EngineSettings::Combat::RESOURCE_ROUND_METHOD_CEIL) {
		return ceilf(resource_val);
	}
	else {
		// EngineSettings::Combat::RESOURCE_ROUND_METHOD_NONE
		return resource_val;
	}
}

void EngineSettings::EquipFlags::load() {
	// reset to defaults
	list.clear();

	EquipFlag temp;
	EquipFlag* current = &temp;

	FileParser infile;
	// @CLASS EngineSettings: Equip flags|Description of engine/equip_flags.txt
	if (infile.open("engine/equip_flags.txt", FileParser::MOD_FILE, FileParser::ERROR_NORMAL)) {
		while (infile.next()) {
			if (infile.new_section) {
				if (infile.section == "flag") {
					temp = EquipFlag();
					current = &temp;
				}
			}

			if (infile.section != "flag")
				continue;

			// if we want to replace a list item by ID, the ID needs to be parsed first
			// but it is not essential if we're just adding to the list, so this is simply a warning
			if (infile.key != "id" && current->id.empty()) {
				infile.error("EngineSettings: Expected 'id', but found '%s'.", infile.key.c_str());
			}

			if (infile.key == "id") {
				// @ATTR flag.id|string|An identifier for this equip flag.
				bool found_id = false;
				for (size_t i = 0; i < list.size(); ++i) {
					if (list[i].id == infile.val) {
						current = &list[i];
						found_id = true;
					}
				}
				if (!found_id) {
					list.push_back(temp);
					current = &(list.back());
					current->id = infile.val;
				}
			}
			else if (infile.key == "name") {
				// @ATTR flag.name|string|The displayed name of this equip flag.
				current->name = infile.val;
			}

			else infile.error("EngineSettings: '%s' is not a valid key.", infile.key.c_str());
		}
		infile.close();
	}
}

void EngineSettings::PrimaryStats::load() {
	// reset to defaults
	list.clear();

	PrimaryStat temp;
	PrimaryStat* current = &temp;

	FileParser infile;
	// @CLASS EngineSettings: Primary Stats|Description of engine/primary_stats.txt
	if (infile.open("engine/primary_stats.txt", FileParser::MOD_FILE, FileParser::ERROR_NORMAL)) {
		while (infile.next()) {
			if (infile.new_section) {
				if (infile.section == "stat") {
					temp = PrimaryStat();
					current = &temp;
				}
			}

			if (infile.section != "stat")
				continue;

			// if we want to replace a list item by ID, the ID needs to be parsed first
			// but it is not essential if we're just adding to the list, so this is simply a warning
			if (infile.key != "id" && current->id.empty()) {
				infile.error("EngineSettings: Expected 'id', but found '%s'.", infile.key.c_str());
			}

			if (infile.key == "id") {
				// @ATTR stat.id|string|An identifier for this primary stat.
				bool found_id = false;
				for (size_t i = 0; i < list.size(); ++i) {
					if (list[i].id == infile.val) {
						current = &list[i];
						found_id = true;
					}
				}
				if (!found_id) {
					list.push_back(temp);
					current = &(list.back());
					current->id = infile.val;
				}
			}
			else if (infile.key == "name") {
				// @ATTR stat.name|string|The displayed name of this primary stat.
				current->name = msg->get(infile.val);
			}

			else infile.error("EngineSettings: '%s' is not a valid key.", infile.key.c_str());
		}
		infile.close();
	}
}

size_t EngineSettings::PrimaryStats::getIndexByID(const std::string& id) {
	for (size_t i = 0; i < list.size(); ++i) {
		if (id == list[i].id)
			return i;
	}

	return list.size();
}

EngineSettings::HeroClasses::HeroClass::HeroClass()
	: name("")
	, description("")
	, currency(0)
	, equipment("")
	, carried("")
	, primary((eset ? eset->primary_stats.list.size() : 0), 0)
	, hotkeys(std::vector<PowerID>(MenuActionBar::SLOT_MAX, 0))
	, power_tree("")
	, default_power_tab(-1)
{
}

void EngineSettings::HeroClasses::load() {
	// reset to defaults
	list.clear();

	HeroClass temp;
	HeroClass* current = &temp;

	FileParser infile;
	// @CLASS EngineSettings: Classes|Description of engine/classes.txt
	if (infile.open("engine/classes.txt", FileParser::MOD_FILE, FileParser::ERROR_NORMAL)) {
		while (infile.next()) {
			if (infile.new_section) {
				if (infile.section == "class") {
					temp = HeroClass();
					current = &temp;
				}
			}

			if (infile.section != "class")
				continue;

			// if we want to replace a list item by ID (HeroClass uses 'name' as its ID), the ID needs to be parsed first
			// but it is not essential if we're just adding to the list, so this is simply a warning
			if (infile.key != "name" && current->name.empty()) {
				infile.error("EngineSettings: Expected 'name', but found '%s'.", infile.key.c_str());
			}

			if (infile.key == "name") {
				// @ATTR name|string|The displayed name of this class.
				bool found_id = false;
				for (size_t i = 0; i < list.size(); ++i) {
					if (list[i].name == infile.val) {
						current = &list[i];
						found_id = true;
					}
				}
				if (!found_id) {
					list.push_back(temp);
					current = &(list.back());
					current->name = infile.val;
				}
			}
			else if (infile.key == "description") {
				// @ATTR description|string|A description of this class.
				current->description = infile.val;
			}
			else if (infile.key == "currency") {
				// @ATTR currency|int|The amount of currency this class will start with.
				current->currency = Parse::toInt(infile.val);
			}
			else if (infile.key == "equipment") {
				// @ATTR equipment|list(item_id)|A list of items that are equipped when starting with this class.
				current->equipment = infile.val;
			}
			else if (infile.key == "carried") {
				// @ATTR carried|list(item_id)|A list of items that are placed in the normal inventorty when starting with this class.
				current->carried = infile.val;
			}

			else if (infile.key == "primary") {
				// @ATTR primary|predefined_string, int : Primary stat name, Default value|Class starts with this value for the specified stat.
				current->primary.clear();
				current->primary.resize(eset->primary_stats.list.size(), 0);

				std::string prim_stat = Parse::popFirstString(infile.val);
				size_t prim_stat_index = eset->primary_stats.getIndexByID(prim_stat);

				if (prim_stat_index != eset->primary_stats.list.size()) {
					current->primary[prim_stat_index] = Parse::toInt(infile.val);
				}
				else {
					infile.error("EngineSettings: '%s' is not a valid primary stat.", prim_stat.c_str());
				}
			}
			else if (infile.key == "actionbar") {
				// @ATTR actionbar|list(power_id)|A list of powers to place in the action bar for the class.
				current->hotkeys.clear();
				current->hotkeys.resize(MenuActionBar::SLOT_MAX, 0);

				for (int i=0; i<12; i++) {
					current->hotkeys[i] = Parse::toPowerID(Parse::popFirstString(infile.val));
				}
			}
			else if (infile.key == "powers") {
				// @ATTR powers|list(power_id)|A list of powers that are unlocked when starting this class.
				current->powers.clear();

				std::string power;
				while ( (power = Parse::popFirstString(infile.val)) != "") {
					current->powers.push_back(Parse::toPowerID(power));
				}
			}
			else if (infile.key == "campaign") {
				// @ATTR campaign|list(string)|A list of campaign statuses that are set when starting this class.
				current->statuses.clear();

				std::string status;
				while ( (status = Parse::popFirstString(infile.val)) != "") {
					current->statuses.push_back(status);
				}
			}
			else if (infile.key == "power_tree") {
				// @ATTR power_tree|string|Power tree that will be loaded by MenuPowers
				current->power_tree = infile.val;
			}
			else if (infile.key == "hero_options") {
				// @ATTR hero_options|list(int)|A list of indicies of the hero options this class can use.
				current->options.clear();

				std::string hero_option;
				while ( (hero_option = Parse::popFirstString(infile.val)) != "") {
					current->options.push_back(Parse::toInt(hero_option));
				}

				std::sort(current->options.begin(), current->options.end());
			}
			else if (infile.key == "default_power_tab") {
				// @ATTR default_power_tab|int|Index of the tab to switch to when opening the Powers menu
				current->default_power_tab = Parse::toInt(infile.val);
			}

			else infile.error("EngineSettings: '%s' is not a valid key.", infile.key.c_str());
		}
		infile.close();
	}

	// Make a default hero class if none were found
	if (list.empty()) {
		HeroClass c;
		c.name = "Adventurer";
		msg->get("Adventurer"); // this is needed for translation
		list.push_back(c);
	}
}

EngineSettings::HeroClasses::HeroClass* EngineSettings::HeroClasses::getByName(const std::string& name) {
	if (name.empty())
		return NULL;

	for (size_t i = 0; i < list.size(); ++i) {
		if (name == list[i].name) {
			return &list[i];
		}
	}

	return NULL;
}

EngineSettings::DamageTypes::DamageType::DamageType()
	: is_elemental(false)
	, is_deprecated_element(false)
{}

void EngineSettings::DamageTypes::load() {
	// reset to defaults
	list.clear();
	count = 0;

	std::stringstream ss;

	DamageType temp;
	DamageType* current = &temp;

	FileParser infile;
	// @CLASS EngineSettings: Damage Types|Description of engine/damage_types.txt
	if (infile.open("engine/damage_types.txt", FileParser::MOD_FILE, FileParser::ERROR_NORMAL)) {
		while (infile.next()) {
			if (infile.new_section) {
				if (infile.section == "damage_type") {
					temp = DamageType();
					current = &temp;
				}
			}

			if (infile.section != "damage_type")
				continue;

			// if we want to replace a list item by ID, the ID needs to be parsed first
			// but it is not essential if we're just adding to the list, so this is simply a warning
			if (infile.key != "id" && current->id.empty()) {
				infile.error("EngineSettings: Expected 'id', but found '%s'.", infile.key.c_str());
			}

			if (infile.key == "id") {
				// @ATTR damage_type.id|string|The identifier used for Item damage_type and Power base_damage.
				bool found_id = false;
				for (size_t i = 0; i < list.size(); ++i) {
					if (list[i].id == infile.val) {
						current = &list[i];
						found_id = true;
					}
				}
				if (!found_id) {
					list.push_back(temp);
					current = &(list.back());
					current->id = infile.val;
				}
			}
			// @ATTR damage_type.name|string|The displayed name for the value of this damage type.
			else if (infile.key == "name") current->name = msg->get(infile.val);
			// @ATTR damage_type.name_short|string|An optional, shorter displayed name for this damage type. Used for labels for 'Resist Damage' stats. If left blank, 'name' will be used instead.
			else if (infile.key == "name_short") current->name_short = msg->get(infile.val);
			// @ATTR damage_type.description|string|The description that will be displayed in the Character menu tooltips.
			else if (infile.key == "description") current->description = msg->get(infile.val);
			// @ATTR damage_type.min|string|The identifier used as a Stat type and an Effect type, for the minimum damage of this type.
			else if (infile.key == "min") current->min = infile.val;
			// @ATTR damage_type.max|string|The identifier used as a Stat type and an Effect type, for the maximum damage of this type.
			else if (infile.key == "max") current->max = infile.val;
			// @ATTR damage_type.elemental|bool|If true, this damage type will be flagged as elemental. Elemental damage will be additionally applied to Powers with non-elemental base damage.
			else if (infile.key == "elemental") current->is_elemental = Parse::toBool(infile.val);

			else infile.error("EngineSettings: '%s' is not a valid key.", infile.key.c_str());
		}
		infile.close();
	}

	temp = DamageType();
	current = &temp;

	// For backwards-compatibility, load engine/elements.txt as damage types
	// @CLASS EngineSettings: Elements|(Deprecated in v1.14.85, use engine/damage_types.txt instead) Description of engine/elements.txt
	if (infile.open("engine/elements.txt", FileParser::MOD_FILE, FileParser::ERROR_NORMAL)) {
		while (infile.next()) {
			if (infile.new_section) {
				if (infile.section == "element") {
					temp = DamageType();
					current = &temp;

					current->is_elemental = true;
					current->is_deprecated_element = true;
				}
			}

			if (infile.section != "element")
				continue;

			// if we want to replace a list item by ID, the ID needs to be parsed first
			// but it is not essential if we're just adding to the list, so this is simply a warning
			if (infile.key != "id" && current->id.empty()) {
				infile.error("EngineSettings: Expected 'id', but found '%s'.", infile.key.c_str());
			}

			if (infile.key == "id") {
				// @ATTR element.id|string|An identifier for this element. When used as a resistance, "_resist" is appended to the id. For example, if the id is "fire", the resist id is "fire_resist".
				bool found_id = false;
				for (size_t i = 0; i < list.size(); ++i) {
					if (list[i].id == infile.val) {
						current = &list[i];
						found_id = true;
					}
				}
				if (!found_id) {
					list.push_back(temp);
					current = &(list.back());
					current->id = infile.val;
				}
			}
			// @ATTR element.name|string|The displayed name of this element.
			else if (infile.key == "name") current->name = msg->get(infile.val);

			else infile.error("EngineSettings: '%s' is not a valid key.", infile.key.c_str());
		}
		infile.close();
	}

	// Total stat count. Each damage type has 3: min, max, resist
	count = list.size() * 3;

	for (size_t i = 0; i < list.size(); ++i) {
		// create missing IDs from the base ID if needed
		if (list[i].min.empty()) {
			list[i].min = "dmg_" + list[i].id + "_min";
		}
		if (list[i].max.empty()) {
			list[i].max = "dmg_" + list[i].id + "_max";
		}
		if (list[i].resist.empty()) {
			list[i].resist = list[i].id + "_resist";
		}

		// use the IDs if the damage type doesn't have printable names
		if (list[i].name.empty()) {
			list[i].name = list[i].id;
		}
		if (list[i].name_short.empty()) {
			list[i].name_short = list[i].name;
		}
		if (list[i].name_min.empty()) {
			list[i].name_min = msg->getv("%s (Min.)", list[i].name.c_str());
		}
		if (list[i].name_max.empty()) {
			list[i].name_max = msg->getv("%s (Max.)", list[i].name.c_str());
		}
		if (list[i].name_resist.empty()) {
			list[i].name_resist = msg->getv("Resist Damage (%s)", list[i].name_short.c_str());
		}
	}
}

size_t EngineSettings::DamageTypes::indexToMin(size_t list_index) {
	return list_index * 3;
}

size_t EngineSettings::DamageTypes::indexToMax(size_t list_index) {
	return (list_index * 3) + 1;
}

size_t EngineSettings::DamageTypes::indexToResist(size_t list_index) {
	return (list_index * 3) + 2;
}

void EngineSettings::DeathPenalty::load() {
	// reset to defaults
	enabled = true;
	permadeath = false;
	currency = 50;
	xp = 0;
	xp_current = 0;
	item = false;

	FileParser infile;
	// @CLASS EngineSettings: Death penalty|Description of engine/death_penalty.txt
	if (infile.open("engine/death_penalty.txt", FileParser::MOD_FILE, FileParser::ERROR_NORMAL)) {
		while (infile.next()) {
			// @ATTR enable|bool|Enable the death penalty.
			if (infile.key == "enable") enabled = Parse::toBool(infile.val);
			// @ATTR permadeath|bool|Force permadeath for all new saves.
			else if (infile.key == "permadeath") permadeath = Parse::toBool(infile.val);
			// @ATTR currency|float|Remove this percentage of currency.
			else if (infile.key == "currency") currency = Parse::toFloat(infile.val);
			// @ATTR xp_total|float|Remove this percentage of total XP.
			else if (infile.key == "xp_total") xp = Parse::toFloat(infile.val);
			// @ATTR xp_current_level|float|Remove this percentage of the XP gained since the last level.
			else if (infile.key == "xp_current_level") xp_current = Parse::toFloat(infile.val);
			// @ATTR random_item|bool|Removes a random item from the player's inventory.
			else if (infile.key == "random_item") item = Parse::toBool(infile.val);

			else infile.error("EngineSettings: '%s' is not a valid key.", infile.key.c_str());
		}
		infile.close();
	}

}

void EngineSettings::Tooltips::load() {
	// reset to defaults
	offset = 0;
	width = 1;
	margin = 0;
	margin_npc = 0;
	background_border = 0;
	visible_max = 3;

	FileParser infile;
	// @CLASS EngineSettings: Tooltips|Description of engine/tooltips.txt
	if (infile.open("engine/tooltips.txt", FileParser::MOD_FILE, FileParser::ERROR_NORMAL)) {
		while (infile.next()) {
			// @ATTR tooltip_offset|int|Offset in pixels from the origin point (usually mouse cursor).
			if (infile.key == "tooltip_offset")
				offset = Parse::toInt(infile.val);
			// @ATTR tooltip_width|int|Maximum width of tooltip in pixels.
			else if (infile.key == "tooltip_width")
				width = Parse::toInt(infile.val);
			// @ATTR tooltip_margin|int|Padding between the text and the tooltip borders.
			else if (infile.key == "tooltip_margin")
				margin = Parse::toInt(infile.val);
			// @ATTR npc_tooltip_margin|int|Vertical offset for NPC labels.
			else if (infile.key == "npc_tooltip_margin")
				margin_npc = Parse::toInt(infile.val);
			// @ATTR tooltip_background_border|int|The pixel size of the border in "images/menus/tooltips.png".
			else if (infile.key == "tooltip_background_border")
				background_border = Parse::toInt(infile.val);
			// @ATTR tooltip_visible_max|int|The maximum number of floating tooltips on screen at once. Defaults to 3.
			else if (infile.key == "tooltip_visible_max") {
				visible_max = static_cast<size_t>(Parse::toInt(infile.val));

				if (visible_max < 1) {
					visible_max = 1;
					infile.error("EngineSettings: tooltip_visible_max must be greater than or equal to 1.");
				}
			}

			else infile.error("EngineSettings: '%s' is not a valid key.", infile.key.c_str());
		}
		infile.close();
	}
}

void EngineSettings::Loot::load() {
	// reset to defaults
	tooltip_margin = 0;
	autopickup_currency = false;
	autopickup_range = eset->misc.interact_range;
	currency = "Gold";
	vendor_ratio_buy = 1.0f;
	vendor_ratio_sell = 0.25f;
	vendor_ratio_sell_old = 0;
	sfx_loot = "";
	drop_max = 1;
	drop_radius = 1;
	hide_radius = 3.0f;
	extended_items_offset = 0;

	FileParser infile;
	// @CLASS EngineSettings: Loot|Description of engine/loot.txt
	if (infile.open("engine/loot.txt", FileParser::MOD_FILE, FileParser::ERROR_NORMAL)) {
		while (infile.next()) {
			if (infile.key == "tooltip_margin") {
				// @ATTR tooltip_margin|int|Vertical offset of the loot tooltip from the loot itself.
				tooltip_margin = Parse::toInt(infile.val);
			}
			else if (infile.key == "autopickup_currency") {
				// @ATTR autopickup_currency|bool|Enable autopickup for currency
				autopickup_currency = Parse::toBool(infile.val);
			}
			else if (infile.key == "autopickup_range") {
				// @ATTR autopickup_range|float|Minimum distance the player must be from loot to trigger autopickup.
				autopickup_range = Parse::toFloat(infile.val);
			}
			else if (infile.key == "currency_name") {
				// @ATTR currency_name|string|Define the name of currency in game
				currency = msg->get(infile.val);
			}
			else if (infile.key == "vendor_ratio_buy") {
				// @ATTR vendor_ratio_buy|float|Global multiplier for item prices on the "Buy" tab of vendors. Defaults to 1.0.
				vendor_ratio_buy = Parse::toFloat(infile.val);
			}
			else if (infile.key == "vendor_ratio_sell") {
				// @ATTR vendor_ratio_sell|float|Global multiplier for the currency gained when selling an item and the price of items on the "Sell" tab of vendors. Defaults to 0.25.
				vendor_ratio_sell = Parse::toFloat(infile.val);
			}
			else if (infile.key == "vendor_ratio_sell_old") {
				// @ATTR vendor_ratio_sell_old|float|Global multiplier for item prices on the "Sell" tab of vendors after the player has left the map or quit the game. Falls back to the value of vendor_ratio_sell by default.
				vendor_ratio_sell_old = Parse::toFloat(infile.val);
			}
			else if (infile.key == "sfx_loot") {
				// @ATTR sfx_loot|filename|Filename of a sound effect to play for dropping loot.
				sfx_loot = infile.val;
			}
			else if (infile.key == "drop_max") {
				// @ATTR drop_max|int|The maximum number of random item stacks that can drop at once
				drop_max = std::max(Parse::toInt(infile.val), 1);
			}
			else if (infile.key == "drop_radius") {
				// @ATTR drop_radius|int|The distance (in tiles) away from the origin that loot can drop
				drop_radius = std::max(Parse::toInt(infile.val), 1);
			}
			else if (infile.key == "hide_radius") {
				// @ATTR hide_radius|float|If an entity is within this radius relative to a piece of loot, the label will be hidden unless highlighted with the cursor.
				hide_radius = Parse::toFloat(infile.val);
			}
			else if (infile.key == "vendor_ratio") {
				// @ATTR vendor_ratio|int|(Deprecated in v1.12.85; use 'vendor_ratio_sell' instead) Percentage of item buying price to use as selling price. Also used as the buyback price until the player leaves the map.
				vendor_ratio_sell = static_cast<float>(Parse::toInt(infile.val)) / 100.0f;
				infile.error("EngineSettings: vendor_ratio is deprecated. Use 'vendor_ratio_sell=%.2f' instead.", vendor_ratio_sell);
			}
			else if (infile.key == "vendor_ratio_buyback") {
				// @ATTR vendor_ratio_buyback|int|(Deprecated in v1.12.85; use 'vendor_ratio_sell_old' instead) Percentage of item buying price to use as the buying price for previously sold items.
				vendor_ratio_sell_old = static_cast<float>(Parse::toInt(infile.val)) / 100.0f;
				infile.error("EngineSettings: vendor_ratio_buyback is deprecated. Use 'vendor_ratio_sell_old=%.2f' instead.", vendor_ratio_sell_old);
			}
			else if (infile.key == "extended_items_offset") {
				// @ATTR extended_items_offset|item_id|Sets the starting item ID that extended items will be stored at. The default value, 0, will place extended items at the directly after the last item ID defined in items/items.txt.
				extended_items_offset = Parse::toItemID(infile.val);
			}
			else {
				infile.error("EngineSettings: '%s' is not a valid key.", infile.key.c_str());
			}
		}
		infile.close();
	}
}

void EngineSettings::Tileset::load() {
	// reset to defaults
	units_per_pixel_x = 2;
	units_per_pixel_y = 4;
	tile_w = 64;
	tile_h = 32;
	tile_w_half = tile_w/2;
	tile_h_half = tile_h/2;
	orientation = TILESET_ISOMETRIC;

	FileParser infile;
	// @CLASS EngineSettings: Tileset config|Description of engine/tileset_config.txt
	if (infile.open("engine/tileset_config.txt", FileParser::MOD_FILE, FileParser::ERROR_NORMAL)) {
		while (infile.next()) {
			if (infile.key == "tile_size") {
				// @ATTR tile_size|int, int : Width, Height|The width and height of a tile.
				tile_w = static_cast<unsigned short>(Parse::popFirstInt(infile.val));
				tile_h = static_cast<unsigned short>(Parse::popFirstInt(infile.val));
				tile_w_half = tile_w /2;
				tile_h_half = tile_h /2;
			}
			else if (infile.key == "orientation") {
				// @ATTR orientation|["isometric", "orthogonal"]|The perspective of tiles; isometric or orthogonal.
				if (infile.val == "isometric")
					orientation = TILESET_ISOMETRIC;
				else if (infile.val == "orthogonal")
					orientation = TILESET_ORTHOGONAL;
			}
			else {
				infile.error("EngineSettings: '%s' is not a valid key.", infile.key.c_str());
			}
		}
		infile.close();
	}
	else {
		Utils::logError("Unable to open engine/tileset_config.txt! Defaulting to 64x32 isometric tiles.");
	}

	// Init automatically calculated parameters
	if (orientation == TILESET_ISOMETRIC) {
		if (tile_w > 0 && tile_h > 0) {
			units_per_pixel_x = 2.0f / tile_w;
			units_per_pixel_y = 2.0f / tile_h;
		}
		else {
			Utils::logError("EngineSettings: Tile dimensions must be greater than 0. Resetting to the default size of 64x32.");
			tile_w = 64;
			tile_h = 32;
		}
	}
	else { // TILESET_ORTHOGONAL
		if (tile_w > 0 && tile_h > 0) {
			units_per_pixel_x = 1.0f / tile_w;
			units_per_pixel_y = 1.0f / tile_h;
		}
		else {
			Utils::logError("EngineSettings: Tile dimensions must be greater than 0. Resetting to the default size of 64x32.");
			tile_w = 64;
			tile_h = 32;
		}
	}
	if (units_per_pixel_x == 0 || units_per_pixel_y == 0) {
		Utils::logError("EngineSettings: One of UNITS_PER_PIXEL values is zero! %dx%d", static_cast<int>(units_per_pixel_x), static_cast<int>(units_per_pixel_y));
		Utils::logErrorDialog("EngineSettings: One of UNITS_PER_PIXEL values is zero! %dx%d", static_cast<int>(units_per_pixel_x), static_cast<int>(units_per_pixel_y));
		mods->resetModConfig();
		Utils::Exit(1);
	}
};

void EngineSettings::Widgets::load() {
	// reset to defaults
	selection_rect_color = Color(255, 248, 220, 255);
	selection_rect_corner_size = 4;
	colorblind_highlight_offset = Point(2, 2);

	tab_padding = Point(8, 0);
	tab_text_padding = 0;

	slot_quantity_label = LabelInfo();
	slot_quantity_color = font->getColor(FontEngine::COLOR_WIDGET_NORMAL);
	slot_quantity_bg_color = Color(0, 0, 0, 0);
	slot_hotkey_label = LabelInfo();
	slot_hotkey_color = font->getColor(FontEngine::COLOR_WIDGET_NORMAL);
	slot_hotkey_label.hidden = true;
	slot_hotkey_bg_color = Color(0, 0, 0, 0);

	listbox_text_margin = Point(8, 8);

	horizontal_list_text_width = 150;

	scrollbar_bg_color = Color(0,0,0,64);

	log_padding = 4;

	sound_activate = "";

	FileParser infile;
	// @CLASS EngineSettings: Widgets|Description of engine/widget_settings.txt
	if (infile.open("engine/widget_settings.txt", FileParser::MOD_FILE, FileParser::ERROR_NONE)) {
		while (infile.next()) {
			if (infile.section == "misc") {
				if (infile.key == "selection_rect_color") {
					// @ATTR misc.selection_rect_color|color, int : Color, Alpha|Color of the selection rectangle when navigating widgets without a mouse.
					selection_rect_color = Parse::toRGBA(infile.val);
				}
				else if (infile.key == "selection_rect_corner_size") {
					// @ATTR misc.selection_rect_corner_size|int|Size of the corners on the selection rectangle shown when navigating widgets. Set to 0 to drawn the entire rectangle instead.
					selection_rect_corner_size = Parse::toInt(infile.val);
				}
				else if (infile.key == "colorblind_highlight_offset") {
					// @ATTR misc.colorblind_highlight_offset|int, int : X offset, Y offset|The pixel offset of the '*' marker on highlighted icons in colorblind mode.
					colorblind_highlight_offset = Parse::toPoint(infile.val);
				}
			}
			else if (infile.section == "tab") {
				if (infile.key == "padding") {
					// @ATTR tab.padding|int, int : Left/right padding, Top padding|The pixel padding around tabs. Controls how the left and right edges are drawn.
					tab_padding = Parse::toPoint(infile.val);
				}
				else if (infile.key == "text_padding") {
					// @ATTR tab.text_padding|int : Left/right padding|The padding (in pixels) on the left and right sides of the tab's text label. In contrast to 'padding', this does not affect how the tab background is drawn.
					tab_text_padding = Parse::toInt(infile.val);
				}
			}
			else if (infile.section == "slot") {
				if (infile.key == "quantity_label") {
					// @ATTR slot.quantity_label|label|Setting for the slot quantity text.
					slot_quantity_label = Parse::popLabelInfo(infile.val);
				}
				else if (infile.key == "quantity_color") {
					// @ATTR slot.quantity_color|color|Text color for the slot quantity text.
					slot_quantity_color = Parse::toRGB(infile.val);
				}
				else if (infile.key == "quantity_bg_color") {
					// @ATTR slot.quantity_bg_color|color, int : Color, Alpha|If a slot has a quantity, a rectangle filled with this color will be placed beneath the text.
					slot_quantity_bg_color = Parse::toRGBA(infile.val);
				}
				else if (infile.key == "hotkey_label") {
					// @ATTR slot.hotkey_label|label|Setting for the slot hotkey text.
					slot_hotkey_label = Parse::popLabelInfo(infile.val);
				}
				else if (infile.key == "hotkey_color") {
					// @ATTR slot.hotkey_color|color|Text color for the slot hotkey text.
					slot_hotkey_color = Parse::toRGB(infile.val);
				}
				else if (infile.key == "hotkey_bg_color") {
					// @ATTR slot.hotkey_bg_color|color, int : Color, Alpha|If a slot has a hotkey, a rectangle filled with this color will be placed beneath the text.
					slot_hotkey_bg_color = Parse::toRGBA(infile.val);
				}
			}
			else if (infile.section == "listbox") {
				if (infile.key == "text_margin") {
					// @ATTR listbox.text_margin|int, int : Left margin, Right margin|The pixel margin to leave on the left and right sides of listbox element text.
					listbox_text_margin = Parse::toPoint(infile.val);
				}
			}
			else if (infile.section == "horizontal_list") {
				if (infile.key == "text_width") {
					// @ATTR horizontal_list.text_width|int|The pixel width of the text area that displays the currently selected item. Default is 150 pixels;
					horizontal_list_text_width = Parse::toInt(infile.val);
				}
			}
			else if (infile.section == "scrollbar") {
				if (infile.key == "bg_color") {
					// @ATTR scrollbar.bg_color|color, int : Color, Alpha|The background color for the entire scrollbar.
					scrollbar_bg_color = Parse::toRGBA(infile.val);
				}
			}
			else if (infile.section == "log") {
				if (infile.key == "padding") {
					// @ATTR log.padding|int|The padding in pixels of the log text area.
					log_padding = Parse::toInt(infile.val);
				}
			}
			else if (infile.section == "sound") {
				// @ATTR sound.activate|filename|The sound effect file to play when certain widgets are activated/clicked.
				sound_activate = infile.val;
			}
		}
	}
}

void EngineSettings::XPTable::load() {
	xp_table.clear();

	FileParser infile;
	// @CLASS EngineSettings: XP table|Description of engine/xp_table.txt
	if (infile.open("engine/xp_table.txt", FileParser::MOD_FILE, FileParser::ERROR_NORMAL)) {
		while(infile.next()) {
			if (infile.key == "level") {
				// @ATTR level|int, int : Level, XP|The amount of XP required for this level.
				unsigned lvl_id = Parse::popFirstInt(infile.val);
				unsigned long lvl_xp = Parse::toUnsignedLong(Parse::popFirstString(infile.val));

				if (lvl_id > xp_table.size())
					xp_table.resize(lvl_id);

				xp_table[lvl_id - 1] = lvl_xp;

				// validate that XP is greater than previous levels
				// corrections are done after then entire table is loaded
				for (size_t i = 0; i < lvl_id - 1; ++i) {
					if (xp_table[lvl_id - 1] <= xp_table[i]) {
						infile.error("EngineSettings: XP for level %u is less than previous levels.", lvl_id);
						break;
					}
				}
			}
		}
		infile.close();
	}

	// set invalid XP thresolds to valid values
	for (size_t i = 1; i < xp_table.size(); ++i) {
		if (xp_table[i] <= xp_table[i-1]) {
			xp_table[i] = xp_table[i-1] + 1;
			Utils::logInfo("EngineSettings: Setting XP for level %u to %lu.", i+1, xp_table[i]);
		}
	}

	if (xp_table.empty()) {
		Utils::logError("EngineSettings: No XP table defined.");
		xp_table.push_back(0);
	}
}

unsigned long EngineSettings::XPTable::getLevelXP(int level) {
	if (level <= 1 || xp_table.empty())
		return 0;
	else if (level > static_cast<int>(xp_table.size()))
		return xp_table.back();
	else
		return xp_table[level - 1];
}

int EngineSettings::XPTable::getMaxLevel() {
	return static_cast<int>(xp_table.size());
}

int EngineSettings::XPTable::getLevelFromXP(unsigned long level_xp) {
	int level = 0;

	for (size_t i = 0; i < xp_table.size(); ++i) {
		if (level_xp >= xp_table[i])
			level = static_cast<int>(i + 1);
	}

	return level;
}

void EngineSettings::NumberFormat::load() {
	// reset to defaults
	player_statbar = 0;
	enemy_statbar = 0;
	combat_text = 0;
	character_menu = 2;
	item_tooltips = 2;
	power_tooltips = 2;
	durations = 1;
	death_penalty = 2;

	FileParser infile;
	// @CLASS EngineSettings: Number Format|Description of engine/number_format.txt
	if (infile.open("engine/number_format.txt", FileParser::MOD_FILE, FileParser::ERROR_NONE)) {
		while (infile.next()) {
			// @ATTR player_statbar|int|Number of digits after the decimal place to display for values in the player's statbars (HP/MP).
			if (infile.key == "player_statbar")
				player_statbar = std::max(0, Parse::toInt(infile.val));
			// @ATTR enemy_statbar|int|Number of digits after the decimal place to display for values in the enemy HP statbar.
			else if (infile.key == "enemy_statbar")
				enemy_statbar = std::max(0, Parse::toInt(infile.val));
			// @ATTR combat_text|int|Number of digits after the decimal place to display for values in combat text.
			else if (infile.key == "combat_text")
				combat_text = std::max(0, Parse::toInt(infile.val));
			// @ATTR character_menu|int|Number of digits after the decimal place to display for values in the 'Character' menu.
			else if (infile.key == "character_menu")
				character_menu = std::max(0, Parse::toInt(infile.val));
			// @ATTR item_tooltips|int|Number of digits after the decimal place to display for values in item tooltips.
			else if (infile.key == "item_tooltips")
				item_tooltips = std::max(0, Parse::toInt(infile.val));
			// @ATTR power_tooltips|int|Number of digits after the decimal place to display for values in power tooltips (except durations).
			else if (infile.key == "power_tooltips")
				power_tooltips = std::max(0, Parse::toInt(infile.val));
			// @ATTR durations|int|Number of digits after the decimal place to display for durations.
			else if (infile.key == "durations")
				durations = std::max(0, Parse::toInt(infile.val));
			// @ATTR death_penalty|int|Number of digits after the decimal place to display for death penalty messages.
			else if (infile.key == "death_penalty")
				death_penalty = std::max(0, Parse::toInt(infile.val));

			else
				infile.error("EngineSettings: '%s' is not a valid key.", infile.key.c_str());
		}
		infile.close();
	}

}

EngineSettings::ResourceStats::ResourceStat::ResourceStat()
	: ids(EngineSettings::ResourceStats::STAT_EFFECT_COUNT, "")
	, text(EngineSettings::ResourceStats::STAT_COUNT, "")
	, text_desc(EngineSettings::ResourceStats::STAT_COUNT, "")
{
}

void EngineSettings::ResourceStats::load() {
	// reset to defaults
	list.clear();
	stat_count = 0;
	effect_count = 0;
	stat_effect_count = 0;

	ResourceStat temp;
	ResourceStat* current = &temp;

	FileParser infile;
	// @CLASS EngineSettings: Resource Stats|Description of engine/resource_stats.txt
	if (infile.open("engine/resource_stats.txt", FileParser::MOD_FILE, FileParser::ERROR_NORMAL)) {
		while (infile.next()) {
			if (infile.new_section) {
				if (infile.section == "resource_stat") {
					temp = ResourceStat();
					current = &temp;
				}
			}

			if (infile.section != "resource_stat")
				continue;

			// if we want to replace a list item by ID (ResourceStat uses stat_base as its primary ID), the ID needs to be parsed first
			// but it is not essential if we're just adding to the list, so this is simply a warning
			if (infile.key != "stat_base" && current->ids[STAT_BASE].empty()) {
				infile.error("EngineSettings: Expected 'id', but found '%s'.", infile.key.c_str());
			}

			// TODO check for conflicts with built-in Stats and already parsed ResourceStats
			if (infile.key == "stat_base") {
				// @ATTR resource_stat.stat_base|string|The identifier used for the base ("Max") stat.
				bool found_id = false;
				for (size_t i = 0; i < list.size(); ++i) {
					if (list[i].ids[STAT_BASE] == infile.val) {
						current = &list[i];
						found_id = true;
					}
				}
				if (!found_id) {
					list.push_back(temp);
					current = &(list.back());
					current->ids[STAT_BASE] = infile.val;
				}
			}

			// @ATTR resource_stat.stat_regen|string|The identifier used for the regeneration stat.
			else if (infile.key == "stat_regen") current->ids[STAT_REGEN] = infile.val;
			// @ATTR resource_stat.stat_steal|string|The identifier used for steal stat.
			else if (infile.key == "stat_steal") current->ids[STAT_STEAL] = infile.val;
			// @ATTR resource_stat.stat_resist_steal|string|The identifier used for the resistance to steal stat.
			else if (infile.key == "stat_resist_steal") current->ids[STAT_RESIST_STEAL] = infile.val;
			// @ATTR resource_stat.stat_heal|string|The identifier used for heal-over-time effects.
			else if (infile.key == "stat_heal") current->ids[STAT_HEAL] = infile.val;
			// @ATTR resource_stat.stat_heal_percent|string|The identifier used for percentage-based heal-over-time effects.
			else if (infile.key == "stat_heal_percent") current->ids[STAT_HEAL_PERCENT] = infile.val;

			// @ATTR resource_stat.menu_filename|filename|The MenuStatBar definition file to use for displaying this stat.
			else if (infile.key == "menu_filename") current->menu_filename = infile.val;

			// @ATTR resource_stat.text_base|string|The printed name of the base ("Max") stat as seen in-game.
			else if (infile.key == "text_base") current->text[STAT_BASE] = msg->get(infile.val);
			// @ATTR resource_stat.text_base_desc|string|The printed description of the base ("Max") stat as seen in-game.
			else if (infile.key == "text_base_desc") current->text_desc[STAT_BASE] = msg->get(infile.val);

			// @ATTR resource_stat.text_regen|string|The name of the regeneration stat as seen in-game.
			else if (infile.key == "text_regen") current->text[STAT_REGEN] = msg->get(infile.val);
			// @ATTR resource_stat.text_regen_desc|string|The description of the regeneration stat as seen in-game.
			else if (infile.key == "text_regen_desc") current->text_desc[STAT_REGEN] = msg->get(infile.val);

			// @ATTR resource_stat.text_steal|string|The name of the steal stat as seen in-game.
			else if (infile.key == "text_steal") current->text[STAT_STEAL] = msg->get(infile.val);
			// @ATTR resource_stat.text_steal_desc|string|The description of the steal stat as seen in-game.
			else if (infile.key == "text_steal_desc") current->text_desc[STAT_STEAL] = msg->get(infile.val);

			// @ATTR resource_stat.text_resist_steal|string|The name of the resistance to steal stat as seen in-game.
			else if (infile.key == "text_resist_steal") current->text[STAT_RESIST_STEAL] = msg->get(infile.val);
			// @ATTR resource_stat.text_resist_steal_desc|string|The description of the resistance to steal stat as seen in-game.
			else if (infile.key == "text_resist_steal_desc") current->text_desc[STAT_RESIST_STEAL] = msg->get(infile.val);

			// @ATTR resource_stat.text_combat_heal|string|The name of the stat in combat text as seen during heal-over-time.
			else if (infile.key == "text_combat_heal") current->text_combat_heal = msg->get(infile.val);
			// @ATTR resource_stat.text_log_restore|string|The text in the player's log when this stat is restored via EventManager's 'restore' property.
			else if (infile.key == "text_log_restore") current->text_log_restore = msg->get(infile.val);
			// @ATTR resource_stat.text_log_low|string|The text in the player's log when trying to use a Power that requires more than the available amount of this resource.
			else if (infile.key == "text_log_low") current->text_log_low = msg->get(infile.val);
			// @ATTR resource_stat.text_tooltip_heal|string|The text in Power tooltips used for heal-over-time Effects.
			else if (infile.key == "text_tooltip_heal") current->text_tooltip_heal = msg->get(infile.val);
			// @ATTR resource_stat.text_tooltip_cost|string|The text in Power tooltips that describes the casting cost of this resource.
			else if (infile.key == "text_tooltip_cost") current->text_tooltip_cost = msg->get(infile.val);

			else infile.error("EngineSettings: '%s' is not a valid key.", infile.key.c_str());
		}
		infile.close();
	}

	stat_count = list.size() * EngineSettings::ResourceStats::STAT_COUNT; // base, regen, steal, resist_steal
	effect_count = list.size() * EngineSettings::ResourceStats::EFFECT_COUNT; // heal, heal_percent
	stat_effect_count = stat_count + effect_count;
}

