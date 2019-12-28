#pragma once

#include <cstdint>
#include <queue>
#include <memory>
#include <string>
#include <cassert>
#include <mutex>
#include "util/BinaryReader.h"
#include "util/vecmat.h"

enum NetClientMessages {
	NETCLIMSG_HELLO = 1,
	NETCLIMSG_TEST,
	NETCLIMSG_GAME_SET,
	NETCLIMSG_OBJECT_CREATED,
	NETCLIMSG_OBJECT_PARENT_SET,
	NETCLIMSG_OBJECT_ITEM_SET,
	NETCLIMSG_TERRAIN_SET,
	NETCLIMSG_OBJECT_POSITION_SET,
	NETCLIMSG_OBJECT_ORIENTATION_SET,
	NETCLIMSG_OBJECT_SUBTYPE_APPEARANCE_SET,
	NETCLIMSG_OBJECT_COLOR_SET,
};

enum NetServerMessages {
	NETSRVMSG_HELLO = 1,
	NETSRVMSG_TEST,
	NETSRVMSG_COMMAND,
	NETSRVMSG_PAUSE,
};

struct NetPacketWriter {
	static const size_t NET_MAX_PACKET_SIZE = 256;

	uint8_t *nextWritePnt;
	uint8_t data[NET_MAX_PACKET_SIZE];

	size_t size() const { return nextWritePnt - data; }
	size_t rem() const { return NET_MAX_PACKET_SIZE - size(); }

	void writeUint8(uint8_t a) { assert(rem() >= 1);  *nextWritePnt = a; nextWritePnt++; }
	void writeUint16(uint16_t a) { assert(rem() >= 2); *(uint16_t*)nextWritePnt = a; nextWritePnt += 2; }
	void writeUint32(uint32_t a) { assert(rem() >= 4); *(uint32_t*)nextWritePnt = a; nextWritePnt += 4; }
	void writeFloat(float a) { assert(rem() >= 4); *(float*)nextWritePnt = a; nextWritePnt += 4; }
	void writeString(const std::string &str) { size_t len = str.size();  assert(rem() >= len); memcpy(nextWritePnt, str.c_str(), len); nextWritePnt += len; }
	void writeStringZ(const std::string &str) { writeString(str); writeUint8(0); }
	void writeVector3(const Vector3 &vec) { writeFloat(vec.x); writeFloat(vec.y); writeFloat(vec.z); }

	std::string getString() const { return std::string((char*)data, size()); }

	//NetPacketWriter() : nextWritePnt(data) {}
	NetPacketWriter(uint8_t type) { nextWritePnt = data;  writeUint8(type); }
};

struct NetPacket {
	//std::unique_ptr<uint8_t> data;
	std::string data;
	NetPacket(const std::string &data) : data(data) {}
	NetPacket(const std::string &&data) : data(data) {}
};

struct NetLocalBuffer {
	std::mutex mutex;
	std::queue<NetPacket> queue;
};

struct NetLink {
	// Reading
	virtual bool available() = 0;
	virtual NetPacket receive() = 0;
	virtual void send(const NetPacket &packet) = 0;

	void send(const NetPacketWriter &writer) { send(NetPacket(writer.getString())); }

	//int beginRead() = 0;
	//void endRead() = 0;
};

struct NetLocalLink : NetLink {
	NetLocalBuffer *inMessages, *outMessages;

	bool available() override;
	NetPacket receive() override;
	void send(const NetPacket &packet) override;

	NetLocalLink(NetLocalBuffer *inMessages, NetLocalBuffer *outMessages) : inMessages(inMessages), outMessages(outMessages) {}
};
