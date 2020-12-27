#pragma once

#include <vector>
#include "actions.h"

struct ServerGameObject;
struct OrderBlueprint;
struct Cursor;

struct Command {
	int id;
	OrderBlueprint *order = nullptr;
	ActionSequence startSequence;
	Cursor *cursor = nullptr;
	std::vector<int> cursorConditions;

	void parse(GSFileParser &gsf, GameSet &gs);
	void execute(ServerGameObject *self, ServerGameObject *target = nullptr, int assignmentMode = 0);
};