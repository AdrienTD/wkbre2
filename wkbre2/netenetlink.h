#pragma once

#include "network.h"
#include <enet/enet.h>

struct NetEnetLink : NetLink {
	ENetHost *host;
	ENetPeer *peer;
	std::queue<ENetPacket *> packets;

	bool available() override;
	NetPacket receive() override;
	void send(const NetPacket &packet) override;

	NetEnetLink(ENetHost *host, ENetPeer *peer) : host(host), peer(peer) {}
};