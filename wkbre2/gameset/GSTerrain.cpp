// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "GSTerrain.h"
#include "gameset.h"
#include "util/GSFileParser.h"

void GSTerrain::parse(GSFileParser& gsf, GameSet& gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		auto tag = gsf.nextTag();
		if (tag == "STARTS_WITH_ITEM") {
			int item = gs.items.readIndex(gsf);
			itemValues[item] = gsf.nextFloat();
		}
		else if (tag == "ASSOCIATE_TILE_TEXTURE_GROUP") {
			auto grp = gsf.nextString(true);
			associatedGroups.push_back(grp);
			gs.associatedTileTexGroups[grp] = this;
		}
		else if (tag == "TERRAIN_COST") {
			terrainCost = gsf.nextFloat();
		}
		else if (tag == "END_TERRAIN")
			break;
		gsf.advanceLine();
	}
}
