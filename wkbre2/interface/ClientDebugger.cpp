#include "ClientDebugger.h"
#include "../imgui/imgui.h"
#include "../client.h"
#include "../gameset/gameset.h"
#include "../gameset/GameObjBlueprint.h"
#include "../tags.h"
#include "../window.h"
#include "ClientInterface.h"

extern int g_diag_animHits, g_diag_animMisses;

namespace {
	const ImVec4 ivcolortable[8] = {
		ImVec4(0.5f, 0.25f, 0.25f, 1), ImVec4(0, 0, 1, 1), ImVec4(1, 1, 0, 1), ImVec4(1, 0, 0, 1),
		ImVec4(0, 1, 0, 1), ImVec4(1, 0, 1, 1), ImVec4(1, 0.5f, 0, 1), ImVec4(0, 1, 1, 1)
	};

	bool IGPlayerChooser(char *popupid, CliGORef &ref, Client *client)
	{
		bool r = false;
		ImGui::PushID(popupid);
		ImGui::PushItemWidth(-1);
		ImVec2 bpos = ImGui::GetCursorPos();
		int tlh = ImGui::GetTextLineHeight();
		if (ImGui::BeginCombo("##Combo", "", 0))
		{
			for (auto &type : client->level->children) {
				if ((type.first & 63) == Tags::GAMEOBJCLASS_PLAYER) {
					for (ClientGameObject *player : type.second) {
						ImGui::PushID(player->id);
						if (ImGui::Selectable("##PlayerSelectable")) {
							ref = player;
							r = true;
						}
						ImGui::SameLine();
						ImGui::Image(nullptr, ImVec2(tlh, tlh), ImVec2(0, 0), ImVec2(1, 1), ivcolortable[player->color]);
						ImGui::SameLine();
						ImGui::Text("%S (%u)", L"TODO", player->id);
						ImGui::PopID();
					}
				}
			}
			ImGui::EndCombo();
		}
		ImGui::SetCursorPos(bpos);
		ImGui::AlignTextToFramePadding();
		tlh = ImGui::GetTextLineHeightWithSpacing();
		ClientGameObject *player = ref.get();
		if (player) {
			ImGui::Image(nullptr, ImVec2(tlh, tlh), ImVec2(0, 0), ImVec2(1, 1), ivcolortable[player->color]);
			ImGui::SameLine();
			ImGui::Text("%S (%u)", L"TODO", player->id);
		}
		else {
			ImGui::Image(nullptr, ImVec2(tlh, tlh), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1));
			ImGui::SameLine();
			ImGui::Text("Click me to select a player.");
		}
		ImGui::PopItemWidth();
		ImGui::PopID();
		return r;
	}
}

void ClientDebugger::draw()
{
	ImGui::Begin("Client messages");

	static char newmsg[256]; static int num = 0;
	ImGui::PushItemWidth(-1.0f);
	bool msgEntered = ImGui::InputText("##newmsg", newmsg, sizeof(newmsg), ImGuiInputTextFlags_EnterReturnsTrue);
	ImGui::PopItemWidth();
	msgEntered |= ImGui::Button("Send");
	if (msgEntered)
		client->sendMessage(newmsg);
	ImGui::SameLine();
	if (ImGui::Button("Send number"))
		client->sendMessage(itoa(num++, newmsg, 10));
	ImGui::SameLine();
	if (ImGui::Button("Stress")) {
		for (int i = 0; i < 256; i++)
			client->sendMessage(itoa(num++, newmsg, 10));
	}
	ImGui::Separator();
	ImGui::BeginChild("ChatMessages", ImVec2(0,0), false);
	for (const std::string &str : client->chatMessages)
		ImGui::Text("%s", str.c_str());
	static size_t numMessages = client->chatMessages.size();
	if (numMessages != client->chatMessages.size()) {
		numMessages = client->chatMessages.size();
		ImGui::SetScrollHereY();
	}
	ImGui::EndChild();
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
		ImGui::Text("Object ID %i at %p\n%s", sel->id, sel, sel->blueprint->getFullName().c_str());
		ImGui::DragFloat3("Position", &sel->position.x);
		ImGui::DragFloat2("Orientation", &sel->orientation.x);
		ImGui::Text("Subtype=%i, Appearance=%i", sel->subtype, sel->appearance);
		ImGui::Text("Items:");
		for (auto item : sel->items)
			if (item.first != -1)
				ImGui::BulletText("\"%s\": %f", client->gameSet->items.names.getString(item.first).c_str(), item.second);
		ImGui::Text("Commands:");
		static int targetid = 0;
		ImGui::InputInt("Target ID", &targetid);
		auto &io = ImGui::GetIO();
		int assignmentMode = io.KeyCtrl ? Tags::ORDERASSIGNMODE_DO_FIRST : (io.KeyShift ? Tags::ORDERASSIGNMODE_DO_LAST : Tags::ORDERASSIGNMODE_FORGET_EVERYTHING_ELSE);
		for (Command *cmd : sel->blueprint->offeredCommands)
			if (ImGui::Button(client->gameSet->commands.names.getString(cmd->id).c_str()))
				client->sendCommand(sel, cmd, assignmentMode, client->findObject(targetid));
	}
	else
		ImGui::Text("No object selected.");
	ImGui::End();

	ImGui::Begin("Camera");
	ImGui::DragFloat3("Position", &client->camera.position.x);
	ImGui::DragFloat2("Orientation", &client->camera.orientation.x, 0.1f);
	ImGui::DragFloat2("Cam distance", &client->camera.nearDist);
	ImGui::End();

	if (client->gameSet) {
		ImGui::Begin("Stampdown");
		//ImGui::Text("En chantier");
		IGPlayerChooser("StampdownPlayerSelect", cliUi->stampdownPlayer, client);
		ImGui::Checkbox("Send Stampdown Event", &cliUi->sendStampdownEvent);
		ImGui::Separator();
		ImGui::BeginChild("StampdownList", ImVec2(0, 0), false);
		for (auto &cls : client->gameSet->objBlueprints) {
			for (auto &el : cls.names.str2idMap) {
				GameObjBlueprint &type = cls.blueprints[el.second];
				if (ImGui::Selectable(type.getFullName().c_str()))
					cliUi->stampdownBlueprint = &type;
			}
		}
		ImGui::EndChild();
		ImGui::End();
	}

	ImGui::Begin("Diagnostics");
	ImGui::Text("animHits = %i", g_diag_animHits);
	ImGui::Text("animMisses = %i", g_diag_animMisses);
	ImGui::End();
	g_diag_animHits = 0;
	g_diag_animMisses = 0;

	client->timeManager.imgui();

	if (client->gameSet) {
		gsDebugger.setGameSet(client->gameSet);
		gsDebugger.draw();
	}
}
