// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "GameSetDebugger.h"
#include "../imgui/imgui.h"
#include "../gameset/gameset.h"

template<typename T> static void enumBlueprintList(const char* name, const T& blist) {
	if (ImGui::TreeNode(name)) {
		int i = 0;
		for (auto& elem : blist.names.id2strVector) {
			ImGui::BulletText("%i: %s", i++, elem->first.c_str());
		}
		ImGui::TreePop();
	}
}

template<typename T, typename ... Args> static void enumBlueprintList(const char* name, const T& blist, const Args & ... rest) {
	enumBlueprintList(name, blist);
	enumBlueprintList(rest ...);
}

void GameSetDebugger::draw()
{
	if (!gameSet) return;
	ImGui::Begin("Game set");
	if (ImGui::TreeNode("Object blueprints")) {
		for (size_t i = 0; i < std::size(gameSet->objBlueprints); i++) {
			enumBlueprintList(Tags::GAMEOBJCLASS_tagDict.getStringFromID(i), gameSet->objBlueprints[i]);
		}
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Misc blueprints")) {
		enumBlueprintList(
			"items", gameSet->items,
			"equations", gameSet->equations,
			"actionSequences", gameSet->actionSequences,
			"appearances", gameSet->appearances,
			"commands", gameSet->commands,
			"animations", gameSet->animations,
			"orders", gameSet->orders,
			"tasks", gameSet->tasks,
			"orderAssignments", gameSet->orderAssignments,
			"aliases", gameSet->aliases,
			"events", gameSet->events,
			"reactions", gameSet->reactions,
			"prTriggers", gameSet->prTriggers,
			"objectCreations", gameSet->objectCreations,
			"objectFinderDefinitions", gameSet->objectFinderDefinitions,
			"associations", gameSet->associations,
			"typeTags", gameSet->typeTags,
			"valueTags", gameSet->valueTags,
			"diplomaticStatuses", gameSet->diplomaticStatuses,
			"conditions", gameSet->conditions,
			"gameTextWindows", gameSet->gameTextWindows,
			"taskCategories", gameSet->taskCategories,
			"orderCategories", gameSet->orderCategories,
			"packages", gameSet->packages,
			"clips", gameSet->clips,
			"cameraPaths", gameSet->cameraPaths
		);
		ImGui::TreePop();
	}
	ImGui::End();
}
