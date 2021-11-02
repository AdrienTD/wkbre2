// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <vector>

struct ServerGameObject;
struct ClientGameObject;
struct GSFileParser;
struct GameSet;
struct SrvScriptContext;
struct CliScriptContext;

struct ObjectFinder {
	using EvalRetSrv = std::vector<ServerGameObject*>;
	using EvalRetCli = std::vector<ClientGameObject*>;
	virtual ~ObjectFinder() {}
	virtual std::vector<ServerGameObject*> eval(SrvScriptContext* ctx) = 0;
	virtual std::vector<ClientGameObject*> eval(CliScriptContext* ctx);
	virtual void parse(GSFileParser &gsf, GameSet &gs) = 0;
	template<typename CTX> typename CTX::AnyGO* getFirst(CTX* ctx) {
		auto objlist = eval(ctx);
		return objlist.empty() ? nullptr : objlist[0];
	}
};

ObjectFinder *ReadFinder(GSFileParser &gsf, const GameSet &gs);
ObjectFinder *ReadFinderNode(GSFileParser &gsf, const GameSet &gs);
