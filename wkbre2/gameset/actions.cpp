#include "actions.h"
#include "finder.h"
#include "../util/GSFileParser.h"
#include "gameset.h"
#include "../util/util.h"
#include "../server.h"
#include "ScriptContext.h"

struct ActionUnknown : Action {
	std::string name;
	virtual void run(ServerGameObject *self) override {
		ferr("Unknown action %s!", name.c_str());
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {}
	ActionUnknown(const std::string& name) : name(name) {}
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

struct ActionTraceFinderResults : Action {
	std::string message;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(ServerGameObject *self) override {
		auto vec = finder->eval(self);
		printf("Trace Finder: %s\n %zi objects:\n", message.c_str(), vec.size());
		for (ServerGameObject *obj : vec)
			printf(" - %i %s\n", obj->id, obj->blueprint->getFullName().c_str());
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		message = gsf.nextString(true);
		finder.reset(ReadFinder(gsf, gs));
	}
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
		for (ServerGameObject* obj : finder->eval(self)) {
			auto _ = SrvScriptContext::sequenceExecutor.change(self);
			sequence->run(obj);
		}
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
			oabp->assignTo(obj, self);
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

struct ActionAddReaction : Action {
	Reaction *reaction;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(ServerGameObject *self) override {
		for(ServerGameObject *obj : finder->eval(self))
			obj->individualReactions.insert(reaction);
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		reaction = gs.reactions.readPtr(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionRemoveReaction : Action {
	Reaction *reaction;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(ServerGameObject *self) override {
		for (ServerGameObject *obj : finder->eval(self))
			obj->individualReactions.erase(reaction);
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		reaction = gs.reactions.readPtr(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionSendEvent : Action {
	int event = -1;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(ServerGameObject *self) override {
		if (event == -1) return;
		for (ServerGameObject *obj : finder->eval(self))
			obj->sendEvent(event, self);
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		event = gs.events.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionCreateObjectVia : Action {
	ObjectCreation *info;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(ServerGameObject *self) override {
		for (ServerGameObject *obj : finder->eval(self))
			info->run(obj);
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		info = gs.objectCreations.readPtr(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionRegisterAssociates : Action {
	std::unique_ptr<ObjectFinder> f_aors, f_ated;
	int category;
	virtual void run(ServerGameObject *self) override {
		auto aors = f_aors->eval(self);
		auto ated = f_ated->eval(self);
		for (ServerGameObject *aor : aors) {
			for (ServerGameObject *obj : ated)
				aor->associateObject(category, obj);
		}
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		f_aors.reset(ReadFinder(gsf, gs));
		category = gs.associations.readIndex(gsf);
		f_ated.reset(ReadFinder(gsf, gs));
	}
};

struct ActionDeregisterAssociates : Action {
	std::unique_ptr<ObjectFinder> f_aors, f_ated;
	int category;
	virtual void run(ServerGameObject *self) override {
		auto aors = f_aors->eval(self);
		auto ated = f_ated->eval(self);
		for (ServerGameObject *aor : aors) {
			for (ServerGameObject *obj : ated)
				aor->dissociateObject(category, obj);
		}
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		f_aors.reset(ReadFinder(gsf, gs));
		category = gs.associations.readIndex(gsf);
		f_ated.reset(ReadFinder(gsf, gs));
	}
};

struct ActionClearAssociates : Action {
	int category;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(ServerGameObject *self) override {
		for (ServerGameObject *obj : finder->eval(self))
			obj->clearAssociates(category);
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		category = gs.associations.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionConvertTo : Action {
	GameObjBlueprint *postbp;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(ServerGameObject *self) override {
		for (ServerGameObject *obj : finder->eval(self))
			obj->convertTo(postbp);
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		postbp = gs.readObjBlueprintPtr(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionNop : Action {
	virtual void run(ServerGameObject *self) override {}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {}
};

struct ActionSwitchAppearance : Action {
	int appear;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(ServerGameObject *self) override {
		for (ServerGameObject *obj : finder->eval(self))
			obj->setSubtypeAndAppearance(obj->subtype, appear);
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		appear = gs.appearances.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionConvertAccordingToTag : Action {
	int typeTag;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(ServerGameObject *self) override {
		for (ServerGameObject *obj : finder->eval(self)) {
			auto it = obj->blueprint->mappedTypeTags.find(typeTag);
			if(it != obj->blueprint->mappedTypeTags.end())
				obj->convertTo(it->second);
		}
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		typeTag = gs.typeTags.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionRepeatSequence : Action {
	ActionSequence *sequence;
	std::unique_ptr<ObjectFinder> finder;
	std::unique_ptr<ValueDeterminer> vdcount;
	virtual void run(ServerGameObject* self) override {
		int count = (int)vdcount->eval(self);
		for (ServerGameObject* obj : finder->eval(self)) {
			auto _ = SrvScriptContext::sequenceExecutor.change(self);
			for (int i = 0; i < count; i++)
				sequence->run(obj);
		}
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		sequence = gs.actionSequences.readPtr(gsf);
		finder.reset(ReadFinder(gsf, gs));
		vdcount.reset(ReadValueDeterminer(gsf, gs));
	}
};

struct ActionIdentifyAndMarkClusters : Action {
	GameObjBlueprint* objtype;
	std::unique_ptr<ObjectFinder> finder;
	std::unique_ptr<ValueDeterminer> vcr;
	std::unique_ptr<ValueDeterminer> vir;
	std::unique_ptr<ValueDeterminer> vmr;
	virtual void run(ServerGameObject* self) override {
		ServerGameObject* player = self->getPlayer();
		auto gl = finder->eval(self);
		float rad = vcr->eval(self);
		float fmr = vmr->eval(self);
		float sqrad = rad * rad;
		DynArray<bool> taken(gl.size());
		for (bool& b : taken) b = false;
		for (size_t i = 0; i < gl.size(); i++) {
			if (taken[i]) continue;
			ServerGameObject* o = gl[i];

			float clrat = 0;
			for (size_t j = 0; j < gl.size(); j++) {
				if (taken[j]) continue;
				ServerGameObject* p = gl[j];
				if ((p->position - o->position).sqlen2xz() < sqrad) {
					clrat += vir->eval(p);
				}
			}

			if (clrat >= fmr) {
				ServerGameObject* mark = Server::instance->createObject(objtype);
				mark->setParent(player);
				mark->setPosition(o->position);
				for (size_t j = 0; j < gl.size(); j++) {
					if (taken[j]) continue;
					ServerGameObject* p = gl[j];
					if ((p->position - o->position).sqlen2xz() < sqrad) {
						taken[j] = true;
					}
				}
			}
		}
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		objtype = gs.objBlueprints[Tags::GAMEOBJCLASS_MARKER].readPtr(gsf);
		finder.reset(ReadFinder(gsf, gs));
		vcr.reset(ReadValueDeterminer(gsf, gs));
		vir.reset(ReadValueDeterminer(gsf, gs));
		vmr.reset(ReadValueDeterminer(gsf, gs));
	}
};

struct ActionExecuteSequenceOverPeriod : Action {
	ActionSequence* sequence;
	std::unique_ptr<ObjectFinder> finder;
	std::unique_ptr<ValueDeterminer> vdperiod;
	virtual void run(ServerGameObject* self) override {
		auto vec = finder->eval(self);
		Server::OverPeriodSequence ops;
		ops.actionSequence = sequence;
		ops.executor = self;
		ops.startTime = Server::instance->timeManager.currentTime;
		ops.period = vdperiod->eval(self);
		ops.remainingObjects = std::vector<SrvGORef>(vec.begin(), vec.end());
		ops.numExecutionsDone = 0;
		ops.numTotalExecutions = ops.remainingObjects.size();
		Server::instance->overPeriodSequences.push_back(std::move(ops));
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		sequence = gs.actionSequences.readPtr(gsf);
		finder.reset(ReadFinder(gsf, gs));
		vdperiod.reset(ReadValueDeterminer(gsf, gs));
	}
};

struct ActionRepeatSequenceOverPeriod : Action {
	ActionSequence* sequence;
	std::unique_ptr<ObjectFinder> finder;
	std::unique_ptr<ValueDeterminer> vdcount;
	std::unique_ptr<ValueDeterminer> vdperiod;
	virtual void run(ServerGameObject* self) override {
		auto vec = finder->eval(self);
		Server::OverPeriodSequence ops;
		ops.actionSequence = sequence;
		ops.executor = self;
		ops.startTime = Server::instance->timeManager.currentTime;
		ops.period = vdperiod->eval(self);
		ops.remainingObjects = std::vector<SrvGORef>(vec.begin(), vec.end());
		ops.numExecutionsDone = 0;
		ops.numTotalExecutions = (int)vdcount->eval(self);
		Server::instance->repeatOverPeriodSequences.push_back(std::move(ops));
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		sequence = gs.actionSequences.readPtr(gsf);
		finder.reset(ReadFinder(gsf, gs));
		vdcount.reset(ReadValueDeterminer(gsf, gs));
		vdperiod.reset(ReadValueDeterminer(gsf, gs));
	}
};

struct ActionDisplayGameTextWindow : Action {
	int gtwIndex;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(ServerGameObject* self) override {
		for (ServerGameObject* player : finder->eval(self))
			Server::instance->showGameTextWindow(player, gtwIndex);
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		gtwIndex = gs.gameTextWindows.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionSetScale : Action {
	std::unique_ptr<ObjectFinder> finder;
	std::unique_ptr<ValueDeterminer> vdx;
	std::unique_ptr<ValueDeterminer> vdy;
	std::unique_ptr<ValueDeterminer> vdz;
	virtual void run(ServerGameObject* self) override {
		Vector3 scale = Vector3(vdx->eval(self), vdy->eval(self), vdz->eval(self));
		for (ServerGameObject* obj : finder->eval(self))
			obj->setScale(scale);
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
		vdx.reset(ReadValueDeterminer(gsf, gs));
		vdy.reset(ReadValueDeterminer(gsf, gs));
		vdz.reset(ReadValueDeterminer(gsf, gs));
	}
};

struct ActionTerminate : Action {
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(ServerGameObject* self) override {
		for (ServerGameObject* obj : finder->eval(self))
			obj->terminate();
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionHideGameTextWindow : Action {
	int gtwIndex;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(ServerGameObject* self) override {
		for (ServerGameObject* player : finder->eval(self))
			Server::instance->hideGameTextWindow(player, gtwIndex);
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		gtwIndex = gs.gameTextWindows.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionHideCurrentGameTextWindow : Action {
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(ServerGameObject* self) override {
		for (ServerGameObject* player : finder->eval(self))
			Server::instance->hideCurrentGameTextWindow(player);
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionDisable : Action {
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(ServerGameObject* self) override {
		for (ServerGameObject* obj : finder->eval(self))
			obj->disable();
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionEnable : Action {
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(ServerGameObject* self) override {
		for (ServerGameObject* obj : finder->eval(self))
			obj->enable();
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionSetSelectable : Action {
	bool value;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(ServerGameObject* self) override {
		for (ServerGameObject* obj : finder->eval(self))
			obj->updateFlags((obj->flags & ~ServerGameObject::fSelectable) | (value ? ServerGameObject::fSelectable : 0));
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		value = gsf.nextInt();
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionSetTargetable : Action {
	bool value;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(ServerGameObject* self) override {
		for (ServerGameObject* obj : finder->eval(self))
			obj->updateFlags((obj->flags & ~ServerGameObject::fTargetable) | (value ? ServerGameObject::fTargetable : 0));
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		value = gsf.nextInt();
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionSetRenderable : Action {
	bool value;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(ServerGameObject* self) override {
		for (ServerGameObject* obj : finder->eval(self))
			obj->updateFlags((obj->flags & ~ServerGameObject::fRenderable) | (value ? ServerGameObject::fRenderable : 0));
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		value = gsf.nextInt();
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionSendPackage : Action {
	GSPackage* package;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(ServerGameObject* self) override {
		for (ServerGameObject* obj : finder->eval(self))
			package->send(obj, self);
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		package = gs.packages.readPtr(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

Action *ReadAction(GSFileParser &gsf, const GameSet &gs)
{
	Action *action;
	const auto name = gsf.nextString();
	switch (Tags::ACTION_tagDict.getTagID(name.c_str())) {
	case Tags::ACTION_TRACE: action = new ActionTrace; break;
	case Tags::ACTION_TRACE_VALUE: action = new ActionTraceValue; break;
	case Tags::ACTION_TRACE_FINDER_RESULTS: action = new ActionTraceFinderResults; break;
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
	case Tags::ACTION_ADD_REACTION: action = new ActionAddReaction; break;
	case Tags::ACTION_REMOVE_REACTION: action = new ActionRemoveReaction; break;
	case Tags::ACTION_SEND_EVENT: action = new ActionSendEvent; break;
	case Tags::ACTION_CREATE_OBJECT_VIA: action = new ActionCreateObjectVia; break;
	case Tags::ACTION_REGISTER_ASSOCIATES: action = new ActionRegisterAssociates; break;
	case Tags::ACTION_DEREGISTER_ASSOCIATES: action = new ActionDeregisterAssociates; break;
	case Tags::ACTION_CLEAR_ASSOCIATES: action = new ActionClearAssociates; break;
	case Tags::ACTION_CONVERT_TO: action = new ActionConvertTo; break;
	case Tags::ACTION_SWITCH_APPEARANCE: action = new ActionSwitchAppearance; break;
	case Tags::ACTION_CONVERT_ACCORDING_TO_TAG: action = new ActionConvertAccordingToTag; break;
	case Tags::ACTION_REPEAT_SEQUENCE: action = new ActionRepeatSequence; break;
	case Tags::ACTION_IDENTIFY_AND_MARK_CLUSTERS: action = new ActionIdentifyAndMarkClusters; break;
	case Tags::ACTION_EXECUTE_SEQUENCE_OVER_PERIOD: action = new ActionExecuteSequenceOverPeriod; break;
	case Tags::ACTION_REPEAT_SEQUENCE_OVER_PERIOD: action = new ActionRepeatSequenceOverPeriod; break;
	case Tags::ACTION_DISPLAY_GAME_TEXT_WINDOW: action = new ActionDisplayGameTextWindow; break;
	case Tags::ACTION_SET_SCALE: action = new ActionSetScale; break;
	case Tags::ACTION_TERMINATE: action = new ActionTerminate; break;
	case Tags::ACTION_HIDE_GAME_TEXT_WINDOW: action = new ActionHideGameTextWindow; break;
	case Tags::ACTION_HIDE_CURRENT_GAME_TEXT_WINDOW: action = new ActionHideCurrentGameTextWindow; break;
	case Tags::ACTION_DISABLE: action = new ActionDisable; break;
	case Tags::ACTION_ENABLE: action = new ActionEnable; break;
	case Tags::ACTION_SET_SELECTABLE: action = new ActionSetSelectable; break;
	case Tags::ACTION_SET_TARGETABLE: action = new ActionSetTargetable; break;
	case Tags::ACTION_SET_RENDERABLE: action = new ActionSetRenderable; break;
	case Tags::ACTION_SEND_PACKAGE: action = new ActionSendPackage; break;
		// Below are ignored actions (that should not affect gameplay very much)
	case Tags::ACTION_STOP_SOUND:
	case Tags::ACTION_PLAY_SOUND:
	case Tags::ACTION_PLAY_SOUND_AT_POSITION:
	case Tags::ACTION_PLAY_SPECIAL_EFFECT:
	case Tags::ACTION_PLAY_SPECIAL_EFFECT_BETWEEN:
	case Tags::ACTION_ATTACH_SPECIAL_EFFECT:
	case Tags::ACTION_ATTACH_LOOPING_SPECIAL_EFFECT:
	case Tags::ACTION_DETACH_LOOPING_SPECIAL_EFFECT:
	case Tags::ACTION_REVEAL_FOG_OF_WAR:
	case Tags::ACTION_COLLAPSING_CIRCLE_ON_MINIMAP:
	case Tags::ACTION_SHOW_BLINKING_DOT_ON_MINIMAP:
	case Tags::ACTION_ENABLE_GAME_INTERFACE:
	case Tags::ACTION_DISABLE_GAME_INTERFACE:
	case Tags::ACTION_SET_ACTIVE_MISSION_OBJECTIVES:
	case Tags::ACTION_SHOW_MISSION_OBJECTIVES_ENTRY:
	case Tags::ACTION_SHOW_MISSION_OBJECTIVES_ENTRY_INACTIVE:
	case Tags::ACTION_HIDE_MISSION_OBJECTIVES_ENTRY:
	case Tags::ACTION_TRACK_OBJECT_POSITION_FROM_MISSION_OBJECTIVES_ENTRY:
	case Tags::ACTION_STOP_INDICATING_POSITION_OF_MISSION_OBJECTIVES_ENTRY:
	case Tags::ACTION_FORCE_PLAY_MUSIC:
	case Tags::ACTION_ADOPT_APPEARANCE_FOR:
	case Tags::ACTION_ADOPT_DEFAULT_APPEARANCE_FOR:
	case Tags::ACTION_ACTIVATE_COMMISSION:
	case Tags::ACTION_DEACTIVATE_COMMISSION:
	case Tags::ACTION_SWITCH_ON_INTENSITY_MAP:
	case Tags::ACTION_FADE_STOP_MUSIC:
	case Tags::ACTION_DECLINE_DIPLOMATIC_OFFER:
	case Tags::ACTION_SEND_CHAT_MESSAGE:
	case Tags::ACTION_MAKE_DIPLOMATIC_OFFER:
	case Tags::ACTION_SET_CHAT_PERSONALITY:
	case Tags::ACTION_UPDATE_USER_PROFILE:
	case Tags::ACTION_UNLOCK_LEVEL:
	case Tags::ACTION_BOOT_LEVEL:
	case Tags::ACTION_EXIT_TO_MAIN_MENU:
	case Tags::ACTION_CONQUER_LEVEL:
	case Tags::ACTION_DISPLAY_LOAD_GAME_MENU:
	case Tags::ACTION_PLAY_MUSIC:
	case Tags::ACTION_PLAY_CLIP:
	case Tags::ACTION_SNAP_CAMERA_TO_POSITION:
	case Tags::ACTION_STORE_CAMERA_POSITION:
	case Tags::ACTION_SNAP_CAMERA_TO_STORED_POSITION:
	case Tags::ACTION_INTERPOLATE_CAMERA_TO_POSITION:
	case Tags::ACTION_PLAY_CAMERA_PATH:
	case Tags::ACTION_STOP_CAMERA_PATH_PLAYBACK:
	case Tags::ACTION_SKIP_CAMERA_PATH_PLAYBACK:
	case Tags::ACTION_INTERPOLATE_CAMERA_TO_STORED_POSITION:
		action = new ActionNop; break;
		//
	default: action = new ActionUnknown(name); break;
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