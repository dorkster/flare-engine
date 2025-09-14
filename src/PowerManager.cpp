/*
Copyright © 2011-2012 Clint Bellanger
Copyright © 2012 Igor Paliychuk
Copyright © 2012 Stefan Beller
Copyright © 2013 Henrik Andersson
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
 * class PowerManager
 */


#include "Animation.h"
#include "AnimationManager.h"
#include "AnimationSet.h"
#include "Avatar.h"
#include "CombatText.h"
#include "EffectManager.h"
#include "EntityManager.h"
#include "EngineSettings.h"
#include "EventManager.h"
#include "FileParser.h"
#include "Hazard.h"
#include "InputState.h"
#include "ItemManager.h"
#include "MapCollision.h"
#include "MapRenderer.h"
#include "Menu.h"
#include "MenuInventory.h"
#include "MenuManager.h"
#include "MessageEngine.h"
#include "PowerManager.h"
#include "Settings.h"
#include "SharedGameResources.h"
#include "SharedResources.h"
#include "SoundManager.h"
#include "StatBlock.h"
#include "Utils.h"
#include "UtilsFileSystem.h"
#include "UtilsMath.h"
#include "UtilsParsing.h"
#include <cmath>
#include <climits>
#include <float.h>

Power::Power()
	: prevent_interrupt(false)
	, face(false)
	, beacon(false)
	, passive(false)
	, meta_power(false)
	, no_actionbar(false)
	, sacrifice(false)
	, requires_los(false)
	, requires_los_default(true)
	, requires_empty_target(false)
	, consumable(false)
	, requires_targeting(false)
	, sfx_hit_enable(false)
	, directional(false)
	, aim_assist(false)
	, on_floor(false)
	, complete_animation(false)
	, use_hazard(false)
	, no_attack(false)
	, no_aggro(false)
	, relative_pos(false)
	, multitarget(false)
	, multihit(false)
	, expire_with_caster(false)
	, ignore_zero_damage(false)
	, lock_target_to_direction(false)
	, target_party(false)
	, trait_armor_penetration(false)
	, trait_avoidance_ignore(false)
	, manual_untransform(false)
	, keep_equipment(false)
	, untransform_on_hit(false)
	, buff(false)
	, buff_teleport(false)
	, buff_party(false)
	, wall_reflect(false)
	, target_movement_normal(true)
	, target_movement_flying(true)
	, target_movement_intangible(true)
	, walls_block_aoe(false)
	, requires_corpse(false)
	, remove_corpse(false)
	, post_hazards_skip_target(false)
	, can_trigger_passives(false)
	, passive_effects_persist(false)

	, spawn_limit_mode(SPAWN_LIMIT_MODE_UNLIMITED)

	, visual_random(0)
	, visual_option(0)

	, type(-1)
	, icon(-1)
	, new_state(-1)
	, state_duration(0)
	, source_type(-1)
	, count(1)
	, passive_trigger(-1)
	, requires_spawns(0)
	, cooldown(0)
	, requires_hpmp_state_mode(RESOURCESTATE_ANY)
	, requires_resource_stat_state_mode(RESOURCESTATE_ALL)
	, sfx_index(-1)
	, lifespan(0)
	, starting_pos(STARTING_POS_SOURCE)
	, movement_type(MapCollision::MOVE_FLYING)
	, mod_accuracy_mode(-1)
	, mod_crit_mode(-1)
	, mod_damage_mode(-1)
	, delay(0)
	, transform_duration(0)
	, target_neighbor(0)
	, script_trigger(-1)

	, requires_mp(0)
	, requires_hp(0)
	, speed(0)
	, charge_speed(0.0f)
	, attack_speed(100.0f)
	, radius(0)
	, target_range(0)
	, combat_range(0)
	, mod_accuracy_value(100)
	, mod_crit_value(100)
	, mod_damage_value_min(100)
	, mod_damage_value_max(0)
	, hp_steal(0)
	, mp_steal(0)
	, missile_angle(0)
	, angle_variance(0)
	, speed_variance(0)
	, spawn_limit_count(1)
	, spawn_limit_ratio(1)
	, trait_crits_impaired(0)
	, target_nearest(0)

	, base_damage(eset ? eset->damage_types.list.size() : 0)
	, converted_damage(eset ? eset->damage_types.list.size() : 0)
	, spawn_limit_stat(0)

	, sfx_hit(0)
	, buff_party_power_id(0)

	, requires_hp_state()
	, requires_mp_state()

	, spawn_level()

	, name("")
	, description("")
	, attack_anim("")
	, animation_name("")
	, spawn_type("")
	, script("")

	, requires_resource_stat(eset->resource_stats.list.size(), 0)
	, requires_resource_stat_state(eset->resource_stats.list.size())
	, resource_steal(eset->resource_stats.list.size(), 0)
{
}

/**
 * PowerManager constructor
 */
PowerManager::PowerManager()
	: collider(NULL)
	, used_items()
	, used_equipped_items() {
	loadEffects();
	loadPowers();
}

bool PowerManager::isValid(PowerID power_id) {
	return power_id > 0 && power_id < powers.size() && powers[power_id];
}

void PowerManager::loadEffects() {
	FileParser infile;

	// @CLASS PowerManager: Effects|Description of powers/effects.txt
	if (!infile.open("powers/effects.txt", FileParser::MOD_FILE, FileParser::ERROR_NORMAL))
		return;

	while (infile.next()) {
		if (infile.new_section) {
			if (infile.section == "effect") {
				// check if the previous effect and remove it if there is no identifier
				if (!effects.empty() && effects.back().id == "") {
					effects.pop_back();
				}
				effects.resize(effects.size()+1);
				effect_animations.resize(effects.size());
			}
		}

		if (effects.empty() || infile.section != "effect")
			continue;

		if (infile.key == "id") {
			// @ATTR effect.id|string|Unique identifier for the effect definition.
			effects.back().id = infile.val;
		}
		else if (infile.key == "type") {
			// @ATTR effect.type|string|Defines the type of effect
			effects.back().type = Effect::getTypeFromString(infile.val);
			effects.back().is_immunity_type = Effect::isImmunityTypeString(infile.val);
			if (effects.back().is_immunity_type) {
				infile.error("PowerManager: '%s' is deprecated. Replace with a corresponding 'resist' effect.", infile.val.c_str());
			}
		}
		else if (infile.key == "name") {
			// @ATTR effect.name|string|A displayed name that is shown when hovering the mouse over the effect icon.
			effects.back().name = infile.val;
		}
		else if (infile.key == "icon") {
			// @ATTR effect.icon|icon_id|The icon to visually represent the effect in the status area
			effects.back().icon = Parse::toInt(infile.val);
		}
		else if (infile.key == "animation") {
			// @ATTR effect.animation|filename|The filename of effect animation.
			effects.back().animation = infile.val;
		}
		else if (infile.key == "can_stack") {
			// @ATTR effect.can_stack|bool|Allows multiple instances of this effect
			effects.back().can_stack = Parse::toBool(infile.val);
		}
		else if (infile.key == "max_stacks") {
			// @ATTR effect.max_stacks|int|Maximum allowed instances of this effect, -1 for no limits
			effects.back().max_stacks = Parse::toInt(infile.val);
		}
		else if (infile.key == "group_stack") {
			// @ATTR effect.group_stack|bool|For effects that can stack, setting this to true will combine those effects into a single status icon.
			effects.back().group_stack = Parse::toBool(infile.val);
		}
		else if (infile.key == "render_above") {
			// @ATTR effect.render_above|bool|Effect is rendered above
			effects.back().render_above = Parse::toBool(infile.val);
		}
		else if (infile.key == "color_mod") {
			// @ATTR effect.color_mod|color|Changes the color of the afflicted entity.
			effects.back().color_mod = Parse::toRGB(infile.val);
		}
		else if (infile.key == "alpha_mod") {
			// @ATTR effect.alpha_mod|int|Changes the alpha of the afflicted entity.
			effects.back().alpha_mod = static_cast<uint8_t>(Parse::toInt(infile.val));
		}
		else if (infile.key == "attack_speed_anim") {
			// @ATTR effect.attack_speed_anim|string|If the type of Effect is attack_speed, this defines the attack animation that will have its speed changed.
			effects.back().attack_speed_anim = infile.val;
		}
		else {
			infile.error("PowerManager: '%s' is not a valid key.", infile.key.c_str());
		}
	}
	infile.close();

	// check if the last effect and remove it if there is no identifier
	if (!effects.empty() && effects.back().id == "") {
		effects.pop_back();
	}

	// load animations
	for (size_t i = 0; i < effects.size(); ++i) {
		if (!effects[i].animation.empty()) {
			anim->increaseCount(effects[i].animation);
			effect_animations[i] = anim->getAnimationSet(effects[i].animation)->getAnimation("");
		}
	}
}

