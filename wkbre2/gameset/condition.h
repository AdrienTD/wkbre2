// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <string>

struct GameSet;
struct GSFileParser;
struct ValueDeterminer;

struct GSCondition {
	ValueDeterminer* test;
	std::string hint;

	void parse(GSFileParser &gsf, GameSet &gs);
};