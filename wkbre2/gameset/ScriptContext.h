#pragma once

#include "../GameObjectRef.h"
#include <type_traits>

template<typename T> struct TemporaryValue {
	T & var;
	const T original;

	TemporaryValue(T & var) : var(var), original(var) {}
	TemporaryValue(T & var, const T &val) : var(var), original(var) { var = val; }
	~TemporaryValue() { var = original; }

	explicit operator bool() const { return true; }
};

template<typename TProgram, typename TAnyGO> struct ScriptContext {
	using Program = TProgram;
	using AnyGO = TAnyGO;

	struct CtxElement {
	private:
		AnyGO* object = nullptr;
	public:
		TemporaryValue<decltype(object)> change(AnyGO *go) {
			return TemporaryValue<decltype(object)>(object, go); // copy elision required!!! (should be mandatory for C++17)
		}
		AnyGO *get() const { return object; }
		CtxElement() {}
		CtxElement(AnyGO* obj) : object(obj) {}
	};

	union {
		typename std::conditional<std::is_same<Program, Server>::value, Server*, void*>::type server;
		typename std::conditional<std::is_same<Program, Client>::value, Client*, void*>::type client;
		Program* program;
	};

	CtxElement self;
	CtxElement candidate, creator, packageSender, sequenceExecutor, target, orderGiver, chainOriginalSelf, collisionSubject, selectedObject;

	ScriptContext(Program* program, AnyGO* self = nullptr) : program(program), self(self) {}
};

struct SrvScriptContext : ScriptContext<Server, ServerGameObject> {
	SrvScriptContext(Server* program, ServerGameObject* self = nullptr);
};
struct CliScriptContext : ScriptContext<Client, ClientGameObject> {
	CliScriptContext(Client* program, ClientGameObject* self = nullptr);
};
