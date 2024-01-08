// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <string>
#include <vector>

struct GameSet;
struct GSFileParser;
struct ValueDeterminer;
struct Language;
struct ScriptContext;

struct GSCondition {
	ValueDeterminer* test;
	std::string hint;
	std::vector<ValueDeterminer*> hintValues;

	void parse(GSFileParser &gsf, GameSet &gs);
	std::string getFormattedHint(const Language& lang, ScriptContext* ctx) const;
	static std::string getFormattedHint(const std::string& hint, const std::vector<ValueDeterminer*>& hintValues, const Language& lang, ScriptContext* ctx);
};