void PowerManager::loadPowers() {
	FileParser infile;

	// @CLASS PowerManager: Powers|Description of powers/powers.txt
	if (!infile.open("powers/powers.txt", FileParser::MOD_FILE, FileParser::ERROR_NORMAL))
		return;

	bool clear_post_effects = false;

	PowerID input_id = 0;
	Power* power;
	bool id_line;

	while (infile.next()) {
		// id needs to be the first component of each power.  That is how we write
		// data to the correct power.
		if (infile.key == "id") {
			// @ATTR power.id|power_id|Uniq identifier for the power definition.
			id_line = true;
			input_id = Parse::toPowerID(infile.val);
			if (input_id < powers.size() && powers[input_id]) {
				clear_post_effects = true;
			}
			else {
				powers.resize(std::max(input_id+1, powers.size()), NULL);
				power_animations.resize(powers.size(), NULL);
				powers[input_id] = new Power();
			}
			power = powers[input_id];

			continue;
		}
		else id_line = false;

		if (input_id < 1) {
			if (id_line) infile.error("PowerManager: Power index out of bounds 1-%d, skipping power.", INT_MAX);
		}
		if (id_line)
			continue;

		if (infile.key == "type") {
			// @ATTR power.type|["fixed", "missile", "repeater", "spawn", "transform", "block"]|Defines the type of power definiton
			if (infile.val == "fixed") power->type = Power::TYPE_FIXED;
			else if (infile.val == "missile") power->type = Power::TYPE_MISSILE;
			else if (infile.val == "repeater") power->type = Power::TYPE_REPEATER;
			else if (infile.val == "spawn") power->type = Power::TYPE_SPAWN;
			else if (infile.val == "transform") power->type = Power::TYPE_TRANSFORM;
			else if (infile.val == "block") power->type = Power::TYPE_BLOCK;
			else infile.error("PowerManager: Unknown type '%s'", infile.val.c_str());
		}
		else if (infile.key == "name") {
			// @ATTR power.name|string|The name of the power
			power->name = msg->get(infile.val);
		}
		else if (infile.key == "description") {
			// @ATTR power.description|string|Description of the power
			power->description = msg->get(infile.val);
		}
		else if (infile.key == "icon") {
			// @ATTR power.icon|icon_id|The icon to visually represent the power eg. in skill tree or action bar.
			power->icon = Parse::toInt(infile.val);
		}
		else if (infile.key == "new_state") {
			// @ATTR power.new_state|predefined_string|When power is used, hero or enemy will change to this state. Must be one of the states ["instant", user defined]
			if (infile.val == "instant") power->new_state = Power::STATE_INSTANT;
			else {
				power->new_state = Power::STATE_ATTACK;
				power->attack_anim = infile.val;
			}
		}
		else if (infile.key == "state_duration") {
			// @ATTR power.state_duration|duration|Sets the length of time the caster is in their state animation. A time longer than the animation length will cause the animation to pause on the last frame. Times shorter than the state animation length will have no effect.
			power->state_duration = Parse::toDuration(infile.val);
		}
		else if (infile.key == "prevent_interrupt") {
			// @ATTR power.prevent_interrupt|bool|Prevents the caster from being interrupted by a hit when casting this power.
			power->prevent_interrupt = Parse::toBool(infile.val);
		}
		else if (infile.key == "face") {
			// @ATTR power.face|bool|Power will make hero or enemy to face the target location.
			power->face = Parse::toBool(infile.val);
		}
		else if (infile.key == "source_type") {
			// @ATTR power.source_type|["hero", "neutral", "enemy"]|Determines which entities the power can effect.
			if (infile.val == "hero") power->source_type = Power::SOURCE_TYPE_HERO;
			else if (infile.val == "neutral") power->source_type = Power::SOURCE_TYPE_NEUTRAL;
			else if (infile.val == "enemy") power->source_type = Power::SOURCE_TYPE_ENEMY;
			else infile.error("PowerManager: Unknown source_type '%s'", infile.val.c_str());
		}
		else if (infile.key == "beacon") {
			// @ATTR power.beacon|bool|True if enemy is calling its allies.
			power->beacon = Parse::toBool(infile.val);
		}
		else if (infile.key == "count") {
			// @ATTR power.count|int|The count of hazards/effect or spawns to be created by this power.
			power->count = Parse::toInt(infile.val);
		}
		else if (infile.key == "passive") {
			// @ATTR power.passive|bool|If power is unlocked when the hero or enemy spawns it will be automatically activated.
			power->passive = Parse::toBool(infile.val);
		}
		else if (infile.key == "passive_trigger") {
			// @ATTR power.passive_trigger|["on_block", "on_hit", "on_halfdeath", "on_joincombat", "on_death", "on_active_power"]|This will only activate a passive power under a certain condition.
			if (infile.val == "on_block") power->passive_trigger = Power::TRIGGER_BLOCK;
			else if (infile.val == "on_hit") power->passive_trigger = Power::TRIGGER_HIT;
			else if (infile.val == "on_halfdeath") power->passive_trigger = Power::TRIGGER_HALFDEATH;
			else if (infile.val == "on_joincombat") power->passive_trigger = Power::TRIGGER_JOINCOMBAT;
			else if (infile.val == "on_death") power->passive_trigger = Power::TRIGGER_DEATH;
			else if (infile.val == "on_active_power") power->passive_trigger = Power::TRIGGER_ACTIVE_POWER;
			else infile.error("PowerManager: Unknown passive trigger '%s'", infile.val.c_str());
		}
		else if (infile.key == "meta_power") {
			// @ATTR power.meta_power|bool|If true, this power can not be used on it's own. Instead, it should be replaced via an item with a replace_power entry.
			power->meta_power = Parse::toBool(infile.val);
		}
		else if (infile.key == "no_actionbar") {
			// @ATTR power.no_actionbar|bool|If true, this power is prevented from being placed on the actionbar.
			power->no_actionbar = Parse::toBool(infile.val);
		}
		// power requirements
		else if (infile.key == "requires_flags") {
			// @ATTR power.requires_flags|list(predefined_string)|A comma separated list of equip flags that are required to use this power. See engine/equip_flags.txt
			power->requires_flags.clear();
			std::string flag = Parse::popFirstString(infile.val);

			while (flag != "") {
				power->requires_flags.insert(flag);
				flag = Parse::popFirstString(infile.val);
			}
		}
		else if (infile.key == "requires_mp") {
			// @ATTR power.requires_mp|float|Require an amount of MP to use the power. The amount will be consumed on usage.
			power->requires_mp = Parse::toFloat(infile.val);
		}
		else if (infile.key == "requires_hp") {
			// @ATTR power.requires_hp|float|Require an amount of HP to use the power. The amount will be consumed on usage.
			power->requires_hp = Parse::toFloat(infile.val);
		}
		else if (infile.key == "requires_resource_stat") {
			// @ATTR power.requires_resource_stat|repeatable(predefined_string, float) : Resource stat ID, Required amount|Requires an amount of a given resource to use the power. The amount will be consumed on usage.
			std::string stat_id = Parse::popFirstString(infile.val);
			float stat_val = Parse::popFirstFloat(infile.val);

			bool found_stat_id = false;
			for (size_t i = 0; i < power->requires_resource_stat.size(); ++i) {
				if (stat_id == eset->resource_stats.list[i].ids[EngineSettings::ResourceStats::STAT_BASE]) {
					power->requires_resource_stat[i] = stat_val;
					found_stat_id = true;
					break;
				}
			}

			if (!found_stat_id) {
				infile.error("PowerManager: '%s' is not a valid resource stat.", stat_id.c_str());
			}
		}
		else if (infile.key == "sacrifice") {
			// @ATTR power.sacrifice|bool|If the power has requires_hp, allow it to kill the caster.
			power->sacrifice = Parse::toBool(infile.val);
		}
		else if (infile.key == "requires_los") {
			// @ATTR power.requires_los|bool|Requires a line-of-sight to target.
			power->requires_los = Parse::toBool(infile.val);
			power->requires_los_default = false;
		}
		else if (infile.key == "requires_empty_target") {
			// @ATTR power.requires_empty_target|bool|The power can only be cast when target tile is empty.
			power->requires_empty_target = Parse::toBool(infile.val);
		}
		else if (infile.key == "requires_item") {
			// @ATTR power.requires_item|repeatable(item_id, int) : Item, Quantity|Requires a specific item of a specific quantity in inventory. If quantity > 0, then the item will be removed.
			PowerRequiredItem pri;
			pri.id = items->verifyID(Parse::toItemID(Parse::popFirstString(infile.val)), &infile, !ItemManager::VERIFY_ALLOW_ZERO, !ItemManager::VERIFY_ALLOCATE);
			if (pri.id > 0) {
				pri.quantity = Parse::toInt(Parse::popFirstString(infile.val), 1);
				pri.equipped = false;
				power->required_items.push_back(pri);
			}
		}
		else if (infile.key == "requires_equipped_item") {
			// @ATTR power.requires_equipped_item|repeatable(item_id, int) : Item, Quantity|Requires a specific item of a specific quantity to be equipped on hero. If quantity > 0, then the item will be removed.
			PowerRequiredItem pri;
			pri.id = items->verifyID(Parse::toItemID(Parse::popFirstString(infile.val)), &infile, !ItemManager::VERIFY_ALLOW_ZERO, !ItemManager::VERIFY_ALLOCATE);
			if (pri.id > 0) {
				pri.quantity = Parse::popFirstInt(infile.val);
				pri.equipped = true;

				// a maximum of 1 equipped item can be consumed at a time
				if (pri.quantity > 1) {
					infile.error("PowerManager: Only 1 equipped item can be consumed at a time.");
					pri.quantity = std::min(pri.quantity, 1);
				}

				power->required_items.push_back(pri);
			}
		}
		else if (infile.key == "requires_targeting") {
			// @ATTR power.requires_targeting|bool|Power is only used when targeting using click-to-target.
			power->requires_targeting = Parse::toBool(infile.val);
		}
		else if (infile.key == "requires_spawns") {
			// @ATTR power.requires_spawns|int|The caster must have at least this many summoned creatures to use this power.
			power->requires_spawns = Parse::toInt(infile.val);
		}
		else if (infile.key == "cooldown") {
			// @ATTR power.cooldown|duration|Specify the duration for cooldown of the power in 'ms' or 's'.
			power->cooldown = Parse::toDuration(infile.val);
		}
		else if (infile.key == "requires_hpmp_state") {
			// @ATTR power.requires_hpmp_state|["all", "any"], ["percent", "not_percent", "percent_exact", "ignore"], float , ["percent", "not_percent", "percent_exact", "ignore"], float: Mode, HP state, HP Percentage value, MP state, MP Percentage value|Power can only be used when HP/MP matches the specified state. In 'all' mode, both HP and MP must meet the requirements, where as only one must in 'any' mode. To check a single stat, use 'all' mode and set the 'ignore' state for the other stat.

			std::string mode = Parse::popFirstString(infile.val);
			std::string state_hp = Parse::popFirstString(infile.val);
			std::string state_hp_val = Parse::popFirstString(infile.val);
			std::string state_mp = Parse::popFirstString(infile.val);
			std::string state_mp_val = Parse::popFirstString(infile.val);

			power->requires_hp_state.value = Parse::toFloat(state_hp_val);
			power->requires_mp_state.value = Parse::toFloat(state_mp_val);

			if (state_hp == "percent") {
				power->requires_hp_state.state = Power::RESOURCESTATE_PERCENT;
			}
			else if (state_hp == "not_percent") {
				power->requires_hp_state.state = Power::RESOURCESTATE_NOT_PERCENT;
			}
			else if (state_hp == "percent_exact") {
				power->requires_hp_state.state = Power::RESOURCESTATE_PERCENT_EXACT;
			}
			else if (state_hp == "ignore" || state_hp.empty()) {
				power->requires_hp_state.state = Power::RESOURCESTATE_IGNORE;
			}
			else {
				infile.error("PowerManager: '%s' is not a valid hp/mp state. Use 'percent', 'not_percent', or 'ignore'.", state_hp.c_str());
			}

			if (state_mp == "percent") {
				power->requires_mp_state.state = Power::RESOURCESTATE_PERCENT;
			}
			else if (state_mp == "not_percent") {
				power->requires_mp_state.state = Power::RESOURCESTATE_NOT_PERCENT;
			}
			else if (state_hp == "percent_exact") {
				power->requires_mp_state.state = Power::RESOURCESTATE_PERCENT_EXACT;
			}
			else if (state_mp == "ignore" || state_mp.empty()) {
				power->requires_mp_state.state = Power::RESOURCESTATE_IGNORE;
			}
			else {
				infile.error("PowerManager: '%s' is not a valid hp/mp state. Use 'percent', 'not_percent', or 'ignore'.", state_mp.c_str());
			}

			if (mode == "any") {
				power->requires_hpmp_state_mode = Power::RESOURCESTATE_ANY;
			}
			else if (mode == "all") {
				power->requires_hpmp_state_mode = Power::RESOURCESTATE_ALL;
			}
			else if (mode == "hp") {
				// TODO deprecated
				infile.error("PowerManager: 'hp' has been deprecated. Use 'all' or 'any'.");

				power->requires_hpmp_state_mode = Power::RESOURCESTATE_ALL;
				power->requires_mp_state.state = Power::RESOURCESTATE_IGNORE;
			}
			else if (mode == "mp") {
				// TODO deprecated
				infile.error("PowerManager: 'mp' has been deprecated. Use 'any' or 'all'.");

				// use the HP values for MP, then ignore the HP stat
				power->requires_hpmp_state_mode = Power::RESOURCESTATE_ALL;
				power->requires_mp_state.state = power->requires_hp_state.state;
				power->requires_mp_state.value = power->requires_hp_state.value;
				power->requires_hp_state.state = Power::RESOURCESTATE_IGNORE;
			}
			else {
				infile.error("PowerManager: Please specify 'any' or 'all'.");
			}
		}
		else if (infile.key == "requires_resource_stat_state") {
			// @ATTR power.requires_resource_stat_state|predefined_string, ["percent", "not_percent", "percent_exact", "ignore"], float : Resource stat ID, State, Percentage value|Power can only be used when the resource stat matches the specified state. See 'requires_resource_stat_state_mode' for combining multiple resource states and, optionally, HP/MP state.
			std::string stat_id = Parse::popFirstString(infile.val);
			std::string state = Parse::popFirstString(infile.val);
			float value = Parse::popFirstFloat(infile.val);

			for (size_t i = 0; i < power->requires_resource_stat_state.size(); ++i) {
				if (stat_id == eset->resource_stats.list[i].ids[EngineSettings::ResourceStats::STAT_BASE]) {
					power->requires_resource_stat_state[i].value = value;

					if (state == "percent") {
						power->requires_resource_stat_state[i].state = Power::RESOURCESTATE_PERCENT;
					}
					else if (state == "not_percent") {
						power->requires_resource_stat_state[i].state = Power::RESOURCESTATE_NOT_PERCENT;
					}
					else if (state == "percent_exact") {
						power->requires_resource_stat_state[i].state = Power::RESOURCESTATE_PERCENT_EXACT;
					}
					else if (state.empty() || state == "ignore") {
						power->requires_resource_stat_state[i].state = Power::RESOURCESTATE_IGNORE;
					}
					else {
						infile.error("PowerManager: '%s' is not a valid resource stat state. Use 'percent', 'not_percent', or 'ignore'.", state.c_str());
					}

					break;
				}
			}
		}
		else if (infile.key == "requires_resource_stat_state_mode") {
			// @ATTR|power.requires_resource_stat_state_mode|["all", "any", "any_hpmp"]|Determines how required resource state is handled for multiple resources. "all" means that HP/MP state AND all resource stat states must pass. "any" means that HP/MP state must pass AND at least one resource stat state must pass. "any_hpmp" means that HP/MP state OR at least one resource stat state is required to pass.
			std::string mode = Parse::popFirstString(infile.val);

			if (mode == "all") {
				power->requires_resource_stat_state_mode = Power::RESOURCESTATE_ALL;
			}
			else if (mode == "any") {
				power->requires_resource_stat_state_mode = Power::RESOURCESTATE_ANY;
			}
			else if (mode == "any_hpmp") {
				power->requires_resource_stat_state_mode = Power::RESOURCESTATE_ANY_HPMP;
			}
			else {
				infile.error("PowerManager: '%s' is not a valid mode. Possible modes include: 'all', 'any', or 'any_hpmp'.", mode.c_str());
			}
		}
		// animation info
		else if (infile.key == "animation") {
			// @ATTR power.animation|filename|The filename of the power animation.
			power->animation_name = infile.val;
		}
		else if (infile.key == "soundfx") {
			// @ATTR power.soundfx|filename|Filename of a sound effect to play when the power is used.
			power->sfx_index = loadSFX(infile.val);
		}
		else if (infile.key == "soundfx_hit") {
			// @ATTR power.soundfx_hit|filename|Filename of a sound effect to play when the power's hazard hits a valid target.
			int sfx_id = loadSFX(infile.val);
			if (sfx_id != -1) {
				power->sfx_hit = sfx[sfx_id];
				power->sfx_hit_enable = true;
			}
		}
		else if (infile.key == "directional") {
			// @ATTR power.directional|bool|The animation sprite sheet contains 8 directions, one per row.
			power->directional = Parse::toBool(infile.val);
		}
		else if (infile.key == "visual_option") {
			// @ATTR power.visual_option|int|If the power is not directional, this option determines which direction should be used for the animation.
			power->visual_option = static_cast<unsigned short>(Parse::toInt(infile.val));
		}
		else if (infile.key == "visual_random") {
			// @ATTR power.visual_random|int|If the power is not directional, this option determines which direction should be used for the animation. The resulting direction is random in the range of (visual_option, visual_option + visual_random).
			power->visual_random = static_cast<unsigned short>(Parse::toInt(infile.val));
		}
		else if (infile.key == "aim_assist") {
			// @ATTR power.aim_assist|bool|If true, power targeting will be offset vertically by the number of pixels set with "aim_assist" in engine/misc.txt.
			power->aim_assist = Parse::toBool(infile.val);
		}
		else if (infile.key == "speed") {
			// @ATTR power.speed|float|The speed of missile hazard, the unit is defined as map units per frame.
			power->speed = Parse::toFloat(infile.val) / settings->max_frames_per_sec;
		}
		else if (infile.key == "lifespan") {
			// @ATTR power.lifespan|duration|How long the hazard/animation lasts in 'ms' or 's'.
			power->lifespan = Parse::toDuration(infile.val);
		}
		else if (infile.key == "floor") {
			// @ATTR power.floor|bool|The hazard is drawn between the background and the object layer.
			power->on_floor = Parse::toBool(infile.val);
		}
		else if (infile.key == "complete_animation") {
			// @ATTR power.complete_animation|bool|For hazards; Play the entire animation, even if the hazard has hit a target.
			power->complete_animation = Parse::toBool(infile.val);
		}
		else if (infile.key == "charge_speed") {
			// @ATTR power.charge_speed|float|Moves the caster at this speed in the direction they are facing until the state animation is finished.
			power->charge_speed = Parse::toFloat(infile.val) / settings->max_frames_per_sec;
		}
		else if (infile.key == "attack_speed") {
			// @ATTR power.attack_speed|float|Changes attack animation speed for this Power. A value of 100 is 100% speed (aka normal speed).
			power->attack_speed = Parse::toFloat(infile.val);
			if (power->attack_speed < 100) {
				Utils::logInfo("PowerManager: Attack speeds less than 100 are unsupported."); // TODO is this still true?
				power->attack_speed = 100;
			}
		}
		// hazard traits
		else if (infile.key == "use_hazard") {
			// @ATTR power.use_hazard|bool|Power uses hazard.
			power->use_hazard = Parse::toBool(infile.val);
		}
		else if (infile.key == "no_attack") {
			// @ATTR power.no_attack|bool|Hazard won't affect other entities.
			power->no_attack = Parse::toBool(infile.val);
		}
		else if (infile.key == "no_aggro") {
			// @ATTR power.no_aggro|bool|If true, the Hazard won't put its target in a combat state.
			power->no_aggro = Parse::toBool(infile.val);
		}
		else if (infile.key == "radius") {
			// @ATTR power.radius|float|Radius in map units
			power->radius = Parse::toFloat(infile.val);
		}
		else if (infile.key == "base_damage") {
			// @ATTR power.base_damage|predefined_string : Damage type ID|Determines which damage stat will be used to calculate damage.
			for (size_t i = 0; i < eset->damage_types.list.size(); ++i) {
				if (infile.val == eset->damage_types.list[i].id) {
					power->base_damage = i;
					break;
				}
			}

			if (power->base_damage == eset->damage_types.list.size()) {
				infile.error("PowerManager: Unknown base_damage '%s'", infile.val.c_str());
			}
		}
		else if (infile.key == "starting_pos") {
			// @ATTR power.starting_pos|["source", "target", "melee"]|Start position for hazard
			if (infile.val == "source")      power->starting_pos = Power::STARTING_POS_SOURCE;
			else if (infile.val == "target") power->starting_pos = Power::STARTING_POS_TARGET;
			else if (infile.val == "melee")  power->starting_pos = Power::STARTING_POS_MELEE;
			else infile.error("PowerManager: Unknown starting_pos '%s'", infile.val.c_str());
		}
		else if (infile.key == "relative_pos") {
			// @ATTR power.relative_pos|bool|Hazard will move relative to the caster's position.
			power->relative_pos = Parse::toBool(infile.val);
		}
		else if (infile.key == "multitarget") {
			// @ATTR power.multitarget|bool|Allows a hazard power to hit more than one entity.
			power->multitarget = Parse::toBool(infile.val);
		}
		else if (infile.key == "multihit") {
			// @ATTR power.multihit|bool|Allows a hazard power to hit the same entity more than once.
			power->multihit = Parse::toBool(infile.val);
		}
		else if (infile.key == "expire_with_caster") {
			// @ATTR power.expire_with_caster|bool|If true, hazard will disappear when the caster dies.
			power->expire_with_caster = Parse::toBool(infile.val);
		}
		else if (infile.key == "ignore_zero_damage") {
			// @ATTR power.ignore_zero_damage|bool|If true, hazard can still hit the player when damage is 0, triggering post_power and post_effects.
			power->ignore_zero_damage = Parse::toBool(infile.val);
		}
		else if (infile.key == "lock_target_to_direction") {
			// @ATTR power.lock_target_to_direction|bool|If true, the target is "snapped" to one of the 8 directions.
			power->lock_target_to_direction = Parse::toBool(infile.val);
		}
		else if (infile.key == "movement_type") {
			// @ATTR power.movement_type|["ground", "flying", "intangible"]|For moving hazards (missile/repeater), this defines which parts of the map it can collide with. The default is "flying".
			if (infile.val == "ground")         power->movement_type = MapCollision::MOVE_NORMAL;
			else if (infile.val == "flying")    power->movement_type = MapCollision::MOVE_FLYING;
			else if (infile.val == "intangible") power->movement_type = MapCollision::MOVE_INTANGIBLE;
			else infile.error("PowerManager: Unknown movement_type '%s'", infile.val.c_str());
		}
		else if (infile.key == "trait_armor_penetration") {
			// @ATTR power.trait_armor_penetration|bool|Ignores the target's Absorbtion stat
			power->trait_armor_penetration = Parse::toBool(infile.val);
		}
		else if (infile.key == "trait_avoidance_ignore") {
			// @ATTR power.trait_avoidance_ignore|bool|Ignores the target's Avoidance stat
			power->trait_avoidance_ignore = Parse::toBool(infile.val);
		}
		else if (infile.key == "trait_crits_impaired") {
			// @ATTR power.trait_crits_impaired|int|Increases critical hit percentage for slowed/immobile targets
			power->trait_crits_impaired = Parse::toFloat(infile.val);
		}
		else if (infile.key == "trait_elemental") {
			// @ATTR power.trait_elemental|predefined_string|Converts all damage done to a given damage type (see engine/damage_types.txt). Despite the name, this is not restricted to "elemental" damage types.
			for (size_t i = 0; i < eset->damage_types.list.size(); ++i) {
				if (infile.val == eset->damage_types.list[i].id) {
					power->converted_damage = i;
					break;
				}
			}
		}
		else if (infile.key == "target_range") {
			// @ATTR power.target_range|float|The distance from the caster that the power can be activated
			power->target_range = Parse::popFirstFloat(infile.val);
		}
		//steal effects
		else if (infile.key == "hp_steal") {
			// @ATTR power.hp_steal|float|Percentage of damage to steal into HP
			power->hp_steal = Parse::toFloat(infile.val);
		}
		else if (infile.key == "mp_steal") {
			// @ATTR power.mp_steal|float|Percentage of damage to steal into MP
			power->mp_steal = Parse::toFloat(infile.val);
		}
		else if (infile.key == "resource_steal") {
			// @ATTR power.resource_steal|repeatable(predefined_string, float) : Resource stat ID, Steal amount|Percentage of damage to steal into the specified resource
			std::string stat_id = Parse::popFirstString(infile.val);
			float stat_value = Parse::popFirstFloat(infile.val);

			for (size_t i = 0; i < power->resource_steal.size(); ++i) {
				if (stat_id == eset->resource_stats.list[i].ids[EngineSettings::ResourceStats::STAT_BASE]) {
					power->resource_steal[i] = stat_value;
					break;
				}
			}
		}
		//missile modifiers
		else if (infile.key == "missile_angle") {
			// @ATTR power.missile_angle|float|Angle of missile
			power->missile_angle = Parse::toFloat(infile.val);
		}
		else if (infile.key == "angle_variance") {
			// @ATTR power.angle_variance|float|Percentage of variance added to missile angle
			power->angle_variance = Parse::toFloat(infile.val);
		}
		else if (infile.key == "speed_variance") {
			// @ATTR power.speed_variance|float|Percentage of variance added to missile speed
			power->speed_variance = Parse::toFloat(infile.val);
		}
		//repeater modifiers
		else if (infile.key == "delay") {
			// @ATTR power.delay|duration|Delay between repeats in 'ms' or 's'.
			power->delay = Parse::toDuration(infile.val);
		}
		// buff/debuff durations
		else if (infile.key == "transform_duration") {
			// @ATTR power.transform_duration|duration|Duration for transform in 'ms' or 's'.
			power->transform_duration = Parse::toDuration(infile.val);
		}
		else if (infile.key == "manual_untransform") {
			// @ATTR power.manual_untransform|bool|Force manual untranform
			power->manual_untransform = Parse::toBool(infile.val);
		}
		else if (infile.key == "keep_equipment") {
			// @ATTR power.keep_equipment|bool|Keep equipment while transformed
			power->keep_equipment = Parse::toBool(infile.val);
		}
		else if (infile.key == "untransform_on_hit") {
			// @ATTR power.untransform_on_hit|bool|Force untransform when the player is hit
			power->untransform_on_hit = Parse::toBool(infile.val);
		}
		// buffs
		else if (infile.key == "buff") {
			// @ATTR power.buff|bool|Power is cast upon the caster.
			power->buff= Parse::toBool(infile.val);
		}
		else if (infile.key == "buff_teleport") {
			// @ATTR power.buff_teleport|bool|Power is a teleportation power.
			power->buff_teleport = Parse::toBool(infile.val);
		}
		else if (infile.key == "buff_party") {
			// @ATTR power.buff_party|bool|Power is cast upon party members
			power->buff_party = Parse::toBool(infile.val);
		}
		else if (infile.key == "buff_party_power_id") {
			// @ATTR power.buff_party_power_id|power_id|Only party members that were spawned with this power ID are affected by "buff_party=true". Setting this to 0 will affect all party members.
			power->buff_party_power_id = Parse::toPowerID(infile.val);
		}
		else if (infile.key == "post_effect" || infile.key == "post_effect_src") {
			// @ATTR power.post_effect|predefined_string, float, duration , float: Effect ID, Magnitude, Duration, Chance to apply|Post effect to apply to target. Duration is in 'ms' or 's'.
			// @ATTR power.post_effect_src|predefined_string, float, duration , float: Effect ID, Magnitude, Duration, Chance to apply|Post effect to apply to caster. Duration is in 'ms' or 's'.
			if (clear_post_effects) {
				power->post_effects.clear();
				clear_post_effects = false;
			}
			PostEffect pe;
			pe.id = Parse::popFirstString(infile.val);
			if (!isValidEffect(pe.id)) {
				infile.error("PowerManager: Unknown effect '%s'", pe.id.c_str());
			}
			else {
				if (infile.key == "post_effect_src")
					pe.target_src = true;

				std::string magnitude_str = Parse::popFirstString(infile.val);
				if (!magnitude_str.empty()) {
					if (magnitude_str[magnitude_str.size() - 1] == '%') {
						pe.is_multiplier = true;
						magnitude_str.resize(magnitude_str.size() - 1);
						pe.magnitude = Parse::toFloat(magnitude_str) / 100;
					}
					else {
						pe.magnitude = Parse::toFloat(magnitude_str);
					}
				}

				// TODO deprecated
				if (pe.id == "hp_percent") {
					infile.error("PowerManager: 'hp_percent' is deprecated. Converting to hp.");
					pe.id = "hp";
					pe.is_multiplier = true;
					pe.magnitude = (pe.magnitude + 100) / 100;
				}
				else if (pe.id == "mp_percent") {
					infile.error("PowerManager: 'mp_percent' is deprecated. Converting to mp.");
					pe.id = "mp";
					pe.is_multiplier = true;
					pe.magnitude = (pe.magnitude + 100) / 100;
				}

				pe.duration = Parse::toDuration(Parse::popFirstString(infile.val));
				std::string chance = Parse::popFirstString(infile.val);
				if (!chance.empty()) {
					pe.chance = Parse::toFloat(chance);
				}

				int pe_type;
				bool is_immunity_type = false;
				EffectDef* effect_def = getEffectDef(pe.id);
				if (effect_def) {
					pe_type = effect_def->type;
					is_immunity_type = effect_def->is_immunity_type;
				}
				else {
					pe_type = Effect::getTypeFromString(pe.id);
					is_immunity_type = Effect::isImmunityTypeString(pe.id);
				}

				if (is_immunity_type && (pe_type == Effect::RESIST_ALL || Effect::typeIsEffectResist(pe_type))) {
					infile.error("PowerManager: Post effect '%s' matches a deprecated type. Converting to a resistance with 100 magnitude.", pe.id.c_str());
					pe.magnitude = 100;
				}

				power->post_effects.push_back(pe);
			}
		}
		// pre and post power effects
		else if (infile.key == "pre_power") {
			// @ATTR power.pre_power|repeatable(power_id, float) : Power, Chance to cast|Trigger a power immediately when casting this one.
			ChainPower chain_power;
			chain_power.type = ChainPower::TYPE_PRE;
			chain_power.id = Parse::toPowerID(Parse::popFirstString(infile.val));
			std::string chance = Parse::popFirstString(infile.val);
			if (!chance.empty()) {
				chain_power.chance = Parse::toFloat(chance);
			}
			if (chain_power.id > 0) {
				power->chain_powers.push_back(chain_power);
			}
		}
		else if (infile.key == "post_power") {
			// @ATTR power.post_power|repeatable(power_id, int) : Power, Chance to cast|Trigger a power if the hazard did damage. For 'block' type powers, this power will be triggered when the blocker takes damage.
			ChainPower chain_power;
			chain_power.type = ChainPower::TYPE_POST;
			chain_power.id = Parse::toPowerID(Parse::popFirstString(infile.val));
			std::string chance = Parse::popFirstString(infile.val);
			if (!chance.empty()) {
				chain_power.chance = Parse::toFloat(chance);
			}
			if (chain_power.id > 0) {
				power->chain_powers.push_back(chain_power);
			}
		}
		else if (infile.key == "wall_power") {
			// @ATTR power.wall_power|repeatable(power_id, int) : Power, Chance to cast|Trigger a power if the hazard hit a wall.
			ChainPower chain_power;
			chain_power.type = ChainPower::TYPE_WALL;
			chain_power.id = Parse::toPowerID(Parse::popFirstString(infile.val));
			std::string chance = Parse::popFirstString(infile.val);
			if (!chance.empty()) {
				chain_power.chance = Parse::toFloat(chance);
			}
			if (chain_power.id > 0) {
				power->chain_powers.push_back(chain_power);
			}
		}
		else if (infile.key == "expire_power") {
			// @ATTR power.expire_power|repeatable(power_id, int) : Power, Chance to cast|Trigger a power when the hazard's lifespan expires.
			ChainPower chain_power;
			chain_power.type = ChainPower::TYPE_EXPIRE;
			chain_power.id = Parse::toPowerID(Parse::popFirstString(infile.val));
			std::string chance = Parse::popFirstString(infile.val);
			if (!chance.empty()) {
				chain_power.chance = Parse::toFloat(chance);
			}
			if (chain_power.id > 0) {
				power->chain_powers.push_back(chain_power);
			}
		}
		else if (infile.key == "wall_reflect") {
			// @ATTR power.wall_reflect|bool|Moving power will bounce off walls and keep going
			power->wall_reflect = Parse::toBool(infile.val);
		}

		// spawn info
		else if (infile.key == "spawn_type") {
			// @ATTR power.spawn_type|predefined_string|For non-transform powers, an enemy is spawned from this category. For transform powers, the caster will transform into a creature from this category.
			power->spawn_type = infile.val;
		}
		else if (infile.key == "target_neighbor") {
			// @ATTR power.target_neighbor|int|Target is changed to an adjacent tile within a radius.
			power->target_neighbor = Parse::toInt(infile.val);
		}
		else if (infile.key == "spawn_limit") {
			// @ATTR power.spawn_limit|["unlimited", "fixed", "stat"], int, float, predefined_string : Mode, Entity Level, Ratio, Primary stat|The maximum number of creatures that can be spawned and alive from this power. The need for the last three parameters depends on the mode being used. The "unlimited" mode requires no parameters and will remove any spawn limit requirements. The "fixed" mode takes one parameter as the spawn limit. The "stat" mode also requires the ratio and primary stat ID as parameters. The ratio adjusts the scaling of the spawn limit. For example, spawn_limit=stat,1,2,physical will set the spawn limit to 1/2 the summoner's Physical stat.
			std::string mode = Parse::popFirstString(infile.val);
			if (mode == "fixed") power->spawn_limit_mode = Power::SPAWN_LIMIT_MODE_FIXED;
			else if (mode == "stat") power->spawn_limit_mode = Power::SPAWN_LIMIT_MODE_STAT;
			else if (mode == "unlimited") power->spawn_limit_mode = Power::SPAWN_LIMIT_MODE_UNLIMITED;
			else infile.error("PowerManager: Unknown spawn_limit_mode '%s'", mode.c_str());

			if(power->spawn_limit_mode != Power::SPAWN_LIMIT_MODE_UNLIMITED) {
				power->spawn_limit_count = static_cast<float>(Parse::popFirstInt(infile.val));

				if(power->spawn_limit_mode == Power::SPAWN_LIMIT_MODE_STAT) {
					power->spawn_limit_ratio = Parse::popFirstFloat(infile.val);

					std::string stat = Parse::popFirstString(infile.val);
					size_t prim_stat_index = eset->primary_stats.getIndexByID(stat);

					if (prim_stat_index != eset->primary_stats.list.size()) {
						power->spawn_limit_stat = prim_stat_index;
					}
					else {
						infile.error("PowerManager: '%s' is not a valid primary stat.", stat.c_str());
					}
				}
			}
		}
		else if (infile.key == "spawn_level") {
			// @ATTR power.spawn_level|["default", "fixed", "level", "stat"], int, float, predefined_string : Mode, Entity Level, Ratio, Primary stat|The level of spawned creatures. The need for the last three parameters depends on the mode being used. The "default" mode will just use the entity's normal level and doesn't require any additional parameters. The "fixed" mode only requires the entity level as a parameter. The "stat" and "level" modes also require the ratio as a parameter. The ratio adjusts the scaling of the entity level. For example, spawn_level=stat,1,2,physical will set the spawned entity level to 1/2 the summoner's Physical stat. Only the "stat" mode requires the last parameter, which is simply the ID of the primary stat that should be used for scaling.
			std::string mode = Parse::popFirstString(infile.val);
			if (mode == "default") power->spawn_level.mode = SpawnLevel::MODE_DEFAULT;
			else if (mode == "fixed") power->spawn_level.mode = SpawnLevel::MODE_FIXED;
			else if (mode == "stat") power->spawn_level.mode = SpawnLevel::MODE_STAT;
			else if (mode == "level") power->spawn_level.mode = SpawnLevel::MODE_LEVEL;
			else infile.error("PowerManager: Unknown spawn level mode '%s'", mode.c_str());

			if(power->spawn_level.mode != SpawnLevel::MODE_DEFAULT) {
				power->spawn_level.count = static_cast<float>(Parse::popFirstInt(infile.val));

				if(power->spawn_level.mode != SpawnLevel::MODE_FIXED) {
					power->spawn_level.ratio = Parse::popFirstFloat(infile.val);

					if(power->spawn_level.mode == SpawnLevel::MODE_STAT) {
						std::string stat = Parse::popFirstString(infile.val);
						size_t prim_stat_index = eset->primary_stats.getIndexByID(stat);

						if (prim_stat_index != eset->primary_stats.list.size()) {
							power->spawn_level.stat = prim_stat_index;
						}
						else {
							infile.error("PowerManager: '%s' is not a valid primary stat.", stat.c_str());
						}
					}
				}
			}
		}
		else if (infile.key == "target_party") {
			// @ATTR power.target_party|bool|Hazard will only affect party members.
			power->target_party = Parse::toBool(infile.val);
		}
		else if (infile.key == "target_categories") {
			// @ATTR power.target_categories|list(predefined_string)|Hazard will only affect enemies in these categories.
			power->target_categories.clear();
			std::string cat;
			while ((cat = Parse::popFirstString(infile.val)) != "") {
				power->target_categories.push_back(cat);
			}
		}
		else if (infile.key == "modifier_accuracy") {
			// @ATTR power.modifier_accuracy|["multiply", "add", "absolute"], float : Mode, Value|Changes this power's accuracy.
			std::string mode = Parse::popFirstString(infile.val);
			if(mode == "multiply") power->mod_accuracy_mode = Power::STAT_MODIFIER_MODE_MULTIPLY;
			else if(mode == "add") power->mod_accuracy_mode = Power::STAT_MODIFIER_MODE_ADD;
			else if(mode == "absolute") power->mod_accuracy_mode = Power::STAT_MODIFIER_MODE_ABSOLUTE;
			else infile.error("PowerManager: Unknown stat_modifier_mode '%s'", mode.c_str());

			power->mod_accuracy_value = Parse::popFirstFloat(infile.val);
		}
		else if (infile.key == "modifier_damage") {
			// @ATTR power.modifier_damage|["multiply", "add", "absolute"], float, float : Mode, Min, Max|Changes this power's damage. The "Max" value is ignored, except in the case of "absolute" modifiers.
			std::string mode = Parse::popFirstString(infile.val);
			if(mode == "multiply") power->mod_damage_mode = Power::STAT_MODIFIER_MODE_MULTIPLY;
			else if(mode == "add") power->mod_damage_mode = Power::STAT_MODIFIER_MODE_ADD;
			else if(mode == "absolute") power->mod_damage_mode = Power::STAT_MODIFIER_MODE_ABSOLUTE;
			else infile.error("PowerManager: Unknown stat_modifier_mode '%s'", mode.c_str());

			power->mod_damage_value_min = Parse::popFirstFloat(infile.val);
			power->mod_damage_value_max = Parse::popFirstFloat(infile.val);
		}
		else if (infile.key == "modifier_critical") {
			// @ATTR power.modifier_critical|["multiply", "add", "absolute"], float : Mode, Value|Changes the chance that this power will land a critical hit.
			std::string mode = Parse::popFirstString(infile.val);
			if(mode == "multiply") power->mod_crit_mode = Power::STAT_MODIFIER_MODE_MULTIPLY;
			else if(mode == "add") power->mod_crit_mode = Power::STAT_MODIFIER_MODE_ADD;
			else if(mode == "absolute") power->mod_crit_mode = Power::STAT_MODIFIER_MODE_ABSOLUTE;
			else infile.error("PowerManager: Unknown stat_modifier_mode '%s'", mode.c_str());

			power->mod_crit_value = Parse::popFirstFloat(infile.val);
		}
		else if (infile.key == "target_movement_normal") {
			// @ATTR power.target_movement_normal|bool|Power can affect entities with normal movement (aka walking on ground)
			power->target_movement_normal = Parse::toBool(infile.val);
		}
		else if (infile.key == "target_movement_flying") {
			// @ATTR power.target_movement_flying|bool|Power can affect flying entities
			power->target_movement_flying = Parse::toBool(infile.val);
		}
		else if (infile.key == "target_movement_intangible") {
			// @ATTR power.target_movement_intangible|bool|Power can affect intangible entities
			power->target_movement_intangible = Parse::toBool(infile.val);
		}
		else if (infile.key == "walls_block_aoe") {
			// @ATTR power.walls_block_aoe|bool|When true, prevents hazard aoe from hitting targets that are behind walls/pits.
			power->walls_block_aoe = Parse::toBool(infile.val);
		}
		else if (infile.key == "script") {
			// @ATTR power.script|["on_cast", "on_hit", "on_wall"], filename : Trigger, Filename|Loads and executes a script file when the trigger is activated.
			std::string trigger = Parse::popFirstString(infile.val);
			if (trigger == "on_cast") power->script_trigger = Power::SCRIPT_TRIGGER_CAST;
			else if (trigger == "on_hit") power->script_trigger = Power::SCRIPT_TRIGGER_HIT;
			else if (trigger == "on_wall") power->script_trigger = Power::SCRIPT_TRIGGER_WALL;
			else infile.error("PowerManager: Unknown script trigger '%s'", trigger.c_str());

			power->script = Parse::popFirstString(infile.val);
		}
		else if (infile.key == "remove_effect") {
			// @ATTR power.remove_effect|repeatable(predefined_string, int) : Effect ID, Number of Effect instances|Removes a number of instances of a specific Effect ID. Omitting the number of instances, or setting it to zero, will remove all instances/stacks.
			std::string first = Parse::popFirstString(infile.val);
			int second = Parse::popFirstInt(infile.val);
			power->remove_effects.push_back(std::pair<std::string, int>(first, second));
		}
		else if (infile.key == "replace_by_effect") {
			// @ATTR power.replace_by_effect|repeatable(power_id, predefined_string, int) : Power ID, Effect ID, Number of Effect instances|If the caster has at least the number of instances of the Effect ID, the defined Power ID will be cast instead.
			PowerReplaceByEffect prbe;
			prbe.power_id = Parse::toPowerID(Parse::popFirstString(infile.val));
			prbe.effect_id = Parse::popFirstString(infile.val);
			prbe.count = Parse::popFirstInt(infile.val);
			power->replace_by_effect.push_back(prbe);
		}
		else if (infile.key == "requires_corpse") {
			// @ATTR power.requires_corpse|["consume", bool]|If true, a corpse must be targeted for this power to be used. If "consume", then the corpse is also consumed on Power use.
			if (infile.val == "consume") {
				power->requires_corpse = true;
				power->remove_corpse = true;
			}
			else {
				power->requires_corpse = Parse::toBool(infile.val);
				power->remove_corpse = false;
			}
		}
		else if (infile.key == "target_nearest") {
			// @ATTR power.target_nearest|float|Will automatically target the nearest enemy within the specified range.
			power->target_nearest = Parse::toFloat(infile.val);
		}
		else if (infile.key == "disable_equip_slots") {
			// @ATTR power.disable_equip_slots|list(predefined_string)|Passive powers only. A comma separated list of equip slot types to disable when this power is active.
			power->disable_equip_slots.clear();
			std::string slot_type = Parse::popFirstString(infile.val);

			while (slot_type != "") {
				power->disable_equip_slots.push_back(items->getItemTypeIndexByString(slot_type));
				slot_type = Parse::popFirstString(infile.val);
			}
		}
		else if (infile.key == "post_hazards_skip_target") {
			// @ATTR power.post_hazards_skip_target|bool|When this power's hazard hits a target, this property determines if hazards spawned by post_power can hit the same target.
			power->post_hazards_skip_target = Parse::toBool(infile.val);
		}
		else if (infile.key == "can_trigger_passives") {
			// @ATTR power.can_trigger_passives|bool|If true, this power can trigger passive powers that have passive_trigger=on_active_power.
			power->can_trigger_passives = Parse::toBool(infile.val);
		}
		else if (infile.key == "passive_effects_persist") {
			// @ATTR power.passive_effects_persist|bool|If true, post effects from this passive power will not be removed if the passive power is deactivated.
			power->passive_effects_persist = Parse::toBool(infile.val);
		}

		else infile.error("PowerManager: '%s' is not a valid key", infile.key.c_str());
	}
	infile.close();

	size_t count_allocated = 0;
	for (size_t i = 0; i < powers.size(); ++i) {
		power = powers[i];

		if (!power)
			continue;
		else
			count_allocated++;

		// load animations
		if (!power->animation_name.empty()) {
			anim->increaseCount(power->animation_name);
			power_animations[i] = anim->getAnimationSet(power->animation_name)->getAnimation("");
		}

		// verify power ids
		power->buff_party_power_id = verifyID(power->buff_party_power_id, NULL, ALLOW_ZERO_ID);

		for (size_t j = power->chain_powers.size(); j > 0; --j) {
			size_t index = j-1;
			power->chain_powers[index].id = verifyID(power->chain_powers[index].id, NULL, !ALLOW_ZERO_ID);
			if (power->chain_powers[index].id == 0) {
				if (power->chain_powers[index].type == ChainPower::TYPE_PRE)
					Utils::logError("PowerManager: Removed pre_power from power %zu.", i);
				else if (power->chain_powers[index].type == ChainPower::TYPE_POST)
					Utils::logError("PowerManager: Removed post_power from power %zu.", i);
				else if (power->chain_powers[index].type == ChainPower::TYPE_WALL)
					Utils::logError("PowerManager: Removed wall_power from power %zu.", i);

				power->chain_powers.erase(power->chain_powers.begin() + index);
			}
		}

		for (size_t j = power->replace_by_effect.size(); j > 0; --j) {
			size_t index = j-1;
			power->replace_by_effect[index].power_id = verifyID(power->replace_by_effect[index].power_id, NULL, !ALLOW_ZERO_ID);
			if (power->replace_by_effect[index].power_id == 0) {
				Utils::logError("PowerManager: Removed replace_by_effect from power %zu.", i);
				power->replace_by_effect.erase(power->replace_by_effect.begin() + index);
			}
		}

		// calculate effective combat range
		{
			// TODO apparently, missiles and repeaters don't need to have "use_hazard=true"?
			if (!( (!power->use_hazard && power->type == Power::TYPE_FIXED) || power->no_attack) ) {
				if (power->type == Power::TYPE_FIXED) {
					if (power->relative_pos) {
						power->combat_range += power->charge_speed * static_cast<float>(power->lifespan);
					}
					if (power->starting_pos == Power::STARTING_POS_TARGET) {
						power->combat_range = FLT_MAX - power->radius;
					}
				}
				else if (power->type == Power::TYPE_MISSILE) {
					power->combat_range += power->speed * static_cast<float>(power->lifespan);
				}
				else if (power->type == Power::TYPE_REPEATER) {
					power->combat_range += power->speed * static_cast<float>(power->count);
				}

				power->combat_range += (power->radius / 2.f);
			}
		}
	}
	Utils::logInfo("PowerManager: Power IDs = %zu reserved / %zu allocated / %zu empty / %zu bytes used", powers.size()-1, count_allocated, powers.size()-1-count_allocated, (sizeof(Power*) * powers.size()) + (sizeof(Power) * count_allocated));

	// verify power ids in items
	for (size_t i = 1; i < items->items.size(); ++i) {
		Item* item = items->items[i];

		if (!item)
			continue;

		item->power = verifyID(item->power, NULL, ALLOW_ZERO_ID);

		for (size_t j = item->bonus.size(); j > 0; --j) {
			size_t index = j-1;
			if (item->bonus[index].type == BonusData::POWER_LEVEL) {
				item->bonus[index].power_id = verifyID(item->bonus[index].power_id, NULL, !ALLOW_ZERO_ID);

				if (item->bonus[index].power_id == 0)
					item->bonus.erase(item->bonus.begin() + index);
			}
		}

		for (size_t j = item->replace_power.size(); j > 0; --j) {
			size_t index = j-1;
			item->replace_power[index].first = verifyID(item->replace_power[index].first, NULL, !ALLOW_ZERO_ID);
			item->replace_power[index].second = verifyID(item->replace_power[index].second, NULL, !ALLOW_ZERO_ID);

			if (item->replace_power[index].first == 0 || item->replace_power[index].second == 0)
				item->replace_power.erase(item->replace_power.begin() + index);
		}
	}
}

