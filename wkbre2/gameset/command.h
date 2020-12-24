#pragma once

#include "actions.h"

struct ServerGameObject;
struct OrderBlueprint;

struct Command {
	int id;
	OrderBlueprint *order = nullptr;
	ActionSequence startSequence;

	void parse(GSFileParser &gsf, GameSet &gs);
	void execute(ServerGameObject *self, ServerGameObject *target = nullptr, int assignmentMode = 0);
};