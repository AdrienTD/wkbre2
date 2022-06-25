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
struct GSCommission;

struct WorkOrderInstance {
	SrvGORef city;
	ObjectFinder* unitFinder;
	const WorkOrder* workOrder;
};

struct CommissionInstance {
	struct RequirementInstance {
		//const T* gsRequirement;
		int state = 0; // active, complete, stalled
	};
	struct CharacterRequirementInstance : RequirementInstance {

	};
	struct BuildingRequirementInstance : RequirementInstance {
		std::vector<SrvGORef> foundations;
	};
	struct UpgradeRequirementInstance : RequirementInstance {

	};

	const GSCommission* blueprint;
	SrvGORef handlerObject;
	
	std::vector<CharacterRequirementInstance> characterReqInsts;
	std::vector<BuildingRequirementInstance> buildingReqInsts;
	std::vector<UpgradeRequirementInstance> upgradeReqInsts;
};

struct AIController {
	ServerGameObject* gameObj;
	PlanNodeSequence* planBlueprint = nullptr; PlanNodeSequence::State planState;
	std::vector<WorkOrderInstance> workOrderInstances;
	std::vector<CommissionInstance> commissionInstances;
	
	AIController(ServerGameObject* obj) : gameObj(obj) {}
	void parse(GSFileParser& gsf, const GameSet& gs);
	void update();

	void activatePlan(int planTag);
	void abandonPlan();
	void registerWorkOrder(ServerGameObject* city, ObjectFinder* unitFinder, const WorkOrder* workOrder);
	void activateCommission(const GSCommission* commission, ServerGameObject* handler);
	void deactivateCommission(const GSCommission* commission, ServerGameObject* handler);
};
