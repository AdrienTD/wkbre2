#pragma once

#include "../GameObjectRef.h"

template<typename T> struct TemporaryValue {
	T & var;
	const T original;

	TemporaryValue(T & var) : var(var), original(var) {}
	TemporaryValue(T & var, const T &val) : var(var), original(var) { var = val; }
	~TemporaryValue() { var = original; }

	explicit operator bool() const { return true; }
};

template<typename Program, typename AnyGO> struct ScriptContext {
	struct CtxElement {
	private:
		GameObjectRef<Program, AnyGO> object;
	public:
		TemporaryValue<decltype(object)> change(AnyGO *go) {
			return TemporaryValue<decltype(object)>(object, go); // copy elision required!!! (should be mandatory for C++17)
		}
		AnyGO *get() { return object.get(); }
	};
	static CtxElement candidate, creator, packageSender, sequenceExecutor;
};

extern template struct ScriptContext<Server, ServerGameObject>;
extern template struct ScriptContext<Client, ClientGameObject>;

using SrvScriptContext = ScriptContext<Server, ServerGameObject>;
using CliScriptContext = ScriptContext<Client, ClientGameObject>;

//template<typename T> ScriptContext<T> g_scriptCtx;
//
//extern ScriptContext<ServerGameObject> g_srvScriptCtx;
//extern ScriptContext<ClientGameObject> g_cliScriptCtx;
//
//template<typename T> ScriptContext<T> &getScriptCtx() { static_assert(false, "don't use this"); }
//template<> ScriptContext<ServerGameObject> getScriptCtx<ServerGameObject>() { return g_srvScriptCtx; }
//template<> ScriptContext<ClientGameObject> getScriptCtx<ClientGameObject>() { return g_cliScriptCtx; }