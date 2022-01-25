// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <vector>
#include "actions.h"

struct GameObjBlueprint;
struct GSFileParser;
struct GameSet;
struct ValueDeterminer;

struct ArmyCreationSchedule {
	struct SpawnChar {
		GameObjBlueprint* type = nullptr;
		std::unique_ptr<ValueDeterminer> count = 0;
	};
	GameObjBlueprint* armyType = nullptr;
	std::vector<SpawnChar> spawnCharacters;
	ActionSequence armyReadySequence;

	void parse(GSFileParser& gsf, const GameSet& gs);
};