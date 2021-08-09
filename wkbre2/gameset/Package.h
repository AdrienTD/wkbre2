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