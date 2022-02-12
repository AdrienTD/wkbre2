// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "server.h"
#include "file.h"
#include "util/GSFileParser.h"
#include "tags.h"
#include "gameset/gameset.h"
#include "network.h"
#include "terrain.h"
#include "gameset/ScriptContext.h"
#include "NNSearch.h"
#include "settings.h"
#include <nlohmann/json.hpp>
#include <codecvt>
#include <atomic>

Server *Server::instance = nullptr;

std::atomic_int g_diag_serverTicks{ 0 };

void Server::loadSaveGame(const char * filename)
{
	char *filetext; int filesize;
	LoadFile(filename, &filetext, &filesize, 1);
	filetext[filesize] = 0;
	GSFileParser gsf(filetext);

	while (!gsf.eof)
	{
		std::string strtag = gsf.nextTag();
		int tag = Tags::SAVEGAME_tagDict.getTagID(strtag.c_str());

		switch (tag) {
		case Tags::SAVEGAME_GAME_SET: {
			std::string gsFileName = gsf.nextString(true);
			int version = g_settings.at("gameVersion").get<int>();
			gameSet = new GameSet(gsFileName.c_str(), version);
			NetPacketWriter msg(NETCLIMSG_GAME_SET);
			msg.writeStringZ(gsFileName);
			msg.writeUint8((uint8_t)version);
			sendToAll(msg);
			break;
		}
		case Tags::SAVEGAME_NEXT_UNIQUE_ID:
			nextUniqueId = gsf.nextInt();
			break;
		case Tags::SAVEGAME_PREDEC:
			loadSavePredec(gsf); break;
		case Tags::SAVEGAME_LEVEL:
			level = loadObject(gsf, strtag);
			break;
		case Tags::SAVEGAME_TIME_MANAGER_STATE:
			timeManager.load(gsf);
			break;
		case Tags::SAVEGAME_NUM_HUMAN_PLAYERS: {
			size_t numPlayers = gsf.nextInt();
			numPlayers = std::min(clientLinks.size(), numPlayers);
			for (size_t i = 0; i < clientLinks.size(); i++)
				setClientPlayerControl(i, gsf.nextInt());
			break;
		}
		}
		gsf.advanceLine();
	}

	free(filetext);

	for (auto &t : postAssociations)
		std::get<0>(t)->associateObject(std::get<1>(t), findObject(std::get<2>(t)));

	for (size_t i = 0; i < clientPlayerObjects.size(); i++)
		clientPlayerObjects[i]->clientIndex = i;

	timeManager.unlock();
	timeManager.unpause();
	lastSync = time(nullptr);
}

ServerGameObject* Server::createObject(GameObjBlueprint * blueprint, uint32_t id)
{
	if (!id) {
		while (idmap.count(++nextUniqueId) != 0);
		id = nextUniqueId;
	}
	ServerGameObject *obj = new ServerGameObject(id, blueprint);
	idmap[id] = obj;

	obj->subtype = rand() % blueprint->subtypeNames.size();
	auto& stappearances = blueprint->subtypes[obj->subtype].appearances;
	if (stappearances.count(0))
		obj->appearance = 0;
	else if (!stappearances.empty())
		obj->appearance = std::next(stappearances.begin(), rand() % stappearances.size())->first;

	NetPacketWriter msg(NETCLIMSG_OBJECT_CREATED);
	msg.writeUint32(id);
	msg.writeUint32(blueprint->getFullId());
	msg.writeUint32(obj->subtype);
	msg.writeUint32(obj->appearance);
	sendToAll(msg);

	obj->updateSightRange();

	Vector3 randVec{ (float)rand() / (float)(RAND_MAX), (float)rand() / (float)(RAND_MAX), (float)rand() / (float)(RAND_MAX) };
	Vector3 varyScale = blueprint->minScaleVary + randVec * (blueprint->maxScaleVary - blueprint->minScaleVary);
	obj->setScale(blueprint->scaleAppearance * varyScale);

	return obj;
}

void Server::deleteObject(ServerGameObject * obj)
{
	if (obj->deleted) return;
	obj->deleted = true;
	// delete subordinates first
	for (auto& st : obj->children) {
		for (CommonGameObject* child : st.second) {
			deleteObject((ServerGameObject*)child);
		}
	}
	if (objToDeleteLast) {
		objToDeleteLast->nextDeleted = obj;
		objToDeleteLast = obj;
	}
	else {
		objToDelete = obj;
		objToDeleteLast = obj;
	}
}

void Server::destroyObject(ServerGameObject* obj)
{
	// remove associations from/to this object
	for (auto& ref : obj->associates)
		for (const SrvGORef& ass : ref.second) {
			assert(ass.get());
			ass->associators[ref.first].erase(obj);
		}
	for (auto& ref : obj->associators)
		for (const SrvGORef& ass : ref.second) {
			assert(ass.get());
			ass->associates[ref.first].erase(obj);
		}

	// remove from parent's children
	auto& vec = obj->parent->children.at(obj->blueprint->getFullId());
	vec.erase(std::find(vec.begin(), vec.end(), obj));
	//std::swap(*std::find(vec.begin(), vec.end(), obj), vec.back());
	//vec.pop_back();

	// remove from ID map
	idmap.erase(obj->id);

	// delete the object, bye!
	int id = obj->id;
	delete obj;

	// report removal to the clients
	NetPacketWriter msg(NETCLIMSG_OBJECT_REMOVED);
	msg.writeUint32(id);
	sendToAll(msg);
}