bool PowerManager::isValidEffect(const std::string& type) {
	if (type == "speed")
		return true;
	if (type == "attack_speed")
		return true;

	// TODO deprecated
	if (type == "hp_percent")
		return true;
	if (type == "mp_percent")
		return true;

	for (size_t i = 0; i < eset->primary_stats.list.size(); ++i) {
		if (type == eset->primary_stats.list[i].id)
			return true;
	}

	for (size_t i = 0; i < eset->damage_types.list.size(); ++i) {
		if (type == eset->damage_types.list[i].min)
			return true;
		else if (type == eset->damage_types.list[i].max)
			return true;
		else if (type == eset->damage_types.list[i].resist)
			return true;
	}

	for (int i=0; i<Stats::COUNT; ++i) {
		if (type == Stats::KEY[i])
			return true;
	}

	if (getEffectDef(type) != NULL)
		return true;

	if (Effect::getTypeFromString(type) != Effect::NONE)
		return true;

	return false;
}

/**
 * Load the specified sound effect for this power
 *
 * @param filename The .ogg file containing the sound for this power, assumed to be in soundfx/powers/
 * @return The sfx[] array index for this mix chunk, or -1 upon load failure
 */
int PowerManager::loadSFX(const std::string& filename) {

	SoundID sid = snd->load(filename, "PowerManager sfx");
	std::vector<SoundID>::iterator it = std::find(sfx.begin(), sfx.end(), sid);
	if (it == sfx.end()) {
		sfx.push_back(sid);
		return static_cast<int>(sfx.size()) - 1;
	}

	return static_cast<int>(it - sfx.begin());
}


