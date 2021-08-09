#pragma once

#include <vector>
#include "actions.h"

struct GSFileParser;
struct GameSet;
struct PackageReceiptTrigger;

struct Reaction {
	std::vector<int> events;
	std::vector<PackageReceiptTrigger*> prTriggers;
	ActionSequence actions;

	void parse(GSFileParser &gsf, GameSet &gs);
	bool canBeTriggeredBy(int evt, ServerGameObject* obj, ServerGameObject* sender);
};

struct PackageReceiptTrigger {
	std::vector<int> events;
	std::vector<int> assessments;

	void parse(GSFileParser &gsf, GameSet &gs);
	bool canBeTriggeredBy(int evt, ServerGameObject* obj, ServerGameObject* sender);
};