#pragma once

#include <cstdint>
#include <map>
#include <vector>
#include "common.h"
#include "TimeManager.h"
#include "Order.h"
#include "GameObjectRef.h"
#include <unordered_set>

struct GameSet;
struct GSFileParser;
struct GameObjBlueprint;
struct NetLink;
struct NetPacket;
struct NetPacketWriter;
struct Terrain;
struct ActionSequence;
struct Server;

struct ServerGameObject : CommonGameObject<ServerGameObject> {
	using Program = Server;

	OrderConfiguration orderConfig;

	ServerGameObject(uint32_t id, GameObjBlueprint *blueprint) : CommonGameObject<ServerGameObject>(id, blueprint), orderConfig(this) {}

	void setItem(int index, float value);
	void setParent(ServerGameObject *newParent);
	void setPosition(const Vector3 &position);
	void setOrientation(const Vector3 &orientation);
	void setSubtypeAndAppearance(int new_subtype, int new_appearance);
	void setColor(int color);
	void setAnimation(int animationIndex);
	void startMovement(const Vector3 &destination);
	void stopMovement();

	//void addOrderAtBegin(int orderType);
	//void addOrderAtEnd(int orderType);
	//void removeOrder(int orderIndex);
};

struct Server
{
	using GameObject = ServerGameObject;
	static Server *instance;

	GameSet *gameSet;

	uint32_t randomSeed;
	TimeManager timeManager;
	uint32_t nextUniqueId;

	ServerGameObject *level;
	std::map<uint32_t, ServerGameObject*> idmap;
	Terrain *terrain;

	struct DelayedSequence {
		ActionSequence* actionSequence; // or ActionSequence*
		SrvGORef executor;
		std::vector<SrvGORef> selfs;
	};
	std::multimap<game_time_t, DelayedSequence> delayedSequences;

	std::map<int, std::unordered_set<SrvGORef>> aliases;

	std::vector<std::string> chatMessages;

	std::vector<NetLink*> clientLinks;

	time_t lastSync;

	Server() : gameSet(nullptr), level(nullptr), terrain(nullptr) { instance = this; }

	void loadSaveGame(const char *filename);
	ServerGameObject *createObject(GameObjBlueprint *blueprint, uint32_t id = 0);
	void deleteObject(ServerGameObject *obj);

	ServerGameObject *findObject(uint32_t id) { return idmap[id]; }

	void syncTime();

	void tick();

private:
	std::map<uint32_t, GameObjBlueprint*> predec;

	ServerGameObject * loadObject(GSFileParser & gsf, const std::string & clsname);
	void loadSavePredec(GSFileParser & gsf);

	void sendToAll(const NetPacket &packet);
	void sendToAll(const NetPacketWriter &packet);

	friend struct ServerGameObject;
};