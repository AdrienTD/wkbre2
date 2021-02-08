#pragma once

#include "actions.h"

struct GameObjBlueprint;
struct ObjectFinder;
struct PositionDeterminer;
struct GSFileParser;
struct GameSet;
struct SrvScriptContext;

struct ObjectCreation {
	GameObjBlueprint *typeToCreate;
	ObjectFinder *controller;
	PositionDeterminer *createAt = nullptr;
	ActionSequence postCreationSequence;

	void parse(GSFileParser &gsf, GameSet &gs);
	void run(ServerGameObject* creator, SrvScriptContext* ctx);
};