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
#include "../util/StriCompare.h"
#include "reaction.h"
#include "ObjectCreation.h"

struct GameObjBlueprint;

template<typename T> struct GSBlueprintList {
	IndexedStringList names;
	DynArray<T> blueprints;
	T & operator[](size_t index) { return blueprints[index]; }
	const T & operator[](size_t index) const { return blueprints[index]; }
	size_t size() const { return names.size(); }
	void pass() { blueprints.resize(names.size()); }
	int readIndex(GSFileParser &gsf) const { return names.getIndex(gsf.nextString(true)); }
	T & readRef(GSFileParser &gsf) { return blueprints[readIndex(gsf)]; }
	const T & readRef(GSFileParser &gsf) const { return blueprints[readIndex(gsf)]; }
	T * readPtr(GSFileParser &gsf) { return &blueprints[readIndex(gsf)]; }
	const T * readPtr(GSFileParser &gsf) const { return &blueprints[readIndex(gsf)]; }
};

template<> struct GSBlueprintList<void> {
	IndexedStringList names;
	size_t size() const { return names.size(); }
	void pass() {}
	int readIndex(GSFileParser &gsf) const { return names.getIndex(gsf.nextString(true)); }
};

struct GameSet
{
	GSBlueprintList<GameObjBlueprint> objBlueprints[Tags::GAMEOBJCLASS_COUNT];
	GSBlueprintList<int> items;
	GSBlueprintList<ValueDeterminer*> equations;
	GSBlueprintList<ActionSequence> actionSequences;
	GSBlueprintList<void> appearances;
	GSBlueprintList<Command> commands;
	GSBlueprintList<void> animations;
	GSBlueprintList<OrderBlueprint> orders;
	GSBlueprintList<TaskBlueprint> tasks;
	GSBlueprintList<OrderAssignmentBlueprint> orderAssignments;
	GSBlueprintList<void> aliases;
	GSBlueprintList<void> events;
	GSBlueprintList<Reaction> reactions;
	GSBlueprintList<PackageReceiptTrigger> prTriggers;
	GSBlueprintList<ObjectCreation> objectCreations;
	GSBlueprintList<ObjectFinder*> objectFinderDefinitions;
	GSBlueprintList<void> associations;
	GSBlueprintList<void> typeTags;
	GSBlueprintList<std::unique_ptr<ValueDeterminer>> valueTags;

	std::map<std::string, float, StriCompare> definedValues;

	ModelCache modelCache;

	void parseFile(const char *fn, int pass);
	void load(const char *fn);

	GameObjBlueprint *findBlueprint(int cls, const std::string &name)
	{
		int x = objBlueprints[cls].names.getIndex(name);
		if (x == -1) return nullptr;
		else return &objBlueprints[cls][x];
	}

	GameObjBlueprint *getBlueprint(uint32_t fullId) {
		uint32_t clid = fullId & 63, tyid = fullId >> 6;
		return &objBlueprints[clid][tyid];
	}

	GameObjBlueprint *readObjBlueprintPtr(GSFileParser &gsf);

	GameSet() {}
	GameSet(const char *fn) { load(fn); }
};