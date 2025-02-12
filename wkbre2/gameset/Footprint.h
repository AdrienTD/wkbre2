// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <vector>

struct GameSet;
struct GSFileParser;

struct Footprint {
	float originX = 0.0f, originZ = 0.0f;
	struct TileOffset {
		int offsetX, offsetZ;
		int mode = 0;
		TileOffset(int offsetX, int offsetZ) : offsetX(offsetX), offsetZ(offsetZ) {}
		std::pair<int, int> rotate(float angle) const;
	};
	std::vector<TileOffset> tiles;

	int wallOriginDefaultMinX = 0, wallOriginDefaultMinY = 0;
	int wallOriginDefaultMaxX = 0, wallOriginDefaultMaxY = 0;

	void parse(GSFileParser& gsf, GameSet& gs);
	std::pair<float, float> rotateOrigin(float angle) const;
};