/**
 * Set new collision object
 */
void PowerManager::handleNewMap(MapCollision *_collider) {
	collider = _collider;
}

/**
 * Check if the target is valid (not an empty area or a wall). Only used by the hero
 */
bool PowerManager::hasValidTarget(PowerID power_index, StatBlock *src_stats, const FPoint& target) {

	if (!collider) return false;

	Power* power = powers[power_index];

	FPoint limit_target = Utils::clampDistance(power->target_range, src_stats->pos, target);

	if (power->requires_los && !collider->lineOfSight(src_stats->pos.x, src_stats->pos.y, limit_target.x, limit_target.y))
		return false;

	if (power->requires_empty_target && !collider->isValidPosition(limit_target.x, limit_target.y, power->movement_type, MapCollision::COLLIDE_TYPE_ALL_ENTITIES))
		return false;

	if (power->buff_teleport && power->target_neighbor < 1 && !collider->isValidPosition(limit_target.x, limit_target.y, src_stats->movement_type, MapCollision::COLLIDE_TYPE_ALL_ENTITIES))
		return false;

	return true;
}

/**
 * Apply basic power info to a new hazard.
 *
 * This can be called several times to combine powers.
 * Typically done when a base power can be modified by equipment
 * (e.g. ammo type affects the traits of powers that shoot)
 *
 * @param power_index The activated power ID
 * @param src_stats The StatBlock of the power activator
 * @param target Aim position in map coordinates
 * @param haz A newly-initialized hazard
 */
