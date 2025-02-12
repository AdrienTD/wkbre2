#pragma once

#include "common.h"
#include "GameObjectRef.h"
#include "util/vecmat.h"

struct GameObjBlueprint;
struct CommonGameState;

struct StampdownPlan {
	enum struct Status {
		Valid = 0,
		InvalidNoExplanation,
		InvalidTerrain,
		InvalidNotInCity,
		InvalidOccupied
	};
	struct StampdownCreateAction {
		GameObjBlueprint* blueprint;
		Vector3 position;
		Vector3 orientation;
	};
	struct StampdownRemoveAction {
		GameObjectRef object;
	};

	Status status = Status::InvalidNoExplanation;
	std::vector<StampdownCreateAction> toCreate;
	std::vector<StampdownRemoveAction> toRemove;

	GameObjectRef ownerObject;
	GameObjBlueprint* newOwnerBlueprint = nullptr;
	CommonGameObject::CityRectangle cityRectangle;

	static StampdownPlan getStampdownPlan(
		CommonGameState* gameState, CommonGameObject* playerObj, GameObjBlueprint* blueprint, Vector3 position, Vector3 orientation);
};
