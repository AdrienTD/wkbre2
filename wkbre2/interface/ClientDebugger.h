#pragma once

#include "../GameObjectRef.h"

struct Client;
struct ClientGameObject;

struct ClientDebugger {
public:
	Client *client;
	ClientDebugger(Client *client) : client(client) {}
	void draw();
	void selectObject(ClientGameObject *obj) { selectedObject = obj; }
private:
	CliGORef selectedObject;
};