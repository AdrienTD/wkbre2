#pragma once

#include <vector>
#include "../server.h"

struct ObjectFinder {
	virtual std::vector<ServerGameObject*> eval(ServerGameObject *self) = 0;
	ServerGameObject *getFirst(ServerGameObject *self) {
		std::vector<ServerGameObject*> objlist = eval(self);
		if (objlist.size() > 0)
			return objlist[0];
		return nullptr;
	}
};

ObjectFinder *ReadFinder(GSFileParser &gsf, const GameSet &gs);