ServerGameObject* Server::loadObject(GSFileParser & gsf, const std::string &clsname)
{
	int cls = Tags::GAMEOBJCLASS_tagDict.getTagID(clsname.c_str());
	int goid = gsf.nextInt();
	std::string bpname = gsf.nextString(true);

	GameObjBlueprint *blueprint;
	if (bpname.empty())
		blueprint = predec[goid];
	else
		blueprint = gameSet->findBlueprint(cls, bpname);

	ServerGameObject *obj = createObject(blueprint, goid);

	gsf.advanceLine();
	std::string endtag = "END_" + clsname;
	while (!gsf.eof) {
		std::string strtag = gsf.nextTag();
		int tag = Tags::GAMEOBJ_tagDict.getTagID(strtag.c_str());
		switch (tag) {
		case Tags::GAMEOBJ_ITEM: {
			std::string itemstr = gsf.nextString(true);
			int item = gameSet->items.names.getIndex(itemstr);
			if (item != -1)
				obj->setItem(item, gsf.nextFloat());
			else
				printf("WARNING: Item \"%s\" doesn't exist!\n", itemstr.c_str());
			break;
		}
		case Tags::GAMEOBJ_POSITION: {
			float x = gsf.nextFloat();
			float y = gsf.nextFloat();
			float z = gsf.nextFloat();
			obj->setPosition(Vector3(x, y, z));
			break;
		}
		case Tags::GAMEOBJ_ORIENTATION: {
			float x = gsf.nextFloat();
			float y = gsf.nextFloat();
			float z = gsf.nextFloat();
			obj->setOrientation(Vector3(x, y, z));
			break;
		}
		case Tags::GAMEOBJ_MAP: {
			std::string mapfp = gsf.nextString(true);
			this->terrain = new Terrain();
			this->terrain->readFromFile(mapfp.c_str());
			NetPacketWriter packet(NETCLIMSG_TERRAIN_SET);
			packet.writeStringZ(mapfp);
			sendToAll(packet);
			auto area = this->terrain->getNumPlayableTiles();
			this->tiles = std::make_unique<Tile[]>(area.first * area.second);
			break;
		}
		case Tags::GAMEOBJ_COLOUR_INDEX: {
			obj->setColor(gsf.nextInt());
			break;
		}
		case Tags::GAMEOBJ_SUBTYPE: {
			obj->setSubtypeAndAppearance(obj->blueprint->subtypeNames.getIndex(gsf.nextString(true)), obj->appearance);
			break;
		}
		case Tags::GAMEOBJ_APPEARANCE: {
			std::string subtype = gsf.nextString(true);
			std::string appear = gsf.nextString(true);
			obj->setSubtypeAndAppearance(obj->blueprint->subtypeNames.getIndex(subtype), gameSet->appearances.names.getIndex(appear));
			break;
		}
		case Tags::GAMEOBJ_ORDER_CONFIGURATION: {
			gsf.advanceLine();
			while(!gsf.eof) {
				std::string strtag = gsf.nextTag();
				if (strtag == "UNIQUE_ORDER_ID") {
					obj->orderConfig.nextOrderId = gsf.nextInt();
				}
				else if (strtag == "ORDER") {
					auto orderName = gsf.nextString(true);
					int orderType = gameSet->orders.names.getIndex(orderName); assert(orderType != -1);
					gsf.advanceLine();
					OrderBlueprint &orderBp = gameSet->orders[orderType];
					obj->orderConfig.orders.emplace_back(0, &orderBp, obj);
					Order &order = obj->orderConfig.orders.back();
					while(!gsf.eof) {
						std::string ordtag = gsf.nextTag();
						if (ordtag == "PROCESS_STATE") {
							order.state = gsf.nextInt();
						}
						else if (ordtag == "ORDER_ID") {
							order.id = gsf.nextInt();
						}
						else if (ordtag == "UNIQUE_TASK_ID") {
							order.nextTaskId = gsf.nextInt();
						}
						else if (ordtag == "CURRENT_TASK") {
							order.currentTask = gsf.nextInt();
						}
						else if (ordtag == "TASK") {
							int taskType = gameSet->tasks.names.getIndex(gsf.nextString(true)); assert(taskType != -1);
							gsf.advanceLine();
							TaskBlueprint &taskBp = gameSet->tasks[taskType];
							Task *taskptr = Task::create(0, &taskBp, &order);
							Task &task = *taskptr;
							while (!gsf.eof) {
								std::string tsktag = gsf.nextTag();
								if (tsktag == "TARGET") {
									task.setTarget(findObject(gsf.nextInt()));
								}
								else if (tsktag == "PROXIMITY") {
									task.proximity = gsf.nextFloat();
								}
								else if (tsktag == "PROXIMITY_SATISFIED") {
									task.proximitySatisfied = gsf.nextInt();
								}
								else if (tsktag == "TRIGGERS_STARTED") {
									task.triggersStarted = gsf.nextInt();
								}
								else if (tsktag == "TRIGGER") {
									task.triggers[gsf.nextInt()]->parse(gsf, *gameSet);
								}
								else if (tsktag == "FIRST_EXECUTION") {
									task.firstExecution = gsf.nextInt();
								}
								else if (tsktag == "PROCESS_STATE") {
									task.state = gsf.nextInt();
								}
								else if (tsktag == "TASK_ID") {
									task.id = gsf.nextInt();
								}
								else if (tsktag == "START_SEQUENCE_EXECUTED") {
									task.startSequenceExecuted = gsf.nextInt();
								}
								else if (tsktag == "DESTINATION") {
									float x = gsf.nextFloat();
									float z = gsf.nextFloat();
									task.destination = Vector3(x, 0.0f, z);
								}
								else if (tsktag == "LAST_DESTINATION_VALID") {
									task.lastDestinationValid = gsf.nextInt();
								}
								else if (tsktag == "SPAWN_BLUEPRINT") {
									if (SpawnTask* spawnTask = dynamic_cast<SpawnTask*>(taskptr)) {
										spawnTask->toSpawn = gameSet->readObjBlueprintPtr(gsf);
									}
								}
								else if (tsktag == "END_TASK") {
									break;
								}
								gsf.advanceLine();
							}
							order.tasks.push_back(taskptr);
						}
						else if (ordtag == "END_ORDER") {
							break;
						}
						gsf.advanceLine();
					}
				}
				else if (strtag == "END_ORDER_CONFIGURATION")
					break;
				gsf.advanceLine();
			}
			break;
		}
		case Tags::GAMEOBJ_ALIAS: {
			auto &alias = aliases[gameSet->aliases.readIndex(gsf)];
			int count = gsf.nextInt();
			for (int i = 0; i < count; i++) {
				alias.emplace(gsf.nextInt());
			}
			break;
		}
		case Tags::GAMEOBJ_INDIVIDUAL_REACTION: {
			obj->individualReactions.insert(gameSet->reactions.readPtr(gsf));
			break;
		}
		case Tags::GAMEOBJ_ASSOCIATE: {
			int category = gameSet->associations.readIndex(gsf);
			int count = gsf.nextInt();
			for (int i = 0; i < count; i++)
				postAssociations.emplace_back(obj, category, gsf.nextInt());
				//obj->associateObject(category, findObject(gsf.nextInt()));
			break;
		}
		case Tags::GAMEOBJ_DIPLOMATIC_STATUS_BETWEEN: {
			int id1 = gsf.nextInt();
			int id2 = gsf.nextInt();
			int status = gameSet->diplomaticStatuses.readIndex(gsf);
			setDiplomaticStatus(findObject(id1), findObject(id2), status);
			break;
		}
		case Tags::GAMEOBJ_NEXT_UNIQUE_ID: {
			this->nextUniqueId = gsf.nextInt();
			break;
		}
		case Tags::GAMEOBJ_TILES: {
			int len = gsf.nextInt();
			for (int i = 0; i < len; i++) {
				int x = gsf.nextInt();
				int z = gsf.nextInt();
				obj->addZoneTile(x, z);
			}
			break;
		}
		case Tags::GAMEOBJ_NAME: {
			if (gameSet->version >= gameSet->GSVERSION_WKBATTLES && blueprint->bpClass == Tags::GAMEOBJCLASS_PLAYER) {
				int numChars = gsf.nextInt() & 255;
				std::wstring wname = std::wstring(numChars, 0);
				for (int i = 0; i < numChars; i++)
					wname [i] = (wchar_t)gsf.nextInt();
				std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> cvt;
				obj->setName(cvt.to_bytes(wname));
			}
			else {
				obj->setName(gsf.nextString(true));
			}
			break;
		}
		case Tags::GAMEOBJ_AI_CONTROLLER: {
			obj->aiController.parse(gsf, *gameSet);
			break;
		}
		case Tags::GAMEOBJ_PLAYER:
		case Tags::GAMEOBJ_CHARACTER:
		case Tags::GAMEOBJ_BUILDING:
		case Tags::GAMEOBJ_MARKER:
		case Tags::GAMEOBJ_CONTAINER:
		case Tags::GAMEOBJ_PROP:
		case Tags::GAMEOBJ_CITY:
		case Tags::GAMEOBJ_TOWN:
		case Tags::GAMEOBJ_FORMATION:
		case Tags::GAMEOBJ_ARMY:
		case Tags::GAMEOBJ_MISSILE:
		case Tags::GAMEOBJ_TERRAIN_ZONE:
		//case Tags::GAMEOBJ_USER:
		{
			ServerGameObject *child = loadObject(gsf, strtag);
			child->setParent(obj);
			break;
		}
		}
		if (strtag == endtag)
			return obj;
		gsf.advanceLine();
	}

	return obj;
}

