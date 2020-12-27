#include "ClientInterface.h"
#include "../gfx/TerrainRenderer.h"
#include "../gfx/DefaultTerrainRenderer.h"
#include "../client.h"
#include "../gfx/renderer.h"
#include "ClientDebugger.h"
#include "../imguiimpl.h"
#include "../window.h"
#include <SDL_scancode.h>
#include <SDL_mouse.h>
#include "../tags.h"
#include "../gameset/gameset.h"
#include "../gfx/SceneRenderer.h"
#include "../gfx/DefaultSceneRenderer.h"
#include "../imgui/imgui.h"
#include <ctime>
#include "../terrain.h"
#include <SDL_timer.h>
#include "../gameset/ScriptContext.h"

namespace {
	Vector3 getRay(const Camera &cam) {
		//float ay = (1 - g_mouseX * 2.0f / g_windowWidth) * 0.45 * g_windowWidth / g_windowHeight;
		//float ax = (1 - g_mouseY * 2.0f / g_windowHeight) * 0.45;
		//Vector3 direction = Vector3(
		//	-sin(cam.orientation.y + ay)*cos(cam.orientation.x + ax),
		//	sin(cam.orientation.x + ax),
		//	cos(cam.orientation.y + ay)*cos(cam.orientation.x + ax)
		//);
		//return direction;
		const float zNear = 0.1f;
		Vector3 xvec = cam.direction.normal().cross(Vector3(0, 1, 0)).normal();
		Vector3 yvec = cam.direction.normal().cross(xvec).normal();
		//Vector3 nearori = cam.position + cam.direction.normal() * zNear;
		float ys = tan(0.45f) * zNear;
		yvec *= ys;
		xvec *= ys * g_windowWidth / g_windowHeight;
		yvec *= (1 - g_mouseY * 2.0f / g_windowHeight);
		xvec *= (1 - g_mouseX * 2.0f / g_windowWidth);
		//Vector3 posonnear = nearori + xvec - yvec;
		//return posonnear - cam.position;
		return cam.direction.normal() * zNear + xvec - yvec;
	}

	int InMap(const Vector3 &v, const Terrain &map)
	{
		//printf("%f < %f < %f\n", (float)(-(int)map.edge * 5), v.x, (float)((map.width - map.edge) * 5));
		if ((v.x >= -(map.edge * 5.0f)) && (v.x < ((map.width - map.edge) * 5.0f)))
			if ((v.z >= -(map.edge * 5.0f)) && (v.z < ((map.height - map.edge) * 5.0f)))
				return 1;
		return 0;
	}
	
	Vector3 CalcStampdownPos(const Vector3 &raystart, const Vector3 &raydir, const Terrain &terrain)
	{
		float lv = 4;

		Vector3 ptt = raystart, vta = raydir.normal() * lv;

		float h;
		const float farzvalue = 250.0f;
		int nlp = farzvalue * 1.5f / lv;
		int m = (ptt.y < terrain.getHeight(ptt.x, ptt.z)) ? 0 : 1;

		for (int i = 0; i < nlp; i++) {
			ptt += vta;
			if (!InMap(ptt, terrain)) continue;
			h = terrain.getHeight(ptt.x, ptt.z);

			if (ptt.y == h)
				break;

			if (m ^ ((ptt.y > h) ? 1 : 0)) {
				vta *= -0.5f; m = 1 - m;
			}
		}

		if (InMap(ptt, terrain)) {
			Vector3 mapstdownpos = ptt; mapstdownpos.y = h; //mapstdownvalid = 1;
			return mapstdownpos;
		}
		else
			return Vector3(0, 0, 0);
		//if (mapstdownvalid && InLevel(mapstdownpos)) {
		//	stdownpos = mapstdownpos; stdownvalid = 1;
		//}
		//else	stdownvalid = 0;
	}

	bool RayIntersectsSphere(const Vector3 &raystart, const Vector3 &raydir, const Vector3 &center, float radius)
	{
		Vector3 rdnorm = raydir.normal();
		Vector3 smc = raystart - center;
		float ddt = rdnorm.dot(smc);
		float delta = ddt * ddt - rdnorm.sqlen3()*(smc.sqlen3() - radius * radius);
		if (delta < 0.0f)
			return false;
		float k1 = -ddt + sqrtf(delta),
			k2 = -ddt - sqrtf(delta);
		if (k1 < 0.0f && k2 < 0.0f)
			return false;
		return true;
	}

	Matrix CreateWorldMatrix(const Vector3 &is, const Vector3 &ir, const Vector3 &it)
	{
		//Matrix mscale, mrot, mtrans;
		//CreateScaleMatrix(&mscale, is.x, is.y, is.z);
		//CreateRotationYXZMatrix(&mrot, ir.y, ir.x, ir.z);
		//CreateTranslationMatrix(&mtrans, it.x, it.y, it.z);
		//*mWorld = mscale * mrot*mtrans;
		return Matrix::getScaleMatrix(is) * Matrix::getRotationYMatrix(ir.y) * Matrix::getRotationXMatrix(ir.x)
			* Matrix::getRotationZMatrix(ir.z) * Matrix::getTranslationMatrix(it);
	}
}

