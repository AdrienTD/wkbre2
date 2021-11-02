// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <cstdint>
#include <map>
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

template<class AnyGameObject> struct CommonGameObject {
	uint32_t id;
	GameObjBlueprint *blueprint;

	AnyGameObject *parent;
	std::map<int, std::vector<AnyGameObject*>> children;

	std::map<int, float> items;
	std::map<std::pair<int, int>, float> indexedItems;

	Vector3 position, orientation, scale = Vector3(1.0f, 1.0f, 1.0f);

	int subtype = 0, appearance = 0;
	int color = 0;

	//AnyGameObject *player = nullptr;

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

	float getItem(int item)
	{
		auto it = items.find(item);
		if (it != items.end())
			return it->second;
		auto bt = blueprint->startItemValues.find(item);
		if (bt != blueprint->startItemValues.end())
			return bt->second;
		return 0.0f;
	}

	float getIndexedItem(int item, int index) { return indexedItems[std::make_pair(item, index)]; }

	AnyGameObject *getPlayer() {
		for (CommonGameObject* obj = this; obj; obj = obj->parent) {
			if (obj->blueprint->bpClass == Tags::GAMEOBJCLASS_PLAYER)
				return (AnyGameObject*)obj;
		}
		return nullptr;
	}

	Matrix getWorldMatrix() {
		return Matrix::getScaleMatrix(scale)
			* Matrix::getRotationXMatrix(-orientation.x)
			* Matrix::getRotationYMatrix(-orientation.y)
			* Matrix::getRotationZMatrix(-orientation.z)
			* Matrix::getTranslationMatrix(position);
	}

	bool isInteractable() { return (disableCount <= 0) && !(flags & fTerminated); }

	Model* getModel() {
		return blueprint->getModel(subtype, appearance, animationIndex, animationVariant);
	}

	CommonGameObject(uint32_t id, GameObjBlueprint *blueprint) : id(id), blueprint(blueprint), parent(nullptr) {}
};

template<typename Program, typename AnyGameObject> struct CommonGameState {
	std::map<int, std::unordered_set<GameObjectRef<Program, AnyGameObject>>> aliases;
	std::map<std::pair<int, int>, int> diplomaticStatuses;

	int getDiplomaticStatus(AnyGameObject *a, AnyGameObject *b) const {
		if (a == b) return 0; // player is always friendly with itself :)
		auto it = (a->id <= b->id) ? diplomaticStatuses.find({ a->id, b->id }) : diplomaticStatuses.find({ b->id, a->id });
		if (it != diplomaticStatuses.end())
			return it->second;
		else
			return ((Program*)this)->gameSet->defaultDiplomaticStatus;
	}

};