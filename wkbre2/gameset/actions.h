#pragma once

#include <vector>
#include <memory>

struct ServerGameObject;
struct GSFileParser;
struct GameSet;

struct Action {
	virtual void run(ServerGameObject *self) = 0;
};

struct ActionSequence {
	std::vector<Action*> actionList;
	void run(ServerGameObject *self) {
		for (Action *action : actionList)
			action->run(self);
	}
	void init(GSFileParser &gsf, const GameSet &gs, const char *endtag);
	~ActionSequence() {
		for (Action *action : actionList)
			delete action;
	}
};

Action *ReadAction(GSFileParser &gsf, const GameSet &gs);
