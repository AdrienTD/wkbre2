// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include "gameset/Plan.h"

struct ServerGameObject;
struct GSFileParser;
struct GameSet;

struct AIController {
	ServerGameObject* gameObj;
	PlanNodeSequence* planBlueprint = nullptr; PlanNodeSequence::State planState;
	
	AIController(ServerGameObject* obj) : gameObj(obj) {}
	void parse(GSFileParser& gsf, const GameSet& gs);
	void update();
};