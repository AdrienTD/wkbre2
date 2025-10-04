#include "AIController.h"
#include "server.h"
#include "gameset/ScriptContext.h"
#include "gameset/Plan.h"
#include "gameset/gameset.h"
#include "util/GSFileParser.h"
#include "gameset/finder.h"
#include "Order.h"

void AIController::parse(GSFileParser& gsf, const GameSet& gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		std::string strtag = gsf.nextTag();
		if (strtag == "END_AI_CONTROLLER")
			break;
		else if (strtag == "MASTER_PLAN") {
			activatePlan(gs.plans.readIndex(gsf));
		}
		else if (strtag == "WORK_ORDER") {
			// WKBattles work orders
			ServerGameObject* city = Server::instance->findObject(gsf.nextInt());
			ObjectFinder* unitFinder = gs.objectFinderDefinitions.readRef(gsf);
			const WorkOrder* workOrder = gs.workOrders.readPtr(gsf);
			registerWorkOrder(city, unitFinder, workOrder);
			// normally a block ending with END_WORK_ORDER comes afterwards, but apparently it is empty, no need to parse
		}
		// TODO: SETTLEMENT_CONTROLLER for WK1 work orders
		gsf.advanceLine();
	}
}

void AIController::update()
{
	// ---- Plan ----
	if (planBlueprint) {
		SrvScriptContext ctx{ Server::instance, gameObj };
		bool done = planBlueprint->execute(&planState, &ctx);
		if (done) {
			planBlueprint = nullptr;
			planState.nodeStates.clear();
		}
	}

	// ---- Work Orders ----
	for (auto& woi : workOrderInstances) {
		if (!woi.city)
			continue;
		SrvScriptContext ctx{ Server::instance, woi.city };
		auto units = woi.unitFinder->eval(&ctx);

		//// count current orders
		//DynArray<int> unitsPerAssignment;
		//unitsPerAssignment.resize(woi.workOrder->assignments.size());
		//for (auto& val : unitsPerAssignment)
		//	val = 0;
		//for (auto& unit : units) {
		//	if (unit->blueprint->bpClass == Tags::GAMEOBJCLASS_CHARACTER) {
		//		for (size_t i = 0; i < woi.workOrder->assignments.size(); ++i) {
		//			auto& asg = woi.workOrder->assignments[i];
		//			Order* order = unit->orderConfig.getCurrentOrder();
		//			if (order && order->blueprint == asg.order) {
		//				unitsPerAssignment[i] += 1;
		//			}
		//		}
		//	}
		//}

		// Very simple implementation with flaws

		auto idComparator = [](ServerGameObject* a, ServerGameObject* b) -> bool {return a->id < b->id; };
		std::sort(units.begin(), units.end(), idComparator);
		auto popUnitAndAssignTo = [&units](ServerGameObject* target, const OrderBlueprint* order) {
			if (units.empty()) return;
			ServerGameObject* unit = (ServerGameObject*)units.back();
			Order* currentOrder = unit->orderConfig.getCurrentOrder();
			if (!currentOrder || currentOrder->blueprint != order) {
				unit->orderConfig.addOrder((OrderBlueprint*)order, Tags::ORDERASSIGNMODE_FORGET_EVERYTHING_ELSE, target);
			}
			units.pop_back();
		};
		for (size_t i = 0; i < woi.workOrder->assignments.size(); ++i) {
			auto& asg = woi.workOrder->assignments[i];
			int quantity = 0;
			switch (asg.quantityType) {
			case WorkOrder::Assignment::INDIVIDUALS:
				quantity = (int)asg.quantityValue->eval(&ctx); // round down or up?
				break;
			case WorkOrder::Assignment::FRACTION_DOWN:
				quantity = (int)((float)units.size() * asg.quantityValue->eval(&ctx)); // round down or up?
				break;
			case WorkOrder::Assignment::REMAINING_WORKERS:
				quantity = (int)units.size();
				break;
			}
			switch (asg.distType) {
			case WorkOrder::Assignment::DIST_PER: {
				// for every target we assign "quantity" units
				auto targets = asg.distFinder->eval(&ctx);
				std::sort(targets.begin(), targets.end(), idComparator);
				for (auto& target : targets) {
					for (int i = 0; i < quantity; i++) {
						popUnitAndAssignTo(target, asg.order);
					}
				}
				break;
			}
			case WorkOrder::Assignment::DIST_BETWEEN: {
				// assign "quantity" units across the targets
				auto targets = asg.distFinder->eval(&ctx);
				std::sort(targets.begin(), targets.end(), idComparator);
				if (!targets.empty()) {
					for (int i = 0; i < quantity; i++) {
						int t = i % targets.size();
						popUnitAndAssignTo(targets[t], asg.order);
					}
				}
				break;
			}
			case WorkOrder::Assignment::DIST_AUTO_IDENTIFY:
				// assign "quantity" units to random objects
				for (int i = 0; i < quantity; i++) {
					popUnitAndAssignTo(nullptr, asg.order);
				}
				break;
			}
		}
	}
	auto wit = std::remove_if(workOrderInstances.begin(), workOrderInstances.end(),
		[](auto& woi) -> bool {return !woi.city; }
	);
	workOrderInstances.erase(wit, workOrderInstances.end());

	static std::vector<std::pair<const GSCommission*, SrvGORef>> completedComInsts;
	completedComInsts.clear();

	// ---- Commissions ----
	int cid = -1;
	for (auto& cominst : commissionInstances) {
		++cid;
		const GSCommission* gscomm = cominst.blueprint;
		ServerGameObject* obj = cominst.handlerObject.get();
		if (!obj) continue;
		SrvScriptContext ctx{ Server::instance, obj };
		if (gscomm->suspensionCondition && gscomm->suspensionCondition->booleval(&ctx))
			continue;

		size_t completeCount = 0;

		// Character Requirements
		for (size_t ri = 0; ri < cominst.characterReqInsts.size(); ++ri) {
			auto& gsreq = gscomm->characterRequirements[ri];
			auto& reqinst = cominst.characterReqInsts[ri];
			size_t requiredCount = (size_t)std::ceil(gsreq.vdCountNeeded->eval(&ctx));
			size_t existingCount = gsreq.fdAlreadyHaving->eval(&ctx).size();
			//size_t inCtrCount = reqinst.foundations.size();
			if (existingCount < requiredCount) {
				// find spawn buildings and create spawn order if available
				auto& charToSpawn = gsreq.ladder->entries.back();
				SrvFinderResult buildingList = charToSpawn.finder->eval(&ctx);
				for (ServerGameObject* building : buildingList) {
					if (building->blueprint->bpClass == Tags::GAMEOBJCLASS_BUILDING) {
						if (building->orderConfig.getCurrentOrder() == nullptr) {
							int ordid = Server::instance->gameSet->orders.names.getIndex("Spawn " + charToSpawn.type->name);
							const OrderBlueprint* orderBp = &Server::instance->gameSet->orders[ordid];
							Order* order = building->orderConfig.addOrder(orderBp, Tags::ORDERASSIGNMODE_DO_FIRST, nullptr, {-1,-1,-1}, false);
							auto* task = (SpawnTask*)(order->tasks[0].get());
							task->toSpawn = charToSpawn.type;
							task->aiCommissioner = obj;
						}
					}
				}
			}
			else {
				completeCount += 1;
			}
		}

		// Building Requirements
		int isFoundationItem = Server::instance->gameSet->items.names.getIndex("Is a Foundation");
		for (size_t ri = 0; ri < cominst.buildingReqInsts.size(); ++ri) {
			auto& gsreq = gscomm->buildingRequirements[ri];
			auto& reqinst = cominst.buildingReqInsts[ri];
			auto it = std::remove_if(reqinst.foundations.begin(), reqinst.foundations.end(),
				[isFoundationItem](SrvGORef& ref) { return !(ref && ref->getItem(isFoundationItem) > 0.0f); }
			);
			reqinst.foundations.erase(it, reqinst.foundations.end());
			size_t requiredCount = (size_t)gsreq.vdCountNeeded->eval(&ctx);
			size_t existingCount = gsreq.fdAlreadyHaving->eval(&ctx).size();
			size_t inCtrCount = reqinst.foundations.size();
			while (existingCount + inCtrCount < requiredCount) {
				auto posori = gsreq.pdBuildPosition->eval(&ctx);
				const GameObjBlueprint* bpFoundation = Server::instance->gameSet->findBlueprint(Tags::GAMEOBJCLASS_BUILDING, gsreq.bpBuilding->name + " Foundation");
				ServerGameObject* foundation = Server::instance->spawnObject(bpFoundation, obj->getPlayer(), posori.position, Vector3(0,0,0));
				obj->sendEvent(Tags::PDEVENT_ON_COMMISSIONED, foundation);
				reqinst.foundations.emplace_back(foundation);
				inCtrCount = reqinst.foundations.size();
			}
			if (existingCount >= requiredCount) {
				completeCount += 1;
			}
		}

		// Upgrade Requirements
		for (size_t ri = 0; ri < cominst.upgradeReqInsts.size(); ++ri) {
			auto& gsreq = gscomm->upgradeRequirements[ri];
			auto& reqinst = cominst.upgradeReqInsts[ri];
			size_t requiredCount = (size_t)gsreq.vdCountNeeded->eval(&ctx);
			if (!reqinst.done && requiredCount > 0) {
				// find supporting buildings and launch upgrades
				// TODO: Prevent upgrades from happening multiple times (check preconditions)
				// might need to use COMMAND instead of ORDER
				SrvFinderResult buildingList = gsreq.fdSupportedBuildings->eval(&ctx);
				for (ServerGameObject* building : buildingList) {
					if (building->blueprint->bpClass == Tags::GAMEOBJCLASS_BUILDING) {
						if (building->orderConfig.getCurrentOrder() == nullptr) {
							OrderBlueprint* orderBp = gsreq.order;
							Order* order = building->orderConfig.addOrder(orderBp, Tags::ORDERASSIGNMODE_DO_FIRST);
							reqinst.done = true;
							break;
						}
					}
				}
			}
			else if (reqinst.done) {
				completeCount += 1;
			}
		}

		if (completeCount == cominst.characterReqInsts.size() + cominst.buildingReqInsts.size() + cominst.upgradeReqInsts.size()) {
			completedComInsts.push_back({ cominst.blueprint, cominst.handlerObject });
		}
	}

	for (auto& [com, hand] : completedComInsts) {
		hand->sendEvent(com->onCompleteEvent);
	}

	auto cit = std::remove_if(commissionInstances.begin(), commissionInstances.end(),
		[](auto& ci) -> bool {return !ci.handlerObject.get(); }
	);
	commissionInstances.erase(cit, commissionInstances.end());
}

