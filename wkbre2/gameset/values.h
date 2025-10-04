// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

struct GSFileParser;
struct GameSet;
struct ServerGameObject;
struct ClientGameObject;
struct ScriptContext;
//struct SrvScriptContext;
//struct CliScriptContext;

//namespace Script {

struct ValueDeterminer {
	using EvalRetSrv = float;
	using EvalRetCli = float;
	virtual ~ValueDeterminer() {}
	virtual float eval(ScriptContext* ctx) = 0;
	virtual void parse(GSFileParser &gsf, const GameSet &gs) = 0;
	bool booleval(ScriptContext* ctx) { return eval(ctx) > 0.0f; }
	float fail(ScriptContext* ctx);
};

ValueDeterminer *ReadValueDeterminer(::GSFileParser &gsf, const ::GameSet &gs);
ValueDeterminer *ReadEquationNode(::GSFileParser &gsf, const ::GameSet &gs);

//}