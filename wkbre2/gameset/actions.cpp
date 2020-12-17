#include "actions.h"
#include "finder.h"
#include "../util/GSFileParser.h"
#include "gameset.h"
#include "../util/util.h"

struct ActionUnknown : Action {
	void run(ServerGameObject *self) {
		ferr("Unknown action!");
	}
};

struct ActionTrace : Action {
	std::string message;
	void run(ServerGameObject *self) {
		printf("Trace: %s\n", message.c_str());
	}
	ActionTrace(std::string &&message) : message(message) {}
};

struct ActionTraceValue : Action {
	std::string message;
	ValueDeterminer *value;
	void run(ServerGameObject *self) {
		printf("Trace Value: %s %f\n", message.c_str(), value->eval(self));
	}
	ActionTraceValue(std::string &&message, ValueDeterminer *value) : message(message), value(value) {}
};

struct ActionUponCondition : Action {
	ValueDeterminer *value;
	ActionSequence trueList, falseList;
	void run(ServerGameObject *self) {
		if (value->eval(self) > 0.0f)
			trueList.run(self);
		else
			falseList.run(self);
	}
	ActionUponCondition(GSFileParser &gsf, const GameSet &gs) {
		value = ReadValueDeterminer(gsf, gs);
		gsf.advanceLine();
		ActionSequence *curlist = &trueList;
		while (!gsf.eof) {
			std::string strtag = gsf.nextTag();

			if (strtag == "ACTION")
				curlist->actionList.push_back(std::unique_ptr<Action>(ReadAction(gsf, gs)));
			else if (strtag == "ELSE")
				curlist = &falseList;
			else if (strtag == "END_UPON_CONDITION")
				return;
			gsf.advanceLine();
		}
		ferr("END_UPON_CONDITION missing!");
	}
};

struct ActionSetItem : Action {
	int item;
	ObjectFinder *finder;
	ValueDeterminer *value;
	void run(ServerGameObject *self) {
		for (ServerGameObject *obj : finder->eval(self)) {
			obj->setItem(item, value->eval(self));
		}
	}
	ActionSetItem(int item, ObjectFinder *finder, ValueDeterminer *value) : item(item), finder(finder), value(value) {}
};

struct ActionIncreaseItem : Action {
	int item;
	ObjectFinder *finder;
	ValueDeterminer *value;
	void run(ServerGameObject *self) {
		for (ServerGameObject *obj : finder->eval(self)) {
			obj->setItem(item, obj->getItem(item) + value->eval(self));
		}
	}
	ActionIncreaseItem(int item, ObjectFinder *finder, ValueDeterminer *value) : item(item), finder(finder), value(value) {}
};

struct ActionDecreaseItem : Action {
	int item;
	ObjectFinder *finder;
	ValueDeterminer *value;
	void run(ServerGameObject *self) {
		for (ServerGameObject *obj : finder->eval(self)) {
			obj->setItem(item, obj->getItem(item) - value->eval(self));
		}
	}
	ActionDecreaseItem(int item, ObjectFinder *finder, ValueDeterminer *value) : item(item), finder(finder), value(value) {}
};

struct ActionExecuteSequence : Action {
	ActionSequence *sequence;
	ObjectFinder *finder;
	void run(ServerGameObject* self) {
		for(ServerGameObject* obj : finder->eval(self))
			sequence->run(obj);
	}
	ActionExecuteSequence(ActionSequence *sequence, ObjectFinder *finder) : sequence(sequence), finder(finder) {}
};

struct ActionExecuteSequenceAfterDelay : Action {
	ActionSequence *sequence;
	ObjectFinder *finder;
	ValueDeterminer *delay;
	void run(ServerGameObject* self) {
		Server::DelayedSequence ds;
		ds.executor = self;
		ds.actionSequence = sequence;
		ds.selfs = finder->eval(self);
		game_time_t atTime = Server::instance->timeManager.currentTime + delay->eval(self);
		Server::instance->delayedSequences.insert(std::make_pair(atTime, ds));
	}
	ActionExecuteSequenceAfterDelay(ActionSequence *sequence, ObjectFinder *finder, ValueDeterminer *delay) : sequence(sequence), finder(finder), delay(delay) {}
};

