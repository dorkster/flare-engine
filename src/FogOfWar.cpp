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
 * class FogOfWar
 *
 * Contains logic and rendering routines for fog of war.
 */

#include "Avatar.h"
#include "FogOfWar.h"
#include "MapRenderer.h"
#include "SharedGameResources.h"

FogOfWar::FogOfWar()
	: layer_id(0) {	
}

void FogOfWar::logic() {
	short start_x;
	short start_y;
	short end_x;
	short end_y;
	start_x = static_cast<short>(pc->stats.pos.x-pc->sight-2);
	start_y = static_cast<short>(pc->stats.pos.y-pc->sight-2);
	end_x = static_cast<short>(pc->stats.pos.x+pc->sight+2);
	end_y = static_cast<short>(pc->stats.pos.y+pc->sight+2);

	if (start_x < 0) start_x = 0;
	if (start_y < 0) start_y = 0;
	if (end_x > mapr->w) end_x = mapr->w;
	if (end_y > mapr->h) end_y = mapr->h;

	for (unsigned short lx = start_x; lx < end_x; lx++) {
		for (unsigned short ly = start_y; ly < end_y; ly++) {
			Point lPoint(lx, ly);									
			float delta = Utils::calcDist(FPoint(lPoint), FPoint(pc->stats.pos));
			if (delta < pc->sight) {
				mapr->layers[layer_id][lx][ly] = 0;
			}
			else if (mapr->layers[layer_id][lx][ly] == 0) {
				mapr->layers[layer_id][lx][ly] = 2;
			}
		}
	}
}

FogOfWar::~FogOfWar() {
}
