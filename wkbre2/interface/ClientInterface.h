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
struct ParticleContainer;
struct PSCache;
struct ParticleRenderer;

struct ClientInterface {
	Client *client;
	IRenderer *gfx;
	TerrainRenderer *terrainRenderer;
	SceneRenderer *sceneRenderer;
	ClientDebugger debugger;
	Scene *scene;
	ParticleContainer* particlesContainer;
	PSCache* psCache;
	ParticleRenderer* particleRenderer;

	void iter();
	void render();

	void updateTerrain();

	ClientInterface(Client *client, IRenderer *gfx) : client(client), gfx(gfx), terrainRenderer(nullptr),
		sceneRenderer(nullptr), debugger(client, this), scene(nullptr), particlesContainer(nullptr), psCache(nullptr),
		particleRenderer(nullptr), uiTexCache(gfx) {}

	GameObjBlueprint *stampdownBlueprint = nullptr;
	CliGORef stampdownPlayer;
	bool sendStampdownEvent = false;
	std::unordered_set<CliGORef> selection;
	TextureCache uiTexCache;
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
	bool selBoxOn = false;
	int selBoxStartX, selBoxStartY, selBoxEndX, selBoxEndY;
	std::vector<CliGORef> nextSelFromBox;

	void drawObject(ClientGameObject* obj);
	void drawAttachmentPoints(SceneEntity* sceneEntity, uint32_t objid = 0);
};