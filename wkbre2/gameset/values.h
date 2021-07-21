#pragma once

struct GSFileParser;
struct GameSet;
struct ServerGameObject;
struct ClientGameObject;
struct SrvScriptContext;
struct CliScriptContext;

//namespace Script {

struct ValueDeterminer {
	using EvalRetSrv = float;
	using EvalRetCli = float;
	virtual ~ValueDeterminer() {}
	virtual float eval(SrvScriptContext* ctx) = 0;
	virtual float eval(CliScriptContext* ctx);
	virtual void parse(GSFileParser &gsf, GameSet &gs) = 0;
	template<typename CTX> bool booleval(CTX* ctx) { return eval(ctx) > 0.0f; }
};

ValueDeterminer *ReadValueDeterminer(::GSFileParser &gsf, const ::GameSet &gs);
ValueDeterminer *ReadEquationNode(::GSFileParser &gsf, const ::GameSet &gs);

//}