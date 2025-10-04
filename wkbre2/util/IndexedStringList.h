// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <cstring>
#include <string>
#include <map>
#include <vector>
#include "StriCompare.h"

// Basically a vector of strings but with a map for quick string searching
struct IndexedStringList
{
	std::map<std::string, int, StrCICompareClass> str2idMap;
	std::vector<decltype(str2idMap)::iterator> id2strVector;

	struct iterator {
		decltype(id2strVector)::const_iterator it;
		iterator(const decltype(it) &it) : it(it) {}
		bool operator!= (iterator &other) { return it != other.it; }
		void operator++ () { it++; }
		const std::string & operator* () { return (*it)->first; }
	};

	void clear()
	{
		id2strVector.clear();
		str2idMap.clear();
	}

	int insertString(const std::string &str)
	{
		int nextid = static_cast<int>(id2strVector.size());
		auto [iterator, inserted] = str2idMap.try_emplace(str, nextid);
		if (inserted) { // If the string was inserted correctly
			id2strVector.push_back(iterator);
			return nextid;
		}
		return iterator->second;
	}

	int getIndex(const std::string &str) const
	{
		auto it = str2idMap.find(str);
		if (it != str2idMap.end())
			return it->second;
		else
			return -1;
	}

	const std::string &getString(int id) const
	{
		return id2strVector[id]->first;
	}

	size_t size() const
	{
		return id2strVector.size();
	}
	auto begin() const { return iterator(id2strVector.begin()); }
	auto end() const { return iterator(id2strVector.end()); }

	int operator[] (const std::string &str) const { return getIndex(str); }
	const std::string& operator[] (int id) const { return getString(id); }
};