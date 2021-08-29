#include "OrderBlueprint.h"
#include "../util/GSFileParser.h"
#include "../gameset/gameset.h"
#include "finder.h"
#include "../server.h"
#include "ScriptContext.h"

void OrderBlueprint::parse(GSFileParser & gsf, GameSet &gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		std::string tag = gsf.nextTag();
		if (tag == "END_ORDER")
			break;
		else if (tag == "IN_ORDER_CATEGORY")
			category = gs.orderCategories.readIndex(gsf);
		else if (tag == "CLASS_TYPE")
			this->classType = Tags::ORDTSKTYPE_tagDict.getTagID(gsf.nextString().c_str());
		else if (tag == "USE_TASK")
			this->tasks.push_back(gs.tasks.readPtr(gsf));
		else if (tag == "FLAG") {
			std::string flag = gsf.nextString();
			if (flag == "CYCLE_ORDER")
				cycleOrder = true;
			else if (flag == "CANNOT_INTERRUPT_ORDER")
				cannotInterruptOrder = true;
		}
		else if (tag == "PRE_CONDITION")
			preConditions.push_back(gs.equations.readIndex(gsf));
		else if (tag == "INITIALISATION_SEQUENCE")
			this->initSequence.init(gsf, gs, "END_INITIALISATION_SEQUENCE");
		else if (tag == "START_SEQUENCE")
			this->startSequence.init(gsf, gs, "END_START_SEQUENCE");
		else if (tag == "RESUMPTION_SEQUENCE")
			this->resumptionSequence.init(gsf, gs, "END_RESUMPTION_SEQUENCE");
		else if (tag == "TERMINATION_SEQUENCE")
			this->terminationSequence.init(gsf, gs, "END_TERMINATION_SEQUENCE");
		else if (tag == "CANCELLATION_SEQUENCE")
			this->cancellationSequence.init(gsf, gs, "END_CANCELLATION_SEQUENCE");
		gsf.advanceLine();
	}
}

void TaskBlueprint::parse(GSFileParser & gsf, GameSet &gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		std::string tag = gsf.nextTag();
		if (tag == "END_TASK")
			break;
		else if (tag == "IN_TASK_CATEGORY")
			category = gs.taskCategories.readIndex(gsf);
		else if (tag == "CLASS_TYPE")
			this->classType = Tags::ORDTSKTYPE_tagDict.getTagID(gsf.nextString().c_str());
		else if (tag == "TASK_TARGET")
			this->taskTarget = ReadFinder(gsf, gs);
		else if (tag == "INITIALISATION_SEQUENCE")
			this->initSequence.init(gsf, gs, "END_INITIALISATION_SEQUENCE");
		else if (tag == "START_SEQUENCE")
			this->startSequence.init(gsf, gs, "END_START_SEQUENCE");
		else if (tag == "RESUMPTION_SEQUENCE")
			this->resumptionSequence.init(gsf, gs, "END_RESUMPTION_SEQUENCE");
		else if (tag == "TERMINATION_SEQUENCE")
			this->terminationSequence.init(gsf, gs, "END_TERMINATION_SEQUENCE");
		else if (tag == "CANCELLATION_SEQUENCE")
			this->cancellationSequence.init(gsf, gs, "END_CANCELLATION_SEQUENCE");
		else if (tag == "TRIGGER") {
			auto trigname = gsf.nextString();
			if (trigname == "COLLISION")
				collisionTrigger.init(gsf, gs, "END_TRIGGER");
			else if (trigname == "STRUCK_FLOOR")
				struckFloorTrigger.init(gsf, gs, "END_TRIGGER");
			else {
				this->triggers.emplace_back(Tags::TASKTRIGGER_tagDict.getTagID(trigname.c_str()));
				TriggerBlueprint& trig = this->triggers.back();
				if (trig.type == Tags::TASKTRIGGER_TIMER)
					trig.period = ReadValueDeterminer(gsf, gs);
				trig.actions.init(gsf, gs, "END_TRIGGER");
			}
		}
		else if (tag == "PROXIMITY_REQUIREMENT")
			this->proximityRequirement = ReadValueDeterminer(gsf, gs);
		else if (tag == "PROXIMITY_SATISFIED_SEQUENCE")
			this->proximitySatisfiedSequence.init(gsf, gs, "END_PROXIMITY_SATISFIED_SEQUENCE");
		else if (tag == "PLAY_ANIMATION") {
			auto animtype = gsf.nextString();
			if (animtype == "TAG")
				this->defaultAnim = gs.animations.readIndex(gsf);
			else if (animtype == "FROM_SELECTION") {
				gsf.advanceLine();
				while (!gsf.eof) {
					auto tag = gsf.nextTag();
					if (tag == "ON_EQUATION_SUCCESS") {
						int equ = gs.equations.readIndex(gsf);
						int anim = gs.animations.readIndex(gsf);
						this->equAnims.emplace_back(equ, anim);
					}
					else if (tag == "DEFAULT_ANIMATION")
						this->defaultAnim = gs.animations.readIndex(gsf);
					else if (tag == "END_PLAY_ANIMATION")
						break;
					gsf.advanceLine();
				}
			}
		}
		else if (tag == "USE_PREVIOUS_TASK_TARGET")
			usePreviousTaskTarget = true;
		else if (tag == "TERMINATE_ENTIRE_ORDER_IF_NO_TARGET")
			terminateEntireOrderIfNoTarget = true;
		else if (tag == "REJECT_TARGET_IF_IT_IS_TERMINATED")
			rejectTargetIfItIsTerminated = true;
		else if (tag == "PLAY_ANIMATION_ONCE")
			playAnimationOnce = true;
		gsf.advanceLine();
	}
}

void OrderAssignmentBlueprint::parse(GSFileParser & gsf, GameSet &gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		std::string tag = gsf.nextTag();
		if (tag == "END_ORDER_ASSIGNMENT")
			break;
		else if (tag == "ORDER_TO_ASSIGN")
			this->orderToAssign = gs.orders.readPtr(gsf);
		else if (tag == "ORDER_ASSIGNMENT_MODE")
			this->orderAssignmentMode = Tags::ORDERASSIGNMODE_tagDict.getTagID(gsf.nextString().c_str());
		else if (tag == "ORDER_TARGET")
			this->orderTarget = ReadFinder(gsf, gs);
		gsf.advanceLine();
	}
}

void OrderAssignmentBlueprint::assignTo(ServerGameObject* gameobj, SrvScriptContext* ctx, ServerGameObject* giver) const
{
	auto _1 = ctx->self.change(gameobj);
	auto _2 = ctx->orderGiver.change(giver);
	ServerGameObject *target = this->orderTarget ? this->orderTarget->getFirst(ctx) : nullptr;
	gameobj->orderConfig.addOrder(this->orderToAssign, this->orderAssignmentMode, target);
}