void Server::loadSavePredec(GSFileParser & gsf)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		std::string strtag = gsf.nextTag();
		int cls = Tags::GAMEOBJCLASS_tagDict.getTagID(strtag.c_str());
		if (cls != -1) {
			std::string bpname = gsf.nextString(true);
			GameObjBlueprint *bp = gameSet->findBlueprint(cls, bpname);
			int numObjs = gsf.nextInt();
			int i = 0;
			while (i < numObjs) {
				int soi = gsf.nextInt();
				if (!soi) {
					gsf.advanceLine(); continue;
				}
				predec[soi] = bp;
				i++;
			}
		}
		else if (strtag == "END_PREDEC")
			break;
		gsf.advanceLine();
	}
}

void ServerGameObject::setItem(int index, float value)
{
	assert(index != -1);
	items[index] = value;

	NetPacketWriter msg(NETCLIMSG_OBJECT_ITEM_SET);
	msg.writeUint32(this->id);
	msg.writeUint32(index);
	msg.writeFloat(value);
	Server::instance->sendToAll(msg);

}

void ServerGameObject::setParent(ServerGameObject * newParent)
{
	if (this->parent) {
		auto &vec = this->parent->children[this->blueprint->getFullId()];
		vec.erase(std::remove(vec.begin(), vec.end(), this), vec.end()); // is order important?
	}
	this->parent = newParent;
	if(newParent)
		newParent->children[this->blueprint->getFullId()].push_back(this);

	NetPacketWriter msg(NETCLIMSG_OBJECT_PARENT_SET);
	msg.writeUint32(this->id);
	msg.writeUint32(newParent->id);
	Server::instance->sendToAll(msg);
	
}

void ServerGameObject::setPosition(const Vector3 & position)
{
	updatePosition(position);
	if (Server::instance->terrain)
		if (blueprint->bpClass != Tags::GAMEOBJCLASS_MISSILE)
			this->position.y = Server::instance->terrain->getHeightEx(position.x, position.z, blueprint->canWalkOnWater());

	NetPacketWriter msg(NETCLIMSG_OBJECT_POSITION_SET);
	msg.writeUint32(this->id);
	msg.writeVector3(this->position);
	Server::instance->sendToAll(msg);
}

void ServerGameObject::setOrientation(const Vector3 & orientation)
{
	updateOccupiedTiles(position, this->orientation, position, orientation);
	this->orientation = orientation;

	NetPacketWriter msg(NETCLIMSG_OBJECT_ORIENTATION_SET);
	msg.writeUint32(this->id);
	msg.writeVector3(orientation);
	Server::instance->sendToAll(msg);
}

void ServerGameObject::setSubtypeAndAppearance(int new_subtype, int new_appearance)
{
	if (this->blueprint->subtypes[new_subtype].appearances.count(new_appearance) == 0) {
		if (new_appearance != 0)
			new_appearance = this->appearance;
		// If default or original appearance does not exist, use random one (needed for Death animations)
		auto& stappearances = blueprint->subtypes[new_subtype].appearances;
		if (stappearances.count(new_appearance) == 0) {
			if (!stappearances.empty())
				new_appearance = std::next(stappearances.begin(), rand() % stappearances.size())->first;
			else
				new_appearance = 0;
		}
	}

	if (new_subtype == this->subtype && new_appearance == this->appearance)
		return;

	this->subtype = new_subtype;
	this->appearance = new_appearance;

	NetPacketWriter msg(NETCLIMSG_OBJECT_SUBTYPE_APPEARANCE_SET);
	msg.writeUint32(this->id);
	msg.writeUint32(new_subtype);
	msg.writeUint32(new_appearance);
	Server::instance->sendToAll(msg);
}

