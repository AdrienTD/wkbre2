#pragma once

#include "actions.h"

struct ServerGameObject;

struct Command {
	int id;
	ActionSequence startSequence;

	void parse(GSFileParser &gsf, const GameSet &gs);
	void execute(ServerGameObject *self, ServerGameObject *target = nullptr, int assignmentMode = 0);
};