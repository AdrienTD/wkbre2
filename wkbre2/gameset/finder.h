// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <vector>

struct CommonGameObject;
struct ServerGameObject;
struct ClientGameObject;
struct GSFileParser;
struct GameSet;
struct ScriptContext;
struct SrvScriptContext;
struct CliScriptContext;

struct ObjectFinder {
	using EvalRetSrv = std::vector<ServerGameObject*>;
	using EvalRetCli = std::vector<ClientGameObject*>;
	virtual ~ObjectFinder() {}
	virtual std::vector<CommonGameObject*> eval(ScriptContext* ctx) = 0;
	virtual void parse(GSFileParser &gsf, GameSet &gs) = 0;

	CommonGameObject* getFirst(ScriptContext* ctx) {
		auto objlist = eval(ctx);
		return objlist.empty() ? nullptr : objlist[0];
	}
	std::vector<CommonGameObject*> fail(ScriptContext* ctx);

	std::vector<ServerGameObject*> eval(SrvScriptContext* ctx) {
		// TEMP TODO: Avoid copy
		auto res = eval((ScriptContext*)ctx);
		ServerGameObject** ptr = (ServerGameObject**)res.data();
		auto cp = std::vector<ServerGameObject*>{ ptr, ptr + res.size() };
		return cp;
	}
	std::vector<ClientGameObject*> eval(CliScriptContext* ctx) {
		// TEMP TODO: Avoid copy
		auto res = eval((ScriptContext*)ctx);
		ClientGameObject** ptr = (ClientGameObject**)res.data();
		auto cp = std::vector<ClientGameObject*>{ ptr, ptr + res.size() };
		return cp;
	}
	ServerGameObject* getFirst(SrvScriptContext* ctx) {
		return (ServerGameObject*)getFirst((ScriptContext*)ctx);
	}
	ClientGameObject* getFirst(CliScriptContext* ctx) {
		return (ClientGameObject*)getFirst((ScriptContext*)ctx);
	}
};

ObjectFinder *ReadFinder(GSFileParser &gsf, const GameSet &gs);
ObjectFinder *ReadFinderNode(GSFileParser &gsf, const GameSet &gs);
