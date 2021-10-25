#pragma once

#include <cstring>
#include <string>

struct StriCompare
{
	bool operator() (const std::string &a, const std::string &b) const {
		return _stricmp(a.c_str(), b.c_str()) < 0;
	}
};
