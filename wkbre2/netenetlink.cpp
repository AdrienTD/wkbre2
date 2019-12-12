#include "netenetlink.h"

bool NetEnetLink::available() {
	return !packets.empty();
}

NetPacket NetEnetLink::receive() {
	ENetPacket *epacket = (ENetPacket*)packets.front();
	packets.pop();
	NetPacket netpacket = NetPacket(std::string((char*)epacket->data, epacket->dataLength));
	enet_packet_destroy(epacket);
	return netpacket;
}

void NetEnetLink::send(const NetPacket & packet) {
	ENetPacket *epacket = enet_packet_create(packet.data.c_str(), packet.data.size(), ENET_PACKET_FLAG_RELIABLE);
	enet_peer_send((ENetPeer*)peer, 0, epacket);
}