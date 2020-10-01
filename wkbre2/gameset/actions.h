#pragma once

#include <vector>
#include <memory>
#include <memory>

struct ServerGameObject;
struct GSFileParser;
struct GameSet;

struct Action {
	virtual void run(ServerGameObject *self) = 0;
};

struct ActionSequence {
	std::vector<std::unique_ptr<Action>> actionList;
	void run(ServerGameObject *self) {
		for (auto &action : actionList)
			action->run(self);
	}
	void init(GSFileParser &gsf, const GameSet &gs, const char *endtag);
};

Action *ReadAction(GSFileParser &gsf, const GameSet &gs);
