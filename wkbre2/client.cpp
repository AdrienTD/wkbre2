// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#define _USE_MATH_DEFINES
#include "client.h"
#include "server.h"
#include "network.h"
#include "gameset/gameset.h"
#include <cassert>
#include "terrain.h"
#include "interface/ClientInterface.h"
#include <SDL2/SDL_timer.h>
#include "gameset/ScriptContext.h"
#include <cmath>
#include "SoundPlayer.h"

Client * Client::instance = nullptr;

void Client::loadFromServerObject(Server & server)
{
}

void Client::tick()
{
	timeManager.tick();

	const auto walkObj = [this](ClientGameObject *obj, auto &rec) -> void {
		if (obj->movement.isMoving()) {
			Vector3 newPosition = obj->movement.getNewPosition(timeManager.currentTime);
			newPosition.y = terrain->getHeightEx(newPosition.x, newPosition.z, obj->blueprint->canWalkOnWater());
			obj->updatePosition(newPosition);
			Vector3 dir = obj->movement.getDirection();
			obj->orientation.y = atan2f(dir.x, -dir.z);
		}
		if (obj->trajectory.isMoving()) {
			obj->updatePosition(obj->trajectory.getPosition(timeManager.currentTime));
			obj->orientation = obj->trajectory.getRotationAngles(timeManager.currentTime);
		}
		for (auto &it : obj->children)
			for (CommonGameObject *obj : it.second)
				rec((ClientGameObject*)obj, rec);
	};
	if(getLevel())
		walkObj(getLevel(), walkObj);

	// Camera paths
	auto resetCameraMode = [this](bool skipActions = false) {
		if (cameraCurrentPath && !skipActions) {
			sendCameraPathEnded(cameraPathIndex);
		}
		cameraMode = 0;
		cameraCurrentPath = nullptr;
	};
	if (cameraMode == 1) {
		float camTime = (float)(SDL_GetTicks() - cameraStartTime) / 1000.0f;
		float prevTime = 0.0f, nextTime = 0.0f;
		size_t nextNode;
		for (nextNode = 1; nextNode < cameraPathPos.size(); nextNode++) {
			prevTime = nextTime;
			nextTime += cameraPathDur[nextNode];
			if (camTime < nextTime)
				break;
		}
		if (nextNode == cameraPathPos.size()) {
			resetCameraMode();
		}
		else {
			auto p0 = cameraPathPos[nextNode - 1];
			auto p1 = cameraPathPos[nextNode];
			// Limit angles to [0;2pi)
			constexpr float pi = (float)M_PI;
			for (auto& p : { &p0, &p1 }) {
				for (float& c : p->second) {
					c = fmodf(c, 2*pi);
					if (c < 0.0f) c += 2*pi;
				}
			}
			// Try to make distances between angles as small as possible (< pi radians)
			for (int i = 0; i < 3; i++) {
				float d = p1.second.coord[i] - p0.second.coord[i];
				if (fabs(d) > pi) {
					if (d > 0)
						p1.second.coord[i] -= 2 * pi;
					else
						p1.second.coord[i] += 2 * pi;
				}
			}
			float dt = (camTime - prevTime) / (nextTime - prevTime);
			Vector3 pos = (p1.first - p0.first)*dt + p0.first;
			Vector3 ori = (p1.second - p0.second)*dt + p0.second;
			camera.position = pos;
			camera.orientation = ori;
		}
	}

	// Sound & music
	SoundPlayer* soundPlayer = SoundPlayer::getSoundPlayer();
	soundPlayer->setListenerPosition(camera.position, camera.direction.normal());
	if (isMusicPlaying) {
		if (!soundPlayer->isMusicPlaying()) {
			isMusicPlaying = false;
			NetPacketWriter msg{ NETSRVMSG_MUSIC_COMPLETED };
			serverLink->send(msg);
		}
	}

	// Special effects
	specialEffects.remove_if([this](SpecialEffect& sfx) {return timeManager.currentTime >= sfx.startTime + sfx.entity.model->getDuration(); });
	for (auto nextIt = loopingSpecialEffects.begin(); nextIt != loopingSpecialEffects.end();) {
		auto it = nextIt++;
		auto& lsfx = *it;
		if (!lsfx.first.first.get())
			loopingSpecialEffects.erase(it);
	}

	if (serverLink) {
		uint32_t pstime = SDL_GetTicks();
		if ((pstime - dbgLastMessageCountTime) >= 1000) {
			dbgLastMessageCountTime = pstime;
			dbgNumMessagesPerSec = dbgNumMessagesInCurrentSec;
			dbgNumMessagesInCurrentSec = 0;
		}
		dbgNumMessagesPerTick = 0;

		int pcnt = 1000000; //100;
		while (serverLink->available() && (pcnt--)) {
			NetPacket packet = serverLink->receive();
			BinaryReader br(packet.data.c_str());
			uint8_t type = br.readUint8();
			++dbgNumMessagesPerTick;

			switch (type) {
			case NETCLIMSG_TEST: {
				std::string msg = br.readStringZ();
				chatMessages.push_back(msg);
				info("Client got message: %s\n", msg.c_str());
				break;
			}
			case NETCLIMSG_GAME_SET:
			{
				std::string gsFileName = br.readStringZ();
				int gsVersion = br.readUint8();
				if (localServer) {
					info("Client takes gameset from local server.\n");
					gameSet = Server::instance->gameSet;
				}
				else {
					info("Loading game set: %s\n", gsFileName.c_str());
					gameSet = new GameSet(gsFileName.c_str(), gsVersion);
				}
				break;
			}
			case NETCLIMSG_CONTROL_PLAYER: {
				clientPlayer = br.readUint32();
				break;
			}
			case NETCLIMSG_OBJECT_CREATED: {
				uint32_t objid = br.readUint32();
				uint32_t bpid = br.readUint32();
				uint32_t subtype = br.readUint32();
				uint32_t appear = br.readUint32();
				GameObjBlueprint *blueprint = gameSet->getBlueprint(bpid);
				info("Object %u of type %s is created.\n", objid, blueprint->getFullName().c_str());
				ClientGameObject *obj = createObject(blueprint, objid);
				obj->subtype = subtype;
				obj->appearance = appear;
				if (blueprint->bpClass == Tags::GAMEOBJCLASS_LEVEL)
					level = obj;
				break;
			}
			case NETCLIMSG_OBJECT_ITEM_SET: {
				uint32_t objid = br.readUint32();
				uint32_t index = br.readUint32();
				float value = br.readFloat();
				info("Object %u changed its item \"%s\" to the value %f.\n", objid, gameSet->items.names.getString(index).c_str(), value);
				auto* obj = findObject(objid);
				if (!obj) printf("CLI-WARN: Obj %i from message does not exist\n", objid);
				if (obj)
					obj->items[index] = value;
				break;
			}
			case NETCLIMSG_OBJECT_PARENT_SET: {
				uint32_t objid = br.readUint32();
				uint32_t parentid = br.readUint32();
				info("Object %u is parented to object %u.\n", objid, parentid);
				ClientGameObject *obj = findObject(objid), *newParent = findObject(parentid);
				if (obj->parent) {
					auto &vec = obj->parent->children[obj->blueprint->getFullId()];
					vec.erase(std::remove(vec.begin(), vec.end(), obj), vec.end()); // is order important?
				}
				obj->parent = newParent;
				if (newParent)
					newParent->children[obj->blueprint->getFullId()].push_back(obj);
				break;
			}
			case NETCLIMSG_TERRAIN_SET: {
				std::string mapfp = br.readStringZ();
				terrain = new Terrain;
				terrain->readFromFile(mapfp.c_str());
				if (cliInterface)
					cliInterface->updateTerrain();
				auto area = this->terrain->getNumPlayableTiles();
				this->tiles = std::make_unique<Tile[]>(area.first * area.second);
				break;
			}
			case NETCLIMSG_OBJECT_POSITION_SET: {
				uint32_t objid = br.readUint32();
				Vector3 pos = br.readVector3();
				info("Object %u teleported to (%f,%f,%f)\n", objid, pos.x, pos.y, pos.z);
				if (ClientGameObject *obj = findObject(objid)) {
					obj->updatePosition(pos);
				}
				break;
			}
			case NETCLIMSG_OBJECT_ORIENTATION_SET: {
				uint32_t objid = br.readUint32();
				Vector3 ori = br.readVector3();
				info("Object %u oriented to (%f,%f,%f)\n", objid, ori.x, ori.y, ori.z);
				if (ClientGameObject *obj = findObject(objid)) {
					obj->orientation = ori;
				}
				break;
			}
			case NETCLIMSG_OBJECT_SUBTYPE_APPEARANCE_SET: {
				uint32_t objid = br.readUint32();
				uint32_t subtype = br.readUint32();
				uint32_t appear = br.readUint32();
				if (ClientGameObject *obj = findObject(objid)) {
					obj->subtype = subtype;
					obj->appearance = appear;
				}
				break;
			}
			case NETCLIMSG_OBJECT_COLOR_SET: {
				uint32_t objid = br.readUint32();
				uint8_t color = br.readUint8();
				info("Object %u changed its color to %u\n", objid, color);
				if (ClientGameObject *obj = findObject(objid)) {
					obj->color = color;
				}
				break;
			}
			case NETCLIMSG_TIME_SYNC: {
				timeManager.psCurrentTime = br.readUint32();
				timeManager.currentTime = (float)timeManager.psCurrentTime / 1000.0f;
				timeManager.paused = br.readUint8();
				break;
			}
			case NETCLIMSG_OBJECT_ANIM_SET: {
				uint32_t objid = br.readUint32();
				if (ClientGameObject *obj = findObject(objid)) {
					obj->animationIndex = br.readUint32();
					obj->animationVariant = br.readUint32();
					obj->animStartTime = br.readFloat();
					obj->animClamped = br.readUint8();
					obj->animSynchronizedTask = br.readUint32();
				}
				break;
			}
			case NETCLIMSG_OBJECT_MOVEMENT_STARTED: {
				uint32_t objid = br.readUint32();
				Vector3 src = br.readVector3();
				Vector3 dst = br.readVector3();
				float startTime = br.readFloat();
				float speed = br.readFloat();
				if (ClientGameObject *obj = findObject(objid)) {
					obj->movement.startMovement(src, dst, startTime, speed);
				}
				break;
			}
			case NETCLIMSG_OBJECT_MOVEMENT_STOPPED: {
				uint32_t objid = br.readUint32();
				if (ClientGameObject *obj = findObject(objid)) {
					obj->movement.stopMovement();
				}
				break;
			}
			case NETCLIMSG_OBJECT_REMOVED: {
				uint32_t objid = br.readUint32();
				if (ClientGameObject *obj = findObject(objid)) {
					// remove from parent's children
					auto &vec = obj->parent->children.at(obj->blueprint->getFullId());
					vec.erase(std::find(vec.begin(), vec.end(), obj));
					// remove from ID map
					idmap.erase(obj->id);
					// delete the object, bye!
					delete obj;
				}
				break;
			}
			case NETCLIMSG_OBJECT_CONVERTED: {
				uint32_t objid = br.readUint32();
				uint32_t postbpid = br.readUint32();
				if (ClientGameObject *obj = findObject(objid)) {
					GameObjBlueprint *postbp = gameSet->getBlueprint(postbpid);
					// remove from parent's children
					auto &vec = obj->parent->children.at(obj->blueprint->getFullId());
					vec.erase(std::find(vec.begin(), vec.end(), obj));
					// add it back at the correct blueprint key
					obj->parent->children[postbp->getFullId()].push_back(obj);
					// now converted!
					obj->blueprint = postbp;
				}
				break;
			}
			case NETCLIMSG_DIPLOMATIC_STATUS_SET: {
				uint32_t id1 = br.readUint32();
				uint32_t id2 = br.readUint32();
				uint8_t status = br.readUint8();
				ClientGameObject *a = findObject(id1), *b = findObject(id2);
				if (a && b) {
					int &ref = (a->id <= b->id) ? diplomaticStatuses[{ a->id, b->id }] : diplomaticStatuses[{ b->id, a->id }];
					ref = status;
				}
				break;
			}
			case NETCLIMSG_SHOW_GAME_TEXT_WINDOW: {
				int gtwIndex = br.readUint32();
				gtwStates[gtwIndex] = 0;
				const auto& snd = gameSet->gameTextWindows[gtwIndex].pages[0].activationSound;
				if(!snd.empty())
					SoundPlayer::getSoundPlayer()->playSound(snd);
				break;
			}
			case NETCLIMSG_OBJECT_SCALE_SET: {
				auto id = br.readUint32();
				Vector3 scale = br.readVector3();
				if (ClientGameObject* obj = findObject(id))
					obj->scale = scale;
				break;
			}
			case NETCLIMSG_HIDE_GAME_TEXT_WINDOW: {
				const int gtwIndex = br.readUint32();
				gtwStates.erase(gtwIndex);
				break;
			}
			case NETCLIMSG_HIDE_CURRENT_GAME_TEXT_WINDOW: {
				gtwStates.clear();
				break;
			}
			case NETCLIMSG_OBJECT_FLAGS_SET: {
				uint32_t objid = br.readUint32();
				uint8_t flags = br.readUint8();
				if (ClientGameObject* obj = findObject(objid)) {
					obj->flags = flags;
				}
				break;
			}
			case NETCLIMSG_SNAP_CAMERA_POSITION: {
				resetCameraMode();
				camera.position = br.readVector3();
				camera.orientation = br.readVector3();
				break;
			}
			case NETCLIMSG_STORE_CAMERA_POSITION: {
				storedCameraPosition = camera.position;
				storedCameraOrientation = camera.orientation;
				break;
			}
			case NETCLIMSG_RESTORE_CAMERA_POSITION: {
				resetCameraMode();
				camera.position = storedCameraPosition;
				camera.orientation = storedCameraOrientation;
				break;
			}
			case NETCLIMSG_PLAY_CAMERA_PATH: {
				resetCameraMode();
				int camPathIndex = br.readUint32();
				const CameraPath& gspath = gameSet->cameraPaths[camPathIndex]; // TODO: boundary check
				cameraMode = 1;
				cameraCurrentPath = &gspath;
				cameraPathIndex = camPathIndex;
				cameraStartTime = SDL_GetTicks();
				cameraPathPos.clear();
				cameraPathDur.clear();
				if (gspath.startAtCurrentCameraPosition) {
					cameraPathPos.push_back({ camera.position, camera.orientation });
					cameraPathDur.push_back(1.0f);
				}
				for (auto& node : gspath.pathNodes) {
					CliScriptContext ctx(this);
					OrientedPosition op = node.position->eval(&ctx);
					cameraPathPos.push_back({ op.position, op.rotation });
					cameraPathDur.push_back(node.duration->eval(&ctx));
				}
				if (!cameraPathPos.empty()) {
					std::tie(camera.position, camera.orientation) = cameraPathPos.front();
				}
				break;
			}
			case NETCLIMSG_STOP_CAMERA_PATH: {
				bool skipActions = br.readUint8();
				resetCameraMode(skipActions);
				break;
			}
			case NETCLIMSG_OBJECT_TRAJECTORY_STARTED: {
				uint32_t objid; Vector3 initPos, initVel; float startTime;
				br.readTo(objid, initPos, initVel, startTime);
				ClientGameObject* obj = findObject(objid);
				obj->trajectory.start(initPos, initVel, startTime);
				break;
			}
			case NETCLIMSG_GAME_SPEED_CHANGED: {
				timeManager.setSpeed(br.readFloat());
				break;
			}
			case NETCLIMSG_SOUND_AT_OBJECT: {
				uint32_t objid; int soundTag; uint32_t targetid; uint8_t randval;
				br.readTo(objid, soundTag, targetid, randval);
				if (ClientGameObject* from = findObject(objid)) {
					if (ClientGameObject* to = findObject(targetid)) {
						std::string path; float refDist, maxDist;
						std::tie(path, refDist, maxDist) = from->blueprint->getSound(soundTag, from->subtype);
						if (!path.empty()) {
							std::string gfspath = "Warrior Kings Game Set\\Sounds\\" + path;
							if (to->blueprint->bpClass == Tags::GAMEOBJCLASS_PLAYER)
								SoundPlayer::getSoundPlayer()->playSound(gfspath);
							else {
								//float rolloff = (refDist / 0.1f - refDist) / (maxDist - refDist);
								SoundPlayer::getSoundPlayer()->playSound3D(gfspath, to->position, refDist, maxDist);
							}
						}
					}
				}
				break;
			}
			case NETCLIMSG_SOUND_AT_POSITION: {
				uint32_t objid; int soundTag; Vector3 targetPos; uint8_t randval;
				br.readTo(objid, soundTag, targetPos, randval);
				if (ClientGameObject* from = findObject(objid)) {
					std::string path; float refDist, maxDist;
					std::tie(path, refDist, maxDist) = from->blueprint->getSound(soundTag, from->subtype);
					if (!path.empty()) {
						std::string gfspath = "Warrior Kings Game Set\\Sounds\\" + path;
						SoundPlayer::getSoundPlayer()->playSound3D(gfspath, targetPos, refDist, maxDist);
					}
				}
				break;
			}
			case NETCLIMSG_MUSIC_CHANGED: {
				int musicTag;
				br.readTo(musicTag);
				if (ClientGameObject* player = clientPlayer) {
					auto it = player->blueprint->musicMap.find(musicTag);
					if (it != player->blueprint->musicMap.end()) {
						size_t var = (size_t)rand() % it->second.size();
						SoundPlayer::getSoundPlayer()->playMusic("Warrior Kings Game Set\\Sounds\\" + it->second[var]);
						isMusicPlaying = true;
					}
				}
				break;
			}
			case NETCLIMSG_SPECIAL_EFFECT_PLAYED_AT_POS: {
				uint32_t objid; int sfxTag; Vector3 pos;
				br.readTo(objid, sfxTag, pos);
				if (auto* obj = findObject(objid)) {
					if (Model* sfxmodel = obj->blueprint->getSpecialEffect(sfxTag)) {
						specialEffects.emplace_back();
						auto& sfx = specialEffects.back();
						sfx.entity.transform = Matrix::getTranslationMatrix(pos);
						sfx.entity.model = sfxmodel;
						sfx.entity.color = obj->color;
						sfx.startTime = timeManager.currentTime; // problem: client might start at different time...
					}
				}
				break;
			}
			case NETCLIMSG_SPECIAL_EFFECT_PLAYED_BETWEEN: {
				uint32_t objid; int sfxTag; Vector3 pos1, pos2;
				br.readTo(objid, sfxTag, pos1, pos2);
				if (auto* obj = findObject(objid)) {
					if (Model* sfxmodel = obj->blueprint->getSpecialEffect(sfxTag)) {
						Vector3 dir = pos2 - pos1;
						float dist = dir.len3();
						float ang = std::atan2(dir.x, -dir.z);

						specialEffects.emplace_back();
						auto& sfx = specialEffects.back();
						sfx.entity.transform = Matrix::getScaleMatrix({ dist,dist,dist }) * Matrix::getRotationYMatrix(-ang) * Matrix::getTranslationMatrix(pos1);
						sfx.entity.model = sfxmodel;
						sfx.entity.color = obj->color;
						sfx.startTime = timeManager.currentTime; // problem: client might start at different time...
					}
				}
				break;
			}
			case NETCLIMSG_ATTACHED_SPECIAL_EFFECT_PLAYED: {
				uint32_t objid; int sfxTag; uint32_t targetid;
				br.readTo(objid, sfxTag, targetid);
				if (auto* obj = findObject(objid)) {
					if (auto* target = findObject(targetid)) {
						if (Model* sfxmodel = obj->blueprint->getSpecialEffect(sfxTag)) {
							specialEffects.emplace_back();
							auto& sfx = specialEffects.back();
							sfx.entity.model = sfxmodel;
							sfx.entity.color = obj->color;
							sfx.attachedObj = target;
							sfx.startTime = timeManager.currentTime; // problem: client might start at different time...
						}
					}
				}
				break;
			}
			case NETCLIMSG_OBJECT_NAME_SET: {
				uint32_t objid = br.readUint32();
				int numChars = br.readUint8();
				if (ClientGameObject* obj = findObject(objid))
					obj->name = br.readString(numChars);
				break;
			}
			case NETCLIMSG_CURRENT_ORDER_REPORT: {
				uint32_t objid = br.readUint32();
				if (ClientGameObject* obj = findObject(objid))
					obj->reportedCurrentOrder = (int)br.readUint32();
				break;
			}
			case NETCLIMSG_LOOPING_SPECIAL_EFFECT_ATTACHED: {
				uint32_t objid; int sfxTag; Vector3 pos;
				br.readTo(objid, sfxTag, pos);
				if (ClientGameObject* obj = findObject(objid)) {
					if (Model* sfxmodel = obj->blueprint->getSpecialEffect(sfxTag)) {
						std::pair<std::pair<CliGORef, int>, SceneEntity> p;
						p.first = { objid, sfxTag };
						p.second.transform = Matrix::getTranslationMatrix(pos);
						p.second.model = sfxmodel;
						p.second.color = obj->color;
						loopingSpecialEffects.insert(std::move(p));
					}
				}
				break;
			}
			case NETCLIMSG_LOOPING_SPECIAL_EFFECT_DETACHED: {
				uint32_t objid; int sfxTag;
				br.readTo(objid, sfxTag);
				if (ClientGameObject* obj = findObject(objid)) {
					auto it = loopingSpecialEffects.find({ objid, sfxTag });
					if (it != loopingSpecialEffects.end()) {
						loopingSpecialEffects.erase(it);
					}
				}
				break;
			}
			case NETCLIMSG_SKIP_CAMERA_PATH: {
				if (cameraCurrentPath && !cameraPathPos.empty()) {
					std::tie(camera.position, camera.orientation) = cameraPathPos.back();
				}
				resetCameraMode();
				break;
			}
			case NETCLIMSG_ALIAS_ASSIGNED: {
				auto [aliasIndex, objectId] = br.readValues<int, uint32_t>();
				aliases[aliasIndex].insert(objectId);
				break;
			}
			case NETCLIMSG_ALIAS_UNASSIGNED: {
				auto [aliasIndex, objectId] = br.readValues<int, uint32_t>();
				aliases[aliasIndex].erase(objectId);
				break;
			}
			case NETCLIMSG_ALIAS_CLEARED: {
				auto [aliasIndex] = br.readValues<int>();
				aliases[aliasIndex].clear();
				break;
			}
			case NETCLIMSG_INTERPOLATE_CAMERA_TO_POSITION: {
				resetCameraMode();
				auto [targetPosition, targetOrientation, duration] = br.readValues<Vector3, Vector3, float>();
				cameraMode = 1;
				cameraCurrentPath = nullptr;
				cameraPathIndex = -1;
				cameraStartTime = SDL_GetTicks();
				cameraPathPos = { {camera.position, camera.orientation}, {targetPosition, targetOrientation} };
				cameraPathDur = { 1.0f, duration };
				break;
			}
			case NETCLIMSG_INTERPOLATE_CAMERA_TO_STORED_POSITION: {
				resetCameraMode();
				auto [duration] = br.readValues<float>();
				cameraMode = 1;
				cameraCurrentPath = nullptr;
				cameraPathIndex = -1;
				cameraStartTime = SDL_GetTicks();
				cameraPathPos = { {camera.position, camera.orientation}, {storedCameraPosition, storedCameraOrientation} };
				cameraPathDur = { 1.0f, duration };
				break;
			}
			case NETCLIMSG_UPDATE_BUILDING_ORDER_COUNT_MAP: {
				auto [objectId, orderBlueprintId, updatedCount] = br.readValues<uint32_t, int, int>();
				if (ClientGameObject* obj = findObject(objectId))
					obj->buildingOrderCountMap[orderBlueprintId] = updatedCount;
				break;
			}
			case NETCLIMSG_CITY_RECTANGLE_ADDED: {
				auto [objectId, xStart, yStart, xEnd, yEnd] = br.readValues<uint32_t, int, int, int, int>();
				ClientGameObject::CityRectangle rect;
				rect.xStart = xStart;
				rect.yStart = yStart;
				rect.xEnd = xEnd;
				rect.yEnd = yEnd;
				if (ClientGameObject* obj = findObject(objectId))
					obj->cityRectangles.push_back(rect);
			}
			}
		}
		dbgNumMessagesInCurrentSec += dbgNumMessagesPerTick;
	}
}