void ServerGameObject::setColor(int color)
{
	this->color = color;
	NetPacketWriter msg(NETCLIMSG_OBJECT_COLOR_SET);
	msg.writeUint32(this->id);
	msg.writeUint8(color);
	Server::instance->sendToAll(msg);
}

void ServerGameObject::setAnimation(int animationIndex, bool isClamped)
{
	this->animationIndex = animationIndex;
	this->animationVariant = animationVariant;
	this->animStartTime = Server::instance->timeManager.currentTime;
	this->animClamped = isClamped;
	NetPacketWriter msg(NETCLIMSG_OBJECT_ANIM_SET);
	msg.writeUint32(this->id);
	msg.writeUint32(this->animationIndex);
	msg.writeUint32(this->animationVariant);
	msg.writeFloat(this->animStartTime);
	msg.writeUint8(this->animClamped);
	Server::instance->sendToAll(msg);
}

void ServerGameObject::startMovement(const Vector3 & destination)
{
	float speed = computeSpeed();
	movement.startMovement(position, destination, Server::instance->timeManager.currentTime, speed);
	currentSpeed = speed;
	NetPacketWriter npw{ NETCLIMSG_OBJECT_MOVEMENT_STARTED };
	npw.writeUint32(this->id);
	npw.writeVector3(position);
	npw.writeVector3(destination);
	npw.writeFloat(Server::instance->timeManager.currentTime);
	npw.writeFloat(speed);
	Server::instance->sendToAll(npw);

	// Find and play movement animation
	int anim = -1;
	if (!blueprint->movementBands.empty()) {
		// take movement band with nearest natural speed
		auto mbcmp = [speed](GameObjBlueprint::MovementBand& mb1, GameObjBlueprint::MovementBand& mb2) {
			return std::abs(mb1.naturalMovementSpeed - speed) < std::abs(mb2.naturalMovementSpeed - speed);
		};
		auto it = std::min_element(blueprint->movementBands.begin(), blueprint->movementBands.end(), mbcmp);
		auto& mb = *it;
		anim = mb.defaultAnim;
		auto& gs = Server::instance->gameSet;
		SrvScriptContext ctx{ Server::instance, this };
		for (auto& p : mb.onEquAnims) {
			if (gs->equations[p.first]->booleval(&ctx)) {
				anim = p.second;
			}
		}
	}
	if (anim == -1 || blueprint->subtypes[subtype].appearances[appearance].animations.count(anim) == 0)
		anim = Server::instance->gameSet->animations.names["Move"];
	setAnimation(anim);
}

void ServerGameObject::stopMovement()
{
	movement.stopMovement();
	currentSpeed = 0.0f;
	NetPacketWriter npw{ NETCLIMSG_OBJECT_MOVEMENT_STOPPED };
	npw.writeUint32(this->id);
	Server::instance->sendToAll(npw);
	setAnimation(0); // default animation
}

void ServerGameObject::sendEvent(int evt, ServerGameObject * sender)
{
	// Problem: reaction can be executed twice if it is in both intrinsics and individuals, but is it worth checking that?
	SrvScriptContext ctx(Server::instance, this);
	auto _ = ctx.change(ctx.packageSender, sender);
	const auto ircopy = individualReactions;
	for (Reaction* r : ircopy)
		if (r->canBeTriggeredBy(evt, this, sender))
			r->actions.run(&ctx);
	for (Reaction *r : blueprint->intrinsicReactions)
		if (r->canBeTriggeredBy(evt, this, sender))
			r->actions.run(&ctx);
}

void ServerGameObject::associateObject(int category, ServerGameObject * associated)
{
	assert(this && associated);
	this->associates[category].insert(associated);
	associated->associators[category].insert(this);
}

void ServerGameObject::dissociateObject(int category, ServerGameObject * associated)
{
	assert(this && associated);
	this->associates[category].erase(associated);
	associated->associators[category].erase(this);
}

void ServerGameObject::clearAssociates(int category)
{
	assert(this);
	for (auto &obj : associates[category])
		obj->associators[category].erase(this);
	associates[category].clear();
}

void ServerGameObject::convertTo(GameObjBlueprint * postbp)
{
	// remove from parent's children
	auto &vec = parent->children.at(blueprint->getFullId());
	vec.erase(std::find(vec.begin(), vec.end(), this));
	// add it back at the correct blueprint key
	parent->children[postbp->getFullId()].push_back(this);
	// backup of previous blueprint
	GameObjBlueprint* prevbp = blueprint;
	// now converted!
	blueprint = postbp;
	// inform the clients
	NetPacketWriter npw{ NETCLIMSG_OBJECT_CONVERTED };
	npw.writeUint32(this->id);
	npw.writeUint32(postbp->getFullId());
	Server::instance->sendToAll(npw);
	// change subtype to matching one
	int postsubtype = postbp->subtypeNames.getIndex(prevbp->subtypeNames.getString(this->subtype));
	this->setSubtypeAndAppearance((postsubtype != -1) ? postsubtype : 0, 0); // TODO: random subtype as fallback?
	// update sight range
	this->updateSightRange();
	// update footprint
	this->updateOccupiedTiles(this->position, this->orientation, this->position, this->orientation);
}

void ServerGameObject::setScale(const Vector3& scale)
{
	this->scale = scale;
	NetPacketWriter npw{ NETCLIMSG_OBJECT_SCALE_SET };
	npw.writeUint32(this->id);
	npw.writeVector3(scale);
	Server::instance->sendToAll(npw);
}

void ServerGameObject::terminate()
{
	// Termination depends on the object class
	SrvGORef ref = this;
	flags |= fTerminated;
	sendEvent(Tags::PDEVENT_ON_TERMINATION);
	if (!ref) return;
	sendEvent(Tags::PDEVENT_ON_DESTRUCTION);
	if (!ref) return;
	if (blueprint->bpClass != Tags::GAMEOBJCLASS_CHARACTER)
		Server::instance->deleteObject(this);
}

void ServerGameObject::updateFlags(int value)
{
	flags = value;
	NetPacketWriter npw{ NETCLIMSG_OBJECT_FLAGS_SET };
	npw.writeUint32(this->id);
	npw.writeUint8(flags);
	Server::instance->sendToAll(npw);
}

void ServerGameObject::disable()
{
	disableCount++;
}

void ServerGameObject::enable()
{
	if (disableCount > 0)
		disableCount--;
}

