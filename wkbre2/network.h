// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

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
	NETCLIMSG_TIME_SYNC,
	NETCLIMSG_OBJECT_ANIM_SET,
	NETCLIMSG_OBJECT_MOVEMENT_STARTED,
	NETCLIMSG_OBJECT_MOVEMENT_STOPPED,
	NETCLIMSG_OBJECT_REMOVED,
	NETCLIMSG_OBJECT_CONVERTED,
	NETCLIMSG_DIPLOMATIC_STATUS_SET,
	NETCLIMSG_SHOW_GAME_TEXT_WINDOW,
	NETCLIMSG_OBJECT_SCALE_SET,
	NETCLIMSG_HIDE_GAME_TEXT_WINDOW,
	NETCLIMSG_HIDE_CURRENT_GAME_TEXT_WINDOW,
	NETCLIMSG_OBJECT_FLAGS_SET,
	NETCLIMSG_OBJECT_DISABLE_COUNT_SET,
	NETCLIMSG_STORE_CAMERA_POSITION,
	NETCLIMSG_RESTORE_CAMERA_POSITION,
	NETCLIMSG_PLAY_CAMERA_PATH,
	NETCLIMSG_STOP_CAMERA_PATH,
	NETCLIMSG_OBJECT_TRAJECTORY_STARTED,
	NETCLIMSG_GAME_SPEED_CHANGED,
	NETCLIMSG_SOUND_AT_OBJECT,
	NETCLIMSG_SOUND_AT_POSITION,
	NETCLIMSG_MUSIC_CHANGED,
	NETCLIMSG_CONTROL_PLAYER,
	NETCLIMSG_SNAP_CAMERA_POSITION,
	NETCLIMSG_SPECIAL_EFFECT_PLAYED_AT_POS,
	NETCLIMSG_SPECIAL_EFFECT_PLAYED_BETWEEN,
	NETCLIMSG_ATTACHED_SPECIAL_EFFECT_PLAYED,
	NETCLIMSG_OBJECT_NAME_SET,
	NETCLIMSG_CURRENT_ORDER_REPORT,
	NETCLIMSG_LOOPING_SPECIAL_EFFECT_ATTACHED,
	NETCLIMSG_LOOPING_SPECIAL_EFFECT_DETACHED,
	NETCLIMSG_SKIP_CAMERA_PATH,
	NETCLIMSG_ALIAS_ASSIGNED,
	NETCLIMSG_ALIAS_UNASSIGNED,
	NETCLIMSG_ALIAS_CLEARED,
	NETCLIMSG_INTERPOLATE_CAMERA_TO_POSITION,
	NETCLIMSG_INTERPOLATE_CAMERA_TO_STORED_POSITION,
	NETCLIMSG_UPDATE_BUILDING_ORDER_COUNT_MAP,
};

enum NetServerMessages {
	NETSRVMSG_HELLO = 1,
	NETSRVMSG_TEST,
	NETSRVMSG_COMMAND,
	NETSRVMSG_PAUSE,
	NETSRVMSG_STAMPDOWN,
	NETSRVMSG_START_LEVEL,
	NETSRVMSG_GAME_TEXT_WINDOW_BUTTON_CLICKED,
	NETSRVMSG_CAMERA_PATH_ENDED,
	NETSRVMSG_CHANGE_GAME_SPEED,
	NETSRVMSG_MUSIC_COMPLETED,
	NETSRVMSG_TERMINATE_OBJECT,
	NETSRVMSG_BUILD_LAST_STAMPDOWNED_OBJECT,
	NETSRVMSG_CANCEL_COMMAND,
	NETSRVMSG_PUT_UNIT_INTO_NEW_FORMATION,
	NETSRVMSG_CREATE_NEW_FORMATION,
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

	void writeValues(uint8_t a) { writeUint8(a); }
	void writeValues(uint16_t a) { writeUint16(a); }
	void writeValues(uint32_t a) { writeUint32(a); }
	void writeValues(int32_t a) { writeUint32((uint32_t)a); }
	void writeValues(float a) { writeFloat(a); }
	void writeValues(const std::string & str) { writeStringZ(str); }
	void writeValues(const Vector3 &vec) { writeVector3(vec); }
	template<typename T, typename ... Args> void writeValues(T val, Args ... rest) { writeValues(val); writeValues(rest...); }

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
