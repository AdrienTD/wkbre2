// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "ClientInterface.h"
#include "../gfx/TerrainRenderer.h"
#include "../gfx/DefaultTerrainRenderer.h"
#include "../client.h"
#include "../gfx/renderer.h"
#include "ClientDebugger.h"
#include "../imguiimpl.h"
#include "../window.h"
#include <SDL2/SDL_scancode.h>
#include <SDL2/SDL_mouse.h>
#include "../tags.h"
#include "../gameset/gameset.h"
#include "../gfx/SceneRenderer.h"
#include "../gfx/DefaultSceneRenderer.h"
#include "../imgui/imgui.h"
#include <ctime>
#include "../terrain.h"
#include <SDL2/SDL_timer.h>
#include "../gameset/ScriptContext.h"
#include <stdexcept>
#include "../ParticleContainer.h"
#include "../ParticleSystem.h"
#include "../gfx/DefaultParticleRenderer.h"
#include "../gfx/D3D11EnhancedTerrainRenderer.h"
#include "../settings.h"
#include <nlohmann/json.hpp>
#include "../gfx/D3D11EnhancedSceneRenderer.h"
#include "../StampdownPlan.h"

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
		int nlp = static_cast<int>(farzvalue * 1.5f / lv);
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
		//Vector3 side = client->camera.direction.cross(Vector3(0, 1, 0));
		//float dist = client->camera.direction.dot(obj->position - client->camera.position);
		//if (dist < client->camera.nearDist || dist > client->camera.farDist)
		//	goto drawsub;

		Model* model = nullptr;
		const auto& ap = obj->blueprint->subtypes[obj->subtype].appearances[obj->appearance];
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
		if (!model)
			goto drawsub;

		Vector3 sphereCenter = model->getSphereCenter().transform(obj->getWorldMatrix());
		float sphereRadius = model->getSphereRadius() * std::max({ obj->scale.x, obj->scale.y, obj->scale.z });
		Vector3 ttpp = sphereCenter.transformScreenCoords(client->camera.sceneMatrix);
		bool onCam = (client->camera.position - sphereCenter).len3() < sphereRadius;
		if (!onCam)
			onCam = !(ttpp.x < -1 || ttpp.x > 1 || ttpp.y < -1 || ttpp.y > 1 || ttpp.z < -1 || ttpp.z > 1);
		if (!onCam) {
			Vector3 camdir = client->camera.direction.normal();
			Vector3 H = client->camera.position + camdir * camdir.dot(sphereCenter - client->camera.position);
			Vector3 sphereEnd = sphereCenter + (H - sphereCenter).normal() * sphereRadius;
			ttpp = sphereEnd.transformScreenCoords(client->camera.sceneMatrix);
			onCam = !(ttpp.x < -1 || ttpp.x > 1 || ttpp.y < -1 || ttpp.y > 1 || ttpp.z < -1 || ttpp.z > 1);
		}
		if (onCam) {
			if (model) {
				obj->sceneEntity.model = model;
				obj->sceneEntity.transform = obj->getWorldMatrix();
				obj->sceneEntity.color = obj->getPlayer()->color;
				if (obj->animSynchronizedTask != -1) {
					CliScriptContext ctx{ client, obj };
					float frac = client->gameSet->tasks[obj->animSynchronizedTask].synchAnimationToFraction->eval(&ctx);
					obj->sceneEntity.animTime = (uint32_t)(model->getDuration() * frac * 1000.0f);
				}
				else
					obj->sceneEntity.animTime = (uint32_t)((client->timeManager.currentTime - obj->animStartTime) * 1000.0f);
				obj->sceneEntity.flags = 0;
				if (obj->animClamped) obj->sceneEntity.flags |= SceneEntity::SEFLAG_ANIM_CLAMP_END;
				if (selection.count(obj) >= 1) obj->sceneEntity.flags |= SceneEntity::SEFLAG_NOLIGHTING;
				scene->add(&obj->sceneEntity);
				numObjectsDrawn++;
				// Attachment points
				drawAttachmentPoints(&obj->sceneEntity, obj->id);
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
									nextSelectedObject = obj;
									nextSelObjDistance = dist;
								}
							}
						}
					}
				}
				// Box selection check
				if (selBoxOn) {
					int bx = static_cast<int>((ttpp.x + 1.0f) * g_windowWidth / 2.0f);
					int by = static_cast<int>((-ttpp.y + 1.0f) * g_windowHeight / 2.0f);
					bool inBox = ((selBoxStartX < bx) && (bx < selBoxEndX)) || ((selBoxEndX < bx) && (bx < selBoxStartX));
					inBox = inBox && (((selBoxStartY < by) && (by < selBoxEndY)) || ((selBoxEndY < by) && (by < selBoxStartY)));
					if (inBox) {
						nextSelFromBox.push_back(obj);
					}
				}
			}
		}
	}