void PowerManager::initHazard(PowerID power_index, StatBlock *src_stats, const FPoint& origin, const FPoint& target, Hazard *haz) {

	//the hazard holds the statblock of its source
	haz->src_stats = src_stats;

	haz->power_index = power_index;
	haz->power = powers[power_index];

	if (haz->power->source_type == -1) {
		if (src_stats->hero) haz->source_type = Power::SOURCE_TYPE_HERO;
		else if (src_stats->hero_ally) haz->source_type = Power::SOURCE_TYPE_ALLY;
		else haz->source_type = Power::SOURCE_TYPE_ENEMY;
	}
	else {
		haz->source_type = haz->power->source_type;
	}

	// Hazard attributes based on power source
	haz->crit_chance = src_stats->get(Stats::CRIT);
	haz->accuracy = src_stats->get(Stats::ACCURACY);

	if (haz->power->base_damage != haz->damage.size()) {
		for (size_t i = 0; i < haz->damage.size(); ++i) {
			if (haz->power->converted_damage != haz->damage.size()) {
				if (i == haz->power->converted_damage || i == haz->power->base_damage) {
					// the power's base damage is converted to a single damage type
					// any additional damage that matches the converted type is also added
					haz->damage[haz->power->converted_damage].min += src_stats->getDamageMin(i);
					haz->damage[haz->power->converted_damage].max += src_stats->getDamageMax(i);
				}
			}
			else if (i == haz->power->base_damage || (!eset->damage_types.list[haz->power->base_damage].is_elemental && eset->damage_types.list[i].is_elemental)) {
				// damage is added based on the power's base damage attribute
				// if the base damage type is non-elemental, we also do damage for all elemental types
				// this allows us to have, for example, a sword that primarily deals melee damage with additional fire damage
				haz->damage[i].min += src_stats->getDamageMin(i);
				haz->damage[i].max += src_stats->getDamageMax(i);
			}
		}
	}

	// animation properties
	if (haz->power->animation_name != "") {
		haz->loadAnimation(haz->power->animation_name);
	}

	if (haz->power->directional) {
		haz->direction = Utils::calcDirection(origin.x, origin.y, target.x, target.y);
	}
	else if (haz->power->visual_random) {
		haz->direction = static_cast<unsigned short>(rand()) % haz->power->visual_random;
		haz->direction += haz->power->visual_option;
	}
	else if (haz->power->visual_option) {
		haz->direction = haz->power->visual_option;
	}

	// combat traits
	haz->base_speed = haz->power->speed;
	haz->lifespan = haz->power->lifespan;
	haz->active = !haz->power->no_attack;

	// hazard starting position
	if (haz->power->starting_pos == Power::STARTING_POS_SOURCE) {
		haz->pos = origin;
	}
	else if (haz->power->starting_pos == Power::STARTING_POS_TARGET) {
		haz->pos = Utils::clampDistance(haz->power->target_range, origin, target);
	}
	else if (haz->power->starting_pos == Power::STARTING_POS_MELEE) {
		haz->pos = Utils::calcVector(origin, src_stats->direction, src_stats->melee_range);
	}

	if (haz->power->target_neighbor > 0 && collider) {
		haz->pos = collider->getRandomNeighbor(Point(haz->pos), haz->power->target_neighbor, haz->power->movement_type, MapCollision::COLLIDE_TYPE_HAZARD);
	}

	if (haz->power->relative_pos) {
		haz->relative_pos = true;
		// TODO use origin here?
		haz->pos_offset.x = src_stats->pos.x - haz->pos.x;
		haz->pos_offset.y = src_stats->pos.y - haz->pos.y;
	}
}

