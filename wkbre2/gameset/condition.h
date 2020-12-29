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