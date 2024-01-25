// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <vector>
#include "actions.h"

struct TaskBlueprint;
struct ObjectFinder;
struct ValueDeterminer;
struct GSFileParser;
struct GameSet;
struct ServerGameObject;
struct SrvScriptContext;

struct TriggerBlueprint {
	int type;
	ActionSequence actions;
	ValueDeterminer *period = nullptr;
	TriggerBlueprint(int type) : type(type) {}
};

struct OrderBlueprint {
	int bpid;
	int classType;
	int category = -1;
	bool cycleOrder = false;
	bool cannotInterruptOrder = false;
	std::vector<int> preConditions;
	std::vector<TaskBlueprint*> tasks;
	ActionSequence initSequence, startSequence, resumptionSequence, terminationSequence, cancellationSequence;

	void parse(GSFileParser &gsf, GameSet &gs);
};

struct TaskBlueprint {
	int bpid;
	int classType;
	int category = -1;
	ObjectFinder *taskTarget = nullptr;
	bool usePreviousTaskTarget = false, terminateEntireOrderIfNoTarget = false, rejectTargetIfItIsTerminated = false;
	ActionSequence initSequence, startSequence, resumptionSequence, terminationSequence, cancellationSequence;
	std::vector<TriggerBlueprint> triggers;
	ActionSequence collisionTrigger, struckFloorTrigger;

	ValueDeterminer *proximityRequirement = nullptr;
	ActionSequence proximitySatisfiedSequence;

	std::vector<std::pair<int, int>> equAnims;
	int defaultAnim = -1;
	bool playAnimationOnce = false;

	ActionSequence movementCompletedSequence;

	ValueDeterminer* synchAnimationToFraction = nullptr;

	void parse(GSFileParser &gsf, GameSet &gs);
};

struct OrderAssignmentBlueprint {
	OrderBlueprint *orderToAssign = nullptr;
	int orderAssignmentMode = 0;
	ObjectFinder *orderTarget = nullptr;
	void parse(GSFileParser &gsf, GameSet &gs);
	void assignTo(ServerGameObject* gameobj, SrvScriptContext* ctx, ServerGameObject* giver = nullptr) const;
};