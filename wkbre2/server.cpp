#include "server.h"
#include "file.h"
#include "util/GSFileParser.h"
#include "tags.h"
#include "gameset/gameset.h"
#include "network.h"
#include "terrain.h"
#include "gameset/ScriptContext.h"
#include "NNSearch.h"

Server *Server::instance = nullptr;

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
			gameSet = new GameSet(gsFileName.c_str());
			NetPacketWriter msg(NETCLIMSG_GAME_SET);
			msg.writeStringZ(gsFileName);
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
		}
		gsf.advanceLine();
	}

	free(filetext);

	for (auto &t : postAssociations)
		std::get<0>(t)->associateObject(std::get<1>(t), findObject(std::get<2>(t)));

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

	NetPacketWriter msg(NETCLIMSG_OBJECT_CREATED);
	msg.writeUint32(id);
	msg.writeUint32(blueprint->getFullId());
	msg.writeUint32(obj->subtype);
	sendToAll(msg);

	if (blueprint->sightRangeEquation != -1) {
		SrvScriptContext ctx{ this, obj };
		obj->setItem(Tags::PDITEM_ACTUAL_SIGHT_RANGE, gameSet->equations[blueprint->sightRangeEquation]->eval(&ctx));
	}

	return obj;
}

void Server::deleteObject(ServerGameObject * obj)
{
	int id = obj->id;

	if (obj->deleted) return;
	obj->deleted = true;
	obj->nextDeleted = objToDelete;
	objToDelete = obj;

	// remove associations from/to this object
	for (auto &ref : obj->associates)
		for (const SrvGORef &ass : ref.second) {
			assert(ass.get());
			ass->associators[ref.first].erase(obj);
		}
	for (auto &ref : obj->associators)
		for (const SrvGORef &ass : ref.second) {
			assert(ass.get());
			ass->associates[ref.first].erase(obj);
		}

	// remove from parent's children
	auto &vec = obj->parent->children.at(obj->blueprint->getFullId());
	vec.erase(std::find(vec.begin(), vec.end(), obj));
	//std::swap(*std::find(vec.begin(), vec.end(), obj), vec.back());
	//vec.pop_back();
	// remove from ID map
	idmap.erase(obj->id);
	// delete the object, bye!
	//delete obj;
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
			this->tileObjList = new std::vector<SrvGORef>[area.first * area.second];
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
							Task *taskptr = new Task(0, &taskBp, &order);
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
		case Tags::GAMESET_MISSILE:
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

	NetPacketWriter msg(NETCLIMSG_OBJECT_POSITION_SET);
	msg.writeUint32(this->id);
	msg.writeVector3(position);
	Server::instance->sendToAll(msg);
}

void ServerGameObject::setOrientation(const Vector3 & orientation)
{
	this->orientation = orientation;

	NetPacketWriter msg(NETCLIMSG_OBJECT_ORIENTATION_SET);
	msg.writeUint32(this->id);
	msg.writeVector3(orientation);
	Server::instance->sendToAll(msg);
}

