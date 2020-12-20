#include "ServerDebugger.h"
#include "../server.h"
#include "../imgui/imgui.h"
#include "../gameset/gameset.h"
#include "../file.h"
#include "../util/growbuffer.h"

void ServerDebugger::draw() {
	static GrowStringList savegameList;
	static int selsavegame = 0;
	if (savegameList.len == 0) {
		ListFiles("Save_Games", &savegameList);
	}

	ImGui::Begin("Server messages");
	//ImGui::ListBoxHeader("Savegame");
	//ImGui::Selectable("abc");
	//ImGui::ListBoxFooter();
	ImGui::ListBox("Savegame", &selsavegame, [](void *data, int idx, const char **out_text) -> bool {*out_text = ((GrowStringList*)data)->getdp(idx); return true; }, &savegameList, savegameList.len);
	if (ImGui::Button("Load save")) {
		server->loadSaveGame(std::string("Save_Games\\").append(savegameList.getdp(selsavegame)).c_str()); //("Save_Games\\heymap.sav");
	}
	for (const std::string &str : server->chatMessages)
		ImGui::Text("%s", str.c_str());
	ImGui::End();

	ImGui::Begin("Server Object Tree");
	const auto walkOnObj = [this](const auto &walkOnObj, ServerGameObject *obj) -> void {
		bool b = ImGui::TreeNodeEx(obj, ImGuiTreeNodeFlags_DefaultOpen | (obj->children.empty() ? ImGuiTreeNodeFlags_Leaf : 0), "%i: %s \"%s\"", obj->id, Tags::GAMEOBJCLASS_tagDict.getStringFromID(obj->blueprint->bpClass), obj->blueprint->name.c_str());
		if (ImGui::IsItemClicked())
			selectedObject = obj;
		if (b) {
			for (auto typechildren : obj->children)
				for (ServerGameObject *child : typechildren.second)
					walkOnObj(walkOnObj, child);
			ImGui::TreePop();
		}
	};
	if (server->level)
		walkOnObj(walkOnObj, server->level);
	else
		ImGui::Text("No level loaded");
	ImGui::End();

	ImGui::Begin("Server Object Properties");
	if (selectedObject) {
		ServerGameObject *sel = selectedObject;
		ImGui::Text("Object ID %i at %p", sel->id, sel);

		Vector3 newpos = sel->position, newori = sel->orientation;
		if(ImGui::DragFloat3("Position", &newpos.x))
			sel->setPosition(newpos);
		if(ImGui::DragFloat2("Orientation", &newori.x))
			sel->setOrientation(newori);

		ImGui::Text("Subtype=%i, Appearance=%i", sel->subtype, sel->appearance);
		ImGui::Text("Items:");
		for (auto item : sel->items)
			if (item.first != -1)
				ImGui::BulletText("\"%s\": %f", server->gameSet->itemNames.getString(item.first).c_str(), item.second);

		ImGui::Text("Orders:");
		if (ImGui::Button("Add")) {
			ImGui::OpenPopup("AddNewOrder");
		}
		if (ImGui::BeginPopup("AddNewOrder")) {
			for (auto &it : server->gameSet->orderNames.str2idMap) {
				if (ImGui::Selectable(it.first.c_str())) {
					sel->orderConfig.addOrder(&server->gameSet->orders[it.second]);
				}
			}
			ImGui::EndPopup();
		}
		for (auto &order : sel->orderConfig.orders) {
			if (ImGui::TreeNode(&order, "Order %i s%i: %s", order.id, order.state, server->gameSet->orderNames.getString(order.blueprint->bpid).c_str())) {
				if (ImGui::Button("Break there")) {
					__debugbreak();
				}
				for (Task *task : order.tasks) {
					if (ImGui::TreeNode(task, "Task %i s%i: %s", task->id, task->state, server->gameSet->taskNames.getString(task->blueprint->bpid).c_str())) {
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}
		}
	}
	else
		ImGui::Text("No object selected.");
	ImGui::End();

	server->timeManager.imgui();
}