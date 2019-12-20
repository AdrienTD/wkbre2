#pragma once

#include "ClientDebugger.h"
#include "../scene.h"
#include <ctime>

struct Client;
struct TerrainRenderer;
struct IRenderer;
struct SceneRenderer;
struct ClientGameObject;

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
		sceneRenderer(nullptr), debugger(client), scene(nullptr) {}

private:
	int camrotoffx, camrotoffy;
	float camrotorix, camrotoriy;
	bool camroton = false;
	int numObjectsDrawn = 0;
	int framesPerSecond = 0, nextFps = 0;
	time_t lastfpscheck = 0;
	Vector3 rayDirection;
	ClientGameObject *nextSelectedObject = nullptr;
};