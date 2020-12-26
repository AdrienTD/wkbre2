#include "OrderBlueprint.h"
#include "../util/GSFileParser.h"
#include "../gameset/gameset.h"
#include "finder.h"
#include "../server.h"

void OrderBlueprint::parse(GSFileParser & gsf, GameSet &gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		std::string tag = gsf.nextTag();
		if (tag == "END_ORDER")
			break;
		else if (tag == "CLASS_TYPE")
			this->classType = Tags::ORDTSKTYPE_tagDict.getTagID(gsf.nextString().c_str());
		else if (tag == "USE_TASK")
			this->tasks.push_back(gs.tasks.readPtr(gsf));
		else if (tag == "FLAG") {
			std::string flag = gsf.nextString();
			if (flag == "CYCLE_ORDER")
				cycleOrder = true;
		}
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
			this->triggers.emplace_back(Tags::TASKTRIGGER_tagDict.getTagID(gsf.nextString().c_str()));
			TriggerBlueprint &trig = this->triggers.back();
			if (trig.type == Tags::TASKTRIGGER_TIMER)
				trig.period = ReadValueDeterminer(gsf, gs);
			trig.actions.init(gsf, gs, "END_TRIGGER");
		}
		else if (tag == "PROXIMITY_REQUIREMENT")
			this->proximityRequirement = ReadValueDeterminer(gsf, gs);
		else if (tag == "PROXIMITY_SATISFIED_SEQUENCE")
			this->proximitySatisfiedSequence.init(gsf, gs, "END_PROXIMITY_SATISFIED_SEQUENCE");
		else if (tag == "PLAY_ANIMATION") {
			if (gsf.nextString() == "TAG")
				this->defaultAnim = gs.animations.readIndex(gsf);
		}
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

void OrderAssignmentBlueprint::assignTo(ServerGameObject * gameobj) const
{
	ServerGameObject *target = this->orderTarget ? this->orderTarget->getFirst(gameobj) : nullptr;
	gameobj->orderConfig.addOrder(this->orderToAssign, this->orderAssignmentMode, target);
}
