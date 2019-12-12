#include "client.h"
#include "server.h"
#include "network.h"
#include "gameset/gameset.h"
#include <cassert>
#include "terrain.h"
#include "interface/ClientInterface.h"

void Client::loadFromServerObject(Server & server)
{
}

void Client::tick()
{
	if (serverLink) {
		int pcnt = 10;
		while (serverLink->available() && (pcnt--)) {
			NetPacket packet = serverLink->receive();
			BinaryReader br(packet.data.c_str());
			uint8_t type = br.readUint8();

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
				if (localServer) {
					info("Client takes gameset from local server.\n");
					gameSet = Server::instance->gameSet;
				}
				else {
					info("Loading game set: %s\n", gsFileName.c_str());
					gameSet = new GameSet(gsFileName.c_str());
				}
				break;
			}
			case NETCLIMSG_OBJECT_CREATED: {
				uint32_t objid = br.readUint32();
				uint32_t bpid = br.readUint32();
				uint32_t subtype = br.readUint32();
				GameObjBlueprint *blueprint = gameSet->getBlueprint(bpid);
				info("Object %u of type %s is created.\n", objid, blueprint->getFullName().c_str());
				ClientGameObject *obj = createObject(blueprint, objid);
				obj->subtype = subtype;
				if (blueprint->bpClass == Tags::GAMEOBJCLASS_LEVEL)
					level = obj;
				break;
			}
			case NETCLIMSG_OBJECT_ITEM_SET: {
				uint32_t objid = br.readUint32();
				uint32_t index = br.readUint32();
				float value = br.readFloat();
				info("Object %u changed its item \"%s\" to the value %f.\n", objid, gameSet->itemNames.getString(index).c_str(), value);
				findObject(objid)->items[index] = value;
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
				terrain->readBCM(mapfp.c_str());
				if (cliInterface)
					cliInterface->updateTerrain();
				break;
			}
			case NETCLIMSG_OBJECT_POSITION_SET: {
				uint32_t objid = br.readUint32();
				Vector3 pos = br.readVector3();
				info("Object %u teleported to (%f,%f,%f)\n", objid, pos.x, pos.y, pos.z);
				if (ClientGameObject *obj = findObject(objid)) {
					obj->position = pos;
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
			}
		}
	}
}

void Client::sendMessage(const std::string &msg) {
	NetPacketWriter packet(NETSRVMSG_TEST);
	packet.writeStringZ(msg);
	serverLink->send(packet);
}

void Client::sendCommand(ClientGameObject * obj, Command * cmd)
{
	NetPacketWriter packet(NETSRVMSG_COMMAND);
	packet.writeUint32(cmd->id);
	packet.writeUint32(obj->id);
	serverLink->send(packet);
}

ClientGameObject * Client::createObject(GameObjBlueprint * blueprint, uint32_t id)
{
	ClientGameObject *obj = new ClientGameObject(id, blueprint);
	assert(idmap[id] == nullptr);
	idmap[id] = obj;
	return obj;
}