/**
 * Any attack-based effects are handled by hazards.
 * Self-enhancements (buffs) are handled by this function.
 */
void PowerManager::buff(PowerID power_index, StatBlock *src_stats, const FPoint& origin, const FPoint& target) {
	Power* power = powers[power_index];

	// teleport to the target location
	if (power->buff_teleport) {
		FPoint limit_target = Utils::clampDistance(power->target_range,src_stats->pos,target);
		if (power->target_neighbor > 0 && collider) {
			FPoint new_target = collider->getRandomNeighbor(Point(limit_target), power->target_neighbor, power->movement_type, MapCollision::COLLIDE_TYPE_ALL_ENTITIES);
			if (floorf(new_target.x) == floorf(limit_target.x) && floorf(new_target.y) == floorf(limit_target.y)) {
				src_stats->teleportation = false;
			}
			else {
				src_stats->teleportation = true;
				src_stats->teleport_destination.x = new_target.x;
				src_stats->teleport_destination.y = new_target.y;
			}
		}
		else {
			src_stats->teleportation = true;
			src_stats->teleport_destination.x = limit_target.x;
			src_stats->teleport_destination.y = limit_target.y;
		}
	}

	// handle all other effects
	if (power->buff || (power->buff_party && (src_stats->hero_ally || src_stats->enemy_ally))) {
		int source_type = src_stats->hero ? Power::SOURCE_TYPE_HERO : (src_stats->hero_ally ? Power::SOURCE_TYPE_ALLY : Power::SOURCE_TYPE_ENEMY);
		effect(src_stats, src_stats, power_index, source_type);
	}

	if (power->buff_party && !power->passive) {
		src_stats->party_buffs.push(power_index);
	}

	// activate any post powers here if the power doesn't use a hazard
	// otherwise the post power will chain off the hazard itself
	// this is also where Effects are removed for non-hazard powers
	if (!power->use_hazard) {
		src_stats->effects.removeEffectID(power->remove_effects);

		if (!power->passive) {
			for (size_t i = 0; i < power->chain_powers.size(); ++i) {
				ChainPower& chain_power = power->chain_powers[i];
				if (chain_power.type == ChainPower::TYPE_POST && Math::percentChanceF(chain_power.chance)) {
					activate(chain_power.id, src_stats, origin, src_stats->pos);
				}
			}
		}
	}
}

/**
 * Play the sound effect for this power
 * Equipped items may have unique sounds
 */
void PowerManager::playSound(PowerID power_index) {
	if (powers[power_index]->sfx_index != -1)
		snd->play(sfx[powers[power_index]->sfx_index], snd->DEFAULT_CHANNEL, snd->NO_POS, !snd->LOOP);
}

bool PowerManager::effect(StatBlock *target_stats, StatBlock *caster_stats, PowerID power_index, int source_type) {
	if (!isValid(power_index))
		return false;

	const Power* pwr = powers[power_index];
	for (size_t i = 0; i < pwr->post_effects.size(); ++i) {
		const PostEffect& pe = pwr->post_effects[i];

		if (!Math::percentChanceF(pe.chance))
			continue;

		EffectDef effect_data;
		EffectDef* effect_ptr = getEffectDef(pe.id);

		float magnitude = pe.magnitude;
		int duration = pe.duration;

		StatBlock *dest_stats = pe.target_src ? caster_stats : target_stats;
		if (dest_stats->hp <= 0 && !(effect_data.type == Effect::REVIVE || (effect_data.type == Effect::NONE && pe.id == "revive")))
			continue;

		if (effect_ptr != NULL) {
			// effects loaded from powers/effects.txt
			effect_data = (*effect_ptr);

			if (effect_data.type == Effect::SHIELD) {
				if (pwr->base_damage == eset->damage_types.list.size())
					continue;

				// charge shield to max ment weapon damage * damage multiplier
				if(pwr->mod_damage_mode == Power::STAT_MODIFIER_MODE_MULTIPLY)
					magnitude = caster_stats->getDamageMax(pwr->base_damage) * pwr->mod_damage_value_min / 100;
				else if(pwr->mod_damage_mode == Power::STAT_MODIFIER_MODE_ADD)
					magnitude = caster_stats->getDamageMax(pwr->base_damage) + pwr->mod_damage_value_min;
				else if(pwr->mod_damage_mode == Power::STAT_MODIFIER_MODE_ABSOLUTE)
					magnitude = Math::randBetweenF(pwr->mod_damage_value_min, pwr->mod_damage_value_max);
				else
					magnitude = caster_stats->getDamageMax(pwr->base_damage);

				comb->addString(msg->getv("+%s Shield", Utils::floatToString(magnitude, eset->number_format.combat_text).c_str()), dest_stats->pos, CombatText::MSG_BUFF);
			}
			else if (effect_data.type == Effect::HEAL) {
				if (pwr->base_damage == eset->damage_types.list.size())
					continue;

				// heal for ment weapon damage * damage multiplier
				magnitude = Math::randBetweenF(caster_stats->getDamageMin(pwr->base_damage), caster_stats->getDamageMax(pwr->base_damage));

				if(pwr->mod_damage_mode == Power::STAT_MODIFIER_MODE_MULTIPLY)
					magnitude = magnitude * pwr->mod_damage_value_min / 100;
				else if(pwr->mod_damage_mode == Power::STAT_MODIFIER_MODE_ADD)
					magnitude += pwr->mod_damage_value_min;
				else if(pwr->mod_damage_mode == Power::STAT_MODIFIER_MODE_ABSOLUTE)
					magnitude = Math::randBetweenF(pwr->mod_damage_value_min, pwr->mod_damage_value_max);

				comb->addString(msg->getv("+%s HP", Utils::floatToString(magnitude, eset->number_format.combat_text).c_str()), dest_stats->pos, CombatText::MSG_BUFF);
				dest_stats->hp += magnitude;
				if (dest_stats->hp > dest_stats->get(Stats::HP_MAX)) dest_stats->hp = dest_stats->get(Stats::HP_MAX);
			}
			else if (effect_data.type == Effect::KNOCKBACK) {
				if (dest_stats->speed_default == 0) {
					// enemies that can't move can't be knocked back
					continue;
				}
				dest_stats->knockback_srcpos = pe.target_src ? target_stats->pos : caster_stats->pos;
				dest_stats->knockback_destpos = pe.target_src ? caster_stats->pos : target_stats->pos;
			}
		}
		else {
			// all other effects
			effect_data.id = pe.id;
			effect_data.type = Effect::getTypeFromString(pe.id);
		}

		EffectParams effect_params;
		effect_params.duration = duration;
		effect_params.magnitude = magnitude;
		effect_params.source_type = source_type;
		effect_params.power_id = power_index;
		effect_params.is_multiplier = pe.is_multiplier;

		dest_stats->effects.addEffect(dest_stats, effect_data, effect_params);
	}

	return true;
}

/**
 * The activated power creates a static effect (not a moving hazard)
 *
 * @param power_index The activated power ID
 * @param src_stats The StatBlock of the power activator
 * @param target The mouse cursor position in map coordinates
 * return boolean true if successful
 */
bool PowerManager::fixed(PowerID power_index, StatBlock *src_stats, const FPoint& origin, const FPoint& target) {
	Power* power = powers[power_index];

	if (power->use_hazard) {
		int delay_iterator = 0;
		for (int i = 0; i < power->count; i++) {
			Hazard *haz = new Hazard(collider);
			initHazard(power_index, src_stats, origin, target, haz);

			// add optional delay
			haz->delay_frames = delay_iterator;
			delay_iterator += power->delay;

			// Hazard memory is now the responsibility of HazardManager
			hazards.push(haz);
		}
	}

	buff(power_index, src_stats, origin, target);

	// If there's a sound effect, play it here
	playSound(power_index);

	payPowerCost(power_index, src_stats);
	return true;
}

/**
 * The activated power creates a group of missile hazards (e.g. arrow, thrown knife, firebolt).
 * Each individual missile is a single animated hazard that travels from the caster position to the
 * mouse target position.
 *
 * @param power_index The activated power ID
 * @param src_stats The StatBlock of the power activator
 * @param target The mouse cursor position in map coordinates
 * return boolean true if successful
 */
bool PowerManager::missile(PowerID power_index, StatBlock *src_stats, const FPoint& origin, const FPoint& target) {
	Power* power = powers[power_index];

	FPoint src;
	if (power->starting_pos == Power::STARTING_POS_TARGET) {
		src = target;
	}
	else {
		src = origin;
	}

	// calculate polar coordinates angle
	float theta = Utils::calcTheta(src.x, src.y, target.x, target.y);

	int delay_iterator = 0;

	//generate hazards
	for (int i = 0; i < power->count; i++) {
		Hazard *haz = new Hazard(collider);
		initHazard(power_index, src_stats, origin, target, haz);

		//calculate individual missile angle
		float offset_angle = ((1.0f - static_cast<float>(power->count))/2 + static_cast<float>(i)) * (power->missile_angle * static_cast<float>(M_PI) / 180.0f);
		float variance = 0;
		if (power->angle_variance != 0) {
			//random arc between negative angle_variance and positive angle_variance
			variance = Math::randBetweenF(power->angle_variance * -1.f, power->angle_variance) * static_cast<float>(M_PI) / 180.0f;
		}
		float alpha = theta + offset_angle + variance;

		//calculate the missile velocity
		float speed_var = 0;
		if (power->speed_variance != 0) {
			const float var = power->speed_variance;
			speed_var = ((var * 2.0f * static_cast<float>(rand())) / static_cast<float>(RAND_MAX)) - var;
		}

		// set speed and angle
		haz->base_speed += speed_var;
		haz->setAngle(alpha);

		// add optional delay
		haz->delay_frames = delay_iterator;
		delay_iterator += power->delay;

		hazards.push(haz);
	}

	payPowerCost(power_index, src_stats);

	playSound(power_index);
	return true;
}

/**
 * Repeaters are multiple hazards that spawn in a straight line
 */
