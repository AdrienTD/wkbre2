#pragma once

#include "ClientDebugger.h"
#include "../scene.h"
#include <ctime>
#include "../GameObjectRef.h"
#include "../Language.h"
#include <deque>
#include <unordered_set>

struct Client;
struct TerrainRenderer;
struct IRenderer;
struct SceneRenderer;
struct ClientGameObject;
struct GameObjBlueprint;

struct ClientInterface {
	Client *client;
	IRenderer *gfx;
	TerrainRenderer *terrainRenderer;
	SceneRenderer *sceneRenderer;
	ClientDebugger debugger;
	Scene *scene;

	void drawObject(ClientGameObject * obj);

	void iter();
	void render();

	void updateTerrain();

	ClientInterface(Client *client, IRenderer *gfx) : client(client), gfx(gfx), terrainRenderer(nullptr),
		sceneRenderer(nullptr), debugger(client, this), scene(nullptr) {}

	GameObjBlueprint *stampdownBlueprint = nullptr;
	CliGORef stampdownPlayer;
	bool sendStampdownEvent = false;
private:
	int camrotoffx, camrotoffy;
	float camrotorix, camrotoriy;
	bool camroton = false;
	int numObjectsDrawn = 0;
	int framesPerSecond = 0, nextFps = 0;
	time_t lastfpscheck = 0;
	Vector3 rayDirection;
	CliGORef nextSelectedObject;
	float nextSelObjDistance = 0.0f;
	Language lang;
	std::deque<SceneEntity> attachSceneEntities;
	std::unordered_set<CliGORef> selection;
	bool selBoxOn = false;
	int selBoxStartX, selBoxStartY, selBoxEndX, selBoxEndY;
	std::vector<CliGORef> nextSelFromBox;
};