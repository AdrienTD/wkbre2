// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "QuickSkirmishMenu.h"

struct IRenderer;
struct NetEnetLink;
struct _ENetHost;
struct Server;
struct Client;
struct NetLocalBuffer;
struct NetLocalLink;

struct QuickStartMenu {
	QuickStartMenu(IRenderer* gfx);
	~QuickStartMenu();
	void draw();
	void launchGame();
private:
	std::vector<std::string> savegames;
	size_t savselected = 0;
	IRenderer* gfx;

	enum Mode {
		MODE_UNDEFINED = 0,
		MODE_SINGLEPLAYER = 1,
		MODE_MULTIPLAYER_HOST,
		MODE_MULTIPLAYER_JOIN,
	};
	Mode modeChosen = MODE_UNDEFINED;

	_ENetHost* eserver = nullptr;
	std::vector<NetEnetLink*> guests;
	Server* server = nullptr;
	Client* client = nullptr;
	NetLocalBuffer* queue1 = nullptr, * queue2 = nullptr;
	NetLocalLink* srv2cli = nullptr, * cli2srv = nullptr;
	bool enetOn = false;

	bool showSkirmishWnd = false, showHostWnd = false, showJoinWnd = false;
	char joinAddress[64] = "localhost"; uint16_t joinPort = 1234;

	QuickSkirmishMenu skirmishMenu;

	void netInit();
	void netHandle();
	void netDeinit();
};