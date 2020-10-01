#pragma once

#include <map>
#include <set>
#include <memory>
#include <string>
#include "../util/IndexedStringList.h"
#include "../tags.h"
#include "GameObjBlueprint.h"
#include "values.h"
#include "actions.h"
#include "../util/DynArray.h"
#include "command.h"
#include "../Model.h"
#include "OrderBlueprint.h"

struct GameObjBlueprint;

struct GameSet
{
	struct StriCompare
	{
		bool operator() (const std::string &a, const std::string &b) const {
			return _stricmp(a.c_str(), b.c_str()) < 0;
		}
	};

	IndexedStringList itemNames, equationNames, valueNames, actionSequenceNames,
		appearanceNames, commandNames, animationNames, orderNames, taskNames,
		orderAssignmentNames;
	IndexedStringList objBlueprintNames[Tags::GAMEOBJCLASS_COUNT];
	std::unique_ptr<GameObjBlueprint[]> objBlueprints[Tags::GAMEOBJCLASS_COUNT];
	std::map<std::string, float, StriCompare> definedValues;
	std::unique_ptr<int[]> itemSides;
	std::unique_ptr<ValueDeterminer*[]> equations;
	std::unique_ptr<ActionSequence[]> actionSequences;
	std::unique_ptr<Command[]> commands;
	DynArray<OrderBlueprint> orders;
	DynArray<TaskBlueprint> tasks;
	DynArray<OrderAssignmentBlueprint> orderAssignments;

	ModelCache modelCache;

	void parseFile(const char *fn, int pass);
	void load(const char *fn);

	GameObjBlueprint *findBlueprint(int cls, const std::string &name)
	{
		int x = objBlueprintNames[cls].getIndex(name);
		if (x == -1) return nullptr;
		else return &objBlueprints[cls][x];
	}

	GameObjBlueprint *getBlueprint(uint32_t fullId) {
		uint32_t clid = fullId & 63, tyid = fullId >> 6;
		return &objBlueprints[clid][tyid];
	}

	GameSet() {}
	GameSet(const char *fn) { load(fn); }
};
