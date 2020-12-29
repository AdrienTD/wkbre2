#pragma once

#include <vector>
#include "actions.h"
#include "../util/vecmat.h"

struct ServerGameObject;
struct OrderBlueprint;
struct Cursor;
struct GSCondition;

struct Command {
	int id;
	OrderBlueprint *order = nullptr;
	ActionSequence startSequence;
	Cursor *cursor = nullptr;
	std::vector<int> cursorConditions;
	std::vector<std::pair<GSCondition*, Cursor*>> cursorAvailable;

	void parse(GSFileParser &gsf, GameSet &gs);
	void execute(ServerGameObject *self, ServerGameObject *target = nullptr, int assignmentMode = 0, const Vector3 &destination = Vector3());
};