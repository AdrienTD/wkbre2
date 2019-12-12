#include "ClientInterface.h"
#include "../gfx/TerrainRenderer.h"
#include "../gfx/DefaultTerrainRenderer.h"
#include "../client.h"
#include "../gfx/renderer.h"
#include "ClientDebugger.h"
#include "../imguiimpl.h"
#include "../window.h"
#include <SDL_scancode.h>
#include "../tags.h"
#include "../gameset/gameset.h"
#include "../gfx/SceneRenderer.h"
#include "../gfx/DefaultSceneRenderer.h"
#include "../imgui/imgui.h"
#include <ctime>
#include "../terrain.h"

namespace {
	Vector3 getRay(Camera &cam) {
		float ay = (1 - g_mouseX * 2.0f / g_windowWidth) * 0.9; // *g_windowWidth) / g_windowHeight;
		float ax = (1 - g_mouseY * 2.0f / g_windowHeight) * 0.9;
		Vector3 direction = Vector3(
			-sin(cam.orientation.y + ay)*cos(cam.orientation.x + ax),
			sin(cam.orientation.x + ax),
			cos(cam.orientation.y + ay)*cos(cam.orientation.x + ax)
		);
		return direction;
	}
	/*
	void CalcStampdownPos(Camera &cam, Vector3 &raydir, Terrain &terrain)
	{
		float lv = 4;

		Vector3 ptt = cam.position, vta;
		NormalizeVector3(&vta, &raydir);
		vta *= lv;

		float h;
		const float farzvalue = 250.0f;
		int nlp = farzvalue * 1.5f / lv;
		int m = (ptt.y < terrain.getVertex(ptt.x, ptt.z)) ? 0 : 1;

		for (int i = 0; i < nlp; i++) {
			ptt += vta;
			if (!InMap(ptt)) continue;
			h = GetHeight(ptt.x, ptt.z);

			if (ptt.y == h)
				break;

			if (m ^ ((ptt.y > h) ? 1 : 0)) {
				vta *= -0.5f; m = 1 - m;
			}
		}

		if (InMap(ptt)) {
			mapstdownpos = ptt; mapstdownpos.y = h; mapstdownvalid = 1;
		}
		else	mapstdownvalid = 0;
		if (mapstdownvalid && InLevel(mapstdownpos)) {
			stdownpos = mapstdownpos; stdownvalid = 1;
		}
		else	stdownvalid = 0;
	}
	*/
}

void ClientInterface::drawObject(ClientGameObject *obj) {
	using namespace Tags;
	//if ((obj->blueprint->bpClass == GAMEOBJCLASS_BUILDING) || (obj->blueprint->bpClass == GAMEOBJCLASS_CHARACTER)) {
	if(true) {
		Vector3 ttpp;
		TransformCoord3(&ttpp, &obj->position, &client->camera.sceneMatrix);
		if (!(ttpp.x < -1 || ttpp.x > 1 || ttpp.y < -1 || ttpp.y > 1 || ttpp.z < -1 || ttpp.z > 1)) {
			auto anim = obj->blueprint->subtypes[obj->subtype].appearances[obj->appearance].animations[0];
			if (!anim.empty())
				obj->sceneEntity.model = anim[0]; //anim[rand() % anim.size()];
			else
				obj->sceneEntity.model = nullptr;
			if (obj->sceneEntity.model) {
				//CreateTranslationMatrix(&obj->sceneEntity.transform, obj->position.x, obj->position.y, obj->position.z);
				CreateWorldMatrix(&obj->sceneEntity.transform, Vector3(1,1,1), -obj->orientation, obj->position);
				obj->sceneEntity.color = obj->getPlayer()->color;
				scene->add(&obj->sceneEntity);
				numObjectsDrawn++;
			}
		}
	}
	for (auto &typechildren : obj->children) {
		GameObjBlueprint* blueprint = client->gameSet->getBlueprint(typechildren.first);
		for (ClientGameObject* child : typechildren.second) {
			drawObject(child);
		}
	}
}

void ClientInterface::iter()
{
	//----- Input -----//

	Vector3 forward = client->camera.direction.normal2xz();
	Vector3 strafe = forward.cross(Vector3(0, 1, 0));
	forward *= 2.f; strafe *= 2.f;

	if (g_keyDown[SDL_SCANCODE_UP])
		client->camera.position += forward;
	if (g_keyDown[SDL_SCANCODE_DOWN])
		client->camera.position -= forward;
	if (g_keyDown[SDL_SCANCODE_RIGHT])
		client->camera.position -= strafe;
	if (g_keyDown[SDL_SCANCODE_LEFT])
		client->camera.position += strafe;

	if (g_keyDown[SDL_SCANCODE_0]) {
		if (!camroton) {
			camrotoffx = g_mouseX;
			camrotoffy = g_mouseY;
			camrotorix = client->camera.orientation.x;
			camrotoriy = client->camera.orientation.y;
			camroton = true;
		}
		client->camera.orientation.x = camrotorix - (g_mouseY - camrotoffy) * 0.01f;
		client->camera.orientation.y = camrotoriy - (g_mouseX - camrotoffx) * 0.01f;
	}
	else
		camroton = false;

	if (g_mouseWheel)
		client->camera.position.y += g_mouseWheel;

	//----- ImGui -----//

	ImGuiImpl_NewFrame();
	debugger.draw();
	ImGui::Begin("Client Interface Debug");
	ImGui::Text("ODPF: %i", numObjectsDrawn);
	ImGui::Text("FPS: %i", framesPerSecond);
	ImGui::End();

	//----- Rendering -----//

	gfx->BeginDrawing();

	client->camera.updateMatrix();
	gfx->SetTransformMatrix(&client->camera.sceneMatrix);

	// Get ray


	if (client->gameSet) {
		if(!scene)
			scene = new Scene(gfx, &client->gameSet->modelCache);

		numObjectsDrawn = 0;
		if (client->level)
			drawObject(client->level);
		// test
		Vector3 peapos = client->camera.position + getRay(client->camera).normal() * 16;
		SceneEntity test;
		test.model = client->gameSet->modelCache.getModel("Warrior Kings Game Set\\Characters\\Peasant\\Male\\Peasant1.MESH3");
		CreateTranslationMatrix(&test.transform, peapos.x, peapos.y, peapos.z);
		test.color = 0;
		scene->add(&test);
		//
		if (!sceneRenderer)
			sceneRenderer = new DefaultSceneRenderer(gfx, scene);
		sceneRenderer->render();
		scene->clear();
	}

	if (terrainRenderer)
		terrainRenderer->render();

	gfx->InitImGuiDrawing();
	ImGuiImpl_Render(gfx);

	gfx->EndDrawing();
	HandleWindow();

	nextFps++;
	time_t tim = time(NULL);
	if (tim >= lastfpscheck + 1) {
		framesPerSecond = nextFps;
		nextFps = 0;
		lastfpscheck = tim;
	}
}

void ClientInterface::render()
{
	if(this->terrainRenderer)
		this->terrainRenderer->render();
}

void ClientInterface::updateTerrain() {
	this->terrainRenderer = new DefaultTerrainRenderer(gfx, client->terrain, &client->camera);
}