// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

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
#include <set>
#include "MovementController.h"
#include "AIController.h"
#include "FormationController.h"

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

struct ServerGameObject : SpecificGameObject<Server, ServerGameObject> {
	using Program = Server;

	bool deleted = false; ServerGameObject* nextDeleted = nullptr;

	OrderConfiguration orderConfig;
	std::unordered_set<Reaction*> individualReactions;
	std::unordered_map<int, std::unordered_set<SrvGORef>> associates, associators;
	std::vector<SrvGORef> referencers;
	std::unordered_set<SrvGORef> seenObjects;
	std::set<std::pair<int, int>> zoneTiles;
	int clientIndex = -1;
	bool isMusicPlaying = false;
	float currentSpeed = 0.0f;
	MovementController movementController{ this };
	AIController aiController{ this };
	FormationController formationController{ this };

	ServerGameObject(uint32_t id, GameObjBlueprint *blueprint) : SpecificGameObject<Server, ServerGameObject>(id, blueprint), orderConfig(this) {}

	void setItem(int index, float value);
	void setParent(ServerGameObject *newParent);
	void setPosition(const Vector3 &position);
	void setOrientation(const Vector3 &orientation);
	void setSubtypeAndAppearance(int new_subtype, int new_appearance);
	void setColor(int color);
	void setAnimation(int animationIndex, bool isClamped = false, int synchronizedTask = -1);
	void startMovement(const Vector3 &destination);
	void stopMovement();
	void sendEvent(int evt, ServerGameObject *sender = nullptr);
	void associateObject(int category, ServerGameObject *associated);
	void dissociateObject(int category, ServerGameObject *associated);
	void clearAssociates(int category);
	void convertTo(GameObjBlueprint *postbp);
	void setScale(const Vector3& scale);
	void terminate();
	void destroy();
	void updateFlags(int value);
	void disable();
	void enable();
	void setIndexedItem(int item, int index, float value);
	void startTrajectory(const Vector3& initPos, const Vector3& initVel, float startTime);
	void playSoundAtObject(int soundTag, ServerGameObject* target);
	void playSoundAtPosition(int soundTag, const Vector3& target);
	void playSpecialEffectAt(int sfxTag, const Vector3& position);
	void playSpecialEffectBetween(int sfxTag, const Vector3& position1, const Vector3& position2);
	void playAttachedSpecialEffect(int sfxTag, ServerGameObject* target);
	void setName(const std::string& name);
	void reportCurrentOrder(OrderBlueprint* orderBp);
	void attachLoopingSpecialEffect(int sfxTag, const Vector3& position);
	void detachLoopingSpecialEffect(int sfxTag);
	void updateBuildingOrderCount(OrderBlueprint* orderBp);
	void addCityRectangle(const CityRectangle& rectangle);

	void updatePosition(const Vector3 &newposition, bool events = false);
	void lookForSightRangeEvents();
	void addZoneTile(int tx, int tz);
	//void removeZoneTile(int tx, int tz);
	void updateSightRange();
	void updateOccupiedTiles(const Vector3& oldposition, const Vector3& oldorientation, const Vector3& newposition, const Vector3& neworientation);
	void removeIfNotReferenced();
	float computeSpeed();
	void notifySubordinateRemoved();
};

struct Server : SpecificGameState<ServerGameObject, ProgramType::SERVER>
{
	using GameObject = ServerGameObject;
	static Server *instance;

	//GameSet *gameSet;

	uint32_t randomSeed;
	TimeManager timeManager;
	uint32_t nextUniqueId;

	//ServerGameObject *level;
	//std::map<uint32_t, ServerGameObject*> idmap;

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
	std::vector<SrvGORef> clientPlayerObjects;

	struct ClientInfo {
		SrvGORef lastStampdown;
		std::vector<SrvGORef> unitsForNewFormation;
	};
	std::vector<ClientInfo> clientInfos;

	time_t lastSync;

	std::vector<std::tuple<ServerGameObject*, int, int>> postAssociations;

	ServerGameObject* objToDelete = nullptr, * objToDeleteLast = nullptr;

	Server() { instance = this; }

	void loadSaveGame(const char *filename);
	ServerGameObject *createObject(GameObjBlueprint *blueprint, uint32_t id = 0);
	ServerGameObject* spawnObject(GameObjBlueprint* blueprint, ServerGameObject* parent, const Vector3& initialPosition, const Vector3& initialOrientation);
	void deleteObject(ServerGameObject *obj);
	void destroyObject(ServerGameObject* obj);

	//ServerGameObject* findObject(uint32_t id) { auto it = idmap.find(id); return (it != idmap.end()) ? it->second : nullptr; }

	void addClient(NetLink* link);
	void removeClient(NetLink* link);

	void syncTime();

	void setClientPlayerControl(int clientIndex, uint32_t playerObjId);
	void setDiplomaticStatus(ServerGameObject *a, ServerGameObject *b, int status);
	void showGameTextWindow(ServerGameObject* player, int gtwIndex);
	void hideGameTextWindow(ServerGameObject* player, int gtwIndex);
	void hideCurrentGameTextWindow(ServerGameObject* player);
	void snapCameraPosition(ServerGameObject* player, const Vector3& position, const Vector3& orientation);
	void storeCameraPosition(ServerGameObject* player);
	void restoreCameraPosition(ServerGameObject* player);
	void playCameraPath(ServerGameObject* player, int camPathIndex);
	void stopCameraPath(ServerGameObject* player, bool skipActions);
	void skipCameraPath(ServerGameObject* player);
	void interpolateCameraToPosition(ServerGameObject* player, const Vector3& position, const Vector3& orientation, float duration);
	void interpolateCameraToStoredPosition(ServerGameObject* player, float duration);
	void setGameSpeed(float nextSpeed);
	void playMusic(ServerGameObject* player, int musicTag);
	void assignAlias(int aliasIndex, ServerGameObject* object);
	void unassignAlias(int aliasIndex, ServerGameObject* object);
	void clearAlias(int aliasIndex);

	void tick();

private:
	std::map<uint32_t, GameObjBlueprint*> predec;

	ServerGameObject * loadObject(GSFileParser & gsf, const std::string & clsname);
	void loadSavePredec(GSFileParser & gsf);

	void sendToAll(const NetPacket &packet);
	void sendToAll(const NetPacketWriter &packet);
	void sendTo(ServerGameObject* player, const NetPacketWriter& packet);

	friend struct ServerGameObject;
};