void ServerGameObject::setIndexedItem(int item, int index, float value)
{
	indexedItems[{item, index}] = value;
	// I don't think there is use by the client for indexed items, so no need to send a packet for now
}

void ServerGameObject::startTrajectory(const Vector3& initPos, const Vector3& initVel, float startTime)
{
	trajectory.start(initPos, initVel, startTime);
	NetPacketWriter npw{ NETCLIMSG_OBJECT_TRAJECTORY_STARTED };
	npw.writeValues(this->id, initPos, initVel, startTime);
	Server::instance->sendToAll(npw);
}

void ServerGameObject::playSoundAtObject(int soundTag, ServerGameObject* target)
{
	NetPacketWriter npw{ NETCLIMSG_SOUND_AT_OBJECT };
	uint8_t randval = (uint8_t)rand() & 255;
	npw.writeValues(this->id, soundTag, target->id, randval);
	if (target->blueprint->bpClass == Tags::GAMEOBJCLASS_PLAYER)
		Server::instance->sendTo(target, npw);
	else
		Server::instance->sendToAll(npw);
}

void ServerGameObject::playSoundAtPosition(int soundTag, const Vector3& target)
{
	NetPacketWriter npw{ NETCLIMSG_SOUND_AT_POSITION };
	uint8_t randval = (uint8_t)rand() & 255;
	npw.writeValues(this->id, soundTag, target, randval);
	Server::instance->sendToAll(npw);
}

void ServerGameObject::playSpecialEffectAt(int sfxTag, const Vector3& position)
{
	NetPacketWriter npw{ NETCLIMSG_SPECIAL_EFFECT_PLAYED_AT_POS };
	npw.writeValues(this->id, sfxTag, position);
	Server::instance->sendToAll(npw);
}

void ServerGameObject::playSpecialEffectBetween(int sfxTag, const Vector3& position1, const Vector3& position2)
{
	NetPacketWriter npw{ NETCLIMSG_SPECIAL_EFFECT_PLAYED_BETWEEN };
	npw.writeValues(this->id, sfxTag, position1, position2);
	Server::instance->sendToAll(npw);
}

void ServerGameObject::playAttachedSpecialEffect(int sfxTag, ServerGameObject* target)
{
	NetPacketWriter npw{ NETCLIMSG_ATTACHED_SPECIAL_EFFECT_PLAYED };
	npw.writeValues(this->id, sfxTag, target->id);
	Server::instance->sendToAll(npw);
}

void ServerGameObject::setName(const std::string& name)
{
	this->name = name;
	NetPacketWriter npw{ NETCLIMSG_OBJECT_NAME_SET };
	npw.writeUint32(this->id);
	npw.writeUint8((uint8_t)name.size());
	npw.writeString(name);
	Server::instance->sendToAll(npw);
}

void ServerGameObject::reportCurrentOrder(OrderBlueprint* orderBp)
{
	int orderIndex = orderBp ? Server::instance->gameSet->orders.getIndex(orderBp) : -1;
	if (reportedCurrentOrder == orderIndex)
		return;
	reportedCurrentOrder = orderIndex;
	NetPacketWriter npw{ NETCLIMSG_CURRENT_ORDER_REPORT };
	npw.writeUint32(id);
	npw.writeUint32(reportedCurrentOrder);
	Server::instance->sendToAll(npw);
}

void ServerGameObject::attachLoopingSpecialEffect(int sfxTag, const Vector3& position)
{
	NetPacketWriter npw{ NETCLIMSG_LOOPING_SPECIAL_EFFECT_ATTACHED };
	npw.writeValues(this->id, sfxTag, position);
	Server::instance->sendToAll(npw);
}

void ServerGameObject::detachLoopingSpecialEffect(int sfxTag)
{
	NetPacketWriter npw{ NETCLIMSG_LOOPING_SPECIAL_EFFECT_DETACHED };
	npw.writeValues(this->id, sfxTag);
	Server::instance->sendToAll(npw);
}

void ServerGameObject::updatePosition(const Vector3 & newposition, bool events)
{
	Server *server = Server::instance;
	Vector3 oldposition = position;
	position = newposition;
	if (server->tiles) {
		int trnNumX, trnNumZ;
		std::tie(trnNumX, trnNumZ) = server->terrain->getNumPlayableTiles();
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
				auto &vec = server->tiles[prevtileIndex].objList;
				vec.erase(std::find(vec.begin(), vec.end(), this));
			}
			if (newtileIndex != -1) {
				server->tiles[newtileIndex].objList.push_back(this);
				if (events) {
					for (auto& no : server->tiles[newtileIndex].objList) {
						if (no)
							no->sendEvent(Tags::PDEVENT_ON_SHARE_TILE, this);
					}
				}
			}
			if (events && prevtileIndex != -1 && newtileIndex != -1 && blueprint->bpClass == Tags::GAMEOBJCLASS_CHARACTER) {
				if (server->tiles[prevtileIndex].zone != server->tiles[newtileIndex].zone) {
					if (ServerGameObject* leavingZone = server->tiles[prevtileIndex].zone.get())
						leavingZone->sendEvent(Tags::PDEVENT_ON_OBJECT_EXITS, this);
					if (ServerGameObject* enteringZone = server->tiles[newtileIndex].zone.get())
						enteringZone->sendEvent(Tags::PDEVENT_ON_OBJECT_ENTERS, this);
				}
			}
			updateOccupiedTiles(oldposition, orientation, newposition, orientation);
		}
	}
}

void ServerGameObject::lookForSightRangeEvents()
{
	if (!blueprint->receiveSightRangeEvents)
		return;
	if (blueprint->sightRangeEquation == -1) {
		printf("receiveSightRangeEvents==true but sightRangeEquation not defined\n%s\n", blueprint->getFullName().c_str());
		return;
	}
	SrvScriptContext ctx{ Server::instance, this };
	if (blueprint->shouldProcessSightRange && !blueprint->shouldProcessSightRange->booleval(&ctx))
		return;
	float dist = Server::instance->gameSet->equations[blueprint->sightRangeEquation]->eval(&ctx);
	if (dist <= 0.0f)
		return;
	std::unordered_set<SrvGORef> objfound;
	NNSearch<Server, ServerGameObject> search;
	search.start(Server::instance, this->position, dist * 5.0f);
	while (ServerGameObject* nobj = search.next()) {
		if (!nobj->blueprint->generateSightRangeEvents)
			continue;
		objfound.insert(nobj);
		if (seenObjects.count(nobj) == 0) {
			this->sendEvent(Tags::PDEVENT_ON_SEEING_OBJECT, nobj);
		}
	}
	for (auto& prevSeenObj : seenObjects)
		if (prevSeenObj)
			if (objfound.count(prevSeenObj) == 0)
				this->sendEvent(Tags::PDEVENT_ON_STOP_SEEING_OBJECT, prevSeenObj);
	seenObjects = std::move(objfound);
}

