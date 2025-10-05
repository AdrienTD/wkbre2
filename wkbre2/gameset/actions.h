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
	virtual void parse(GSFileParser &gsf, const GameSet &gs) = 0;
};

struct ActionSequence {
	std::vector<std::unique_ptr<Action>> actionList;

	struct DebugInfo {
		int fileIndex = -1;
		std::vector<int> actionLineIndices;
	};
	std::unique_ptr<DebugInfo> debugInfo;

	void init(GSFileParser& gsf, const GameSet& gs, const char* endtag);
	void run(SrvScriptContext* ctx) const;
	void run(ServerGameObject* self) const;
};

Action *ReadAction(GSFileParser &gsf, const GameSet &gs);
