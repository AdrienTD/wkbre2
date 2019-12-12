#pragma once

#include <cstdint>
#include <map>
#include <vector>
#include "util/vecmat.h"

typedef float game_time_t;

struct TimeManager {
	// float is baaad!!
	game_time_t currentTime = 0.0f, previousTime = 0.0f, elapsedTime = 0.0f, timeSpeed = 1.0f;
	bool paused = true; uint32_t lockCount = 1;

	// synch to clients ...

	void lock() { lockCount++; }
	void unlock() { lockCount--; }

	void tick() {
		if (paused) return;
		previousTime = currentTime;
		currentTime = 132; // magic
		elapsedTime = currentTime - previousTime;
	}

};

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
