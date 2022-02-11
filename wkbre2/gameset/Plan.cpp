// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "Plan.h"
#include "../util/GSFileParser.h"
#include "gameset.h"
#include "../util/util.h"
#include "../server.h"
#include "finder.h"
#include "ScriptContext.h"

struct PFNUnknown : PlanNodeBlueprint {
	struct State : PlanNodeState {
	};
	std::string name;

	virtual void parse(GSFileParser& gsf, const GameSet& gs) {
	}
	virtual PlanNodeState* createState() override {
		return new State;
	}
	virtual void reset(PlanNodeState* state) override {
	}
	virtual bool execute(PlanNodeState* state, SrvScriptContext* ctx) override {
		ferr("Unknown plan node %s", name.c_str());
		return true;
	}
};

struct PFNAction : PlanNodeBlueprint {
	struct State : PlanNodeState {
	};
	Action* action;

	virtual void parse(GSFileParser& gsf, const GameSet& gs) {
		action = ReadAction(gsf, gs);
	}
	virtual PlanNodeState* createState() override {
		return new State;
	}
	virtual void reset(PlanNodeState* state) override {
	}
	virtual bool execute(PlanNodeState* state, SrvScriptContext* ctx) override {
		action->run(ctx);
		return true;
	}
};

struct PFNWaitFor : PlanNodeBlueprint {
	struct State : PlanNodeState {
		float endTime;
	};
	std::unique_ptr<ValueDeterminer> waitTime;
	
	virtual void parse(GSFileParser& gsf, const GameSet& gs) {
		waitTime.reset(ReadValueDeterminer(gsf, gs));
	}
	virtual PlanNodeState* createState() override {
		return new State;
	}
	virtual void reset(PlanNodeState* state) override {
		((State*)state)->endTime = Server::instance->timeManager.currentTime;
	}
	virtual bool execute(PlanNodeState* state, SrvScriptContext* ctx) override {
		return Server::instance->timeManager.currentTime >= (((State*)state)->endTime + waitTime->eval(ctx));
	}
};

struct PFNWaitUntil : PlanNodeBlueprint {
	struct State : PlanNodeState {
	};
	std::unique_ptr<ValueDeterminer> value;
	
	virtual void parse(GSFileParser& gsf, const GameSet& gs) {
		value.reset(ReadValueDeterminer(gsf, gs));
	}
	virtual PlanNodeState* createState() override {
		return new State;
	}
	virtual void reset(PlanNodeState* state) override {
	}
	virtual bool execute(PlanNodeState* state, SrvScriptContext* ctx) override {
		return value->booleval(ctx);
	}
};

struct PFNStartLoop : PlanNodeBlueprint {
	struct State : PlanNodeState {
		PlanNodeSequence::State seqState;
	};
	PlanNodeSequence sequence;
	std::unique_ptr<ValueDeterminer> loopBreakCondition;

	virtual void parse(GSFileParser& gsf, const GameSet& gs) {
		sequence.parse(gsf, gs, "END_LOOP");
		if (gsf.nextString() == "IF")
			loopBreakCondition.reset(ReadValueDeterminer(gsf, gs));
	}
	virtual PlanNodeState* createState() override {
		State* s = new State;
		s->seqState = sequence.createState();
		return s;
	}
	virtual void reset(PlanNodeState* state) override {
		sequence.reset(&((State*)state)->seqState);
	}
	virtual bool execute(PlanNodeState* state, SrvScriptContext* ctx) override {
		bool done = sequence.execute(&((State*)state)->seqState, ctx);
		if (done) {
			if (loopBreakCondition && loopBreakCondition->booleval(ctx))
				return true;
			reset(state);
			return false;
		}
		else
			return false;
	}
};

struct PFNDeployArmyFrom : PlanNodeBlueprint {
	struct State : PlanNodeState {
	};
	const ArmyCreationSchedule* schedule;
	std::unique_ptr<ObjectFinder> finder;

