#include "ServerDebugger.h"
#include "../server.h"
#include "../imgui/imgui.h"
#include "../gameset/gameset.h"
#include "../file.h"
#include "../terrain.h"

void ServerDebugger::draw() {
	static std::vector<std::string> savegameList;
	static int selsavegame = 0;
	if (savegameList.empty()) {
		ListFiles("Save_Games", &savegameList);
	}

	ImGui::Begin("Server messages");
	//ImGui::ListBoxHeader("Savegame");
	//ImGui::Selectable("abc");
	//ImGui::ListBoxFooter();
	ImGui::ListBox("Savegame", &selsavegame, [](void *data, int idx, const char **out_text) -> bool {*out_text = ((std::vector<std::string>*)data)->at(idx).c_str(); return true; }, &savegameList, (int)savegameList.size());
	if (ImGui::Button("Load save")) {
		server->loadSaveGame(std::string("Save_Games\\").append(savegameList[selsavegame]).c_str()); //("Save_Games\\heymap.sav");
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

		if (ImGui::Button("Debug")) {
			__debugbreak();
		}

		Vector3 newpos = sel->position, newori = sel->orientation;
		if(ImGui::DragFloat3("Position", &newpos.x))
			sel->setPosition(newpos);
		if(ImGui::DragFloat2("Orientation", &newori.x))
			sel->setOrientation(newori);

		ImGui::Text("Subtype=%i, Appearance=%i", sel->subtype, sel->appearance);
		ImGui::Text("Items:");
		for (auto &item : sel->items)
			if (item.first != -1)
				ImGui::BulletText("\"%s\": %f", server->gameSet->items.names.getString(item.first).c_str(), item.second);

		ImGui::Text("Orders:");
		if (ImGui::Button("Add")) {
			ImGui::OpenPopup("AddNewOrder");
		}
		if (ImGui::BeginPopup("AddNewOrder")) {
			for (auto &it : server->gameSet->orders.names.str2idMap) {
				if (ImGui::Selectable(it.first.c_str())) {
					sel->orderConfig.addOrder(&server->gameSet->orders[it.second]);
				}
			}
			ImGui::EndPopup();
		}
		for (auto &order : sel->orderConfig.orders) {
			if (ImGui::TreeNode(&order, "Order %i s%i: %s", order.id, order.state, server->gameSet->orders.names.getString(order.blueprint->bpid).c_str())) {
				if (ImGui::Button("Break there")) {
					__debugbreak();
				}
				for (Task *task : order.tasks) {
					if (ImGui::TreeNode(task, "Task %i s%i: %s", task->id, task->state, server->gameSet->tasks.names.getString(task->blueprint->bpid).c_str())) {
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}
		}
		if (ImGui::Button("Move to center")) {
			sel->startMovement(Vector3(server->terrain->width - 2 * server->terrain->edge, 0.0f, server->terrain->height - 2 * server->terrain->edge) * 5.0f);
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop movement")) {
			sel->stopMovement();
		}
	}
	else
		ImGui::Text("No object selected.");
	ImGui::End();

	static bool showTilesWnd = false;
	ImGui::Begin("Additional windows");
	ImGui::Checkbox("Tiles", &showTilesWnd);
	ImGui::End();

	if (showTilesWnd && server->tiles) {
		ImGui::Begin("Tiles", &showTilesWnd, ImGuiWindowFlags_HorizontalScrollbar);
		int X, Z;
		std::tie(X, Z) = server->terrain->getNumPlayableTiles();
		static constexpr int rectsize = 8;
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 scr = ImGui::GetCursorScreenPos();
		//drawList->PrimReserve(6 * X * Z, 4 * X * Z);
		for (int z = Z-1; z >= 0; z--) {
			int y = Z - 1 - z;
			for (int x = 0; x < X; x++) {
				uint32_t color = server->tiles[z * X + x].building ? 0xFFFF00FF : 0xFF00FFFF;
				auto p1 = ImVec2(scr.x + x * rectsize, scr.y + y * rectsize);
				auto p2 = ImVec2(scr.x + (x + 1) * rectsize - 1, scr.y + (y + 1) * rectsize - 1);
				if (ImGui::IsRectVisible(p1, p2))
					drawList->AddRectFilled(p1, p2, color);
			}
		}
		ImGui::Dummy(ImVec2(X* rectsize, Z* rectsize));
		ImGui::End();
	}

	server->timeManager.imgui();
}