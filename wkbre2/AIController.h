// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include "gameset/Plan.h"
#include "GameObjectRef.h"

struct ServerGameObject;
struct GSFileParser;
struct GameSet;
struct WorkOrder;
struct ObjectFinder;

struct WorkOrderInstance {
	SrvGORef city;
	ObjectFinder* unitFinder;
	const WorkOrder* workOrder;
};

struct AIController {
	ServerGameObject* gameObj;
	PlanNodeSequence* planBlueprint = nullptr; PlanNodeSequence::State planState;
	std::vector<WorkOrderInstance> workOrderInstances;
	
	AIController(ServerGameObject* obj) : gameObj(obj) {}
	void parse(GSFileParser& gsf, const GameSet& gs);
	void update();

	void activatePlan(int planTag);
	void abandonPlan();
	void registerWorkOrder(ServerGameObject* city, ObjectFinder* unitFinder, const WorkOrder* workOrder);
};
