#include "Footprint.h"
#include "gameset.h"
#include "../util/GSFileParser.h"

void Footprint::parse(GSFileParser& gsf, GameSet& gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		auto tag = gsf.nextTag();
		if (tag == "ORIGIN") {
			originX = gsf.nextFloat();
			originZ = gsf.nextFloat();
		}
		else if (tag == "TILE_OFFSET") {
			int ox = gsf.nextInt();
			int oz = gsf.nextInt();
			tiles.emplace_back(ox, oz);
			auto& tile = tiles.back();
			gsf.advanceLine();
			while (!gsf.eof) {
				auto tag = gsf.nextTag();
				if (tag == "SET_PATHFINDING") {
					auto word = gsf.nextTag();
					//tile.mode = word == "CANNOT_BE_CONSTRUCTED_UPON" || word == "PASSABLE";
					//tile.mode = 1;
				}
				else if (tag == "END_TILE_OFFSET")
					break;
				gsf.advanceLine();
			}
		}
		else if (tag == "END_FOOTPRINT")
			break;
		gsf.advanceLine();
	}
}

std::pair<float, float> Footprint::rotateOrigin(float angle) const
{
	int rotx = (int)std::round(std::cos(angle));
	int rotz = (int)std::round(std::sin(angle));
	float x = rotx * originX - rotz * originZ;
	float z = rotx * originZ + rotz * originX;
	return { x,z };
}

std::pair<int, int> Footprint::TileOffset::rotate(float angle) const
{
	int rotx = (int)std::round(std::cos(angle));
	int rotz = (int)std::round(std::sin(angle));
	int x = rotx * offsetX - rotz * offsetZ;
	int z = rotx * offsetZ + rotz * offsetX;
	return { x,z };
}
