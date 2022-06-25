#pragma once

#include <memory>
#include <vector>

struct GameObjBlueprint;
struct GSFileParser;
struct GameSet;
struct ValueDeterminer;
struct PositionDeterminer;
struct OrderBlueprint;
struct ObjectFinder;

struct GSCharacterLadder {
	struct LadderEntry {
		GameObjBlueprint* type;
		ObjectFinder* finder;
	};
	std::vector<LadderEntry> entries;
	void parse(GSFileParser& gsf, GameSet& gs);
};

struct GSCommission {
	struct Requirement {
		ValueDeterminer* vdCountNeeded;
	};
	struct CharacterRequirement : Requirement {
		GSCharacterLadder* ladder;
		ObjectFinder* fdAlreadyHaving;
	};
	struct BuildingRequirement : Requirement {
		GameObjBlueprint* bpBuilding;
		PositionDeterminer* pdBuildPosition;
		ObjectFinder* fdAlreadyHaving;
	};
	struct UpgradeRequirement : Requirement {
		OrderBlueprint* order;
		ObjectFinder* fdSupportedBuildings;
	};
	ValueDeterminer* priority = nullptr;
	std::vector<CharacterRequirement> characterRequirements;
	std::vector<BuildingRequirement> buildingRequirements;
	std::vector<UpgradeRequirement> upgradeRequirements;
	ValueDeterminer* suspensionCondition = nullptr;
	void parse(GSFileParser& gsf, GameSet& gs);
};