#include "actions.h"
#include "finder.h"
#include "../util/GSFileParser.h"
#include "gameset.h"
#include "../util/util.h"
#include "../server.h"

struct ActionUnknown : Action {
	virtual void run(ServerGameObject *self) override {
		ferr("Unknown action!");
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {}
};

struct ActionTrace : Action {
	std::string message;
	virtual void run(ServerGameObject *self) override {
		printf("Trace: %s\n", message.c_str());
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		message = gsf.nextString(true);
	}
	ActionTrace() {}
	ActionTrace(std::string &&message) : message(message) {}
};

struct ActionTraceValue : Action {
	std::string message;
	std::unique_ptr<ValueDeterminer> value;
	virtual void run(ServerGameObject *self) override {
		printf("Trace Value: %s %f\n", message.c_str(), value->eval(self));
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		message = gsf.nextString(true);
		value.reset(ReadValueDeterminer(gsf, gs));
	}
	ActionTraceValue() {}
	ActionTraceValue(std::string &&message, ValueDeterminer *value) : message(message), value(value) {}
};

struct ActionUponCondition : Action {
	std::unique_ptr<ValueDeterminer> value;
	ActionSequence trueList, falseList;
	virtual void run(ServerGameObject *self) override {
		if (value->eval(self) > 0.0f)
			trueList.run(self);
		else
			falseList.run(self);
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		value.reset(ReadValueDeterminer(gsf, gs));
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
	std::unique_ptr<ObjectFinder> finder;
	std::unique_ptr<ValueDeterminer> value;
	virtual void run(ServerGameObject *self) override {
		for (ServerGameObject *obj : finder->eval(self)) {
			obj->setItem(item, value->eval(self));
		}
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		item = gs.items.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
		value.reset(ReadValueDeterminer(gsf, gs));
	}
	ActionSetItem() {}
	ActionSetItem(int item, ObjectFinder *finder, ValueDeterminer *value) : item(item), finder(finder), value(value) {}
};

struct ActionIncreaseItem : Action {
	int item;
	std::unique_ptr<ObjectFinder> finder;
	std::unique_ptr<ValueDeterminer> value;
	virtual void run(ServerGameObject *self) override {
		for (ServerGameObject *obj : finder->eval(self)) {
			obj->setItem(item, obj->getItem(item) + value->eval(self));
		}
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		item = gs.items.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
		value.reset(ReadValueDeterminer(gsf, gs));
	}
	ActionIncreaseItem() {}
	ActionIncreaseItem(int item, ObjectFinder *finder, ValueDeterminer *value) : item(item), finder(finder), value(value) {}
};

struct ActionDecreaseItem : Action {
	int item;
	std::unique_ptr<ObjectFinder> finder;
	std::unique_ptr<ValueDeterminer> value;
	virtual void run(ServerGameObject *self) override {
		for (ServerGameObject *obj : finder->eval(self)) {
			obj->setItem(item, obj->getItem(item) - value->eval(self));
		}
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		item = gs.items.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
		value.reset(ReadValueDeterminer(gsf, gs));
	}
	ActionDecreaseItem() {}
	ActionDecreaseItem(int item, ObjectFinder *finder, ValueDeterminer *value) : item(item), finder(finder), value(value) {}
};

struct ActionExecuteSequence : Action {
	ActionSequence *sequence;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(ServerGameObject* self) override {
		for(ServerGameObject* obj : finder->eval(self))
			sequence->run(obj);
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		sequence = gs.actionSequences.readPtr(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
	ActionExecuteSequence() {}
	ActionExecuteSequence(ActionSequence *sequence, ObjectFinder *finder) : sequence(sequence), finder(finder) {}
};

struct ActionExecuteSequenceAfterDelay : Action {
	ActionSequence *sequence;
	std::unique_ptr<ObjectFinder> finder;
	std::unique_ptr<ValueDeterminer> delay;
	virtual void run(ServerGameObject* self) override {
		Server::DelayedSequence ds;
		ds.executor = self;
		ds.actionSequence = sequence;
		//ds.selfs = finder->eval(self);
		for (ServerGameObject *obj : finder->eval(self))
			ds.selfs.emplace_back(obj);
		game_time_t atTime = Server::instance->timeManager.currentTime + delay->eval(self);
		Server::instance->delayedSequences.insert(std::make_pair(atTime, ds));
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		sequence = gs.actionSequences.readPtr(gsf);
		finder.reset(ReadFinder(gsf, gs));
		delay.reset(ReadValueDeterminer(gsf, gs));
	}
	ActionExecuteSequenceAfterDelay() {}
	ActionExecuteSequenceAfterDelay(ActionSequence *sequence, ObjectFinder *finder, ValueDeterminer *delay) : sequence(sequence), finder(finder), delay(delay) {}
};

struct ActionPlayAnimationIfIdle : Action {
	int animationIndex;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(ServerGameObject *self) override {
		auto objs = finder->eval(self);
		for (ServerGameObject *obj : objs) {
			obj->setAnimation(animationIndex);
		}
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		animationIndex = gs.animations.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
	ActionPlayAnimationIfIdle() {}
	ActionPlayAnimationIfIdle(int animTag, ObjectFinder *finder) : animationIndex(animTag), finder(finder) {}
};

struct ActionExecuteOneAtRandom : Action {
	ActionSequence actionseq;
	virtual void run(ServerGameObject *self) override {
		if (!actionseq.actionList.empty()) {
			int x = rand() % actionseq.actionList.size();
			actionseq.actionList[x]->run(self);
		}
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		actionseq.init(gsf, gs, "END_EXECUTE_ONE_AT_RANDOM");
	}
};

struct ActionTerminateThisTask : Action {
	virtual void run(ServerGameObject *self) override {
		if (Order *order = self->orderConfig.getCurrentOrder())
			order->getCurrentTask()->terminate();
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {}
};

struct ActionTerminateThisOrder : Action {
	virtual void run(ServerGameObject *self) override {
		if (Order *order = self->orderConfig.getCurrentOrder())
			order->terminate();
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {}
};

struct ActionTransferControl : Action {
	std::unique_ptr<ObjectFinder> togiveFinder, recipientFinder;
	virtual void run(ServerGameObject *self) override {
		auto objs = togiveFinder->eval(self);
		ServerGameObject *recipient = recipientFinder->getFirst(self);
		for (ServerGameObject *obj : objs)
			obj->setParent(recipient);
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		togiveFinder.reset(ReadFinder(gsf, gs));
		recipientFinder.reset(ReadFinder(gsf, gs));
	}
	ActionTransferControl() {}
	ActionTransferControl(ObjectFinder *togiveFinder, ObjectFinder *recipientFinder) : togiveFinder(togiveFinder), recipientFinder(recipientFinder) {}
};

struct ActionAssignOrderVia : Action {
	const OrderAssignmentBlueprint *oabp;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(ServerGameObject *self) override {
		for (ServerGameObject *obj : finder->eval(self)) {
			oabp->assignTo(obj);
		}
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		oabp = gs.orderAssignments.readPtr(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
	ActionAssignOrderVia() {}
	ActionAssignOrderVia(const OrderAssignmentBlueprint *oabp, ObjectFinder *finder) : oabp(oabp), finder(finder) {}
};

struct ActionRemove : Action {
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(ServerGameObject *self) override {
		for (ServerGameObject *obj : finder->eval(self)) {
			Server::instance->deleteObject(obj);
		}
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
	ActionRemove() {}
	ActionRemove(ObjectFinder *finder) : finder(finder) {}
};

struct ActionAssignAlias : Action {
	int aliasIndex;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(ServerGameObject *self) override {
		auto &alias = Server::instance->aliases[aliasIndex];
		for (ServerGameObject *obj : finder->eval(self)) {
			alias.insert(obj);
		}
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		aliasIndex = gs.aliases.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
	ActionAssignAlias() {}
	ActionAssignAlias(int aliasIndex, ObjectFinder *finder) : aliasIndex(aliasIndex), finder(finder) {}
};

struct ActionUnassignAlias : Action {
	int aliasIndex;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(ServerGameObject *self) override {
		auto &alias = Server::instance->aliases[aliasIndex];
		for (ServerGameObject *obj : finder->eval(self)) {
			alias.erase(obj);
		}
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		aliasIndex = gs.aliases.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
	ActionUnassignAlias() {}
	ActionUnassignAlias(int aliasIndex, ObjectFinder *finder) : aliasIndex(aliasIndex), finder(finder) {}
};

struct ActionClearAlias : Action {
	int aliasIndex;
	virtual void run(ServerGameObject *self) override {
		auto &alias = Server::instance->aliases[aliasIndex];
		alias.clear();
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		aliasIndex = gs.aliases.readIndex(gsf);
	}
	ActionClearAlias() {}
	ActionClearAlias(int aliasIndex) : aliasIndex(aliasIndex) {}
};

Action *ReadAction(GSFileParser &gsf, const GameSet &gs)
{
	Action *action;
	switch (Tags::ACTION_tagDict.getTagID(gsf.nextString().c_str())) {
	case Tags::ACTION_TRACE: action = new ActionTrace; break;
	case Tags::ACTION_TRACE_VALUE: action = new ActionTraceValue; break;
	case Tags::ACTION_SET_ITEM: action = new ActionSetItem; break;
	case Tags::ACTION_INCREASE_ITEM: action = new ActionIncreaseItem; break;
	case Tags::ACTION_DECREASE_ITEM: action = new ActionDecreaseItem; break;
	case Tags::ACTION_UPON_CONDITION:action = new ActionUponCondition; break;
	case Tags::ACTION_EXECUTE_SEQUENCE: action = new ActionExecuteSequence; break;
	case Tags::ACTION_EXECUTE_SEQUENCE_AFTER_DELAY: action = new ActionExecuteSequenceAfterDelay; break;
	case Tags::ACTION_PLAY_ANIMATION_IF_IDLE: action = new ActionPlayAnimationIfIdle; break;
	case Tags::ACTION_EXECUTE_ONE_AT_RANDOM: action = new ActionExecuteOneAtRandom; break;
	case Tags::ACTION_TERMINATE_THIS_TASK: action = new ActionTerminateThisTask; break;
	case Tags::ACTION_TERMINATE_THIS_ORDER: action = new ActionTerminateThisOrder; break;
	case Tags::ACTION_TERMINATE_ORDER: action = new ActionTerminateThisOrder; break;
	case Tags::ACTION_TRANSFER_CONTROL: action = new ActionTransferControl; break;
	case Tags::ACTION_ASSIGN_ORDER_VIA: action = new ActionAssignOrderVia; break;
	case Tags::ACTION_REMOVE: action = new ActionRemove; break;
	case Tags::ACTION_ASSIGN_ALIAS: action = new ActionAssignAlias; break;
	case Tags::ACTION_UNASSIGN_ALIAS: action = new ActionUnassignAlias; break;
	case Tags::ACTION_CLEAR_ALIAS: action = new ActionClearAlias; break;
	default: action = new ActionUnknown; break;
	}
	action->parse(gsf, const_cast<GameSet&>(gs));  // FIXME
	return action;
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