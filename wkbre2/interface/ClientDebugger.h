#pragma once

struct Client;
struct ClientGameObject;

struct ClientDebugger {
public:
	Client *client;
	ClientDebugger(Client *client) : client(client) {}
	void draw();
private:
	ClientGameObject *selectedObject = nullptr;
};