drawsub:
	for (auto &typechildren : obj->children) {
		//GameObjBlueprint* blueprint = client->gameSet->getBlueprint(typechildren.first);
		for (CommonGameObject* child : typechildren.second) {
			drawObject((ClientGameObject*)child);
		}
	}
}

void ClientInterface::drawAttachmentPoints(SceneEntity* sceneEntity, uint32_t objid)
{
	Model* model = sceneEntity->model;
	size_t numAPs = model->getNumAPs();
	for (size_t i = 0; i < numAPs; i++) {
		auto ap = model->getAPInfo(i);
		auto state = model->getAPState(i, sceneEntity->animTime);
		if (state.on) {
			if (StrCICompare(ap.tag, "PS_Trail") == 0) {
				particlesContainer->generateTrail(state.position.transform(sceneEntity->transform), objid, client->timeManager.previousTime, client->timeManager.currentTime);
			}
			else if (ap.tag.substr(0, 3) == "PS_") {
				particlesContainer->generate(psCache->getPS(ap.tag.substr(3)), state.position.transform(sceneEntity->transform), objid, client->timeManager.previousTime, client->timeManager.currentTime);
			}
			if (!ap.path.empty()) {
				attachSceneEntities.emplace_back();
				auto& apse = attachSceneEntities.back();
				apse.model = model->cache->getModel("Warrior Kings Game Set\\" + ap.path);
				apse.transform = Matrix::getTranslationMatrix(state.position) * sceneEntity->transform;
				apse.color = sceneEntity->color;
				apse.animTime = (uint32_t)(client->timeManager.currentTime * 1000.0f);
				scene->add(&apse);
				// Recursively draw attach. points of the attach. point's model!
				drawAttachmentPoints(&apse, objid); // NOTE: Same objid kept, trail might be shared!!
			}
		}
	}
}

#define SOL_NO_CHECK_NUMBER_PRECISION 1
#include <sol/sol.hpp>

