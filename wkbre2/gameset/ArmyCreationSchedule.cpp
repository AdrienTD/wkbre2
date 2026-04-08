// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "ArmyCreationSchedule.h"
#include "util/GSFileParser.h"
#include "gameset.h"

void ArmyCreationSchedule::parse(GSFileParser& gsf, const GameSet& gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		auto tag = gsf.nextTag();
		if (tag == "ARMY_TYPE")
			armyType = gs.objBlueprints[Tags::GAMEOBJCLASS_ARMY].readPtr(gsf);
		else if (tag == "SPAWN_CHARACTER") {
			const GameObjBlueprint* bp = gs.objBlueprints[Tags::GAMEOBJCLASS_CHARACTER].readPtr(gsf);
			ValueDeterminer* val = ReadValueDeterminer(gsf, gs);
			SpawnChar spawnChar = { bp, std::unique_ptr<ValueDeterminer>(val) };
			spawnCharacters.push_back(std::move(spawnChar));
		}
		else if (tag == "ARMY_READY_SEQUENCE") {
			armyReadySequence.init(gsf, gs, "END_ARMY_READY_SEQUENCE");
		}
		else if (tag == "END_ARMY_CREATION_SCHEDULE")
			break;
		gsf.advanceLine();
	}
}
