// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <cstring>
#include <algorithm>
#include <array>

template<int N> struct TagDict
{
	std::array<const char *, N> tags;

	static bool CharStrCompare(const char *a, const char *b) {
		return strcmp(a, b) < 0;
	}

	int getTagID(const char *tag)
	{
		auto it = std::lower_bound(tags.begin(), tags.end(), tag, CharStrCompare);
		if (it == tags.end()) return -1;
		if (strcmp(*it, tag) != 0) return -1;
		return it - tags.begin();
	}

	const char *getStringFromID(int id)
	{
		return tags[id];
	}

	TagDict(std::array<const char *, N> &&tags) : tags(tags) {}
};
