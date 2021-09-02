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
#include <stdexcept>

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
		//float delta = ddt * ddt - rdnorm.sqlen3()*(smc.sqlen3() - radius * radius);
		float delta = ddt * ddt - (smc.sqlen3() - radius * radius);
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
		return Matrix::getScaleMatrix(is) * Matrix::getRotationYMatrix(ir.y) * Matrix::getRotationXMatrix(ir.x)
			* Matrix::getRotationZMatrix(ir.z) * Matrix::getTranslationMatrix(it);
	}

	std::pair<bool, Vector3> getRayTriangleIntersection(const Vector3& rayStart, const Vector3& _rayDir, const Vector3& p1, const Vector3& p2, const Vector3& p3) {
		Vector3 rayDir = _rayDir.normal();
		Vector3 v2 = p2 - p1, v3 = p3 - p1;
		Vector3 trinorm = v2.cross(v3).normal(); // order?
		if (trinorm == Vector3(0, 0, 0))
			return std::make_pair(false, trinorm);
		float rayDir_dot_trinorm = rayDir.dot(trinorm);
		if (rayDir_dot_trinorm < 0.0f)
			return std::make_pair(false, Vector3(0, 0, 0));
		float p = p1.dot(trinorm);
		float alpha = (p - rayStart.dot(trinorm)) / rayDir_dot_trinorm;
		if (alpha < 0.0f)
			return std::make_pair(false, Vector3(0, 0, 0));
		Vector3 sex = rayStart + rayDir * alpha;

		Vector3 c = sex - p1;
		float d = v2.sqlen3() * v3.sqlen3() - v2.dot(v3) * v2.dot(v3);
		//assert(d != 0.0f);
		float a = (c.dot(v2) * v3.sqlen3() - c.dot(v3) * v2.dot(v3)) / d;
		float b = (c.dot(v3) * v2.sqlen3() - c.dot(v2) * v3.dot(v2)) / d;
		if (a >= 0.0f && b >= 0.0f && (a + b) <= 1.0f)
			return std::make_pair(true, sex);
		else
			return std::make_pair(false, Vector3(0, 0, 0));
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
			Model* model = nullptr;
			const auto &ap = obj->blueprint->subtypes[obj->subtype].appearances[obj->appearance];
			auto it = ap.animations.find(obj->animationIndex);
			if (it == ap.animations.end())
				it = ap.animations.find(0);
			if (it != ap.animations.end()) {
				const auto& anim = it->second;
				if (!anim.empty())
					model = anim[obj->animationVariant];
			}
			if (!model)
				model = obj->blueprint->representAs;
			if (model) {
				obj->sceneEntity.model = model;
				obj->sceneEntity.transform = obj->getWorldMatrix();
				obj->sceneEntity.color = obj->getPlayer()->color;
				obj->sceneEntity.animTime = (uint32_t)((client->timeManager.currentTime - obj->animStartTime) * 1000.0f);
				obj->sceneEntity.flags = obj->animClamped ? SceneEntity::SEFLAG_ANIM_CLAMP_END : 0;
				scene->add(&obj->sceneEntity);
				numObjectsDrawn++;
				// Attachment points
				size_t numAPs = model->getNumAPs();
				for (size_t i = 0; i < numAPs; i++) {
					auto ap = model->getAPInfo(i);
					if (!ap.path.empty()) {
						auto state = model->getAPState(i, obj->sceneEntity.animTime);
						attachSceneEntities.emplace_back();
						auto& apse = attachSceneEntities.back();
						apse.model = model->cache->getModel("Warrior Kings Game Set\\" + ap.path);
						apse.transform = Matrix::getTranslationMatrix(state.position) * obj->sceneEntity.transform;
						apse.color = obj->sceneEntity.color;
						apse.animTime = (uint32_t)(client->timeManager.currentTime * 1000.0f);
						scene->add(&apse);
					}
				}
				// Ray collision check
				if (RayIntersectsSphere(client->camera.position, rayDirection, obj->position + obj->sceneEntity.model->getSphereCenter() * obj->scale, obj->sceneEntity.model->getSphereRadius() * std::max({ obj->scale.x, obj->scale.y, obj->scale.z }))) {
					const float *verts = model->interpolate(obj->sceneEntity.animTime);
					Mesh& mesh = model->getStaticModel()->getMesh();
					PolygonList& polylist = mesh.polyLists[0];
					for (size_t g = 0; g < polylist.groups.size(); g++) {
						PolyListGroup& group = polylist.groups[g];
						for (auto& tuple : group.tupleIndex) {
							Vector3 vec[3];
							for (int j = 0; j < 3; j++) {
								uint16_t i = tuple[j];
								IndexTuple& tind = mesh.groupIndices[g][i];
								const float* fl = &verts[tind.vertex * 3];
								Vector3 prever(fl[0], fl[1], fl[2]);
								vec[j] = prever.transform(obj->sceneEntity.transform);
							}
							auto col = getRayTriangleIntersection(client->camera.position, rayDirection, vec[0], vec[2], vec[1]);
							if (col.first) {
								float dist = (client->camera.position - col.second).sqlen3();
								if (dist < nextSelObjDistance) {
									obj->sceneEntity.flags |= SceneEntity::SEFLAG_NOLIGHTNING;
									nextSelectedObject = obj;
									nextSelObjDistance = dist;
								}
							}
						}
					}
				}
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
		lang.load("Languages\\Language.txt");
	}

	static CliGORef selected;

	//----- Command cursors -----//

	Cursor *nextCursor = nullptr;
	Command *rightClickCommand = nullptr;
	if (ClientGameObject *sel = selected.get()) {
		if (true || nextSelectedObject) {
			CliScriptContext ctx(client, sel);
			auto _ = ctx.target.change(nextSelectedObject);
			for (Command *cmd : sel->blueprint->offeredCommands) {
				Cursor *cmdCursor = nullptr;
				for (auto &avcond : cmd->cursorAvailable) {
					if (avcond.first->test->eval(&ctx)) {
						cmdCursor = avcond.second;
						break;
					}
				}
				if (!cmdCursor)
					cmdCursor = cmd->cursor;
				if (cmdCursor) {
					if (std::all_of(cmd->cursorConditions.begin(), cmd->cursorConditions.end(), [&](int eq) {return client->gameSet->equations[eq]->eval(&ctx) > 0.0f; })) {
						nextCursor = cmdCursor;
						rightClickCommand = cmd;
						break;
					}
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

	if (g_mousePressed[SDL_BUTTON_LEFT]) {
		debugger.selectObject(nextSelectedObject);
		selected = nextSelectedObject;
		stampdownBlueprint = nullptr;
	}

	if (g_mousePressed[SDL_BUTTON_RIGHT]) {
		if (stampdownBlueprint) {
			client->sendStampdown(stampdownBlueprint, stampdownPlayer, peapos, sendStampdownEvent);
		}
		else if (rightClickCommand) {
			if (ClientGameObject *sel = selected.get()) {
				int assignmentMode = g_modCtrl ? Tags::ORDERASSIGNMODE_DO_FIRST : (g_modShift ? Tags::ORDERASSIGNMODE_DO_LAST : Tags::ORDERASSIGNMODE_FORGET_EVERYTHING_ELSE);
				client->sendCommand(sel, rightClickCommand, assignmentMode, nextSelectedObject, peapos);
			}
		}
	}

	if (g_keyPressed[SDL_SCANCODE_P]) {
		static bool pauseState = false;
		pauseState = !pauseState;
		client->sendPauseRequest(pauseState);
	}
	if (g_keyPressed[SDL_SCANCODE_KP_PLUS]) {
		client->sendGameSpeedChange(client->timeManager.timeSpeed * 2.0f);
	}
	if (g_keyPressed[SDL_SCANCODE_KP_MINUS]) {
		client->sendGameSpeedChange(client->timeManager.timeSpeed / 2.0f);
	}
	if (g_keyPressed[SDL_SCANCODE_DELETE] && selected)
		client->sendTerminateObject(selected);

	//----- ImGui -----//

	ImGuiImpl_NewFrame();
	debugger.draw();
	ImGui::Begin("Client Interface Debug");
	ImGui::Text("ODPF: %i", numObjectsDrawn);
	ImGui::Text("FPS: %i", framesPerSecond);
	ImGui::DragFloat3("peapos", &peapos.x);
	if (ImGui::Button("Start level"))
		client->sendStartLevelRequest();
	ImGui::End();

	for (auto& activeGtw : client->gtwStates) {
		if (activeGtw.second == -1) continue;
		auto& gtw = client->gameSet->gameTextWindows[activeGtw.first];
		auto& page = gtw.pages[activeGtw.second];
		std::string title = "[GTW] " + client->gameSet->gameTextWindows.names[activeGtw.first];
		ImGui::SetNextWindowPos(ImVec2((float)g_windowWidth/2, (float)g_windowHeight/2), 0, ImVec2(0.5f, 0.5f));
		ImGui::Begin(title.c_str(), nullptr, ImGuiWindowFlags_NoSavedSettings);
		auto loctext = lang.getText(page.textBody);
		std::string ptext;
		for (size_t i = 0; i < loctext.size(); i++) {
			if (loctext[i] == '\\') {
				char c = loctext[i + 1];
				if (c == 'n')
					ptext.push_back('\n');
				else if (c == 0)
					break;
				i++;
			}
			else
				ptext.push_back(loctext[i]);
		}
		ImGui::TextUnformatted(ptext.c_str());
		for (size_t i = 0; i < gtw.buttons.size(); i++) {
			auto& button = gtw.buttons[i];
			if (ImGui::Button(lang.getText(button.text).c_str())) {
				switch (button.onClickWindowAction) {
				case Tags::GTW_BUTTON_WINDOW_ACTION_CLOSE_WINDOW:
					activeGtw.second = -1; break;
				case Tags::GTW_BUTTON_WINDOW_ACTION_MOVE_FIRST_PAGE:
					activeGtw.second = 0; break;
				case Tags::GTW_BUTTON_WINDOW_ACTION_MOVE_NEXT_PAGE:
					if (activeGtw.second < gtw.pages.size() - 1) activeGtw.second++; break;
				case Tags::GTW_BUTTON_WINDOW_ACTION_MOVE_PREVIOUS_PAGE:
					if (activeGtw.second > 0) activeGtw.second--; break;
				}
				if(!button.onClickSequence.actionList.empty())
					client->sendGameTextWindowButtonClicked(activeGtw.first, i);
			}
			ImGui::SameLine();
		}
		ImGui::NewLine();
		ImGui::End();
	}

	//----- Rendering -----//

	gfx->ClearFrame(true, true, client->terrain ? client->terrain->fogColor : 0);
	gfx->BeginDrawing();

	client->camera.updateMatrix();
	gfx->SetTransformMatrix(&client->camera.sceneMatrix);

	gfx->SetFog(client->terrain ? client->terrain->fogColor : 0, client->camera.farDist);

	// Get ray
	rayDirection = getRay(client->camera).normal();
	nextSelectedObject = nullptr;
	nextSelObjDistance = std::numeric_limits<float>::infinity();

	if (client->gameSet) {
		if(!scene)
			scene = new Scene(gfx, &client->gameSet->modelCache);

		numObjectsDrawn = 0;
		attachSceneEntities.clear();
		if (client->level)
			drawObject(client->level);
		// test
		//Vector3 peapos = client->camera.position + getRay(client->camera).normal() * 16;
		peapos = Vector3(0, 0, 0);
		if(client->terrain)
			peapos = CalcStampdownPos(client->camera.position, rayDirection, *client->terrain);
		SceneEntity test;
		if (stampdownBlueprint && stampdownPlayer) {
			//test.model = client->gameSet->modelCache.getModel("Warrior Kings Game Set\\Characters\\Peasant\\Male\\Peasant1.MESH3");
			try {
				test.model = stampdownBlueprint->subtypes.at(0).appearances.at(0).animations.at(0).at(0);
				test.transform = Matrix::getTranslationMatrix(peapos);
				test.color = stampdownPlayer->color;
				scene->add(&test);
			}
			catch (const std::out_of_range &) {
				// nop
			}
		}
		for (auto& sfx : client->specialEffects) {
			if (sfx.attachedObj)
				sfx.entity.transform = sfx.attachedObj->getWorldMatrix();
			sfx.entity.animTime = (uint32_t)((client->timeManager.currentTime - sfx.startTime) * 1000.0f);
			scene->add(&sfx.entity);
		}
		if (client->terrain)
			scene->sunDirection = client->terrain->sunVector;
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