void ServerGameObject::addZoneTile(int tx, int tz)
{
	this->zoneTiles.emplace(tx, tz);
	Server* server = Server::instance;
	auto areaSize = server->terrain->getNumPlayableTiles();
	int tileIndex = tz * areaSize.first + tx;
	server->tiles[tileIndex].zone = this;
	//auto& zl = server->tiles[tileIndex].zoneList;
	//if (std::find(zl.begin(), zl.end(), this) == zl.end())
	//	zl.emplace_back(this);
}

void ServerGameObject::updateSightRange()
{
	if (blueprint->sightRangeEquation != -1) {
		SrvScriptContext ctx{ Server::instance, this };
		setItem(Tags::PDITEM_ACTUAL_SIGHT_RANGE, Server::instance->gameSet->equations[blueprint->sightRangeEquation]->eval(&ctx));
	}
}

void ServerGameObject::updateOccupiedTiles(const Vector3& oldposition, const Vector3& oldorientation, const Vector3& newposition, const Vector3& neworientation)
{
	Server* server = Server::instance;
	if (!server->tiles) return;
	int trnNumX, trnNumZ;
	std::tie(trnNumX, trnNumZ) = server->terrain->getNumPlayableTiles();
	if (blueprint->bpClass == Tags::GAMEOBJCLASS_BUILDING && blueprint->footprint) {
		// free tiles from old position
		auto rotOrigin = blueprint->footprint->rotateOrigin(oldorientation.y);
		int ox = (int)((oldposition.x - rotOrigin.first) / 5.0f);
		int oz = (int)((oldposition.z - rotOrigin.second) / 5.0f);
		for (auto& to : blueprint->footprint->tiles) {
			if (true /*!to.mode*/) {
				auto ro = to.rotate(oldorientation.y);
				int px = ox + ro.first, pz = oz + ro.second;
				if (px >= 0 && px < trnNumX && pz >= 0 && pz < trnNumZ) {
					auto& tile = server->tiles[pz * trnNumX + px];
					if (tile.building == this)
						tile.building = nullptr;
				}
			}
		}
		// take tiles from new position
		rotOrigin = blueprint->footprint->rotateOrigin(neworientation.y);
		ox = (int)((newposition.x - rotOrigin.first) / 5.0f);
		oz = (int)((newposition.z - rotOrigin.second) / 5.0f);
		for (auto& to : blueprint->footprint->tiles) {
			if (true /*!to.mode*/) {
				auto ro = to.rotate(neworientation.y);
				int px = ox + ro.first, pz = oz + ro.second;
				if (px >= 0 && px < trnNumX && pz >= 0 && pz < trnNumZ) {
					auto& tile = server->tiles[pz * trnNumX + px];
					if (!tile.building || tile.buildingPassable) {
						tile.building = this;
						tile.buildingPassable = to.mode;
					}
				}
			}
		}
	}
}

void ServerGameObject::removeIfNotReferenced()
{
	if (blueprint->removeWhenNotReferenced) {
		bool cond = std::all_of(referencers.begin(), referencers.end(), [this](const SrvGORef& ref) {
			if (ref)
				for (auto& order : ref->orderConfig.orders)
					if (order.getCurrentTask()->target == this)
						return false;
			return true;
		});
		cond = cond && std::all_of(associators.begin(), associators.end(), [](const auto& set) {return set.second.empty(); });
		if (cond)
			Server::instance->deleteObject(this);
	}
}

float ServerGameObject::computeSpeed()
{
	float speed = 5.0f;
	if (blueprint->movementSpeedEquation != -1) {
		SrvScriptContext ctx{ Server::instance, this };
		speed = Server::instance->gameSet->equations[blueprint->movementSpeedEquation]->eval(&ctx);
	}
	return speed;
}

void Server::sendToAll(const NetPacket & packet)
{
	for (NetLink *cli : clientLinks)
		cli->send(packet);
}

void Server::sendToAll(const NetPacketWriter & packet)
{
	for (NetLink *cli : clientLinks)
		cli->send(packet);
}

void Server::sendTo(ServerGameObject* player, const NetPacketWriter& packet)
{
	assert(player->blueprint->bpClass == Tags::GAMEOBJCLASS_PLAYER);
	if (player->clientIndex == -1) {
		printf("WARNING: Sending packet to player %i that has no assigned client!\n", player->id);
		return;
	}
	clientLinks[player->clientIndex]->send(packet);
}

void Server::syncTime()
{
	NetPacketWriter msg(NETCLIMSG_TIME_SYNC);
	msg.writeUint32(timeManager.psCurrentTime);
	msg.writeUint8(timeManager.paused);
	sendToAll(msg);
}

void Server::setDiplomaticStatus(ServerGameObject * a, ServerGameObject * b, int status)
{
	if (getDiplomaticStatus(a, b) != status) {
		int &ref = (a->id <= b->id) ? diplomaticStatuses[{ a->id, b->id }] : diplomaticStatuses[{ b->id, a->id }];
		ref = status;
		NetPacketWriter msg{ NETCLIMSG_DIPLOMATIC_STATUS_SET };
		msg.writeUint32(a->id);
		msg.writeUint32(b->id);
		msg.writeUint8(status);
		sendToAll(msg);
	}
}

void Server::showGameTextWindow(ServerGameObject* player, int gtwIndex)
{
	NetPacketWriter msg{ NETCLIMSG_SHOW_GAME_TEXT_WINDOW };
	msg.writeUint32(gtwIndex);
	sendTo(player, msg);
}

void Server::hideGameTextWindow(ServerGameObject* player, int gtwIndex)
{
	NetPacketWriter msg{ NETCLIMSG_HIDE_GAME_TEXT_WINDOW };
	msg.writeUint32(gtwIndex);
	sendTo(player, msg);
}

