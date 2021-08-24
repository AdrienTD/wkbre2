#include "QuickStartMenu.h"
#include "../file.h"
#include "../imgui/imgui.h"
#include "../imguiimpl.h"
#include "../window.h"
#include "../gfx/renderer.h"
#include <SDL2/SDL_keycode.h>

#include "../server.h"
#include "../client.h"
#include "../network.h"
#include "ClientInterface.h"
#include "ServerDebugger.h"
#include "../SoundPlayer.h"
#include "../netenetlink.h"

QuickStartMenu::QuickStartMenu(IRenderer* gfx) : gfx(gfx)
{
	auto* gsl = ListFiles("Save_Games");
	savegames = std::move(*gsl);
	delete gsl;
	//SoundPlayer::getSoundPlayer()->playMusic("Warrior Kings Game Set\\Sounds\\Music\\title_version_2.mp2");
	
	server = new Server;
	client = new Client;
	queue1 = new NetLocalBuffer;
	queue2 = new NetLocalBuffer;
	srv2cli = new NetLocalLink(queue1, queue2);
	cli2srv = new NetLocalLink(queue2, queue1);
	server->addClient(srv2cli);
	client->serverLink = cli2srv;
}

void QuickStartMenu::draw()
{	
	ImGui::Begin("Select sav");
	ImGui::PushItemWidth(-1.0f);
	ImGui::ListBoxHeader("##SaveBox");
	for (size_t i = 0; i < savegames.size(); i++) {
		if (ImGui::Selectable(savegames[i].c_str(), savselected == i)) {
			savselected = i;
		}
	}
	ImGui::ListBoxFooter();
	ImGui::PopItemWidth();
	if (ImGui::Button("Load"))
		modeChosen = MODE_SINGLEPLAYER;
	ImGui::SameLine();
	if (ImGui::Button("Host")) {
		netInit();
		showHostWnd = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Join"))
		showJoinWnd = true;
	ImGui::End();

	if (showHostWnd) {
		ImGui::Begin("Host game", &showHostWnd);
		char taddress[64];
		enet_address_get_host_ip(&eserver->address, taddress, sizeof(taddress));
		ImGui::Text("IP Address: %s", taddress);
		ImGui::Text("Num players: %zu", server->clientLinks.size());
		ImGui::Separator();
		ImGui::BeginChild("Chat", ImVec2(0, 128), true);
		for (auto& s : server->chatMessages)
			ImGui::TextUnformatted(s.data(), s.data() + s.size());
		ImGui::EndChild();
		static char chatInput[128];
		bool enter = ImGui::InputText("##ChatInput", chatInput, sizeof(chatInput), ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::SameLine();
		enter |= ImGui::Button("Send");
		if (enter)
			client->sendMessage(chatInput);
		ImGui::Separator();
		if (ImGui::Button("Start"))
			modeChosen = MODE_MULTIPLAYER_HOST;
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
			showHostWnd = false;
		ImGui::End();
	}
	else {
		netDeinit();
	}

	if (showJoinWnd) {
		ImGui::Begin("Join", &showJoinWnd);
		ImGui::InputText("Address (IP)", joinAddress, sizeof(joinAddress));
		int port = (int)joinPort;
		ImGui::InputInt("Port", &port);
		joinPort = (uint16_t)port;
		if (ImGui::Button("Join")) {
			modeChosen = MODE_MULTIPLAYER_JOIN;
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
			showJoinWnd = false;
		ImGui::End();
	}

	netHandle();
	server->tick();
}

void QuickStartMenu::launchGame()
{
	if (modeChosen == MODE_UNDEFINED)
		return;

	if (modeChosen == MODE_MULTIPLAYER_JOIN) {
		netInit();
		enet_initialize();
		ENetHost* eclient;
		eclient = enet_host_create(nullptr, 1, 2, 0, 0);
		ENetAddress address;
		enet_address_set_host(&address, "localhost");
		address.port = 1234;
		ENetPeer* pserver = enet_host_connect(eclient, &address, 2, 0);

		ClientInterface cliui(client, gfx);
		client->attachInterface(&cliui);

		while (!g_windowQuit) {
			client->tick();

			ENetEvent event;
			while (enet_host_service(eclient, &event, 0) > 0) {
				switch (event.type) {
				case ENET_EVENT_TYPE_CONNECT: {
					printf("connection\n");
					NetEnetLink* link = new NetEnetLink(eclient, event.peer);
					client->serverLink = link;
					event.peer->data = link;
					break;
				}
				case ENET_EVENT_TYPE_DISCONNECT: {
					printf("disconnected\n");
					client->serverLink = nullptr;
					break;
				}
				case ENET_EVENT_TYPE_RECEIVE: {
					//printf("message: %zu\n", event.packet->dataLength);
					NetEnetLink* link = (NetEnetLink*)event.peer->data;
					assert(link == client->serverLink);
					link->packets.push(event.packet);
					break;
				}
				}
			}

			cliui.iter();
		}
		enet_host_destroy(eclient);
		netDeinit();
	}
	else {
		std::string savfile = std::string("Save_Games\\") + savegames.at(savselected);
		std::mutex srvMutex;
		std::thread srvThread([this, savfile, &srvMutex]() {
			srvMutex.lock();
			server->loadSaveGame(savfile.c_str());
			srvMutex.unlock();
			while (!g_windowQuit) {
				srvMutex.lock();
				server->tick();
				srvMutex.unlock();
				_sleep(1000 / 60);
			}
			});

		ClientInterface cliui(client, gfx);
		client->attachInterface(&cliui);
		ServerDebugger srvdbg(server);
		bool onServerUi = false;

		while (!g_windowQuit) {
			client->tick();
			netHandle();
			if (g_keyPressed[SDL_SCANCODE_F1]) {
				onServerUi = !onServerUi;
				if (onServerUi)
					srvMutex.lock();
				else
					srvMutex.unlock();
			}
			if (onServerUi) {
				server->tick();
				ImGuiImpl_NewFrame();
				srvdbg.draw();
				gfx->BeginDrawing();
				gfx->InitImGuiDrawing();
				ImGuiImpl_Render(gfx);
				gfx->EndDrawing();
				HandleWindow();
			}
			else
				cliui.iter();
		}
		if (onServerUi)
			srvMutex.unlock();

		srvThread.join();
	}
}

void QuickStartMenu::netInit()
{
	if (enetOn) return;
	enetOn = true;
	enet_initialize();
	ENetAddress address;
	address.host = ENET_HOST_ANY;
	address.port = 1234;
	eserver = enet_host_create(&address, 16, 2, 0, 0);
}

void QuickStartMenu::netHandle()
{
	if (!enetOn) return;
	ENetEvent event;
	while (enet_host_service(eserver, &event, 0) > 0) {
		switch (event.type) {
		case ENET_EVENT_TYPE_CONNECT: {
			printf("connection\n");
			NetEnetLink* link = new NetEnetLink(eserver, event.peer);
			event.peer->data = link;
			guests.push_back(link);
			if (server)
				server->addClient(link);
			break;
		}
		case ENET_EVENT_TYPE_DISCONNECT: {
			printf("disconnected\n");
			guests.erase(std::find(guests.begin(), guests.end(), (NetEnetLink*)event.peer->data));
			if (server)
				server->removeClient((NetEnetLink*)event.peer->data);
			break;
		}
		case ENET_EVENT_TYPE_RECEIVE: {
			printf("message: %zu\n", event.packet->dataLength);
			NetEnetLink* link = (NetEnetLink*)event.peer->data;
			link->packets.push(event.packet);
			break;
		}
		}
	}
}

void QuickStartMenu::netDeinit()
{
	if (!enetOn) return;
	enetOn = false;
	enet_host_destroy(eserver);
	enet_deinitialize();
}
