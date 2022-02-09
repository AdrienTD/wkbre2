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
			ServerGameObject* unit = units.back();
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