void ClientInterface::iter()
{
	static Vector3 peapos(0, 0, 0);

	static bool firstTime = true;
	static WndCursor *arrowCursor;
	static WndCursor *currentCursor = nullptr;
	if (firstTime) {
		firstTime = false;
		//arrowCursor = WndCreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
		arrowCursor = WndCreateCursor("Interface\\C_DEFAULT.TGA");
		WndSetCursor(arrowCursor);
		currentCursor = arrowCursor;
		lang.load("Languages\\Language.txt");
	}

	//----- Command cursors -----//

	static Command* selectedCursorCommand = nullptr;
	WndCursor *nextCursor = nullptr;
	Command *rightClickCommand = nullptr;
	static std::vector<std::pair<WndCursor*, Command*>> availableCursorCommands;
	availableCursorCommands.clear();
	for (ClientGameObject *sel : selection) {
		if (sel) {
			CliScriptContext ctx(client, sel);
			auto _ = ctx.change(ctx.target, nextSelectedObject);
			for (Command* cmd : sel->blueprint->offeredCommands) {
				WndCursor* cmdCursor = nullptr;
				for (auto& avcond : cmd->cursorAvailable) {
					if (avcond.first->test->eval(&ctx)) {
						cmdCursor = avcond.second;
						break;
					}
				}
				if (!cmdCursor)
					cmdCursor = cmd->cursor;
				if (cmdCursor) {
					if (std::all_of(cmd->cursorConditions.begin(), cmd->cursorConditions.end(), [&](int eq) {return client->gameSet->equations[eq]->eval(&ctx) > 0.0f; })) {
						if (!nextCursor) {
							nextCursor = cmdCursor;
							rightClickCommand = cmd;
						}
						if (std::find_if(availableCursorCommands.begin(), availableCursorCommands.end(), [&](auto& pair) {return pair.second == cmd; }) == availableCursorCommands.end())
							availableCursorCommands.emplace_back(cmdCursor, cmd);
					}
				}
			}
		}
	}
	if (selectedCursorCommand) {
		// if a specfic cursor command is selected, check that it is still valid, and use it instead
		auto it = std::find_if(availableCursorCommands.begin(), availableCursorCommands.end(), [](auto& pair) {return pair.second == selectedCursorCommand; });
		if (it != availableCursorCommands.end()) {
			nextCursor = it->first;
			rightClickCommand = it->second;
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

	if (g_keyDown[SDL_SCANCODE_UP] || g_keyDown[SDL_SCANCODE_W])
		client->camera.position += forward;
	if (g_keyDown[SDL_SCANCODE_DOWN] || g_keyDown[SDL_SCANCODE_S])
		client->camera.position -= forward;
	if (g_keyDown[SDL_SCANCODE_RIGHT] || g_keyDown[SDL_SCANCODE_D])
		client->camera.position -= strafe;
	if (g_keyDown[SDL_SCANCODE_LEFT] || g_keyDown[SDL_SCANCODE_A])
		client->camera.position += strafe;

	if (g_keyDown[SDL_SCANCODE_KP_0] || g_mouseDown[SDL_BUTTON_MIDDLE]) {
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
		client->camera.position.y += g_mouseWheel * 2;

	static bool clickedScriptedUi = false;
	if (!clickedScriptedUi && g_mousePressed[SDL_BUTTON_LEFT]) {
		debugger.selectObject(nextSelectedObject);
		if (!g_modShift)
			selection.clear();
		if (nextSelectedObject) {
			selection.insert(nextSelectedObject);
			if (g_mouseDoubleClicked == SDL_BUTTON_LEFT) {
				printf("Double click!\n");
				// select nearby units of same type
				auto walkObj = [&](ClientGameObject* obj, const auto& rec) -> void {
					if (obj->blueprint == nextSelectedObject->blueprint) {
						if ((obj->position - nextSelectedObject->position).len2xz() < 25.0f) {
							selection.insert(obj);
						}
					}
					for (const auto& [type, objList] : obj->children) {
						for (const auto& child : objList) {
							rec((ClientGameObject*)child, rec);
						}
					}
					};
				walkObj(nextSelectedObject->getPlayer(), walkObj);
			}
		}
		stampdownBlueprint = nullptr;
		selBoxStartX = selBoxEndX = g_mouseX;
		selBoxStartY = selBoxEndY = g_mouseY;
		selBoxOn = true;
		selectedCursorCommand = nullptr;
	}

	if (g_mouseDown[SDL_BUTTON_LEFT]) {
		selBoxEndX = g_mouseX;
		selBoxEndY = g_mouseY;
	}
	else {
		if (selBoxOn) {
			selBoxOn = false;
			//if (!g_modShift)
			//	selection.clear();
			selection.insert(nextSelFromBox.begin(), nextSelFromBox.end());
		}
	}

	if (!clickedScriptedUi && g_mousePressed[SDL_BUTTON_RIGHT]) {
		if (stampdownBlueprint && stampdownPlayer) {
			client->sendStampdown(stampdownBlueprint, stampdownPlayer, peapos, sendStampdownEvent, stampdownFromCommand);
			if (stampdownFromCommand) {
				const Command* cmd = &client->gameSet->commands[client->gameSet->commands.names.getIndex("Build")];
				for (ClientGameObject* obj : selection) {
					if (obj) {
						if (std::find(obj->blueprint->offeredCommands.begin(), obj->blueprint->offeredCommands.end(), cmd) != obj->blueprint->offeredCommands.end()) {
							int assignmentMode = g_modCtrl ? Tags::ORDERASSIGNMODE_DO_FIRST : (g_modShift ? Tags::ORDERASSIGNMODE_DO_LAST : Tags::ORDERASSIGNMODE_FORGET_EVERYTHING_ELSE);
							client->sendBuildLastStampdownedObject(obj, assignmentMode);
						}
					}
				}
				if (!g_modShift && !g_modCtrl)
					stampdownBlueprint = nullptr;
			}
		}
		else if (rightClickCommand) {
			Vector3 destPos = peapos;
			const int NUM_DIRECTIONS = 8;
			const float SEPARATION = 3.0f;
			const float TWOPI = 2.0f * 3.1415f;
			float nextPosDir = 0.0f;
			float nextPosRadius = SEPARATION;
			for (ClientGameObject *sel : selection) {
				if (sel) {
					if (std::find(sel->blueprint->offeredCommands.begin(), sel->blueprint->offeredCommands.end(), rightClickCommand) != sel->blueprint->offeredCommands.end()) {
						// if object offers this command, send the command
						int assignmentMode = g_modCtrl ? Tags::ORDERASSIGNMODE_DO_FIRST : (g_modShift ? Tags::ORDERASSIGNMODE_DO_LAST : Tags::ORDERASSIGNMODE_FORGET_EVERYTHING_ELSE);
						client->sendCommand(sel, rightClickCommand, assignmentMode, nextSelectedObject, destPos);
						// calc destination pos for next selected object
						destPos = peapos + Vector3(std::cos(nextPosDir), 0.0f, std::sin(nextPosDir)) * nextPosRadius;
						nextPosDir += SEPARATION / nextPosRadius;
						if (nextPosDir >= TWOPI) {
							nextPosDir = 0.0f;
							nextPosRadius += SEPARATION;
						}
					}
				}
			}
		}
	}

	clickedScriptedUi = false;

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
	if (g_keyPressed[SDL_SCANCODE_DELETE]) {
		for (ClientGameObject* sel : selection)
			if (sel)
				client->sendTerminateObject(sel);
	}

	static bool debuggerOn = true;
	static bool uiScriptOn = true;
	if (g_keyPressed[SDL_SCANCODE_F2]) {
		debuggerOn = !debuggerOn;
	}
	if (g_keyPressed[SDL_SCANCODE_F3]) {
		uiScriptOn = !uiScriptOn;
	}

	static int elevatorMouseStart = 0;
	static float elevatorCamStart = 0.0f;
	if (g_mousePressed[SDL_BUTTON_X1]) {
		elevatorMouseStart = g_mouseY;
		elevatorCamStart = client->camera.position.y;
	}
	if (g_mouseDown[SDL_BUTTON_X1])
		client->camera.position.y = elevatorCamStart + (elevatorMouseStart - g_mouseY) * 0.1f;

	if (g_keyPressed[SDL_SCANCODE_E]) {
		// select available cursor command next to the one currently shown
		auto it = std::find_if(availableCursorCommands.begin(), availableCursorCommands.end(), [&](auto& pair) { return pair.second == rightClickCommand; });
		if (it != availableCursorCommands.end()) {
			size_t index = it - availableCursorCommands.begin();
			if (index == availableCursorCommands.size() - 1)
				selectedCursorCommand = nullptr;
			else
				selectedCursorCommand = availableCursorCommands[index + 1].second;
		}
	}

	//----- ImGui -----//

	ImGuiImpl_NewFrame();
	if (debuggerOn)
		debugger.draw();
	ImGui::Begin("Client Interface Debug");
	ImGui::Text("ODPF: %i", numObjectsDrawn);
	ImGui::Text("FPS: %i", framesPerSecond);
	ImGui::DragFloat3("peapos", &peapos.x);
	if (ImGui::Button("Start level"))
		client->sendStartLevelRequest();
	static bool showTerrain = true;
	ImGui::Checkbox("Terrain", &showTerrain);
#ifdef _WIN32
	if (g_settings.value<bool>("enhancedGraphics", false)) {
		if (D3D11EnhancedTerrainRenderer* etr = static_cast<D3D11EnhancedTerrainRenderer*>(terrainRenderer)) {
			if (client->terrain)
				ImGui::DragFloat3("Sun", &client->terrain->sunVector.x, 0.1f);
			etr->m_lampPos = peapos + Vector3(0, 4, 0);
			ImGui::DragFloat3("Lamp", &etr->m_lampPos.x, 0.1f);
			ImGui::Checkbox("Bump", &etr->m_bumpOn);
		}
	}
#endif
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
					if (activeGtw.second < (int)gtw.pages.size() - 1) activeGtw.second++; break;
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

	//----- Scriptable UI -----//
	static bool luaInitialized = false;
	static sol::state lua;
	static sol::load_result luaScript;
	static TextureCache csTextureCache(gfx);
	if (uiScriptOn && !luaInitialized) {
		lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string);
		// UI functions
		lua.set_function("drawText", 
			[](int x, int y, const std::string& text) {
				ImGui::GetBackgroundDrawList()->AddText(ImVec2((float)x, (float)y), -1, text.c_str());
			});
		lua.set_function("drawRect",
			[](int x, int y, int width, int height, uint32_t color) {
				ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2((float)x, (float)y), ImVec2((float)(x + width), (float)(y + height)), color);
			});
		lua.set_function("drawImage",
			[](int x, int y, int width, int height, const std::string& path) {
				texture tex = csTextureCache.getTexture(path.c_str(), true);
				ImGui::GetBackgroundDrawList()->AddImage(tex, ImVec2((float)x, (float)y), ImVec2((float)(x + width), (float)(y + height)));
			});
		lua.set_function("getWindowWidth", []() {return g_windowWidth; });
		lua.set_function("getWindowHeight", []() {return g_windowHeight; });
		// Client object
		lua.set_function("getPlayerID",
			[this]() { ClientGameObject* obj = client->clientPlayer.get(); return obj ? obj->id : 0u; }
		);
		lua.set_function("getItem",
			[this](uint32_t id, const std::string& item) {
				int itemId = client->gameSet->items.names.getIndex(item);
				return client->findObject(id)->getItem(itemId);
			});
		lua.set_function("getFirstSelection",
			[this]() -> uint32_t {
				if (selection.empty()) return 0u;
				ClientGameObject* obj = selection.begin()->get();
				return obj ? obj->id : 0u;
			}
		);
		lua.set_function("getColor",
			[this](uint32_t id) {
				ClientGameObject* obj = client->findObject(id);
				return obj ? obj->color : 0;
			});
		lua.set_function("getObjectClass",
			[this](uint32_t id) {
				ClientGameObject* obj = client->findObject(id);
				return obj ? obj->blueprint->bpClass : -1;
			}
		);
		// Client
		lua.set_function("getWKVersion",
			[this]() {return client->gameSet ? client->gameSet->version : 0; }
		);
		lua.set("GSVERSION_WKONE", GameSet::GSVERSION_WKONE);
		lua.set("GSVERSION_WKBATTLES", GameSet::GSVERSION_WKBATTLES);
		lua.set_function("getGameTime", [this]() {return client->timeManager.currentTime; });
		lua.set_function("getSystemTime", [this]() {return (float)SDL_GetTicks() / 1000.0f; });
		// Game set
		lua.set_function("computeEquation",
			[this](const std::string& equationName, uint32_t objId) -> float {
				int equationIndex = client->gameSet->equations.names.getIndex(equationName);
				if (equationIndex == -1) return 0.0f;
				CliScriptContext ctx{ client, client->findObject(objId) };
				return client->gameSet->equations[equationIndex]->eval(&ctx);
			}
		);
		// Blueprint
		lua.set_function("getBlueprint",
			[this](uint32_t id) -> const GameObjBlueprint* {
				ClientGameObject* obj = client->findObject(id);
				return obj ? obj->blueprint : nullptr;
			}
		);
		lua.set_function("getCommand", [this](const std::string& name) -> const Command* {
			int commandIndex = client->gameSet->commands.names.getIndex(name);
			if (commandIndex == -1) return nullptr;
			return &client->gameSet->commands[commandIndex];
			});
		sol::usertype<GameObjBlueprint> blueprint_type = lua.new_usertype<GameObjBlueprint>("GameObjBlueprint");
		blueprint_type.set("name", sol::readonly(&GameObjBlueprint::name));
		blueprint_type.set("bpClass", sol::readonly(&GameObjBlueprint::bpClass));
		//AA lua.set_function("offeredCommands", &GameObjBlueprint::offeredCommands);
		lua.set_function("getOfferedCommandsCount", [](const GameObjBlueprint* bp) {return bp->offeredCommands.size(); });
		lua.set_function("getOfferedCommand", [](const GameObjBlueprint* bp, int index) -> Command* {return bp->offeredCommands.at(index); });
		// Command
		sol::usertype<Command> command_type = lua.new_usertype<Command>("Command");
		lua.set("buttonAvailable", sol::readonly(&Command::buttonAvailable));
		lua.set("buttonDepressed", sol::readonly(&Command::buttonDepressed));
		lua.set("buttonEnabled", sol::readonly(&Command::buttonEnabled));
		lua.set("buttonHighlighted", sol::readonly(&Command::buttonHighlighted));
		lua.set("buttonImpossible", sol::readonly(&Command::buttonImpossible));
		lua.set("buttonWait", sol::readonly(&Command::buttonWait));
		lua.set_function("getCommandHint",
			[this](const Command* cmd, uint32_t objid) -> std::string {
				ClientGameObject* obj = client->findObject(objid);
				if (!obj) return "No object, no hint";
				CliScriptContext ctx{ client, obj };
				auto _ = ctx.change(ctx.selectedObject, obj);
				auto condPred = [&ctx](GSCondition* cond) {return cond->test->booleval(&ctx); };
				auto it = std::find_if_not(cmd->conditionsImpossible.begin(), cmd->conditionsImpossible.end(), condPred);
				if (it != cmd->conditionsImpossible.end()) {
					return (*it)->getFormattedHint(lang, &ctx);
				}
				it = std::find_if_not(cmd->conditionsWait.begin(), cmd->conditionsWait.end(), condPred);
				if (it != cmd->conditionsWait.end()) {
					return (*it)->getFormattedHint(lang, &ctx);
				}
				it = std::find_if_not(cmd->conditionsWarning.begin(), cmd->conditionsWarning.end(), condPred);
				if (it != cmd->conditionsWarning.end()) {
					return (*it)->getFormattedHint(lang, &ctx);
				}
				if (!cmd->defaultHint.empty())
					return GSCondition::getFormattedHint(cmd->defaultHint, cmd->defaultHintValues, lang, &ctx);
				return client->gameSet->commands.getString(cmd);
			}
		);
		// Command list
		enum class CSInfo {
			ENABLED = 0,
			IMPOSSIBLE = 1,
			WAIT = 2
		};
		lua.new_enum("CSInfo",
			"ENABLED", CSInfo::ENABLED,
			"IMPOSSIBLE", CSInfo::IMPOSSIBLE,
			"WAIT", CSInfo::WAIT);
		struct CommandState {
			const Command* command;
			const std::string* texpath;
			CSInfo info;
			int count = 0;
			bool isCurrentOrder = false;
		};
		sol::usertype<CommandState> cs_type = lua.new_usertype<CommandState>("CommandState");
		//cs_type.set("command", sol::readonly(&CommandState::command));
		//cs_type.set("texpath", sol::readonly(&CommandState::texpath));
		cs_type.set("command", sol::readonly_property([](CommandState* cs) {return cs->command; }));
		cs_type.set("texpath", sol::readonly_property([](CommandState* cs) {return *cs->texpath; }));
		cs_type.set("info", sol::readonly(&CommandState::info));
		cs_type.set("count", sol::readonly(&CommandState::count));
		cs_type.set("isCurrentOrder", sol::readonly(&CommandState::isCurrentOrder));
		cs_type.set("buttonAvailable", sol::readonly_property([](CommandState* cs) {return cs->command->buttonAvailable; }));
		cs_type.set("buttonDepressed", sol::readonly_property([](CommandState* cs) {return cs->command->buttonDepressed; }));
		cs_type.set("buttonEnabled", sol::readonly_property([](CommandState* cs) {return cs->command->buttonEnabled; }));
		cs_type.set("buttonHighlighted", sol::readonly_property([](CommandState* cs) {return cs->command->buttonHighlighted; }));
		cs_type.set("buttonImpossible", sol::readonly_property([](CommandState* cs) {return cs->command->buttonImpossible; }));
		cs_type.set("buttonWait", sol::readonly_property([](CommandState* cs) {return cs->command->buttonWait; }));
		
		lua.set_function("listSelectionCommands", [this](const std::function<bool(const std::string&)>& filter) {
			std::vector<CommandState> cmdStates;
			ClientGameObject* sel = nullptr; //selectedObject;
			if (!sel && !selection.empty()) sel = *selection.begin();
			if (sel) {
				CliScriptContext ctx{ client, sel };
				auto _tv = ctx.change(ctx.selectedObject, sel);
				for (Command* cmd : sel->blueprint->offeredCommands) {
					if (cmd->buttonAvailable.empty() && cmd->buttonEnabled.empty())
						continue;
					const std::string cmdname = client->gameSet->commands.names.getString(cmd->id);
					if (!filter(cmdname))
						continue;
					auto iconPred = [this, &ctx](int eq) {return client->gameSet->equations[eq]->booleval(&ctx); };
					auto condPred = [&ctx](GSCondition* cond) {return cond->test->booleval(&ctx); };
					if (std::all_of(cmd->iconConditions.begin(), cmd->iconConditions.end(), iconPred)) {
						// Icon is shown
						CommandState cs;
						cs.command = cmd;
						if (!std::all_of(cmd->conditionsImpossible.begin(), cmd->conditionsImpossible.end(), condPred)) {
							cs.texpath = &cmd->buttonImpossible;
							cs.info = CSInfo::IMPOSSIBLE;
						}
						else if (!std::all_of(cmd->conditionsWait.begin(), cmd->conditionsWait.end(), condPred)) {
							cs.texpath = &cmd->buttonWait;
							cs.info = CSInfo::WAIT;
						}
						else {
							cs.texpath = cmd->buttonEnabled.empty() ? &cmd->buttonAvailable : &cmd->buttonEnabled;
							cs.info = CSInfo::ENABLED;
						}
						if (!cs.texpath->empty()) {
							if (cmd->order) {
								cs.isCurrentOrder = cmd->order->bpid == sel->reportedCurrentOrder;
								if (auto it = sel->buildingOrderCountMap.find(cmd->order->bpid); it != sel->buildingOrderCountMap.end())
									cs.count = it->second;
							}
							cmdStates.push_back(std::move(cs));
							//cmdStates.push_back(lua.create_table_with(
							//	"command", lua.wrapp cs.command,
							//	"texpath", *cs.texpath,
							//	"info", cs.info
							//));
						}
					}
				}
			}
			return sol::as_nested(cmdStates);
			});
		// Mouse
		lua.set_function("getMouseX", []() {return g_mouseX; });
		lua.set_function("getMouseY", []() {return g_mouseY; });
		lua.set_function("isMousePressed", [](int button) {return g_mousePressed[button % std::size(g_mousePressed)]; });
		lua.set_function("isMouseDown", [](int button) {return g_mouseDown[button % std::size(g_mouseDown)]; });
		// Keyboard
		lua.set_function("isCtrlHeld", []() {return g_modCtrl; });
		lua.set_function("isShiftHeld", []() {return g_modShift; });
		lua.set_function("isAltHeld", []() {return g_modAlt; });
		// ImGui
		lua.set_function("setTooltip",[](const std::string& str) { ImGui::SetTooltip("%s", str.c_str()); });
		// Button
		lua.set_function("handleButton",
			[]() -> int {
				clickedScriptedUi = true;
				if (g_mouseReleased[SDL_BUTTON_LEFT]) {
					return 1;
				}
				if (g_mouseReleased[SDL_BUTTON_RIGHT]) {
					return 3;
				}
				return 0;
			});
		lua.set_function("launchCommand",
			[this](const Command* cmd, int assignmentMode, int count) {
				if (cmd->stampdownObject) {
					stampdownBlueprint = cmd->stampdownObject;
					stampdownPlayer = (*selection.begin())->getPlayer();
					stampdownFromCommand = true;
				}
				else if (client->gameSet->commands.getString(cmd) == "Formation") {
					for (ClientGameObject* obj : selection) {
						client->sendPutUnitIntoNewFormation(obj);
					}
					client->sendCreateNewFormation();
				}
				else {
					for (ClientGameObject* obj : selection)
						if (obj)
							if (std::find(obj->blueprint->offeredCommands.begin(), obj->blueprint->offeredCommands.end(), cmd) != obj->blueprint->offeredCommands.end())
								for (int i = 0; i < count; i++)
									client->sendCommand(obj, cmd, assignmentMode);
				}
			});
		lua.set_function("forceLaunchCommand",
			[this](const Command* cmd, int assignmentMode) {
				for (ClientGameObject* obj : selection)
					if (obj)
						client->sendCommand(obj, cmd, assignmentMode);
			});
		lua.set_function("cancelCommand",
			[this](const Command* cmd, int count) {
				for (ClientGameObject* obj : selection)
					if (obj)
						for (int i = 0; i < count; ++i)
							client->sendCancelCommand(obj, cmd);
			});
		// Tags
		auto addTags = [](auto& lua, auto& tagDict, const std::string& prefix) {
			for (size_t i = 0; i < tagDict.tags.size(); ++i) {
				lua.set(prefix + tagDict.tags[i], i);
			}
		};
		addTags(lua, Tags::GAMEOBJCLASS_tagDict, "GAMEOBJCLASS_");
		addTags(lua, Tags::ORDERASSIGNMODE_tagDict, "ORDERASSIGNMODE_");
		// Misc
		//lua.set_function("sin",static_cast<double(*)(double)>(&std::sin));
		//lua.set_function("round",static_cast<double(*)(double)>(&std::round));

		//luaScript = lua.load_file("redata/ClientUIScript.lua");
		luaInitialized = true;
		try {
			lua.script_file("redata/ClientUIScript.lua");
		}
		catch (const sol::error& err) {
			printf("Sol init error:\n%s\n", err.what());
			uiScriptOn = false;
		}
	}
	if (uiScriptOn) {
		try {
			lua["CCUI_PerFrame"]();
		}
		catch (const sol::error& err) {
			printf("Sol error:\n%s\n", err.what());
			uiScriptOn = false;
		}
	}
	if (!uiScriptOn && luaInitialized) {
		luaInitialized = false;
		luaScript = {};
		lua = {};
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
	nextSelFromBox.clear();

	if (client->gameSet) {
		if(!scene)
			scene = new Scene(gfx, &client->gameSet->modelCache);
		if (!particlesContainer)
			particlesContainer = new ParticleContainer;
		if (!psCache)
			psCache = new PSCache("Warrior Kings Game Set\\Particle_Systems");
		if (!particleRenderer)
			particleRenderer = new DefaultParticleRenderer(gfx, particlesContainer);

		numObjectsDrawn = 0;
		attachSceneEntities.clear();
		if (client->getLevel())
			drawObject(client->getLevel());
		if (nextSelectedObject)
			nextSelectedObject->sceneEntity.flags |= SceneEntity::SEFLAG_NOLIGHTING;
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
				if (auto* fp = stampdownBlueprint->footprint) {
					peapos.x = std::floor(peapos.x / 5.0f) * 5.0f + 2.5f + fp->originX;
					peapos.z = std::floor(peapos.z / 5.0f) * 5.0f + 2.5f + fp->originZ;
					if (client->terrain)
						peapos.y = client->terrain->getHeightEx(peapos.x, peapos.z, stampdownBlueprint->canWalkOnWater());
				}
				test.transform = Matrix::getTranslationMatrix(peapos);
				test.color = stampdownPlayer->color;
				scene->add(&test);

				static std::vector<SceneEntity> planEntities;
				planEntities.clear();
				auto plan = StampdownPlan::getStampdownPlan(client, client->clientPlayer.get(), stampdownBlueprint, peapos, Vector3(0, 0, 0));
				planEntities.reserve(plan.toCreate.size());
				for (auto& creation : plan.toCreate) {
					auto& entity = planEntities.emplace_back();
					entity.transform = Matrix::getRotationYMatrix(-creation.orientation.y) * Matrix::getTranslationMatrix(creation.position);
					entity.model = creation.blueprint->subtypes.at(0).appearances.at(0).animations.at(0).at(0);
					entity.color = stampdownPlayer->color;
				}
				for (auto& entity : planEntities) {
					scene->add(&entity);
				}
			}
			catch (const std::out_of_range &) {
				// nop
			}
		}
		for (auto& sfx : client->specialEffects) {
			if (sfx.attachedObj)
				sfx.entity.transform = sfx.attachedObj->getWorldMatrix();
			Vector3 ttpp = sfx.entity.transform.getTranslationVector().transformScreenCoords(client->camera.sceneMatrix);
			if (ttpp.x >= -1 && ttpp.x <= 1 && ttpp.y >= -1 && ttpp.y <= 1 && ttpp.z >= -1 && ttpp.z <= 1) {
				sfx.entity.animTime = (uint32_t)((client->timeManager.currentTime - sfx.startTime) * 1000.0f);
				scene->add(&sfx.entity);
				drawAttachmentPoints(&sfx.entity);
			}
		}
		for (auto& lsfx : client->loopingSpecialEffects) {
			Vector3 ttpp = lsfx.second.transform.getTranslationVector().transformScreenCoords(client->camera.sceneMatrix);
			if (ttpp.x >= -1 && ttpp.x <= 1 && ttpp.y >= -1 && ttpp.y <= 1 && ttpp.z >= -1 && ttpp.z <= 1) {
				lsfx.second.animTime = (uint32_t)(client->timeManager.currentTime * 1000.0f);
				scene->add(&lsfx.second);
				drawAttachmentPoints(&lsfx.second);
			}
		}
		if (client->terrain)
			scene->sunDirection = client->terrain->sunVector;
		if (!sceneRenderer) {
#ifdef _WIN32
			if (g_settings.value<bool>("enhancedGraphics", false))
				sceneRenderer = new D3D11EnhancedSceneRenderer(gfx, scene, &client->camera);
			else
				sceneRenderer = new DefaultSceneRenderer(gfx, scene, &client->camera);
#else
			sceneRenderer = new DefaultSceneRenderer(gfx, scene, &client->camera);
#endif
		}
		sceneRenderer->render();
		scene->clear();
	}

	if (terrainRenderer && showTerrain)
		terrainRenderer->render();

	if (particleRenderer && particlesContainer) {
		gfx->BeginParticles();
		gfx->BeginBatchDrawing();
		particleRenderer->render(client->timeManager.previousTime, client->timeManager.currentTime, client->camera);
		particlesContainer->update(client->timeManager.currentTime);
	}

	if (selBoxOn) {
		gfx->InitRectDrawing();
		gfx->NoTexture(0);
		gfx->DrawFrame(selBoxStartX, selBoxStartY, selBoxEndX - selBoxStartX, selBoxEndY - selBoxStartY, -1);
	}

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
#ifdef _WIN32
	if (g_settings.value<bool>("enhancedGraphics", false))
		this->terrainRenderer = new D3D11EnhancedTerrainRenderer(gfx, client->terrain, &client->camera);
	else
		this->terrainRenderer = new DefaultTerrainRenderer(gfx, client->terrain, &client->camera);
#else
	this->terrainRenderer = new DefaultTerrainRenderer(gfx, client->terrain, &client->camera);
#endif
}