void ServerGameObject::setSubtypeAndAppearance(int new_subtype, int new_appearance)
{
	if (this->blueprint->subtypes[new_subtype].appearances.count(new_appearance) == 0)
		new_appearance = this->appearance;

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

void ServerGameObject::setAnimation(int animationIndex)
{
	this->animationIndex = animationIndex;
	this->animationVariant = animationVariant;
	this->animStartTime = Server::instance->timeManager.currentTime;
	NetPacketWriter msg(NETCLIMSG_OBJECT_ANIM_SET);
	msg.writeUint32(this->id);
	msg.writeUint32(this->animationIndex);
	msg.writeUint32(this->animationVariant);
	msg.writeFloat(this->animStartTime);
	Server::instance->sendToAll(msg);
}

void ServerGameObject::startMovement(const Vector3 & destination)
{
	constexpr float speed = 5.0f;
	movement.startMovement(position, destination, Server::instance->timeManager.currentTime, speed);
	NetPacketWriter npw{ NETCLIMSG_OBJECT_MOVEMENT_STARTED };
	npw.writeUint32(this->id);
	npw.writeVector3(position);
	npw.writeVector3(destination);
	npw.writeFloat(Server::instance->timeManager.currentTime);
	npw.writeFloat(speed);
	Server::instance->sendToAll(npw);
	setAnimation(Server::instance->gameSet->animations.names["Move"]);
}

void ServerGameObject::stopMovement()
{
	movement.stopMovement();
	NetPacketWriter npw{ NETCLIMSG_OBJECT_MOVEMENT_STOPPED };
	npw.writeUint32(this->id);
	Server::instance->sendToAll(npw);
	setAnimation(0); // default animation
}

void ServerGameObject::sendEvent(int evt, ServerGameObject * sender)
{
	// Problem: reaction can be executed twice if it is in both intrinsics and individuals, but is it worth checking that?
	SrvScriptContext ctx(Server::instance, this);
	auto _ = ctx.packageSender.change(sender);
	for (Reaction *r : blueprint->intrinsicReactions)
		if (r->canBeTriggeredBy(evt, this, sender))
			r->actions.run(&ctx);
	const auto ircopy = individualReactions;
	for (Reaction *r : ircopy)
		if (r->canBeTriggeredBy(evt, this, sender))
			r->actions.run(&ctx);
}

void ServerGameObject::associateObject(int category, ServerGameObject * associated)
{
	this->associates[category].insert(associated);
	associated->associators[category].insert(this);
}

void ServerGameObject::dissociateObject(int category, ServerGameObject * associated)
{
	this->associates[category].erase(associated);
	associated->associators[category].erase(this);

}

void ServerGameObject::clearAssociates(int category)
{
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
	// now converted!
	blueprint = postbp;
	// inform the clients
	NetPacketWriter npw{ NETCLIMSG_OBJECT_CONVERTED };
	npw.writeUint32(this->id);
	npw.writeUint32(postbp->getFullId());
	Server::instance->sendToAll(npw);
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
	if (blueprint->bpClass == Tags::GAMEOBJCLASS_CHARACTER) {
		//flags |= fTerminated;
		sendEvent(Tags::PDEVENT_ON_TERMINATION);
	}
	else
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

void ServerGameObject::updatePosition(const Vector3 & newposition, bool events)
{
	Server *server = Server::instance;
	position = newposition;
	if (server->tileObjList) {
		int trnNumX, trnNumZ;
		std::tie(trnNumX, trnNumZ) = server->terrain->getNumPlayableTiles();
		int tx = static_cast<int>(position.x / 5.0f), tz = static_cast<int>(position.z / 5.0f);
		int newtileIndex;
		if (tx >= 0 && tx < trnNumX && tz >= 0 && tz < trnNumZ)
			newtileIndex = tz * trnNumX + tx;
		else
			newtileIndex = -1;
		if (newtileIndex != tileIndex) {
			if (tileIndex != -1) {
				auto &vec = server->tileObjList[tileIndex];
				vec.erase(std::find(vec.begin(), vec.end(), this));
			}
			if (newtileIndex != -1) {
				if (events) {
					for (auto& no : server->tileObjList[newtileIndex]) {
						if (no)
							no->sendEvent(Tags::PDEVENT_ON_SHARE_TILE, this);
					}
				}
				server->tileObjList[newtileIndex].push_back(this);
			}
			tileIndex = newtileIndex;
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
	NNSearch search;
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
	sendToAll(msg); // TODO: Only send to one player's client, not all clients
}

void Server::hideGameTextWindow(ServerGameObject* player, int gtwIndex)
{
	NetPacketWriter msg{ NETCLIMSG_HIDE_GAME_TEXT_WINDOW };
	msg.writeUint32(gtwIndex);
	sendToAll(msg); // TODO: Only send to one player's client, not all clients
}

void Server::hideCurrentGameTextWindow(ServerGameObject* player)
{
	NetPacketWriter msg{ NETCLIMSG_HIDE_CURRENT_GAME_TEXT_WINDOW };
	sendToAll(msg); // TODO: Only send to one player's client, not all clients
}

void Server::storeCameraPosition(ServerGameObject* player)
{
	NetPacketWriter msg{ NETCLIMSG_STORE_CAMERA_POSITION };
	sendToAll(msg);
}

void Server::restoreCameraPosition(ServerGameObject* player)
{
	NetPacketWriter msg{ NETCLIMSG_RESTORE_CAMERA_POSITION };
	sendToAll(msg);
}

void Server::playCameraPath(ServerGameObject* player, int camPathIndex)
{
	NetPacketWriter msg{ NETCLIMSG_PLAY_CAMERA_PATH };
	msg.writeUint32(camPathIndex);
	sendToAll(msg);
}

void Server::stopCameraPath(ServerGameObject* player)
{
	NetPacketWriter msg{ NETCLIMSG_STOP_CAMERA_PATH };
	sendToAll(msg);
}

void Server::setGameSpeed(float nextSpeed)
{
	timeManager.setSpeed(nextSpeed);
	NetPacketWriter msg{ NETCLIMSG_GAME_SPEED_CHANGED };
	msg.writeFloat(nextSpeed);
	sendToAll(msg);
	syncTime();
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
		auto _ = ctx.sequenceExecutor.change(ops.executor);
		for (; ops.numExecutionsDone < predictedExec; ops.numExecutionsDone++) {
			if (ServerGameObject* obj = ops.remainingObjects.back().get()) {
				auto _2 = ctx.self.change(obj);
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
		auto _ = ctx.sequenceExecutor.change(ops.executor);
		for (; ops.numExecutionsDone < predictedExec; ops.numExecutionsDone++) {
			for (auto& ref : ops.remainingObjects) {
				if (ServerGameObject* obj = ref.get()) {
					auto _2 = ctx.self.change(obj);
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
		if (!obj->orderConfig.orders.empty() || obj->blueprint->receiveSightRangeEvents)
			toprocess.emplace_back(obj);
		for (auto &childtype : obj->children) {
			for (ServerGameObject *child : childtype.second)
				func(child, func);
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
			newpos.y = terrain->getHeight(obj->position.x, obj->position.z);
			obj->updatePosition(newpos, true);
			if (!ref) continue;
			Vector3 dir = obj->movement.getDirection();
			obj->orientation.y = atan2f(dir.x, -dir.z);
		}
		else if (obj->trajectory.isMoving()) {
			obj->updatePosition(obj->trajectory.getPosition(timeManager.currentTime));
		}
		if (obj->blueprint->receiveSightRangeEvents) {
			obj->lookForSightRangeEvents();
			if (!ref) continue;
		}
	}

	time_t curtime = time(NULL);
	if (curtime - lastSync >= 1) {
		lastSync = curtime;
		syncTime();
	}

	for (NetLink *cli : clientLinks) {
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
					SrvScriptContext ctx(this, findObject(1027));
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
				break;
			}
			case NETSRVMSG_START_LEVEL: {
				auto walk = [](ServerGameObject* obj, auto rec) -> void {
					obj->sendEvent(Tags::PDEVENT_ON_LEVEL_START);
					for (auto& t : obj->children) {
						for (ServerGameObject* child : t.second)
							rec(child, rec);
					}
				};
				walk(level, walk);
				break;
			}
			case NETSRVMSG_GAME_TEXT_WINDOW_BUTTON_CLICKED: {
				int gtwid = br.readUint32();
				int button = br.readUint32();
				SrvScriptContext ctx(this, findObject(1027));
				gameSet->gameTextWindows[gtwid].buttons[button].onClickSequence.run(&ctx); // TODO: Correct player object
				break;
			}
			case NETSRVMSG_CAMERA_PATH_ENDED: {
				int camPathIndex = br.readUint32();
				SrvScriptContext ctx(this, findObject(1027));
				gameSet->cameraPaths[camPathIndex].postPlaySequence.run(&ctx);
				break;
			}
			case NETSRVMSG_CHANGE_GAME_SPEED: {
				setGameSpeed(br.readFloat());
				break;
			}
			}
		}
	}

	// Free up deleted objects
	while (objToDelete) {
		auto next = objToDelete->nextDeleted;
		deleteObject(objToDelete);
		objToDelete = next;
	}
}
