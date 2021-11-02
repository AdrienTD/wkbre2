// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <string>
#include <unordered_map>

struct Language {
	std::unordered_map<std::string, std::string> text;

	void load(const char* path);
	std::string getText(const std::string& key);

	Language() = default;
	//Language(const char* path) { load(path); }
};