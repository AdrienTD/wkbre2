#pragma once

#include <vector>
#include "actions.h"

struct TaskBlueprint;
struct ObjectFinder;
struct ValueDeterminer;
struct GSFileParser;
struct GameSet;
struct ServerGameObject;

struct TriggerBlueprint {
	int type;
	ActionSequence actions;
	ValueDeterminer *period = nullptr;
	TriggerBlueprint(int type) : type(type) {}
};

struct OrderBlueprint {
	int bpid;
	int classType;
	bool cycleOrder = false;
	std::vector<TaskBlueprint*> tasks;
	ActionSequence initSequence, startSequence, resumptionSequence, terminationSequence, cancellationSequence;

	void parse(GSFileParser &gsf, GameSet &gs);
};

struct TaskBlueprint {
	int bpid;
	int classType;
	ObjectFinder *taskTarget = nullptr;
	ActionSequence initSequence, startSequence, resumptionSequence, terminationSequence, cancellationSequence;
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
	void assignTo(ServerGameObject *gameobj) const;
};