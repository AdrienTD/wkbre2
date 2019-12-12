#include "network.h"

bool NetLocalLink::available() {
	inMessages->mutex.lock();
	bool r = !inMessages->queue.empty();
	inMessages->mutex.unlock();
	return r;
}

NetPacket NetLocalLink::receive() {
	inMessages->mutex.lock();
	NetPacket p = inMessages->queue.front();
	inMessages->queue.pop();
	inMessages->mutex.unlock();
	return p;
}

void NetLocalLink::send(const NetPacket & packet) {
	outMessages->mutex.lock();
	outMessages->queue.push(packet);
	outMessages->mutex.unlock();
}

