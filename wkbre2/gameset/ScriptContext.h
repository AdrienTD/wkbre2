// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <type_traits>

struct CommonGameState;
struct Server;
struct Client;
struct CommonGameObject;
struct ServerGameObject;
struct ClientGameObject;

template<typename T> struct TemporaryValue {
	T & var;
	const T original;

	TemporaryValue(T & var) : var(var), original(var) {}
	TemporaryValue(T & var, const T &val) : var(var), original(var) { var = val; }
	~TemporaryValue() { var = original; }

	explicit constexpr operator bool() const { return true; }
};

struct ScriptContext {
	struct CtxElement {
	private:
		CommonGameObject* object = nullptr;
	public:
		TemporaryValue<decltype(object)> change(CommonGameObject* go) {
			return TemporaryValue<decltype(object)>(object, go); // copy elision required!!! (should be mandatory for C++17)
		}
		CommonGameObject* get() const { return object; }
		CtxElement() {}
		CtxElement(CommonGameObject* obj) : object(obj) {}
	};

	CommonGameObject* get(const CtxElement& var) const { return var.get(); }
	TemporaryValue<CommonGameObject*> change(CtxElement& var, CommonGameObject* obj) { return var.change(obj); }

	CommonGameObject* getSelf() const { return get(self); }
	TemporaryValue<CommonGameObject*> changeSelf(CommonGameObject* obj) { return change(self, obj); }

	bool isServer() const;
	bool isClient() const;

	ScriptContext(CommonGameState* gameState, CommonGameObject* self = nullptr) : gameState(gameState), self{self} {}

	CommonGameState* gameState;
	CtxElement self;
	CtxElement candidate, creator, packageSender, sequenceExecutor, target, orderGiver, chainOriginalSelf, collisionSubject, selectedObject;
};

template<typename TProgram, typename TAnyGO> struct SpecificScriptContext : ScriptContext {
	using Program = TProgram;
	using AnyGO = TAnyGO;

	union {
		typename std::conditional<std::is_same<Program, Server>::value, Server*, void*>::type server;
		typename std::conditional<std::is_same<Program, Client>::value, Client*, void*>::type client;
		Program* program;
	};

	AnyGO* get(const CtxElement& var) const { return (AnyGO*)ScriptContext::get(var); }
	AnyGO* getSelf() const { return get(self); }

	SpecificScriptContext(Program* program, AnyGO* self = nullptr) : ScriptContext(program, self), program(program) {}
};

struct SrvScriptContext : SpecificScriptContext<Server, ServerGameObject> {
	using SpecificScriptContext::SpecificScriptContext;
};
struct CliScriptContext : SpecificScriptContext<Client, ClientGameObject> {
	using SpecificScriptContext::SpecificScriptContext;
};