bool PowerManager::repeater(PowerID power_index, StatBlock *src_stats, const FPoint& origin, const FPoint& target) {
	Power* power = powers[power_index];

	//initialize variables
	FPoint location_iterator;
	FPoint speed;
	int delay_iterator = 0;

	// calculate polar coordinates angle
	float theta = Utils::calcTheta(origin.x, origin.y, target.x, target.y);

	float repeater_speed = (power->speed * settings->max_frames_per_sec) / Settings::LOGIC_FPS;

	speed.x = repeater_speed * cosf(theta);
	speed.y = repeater_speed * sinf(theta);

	location_iterator = origin;

	Hazard* parent_haz = NULL;
	for (int i = 0; i < power->count; i++) {

		location_iterator.x += speed.x;
		location_iterator.y += speed.y;

		// only travels until it hits a wall
		if (collider && !collider->isValidPosition(location_iterator.x, location_iterator.y, power->movement_type, MapCollision::COLLIDE_TYPE_HAZARD)) {
			break; // no more hazards
		}

		Hazard *haz = new Hazard(collider);
		initHazard(power_index, src_stats, origin, target, haz);

		haz->pos = location_iterator;
		haz->delay_frames = delay_iterator;
		delay_iterator += power->delay;

		if (i == 0 && power->count > 1) {
			parent_haz = haz;
		}
		else if (parent_haz != NULL && i > 0) {
			haz->parent = parent_haz;
			parent_haz->children.push_back(haz);
		}

		hazards.push(haz);
	}

	payPowerCost(power_index, src_stats);

	playSound(power_index);

	return true;

}


/**
 * Spawn a creature. Does not create a hazard
 */
bool PowerManager::spawn(PowerID power_index, StatBlock *src_stats, const FPoint& origin, const FPoint& target) {
	if (!collider)
		return false;

	Power* power = powers[power_index];

	Map_Enemy espawn;
	espawn.type = power->spawn_type;
	espawn.summoner = src_stats;

	// enemy spawning position
	if (power->starting_pos == Power::STARTING_POS_SOURCE) {
		espawn.pos = origin;
	}
	else if (power->starting_pos == Power::STARTING_POS_TARGET) {
		espawn.pos = target;
	}
	else if (power->starting_pos == Power::STARTING_POS_MELEE) {
		espawn.pos = Utils::calcVector(origin, src_stats->direction, src_stats->melee_range);
	}

	if (power->target_neighbor > 0 && collider) {
		espawn.pos = collider->getRandomNeighbor(Point(espawn.pos), power->target_neighbor, power->movement_type, MapCollision::COLLIDE_TYPE_ALL_ENTITIES);
	}

	if (!collider->isValidPosition(espawn.pos.x, espawn.pos.y, power->movement_type, MapCollision::COLLIDE_TYPE_ALL_ENTITIES)) {
		// desired spawn point is blocked; try spawning next to the caster
		espawn.pos = collider->getRandomNeighbor(Point(origin), 1, power->movement_type, MapCollision::COLLIDE_TYPE_ALL_ENTITIES);
	}

	if (!collider->isValidPosition(espawn.pos.x, espawn.pos.y, power->movement_type, MapCollision::COLLIDE_TYPE_ALL_ENTITIES)) {
		// couldn't find an open spawn point next to the caster; abort spawning process
		return false;
	}

	espawn.direction = Utils::calcDirection(origin.x, origin.y, target.x, target.y);
	espawn.summon_power_index = power_index;
	espawn.hero_ally = src_stats->hero || src_stats->hero_ally;
	espawn.enemy_ally = !src_stats->hero;

	collider->block(espawn.pos.x, espawn.pos.y, espawn.hero_ally);

	for (int i=0; i < power->count; i++) {
		map_enemies.push(espawn);
	}

	// apply any buffs
	buff(power_index, src_stats, origin, target);

	payPowerCost(power_index, src_stats);

	// If there's a sound effect, play it here
	playSound(power_index);

	return true;
}

/**
 * Transform into a creature. Fully replaces entity characteristics
 */
bool PowerManager::transform(PowerID power_index, StatBlock *src_stats, const FPoint& target) {
	if (!collider)
		return false;

	Power* power = powers[power_index];

	// locking the actionbar prevents power usage until after the hero is transformed
	inpt->lockActionBar();

	if (src_stats->transformed && power->spawn_type != "untransform") {
		pc->logMsg(msg->get("You are already transformed, untransform first."), Avatar::MSG_NORMAL);
		return false;
	}

	// execute untransform powers
	if (power->spawn_type == "untransform" && src_stats->transformed) {
		collider->unblock(src_stats->pos.x, src_stats->pos.y);
		if (collider->isValidPosition(src_stats->pos.x, src_stats->pos.y, MapCollision::MOVE_NORMAL, MapCollision::COLLIDE_TYPE_HERO)) {
			src_stats->transform_duration = 0;
			src_stats->transform_type = "untransform"; // untransform() is called only if type !=""
		}
		else {
			pc->logMsg(msg->get("Could not untransform at this position."), Avatar::MSG_NORMAL);
			inpt->unlockActionBar();
			collider->block(src_stats->pos.x, src_stats->pos.y, false);
			return false;
		}
		collider->block(src_stats->pos.x, src_stats->pos.y, false);
	}
	else {
		if (power->transform_duration == 0) {
			// permanent transformation
			src_stats->transform_duration = -1;
		}
		else if (power->transform_duration > 0) {
			// timed transformation
			src_stats->transform_duration = power->transform_duration;
		}

		src_stats->transform_type = power->spawn_type;
	}

	// apply any buffs
	buff(power_index, src_stats, src_stats->pos, target);

	src_stats->manual_untransform = power->manual_untransform;
	src_stats->transform_with_equipment = power->keep_equipment;
	src_stats->untransform_on_hit = power->untransform_on_hit;

	// If there's a sound effect, play it here
	playSound(power_index);

	payPowerCost(power_index, src_stats);

	return true;
}

/**
 * Stationary blocking with optional buffs/debuffs
 * Only the hero can block
 */
bool PowerManager::block(PowerID power_index, StatBlock *src_stats) {
	Power* power = powers[power_index];

	// if the hero is blocking, we can't activate any more blocking powers
	if (src_stats->effects.triggered_block)
		return false;

	src_stats->effects.triggered_block = true;
	src_stats->block_power = power_index;

	// apply any attached effects
	// passive_trigger MUST be "Power::TRIGGER_BLOCK", since that is how we will later remove effects added by blocking
	power->passive_trigger = Power::TRIGGER_BLOCK;
	effect(src_stats, src_stats, power_index, Power::SOURCE_TYPE_HERO);

	// If there's a sound effect, play it here
	playSound(power_index);

	payPowerCost(power_index, src_stats);

	return true;
}

PowerID PowerManager::checkReplaceByEffect(PowerID power_index, StatBlock *src_stats) {
	if (!isValid(power_index))
		return 0;

	Power* power = powers[power_index];

	for (size_t i = 0; i < power->replace_by_effect.size(); ++i) {
		if (src_stats->effects.hasEffect(power->replace_by_effect[i].effect_id, power->replace_by_effect[i].count)) {
			return power->replace_by_effect[i].power_id;
		}
	}

	return power_index;
}

/**
 * Activate is basically a switch/redirect to the appropriate function
 */
bool PowerManager::activate(PowerID power_index, StatBlock *src_stats, const FPoint& origin, const FPoint& target) {
	if (!isValid(power_index))
		return false;

	Power* power = powers[power_index];

	if (src_stats->hero) {
		if (power->requires_mp > src_stats->mp)
			return false;

		for (size_t i = 0; i < power->requires_resource_stat.size(); ++i) {
			if (power->requires_resource_stat[i] > src_stats->resource_stats[i])
				return false;
		}

		if (!src_stats->target_corpse && src_stats->target_nearest_corpse && checkNearestTargeting(power, src_stats, true))
			src_stats->target_corpse = src_stats->target_nearest_corpse;

		if (power->requires_corpse && !src_stats->target_corpse)
			return false;
	}

	if (src_stats->hp > 0 && power->sacrifice == false && power->requires_hp >= src_stats->hp)
		return false;

	if (power->type == Power::TYPE_BLOCK)
		return block(power_index, src_stats);

	if (power->script_trigger == Power::SCRIPT_TRIGGER_CAST) {
		eventm->executeScript(power->script, origin.x, origin.y);
	}

	// check if we need to snap the target to one of the 8 directions
	FPoint new_target = target;
	if (power->lock_target_to_direction) {
		float dist = Utils::calcDist(origin, new_target);
		int dir = Utils::calcDirection(origin.x, origin.y, new_target.x, new_target.y);
		new_target = Utils::calcVector(origin, dir, dist);
	}
	else if (power->speed != 0) {
		// power wants to move, but we have no direction
		if (origin.x == target.x && origin.y == target.y) {
			float dist = std::max(0.1f, src_stats->melee_range);
			if (src_stats->pos.x == origin.x && src_stats->pos.y == origin.y) {
				// starting from the caster's position, we can use the caster's direction
				new_target = Utils::calcVector(origin, src_stats->direction, dist);
			}
			else {
				// we have nothing to determine direction, so just pick a random one
				new_target = Utils::calcVector(origin, rand() % 8, dist);
			}
		}
	}

	if (!power->passive && power->can_trigger_passives) {
		src_stats->effects.triggered_active_power = true;
	}

	// logic for different types of powers are very different.  We allow these
	// separate functions to handle the details.
	switch (power->type) {
		case Power::TYPE_FIXED:
			return fixed(power_index, src_stats, origin, new_target);
		case Power::TYPE_MISSILE:
			return missile(power_index, src_stats, origin, new_target);
		case Power::TYPE_REPEATER:
			return repeater(power_index, src_stats, origin, new_target);
		case Power::TYPE_SPAWN:
			return spawn(power_index, src_stats, origin, new_target);
		case Power::TYPE_TRANSFORM:
			return transform(power_index, src_stats, new_target);
	}

	return false;
}

/**
 * pay costs, i.e. remove mana or items.
 */
void PowerManager::payPowerCost(PowerID power_index, StatBlock *src_stats) {
	Power* power = powers[power_index];

	if (src_stats) {
		if (src_stats->hero) {
			src_stats->mp -= power->requires_mp;

			for (size_t i = 0; i < power->requires_resource_stat.size(); ++i) {
				src_stats->resource_stats[i] -= power->requires_resource_stat[i];
			}

			for (size_t i = 0; i < power->required_items.size(); ++i) {
				const PowerRequiredItem pri = power->required_items[i];
				if (pri.id > 0) {
					// only allow one instance of duplicate items at a time in the used_equipped_items queue
					// this is useful for alpha_demo's Ouroboros rings, where we have 2 equipped, but only want to remove one at a time
					if (pri.equipped && std::find(used_equipped_items.begin(), used_equipped_items.end(), pri.id) != used_equipped_items.end()) {
						continue;
					}

					int quantity = pri.quantity;
					while (quantity > 0) {
						if (pri.equipped)
							used_equipped_items.push_back(pri.id);
						else
							used_items.push_back(pri.id);

						quantity--;
					}
				}
			}
		}
		if (power->requires_hp > 0) {
			src_stats->takeDamage(power->requires_hp, !StatBlock::TAKE_DMG_CRIT, Power::SOURCE_TYPE_NEUTRAL);
		}

		// consume corpses
		if (power->requires_corpse && power->remove_corpse && src_stats->target_corpse) {
			src_stats->target_corpse->corpse_timer.reset(Timer::END);
			src_stats->target_corpse = NULL;
		}
	}
}

/**
 * Activate an entity's passive powers
 */
void PowerManager::activatePassives(StatBlock *src_stats) {
	bool activated_passive = false;
	bool triggered_others = false;
	// unlocked powers
	for (unsigned i=0; i<src_stats->powers_passive.size(); i++) {
		activated_passive |= activatePassiveByTrigger(src_stats->powers_passive[i], src_stats, triggered_others);
	}

	// item powers
	for (unsigned i=0; i<src_stats->powers_list_items.size(); i++) {
		activated_passive |= activatePassiveByTrigger(src_stats->powers_list_items[i], src_stats, triggered_others);
	}

	// Only trigger normal passives once
	if (triggered_others)
		src_stats->effects.triggered_others = true;

	// the hit/death triggers can be triggered more than once, so reset them here
	// the block trigger is handled in the Avatar class
	src_stats->effects.triggered_hit = false;
	src_stats->effects.triggered_death = false;

	activatePassivePostPowers(src_stats);

	// passive powers can lock equipment slots, so update equipment here
	if (activated_passive && src_stats->hero)
		menu->inv->applyEquipment();
}

