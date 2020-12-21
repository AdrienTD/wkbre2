#pragma once

#include <cstdint>
#include <map>
#include <vector>
#include "util/vecmat.h"
#include "Movement.h"

struct GameObjBlueprint;

template<class AnyGameObject> struct CommonGameObject {
	uint32_t id;
	GameObjBlueprint *blueprint;

	AnyGameObject *parent;
	std::map<int, std::vector<AnyGameObject*>> children;

	std::map<int, float> items;
	std::map<std::pair<int, int>, float> indexedItems;

	Vector3 position, orientation;

	int subtype = 0, appearance = 0;
	int color = 0;

	//AnyGameObject *player = nullptr;

	float animStartTime = 0.0f; // should be game_time_t?
	int animationIndex = 0, animationVariant = 0;

	Movement movement;

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

	CommonGameObject(uint32_t id, GameObjBlueprint *blueprint) : id(id), blueprint(blueprint), parent(nullptr) {}
};