struct ActionPlayAnimationIfIdle : Action {
	int animationIndex;
	ObjectFinder *finder;
	void run(ServerGameObject *self) {
		auto objs = finder->eval(self);
		for (ServerGameObject *obj : objs) {
			obj->setAnimation(animationIndex);
		}
	}
	ActionPlayAnimationIfIdle(int animTag, ObjectFinder *finder) : animationIndex(animTag), finder(finder) {}
};

struct ActionExecuteOneAtRandom : Action {
	ActionSequence actionseq;
	void run(ServerGameObject *self) {
		if (!actionseq.actionList.empty()) {
			int x = rand() % actionseq.actionList.size();
			actionseq.actionList[x]->run(self);
		}
	}
	ActionExecuteOneAtRandom(GSFileParser &gsf, const GameSet &gs) {
		actionseq.init(gsf, gs, "END_EXECUTE_ONE_AT_RANDOM");
	}
};

Action *ReadAction(GSFileParser &gsf, const GameSet &gs)
{
	switch (Tags::ACTION_tagDict.getTagID(gsf.nextString().c_str())) {
	case Tags::ACTION_TRACE:
		return new ActionTrace(gsf.nextString(true));
	case Tags::ACTION_TRACE_VALUE: {
		std::string msg = gsf.nextString(true);
		ValueDeterminer *vd = ReadValueDeterminer(gsf, gs);
		return new ActionTraceValue(std::move(msg), vd);
	}
	case Tags::ACTION_SET_ITEM: {
		int item = gs.itemNames.getIndex(gsf.nextString(true));
		ObjectFinder *finder = ReadFinder(gsf, gs);
		ValueDeterminer *value = ReadValueDeterminer(gsf, gs);
		return new ActionSetItem(item, finder, value);
	}
	case Tags::ACTION_INCREASE_ITEM: {
		int item = gs.itemNames.getIndex(gsf.nextString(true));
		ObjectFinder *finder = ReadFinder(gsf, gs);
		ValueDeterminer *value = ReadValueDeterminer(gsf, gs);
		return new ActionIncreaseItem(item, finder, value);
	}
	case Tags::ACTION_DECREASE_ITEM: {
		int item = gs.itemNames.getIndex(gsf.nextString(true));
		ObjectFinder *finder = ReadFinder(gsf, gs);
		ValueDeterminer *value = ReadValueDeterminer(gsf, gs);
		return new ActionDecreaseItem(item, finder, value);
	}
	case Tags::ACTION_UPON_CONDITION:
		return new ActionUponCondition(gsf, gs);
	case Tags::ACTION_EXECUTE_SEQUENCE: {
		int ax = gs.actionSequenceNames.getIndex(gsf.nextString(true));
		ObjectFinder *finder = ReadFinder(gsf, gs);
		return new ActionExecuteSequence(&gs.actionSequences[ax], finder);
	}
	case Tags::ACTION_EXECUTE_SEQUENCE_AFTER_DELAY: {
		int ax = gs.actionSequenceNames.getIndex(gsf.nextString(true));
		ObjectFinder *finder = ReadFinder(gsf, gs);
		ValueDeterminer *delay = ReadValueDeterminer(gsf, gs);
		return new ActionExecuteSequenceAfterDelay(&gs.actionSequences[ax], finder, delay);
	}
	case Tags::ACTION_PLAY_ANIMATION_IF_IDLE: {
		int animTag = gs.animationNames.getIndex(gsf.nextString(true));
		ObjectFinder *finder = ReadFinder(gsf, gs);
		return new ActionPlayAnimationIfIdle(animTag, finder);
	}
	case Tags::ACTION_EXECUTE_ONE_AT_RANDOM: {
		return new ActionExecuteOneAtRandom(gsf, gs);
	}
	}
	return new ActionUnknown();
}

void ActionSequence::init(GSFileParser &gsf, const GameSet &gs, const char *endtag)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		std::string strtag = gsf.nextTag();

		if (strtag == "ACTION")
			actionList.push_back(std::unique_ptr<Action>(ReadAction(gsf, gs)));
		else if (strtag == endtag)
			return;
		gsf.advanceLine();
	}
	ferr("Action sequence reached end of file without END_ACTION_SEQUENCE!");
}