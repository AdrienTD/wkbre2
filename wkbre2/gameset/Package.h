// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <memory>
#include <vector>

struct ValueDeterminer;
struct ObjectFinder;
struct GSFileParser;
struct GameSet;
struct ServerGameObject;
struct SrvScriptContext;

struct GSPackage {
	struct ItemModification {
		int mode;
		int item;
		ValueDeterminer* value;
	};
	std::vector<ItemModification> itemModifications;
	std::vector<int> gameEvents;
	ObjectFinder* relatedParty = nullptr;

	void parse(GSFileParser& gsf, GameSet& gs);
	void send(ServerGameObject* obj, SrvScriptContext* ctx);
};