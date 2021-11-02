// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <map>
#include <string>
#include <vector>

struct GameSet;
struct GSFileParser;

struct GSTerrain {
	std::map<int, float> itemValues;
	float terrainCost;
	std::vector<std::string> associatedGroups;
	void parse(GSFileParser& gsf, GameSet& gs);
};