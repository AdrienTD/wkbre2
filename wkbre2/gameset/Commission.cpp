#include "Commission.h"
#include "gameset.h"
#include "util/GSFileParser.h"
#include "finder.h"

void GSCharacterLadder::parse(GSFileParser& gsf, GameSet& gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		auto tag = gsf.nextTag();
		if (tag == "CHARACTER") {
			LadderEntry& le = entries.emplace_back();
			le.type = gs.objBlueprints[Tags::GAMEOBJCLASS_CHARACTER].readPtr(gsf);
			le.finder = ReadFinder(gsf, gs);
		}
		else if (tag == "END_CHARACTER_LADDER") {
			break;
		}
		gsf.advanceLine();
	}
}

void GSCommission::parse(GSFileParser& gsf, GameSet& gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		auto tag = gsf.nextTag();
		if (tag == "PRIORITY") {
			priority = ReadValueDeterminer(gsf, gs);
		}
		else if (tag == "CHARACTER_REQUIREMENT") {
			CharacterRequirement& req = characterRequirements.emplace_back();
			req.ladder = gs.characterLadders.readPtr(gsf);
			req.vdCountNeeded = ReadValueDeterminer(gsf, gs);
			req.fdAlreadyHaving = ReadFinder(gsf, gs);
		}
		else if (tag == "BUILDING_REQUIREMENT") {
			BuildingRequirement& req = buildingRequirements.emplace_back();
			req.bpBuilding = gs.objBlueprints[Tags::GAMEOBJCLASS_BUILDING].readPtr(gsf);
			req.pdBuildPosition = PositionDeterminer::createFrom(gsf, gs);
			req.vdCountNeeded = ReadValueDeterminer(gsf, gs);
			req.fdAlreadyHaving = ReadFinder(gsf, gs);
		}
		else if (tag == "UPGRADE_REQUIREMENT") {
			UpgradeRequirement& req = upgradeRequirements.emplace_back();
			req.order = gs.orders.readPtr(gsf);
			req.vdCountNeeded = ReadValueDeterminer(gsf, gs);
			req.fdSupportedBuildings = ReadFinder(gsf, gs);
		}
		else if (tag == "SUSPENSION_CONDITION") {
			suspensionCondition = ReadValueDeterminer(gsf, gs);
		}
		else if (tag == "ON_COMPLETE_EVENT") {
			onCompleteEvent = gs.events.readIndex(gsf);
		}
		else if (tag == "ON_STALLED_EVENT") {
			onStalledEvent = gs.events.readIndex(gsf);
		}
		else if (tag == "END_COMMISSION") {
			break;
		}
		gsf.advanceLine();
	}
}
