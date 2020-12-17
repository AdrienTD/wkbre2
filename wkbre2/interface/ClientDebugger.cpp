#include "ClientDebugger.h"
#include "../imgui/imgui.h"
#include "../client.h"
#include "../gameset/gameset.h"
#include "../gameset/GameObjBlueprint.h"
#include "../tags.h"

void ClientDebugger::draw()
{
	ImGui::Begin("Client messages");

	static char newmsg[256]; static int num = 0;
	ImGui::InputText("##newmsg", newmsg, sizeof(newmsg));
	ImGui::SameLine();
	if (ImGui::Button("Send"))
		client->sendMessage(newmsg);
	if (ImGui::Button("Send number"))
		client->sendMessage(itoa(num++, newmsg, 10));
	if (ImGui::Button("Stress")) {
		for (int i = 0; i < 256; i++)
			client->sendMessage(itoa(num++, newmsg, 10));
	}
	ImGui::Separator();

	for (const std::string &str : client->chatMessages)
		ImGui::Text("%s", str.c_str());
	ImGui::End();

	ImGui::Begin("Client Object Tree");
	const auto walkOnObj = [this](const auto &walkOnObj, ClientGameObject *obj) -> void {
		
		bool b = ImGui::TreeNodeEx(obj, /*ImGuiTreeNodeFlags_DefaultOpen |*/ (obj->children.empty() ? ImGuiTreeNodeFlags_Leaf : 0), "%i: %s \"%s\"", obj->id, Tags::GAMEOBJCLASS_tagDict.getStringFromID(obj->blueprint->bpClass), obj->blueprint->name.c_str());
		if (ImGui::IsItemClicked())
			selectedObject = obj;
		if (b) {
			for (auto typechildren : obj->children)
				for (ClientGameObject *child : typechildren.second)
					walkOnObj(walkOnObj, child);
			ImGui::TreePop();
		}
	};
	if (client->level)
		walkOnObj(walkOnObj, client->level);
	else
		ImGui::Text("No level loaded");
	ImGui::End();

	ImGui::Begin("Client Object Properties");
	if (selectedObject) {
		ClientGameObject *sel = selectedObject;
		ImGui::Text("Object ID %i at %p", sel->id, sel);
		ImGui::DragFloat3("Position", &sel->position.x);
		ImGui::DragFloat2("Orientation", &sel->orientation.x);
		ImGui::Text("Subtype=%i, Appearance=%i", sel->subtype, sel->appearance);
		ImGui::Text("Items:");
		for (auto item : sel->items)
			if (item.first != -1)
				ImGui::BulletText("\"%s\": %f", client->gameSet->itemNames.getString(item.first).c_str(), item.second);
		ImGui::Text("Commands:");
		for (Command *cmd : sel->blueprint->offeredCommands)
			if (ImGui::Button(client->gameSet->commandNames.getString(cmd->id).c_str()))
				client->sendCommand(sel, cmd);
	}
	else
		ImGui::Text("No object selected.");
	ImGui::End();

	ImGui::Begin("Camera");
	ImGui::DragFloat3("Position", &client->camera.position.x);
	ImGui::DragFloat2("Orientation", &client->camera.orientation.x, 0.1f);
	ImGui::DragFloat2("Cam distance", &client->camera.nearDist);
	ImGui::End();

	client->timeManager.imgui();
}
