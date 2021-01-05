#pragma once

#include <cstdint>
#include <map>
#include <vector>
#include "common.h"
#include "util/vecmat.h"
#include "Camera.h"
#include "scene.h"
#include <cstdarg>
#include "TimeManager.h"
#include "GameObjectRef.h"

struct GameSet;
struct GSFileParser;
struct GameObjBlueprint;
struct Server;
struct NetLink;
struct Command;
struct Terrain;
struct ClientInterface;
struct Client;

struct ClientGameObject : CommonGameObject<ClientGameObject> {
	using Program = Client;
	ClientGameObject(uint32_t id, GameObjBlueprint *blueprint) : CommonGameObject<ClientGameObject>(id, blueprint) {}
	SceneEntity sceneEntity;
};

struct Client : CommonGameState<Client, ClientGameObject>
{
	using GameObject = ClientGameObject;
	static Client *instance;

	GameSet *gameSet;

	TimeManager timeManager;

	ClientGameObject *level;
	std::map<uint32_t, ClientGameObject*> idmap;
	Terrain *terrain;

	std::vector<std::string> chatMessages;

	NetLink *serverLink;

	//Vector3 cameraPosition, cameraOrientation;
	//Matrix perspectiveMatrix, lookatMatrix;
	Camera camera;

	ClientInterface *cliInterface;

	bool localServer;

	//void loadSaveGame(const char *filename);
	//ClientGameObject *createObject(GameObjBlueprint *blueprint, uint32_t id = 0);

	Client(bool localServer = false) : gameSet(nullptr), level(nullptr),
		serverLink(nullptr), terrain(nullptr), cliInterface(nullptr), localServer(localServer) {instance = this;}

	ClientGameObject *findObject(uint32_t id) { return idmap[id]; }

	void loadFromServerObject(Server &server);

	void tick();

	void sendMessage(const std::string &msg);
	void sendCommand(ClientGameObject *obj, Command *cmd, int assignmentMode, ClientGameObject *target = nullptr, const Vector3 & destination = Vector3());
	void sendPauseRequest(uint8_t pauseState);
	void sendStampdown(GameObjBlueprint *blueprint, ClientGameObject *player, const Vector3 &position, bool sendEvent = false);
	void sendStartLevelRequest();

	void attachInterface(ClientInterface *cliIface) { cliInterface = cliIface; }

private:
	//ClientGameObject * loadObject(GSFileParser & gsf, const std::string & clsname);
	ClientGameObject *createObject(GameObjBlueprint *blueprint, uint32_t id = 0);
	void info(const char *fmt, ...) {
		va_list args;
		va_start(args, fmt);
		//vprintf(fmt, args);
		va_end(args);
	}
};
