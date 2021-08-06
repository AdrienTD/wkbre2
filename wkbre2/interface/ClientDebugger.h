#pragma once

#include "../GameObjectRef.h"
#include "GameSetDebugger.h"

struct Client;
struct ClientGameObject;
struct ClientInterface;

struct ClientDebugger {
public:
	Client *client;
	ClientInterface *cliUi = nullptr;
	ClientDebugger(Client *client) : client(client) {}
	ClientDebugger(Client *client, ClientInterface *cliUi) : client(client), cliUi(cliUi) {}
	void draw();
	void selectObject(ClientGameObject *obj) { selectedObject = obj; }
private:
	CliGORef selectedObject;
	GameSetDebugger gsDebugger;
};