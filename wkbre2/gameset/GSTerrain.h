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