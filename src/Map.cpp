/*
Copyright © 2011-2012 Clint Bellanger
Copyright © 2012-2013 Stefan Beller
Copyright © 2013 Henrik Andersson
Copyright © 2013-2016 Justin Jacobs

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


#include "CampaignManager.h"
#include "EffectManager.h"
#include "EngineSettings.h"
#include "EventManager.h"
#include "FileParser.h"
#include "FogOfWar.h"
#include "Map.h"
#include "MapRenderer.h"
#include "MessageEngine.h"
#include "ModManager.h"
#include "PowerManager.h"
#include "SaveLoad.h"
#include "Settings.h"
#include "SharedResources.h"
#include "SharedGameResources.h"
#include "StatBlock.h"
#include "Utils.h"
#include "UtilsParsing.h"

Map::Map()
	: filename("")
	, layers()
	, events()
	, w(1)
	, h(1)
	, hero_pos_enabled(false)
	, hero_pos()
	, background_color(0,0,0,0)
	, fogofwar(eset->misc.fogofwar)
	, save_fogofwar(eset->misc.save_fogofwar) {
}

Map::~Map() {
	clearLayers();
}

void Map::clearLayers() {
	layers.clear();
	layernames.clear();
}

void Map::clearQueues() {
	enemies = std::queue<Map_Enemy>();
	map_npcs = std::queue<Map_NPC>();
}

void Map::clearEvents() {
	events.clear();
	delayed_events.clear();
	statblocks.clear();
}

void Map::removeLayer(unsigned index) {
	layernames.erase(layernames.begin() + index);
	layers.erase(layers.begin() + index);
}

int Map::load(const std::string& fname) {
	FileParser infile;

	clearEvents();
	clearLayers();
	clearQueues();

	music_filename = "";
	parallax_filename = "";
	background_color = Color(0,0,0,0);
	fogofwar = eset->misc.fogofwar;
	save_fogofwar = eset->misc.save_fogofwar;

	w = 1;
	h = 1;
	hero_pos_enabled = false;
	hero_pos.x = 0;
	hero_pos.y = 0;

	// @CLASS Map|Description of maps/
	if (!infile.open(fname, FileParser::MOD_FILE, FileParser::ERROR_NORMAL))
		return 0;

	Utils::logInfo("Map: Loading map '%s'", fname.c_str());

	this->filename = fname;

	while (infile.next()) {
		if (infile.new_section) {

			// for sections that are stored in collections, add a new object here
			if (infile.section == "enemy")
				enemy_groups.push(Map_Group());
			else if (infile.section == "npc")
				map_npcs.push(Map_NPC());
			else if (infile.section == "event")
				events.push_back(Event());

		}
		if (infile.section == "header")
			loadHeader(infile);
		else if (infile.section == "layer")
			loadLayer(infile);
		else if (infile.section == "enemy")
			loadEnemyGroup(infile, &enemy_groups.back());
		else if (infile.section == "npc")
			loadNPC(infile);
		else if (infile.section == "event")
			eventm->loadEvent(infile, &events.back());
	}

	infile.close();

	if (fogofwar) fow->load();

	// create StatBlocks for events that need powers
	for (unsigned i=0; i<events.size(); ++i) {
		EventComponent *ec_power = events[i].getComponent(EventComponent::POWER);
		if (ec_power) {
			// store the index of this StatBlock so that we can find it when the event is activated
			ec_power->data[0].Int = addEventStatBlock(events[i]);
		}
	}

	// ensure that our map contains a collision layer
	if (std::find(layernames.begin(), layernames.end(), "collision") == layernames.end()) {
		layernames.push_back("collision");
		layers.resize(layers.size()+1);
		layers.back().resize(w);
		for (size_t i=0; i<layers.back().size(); ++i) {
			layers.back()[i].resize(h, 0);
		}
	}

	// ensure that our map contains a fog of war layer
	if (fogofwar) {
		if (save_fogofwar) {
			std::stringstream ss;
			ss.str("");
			ss << settings->path_user << "saves/" << eset->misc.save_prefix << "/" << save_load->getGameSlot() << "/fow/" << Utils::hashString(mapr->getFilename()) << ".txt";
			if (infile.open(ss.str(), !FileParser::MOD_FILE, FileParser::ERROR_NORMAL)) {
				while (infile.next()) {
					if (infile.section == "layer") {
						if (!loadLayer(infile, !EXIT_ON_FAIL)) {
							for (size_t i = layers.size(); i > 0; i--) {
								size_t layer_index = i-1;
								if (layernames[layer_index] == "fow_fog") {
									layernames.erase(layernames.begin() + layer_index);
									layers.erase(layers.begin() + layer_index);
								}
								else if (layernames[layer_index] == "fow_dark") {
									layernames.erase(layernames.begin() + layer_index);
									layers.erase(layers.begin() + layer_index);
								}
							}
							break;
						}
					}
				}
				infile.close();
			}
		}
		if (std::find(layernames.begin(), layernames.end(), "fow_fog") == layernames.end()) {
			layernames.push_back("fow_fog");
			layers.resize(layers.size()+1);
			layers.back().resize(w);
			for (size_t i=0; i<layers.back().size(); ++i) {
				layers.back()[i].resize(h, FogOfWar::TILE_HIDDEN);
			}
		}

		if (std::find(layernames.begin(), layernames.end(), "fow_dark") == layernames.end()) {
			layernames.push_back("fow_dark");
			layers.resize(layers.size()+1);
			layers.back().resize(w);
			for (size_t i=0; i<layers.back().size(); ++i) {
				layers.back()[i].resize(h, FogOfWar::TILE_HIDDEN);
			}
		}
	}

	if (!hero_pos_enabled) {
		Utils::logError("Map: Hero spawn position (hero_pos) not defined in map header. Defaulting to (0,0).");
	}

	return 0;
}

void Map::loadHeader(FileParser &infile) {
	if (infile.key == "title") {
		// @ATTR title|string|Title of map
		this->title = msg->get(infile.val);
	}
	else if (infile.key == "width") {
		// @ATTR width|int|Width of map
		this->w = static_cast<unsigned short>(std::max(Parse::toInt(infile.val), 1));
	}
	else if (infile.key == "height") {
		// @ATTR height|int|Height of map
		this->h = static_cast<unsigned short>(std::max(Parse::toInt(infile.val), 1));
	}
	else if (infile.key == "tileset") {
		// @ATTR tileset|filename|Filename of a tileset definition to use for map
		this->tileset = infile.val;
	}
	else if (infile.key == "music") {
		// @ATTR music|filename|Filename of background music to use for map
		music_filename = infile.val;
	}
	else if (infile.key == "hero_pos") {
		// @ATTR hero_pos|point|The player will spawn in this location if no point was previously given.
		hero_pos.x = static_cast<float>(Parse::popFirstInt(infile.val)) + 0.5f;
		hero_pos.y = static_cast<float>(Parse::popFirstInt(infile.val)) + 0.5f;
		hero_pos_enabled = true;
	}
	else if (infile.key == "parallax_layers") {
		// @ATTR parallax_layers|filename|Filename of a parallax layers definition.
		parallax_filename = infile.val;
	}
	else if (infile.key == "background_color") {
		// @ATTR background_color|color, int : Color, alpha|Background color for the map.
		background_color = Parse::toRGBA(infile.val);
	}
	else if (infile.key == "fogofwar") {
		// @ATTR fogofwar|int|Set the fog of war type. 0-disabled, 1-minimap, 2-tint, 3-overlay. Overrides engine settings.
		fogofwar = static_cast<unsigned short>(Parse::toInt(infile.val));
	}
	else if (infile.key == "save_fogofwar") {
		// @ATTR save_fogofwar|bool|If true, the fog of war layer keeps track of the progress. Overrides engine settings.
		save_fogofwar = Parse::toBool(infile.val);
	}
	else if (infile.key == "tilewidth") {
		// @ATTR tilewidth|int|Inherited from Tiled map file. Unused by engine.
	}
	else if (infile.key == "tileheight") {
		// @ATTR tileheight|int|Inherited from Tiled map file. Unused by engine.
	}
	else if (infile.key == "orientation") {
		// this is only used by Tiled when importing Flare maps
	}
	else {
		infile.error("Map: '%s' is not a valid key.", infile.key.c_str());
	}
}

bool Map::loadLayer(FileParser &infile, bool exit_on_fail) {
	if (infile.key == "type") {
		// @ATTR layer.type|string|Map layer type.
		layers.resize(layers.size()+1);
		layers.back().resize(w);
		for (size_t i=0; i<layers.back().size(); ++i) {
			layers.back()[i].resize(h);
		}
		layernames.push_back(infile.val);
	}
	else if (infile.key == "format") {
		// @ATTR layer.format|string|Format for map layer, must be 'dec'
		if (infile.val != "dec") {
			infile.error("Map: The format of a layer must be 'dec'!");
			if (exit_on_fail) {
				Utils::logErrorDialog("Map: The format of a layer must be 'dec'!");
				mods->resetModConfig();
				Utils::Exit(1);
			}
			return false;
		}
	}
	else if (infile.key == "data") {
		// @ATTR layer.data|raw|Raw map layer data
		// layer map data handled as a special case
		// The next h lines must contain layer data.
		for (int j=0; j<h; j++) {
			std::string val = infile.getRawLine();
			infile.incrementLineNum();
			if (!val.empty() && val[val.length()-1] != ',') {
				val += ',';
			}

			// verify the width of this row
			int comma_count = 0;
			for (unsigned i=0; i<val.length(); ++i) {
				if (val[i] == ',') comma_count++;
			}
			if (comma_count != w) {
				infile.error("Map: A row of layer data has a width not equal to %d.", w);
				if (exit_on_fail) {
					mods->resetModConfig();
					Utils::Exit(1);
				}
				return false;
			}

			for (int i=0; i<w; i++)
				layers.back()[i][j] = static_cast<unsigned short>(Parse::popFirstInt(val));
		}
	}
	else {
		infile.error("Map: '%s' is not a valid key.", infile.key.c_str());
	}

	return true;
}

void Map::loadEnemyGroup(FileParser &infile, Map_Group *group) {
	if (infile.key == "type") {
		// @ATTR enemygroup.type|string|(IGNORED BY ENGINE) The "type" field, as used by Tiled and other mapping tools.
		group->type = infile.val;
	}
	else if (infile.key == "category") {
		// @ATTR enemygroup.category|predefined_string|The category of enemies that will spawn in this group.
		group->category = infile.val;
	}
	else if (infile.key == "level") {
		// @ATTR enemygroup.level|int, int : Min, Max|Defines the level range of enemies in group. If only one number is given, it's the exact level.
		group->levelmin = std::max(0, Parse::popFirstInt(infile.val));
		group->levelmax = std::max(std::max(0, Parse::toInt(Parse::popFirstString(infile.val))), group->levelmin);
	}
	else if (infile.key == "location") {
		// @ATTR enemygroup.location|rectangle|Location area for enemygroup
		group->pos.x = Parse::popFirstInt(infile.val);
		group->pos.y = Parse::popFirstInt(infile.val);
		group->area.x = Parse::popFirstInt(infile.val);
		group->area.y = Parse::popFirstInt(infile.val);
	}
	else if (infile.key == "number") {
		// @ATTR enemygroup.number|int, int : Min, Max|Defines the range of enemies in group. If only one number is given, it's the exact amount.
		group->numbermin = std::max(0, Parse::popFirstInt(infile.val));
		group->numbermax = std::max(std::max(0, Parse::toInt(Parse::popFirstString(infile.val))), group->numbermin);
	}
	else if (infile.key == "chance") {
		// @ATTR enemygroup.chance|float|Initial percentage chance that this enemy group will be able to spawn enemies.
		group->chance = std::min(100.0f, std::max(0.0f, Parse::popFirstFloat(infile.val)));
	}
	else if (infile.key == "direction") {
		// @ATTR enemygroup.direction|direction|Direction that enemies will initially face.
		group->direction = Parse::toDirection(infile.val);
	}
	else if (infile.key == "waypoints") {
		// @ATTR enemygroup.waypoints|list(point)|Enemy waypoints; single enemy only; negates wander_radius
		std::string a = Parse::popFirstString(infile.val);
		std::string b = Parse::popFirstString(infile.val);

		while (!a.empty()) {
			FPoint p;
			p.x = static_cast<float>(Parse::toInt(a)) + 0.5f;
			p.y = static_cast<float>(Parse::toInt(b)) + 0.5f;
			group->waypoints.push(p);
			a = Parse::popFirstString(infile.val);
			b = Parse::popFirstString(infile.val);
		}

		// disable wander radius, since we can't have waypoints and wandering at the same time
		group->wander_radius = 0;
	}
	else if (infile.key == "wander_radius") {
		// @ATTR enemygroup.wander_radius|int|The radius (in tiles) that an enemy will wander around randomly; negates waypoints
		group->wander_radius = std::max(0, Parse::popFirstInt(infile.val));

		// clear waypoints, since wandering will use the waypoint queue
		while (!group->waypoints.empty()) {
			group->waypoints.pop();
		}
	}
	else if (infile.key == "requires_status") {
		// @ATTR enemygroup.requires_status|list(string)|Statuses required to be set for enemy group to load
		std::string s;
		while ( (s = Parse::popFirstString(infile.val)) != "") {
			EventComponent ec;
			ec.type = EventComponent::REQUIRES_STATUS;
			ec.status = camp->registerStatus(s);
			group->requirements.push_back(ec);
		}
	}
	else if (infile.key == "requires_not_status") {
		// @ATTR enemygroup.requires_not_status|list(string)|Statuses required to be unset for enemy group to load
		std::string s;
		while ( (s = Parse::popFirstString(infile.val)) != "") {
			EventComponent ec;
			ec.type = EventComponent::REQUIRES_NOT_STATUS;
			ec.status = camp->registerStatus(s);
			group->requirements.push_back(ec);
		}
	}
	else if (infile.key == "requires_level") {
		// @ATTR enemygroup.requires_level|int|Player level must be equal or greater to load enemy group
		EventComponent ec;
		ec.type = EventComponent::REQUIRES_LEVEL;
		ec.data[0].Int = Parse::popFirstInt(infile.val);
		group->requirements.push_back(ec);
	}
	else if (infile.key == "requires_not_level") {
		// @ATTR enemygroup.requires_not_level|int|Player level must be lesser to load enemy group
		EventComponent ec;
		ec.type = EventComponent::REQUIRES_NOT_LEVEL;
		ec.data[0].Int = Parse::popFirstInt(infile.val);
		group->requirements.push_back(ec);
	}
	else if (infile.key == "requires_currency") {
		// @ATTR enemygroup.requires_currency|int|Player currency must be equal or greater to load enemy group
		EventComponent ec;
		ec.type = EventComponent::REQUIRES_CURRENCY;
		ec.data[0].Int = Parse::popFirstInt(infile.val);
		group->requirements.push_back(ec);
	}
	else if (infile.key == "requires_not_currency") {
		// @ATTR enemygroup.requires_not_currency|int|Player currency must be lesser to load enemy group
		EventComponent ec;
		ec.type = EventComponent::REQUIRES_NOT_CURRENCY;
		ec.data[0].Int = Parse::popFirstInt(infile.val);
		group->requirements.push_back(ec);
	}
	else if (infile.key == "requires_item") {
		// @ATTR enemygroup.requires_item|list(item_id)|Item required to exist in player inventory to load enemy group. Quantity can be specified by appending ":Q" to the item_id, where Q is an integer.
		std::string s;
		while ( (s = Parse::popFirstString(infile.val)) != "") {
			ItemStack item_stack = Parse::toItemQuantityPair(s);
			EventComponent ec;
			ec.type = EventComponent::REQUIRES_ITEM;
			ec.id = item_stack.item;
			ec.data[0].Int = item_stack.quantity;
			group->requirements.push_back(ec);
		}
	}
	else if (infile.key == "requires_not_item") {
		// @ATTR enemygroup.requires_not_item|list(item_id)|Item required to not exist in player inventory to load enemy group. Quantity can be specified by appending ":Q" to the item_id, where Q is an integer.
		std::string s;
		while ( (s = Parse::popFirstString(infile.val)) != "") {
			ItemStack item_stack = Parse::toItemQuantityPair(s);
			EventComponent ec;
			ec.type = EventComponent::REQUIRES_NOT_ITEM;
			ec.id = item_stack.item;
			ec.data[0].Int = item_stack.quantity;
			group->requirements.push_back(ec);
		}
	}
	else if (infile.key == "requires_class") {
		// @ATTR enemygroup.requires_class|predefined_string|Player base class required to load enemy group
		EventComponent ec;
		ec.type = EventComponent::REQUIRES_CLASS;
		ec.s = Parse::popFirstString(infile.val);
		group->requirements.push_back(ec);
	}
	else if (infile.key == "requires_not_class") {
		// @ATTR enemygroup.requires_not_class|predefined_string|Player base class not required to load enemy group
		EventComponent ec;
		ec.type = EventComponent::REQUIRES_NOT_CLASS;
		ec.s = Parse::popFirstString(infile.val);
		group->requirements.push_back(ec);
	}
	else if (infile.key == "invincible_requires_status") {
		// @ATTR enemygroup.invincible_requires_status|list(string)|Enemies in this group are invincible to hero attacks when these statuses are set.
		std::string s;
		while ((s = Parse::popFirstString(infile.val)) != "") {
			EventComponent ec;
			ec.type = EventComponent::REQUIRES_STATUS;
			ec.status = camp->registerStatus(s);
			group->invincible_requirements.push_back(ec);
		}
	}
	else if (infile.key == "invincible_requires_not_status") {
		// @ATTR enemygroup.invincible_requires_not_status|list(string)|Enemies in this group are invincible to hero attacks when these statuses are not set.
		std::string s;
		while ((s = Parse::popFirstString(infile.val)) != "") {
			EventComponent ec;
			ec.type = EventComponent::REQUIRES_NOT_STATUS;
			ec.status = camp->registerStatus(s);
			group->invincible_requirements.push_back(ec);
		}
	}
	else if (infile.key == "spawn_level") {
		// @ATTR enemygroup.spawn_level|["default", "fixed", "level", "stat"], int, int, predefined_string : Mode, Enemy Level, Ratio, Primary stat|The level of spawned creatures. The need for the last three parameters depends on the mode being used. The "default" mode will just use the entity's normal level and doesn't require any additional parameters. The "fixed" mode only requires the enemy level as a parameter. The "stat" and "level" modes also require the ratio as a parameter. The ratio adjusts the scaling of the entity level. For example, spawn_level=stat,1,2,physical will set the spawned entity level to 1/2 the player's Physical stat. Only the "stat" mode requires the last parameter, which is simply the ID of the primary stat that should be used for scaling.
		std::string mode = Parse::popFirstString(infile.val);
		if (mode == "default") group->spawn_level.mode = SpawnLevel::MODE_DEFAULT;
		else if (mode == "fixed") group->spawn_level.mode = SpawnLevel::MODE_FIXED;
		else if (mode == "stat") group->spawn_level.mode = SpawnLevel::MODE_STAT;
		else if (mode == "level") group->spawn_level.mode = SpawnLevel::MODE_LEVEL;
		else infile.error("Map: Unknown spawn level mode '%s'", mode.c_str());

		if (group->spawn_level.mode != SpawnLevel::MODE_DEFAULT) {
			group->spawn_level.count = static_cast<float>(Parse::popFirstInt(infile.val));

			if(group->spawn_level.mode != SpawnLevel::MODE_FIXED) {
				group->spawn_level.ratio = Parse::popFirstFloat(infile.val);

				if(group->spawn_level.mode == SpawnLevel::MODE_STAT) {
					std::string stat = Parse::popFirstString(infile.val);
					size_t prim_stat_index = eset->primary_stats.getIndexByID(stat);

					if (prim_stat_index != eset->primary_stats.list.size()) {
						group->spawn_level.stat = prim_stat_index;
					}
					else {
						infile.error("Map: '%s' is not a valid primary stat.", stat.c_str());
					}
				}
			}
		}
	}
	else {
		infile.error("Map: '%s' is not a valid key.", infile.key.c_str());
	}
}

void Map::loadNPC(FileParser &infile) {
	if (infile.key == "type") {
		// @ATTR npc.type|string|(IGNORED BY ENGINE) The "type" field, as used by Tiled and other mapping tools.
		map_npcs.back().type = infile.val;
	}
	else if (infile.key == "filename") {
		// @ATTR npc.filename|string|Filename of an NPC definition.
		map_npcs.back().id = infile.val;
	}
	else if (infile.key == "location") {
		// @ATTR npc.location|point|Location of NPC
		map_npcs.back().pos.x = static_cast<float>(Parse::popFirstInt(infile.val)) + 0.5f;
		map_npcs.back().pos.y = static_cast<float>(Parse::popFirstInt(infile.val)) + 0.5f;
	}
	else if (infile.key == "requires_status") {
		// @ATTR npc.requires_status|list(string)|Statuses required to be set for NPC load
		std::string s;
		while ( (s = Parse::popFirstString(infile.val)) != "") {
			EventComponent ec;
			ec.type = EventComponent::REQUIRES_STATUS;
			ec.status = camp->registerStatus(s);
			map_npcs.back().requirements.push_back(ec);
		}
	}
	else if (infile.key == "requires_not_status") {
		// @ATTR npc.requires_not_status|list(string)|Statuses required to be unset for NPC load
		std::string s;
		while ( (s = Parse::popFirstString(infile.val)) != "") {
			EventComponent ec;
			ec.type = EventComponent::REQUIRES_NOT_STATUS;
			ec.status = camp->registerStatus(s);
			map_npcs.back().requirements.push_back(ec);
		}
	}
	else if (infile.key == "requires_level") {
		// @ATTR npc.requires_level|int|Player level must be equal or greater to load NPC
		EventComponent ec;
		ec.type = EventComponent::REQUIRES_LEVEL;
		ec.data[0].Int = Parse::popFirstInt(infile.val);
		map_npcs.back().requirements.push_back(ec);
	}
	else if (infile.key == "requires_not_level") {
		// @ATTR npc.requires_not_level|int|Player level must be lesser to load NPC
		EventComponent ec;
		ec.type = EventComponent::REQUIRES_NOT_LEVEL;
		ec.data[0].Int = Parse::popFirstInt(infile.val);
		map_npcs.back().requirements.push_back(ec);
	}
	else if (infile.key == "requires_currency") {
		// @ATTR npc.requires_currency|int|Player currency must be equal or greater to load NPC
		EventComponent ec;
		ec.type = EventComponent::REQUIRES_CURRENCY;
		ec.data[0].Int = Parse::popFirstInt(infile.val);
		map_npcs.back().requirements.push_back(ec);
	}
	else if (infile.key == "requires_not_currency") {
		// @ATTR npc.requires_not_currency|int|Player currency must be lesser to load NPC
		EventComponent ec;
		ec.type = EventComponent::REQUIRES_NOT_CURRENCY;
		ec.data[0].Int = Parse::popFirstInt(infile.val);
		map_npcs.back().requirements.push_back(ec);
	}
	else if (infile.key == "requires_item") {
		// @ATTR npc.requires_item|list(item_id)|Item required to exist in player inventory to load NPC. Quantity can be specified by appending ":Q" to the item_id, where Q is an integer.
		std::string s;
		while ( (s = Parse::popFirstString(infile.val)) != "") {
			ItemStack item_stack = Parse::toItemQuantityPair(s);
			EventComponent ec;
			ec.type = EventComponent::REQUIRES_ITEM;
			ec.id = item_stack.item;
			ec.data[0].Int = item_stack.quantity;
			map_npcs.back().requirements.push_back(ec);
		}
	}
	else if (infile.key == "requires_not_item") {
		// @ATTR npc.requires_not_item|list(item_id)|Item required to not exist in player inventory to load NPC. Quantity can be specified by appending ":Q" to the item_id, where Q is an integer.
		std::string s;
		while ( (s = Parse::popFirstString(infile.val)) != "") {
			ItemStack item_stack = Parse::toItemQuantityPair(s);
			EventComponent ec;
			ec.type = EventComponent::REQUIRES_NOT_ITEM;
			ec.id = item_stack.item;
			ec.data[0].Int = item_stack.quantity;
			map_npcs.back().requirements.push_back(ec);
		}
	}
	else if (infile.key == "requires_class") {
		// @ATTR npc.requires_class|predefined_string|Player base class required to load NPC
		EventComponent ec;
		ec.type = EventComponent::REQUIRES_CLASS;
		ec.s = Parse::popFirstString(infile.val);
		map_npcs.back().requirements.push_back(ec);
	}
	else if (infile.key == "requires_not_class") {
		// @ATTR npc.requires_not_class|predefined_string|Player base class not required to load NPC
		EventComponent ec;
		ec.type = EventComponent::REQUIRES_NOT_CLASS;
		ec.s = Parse::popFirstString(infile.val);
		map_npcs.back().requirements.push_back(ec);
	}
	else if (infile.key == "direction") {
		// @ATTR npc.direction|direction|Direction that NPC will initially face.
		map_npcs.back().direction = Parse::toDirection(infile.val);
	}
	else if (infile.key == "waypoints") {
		// @ATTR npc.waypoints|list(point)|NPC waypoints; negates wander_radius
		std::string a = Parse::popFirstString(infile.val);
		std::string b = Parse::popFirstString(infile.val);

		while (!a.empty()) {
			FPoint p;
			p.x = static_cast<float>(Parse::toInt(a)) + 0.5f;
			p.y = static_cast<float>(Parse::toInt(b)) + 0.5f;
			map_npcs.back().waypoints.push(p);
			a = Parse::popFirstString(infile.val);
			b = Parse::popFirstString(infile.val);
		}

		// disable wander radius, since we can't have waypoints and wandering at the same time
		map_npcs.back().wander_radius = 0;
	}
	else if (infile.key == "wander_radius") {
		// @ATTR npc.wander_radius|int|The radius (in tiles) that an NPC will wander around randomly; negates waypoints
		map_npcs.back().wander_radius = std::max(0, Parse::popFirstInt(infile.val));

		// clear waypoints, since wandering will use the waypoint queue
		while (!map_npcs.back().waypoints.empty()) {
			map_npcs.back().waypoints.pop();
		}
	}
	else {
		infile.error("Map: '%s' is not a valid key.", infile.key.c_str());
	}
}

int Map::addEventStatBlock(Event &evnt) {
	statblocks.push_back(StatBlock());
	StatBlock *statb = &statblocks.back();

	statb->perfect_accuracy = true; // never miss AND never overhit

	EventComponent *ec_path = evnt.getComponent(EventComponent::POWER_PATH);
	if (ec_path) {
		// source is power path start
		statb->pos.x = static_cast<float>(ec_path->data[0].Int) + 0.5f;
		statb->pos.y = static_cast<float>(ec_path->data[1].Int) + 0.5f;
	}
	else {
		// source is event location
		statb->pos.x = static_cast<float>(evnt.location.x) + 0.5f;
		statb->pos.y = static_cast<float>(evnt.location.y) + 0.5f;
	}

	EventComponent *ec_damage = evnt.getComponent(EventComponent::POWER_DAMAGE);
	if (ec_damage) {
		for (size_t i = 0; i < eset->damage_types.list.size(); ++i) {
			statb->starting[Stats::COUNT + eset->damage_types.indexToMin(i)] = ec_damage->data[0].Float; // min
			statb->starting[Stats::COUNT + eset->damage_types.indexToMax(i)] = ec_damage->data[1].Float; // min
		}
	}

	// this is used to store cooldown ticks for a map power
	// the power id, type, etc are not used
	statb->powers_ai.resize(1);

	// make this StatBlock immune to negative status effects
	// this is mostly to prevent a player with a damage return bonus from damaging this StatBlock
	// create a temporary EffectDef for immunity; will be used for map StatBlocks
	EffectDef immunity_effect;
	immunity_effect.id = "MAP_EVENT_IMMUNITY";
	immunity_effect.type = Effect::RESIST_ALL;

	EffectParams immunity_params;
	immunity_params.magnitude = 100;
	immunity_params.source_type = Power::SOURCE_TYPE_ENEMY;

	statb->effects.addEffect(statb, immunity_effect, immunity_params);

	// ensure the statblock will be alive
	statb->hp = statb->starting[Stats::HP_MAX] = statb->current[Stats::HP_MAX] = 1;

	// ensure that stats are ready to be used by running logic once
	statb->logic();

	return static_cast<int>(statblocks.size())-1;
}
