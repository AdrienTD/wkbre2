// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <string_view>

inline int StrCICompare(std::string_view a, std::string_view b) {
	size_t len = (a.size() < b.size()) ? a.size() : b.size();
	for (size_t i = 0; i < len; ++i) {
		char x = a[i], y = b[i];
		if (x >= 'A' && x <= 'Z')
			x = x - 'A' + 'a';
		if (y >= 'A' && y <= 'Z')
			y = y - 'A' + 'a';
		if (x < y)
			return -1;
		else if (x > y)
			return 1;
	}
	return (a.size() == b.size()) ? 0 : ((a.size() < b.size()) ? -1 : 1);
}

struct StrCICompareClass
{
	bool operator() (std::string_view a, std::string_view b) const {
		return StrCICompare(a, b) < 0;
	}
};
