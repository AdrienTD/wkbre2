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