void Server::hideCurrentGameTextWindow(ServerGameObject* player)
{
	NetPacketWriter msg{ NETCLIMSG_HIDE_CURRENT_GAME_TEXT_WINDOW };
	sendTo(player, msg);
}

void Server::snapCameraPosition(ServerGameObject* player, const Vector3& position, const Vector3& orientation)
{
	NetPacketWriter msg{ NETCLIMSG_SNAP_CAMERA_POSITION };
	msg.writeValues(position, orientation);
	sendTo(player, msg);
}

void Server::storeCameraPosition(ServerGameObject* player)
{
	NetPacketWriter msg{ NETCLIMSG_STORE_CAMERA_POSITION };
	sendTo(player, msg);
}

void Server::restoreCameraPosition(ServerGameObject* player)
{
	NetPacketWriter msg{ NETCLIMSG_RESTORE_CAMERA_POSITION };
	sendTo(player, msg);
}

void Server::playCameraPath(ServerGameObject* player, int camPathIndex)
{
	NetPacketWriter msg{ NETCLIMSG_PLAY_CAMERA_PATH };
	msg.writeUint32(camPathIndex);
	sendTo(player, msg);
}

void Server::stopCameraPath(ServerGameObject* player, bool skipActions)
{
	NetPacketWriter msg{ NETCLIMSG_STOP_CAMERA_PATH };
	msg.writeUint8(skipActions);
	sendTo(player, msg);
}

void Server::skipCameraPath(ServerGameObject* player)
{
	NetPacketWriter msg{ NETCLIMSG_SKIP_CAMERA_PATH };
	sendTo(player, msg);
}

void Server::setGameSpeed(float nextSpeed)
{
	timeManager.setSpeed(nextSpeed);
	NetPacketWriter msg{ NETCLIMSG_GAME_SPEED_CHANGED };
	msg.writeFloat(nextSpeed);
	sendToAll(msg);
	syncTime();
}

void Server::setClientPlayerControl(int clientIndex, uint32_t playerObjId)
{
	clientPlayerObjects[clientIndex] = playerObjId;
	NetPacketWriter msg{ NETCLIMSG_CONTROL_PLAYER };
	msg.writeUint32(playerObjId);
	clientLinks[clientIndex]->send(msg);
}

void Server::playMusic(ServerGameObject* player, int musicTag)
{
	NetPacketWriter msg{ NETCLIMSG_MUSIC_CHANGED };
	msg.writeValues(musicTag);
	sendTo(player, msg);
	player->isMusicPlaying = true;
}

void Server::addClient(NetLink* link)
{
	clientLinks.push_back(link);
	clientPlayerObjects.emplace_back();
	clientInfos.emplace_back();
}

void Server::removeClient(NetLink* link)
{
	auto it = std::find(clientLinks.begin(), clientLinks.end(), link);
	assert(it != clientLinks.end());
	size_t clientIndex = it - clientLinks.begin();
	if (ServerGameObject* player = clientPlayerObjects[clientIndex])
		player->clientIndex = -1;
	clientLinks.erase(it);
	clientPlayerObjects.erase(clientPlayerObjects.begin() + clientIndex);
	clientInfos.erase(clientInfos.begin() + clientIndex);
}

