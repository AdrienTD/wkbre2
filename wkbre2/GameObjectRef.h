// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <cstdint>
#include <functional>

struct CommonGameState;
struct CommonGameObject;

struct GameObjectRef {
	static constexpr uint32_t NULL_GOREF = 0;

	uint32_t objid;

	template<typename Program>
	typename Program::GameObject* getFrom(Program* gameState) const {
		if (objid != NULL_GOREF)
			return gameState->findObject(objid);
		else
			return nullptr;
	}

	template<typename Program>
	typename Program::GameObject* getFrom() const {
		return getFrom(Program::instance);
	}

	void set(uint32_t id) { objid = id; }
	GameObjectRef& operator=(uint32_t id) noexcept { set(id); return *this; }
	GameObjectRef& operator=(decltype(nullptr)) { set(NULL_GOREF); return *this; }
	GameObjectRef& operator=(const GameObjectRef& ref) noexcept { set(ref.objid); return *this; }
	GameObjectRef& operator=(GameObjectRef&& ref) noexcept { objid = ref.objid; ref.objid = NULL_GOREF; return *this; }

	bool operator<(const GameObjectRef& other) const { return objid < other.objid; }
	bool operator==(const GameObjectRef& other) const { return objid == other.objid; }
	bool operator!=(const GameObjectRef& other) const { return objid != other.objid; }

	GameObjectRef() noexcept : objid(NULL_GOREF) {}
	//GameObjectRef(CommonGameObject* obj) noexcept { set(obj->id); }
	GameObjectRef(uint32_t id) noexcept : objid(id) {}
	GameObjectRef(const GameObjectRef& other) noexcept : objid(other.objid) {}
	GameObjectRef(GameObjectRef&& other) noexcept { objid = other.objid; other.objid = NULL_GOREF; }
};

template<typename Program, typename AnyGameObject> struct SpecificGORef : GameObjectRef {
	AnyGameObject* get() const { return getFrom<Program>(); }
	AnyGameObject* operator->() const { return get(); }
	explicit operator bool() const { return get() != nullptr; }
	operator AnyGameObject*() const { return get(); }
	
	void set(AnyGameObject* obj) { if (obj) objid = obj->id; else objid = NULL_GOREF; }
	void set(uint32_t id) { objid = id; }
	SpecificGORef& operator=(AnyGameObject* obj) noexcept { set(obj); return *this; }
	SpecificGORef& operator=(uint32_t id) noexcept { set(id); return *this; }
	SpecificGORef& operator=(const SpecificGORef& ref) noexcept { set(ref.objid); return *this; }
	SpecificGORef& operator=(SpecificGORef&& ref) noexcept { objid = ref.objid; ref.objid = NULL_GOREF; return *this; }

	bool operator<(const SpecificGORef& other) const { return objid < other.objid; }
	bool operator==(const SpecificGORef& other) const { return objid == other.objid; }
	bool operator==(AnyGameObject* obj) const { return objid == obj->id; }

	SpecificGORef() noexcept {}
	SpecificGORef(AnyGameObject* obj) noexcept { set(obj); }
	SpecificGORef(uint32_t id) noexcept : GameObjectRef(id) {}
	SpecificGORef(const SpecificGORef& other) noexcept : GameObjectRef(other) {}
	SpecificGORef(SpecificGORef&& other) noexcept : GameObjectRef(other) {}
};

template<> struct std::hash<GameObjectRef> {
	size_t operator()(const GameObjectRef& ref) const { return ref.objid; }
};
template<typename S, typename T> struct std::hash<SpecificGORef<S, T>> {
	size_t operator()(const SpecificGORef<S, T>& ref) const { return ref.objid; }
};

struct Server;
struct Client;
struct ServerGameObject;
struct ClientGameObject;

typedef GameObjectRef CmnGORef;
typedef SpecificGORef<Server, ServerGameObject> SrvGORef;
typedef SpecificGORef<Client, ClientGameObject> CliGORef;
