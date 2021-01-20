#pragma once

struct ServerGameObject;
struct ClientGameObject;
struct SrvScriptContext;
struct CliScriptContext;

template<typename D, typename T> struct CommonEval : T {
	virtual typename T::EvalRetSrv eval(SrvScriptContext* ctx) override { return ((D*)this)->common_eval(ctx); }
	virtual typename T::EvalRetCli eval(CliScriptContext* ctx) override { return ((D*)this)->common_eval(ctx); }
};
