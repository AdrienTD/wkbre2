// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "actions.h"
#include "finder.h"
#include "../util/GSFileParser.h"
#include "gameset.h"
#include "../util/util.h"
#include "../server.h"
#include "ScriptContext.h"

struct ActionUnknown : Action {
	std::string name, location;
	virtual void run(SrvScriptContext* ctx) override {
		ferr("Unknown action %s at %s!", name.c_str(), location.c_str());
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {}
	ActionUnknown(const std::string& name, const std::string& location) : name(name), location(location) {}
};

struct ActionTrace : Action {
	std::string message;
	virtual void run(SrvScriptContext* ctx) override {
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
	virtual void run(SrvScriptContext* ctx) override {
		printf("Trace Value: %s %f\n", message.c_str(), value->eval(ctx));
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
	virtual void run(SrvScriptContext* ctx) override {
		auto vec = finder->eval(ctx);
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
	virtual void run(SrvScriptContext* ctx) override {
		if (value->eval(ctx) > 0.0f)
			trueList.run(ctx);
		else
			falseList.run(ctx);
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
	virtual void run(SrvScriptContext* ctx) override {
		float val = value->eval(ctx);
		for (ServerGameObject *obj : finder->eval(ctx)) {
			obj->setItem(item, val);
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
	virtual void run(SrvScriptContext* ctx) override {
		float val = value->eval(ctx);
		for (ServerGameObject *obj : finder->eval(ctx)) {
			obj->setItem(item, obj->getItem(item) + val);
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
	virtual void run(SrvScriptContext* ctx) override {
		float val = value->eval(ctx);
		for (ServerGameObject *obj : finder->eval(ctx)) {
			obj->setItem(item, obj->getItem(item) - val);
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
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject* obj : finder->eval(ctx)) {
			auto _1 = ctx->sequenceExecutor.change(ctx->self.get());
			auto _2 = ctx->self.change(obj);
			sequence->run(ctx);
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
	virtual void run(SrvScriptContext* ctx) override {
		Server::DelayedSequence ds;
		ds.executor = ctx->self.get();
		ds.actionSequence = sequence;
		//ds.selfs = finder->eval(ctx);
		for (ServerGameObject *obj : finder->eval(ctx))
			ds.selfs.emplace_back(obj);
		game_time_t atTime = Server::instance->timeManager.currentTime + delay->eval(ctx);
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
	virtual void run(SrvScriptContext* ctx) override {
		auto objs = finder->eval(ctx);
		for (ServerGameObject *obj : objs) {
			if (!obj->orderConfig.getCurrentOrder())
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
	virtual void run(SrvScriptContext* ctx) override {
		if (!actionseq.actionList.empty()) {
			int x = rand() % actionseq.actionList.size();
			actionseq.actionList[x]->run(ctx);
		}
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		actionseq.init(gsf, gs, "END_EXECUTE_ONE_AT_RANDOM");
	}
};

struct ActionTerminateThisTask : Action {
	virtual void run(SrvScriptContext* ctx) override {
		if (Order *order = ctx->self.get()->orderConfig.getCurrentOrder())
			order->getCurrentTask()->terminate();
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {}
};

struct ActionTerminateThisOrder : Action {
	virtual void run(SrvScriptContext* ctx) override {
		if (Order *order = ctx->self.get()->orderConfig.getCurrentOrder())
			order->terminate();
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {}
};

struct ActionTerminateTask : Action {
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject* obj : finder->eval(ctx))
			if (Order* order = obj->orderConfig.getCurrentOrder())
				order->getCurrentTask()->terminate();
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionTerminateOrder : Action {
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject* obj : finder->eval(ctx))
			if (Order* order = obj->orderConfig.getCurrentOrder())
				order->terminate();
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionTransferControl : Action {
	std::unique_ptr<ObjectFinder> togiveFinder, recipientFinder;
	virtual void run(SrvScriptContext* ctx) override {
		auto objs = togiveFinder->eval(ctx);
		ServerGameObject *recipient = recipientFinder->getFirst(ctx);
		if (!recipient) {
			printf("WARNING: Attempt to transfer %zu objects to non-existent parent!\n", objs.size());
			return;
		}
		for (ServerGameObject* obj : objs) {
			obj->setParent(recipient);
			obj->sendEvent(Tags::PDEVENT_ON_CONTROL_TRANSFERRED);
			recipient->sendEvent(Tags::PDEVENT_ON_SUBORDINATE_RECEIVED, obj);
		}
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
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject *obj : finder->eval(ctx)) {
			oabp->assignTo(obj, ctx, ctx->self.get());
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
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject *obj : finder->eval(ctx)) {
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
	virtual void run(SrvScriptContext* ctx) override {
		auto &alias = Server::instance->aliases[aliasIndex];
		for (ServerGameObject *obj : finder->eval(ctx)) {
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
	virtual void run(SrvScriptContext* ctx) override {
		auto &alias = Server::instance->aliases[aliasIndex];
		for (ServerGameObject *obj : finder->eval(ctx)) {
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
	virtual void run(SrvScriptContext* ctx) override {
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
	virtual void run(SrvScriptContext* ctx) override {
		for(ServerGameObject *obj : finder->eval(ctx))
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
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject *obj : finder->eval(ctx))
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
	virtual void run(SrvScriptContext* ctx) override {
		if (event == -1) return;
		for (ServerGameObject *obj : finder->eval(ctx))
			obj->sendEvent(event, ctx->self.get());
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		event = gs.events.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionCreateObjectVia : Action {
	ObjectCreation *info;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject *obj : finder->eval(ctx))
			info->run(obj, ctx);
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		info = gs.objectCreations.readPtr(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionRegisterAssociates : Action {
	std::unique_ptr<ObjectFinder> f_aors, f_ated;
	int category;
	virtual void run(SrvScriptContext* ctx) override {
		auto aors = f_aors->eval(ctx);
		auto ated = f_ated->eval(ctx);
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
	virtual void run(SrvScriptContext* ctx) override {
		auto aors = f_aors->eval(ctx);
		auto ated = f_ated->eval(ctx);
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
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject *obj : finder->eval(ctx))
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
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject* obj : finder->eval(ctx)) {
			obj->convertTo(postbp);
			obj->sendEvent(Tags::PDEVENT_ON_CONVERSION_END);
		}
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		postbp = gs.readObjBlueprintPtr(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionNop : Action {
	virtual void run(SrvScriptContext* ctx) override {}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {}
};

struct ActionSwitchAppearance : Action {
	int appear;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject *obj : finder->eval(ctx))
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
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject *obj : finder->eval(ctx)) {
			auto it = obj->blueprint->mappedTypeTags.find(typeTag);
			if (it != obj->blueprint->mappedTypeTags.end()) {
				obj->convertTo(it->second);
				obj->sendEvent(Tags::PDEVENT_ON_CONVERSION_END);
			}
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
	virtual void run(SrvScriptContext* ctx) override {
		int count = (int)vdcount->eval(ctx);
		for (ServerGameObject* obj : finder->eval(ctx)) {
			auto _1 = ctx->sequenceExecutor.change(ctx->self.get());
			auto _2 = ctx->self.change(obj);
			for (int i = 0; i < count; i++)
				sequence->run(ctx);
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
	virtual void run(SrvScriptContext* ctx) override {
		ServerGameObject* player = ctx->self.get()->getPlayer();
		auto gl = finder->eval(ctx);
		float rad = vcr->eval(ctx);
		float fmr = vmr->eval(ctx);
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
					auto _ = ctx->self.change(p);
					clrat += vir->eval(ctx);
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
	virtual void run(SrvScriptContext* ctx) override {
		auto vec = finder->eval(ctx);
		Server::OverPeriodSequence ops;
		ops.actionSequence = sequence;
		ops.executor = ctx->self.get();
		ops.startTime = Server::instance->timeManager.currentTime;
		ops.period = vdperiod->eval(ctx);
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
	virtual void run(SrvScriptContext* ctx) override {
		auto vec = finder->eval(ctx);
		Server::OverPeriodSequence ops;
		ops.actionSequence = sequence;
		ops.executor = ctx->self.get();
		ops.startTime = Server::instance->timeManager.currentTime;
		ops.period = vdperiod->eval(ctx);
		ops.remainingObjects = std::vector<SrvGORef>(vec.begin(), vec.end());
		ops.numExecutionsDone = 0;
		ops.numTotalExecutions = (int)vdcount->eval(ctx);
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
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject* player : finder->eval(ctx))
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
	virtual void run(SrvScriptContext* ctx) override {
		Vector3 scale = Vector3(vdx->eval(ctx), vdy->eval(ctx), vdz->eval(ctx));
		for (ServerGameObject* obj : finder->eval(ctx))
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
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject* obj : finder->eval(ctx))
			obj->terminate();
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionHideGameTextWindow : Action {
	int gtwIndex;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject* player : finder->eval(ctx))
			Server::instance->hideGameTextWindow(player, gtwIndex);
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		gtwIndex = gs.gameTextWindows.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionHideCurrentGameTextWindow : Action {
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject* player : finder->eval(ctx))
			Server::instance->hideCurrentGameTextWindow(player);
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionDisable : Action {
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject* obj : finder->eval(ctx))
			obj->disable();
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionEnable : Action {
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject* obj : finder->eval(ctx))
			obj->enable();
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionSetSelectable : Action {
	bool value;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject* obj : finder->eval(ctx))
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
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject* obj : finder->eval(ctx))
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
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject* obj : finder->eval(ctx))
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
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject* obj : finder->eval(ctx))
			package->send(obj, ctx);
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		package = gs.packages.readPtr(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionChangeReactionProfile : Action {
	int mode;
	Reaction* reaction;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(SrvScriptContext* ctx) override {
		if (mode == 0)
			for (ServerGameObject* obj : finder->eval(ctx))
				obj->individualReactions.insert(reaction);
		else if (mode == 1)
			for (ServerGameObject* obj : finder->eval(ctx))
				obj->individualReactions.erase(reaction);
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		auto smode = gsf.nextString();
		mode = 0;
		if (smode == "ADD") mode = 0;
		else if (smode == "REMOVE") mode = 1;
		reaction = gs.reactions.readPtr(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionSwitchCommon : Action {
	struct SwitchCase {
		std::unique_ptr<ValueDeterminer> value;
		ActionSequence actions;
	};
	std::vector<SwitchCase> cases;
	void parseCases(GSFileParser& gsf, GameSet& gs, const std::string& endtag) {
		gsf.advanceLine();
		while (!gsf.eol) {
			auto tag = gsf.nextTag();
			if (tag == endtag)
				break;
			else if (tag == "CASE") {
				cases.emplace_back();
				auto& newcase = cases.back();
				newcase.value.reset(ReadValueDeterminer(gsf, gs));
				newcase.actions.init(gsf, gs, "END_CASE");
			}
			gsf.advanceLine();
		}
	}
};

struct ActionSwitchCondition : ActionSwitchCommon {
	std::unique_ptr<ValueDeterminer> valuedet;
	virtual void run(SrvScriptContext* ctx) override {
		float value = valuedet->eval(ctx);
		for (auto& scase : cases)
			if (scase.value->eval(ctx) == value)
				scase.actions.run(ctx);
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		valuedet.reset(ReadValueDeterminer(gsf, gs));
		parseCases(gsf, gs, "END_SWITCH_CONDITION");
	}
};

struct ActionSwitchHighest : ActionSwitchCommon {
	virtual void run(SrvScriptContext* ctx) override {
		float maxvalue = -INFINITY;
		SwitchCase* maxcase = nullptr;
		for (auto& scase : cases) {
			float value = scase.value->eval(ctx);
			if (value > maxvalue) {
				maxvalue = value;
				maxcase = &scase;
			}
		}
		maxcase->actions.run(ctx);
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		parseCases(gsf, gs, "END_SWITCH_HIGHEST");
	}
};

struct ActionPlayClip : Action {
	int clip;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(SrvScriptContext* ctx) override {
		auto _ = ctx->self.change(ctx->self.get()->getPlayer());
		Server::instance->gameSet->clips[clip].postClipSequence.run(ctx);
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		clip = gs.clips.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionStoreCameraPosition : Action {
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject* obj : finder->eval(ctx))
			ctx->server->storeCameraPosition(obj);
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionSnapCameraToStoredPosition : Action {
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject* obj : finder->eval(ctx))
			ctx->server->restoreCameraPosition(obj);
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionPlayCameraPath : Action {
	int camPathIndex;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject* obj : finder->eval(ctx))
			ctx->server->playCameraPath(obj, camPathIndex);
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		camPathIndex = gs.cameraPaths.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionStopCameraPathPlayback : Action {
	std::unique_ptr<ObjectFinder> finder;
	bool skipActions = false;
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject* obj : finder->eval(ctx))
			ctx->server->stopCameraPath(obj, skipActions);
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
		skipActions = gsf.nextString() == "SKIP_ACTIONS";
	}
};

struct ActionCreateFormation : Action {
	GameObjBlueprint* formationType;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(SrvScriptContext* ctx) override {
		ServerGameObject* formation = ctx->server->createObject(formationType);
		formation->setParent(ctx->self.get());
		for (ServerGameObject* obj : finder->eval(ctx))
			obj->setParent(formation);
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		formationType = gs.objBlueprints[Tags::GAMEOBJCLASS_FORMATION].readPtr(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionSetIndexedItem : Action {
	int item;
	std::unique_ptr<ValueDeterminer> index;
	std::unique_ptr<ObjectFinder> finder;
	std::unique_ptr<ValueDeterminer> value;
	virtual void run(SrvScriptContext* ctx) override {
		int x = (int)index->eval(ctx);
		float val = value->eval(ctx);
		for (ServerGameObject* obj : finder->eval(ctx)) {
			obj->setIndexedItem(item, x, val);
		}
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		item = gs.items.readIndex(gsf);
		index.reset(ReadValueDeterminer(gsf, gs));
		finder.reset(ReadFinder(gsf, gs));
		value.reset(ReadValueDeterminer(gsf, gs));
	}
};

struct ActionIncreaseIndexedItem : Action {
	int item;
	std::unique_ptr<ValueDeterminer> index;
	std::unique_ptr<ObjectFinder> finder;
	std::unique_ptr<ValueDeterminer> value;
	virtual void run(SrvScriptContext* ctx) override {
		int x = (int)index->eval(ctx);
		float val = value->eval(ctx);
		for (ServerGameObject* obj : finder->eval(ctx)) {
			obj->setIndexedItem(item, x, obj->getIndexedItem(item, x) + val);
		}
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		item = gs.items.readIndex(gsf);
		index.reset(ReadValueDeterminer(gsf, gs));
		finder.reset(ReadFinder(gsf, gs));
		value.reset(ReadValueDeterminer(gsf, gs));
	}
};

struct ActionDecreaseIndexedItem : Action {
	int item;
	std::unique_ptr<ValueDeterminer> index;
	std::unique_ptr<ObjectFinder> finder;
	std::unique_ptr<ValueDeterminer> value;
	virtual void run(SrvScriptContext* ctx) override {
		int x = (int)index->eval(ctx);
		float val = value->eval(ctx);
		for (ServerGameObject* obj : finder->eval(ctx)) {
			obj->setIndexedItem(item, x, obj->getIndexedItem(item, x) - val);
		}
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		item = gs.items.readIndex(gsf);
		index.reset(ReadValueDeterminer(gsf, gs));
		finder.reset(ReadFinder(gsf, gs));
		value.reset(ReadValueDeterminer(gsf, gs));
	}
};

struct ActionCopyFacingOf : Action {
	std::unique_ptr<ObjectFinder> objTo, objFrom;
	virtual void run(SrvScriptContext* ctx) override {
		if (auto* from = objFrom->getFirst(ctx)) {
			for (auto* to : objTo->eval(ctx))
				to->setOrientation(from->orientation);
		}
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		objTo.reset(ReadFinder(gsf, gs));
		objFrom.reset(ReadFinder(gsf, gs));
	}
};

struct ActionPlaySound : Action {
	int soundTag;
	std::unique_ptr<ObjectFinder> objFrom, objTo;
	virtual void run(SrvScriptContext* ctx) override {
		// TODO: Check for multiple objects
		if (auto* from = objFrom->getFirst(ctx)) {
			if (auto* to = objTo->getFirst(ctx)) {
				from->playSoundAtObject(soundTag, to);
			}
		}
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		soundTag = gs.soundTags.readIndex(gsf);
		objFrom.reset(ReadFinder(gsf, gs));
		objTo.reset(ReadFinder(gsf, gs));
	}
};

struct ActionPlayMusic : Action {
	int musicTag;
	std::unique_ptr<ObjectFinder> fPlayer;
	virtual void run(SrvScriptContext* ctx) override {
		for (auto* player : fPlayer->eval(ctx)) {
			ctx->server->playMusic(player, musicTag);
		}
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		musicTag = gs.musicTags.readIndex(gsf);
		fPlayer.reset(ReadFinder(gsf, gs));
	}
};

struct ActionTeleport : Action {
	std::unique_ptr<ObjectFinder> finder;
	std::unique_ptr<PositionDeterminer> pos;
	virtual void run(SrvScriptContext* ctx) override {
		auto op = pos->eval(ctx);
		for (ServerGameObject* obj : finder->eval(ctx)) {
			obj->orderConfig.cancelAllOrders();
			obj->setPosition(op.position);
			obj->setOrientation(op.rotation);
		}
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
		pos.reset(PositionDeterminer::createFrom(gsf, gs));
	}
};

struct ActionChangeDiplomaticStatus : Action {
	std::unique_ptr<ObjectFinder> fPlayer1;
	std::unique_ptr<ObjectFinder> fPlayer2;
	int nextStatus;
	virtual void run(SrvScriptContext* ctx) override {
		ServerGameObject* player1 = fPlayer1->getFirst(ctx);
		ServerGameObject* player2 = fPlayer2->getFirst(ctx);
		if (player1 && player2) {
			Server::instance->setDiplomaticStatus(player1, player2, nextStatus);
		}
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		fPlayer1.reset(ReadFinder(gsf, gs));
		fPlayer2.reset(ReadFinder(gsf, gs));
		nextStatus = gs.diplomaticStatuses.readIndex(gsf);
	}
};

struct ActionSinkAndRemove : Action {
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject* obj : finder->eval(ctx)) {
			Server::instance->deleteObject(obj);
		}
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionPlaySoundAtPosition_WKO : Action {
	int soundTag;
	std::unique_ptr<ObjectFinder> objPlayers;
	virtual void run(SrvScriptContext* ctx) override {
		auto* from = ctx->self.get();
		from->playSoundAtPosition(soundTag, from->position);
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		soundTag = gs.soundTags.readIndex(gsf);
		objPlayers.reset(ReadFinder(gsf, gs));
	}
};

struct ActionPlaySoundAtPosition_Battles : Action {
	int soundTag;
	std::unique_ptr<PositionDeterminer> pPosition;
	virtual void run(SrvScriptContext* ctx) override {
		auto* from = ctx->self.get();
		from->playSoundAtPosition(soundTag, pPosition->eval(ctx).position);
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		soundTag = gs.soundTags.readIndex(gsf);
		pPosition.reset(PositionDeterminer::createFrom(gsf, gs));
	}
};

struct ActionFaceTowards : Action {
	std::unique_ptr<ObjectFinder> fObjects, fTarget;
	virtual void run(SrvScriptContext* ctx) override {
		if (auto* target = fTarget->getFirst(ctx)) {
			const Vector3& b = target->position;
			for (auto* obj : fObjects->eval(ctx)) {
				const Vector3& a = obj->position;
				Vector3 dir = b - a;
				obj->setOrientation(Vector3(0.0f, std::atan2(dir.x, -dir.z), 0.0f));
				//Vector3 d{ sinf(op.rotation.y), 0.0f, -cosf(op.rotation.y) };
			}
		}
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		fObjects.reset(ReadFinder(gsf, gs));
		fTarget.reset(ReadFinder(gsf, gs));
	}
};

struct ActionSnapCameraToPosition : Action {
	std::unique_ptr<PositionDeterminer> posdet;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(SrvScriptContext* ctx) override {
		auto posori = posdet->eval(ctx);
		for (ServerGameObject* obj : finder->eval(ctx))
			ctx->server->snapCameraPosition(obj, posori.position, posori.rotation);
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		posdet.reset(PositionDeterminer::createFrom(gsf, gs));
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionReevaluateTaskTarget :Action {
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(SrvScriptContext* ctx) override {
		for (auto* obj : finder->eval(ctx)) {
			if (auto* order = obj->orderConfig.getCurrentOrder()) {
				if (auto* task = order->getCurrentTask()) {
					task->reevaluateTarget();
				}
			}
		}
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionPlaySpecialEffect :Action {
	int sfxTag;
	std::unique_ptr<ObjectFinder> fObject;
	std::unique_ptr<PositionDeterminer> pPosition;
	virtual void run(SrvScriptContext* ctx) override {
		if (auto* obj = fObject->getFirst(ctx)) {
			auto posori = pPosition->eval(ctx);
			obj->playSpecialEffectAt(sfxTag, posori.position);
		}
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		sfxTag = gs.specialEffectTags.readIndex(gsf);
		fObject.reset(ReadFinder(gsf, gs));
		pPosition.reset(PositionDeterminer::createFrom(gsf, gs));
	}
};

struct ActionPlaySpecialEffectBetween :Action {
	int sfxTag;
	std::unique_ptr<ObjectFinder> fObject;
	std::unique_ptr<PositionDeterminer> pPosition1, pPosition2;
	virtual void run(SrvScriptContext* ctx) override {
		if (auto* obj = fObject->getFirst(ctx)) {
			Vector3 pos1 = pPosition1->eval(ctx).position;
			Vector3 pos2 = pPosition2->eval(ctx).position;
			obj->playSpecialEffectBetween(sfxTag, pos1, pos2);
		}
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		sfxTag = gs.specialEffectTags.readIndex(gsf);
		fObject.reset(ReadFinder(gsf, gs));
		pPosition1.reset(PositionDeterminer::createFrom(gsf, gs));
		pPosition2.reset(PositionDeterminer::createFrom(gsf, gs));
	}
};

struct ActionAttachSpecialEffect :Action {
	int sfxTag;
	std::unique_ptr<ObjectFinder> fObject;
	std::unique_ptr<ObjectFinder> fTarget;
	virtual void run(SrvScriptContext* ctx) override {
		if (auto* obj = fObject->getFirst(ctx)) {
			for (auto* target : fTarget->eval(ctx)) {
				obj->playAttachedSpecialEffect(sfxTag, target);
			}
		}
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		sfxTag = gs.specialEffectTags.readIndex(gsf);
		fObject.reset(ReadFinder(gsf, gs));
		fTarget.reset(ReadFinder(gsf, gs));
	}
};

struct ActionCreateObject : Action {
	GameObjBlueprint* objbp;
	std::unique_ptr<PositionDeterminer> pPosition;
	virtual void run(SrvScriptContext* ctx) override {
		ServerGameObject* obj = ctx->server->createObject(objbp);
		obj->setParent(ctx->self.get()->getPlayer());
		auto posori = pPosition->eval(ctx);
		obj->setPosition(posori.position);
		obj->setOrientation(posori.rotation);
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		objbp = gs.readObjBlueprintPtr(gsf);
		pPosition.reset(PositionDeterminer::createFrom(gsf, gs));
	}

};

struct ActionAttachLoopingSpecialEffect : Action {
	int sfxTag;
	std::unique_ptr<ObjectFinder> fObject;
	std::unique_ptr<PositionDeterminer> pPosition;
	virtual void run(SrvScriptContext* ctx) override {
		for (auto* obj : fObject->eval(ctx)) {
			auto tvSelf = ctx->self.change(obj);
			obj->attachLoopingSpecialEffect(sfxTag, pPosition->eval(ctx).position);
		}
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		sfxTag = gs.specialEffectTags.readIndex(gsf);
		fObject.reset(ReadFinder(gsf, gs));
		pPosition.reset(PositionDeterminer::createFrom(gsf, gs));
	}
};

struct ActionDetachLoopingSpecialEffect : Action {
	int sfxTag;
	std::unique_ptr<ObjectFinder> fObject;
	virtual void run(SrvScriptContext* ctx) override {
		for (auto* obj : fObject->eval(ctx)) {
			obj->detachLoopingSpecialEffect(sfxTag);
		}
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		sfxTag = gs.specialEffectTags.readIndex(gsf);
		fObject.reset(ReadFinder(gsf, gs));
	}
};

struct ActionSkipCameraPathPlayback : Action {
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject* obj : finder->eval(ctx))
			ctx->server->skipCameraPath(obj);
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionActivatePlan : Action {
	int planBlueprint;
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject* obj : finder->eval(ctx))
			obj->aiController.activatePlan(planBlueprint);
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		planBlueprint = gs.plans.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionRegisterWorkOrder : Action {
	int gsUnitFinder;
	const WorkOrder* workOrder;
	std::unique_ptr<ObjectFinder> cityFinder;
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject* city : cityFinder->eval(ctx)) {
			city->getPlayer()->aiController.registerWorkOrder(city, Server::instance->gameSet->objectFinderDefinitions[gsUnitFinder], workOrder);
		}
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		auto str = gsf.nextString();
		if (str != "FINDER_RESULTS")
			ferr("REGISTER_WORK_ORDER must be followed by a FINDER_RESULTS!");
		gsUnitFinder = gs.objectFinderDefinitions.readIndex(gsf);
		workOrder = gs.workOrders.readPtr(gsf);
		cityFinder.reset(ReadFinder(gsf, gs));
	}
};

struct ActionAbandonPlan : Action {
	std::unique_ptr<ObjectFinder> finder;
	virtual void run(SrvScriptContext* ctx) override {
		for (ServerGameObject* obj : finder->eval(ctx))
			obj->aiController.abandonPlan();
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
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
	case Tags::ACTION_TERMINATE_TASK: action = new ActionTerminateTask; break;
	case Tags::ACTION_TERMINATE_ORDER: action = new ActionTerminateOrder; break;
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
	case Tags::ACTION_CHANGE_REACTION_PROFILE: action = new ActionChangeReactionProfile; break;
	case Tags::ACTION_SWITCH_CONDITION: action = new ActionSwitchCondition; break;
	case Tags::ACTION_SWITCH_HIGHEST: action = new ActionSwitchHighest; break;
	case Tags::ACTION_PLAY_CLIP: action = new ActionPlayClip; break;
	case Tags::ACTION_STORE_CAMERA_POSITION: action = new ActionStoreCameraPosition; break;
	case Tags::ACTION_SNAP_CAMERA_TO_STORED_POSITION: action = new ActionSnapCameraToStoredPosition; break;
	case Tags::ACTION_PLAY_CAMERA_PATH: action = new ActionPlayCameraPath; break;
	case Tags::ACTION_STOP_CAMERA_PATH_PLAYBACK: action = new ActionStopCameraPathPlayback; break;
	case Tags::ACTION_CREATE_FORMATION: action = new ActionCreateFormation; break;
	case Tags::ACTION_CREATE_FORMATION_REFERENCE: action = new ActionCreateFormation; break;
	case Tags::ACTION_SET_INDEXED_ITEM: action = new ActionSetIndexedItem; break;
	case Tags::ACTION_INCREASE_INDEXED_ITEM: action = new ActionIncreaseIndexedItem; break;
	case Tags::ACTION_DECREASE_INDEXED_ITEM: action = new ActionDecreaseIndexedItem; break;
	case Tags::ACTION_COPY_FACING_OF: action = new ActionCopyFacingOf; break;
	case Tags::ACTION_PLAY_SOUND: action = new ActionPlaySound; break;
	case Tags::ACTION_PLAY_MUSIC: action = new ActionPlayMusic; break;
	case Tags::ACTION_FORCE_PLAY_MUSIC: action = new ActionPlayMusic; break;
	case Tags::ACTION_TELEPORT: action = new ActionTeleport; break;
	case Tags::ACTION_CHANGE_DIPLOMATIC_STATUS: action = new ActionChangeDiplomaticStatus; break;
	case Tags::ACTION_SINK_AND_REMOVE: action = new ActionSinkAndRemove; break;
	case Tags::ACTION_PLAY_SOUND_AT_POSITION: if (gs.version >= gs.GSVERSION_WKBATTLES) action = new ActionPlaySoundAtPosition_Battles; else action = new ActionPlaySoundAtPosition_WKO; break;
	case Tags::ACTION_FACE_TOWARDS: action = new ActionFaceTowards; break;
	case Tags::ACTION_SNAP_CAMERA_TO_POSITION: action = new ActionSnapCameraToPosition; break;
	case Tags::ACTION_REEVALUATE_TASK_TARGET: action = new ActionReevaluateTaskTarget; break;
	case Tags::ACTION_PLAY_SPECIAL_EFFECT: action = new ActionPlaySpecialEffect; break;
	case Tags::ACTION_PLAY_SPECIAL_EFFECT_BETWEEN: action = new ActionPlaySpecialEffectBetween; break;
	case Tags::ACTION_ATTACH_SPECIAL_EFFECT: action = new ActionAttachSpecialEffect; break;
	case Tags::ACTION_CREATE_OBJECT: action = new ActionCreateObject; break;
	case Tags::ACTION_ATTACH_LOOPING_SPECIAL_EFFECT: action = new ActionAttachLoopingSpecialEffect; break;
	case Tags::ACTION_DETACH_LOOPING_SPECIAL_EFFECT: action = new ActionDetachLoopingSpecialEffect; break;
	case Tags::ACTION_SKIP_CAMERA_PATH_PLAYBACK: action = new ActionSkipCameraPathPlayback; break;
	case Tags::ACTION_ACTIVATE_PLAN: action = new ActionActivatePlan; break;
	case Tags::ACTION_REGISTER_WORK_ORDER: action = new ActionRegisterWorkOrder; break;
	case Tags::ACTION_ABANDON_PLAN: action = new ActionAbandonPlan; break;
		// Below are ignored actions (that should not affect gameplay very much)
	case Tags::ACTION_STOP_SOUND:
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
	case Tags::ACTION_INDICATE_POSITION_OF_MISSION_OBJECTIVES_ENTRY:
	case Tags::ACTION_STOP_INDICATING_POSITION_OF_MISSION_OBJECTIVES_ENTRY:
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
	case Tags::ACTION_INTERPOLATE_CAMERA_TO_POSITION:
	case Tags::ACTION_INTERPOLATE_CAMERA_TO_STORED_POSITION:
	case Tags::ACTION_LOCK_TIME:
	case Tags::ACTION_UNLOCK_TIME:
	case Tags::ACTION_ENABLE_DIPLOMATIC_REPORT_WINDOW:
	case Tags::ACTION_DISABLE_DIPLOMATIC_REPORT_WINDOW:
	case Tags::ACTION_ENABLE_TRIBUTES_WINDOW:
	case Tags::ACTION_DISABLE_TRIBUTES_WINDOW:
	case Tags::ACTION_SET_RECONNAISSANCE:
	case Tags::ACTION_REMOVE_MULTIPLAYER_PLAYER:
	case Tags::ACTION_RESERVE_TILE_FOR_CONSTRUCTION:
	case Tags::ACTION_RESERVE_SPACE_FOR_CONSTRUCTION:
		action = new ActionNop; break;
		//
	default: action = new ActionUnknown(name, gsf.locate()); printf("WARNING: Unknown ACTION %s at %s\n", name.c_str(), gsf.locate().c_str()); break;
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

void ActionSequence::run(ServerGameObject* self) const {
	SrvScriptContext ctx(Server::instance, self);
	run(&ctx);
}
