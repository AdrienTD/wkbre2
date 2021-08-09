#pragma once

#include <cstdint>
#include <map>
#include <vector>
#include "common.h"
#include "TimeManager.h"
#include "Order.h"
#include "GameObjectRef.h"
#include <unordered_set>
#include <unordered_map>

struct GameSet;
struct GSFileParser;
struct GameObjBlueprint;
struct NetLink;
struct NetPacket;
struct NetPacketWriter;
struct Terrain;
struct ActionSequence;
struct Server;
struct Reaction;

struct ServerGameObject : CommonGameObject<ServerGameObject> {
	using Program = Server;

	bool deleted = false; ServerGameObject* nextDeleted = nullptr;

	OrderConfiguration orderConfig;
	std::unordered_set<Reaction*> individualReactions;
	std::unordered_map<int, std::unordered_set<SrvGORef>> associates, associators;
	int tileIndex = -1;
	std::vector<SrvGORef> referencers;
	std::unordered_set<SrvGORef> seenObjects;

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
	void sendEvent(int evt, ServerGameObject *sender = nullptr);
	void associateObject(int category, ServerGameObject *associated);
	void dissociateObject(int category, ServerGameObject *associated);
	void clearAssociates(int category);
	void convertTo(GameObjBlueprint *postbp);
	void setScale(const Vector3& scale);
	void terminate();
	void updateFlags(int value);
	void disable();
	void enable();
	void setIndexedItem(int item, int index, float value);
	void startTrajectory(const Vector3& initPos, const Vector3& initVel, float startTime);

	void updatePosition(const Vector3 &newposition, bool events = false);
	void lookForSightRangeEvents();
};

struct Server : CommonGameState<Server, ServerGameObject>
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
	std::vector<SrvGORef> *tileObjList = nullptr;

	struct DelayedSequence {
		ActionSequence* actionSequence;
		SrvGORef executor;
		std::vector<SrvGORef> selfs;
	};
	struct OverPeriodSequence {
		ActionSequence* actionSequence;
		SrvGORef executor;
		float startTime, period;
		int numTotalExecutions, numExecutionsDone;
		std::vector<SrvGORef> remainingObjects;
	};
	std::multimap<game_time_t, DelayedSequence> delayedSequences;
	std::vector<OverPeriodSequence> overPeriodSequences;
	std::vector<OverPeriodSequence> repeatOverPeriodSequences;

	std::vector<std::string> chatMessages;

	std::vector<NetLink*> clientLinks;

	time_t lastSync;

	std::vector<std::tuple<ServerGameObject*, int, int>> postAssociations;

	ServerGameObject* objToDelete = nullptr;

	Server() : gameSet(nullptr), level(nullptr), terrain(nullptr) { instance = this; }

	void loadSaveGame(const char *filename);
	ServerGameObject *createObject(GameObjBlueprint *blueprint, uint32_t id = 0);
	void deleteObject(ServerGameObject *obj);

	ServerGameObject *findObject(uint32_t id) { return idmap[id]; }

	void syncTime();

	void setDiplomaticStatus(ServerGameObject *a, ServerGameObject *b, int status);
	void showGameTextWindow(ServerGameObject* player, int gtwIndex);
	void hideGameTextWindow(ServerGameObject* player, int gtwIndex);
	void hideCurrentGameTextWindow(ServerGameObject* player);
	void storeCameraPosition(ServerGameObject* player);
	void restoreCameraPosition(ServerGameObject* player);
	void playCameraPath(ServerGameObject* player, int camPathIndex);
	void stopCameraPath(ServerGameObject* player);
	void setGameSpeed(float nextSpeed);

	void tick();

private:
	std::map<uint32_t, GameObjBlueprint*> predec;

	ServerGameObject * loadObject(GSFileParser & gsf, const std::string & clsname);
	void loadSavePredec(GSFileParser & gsf);

	void sendToAll(const NetPacket &packet);
	void sendToAll(const NetPacketWriter &packet);

	friend struct ServerGameObject;
};