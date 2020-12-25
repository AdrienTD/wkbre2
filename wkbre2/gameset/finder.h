#pragma once

#include <vector>

struct ServerGameObject;
struct ClientGameObject;
struct GSFileParser;
struct GameSet;

struct ObjectFinder {
	using EvalRetSrv = std::vector<ServerGameObject*>;
	using EvalRetCli = std::vector<ClientGameObject*>;
	virtual ~ObjectFinder() {}
	virtual std::vector<ServerGameObject*> eval(ServerGameObject *self) = 0;
	virtual std::vector<ClientGameObject*> eval(ClientGameObject *self);
	virtual void parse(GSFileParser &gsf, GameSet &gs) = 0;
	template<typename T> T *getFirst(T *self) {
		std::vector<T*> objlist = eval(self);
		if (objlist.size() > 0)
			return objlist[0];
		return nullptr;
	}
};

ObjectFinder *ReadFinder(GSFileParser &gsf, const GameSet &gs);
ObjectFinder *ReadFinderNode(GSFileParser &gsf, const GameSet &gs);
