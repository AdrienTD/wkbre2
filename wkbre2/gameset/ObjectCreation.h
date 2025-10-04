// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include "actions.h"

struct GameObjBlueprint;
struct ObjectFinder;
struct PositionDeterminer;
struct GSFileParser;
struct GameSet;
struct SrvScriptContext;

struct ObjectCreation {
	const GameObjBlueprint *typeToCreate = nullptr;
	int mappedTypeToCreate = -1; ObjectFinder* mappedTypeFrom = nullptr;
	ObjectFinder *controller;
	PositionDeterminer *createAt = nullptr;
	ActionSequence postCreationSequence;
	ObjectFinder* subordinates = nullptr;

	void parse(GSFileParser &gsf, const GameSet &gs);
	void run(ServerGameObject* creator, SrvScriptContext* ctx) const;
};