	virtual void parse(GSFileParser& gsf, const GameSet& gs) {
		schedule = gs.armyCreationSchedules.readPtr(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
	virtual PlanNodeState* createState() override {
		return new State;
	}
	virtual void reset(PlanNodeState* state) override {
	}
	virtual bool execute(PlanNodeState* state, SrvScriptContext* ctx) override {
		// very simple implementation for testing
		// TODO redo it
		printf("Army deployed using \"%s\" schedule!\n", ctx->server->gameSet->armyCreationSchedules.getString(schedule).c_str());

		if (ServerGameObject* settlement = finder->getFirst(ctx)) {
			// find the buildings to spawn the unit from
			// TODO: spawn unit from the correct building
			std::vector<ServerGameObject*> buildingCandidates;
			for (auto& ct : settlement->children) {
				if ((ct.first & 63) == Tags::GAMEOBJCLASS_BUILDING)
					for(CommonGameObject* obj : ct.second)
						buildingCandidates.push_back((ServerGameObject*)obj);
			}
			// no buildings -> units can no longer spawn
			if (buildingCandidates.empty())
				return true;

			ServerGameObject* army = ctx->server->createObject(schedule->armyType);
			army->setParent(settlement->getPlayer());
			for (auto& spawn : schedule->spawnCharacters) {
				ServerGameObject* building = buildingCandidates[rand() % buildingCandidates.size()];
				// create the units
				auto _ = ctx->self.change(army);
				int numUnits = (int)spawn.count->eval(ctx); // Not sure what should be the SELF
				numUnits = std::min(numUnits, 7); // prevent formations to be built until they are implemented
				for (int i = 0; i < numUnits; i++) {
					ServerGameObject* unit = ctx->server->createObject(spawn.type);
					unit->setParent(army);
					unit->setPosition(building->position);
				}
			}
			schedule->armyReadySequence.run(army);
		}
		return true;
	}
};

struct PFNNoOperation : PlanNodeBlueprint {
	struct State : PlanNodeState {
	};
	std::string name;

	virtual void parse(GSFileParser& gsf, const GameSet& gs) {
	}
	virtual PlanNodeState* createState() override {
		return new State;
	}
	virtual void reset(PlanNodeState* state) override {
	}
	virtual bool execute(PlanNodeState* state, SrvScriptContext* ctx) override {
		printf("Ignored plan node %s\n", name.c_str());
		return true;
	}
};

struct PFNChooseAtRandom : PlanNodeBlueprint {
	struct State : PlanNodeState {
		PlanNodeSequence::State seqState;
		uint32_t index;
	};
	PlanNodeSequence sequence;

	virtual void parse(GSFileParser& gsf, const GameSet& gs) {
		sequence.parse(gsf, gs, "END_CHOOSE_AT_RANDOM");
	}
	virtual PlanNodeState* createState() override {
		State* s = new State;
		s->seqState = sequence.createState();
		return s;
	}
	virtual void reset(PlanNodeState* state) override {
		State* s = (State*)state;
		s->index = rand() % sequence.nodes.size();
		s->seqState.currentNode = s->index;
		sequence.nodes[s->index]->reset(s->seqState.nodeStates[s->index]);
	}
	virtual bool execute(PlanNodeState* state, SrvScriptContext* ctx) override {
		State* s = (State*)state;
		return sequence.nodes[s->index]->execute(s->seqState.nodeStates[s->index], ctx);
	}
};

struct PFNRegisterWorkOrder : PlanNodeBlueprint {
	struct State : PlanNodeState {
	};
	int gsUnitFinder;
	const WorkOrder* workOrder;
	std::unique_ptr<ObjectFinder> cityFinder;

	virtual void parse(GSFileParser& gsf, const GameSet& gs) {
		auto str = gsf.nextString();
		if (str != "FINDER_RESULTS")
			ferr("REGISTER_WORK_ORDER must be followed by a FINDER_RESULTS!");
		gsUnitFinder = gs.objectFinderDefinitions.readIndex(gsf);
		workOrder = gs.workOrders.readPtr(gsf);
		cityFinder.reset(ReadFinder(gsf, gs));
	}
	virtual PlanNodeState* createState() override {
		return new State;
	}
	virtual void reset(PlanNodeState* state) override {
	}
	virtual bool execute(PlanNodeState* state, SrvScriptContext* ctx) override {
		for (ServerGameObject* city : cityFinder->eval(ctx)) {
			city->getPlayer()->aiController.registerWorkOrder(city, Server::instance->gameSet->objectFinderDefinitions[gsUnitFinder], workOrder);
		}
		return true;
	}
};

struct PFNIf : PlanNodeBlueprint {
	struct State : PlanNodeState {
		PlanNodeSequence::State seqStateTrue, seqStateFalse;
		PlanNodeSequence* chosenSequence = nullptr;
	};
	std::unique_ptr<ValueDeterminer> condition;
	PlanNodeSequence sequenceTrue, sequenceFalse;

	virtual void parse(GSFileParser& gsf, const GameSet& gs) {
		condition.reset(ReadValueDeterminer(gsf, gs));

		gsf.advanceLine();
		PlanNodeSequence* curSequence = &sequenceTrue;
		while (!gsf.eof) {
			std::string strtag = gsf.nextTag();
			if (strtag == "END_IF")
				return;
			else if (strtag == "ELSE")
				curSequence = &sequenceFalse;
			else if (!strtag.empty() && strtag.substr(0, 2) != "//")
				curSequence->nodes.emplace_back(PlanNodeBlueprint::createFrom(strtag, gsf, gs));
			gsf.advanceLine();
		}
		ferr("Plan node IF sequence reached end of file without END_IF!");
	}
	virtual PlanNodeState* createState() override {
		State* s = new State;
		s->seqStateTrue = sequenceTrue.createState();
		s->seqStateFalse = sequenceFalse.createState();
		return s;
	}
	virtual void reset(PlanNodeState* state) override {
		State* s = (State*)state;
		s->chosenSequence = nullptr;
	}
	virtual bool execute(PlanNodeState* state, SrvScriptContext* ctx) override {
		State* s = (State*)state;
		if (!s->chosenSequence) {
			s->chosenSequence = condition->booleval(ctx) ? &sequenceTrue : &sequenceFalse;
		}
		bool done = s->chosenSequence->execute((s->chosenSequence == &sequenceTrue) ? &s->seqStateTrue : &s->seqStateFalse, ctx);
		return done;
	}
};

struct PFNEmbeddedPlan : PlanNodeBlueprint {
	struct State : PlanNodeState {
		PlanNodeSequence::State seqState;
	};
	PlanNodeSequence* embeddedPlan;

	virtual void parse(GSFileParser& gsf, const GameSet& gs) {
		embeddedPlan = const_cast<PlanNodeSequence*>(gs.plans.readPtr(gsf));
	}
	virtual PlanNodeState* createState() override {
		State* s = new State;
		s->seqState = embeddedPlan->createState();
		return s;
	}
	virtual void reset(PlanNodeState* state) override {
		State* s = (State*)state;
		embeddedPlan->reset(&s->seqState);
	}
	virtual bool execute(PlanNodeState* state, SrvScriptContext* ctx) override {
		State* s = (State*)state;
		return embeddedPlan->execute(&s->seqState, ctx);
	}
};

PlanNodeBlueprint* PlanNodeBlueprint::createFrom(const std::string& tag, GSFileParser& gsf, const GameSet& gs)
{
	PlanNodeBlueprint* node;
	if (tag == "ACTION")
		node = new PFNAction;
	else if (tag == "WAIT_FOR")
		node = new PFNWaitFor;
	else if (tag == "WAIT_UNTIL")
		node = new PFNWaitUntil;
	else if (tag == "START_LOOP")
		node = new PFNStartLoop;
	else if (tag == "DEPLOY_ARMY_FROM")
		node = new PFNDeployArmyFrom;
	else if (tag == "CHOOSE_AT_RANDOM")
		node = new PFNChooseAtRandom;
	else if (tag == "REGISTER_WORK_ORDER")
		node = new PFNRegisterWorkOrder;
	else if (tag == "IF")
		node = new PFNIf;
	else if (tag == "EMBEDDED_PLAN")
		node = new PFNEmbeddedPlan;
	else if (tag == "EXPAND_SETTLEMENT" || tag == "REGISTER_EQUILIBRIUM_PROFILE") {
		auto nop = new PFNNoOperation;
		nop->name = tag;
		node = nop;
	}
	else {
		auto unk = new PFNUnknown;
		unk->name = tag;
		node = unk;
	}
	node->parse(gsf, gs);
	return node;
}

void PlanNodeSequence::parse(GSFileParser& gsf, const GameSet& gs, const char* endtag)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		std::string strtag = gsf.nextTag();
		if (strtag == endtag)
			return;
		else if (!strtag.empty() && strtag.substr(0,2) != "//")
			nodes.emplace_back(PlanNodeBlueprint::createFrom(strtag, gsf, gs));
		gsf.advanceLine();
	}
	ferr("Plan node sequence reached end of file without %s!", endtag);
}

PlanNodeSequence::State PlanNodeSequence::createState()
{
	State s;
	for (auto& node : nodes)
		s.nodeStates.push_back(node->createState());
	return s;
}

void PlanNodeSequence::reset(State* state)
{
	state->currentNode = 0;
	nodes[0]->reset(state->nodeStates[0]);
}

bool PlanNodeSequence::execute(State* state, SrvScriptContext* ctx)
{
	while (state->currentNode < nodes.size()) {
		bool done = nodes[state->currentNode]->execute(state->nodeStates[state->currentNode], ctx);
		if (!done)
			break;
		state->currentNode += 1;
		if (state->currentNode < nodes.size())
			nodes[state->currentNode]->reset(state->nodeStates[state->currentNode]);
	}
	return state->currentNode >= nodes.size();
}
