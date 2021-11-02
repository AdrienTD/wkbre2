// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include "network.h"

struct _ENetHost;
struct _ENetPeer;
struct _ENetPacket;

struct NetEnetLink : NetLink {
	_ENetHost *host;
	_ENetPeer *peer;
	std::queue<_ENetPacket *> packets;

	bool available() override;
	NetPacket receive() override;
	void send(const NetPacket &packet) override;

	NetEnetLink(_ENetHost *host, _ENetPeer *peer) : host(host), peer(peer) {}
};