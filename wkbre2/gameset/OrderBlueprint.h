#pragma once

#include <vector>
#include "actions.h"

struct TaskBlueprint;
struct ObjectFinder;
struct ValueDeterminer;
struct GSFileParser;
struct GameSet;

struct TriggerBlueprint {
	int type;
	ActionSequence actions;
	ValueDeterminer *period = nullptr;
	TriggerBlueprint(int type) : type(type) {}
};

struct OrderBlueprint {
	int classType;
	bool cycleOrder = false;
	std::vector<TaskBlueprint*> tasks;
	ActionSequence startSequence, terminationSequence, cancellationSequence;

	void parse(GSFileParser &gsf, GameSet &gs);
};

struct TaskBlueprint {
	int classType;
	ObjectFinder *taskTarget = nullptr;
	ActionSequence startSequence, terminationSequence, cancellationSequence;
	std::vector<TriggerBlueprint> triggers;

	ValueDeterminer *proximityRequirement = nullptr;
	ActionSequence proximitySatisfiedSequence;

	void parse(GSFileParser &gsf, GameSet &gs);
};

struct OrderAssignmentBlueprint {
	OrderBlueprint *orderToAssign = nullptr;
	int orderAssignmentMode = 0;
	ObjectFinder *orderTarget = nullptr;
	void parse(GSFileParser &gsf, GameSet &gs);
};