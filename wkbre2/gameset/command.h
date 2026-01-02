// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <string>
#include <vector>
#include "actions.h"
#include "../util/vecmat.h"

struct ServerGameObject;
struct OrderBlueprint;
struct WndCursor;
struct GSCondition;
struct GameObjBlueprint;
struct ValueDeterminer;

struct Command {
	int id;
	OrderBlueprint *order = nullptr;
	ActionSequence startSequence;
	WndCursor *cursor = nullptr;
	std::vector<int> cursorConditions, iconConditions;
	std::vector<std::pair<GSCondition*, WndCursor*>> cursorAvailable;
	std::string buttonEnabled, buttonWait, buttonDepressed, buttonHighlighted, buttonAvailable, buttonImpossible;
	std::vector<GSCondition*> conditionsImpossible, conditionsWait, conditionsWarning;
	const GameObjBlueprint* stampdownObject = nullptr;
	std::string defaultHint;
	std::vector<ValueDeterminer*> defaultHintValues;
	bool canBeAssignedToSpawnedUnits = false;

	void parse(GSFileParser &gsf, GameSet &gs);
	bool canBeExecuted(ServerGameObject* self, ServerGameObject* target = nullptr) const;
	void execute(ServerGameObject *self, ServerGameObject *target = nullptr, int assignmentMode = 0, const Vector3 &destination = Vector3()) const;
};