void ClientInterface::drawObject(ClientGameObject *obj) {
	using namespace Tags;
	//if ((obj->blueprint->bpClass == GAMEOBJCLASS_BUILDING) || (obj->blueprint->bpClass == GAMEOBJCLASS_CHARACTER)) {
	if(true) {
		Vector3 side = client->camera.direction.cross(Vector3(0, 1, 0));
		float dist = client->camera.direction.dot(obj->position - client->camera.position);
		if (dist < client->camera.nearDist || dist > client->camera.farDist)
			goto drawsub;
		Vector3 ttpp = obj->position.transformScreenCoords(client->camera.sceneMatrix);
		if (!(ttpp.x < -1 || ttpp.x > 1 || ttpp.y < -1 || ttpp.y > 1 || ttpp.z < -1 || ttpp.z > 1)) {
			const auto &ap = obj->blueprint->subtypes[obj->subtype].appearances[obj->appearance];
			auto it = ap.animations.find(obj->animationIndex);
			if (it == ap.animations.end())
				it = ap.animations.find(0);
			if (it == ap.animations.end())
				goto drawsub;
			const auto &anim = it->second;
			if (!anim.empty())
				obj->sceneEntity.model = anim[obj->animationVariant]; //anim[rand() % anim.size()];
			else
				obj->sceneEntity.model = nullptr;
			if (obj->sceneEntity.model) {
				obj->sceneEntity.transform = CreateWorldMatrix(Vector3(1,1,1), -obj->orientation, obj->position);
				obj->sceneEntity.color = obj->getPlayer()->color;
				if (RayIntersectsSphere(client->camera.position, rayDirection, obj->position + obj->sceneEntity.model->getSphereCenter(), obj->sceneEntity.model->getSphereRadius())) {
					obj->sceneEntity.color = 0;
					nextSelectedObject = obj;
				}
				obj->sceneEntity.animTime = (client->timeManager.currentTime - obj->animStartTime) * 1000.0f;
				scene->add(&obj->sceneEntity);
				numObjectsDrawn++;
			}
		}
	}
drawsub:
	for (auto &typechildren : obj->children) {
		GameObjBlueprint* blueprint = client->gameSet->getBlueprint(typechildren.first);
		for (ClientGameObject* child : typechildren.second) {
			drawObject(child);
		}
	}
}

void ClientInterface::iter()
{
	static Vector3 peapos(0, 0, 0);

	static bool firstTime = true;
	static Cursor *arrowCursor;
	static Cursor *currentCursor = nullptr;
	if (firstTime) {
		firstTime = false;
		//arrowCursor = WndCreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
		arrowCursor = WndCreateCursor("Interface\\C_DEFAULT.TGA");
		WndSetCursor(arrowCursor);
		currentCursor = arrowCursor;
	}

	static CliGORef selected;

	//----- Command cursors -----//

	Cursor *nextCursor = nullptr;
	Command *rightClickCommand = nullptr;
	if (ClientGameObject *sel = selected.get()) {
		if (true || nextSelectedObject) {
			auto _ = CliScriptContext::target.change(nextSelectedObject);
			for (Command *cmd : sel->blueprint->offeredCommands) {
				if (cmd->cursor)
					if (std::all_of(cmd->cursorConditions.begin(), cmd->cursorConditions.end(), [&](int eq) {return client->gameSet->equations[eq]->eval(sel) > 0.0f; }))
					{
						nextCursor = cmd->cursor;
						rightClickCommand = cmd;
						break;
					}
			}
		}
	}
	if (!nextCursor)
		nextCursor = arrowCursor;
	if (nextCursor) {
		if (nextCursor != currentCursor) {
			currentCursor = nextCursor;
			WndSetCursor(currentCursor);
		}
	}

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

	if (g_keyDown[SDL_SCANCODE_KP_0]) {
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

	if (g_mouseDown[SDL_BUTTON_LEFT]) {
		debugger.selectObject(nextSelectedObject);
		selected = nextSelectedObject;
	}

	if (g_mouseDown[SDL_BUTTON_RIGHT] && nextSelectedObject) {
		if (ClientGameObject *sel = selected.get()) {
			int assignmentMode = g_modCtrl ? Tags::ORDERASSIGNMODE_DO_FIRST : (g_modShift ? Tags::ORDERASSIGNMODE_DO_LAST : Tags::ORDERASSIGNMODE_FORGET_EVERYTHING_ELSE);
			client->sendCommand(sel, rightClickCommand, nextSelectedObject, assignmentMode);
		}
	}

	if (g_keyPressed[SDL_SCANCODE_P]) {
		static bool pauseState = false;
		pauseState = !pauseState;
		client->sendPauseRequest(pauseState);
	}

	//----- ImGui -----//

	ImGuiImpl_NewFrame();
	debugger.draw();
	ImGui::Begin("Client Interface Debug");
	ImGui::Text("ODPF: %i", numObjectsDrawn);
	ImGui::Text("FPS: %i", framesPerSecond);
	ImGui::DragFloat3("peapos", &peapos.x);
	ImGui::End();

	//----- Rendering -----//

	gfx->BeginDrawing();

	client->camera.updateMatrix();
	gfx->SetTransformMatrix(&client->camera.sceneMatrix);

	// Get ray
	rayDirection = getRay(client->camera).normal();
	nextSelectedObject = nullptr;

	if (client->gameSet) {
		if(!scene)
			scene = new Scene(gfx, &client->gameSet->modelCache);

		numObjectsDrawn = 0;
		if (client->level)
			drawObject(client->level);
		// test
		//Vector3 peapos = client->camera.position + getRay(client->camera).normal() * 16;
		peapos = Vector3(0, 0, 0);
		if(client->terrain)
			peapos = CalcStampdownPos(client->camera.position, rayDirection, *client->terrain);
		SceneEntity test;
		test.model = client->gameSet->modelCache.getModel("Warrior Kings Game Set\\Characters\\Peasant\\Male\\Peasant1.MESH3");
		test.transform = Matrix::getTranslationMatrix(peapos);
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