// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
#include "util/vecmat.h"
#include "Movement.h"
#include "GameObjectRef.h"
#include "Trajectory.h"
#include "tags.h"

struct GameObjBlueprint;
struct Model;

struct CommonGameObject {
	uint32_t id;
	GameObjBlueprint* blueprint;

	CommonGameObject* parent;
	std::map<int, std::vector<CommonGameObject*>> children;

	std::map<int, float> items;
	std::map<std::pair<int, int>, float> indexedItems;

	Vector3 position, orientation, scale = Vector3(1.0f, 1.0f, 1.0f);

	int subtype = 0, appearance = 0;
	int color = 0;

	float animStartTime = 0.0f; // should be game_time_t?
	int animationIndex = 0, animationVariant = 0;
	bool animClamped = false;

	Movement movement;
	Trajectory trajectory;

	enum Flags {
		fSelectable = 1,
		fTargetable = 2,
		fRenderable = 4,
		fTerminated = 8,
		fPlayerTerminated = 16,
	};
	int flags = fSelectable | fTargetable | fRenderable;
	int disableCount = 0;

	std::string name;

	int reportedCurrentOrder = -1;

	int tileIndex = -1;

	float getItem(int item) const;
	float getIndexedItem(int item, int index) const;

	CommonGameObject* getParent() const { return parent; }
	CommonGameObject* getPlayer() const;

	Matrix getWorldMatrix() const {
		return Matrix::getScaleMatrix(scale)
			* Matrix::getRotationXMatrix(-orientation.x)
			* Matrix::getRotationYMatrix(-orientation.y)
			* Matrix::getRotationZMatrix(-orientation.z)
			* Matrix::getTranslationMatrix(position);
	}

	bool isInteractable() const { return (disableCount <= 0) && !(flags & fTerminated); }

	Model* getModel() const;

	template<typename AnyGameObject> AnyGameObject* dyncast() { return (AnyGameObject*)this; }
	template<typename AnyGameObject> const AnyGameObject* dyncast() const { return (const AnyGameObject*)this; }

	CommonGameObject(uint32_t id, GameObjBlueprint *blueprint) : id(id), blueprint(blueprint), parent(nullptr) {}
};

template<typename Program, typename AnyGameObject> struct SpecificGameObject : CommonGameObject {
	using CommonGameObject::CommonGameObject;
	AnyGameObject* getParent() const { return (AnyGameObject*)CommonGameObject::getParent(); }
	AnyGameObject* getPlayer() const { return (AnyGameObject*)CommonGameObject::getPlayer(); }
};

template<typename Program, typename AnyGameObject> struct CommonGameState {
	using PrgGORef = SpecificGORef<Program, AnyGameObject>;

	std::map<int, std::unordered_set<PrgGORef>> aliases;
	std::map<std::pair<int, int>, int> diplomaticStatuses;

	struct Tile {
		std::vector<PrgGORef> objList;
		//std::vector<PrgGORef> zoneList;
		PrgGORef zone;
		PrgGORef building;
		bool buildingPassable;
	};
	std::unique_ptr<Tile[]> tiles;

	int getDiplomaticStatus(AnyGameObject *a, AnyGameObject *b) const {
		if (a == b) return 0; // player is always friendly with itself :)
		auto it = (a->id <= b->id) ? diplomaticStatuses.find({ a->id, b->id }) : diplomaticStatuses.find({ b->id, a->id });
		if (it != diplomaticStatuses.end())
			return it->second;
		else
			return ((Program*)this)->gameSet->defaultDiplomaticStatus;
	}

};