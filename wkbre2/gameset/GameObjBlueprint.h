#pragma once

#include "../util/GSFileParser.h"
//#include "gameset.h"
#include <map>
#include <string>
#include <vector>
#include "../util/IndexedStringList.h"

struct GameSet;
struct Command;
struct Model;
struct Reaction;
struct ValueDeterminer;

struct GameObjBlueprint {
	struct BPAppearance {
		std::string dir;
		std::map<int, std::vector<Model*>> animations;
	};

	struct SoundRef {
		std::string filePath;
		int soundBlueprint = -1;
		SoundRef(std::string&& name) : filePath(name) {}
		SoundRef(int sndBp) : soundBlueprint(sndBp) {}
	};

	struct PhysicalSubtype {
		std::string dir;
		std::map<int, BPAppearance> appearances;
		std::map<int, std::vector<SoundRef>> soundMap;
	};

	int bpClass, bpId;
	std::string name;
	GameSet *gameSet;

	std::map<int, float> startItemValues;

	IndexedStringList subtypeNames;
	std::map<int, PhysicalSubtype> subtypes;
	std::string modelPath;

	std::vector<Command*> offeredCommands;

	std::vector<Reaction*> intrinsicReactions;

	std::map<int, GameObjBlueprint*> mappedTypeTags;
	std::map<int, ValueDeterminer*> mappedValueTags;

	Model* representAs = nullptr;

	ValueDeterminer* missileSpeed = nullptr;

	std::map<int, std::vector<SoundRef>> soundMap;

	void parse(GSFileParser &gsf, const std::string &directory, bool isExtension = false);
	void init(int i_bpClass, int i_bpId, const std::string &i_name, GameSet *i_gameSet) {
		bpClass = i_bpClass; bpId = i_bpId; name = i_name; gameSet = i_gameSet;
	}

	uint32_t getFullId() { return bpClass | (bpId << 6); }
	std::string getFullName();

	//GameObjBlueprint() {}
	//GameObjBlueprint(GameSet *gameSet) : gameSet(gameSet) {}
	//GameObjBlueprint(int cls, const std::string &name, GameSet &gameSet) : bpClass(cls), name(name), gameSet(gameSet) {}

private:
	void loadAnimations(GameObjBlueprint::BPAppearance &ap, const std::string &dir, bool overrideAnims = false);
};