void Server::tick()
{
	timeManager.tick();

	auto it = delayedSequences.begin();
	for (; it != delayedSequences.end(); it++) {
		if (it->first > timeManager.currentTime) {
			break;
		}
		DelayedSequence &ds = it->second;
		for (SrvGORef &obj : ds.selfs) {
			if (obj) {
				SrvScriptContext ctx(this, obj);
				ds.actionSequence->run(&ctx);
			}
		}
	}
	delayedSequences.erase(delayedSequences.begin(), it);

	for (size_t i = 0; i < overPeriodSequences.size(); i++) {
		OverPeriodSequence& ops = overPeriodSequences[i];
		int predictedExec = (timeManager.currentTime - ops.startTime) * ops.numTotalExecutions / ops.period;
		if (predictedExec > ops.numTotalExecutions) predictedExec = ops.numTotalExecutions;
		SrvScriptContext ctx(this);
		auto _ = ctx.change(ctx.sequenceExecutor, ops.executor);
		for (; ops.numExecutionsDone < predictedExec; ops.numExecutionsDone++) {
			if (ServerGameObject* obj = ops.remainingObjects.back().get()) {
				auto _2 = ctx.changeSelf(obj);
				ops.actionSequence->run(&ctx);
			}
			ops.remainingObjects.pop_back();
		}
		if (ops.numExecutionsDone >= ops.numTotalExecutions) {
			std::swap(ops, overPeriodSequences.back());
			overPeriodSequences.pop_back();
			i--;
		}
	}
	for (size_t i = 0; i < repeatOverPeriodSequences.size(); i++) {
		OverPeriodSequence& ops = repeatOverPeriodSequences[i];
		int predictedExec = (timeManager.currentTime - ops.startTime) * ops.numTotalExecutions / ops.period;
		if (predictedExec > ops.numTotalExecutions) predictedExec = ops.numTotalExecutions;
		SrvScriptContext ctx(this);
		auto _ = ctx.change(ctx.sequenceExecutor, ops.executor);
		for (; ops.numExecutionsDone < predictedExec; ops.numExecutionsDone++) {
			for (auto& ref : ops.remainingObjects) {
				if (ServerGameObject* obj = ref.get()) {
					auto _2 = ctx.changeSelf(obj);
					ops.actionSequence->run(&ctx);
				}
			}
		}
		if (ops.numExecutionsDone >= ops.numTotalExecutions) {
			std::swap(ops, repeatOverPeriodSequences.back());
			repeatOverPeriodSequences.pop_back();
			i--;
		}
	}

	static std::vector<SrvGORef> toprocess;
	toprocess.clear();
	const auto processObjOrders = [this](ServerGameObject *obj, auto &func) -> void {
		if (!obj->orderConfig.orders.empty() || obj->blueprint->receiveSightRangeEvents || obj->blueprint->removeWhenNotReferenced)
			toprocess.emplace_back(obj);
		if (obj->blueprint->bpClass == Tags::GAMEOBJCLASS_ARMY) {
			Vector3 avg(0,0,0);
			size_t cnt = 0;
			for (auto& childtype : obj->children) {
				for (CommonGameObject* child : childtype.second) {
					avg += ((ServerGameObject*)child)->position;
					cnt += 1;
				}
			}
			if (cnt > 0u) {
				avg /= cnt;
				obj->updatePosition(avg, false);
			}
		}
		if (obj->blueprint->bpClass == Tags::GAMEOBJCLASS_PLAYER) {
			obj->aiController.update();
		}
		for (auto &childtype : obj->children) {
			for (CommonGameObject* child : childtype.second)
				func((ServerGameObject*)child, func);
		}
	};
	if(level)
		processObjOrders(level, processObjOrders);
	for (const SrvGORef& ref : toprocess) {
		ServerGameObject* obj = ref.get();
		if (!obj) continue;
		obj->orderConfig.process();
		if (!ref) continue;
		if (obj->movement.isMoving()) {
			auto newpos = obj->movement.getNewPosition(timeManager.currentTime);
			newpos.y = terrain->getHeightEx(obj->position.x, obj->position.z, obj->blueprint->canWalkOnWater());
			obj->updatePosition(newpos, true);
			if (!ref) continue;
			Vector3 dir = obj->movement.getDirection();
			obj->orientation.y = atan2f(dir.x, -dir.z);
			// restart movement is speed changes
			float speed = obj->computeSpeed();
			if (speed != obj->currentSpeed) {
				obj->startMovement(obj->movement.getDestination());
			}
			// pf
			obj->movementController.updateMovement();
		}
		else if (obj->trajectory.isMoving()) {
			obj->updatePosition(obj->trajectory.getPosition(timeManager.currentTime));
			obj->orientation = obj->trajectory.getRotationAngles(timeManager.currentTime);
		}
		if (obj->blueprint->receiveSightRangeEvents) {
			obj->lookForSightRangeEvents();
			if (!ref) continue;
		}
		if (obj->blueprint->removeWhenNotReferenced)
			obj->removeIfNotReferenced();
	}

	time_t curtime = time(NULL);
	if (curtime - lastSync >= 1) {
		lastSync = curtime;
		syncTime();
	}

	int clientIndex = -1;
	for (NetLink *cli : clientLinks) {
		ServerGameObject* player = clientPlayerObjects[++clientIndex];
		int pcnt = 20;
		while (cli->available() && (pcnt--)) { // DDoS !!!
			NetPacket packet = cli->receive();
			BinaryReader br(packet.data.c_str());
			uint8_t type = br.readUint8();

			switch (type) {
			case NETSRVMSG_TEST: {
				std::string msg = br.readStringZ();
				chatMessages.push_back(msg);
				printf("Server got message: %s\n", msg.c_str());

				if (msg.size() >= 2 && msg[0] == '!') {
					GSFileParser gsf = GSFileParser(msg.c_str() + 1);
					Action* act = ReadAction(gsf, *gameSet);
					SrvScriptContext ctx(this, player);
					act->run(&ctx);
					delete act;
				}

				NetPacketWriter echo(NETCLIMSG_TEST);
				echo.writeStringZ(msg);
				sendToAll(echo);
				break;
			}
			case NETSRVMSG_COMMAND: {
				int cmdid = br.readUint32();
				int objid = br.readUint32();
				int mode = br.readUint8();
				int targetid = br.readUint32();
				Vector3 destination = br.readVector3();
				gameSet->commands[cmdid].execute(findObject(objid), findObject(targetid), mode, destination);
				break;
			}
			case NETSRVMSG_PAUSE: {
				uint8_t newPaused = br.readUint8();
				if (newPaused)
					timeManager.pause();
				else
					timeManager.unpause();
				syncTime();
				break;
			}
			case NETSRVMSG_STAMPDOWN: {
				uint32_t bpid = br.readUint32();
				uint32_t playerid = br.readUint32();
				Vector3 pos = br.readVector3();
				bool sendEvent = br.readUint8();
				ServerGameObject *obj = createObject(gameSet->getBlueprint(bpid));
				obj->setParent(findObject(playerid));
				obj->setPosition(pos);
				if (sendEvent)
					obj->sendEvent(Tags::PDEVENT_ON_STAMPDOWN);
				clientInfos[clientIndex].lastStampdown = obj;
				break;
			}
			case NETSRVMSG_START_LEVEL: {
				auto walk = [](ServerGameObject* obj, auto rec) -> void {
					obj->sendEvent(Tags::PDEVENT_ON_LEVEL_START);
					for (auto& t : obj->children) {
						for (CommonGameObject* child : t.second)
							rec((ServerGameObject*)child, rec);
					}
				};
				walk(level, walk);
				break;
			}
			case NETSRVMSG_GAME_TEXT_WINDOW_BUTTON_CLICKED: {
				int gtwid = br.readUint32();
				int button = br.readUint32();
				SrvScriptContext ctx(this, player);
				gameSet->gameTextWindows[gtwid].buttons[button].onClickSequence.run(&ctx); // TODO: Correct player object
				break;
			}
			case NETSRVMSG_CAMERA_PATH_ENDED: {
				int camPathIndex = br.readUint32();
				SrvScriptContext ctx(this, player);
				gameSet->cameraPaths[camPathIndex].postPlaySequence.run(&ctx);
				break;
			}
			case NETSRVMSG_CHANGE_GAME_SPEED: {
				setGameSpeed(br.readFloat());
				break;
			}
			case NETSRVMSG_MUSIC_COMPLETED: {
				player->isMusicPlaying = false;
				break;
			}
			case NETSRVMSG_TERMINATE_OBJECT: {
				if (ServerGameObject* obj = findObject(br.readUint32())) {
					deleteObject(obj);
				}
				break;
			}
			case NETSRVMSG_BUILD_LAST_STAMPDOWNED_OBJECT: {
				uint32_t objid = br.readUint32();
				if (ServerGameObject* obj = findObject(objid)) {
					int assignmentMode = br.readUint32();
					int cmdid = gameSet->commands.names.getIndex("Build");
					gameSet->commands[cmdid].execute(obj, clientInfos[clientIndex].lastStampdown, assignmentMode);
				}
				break;
			}
			}
		}
	}

	// Free up deleted objects
	auto* obj = objToDelete;
	while (obj) {
		auto* next = obj->nextDeleted;
		destroyObject(obj);
		obj = next;
	}
	objToDelete = nullptr;
	objToDeleteLast = nullptr;

	++g_diag_serverTicks;
}