void Client::sendMessage(const std::string &msg) {
	NetPacketWriter packet(NETSRVMSG_TEST);
	packet.writeStringZ(msg);
	serverLink->send(packet);
}

void Client::sendCommand(ClientGameObject * obj, const Command * cmd, int assignmentMode, ClientGameObject *target, const Vector3 & destination)
{
	NetPacketWriter packet(NETSRVMSG_COMMAND);
	packet.writeUint32(cmd->id);
	packet.writeUint32(obj->id);
	packet.writeUint8(assignmentMode);
	packet.writeUint32(target ? target->id : 0);
	packet.writeVector3(destination);
	serverLink->send(packet);
}

void Client::sendPauseRequest(uint8_t pauseState)
{
	NetPacketWriter packet(NETSRVMSG_PAUSE);
	packet.writeUint8(pauseState);
	serverLink->send(packet);
}

void Client::sendStampdown(GameObjBlueprint * blueprint, ClientGameObject * player, const Vector3 & position, bool sendEvent, bool inGameplay)
{
	NetPacketWriter msg(NETSRVMSG_STAMPDOWN);
	msg.writeUint32(blueprint->getFullId());
	msg.writeUint32(player->id);
	msg.writeVector3(position);
	uint8_t flags = 0;
	flags |= sendEvent ? 1 : 0;
	flags |= inGameplay ? 2 : 0;
	msg.writeUint8(flags);
	serverLink->send(msg);
}

