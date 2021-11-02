// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <string>
#include <vector>

struct GameSet;
struct GSFileParser;

struct GSSound {
	std::vector<std::string> files;
	// TODO: Find default values
	float startDist = 0.0f;
	float muteDist = 100.0f;
	float gradientStartDist = 30.0f;
	float velocity = 0.0f;
	float rolloff = -1.0f;

	void parse(GSFileParser& gsf, GameSet& gs);
};