bool PowerManager::activatePassiveByTrigger(PowerID power_id, StatBlock *src_stats, bool& triggered_others) {
	Power* power = powers[power_id];

	if (power->passive) {
		int trigger = power->passive_trigger;

		// if the caster is dead, we can only activate passives that would potentially revive the player or are triggered on death
		if (trigger != Power::TRIGGER_DEATH) {
			if (src_stats->hp == 0) {
				bool is_revive_passive = false;
				for (size_t i = 0; i < power->post_effects.size(); ++i) {
					if (Effect::getTypeFromString(power->post_effects[i].id) == Effect::REVIVE) {
						is_revive_passive = true;
						break;
					}
				}
				if (!is_revive_passive)
					return false;
			}
		}

		if (trigger == -1) {
			if (src_stats->effects.triggered_others)
				return false;
			else
				triggered_others = true;
		}
		else if (trigger == Power::TRIGGER_BLOCK && !src_stats->effects.triggered_block)
			return false;
		else if (trigger == Power::TRIGGER_HIT && !src_stats->effects.triggered_hit)
			return false;
		else if (trigger == Power::TRIGGER_HALFDEATH && !src_stats->effects.triggered_halfdeath) {
			if (src_stats->hp > src_stats->get(Stats::HP_MAX)/2)
				return false;
			else
				src_stats->effects.triggered_halfdeath = true;
		}
		else if (trigger == Power::TRIGGER_JOINCOMBAT && !src_stats->effects.triggered_joincombat) {
			if (!src_stats->in_combat)
				return false;
			else
				src_stats->effects.triggered_joincombat = true;
		}
		else if (trigger == Power::TRIGGER_DEATH && !src_stats->effects.triggered_death) {
			return false;
		}
		else if (trigger == Power::TRIGGER_ACTIVE_POWER && !src_stats->effects.triggered_active_power) {
			return false;
		}

		activate(power_id, src_stats, src_stats->pos, src_stats->pos);
		src_stats->refresh_stats = true;

		for (size_t i = 0; i < power->chain_powers.size(); ++i) {
			ChainPower& chain_power = power->chain_powers[i];
			if (chain_power.type == ChainPower::TYPE_POST) {
				src_stats->setPowerCooldown(chain_power.id, powers[chain_power.id]->cooldown);
			}
		}

		return true;
	}
	return false;
}

/**
 * Activate a single passive
 * this is used when unlocking powers in MenuPowers
 */
void PowerManager::activateSinglePassive(StatBlock *src_stats, PowerID id) {
	if (!isValid(id))
		return;

	Power* power = powers[id];

	if (!power->passive) return;

	if (power->passive_trigger == -1) {
		// if the caster is dead, we can only activate passives that would potentially revive the player
		if (src_stats->hp == 0) {
			bool is_revive_passive = false;
			for (size_t i = 0; i < power->post_effects.size(); ++i) {
				if (Effect::getTypeFromString(power->post_effects[i].id) == Effect::REVIVE) {
					is_revive_passive = true;
					break;
				}
			}
			if (!is_revive_passive)
				return;
		}

		activate(id, src_stats, src_stats->pos, src_stats->pos);
		src_stats->refresh_stats = true;
		src_stats->effects.triggered_others = true;

		for (size_t i = 0; i < power->chain_powers.size(); ++i) {
			ChainPower& chain_power = power->chain_powers[i];
			if (chain_power.type == ChainPower::TYPE_POST) {
				src_stats->setPowerCooldown(chain_power.id, powers[chain_power.id]->cooldown);
			}
		}
	}
}

/**
 * Continually activates post_power for each active passive power
 */
void PowerManager::activatePassivePostPowers(StatBlock *src_stats) {
	for (size_t i = 0; i < src_stats->powers_passive.size(); ++i) {
		if (!src_stats->canUsePower(src_stats->powers_passive[i], StatBlock::CAN_USE_PASSIVE))
			continue;

		Power* passive_power = powers[src_stats->powers_passive[i]];

		for (size_t j = 0; j < passive_power->chain_powers.size(); ++j) {
			ChainPower& chain_power = passive_power->chain_powers[j];
			if (powers[chain_power.id]->new_state != Power::STATE_INSTANT)
				continue;

			// blocking powers use a passive trigger, but we only want to activate their post_power when the blocker takes a hit
			if (passive_power->type == Power::TYPE_BLOCK)
				continue;

			if (src_stats->getPowerCooldown(chain_power.id) == 0 && src_stats->canUsePower(chain_power.id, !StatBlock::CAN_USE_PASSIVE)) {
				if (Math::percentChanceF(chain_power.chance)) {
					activate(chain_power.id, src_stats, src_stats->pos, src_stats->pos);
					src_stats->setPowerCooldown(chain_power.id, powers[chain_power.id]->cooldown);
				}
			}
		}
	}
}

EffectDef* PowerManager::getEffectDef(const std::string& id) {
	for (unsigned i=0; i<effects.size(); ++i) {
		if (effects[i].id == id) {
			return &effects[i];
		}
	}
	return NULL;
}

PowerID PowerManager::verifyID(PowerID power_id, FileParser* infile, bool allow_zero) {
	if ((!allow_zero && power_id == 0) || power_id >= powers.size() || (power_id > 0 && !powers[power_id])) {
		if (infile != NULL)
			infile->error("PowerManager: %d is not a valid power id.", power_id);
		else
			Utils::logError("PowerManager: %d is not a valid power id.", power_id);

		return 0;
	}
	return power_id;
}

bool PowerManager::checkNearestTargeting(const Power* pow, const StatBlock *src_stats, bool check_corpses) {
	if (!src_stats)
		return false;

	if (pow->target_nearest <= 0)
		return true;

	if (!check_corpses && src_stats->target_nearest && pow->target_nearest > src_stats->target_nearest_dist)
		return true;
	else if (check_corpses && src_stats->target_nearest_corpse && pow->target_nearest > src_stats->target_nearest_corpse_dist)
		return true;

	return false;
}

bool PowerManager::checkRequiredItems(const Power* pow, const StatBlock *src_stats) {
	for (size_t i = 0; i < pow->required_items.size(); ++i) {
		if (pow->required_items[i].id > 0) {
			if (pow->required_items[i].equipped) {
				if (!menu->inv->equipmentContain(pow->required_items[i].id, 1)) {
					return false;
				}
			}
			else {
				if (!items->requirementsMet(src_stats, pow->required_items[i].id)) {
					return false;
				}

				// Cap the lower bound of quantity to 1.
				// We can set required quantity to 0 in order to not consume the item,
				// but checking for presence of the item requires >0 quantity
				int quantity = std::max(1, pow->required_items[i].quantity);
				if (!menu->inv->inventory[MenuInventory::CARRIED].contain(pow->required_items[i].id, quantity)) {
					return false;
				}
			}
		}
	}

	return true;
}

bool PowerManager::checkRequiredResourceState(const Power* pow, const StatBlock *src_stats) {
	bool hp_ok = true;
	bool mp_ok = true;

	if (pow->requires_hp_state.state == Power::RESOURCESTATE_PERCENT && src_stats->hp * 100 < (src_stats->get(Stats::HP_MAX) * pow->requires_hp_state.value))
		hp_ok = false;
	else if (pow->requires_hp_state.state == Power::RESOURCESTATE_NOT_PERCENT && src_stats->hp * 100 >= (src_stats->get(Stats::HP_MAX) * pow->requires_hp_state.value))
		hp_ok = false;
	else if (pow->requires_hp_state.state == Power::RESOURCESTATE_PERCENT_EXACT && src_stats->hp * 100 != (src_stats->get(Stats::HP_MAX) * pow->requires_hp_state.value))
		hp_ok = false;

	if (pow->requires_mp_state.state == Power::RESOURCESTATE_PERCENT && src_stats->mp * 100 < (src_stats->get(Stats::MP_MAX) * pow->requires_mp_state.value))
		mp_ok = false;
	else if (pow->requires_mp_state.state == Power::RESOURCESTATE_NOT_PERCENT && src_stats->mp * 100 >= (src_stats->get(Stats::MP_MAX) * pow->requires_mp_state.value))
		mp_ok = false;
	else if (pow->requires_mp_state.state == Power::RESOURCESTATE_PERCENT_EXACT && src_stats->mp * 100 != (src_stats->get(Stats::MP_MAX) * pow->requires_mp_state.value))
		mp_ok = false;

	bool hpmp_ok = true;
	if (pow->requires_hpmp_state_mode == Power::RESOURCESTATE_ALL)
		hpmp_ok = hp_ok && mp_ok;
	else if (pow->requires_hpmp_state_mode == Power::RESOURCESTATE_ANY)
		hpmp_ok = hp_ok || mp_ok;

	bool resource_stat_ok = true;
	size_t resource_stat_enabled_count = 0;
	size_t resource_stat_fail_count = 0;

	for (size_t i = 0; i < pow->requires_resource_stat_state.size(); ++i) {
		if (pow->requires_resource_stat_state[i].state != Power::RESOURCESTATE_IGNORE)
			resource_stat_enabled_count++;
		else
			continue;

		float resource_max = src_stats->getResourceStat(i, EngineSettings::ResourceStats::STAT_BASE);
		if (pow->requires_resource_stat_state[i].state == Power::RESOURCESTATE_PERCENT && src_stats->resource_stats[i] * 100 < resource_max * pow->requires_resource_stat_state[i].value)
			resource_stat_fail_count++;
		else if (pow->requires_resource_stat_state[i].state == Power::RESOURCESTATE_NOT_PERCENT && src_stats->resource_stats[i] * 100 >= resource_max * pow->requires_resource_stat_state[i].value)
			resource_stat_fail_count++;
		else if (pow->requires_resource_stat_state[i].state == Power::RESOURCESTATE_PERCENT_EXACT && src_stats->resource_stats[i] * 100 != resource_max * pow->requires_resource_stat_state[i].value)
			resource_stat_fail_count++;
	}

	if (resource_stat_enabled_count > 0) {
		if (pow->requires_resource_stat_state_mode == Power::RESOURCESTATE_ANY_HPMP && !hpmp_ok && resource_stat_fail_count == resource_stat_enabled_count)
			resource_stat_ok = false;
		else if (pow->requires_resource_stat_state_mode == Power::RESOURCESTATE_ANY && resource_stat_fail_count == resource_stat_enabled_count)
			resource_stat_ok = false;
		else if (pow->requires_resource_stat_state_mode == Power::RESOURCESTATE_ALL && resource_stat_fail_count > 0)
			resource_stat_ok = false;
	}

	if (pow->requires_resource_stat_state_mode == Power::RESOURCESTATE_ANY_HPMP)
		return hpmp_ok || resource_stat_ok;
	else
		return hpmp_ok && resource_stat_ok;
}

bool PowerManager::checkCombatRange(PowerID power_index, StatBlock* src_stats, FPoint target) {
	if (!isValid(power_index))
		return false;

	const Power* pow = powers[power_index];

	if (pow->combat_range == 0)
		return false;

	float combat_range = pow->combat_range;
	float target_range = pow->target_range + (pow->radius / 2.f);

	if (pow->starting_pos == Power::STARTING_POS_MELEE) {
		combat_range += src_stats->melee_range;
	}

	if (pow->target_range > 0 && target_range < combat_range) {
		combat_range = target_range;
	}

	return Utils::calcDist(src_stats->pos, target) <= combat_range;
}

bool PowerManager::checkPowerCost(const Power* pow, const StatBlock *src_stats) {
	if (src_stats->mp < pow->requires_mp)
		return false;

	if (!pow->sacrifice && src_stats->hp < pow->requires_hp)
		return false;

	for (size_t i = 0; i < pow->requires_resource_stat.size(); ++i) {
		if (src_stats->resource_stats[i] < pow->requires_resource_stat[i])
			return false;
	}

	return true;
}

PowerManager::~PowerManager() {
	for (size_t i = 0; i < powers.size(); ++i) {
		if (!powers[i])
			continue;

		if (!powers[i]->animation_name.empty()) {
			anim->decreaseCount(powers[i]->animation_name);
		}

		delete powers[i];
		delete power_animations[i];
	}

	for (size_t i = 0; i < effects.size(); ++i) {
		if (effects[i].animation.empty())
			continue;

		anim->decreaseCount(effects[i].animation);

		if (effect_animations[i])
			delete effect_animations[i];
	}

	for (size_t i = 0; i < sfx.size(); i++) {
		snd->unload(sfx[i]);
	}
	sfx.clear();

	while (!hazards.empty()) {
		delete hazards.front();
		hazards.pop();
	}
}