void Client::sendStartLevelRequest()
{
	serverLink->send(NetPacketWriter(NETSRVMSG_START_LEVEL));
}

void Client::sendGameTextWindowButtonClicked(int gtwIndex, int buttonIndex)
{
	NetPacketWriter msg(NETSRVMSG_GAME_TEXT_WINDOW_BUTTON_CLICKED);
	msg.writeUint32(gtwIndex);
	msg.writeUint32(buttonIndex);
	serverLink->send(msg);
}

void Client::sendCameraPathEnded(int camPathIndex)
{
	NetPacketWriter msg{ NETSRVMSG_CAMERA_PATH_ENDED };
	msg.writeUint32(camPathIndex);
	serverLink->send(msg);
}

void Client::sendGameSpeedChange(float nextSpeed)
{
	NetPacketWriter msg{ NETSRVMSG_CHANGE_GAME_SPEED };
	msg.writeFloat(nextSpeed);
	serverLink->send(msg);
}

void Client::sendTerminateObject(ClientGameObject* obj)
{
	NetPacketWriter msg{ NETSRVMSG_TERMINATE_OBJECT };
	msg.writeUint32(obj->id);
	serverLink->send(msg);
}

void Client::sendBuildLastStampdownedObject(ClientGameObject* obj, int assignmentMode)
{
	NetPacketWriter msg{ NETSRVMSG_BUILD_LAST_STAMPDOWNED_OBJECT };
	msg.writeUint32(obj->id);
	msg.writeUint32(assignmentMode);
	serverLink->send(msg);
}

