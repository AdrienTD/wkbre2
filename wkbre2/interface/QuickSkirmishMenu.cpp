#include "QuickSkirmishMenu.h"
#include "../server.h"
#include "../imgui/imgui.h"
#include "../gameset/gameset.h"

#include "../settings.h"
#include <nlohmann/json.hpp>

void QuickSkirmishMenu::imguiWindow()
{
	const bool isWKBattles = g_settings.value<int>("gameVersion", 1) == 2;

	ImGui::SeparatorText("Gameplay mode");
	int modeSelection = (int)mode;
	ImGui::RadioButton("Dev mode", &modeSelection, (int)DevMode);
	ImGui::RadioButton("Standard mode", &modeSelection, (int)StandardMode);
	if (isWKBattles) {
		ImGui::RadioButton("Valhalla mode", &modeSelection, (int)ValhallaMode);
	}
	mode = (GameMode)modeSelection;

	ImGui::PushItemWidth(200.0f);

	if (mode == StandardMode) {
		ImGui::SeparatorText("Skirmish settings");
		ImGui::InputInt("Start Food", &startFood, 50, 100);
		ImGui::InputInt("Start Wood", &startWood, 50, 100);
		ImGui::InputInt("Start Gold", &startGold, 50, 100);
		ImGui::InputInt("Population Limit", &populationLimit, 10, 50);

		if (isWKBattles) {
			static const char* randomResourceValueNames[4] = {
				"Off", "Low", "Medium", "High"
			};
			ImGui::Combo("Randomise Resources", &randomiseResources, randomResourceValueNames, 4);
			
			ImGui::Checkbox("King Piece", &kingPiece);
			ImGui::Checkbox("Clear Fog of War", &clearFogOfWar);
			ImGui::Checkbox("Allow Diplomacy", &allowDiplomacy);
			ImGui::Checkbox("Team Reconnaissance", &teamReconnaissance);
			ImGui::Checkbox("Barbarian Tribes", &barbarianTribes);
			ImGui::Checkbox("Campaign Mode", &campaignMode);
		}
		else {
			ImGui::SeparatorText("Victory Condition");
			ImGui::RadioButton("Destroy Manor", &loseOnManorDestroyed, 1);
			ImGui::RadioButton("Destroy All Enemy Units", &loseOnManorDestroyed, 0);
		}
	}
	else if (mode == ValhallaMode) {
		ImGui::SeparatorText("Valhalla settings");
		ImGui::InputInt("Resource Points", &resourcePoints, 100, 200);
		ImGui::InputInt("Victory Point Limit", &victoryPointLimit, 500, 1000);
		ImGui::InputInt("Valhalla Mode Food", &valhallaModeFood, 500, 1000);
		ImGui::InputInt("Unit Lives", &unitLives);
		ImGui::InputInt("Capturable Flags", &capturableFlags);
		ImGui::Checkbox("Clear Fog of War", &clearFogOfWar);
	}

	ImGui::PopItemWidth();
}

void QuickSkirmishMenu::applySettings(Server& server)
{
	auto setItemByName = [&server](ServerGameObject* obj, const char* name, float value) {
		int item = server.gameSet->items.names.getIndex(name);
		if (item != -1) {
			obj->setItem(item, value);
		}
		};

	ServerGameObject* level = server.getLevel();
	const bool isWKBattles = server.gameSet->version == GameSet::GSVERSION_WKBATTLES;

	if (mode == StandardMode) {
		for (auto& [objType, objList] : level->children) {
			for (auto* comObj : objList) {
				ServerGameObject* obj = comObj->dyncast<ServerGameObject>();
				if (obj->blueprint->bpClass == Tags::GAMEOBJCLASS_PLAYER) {
					obj->setItem(Tags::PDITEM_FOOD, startFood);
					obj->setItem(Tags::PDITEM_WOOD, startWood);
					obj->setItem(Tags::PDITEM_GOLD, startGold);
					setItemByName(obj, "Total Population Count", populationLimit);

					if (isWKBattles) {
						setItemByName(obj, "King Piece Lose Condition Set", kingPiece);
						setItemByName(obj, "Diplomacy Enabled", allowDiplomacy);
					}
					else {
						setItemByName(obj, "Lose on Manor Destroyed", loseOnManorDestroyed);
					}
				}
			}
		}

		if (isWKBattles) {
			setItemByName(level, "Amount of Resources", randomiseResources);
			setItemByName(level, "Random Barbarians Chosen", barbarianTribes);
			setItemByName(level, "Skirmish Campaign Level", campaignMode);
		}
	}

	if (mode == ValhallaMode && isWKBattles) {
		setItemByName(level, "Valhalla Mode Chosen", 1.0f);
		setItemByName(level, "Number of Capturable Flags", capturableFlags);

		for (auto& [objType, objList] : level->children) {
			for (auto* comObj : objList) {
				ServerGameObject* obj = comObj->dyncast<ServerGameObject>();
				if (obj->blueprint->bpClass == Tags::GAMEOBJCLASS_PLAYER) {
					setItemByName(obj, "Valhalla Resource Points", resourcePoints);
					setItemByName(obj, "Victory Time Limit", victoryPointLimit);
					setItemByName(obj, "Food", valhallaModeFood);
					setItemByName(obj, "No. Of Lives Left", unitLives);

				}
			}
		}
	}

	if (mode != DevMode && isWKBattles) {
		auto* humanPlayer = server.gameSet->findBlueprint(Tags::GAMEOBJCLASS_PLAYER, "Human Player");
		const char* aiPlayerNames[] = {
			"Ignis the Lord Protector Kingmaker",
			"Xaphan the Keeper of the Furnace Kingmaker",
			"Grand Duke Guiscard Kingmaker",
			"Lukius, the Iron Priest Kingmaker",
			"Marjoria, War Queen of the Dawn Kingmaker",
			"Princess Sumarya the Vengeful Kingmaker",
			"Katkerry Gan Kingmaker",
			"Ichthyus Granitas Kingmaker",
		};
		int remainingAiTypes = std::size(aiPlayerNames);
		auto playerList = server.getLevel()->children.at(humanPlayer);
		for (auto& obj : playerList) {
			if (obj->id == 1027)
				continue;
			int aiTypeIndex = rand() % remainingAiTypes;
			const char* aiName = aiPlayerNames[aiTypeIndex];
			std::swap(aiPlayerNames[aiTypeIndex], aiPlayerNames[remainingAiTypes - 1]);
			remainingAiTypes -= 1;
			if (remainingAiTypes == 0) {
				remainingAiTypes = std::size(aiPlayerNames);
			}

			auto* aiPlayer = server.gameSet->findBlueprint(Tags::GAMEOBJCLASS_PLAYER, aiName);
			if (aiPlayer) {
				obj->dyncast<ServerGameObject>()->convertTo(aiPlayer);
			}
		}
	}

	if (mode != DevMode) {
		server.startLevel();
	}
}
