// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

struct Server;
struct ServerGameObject;

struct ServerDebugger {
public:
	Server *server;
	ServerDebugger(Server *server) : server(server) {}
	void draw();

private:
	ServerGameObject *selectedObject = nullptr;
};