void Client::sendCancelCommand(ClientGameObject* obj, const Command* cmd)
{
	NetPacketWriter msg{ NETSRVMSG_CANCEL_COMMAND };
	msg.writeValues(obj->id, cmd->id);
	serverLink->send(msg);
}

void Client::sendPutUnitIntoNewFormation(ClientGameObject* obj)
{
	NetPacketWriter msg{ NETSRVMSG_PUT_UNIT_INTO_NEW_FORMATION };
	msg.writeValues(obj->id);
	serverLink->send(msg);
}

void Client::sendCreateNewFormation()
{
	NetPacketWriter msg{ NETSRVMSG_CREATE_NEW_FORMATION };
	serverLink->send(msg);
}

ClientGameObject * Client::createObject(GameObjBlueprint * blueprint, uint32_t id)
{
	ClientGameObject *obj = new ClientGameObject(id, blueprint);
	assert(idmap[id] == nullptr);
	idmap[id] = obj;
	return obj;
}

void ClientGameObject::updatePosition(const Vector3& newposition)
{
	// TODO: Generalize with server equivalent instead of duplicating code
	Client* prog = Client::instance;
	Vector3 oldposition = position;
	position = newposition;
	if (prog->tiles) {
		int trnNumX, trnNumZ;
		std::tie(trnNumX, trnNumZ) = prog->terrain->getNumPlayableTiles();
		int tx = static_cast<int>(position.x / 5.0f), tz = static_cast<int>(position.z / 5.0f);
		int newtileIndex;
		if (tx >= 0 && tx < trnNumX && tz >= 0 && tz < trnNumZ)
			newtileIndex = tz * trnNumX + tx;
		else
			newtileIndex = -1;
		if (newtileIndex != tileIndex) {
			int prevtileIndex = tileIndex;
			tileIndex = newtileIndex;
			if (prevtileIndex != -1) {
				auto& vec = prog->tiles[prevtileIndex].objList;
				vec.erase(std::find(vec.begin(), vec.end(), this->id));
			}
			if (newtileIndex != -1) {
				prog->tiles[newtileIndex].objList.push_back(this->id);
			}
			updateOccupiedTiles(oldposition, orientation, newposition, orientation);
		}
	}
}

void ClientGameObject::updateOccupiedTiles(const Vector3& oldposition, const Vector3& oldorientation, const Vector3& newposition, const Vector3& neworientation)
{
	Client::instance->updateOccupiedTiles(this, oldposition, oldorientation, newposition, neworientation);
}
