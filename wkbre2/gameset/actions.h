// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <vector>
#include <memory>

struct ServerGameObject;
struct GSFileParser;
struct GameSet;
struct SrvScriptContext;

struct Action {
	virtual ~Action() {}
	virtual void run(SrvScriptContext* ctx) = 0;
	virtual void parse(GSFileParser &gsf, GameSet &gs) = 0;
};

struct ActionSequence {
	std::vector<std::unique_ptr<Action>> actionList;
	void init(GSFileParser& gsf, const GameSet& gs, const char* endtag);
	void run(SrvScriptContext* ctx) {
		for (auto &action : actionList)
			action->run(ctx);
	}
	void run(ServerGameObject* self);
};

Action *ReadAction(GSFileParser &gsf, const GameSet &gs);
