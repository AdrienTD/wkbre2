// wkbre2 - WK Engine Reimplementation
// (C) 2021-2022 AdrienTD
// Licensed under the GNU General Public License 3

#include "WorkOrder.h"
#include "gameset.h"
#include "../util/GSFileParser.h"
#include "finder.h"

void WorkOrder::parse(GSFileParser& gsf, const GameSet& gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		auto tag = gsf.nextTag();
		if (tag == "ASSIGN_WORKERS") {
			Assignment asg;

			auto str = gsf.nextString();
			if (str == "INDIVIDUALS") asg.quantityType = Assignment::INDIVIDUALS;
			else if (str == "FRACTION_DOWN") asg.quantityType = Assignment::FRACTION_DOWN;
			else if (str == "REMAINING_WORKERS") asg.quantityType = Assignment::REMAINING_WORKERS;

			if (asg.quantityType != Assignment::REMAINING_WORKERS)
				asg.quantityValue = ReadValueDeterminer(gsf, gs);

			asg.order = gs.orders.readPtr(gsf);

			str = gsf.nextString();
			if (str == "PER") asg.distType = Assignment::DIST_PER;
			else if (str == "BETWEEN") asg.distType = Assignment::DIST_BETWEEN;
			else if (str == "AUTO_IDENTIFY") asg.distType = Assignment::DIST_AUTO_IDENTIFY;

			if (asg.distType != Assignment::DIST_AUTO_IDENTIFY)
				asg.distFinder = ReadFinder(gsf, gs);

			assignments.push_back(std::move(asg));
		}
		else if (tag == "END_WORK_ORDER")
			break;
		gsf.advanceLine();
	}
}
