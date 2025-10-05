// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "GameSetDebugger.h"
#include "../imgui/imgui.h"
#include "../gameset/gameset.h"
#include "../file.h"
#include "../BreakpointManager.h"

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

std::map<int, std::vector<std::string>> g_openGsfFiles;

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
	if (ImGui::TreeNode("Game set files")) {
		for (auto& [gsfFileName, gsfFileIndex] : gameSet->gsfFileList.str2idMap) {
			if (ImGui::Selectable(gsfFileName.c_str())) {
				if (!g_openGsfFiles.count(gsfFileIndex)) {
					char* buf = nullptr; int size = 0;
					LoadFile(gsfFileName.c_str(), &buf, &size);
					auto& lines = g_openGsfFiles[gsfFileIndex];
					int lineStart = 0;
					int ptr = 0;
					while (ptr < size) {
						if (buf[ptr] == '\n') {
							std::string line = std::string(buf + lineStart, buf + ptr);
							if (!line.empty() && line.back() == '\r') line.pop_back();
							lines.push_back(std::move(line));
							lineStart = ptr + 1;
						}
						++ptr;
					}
					lines.push_back(std::string(buf + ptr, buf + size));
				}
			}
		}
		ImGui::TreePop();
	}
	ImGui::End();

	// GSF Scripting Debugger
	for (auto it = g_openGsfFiles.begin(); it != g_openGsfFiles.end();) {
		const auto& [gsfIndex, gsfFileContent] = *it;
		bool keepOpen = true;
		ImGui::SetNextWindowSize(ImVec2(640.0f, 480.0f), ImGuiCond_Appearing);
		bool visible = ImGui::Begin(gameSet->gsfFileList[gsfIndex].c_str(), &keepOpen, ImGuiWindowFlags_NoSavedSettings);
		if (visible) {
			auto& breakpointMgr = BreakpointManager::instance();
			auto currentBreak = breakpointMgr.getCurrentBreakLine();
			ImGui::BeginDisabled(!currentBreak);
			if(ImGui::Button("Resume")) {
				breakpointMgr.resumeFromBreak();
			}
			ImGui::SameLine();
			if (ImGui::Button("Step into")) {
				breakpointMgr.stepInto();
			}
			ImGui::EndDisabled();
			ImGui::Separator();
			const bool contentVisible = ImGui::BeginChild("ScriptContent");
			if (contentVisible) {
				ImGuiListClipper clipper((int)gsfFileContent.size());
				while (clipper.Step()) {
					for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
						ImGui::PushID(i);
						const int lineNumber = i + 1;
						bool breakpoint = breakpointMgr.isBreakpointSet(gsfIndex, lineNumber);
						const float buttonSize = ImGui::GetTextLineHeight();
						auto buttonPos = ImGui::GetCursorScreenPos();
						if (ImGui::InvisibleButton("BreakpointButton", ImVec2(buttonSize, buttonSize))) {
							breakpoint = !breakpoint;
							if (breakpoint) {
								breakpointMgr.setBreakpoint(gsfIndex, lineNumber);
							}
							else {
								breakpointMgr.removeBreakpoint(gsfIndex, lineNumber);
							}
						}
						if (breakpoint || ImGui::IsItemHovered()) {
							const float radius = buttonSize / 2.0f;
							ImGui::GetWindowDrawList()->AddCircleFilled(
								ImVec2{ buttonPos.x + radius, buttonPos.y + radius },
								radius, breakpoint ? 0xFF0000C0 : 0xFF808080);
						}
						ImGui::SameLine();
						ImGui::PushStyleColor(ImGuiCol_Text, 0xFF808080);
						ImGui::Text("%4i", lineNumber);
						ImGui::PopStyleColor();
						ImGui::SameLine();
						const bool broken = currentBreak == std::make_pair(gsfIndex, lineNumber);
						ImGui::PushStyleColor(ImGuiCol_Text, broken ? 0xFF8080FF : 0xFFFFFFFF);
						ImGui::TextUnformatted(gsfFileContent[i].c_str());
						ImGui::PopStyleColor();
						ImGui::PopID();
					}
				}
			}
			ImGui::EndChild();
		}
		ImGui::End();

		if (!keepOpen) {
			it = g_openGsfFiles.erase(it);
		}
		else {
			++it;
		}
	}
}
