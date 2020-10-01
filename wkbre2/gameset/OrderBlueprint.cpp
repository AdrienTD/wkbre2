#include "OrderBlueprint.h"
#include "../util/GSFileParser.h"
#include "../gameset/gameset.h"
#include "finder.h"

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
			this->tasks.push_back(&gs.tasks[gs.taskNames.getIndex(gsf.nextString(true))]);
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
		else if (tag == "START_SEQUENCE")
			this->startSequence.init(gsf, gs, "END_START_SEQUENCE");
		else if (tag == "TRIGGER") {
			TriggerBlueprint trig = TriggerBlueprint(Tags::TASKTRIGGER_tagDict.getTagID(gsf.nextString().c_str()));
			if (trig.type == Tags::TASKTRIGGER_TIMER)
				trig.period = ReadValueDeterminer(gsf, gs);
			trig.actions.init(gsf, gs, "END_TRIGGER");
			this->triggers.push_back(std::move(trig));
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
			this->orderToAssign = &gs.orders[gs.orderNames.getIndex(gsf.nextString(true))];
		else if (tag == "ORDER_ASSIGMENT_MODE")
			this->orderAssignmentMode = Tags::ORDERASSIGNMODE_tagDict.getTagID(gsf.nextString().c_str());
		else if (tag == "ORDER_TARGET")
			this->orderTarget = ReadFinder(gsf, gs);
		gsf.advanceLine();
	}
}