void AIController::activatePlan(int planTag)
{
	auto& plan = Server::instance->gameSet->plans[planTag];
	this->planBlueprint = &plan;
	this->planState = plan.createState();
}

void AIController::abandonPlan()
{
	planBlueprint = nullptr;
	planState = {};
	// TODO: Free the state pointers
}

void AIController::registerWorkOrder(ServerGameObject* city, ObjectFinder* unitFinder, const WorkOrder* workOrder)
{
	WorkOrderInstance woi{ city, unitFinder, workOrder };
	workOrderInstances.push_back(std::move(woi));
}

void AIController::activateCommission(const GSCommission* commission, ServerGameObject* handler)
{
	deactivateCommission(commission, handler);
	CommissionInstance& ci = commissionInstances.emplace_back();
	ci.blueprint = commission;
	ci.handlerObject = handler;
	ci.characterReqInsts.resize(commission->characterRequirements.size());
	ci.buildingReqInsts.resize(commission->buildingRequirements.size());
	ci.upgradeReqInsts.resize(commission->upgradeRequirements.size());
	printf("COMMISSION \"%s\" ACTIVATED\n", Server::instance->gameSet->commissions.getString(commission).c_str());
}

void AIController::deactivateCommission(const GSCommission* commission, ServerGameObject* handler)
{
	for (auto it = commissionInstances.begin(); it != commissionInstances.end(); ++it) {
		auto& ci = *it;
		if (ci.blueprint == commission && ci.handlerObject.get() == handler) {
			commissionInstances.erase(it);
			return;
		}
	}
}
