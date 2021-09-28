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
struct CameraPath;

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

	Camera camera;
	Vector3 storedCameraPosition, storedCameraOrientation;
	int cameraMode = 0;
	const CameraPath* cameraCurrentPath = nullptr;
	int cameraPathIndex = -1;
	std::vector<std::pair<Vector3, Vector3>> cameraPathPos;
	std::vector<float> cameraPathDur;
	uint32_t cameraStartTime;

	ClientInterface *cliInterface;

	bool localServer;
	CliGORef clientPlayer;

	std::map<int, int> gtwStates;
	bool isMusicPlaying = false;

	struct SpecialEffect {
		SceneEntity entity;
		CliGORef attachedObj;
		float startTime;
	};
	std::list<SpecialEffect> specialEffects;
	std::multimap<std::pair<CliGORef, int>, SceneEntity> loopingSpecialEffects;

	//void loadSaveGame(const char *filename);
	//ClientGameObject *createObject(GameObjBlueprint *blueprint, uint32_t id = 0);

	Client(bool localServer = false) : gameSet(nullptr), level(nullptr),
		serverLink(nullptr), terrain(nullptr), cliInterface(nullptr), localServer(localServer) {instance = this;}

	ClientGameObject* findObject(uint32_t id) { auto it = idmap.find(id); if (it != idmap.end()) return it->second; else return nullptr; }

	void loadFromServerObject(Server &server);

	void tick();

	void sendMessage(const std::string &msg);
	void sendCommand(ClientGameObject *obj, Command *cmd, int assignmentMode, ClientGameObject *target = nullptr, const Vector3 & destination = Vector3());
	void sendPauseRequest(uint8_t pauseState);
	void sendStampdown(GameObjBlueprint *blueprint, ClientGameObject *player, const Vector3 &position, bool sendEvent = false);
	void sendStartLevelRequest();
	void sendGameTextWindowButtonClicked(int gtwIndex, int buttonIndex);
	void sendCameraPathEnded(int camPathIndex);
	void sendGameSpeedChange(float nextSpeed);
	void sendTerminateObject(ClientGameObject* obj);
	void sendBuildLastStampdownedObject(ClientGameObject* obj, int assignmentMode);

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
