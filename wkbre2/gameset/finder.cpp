#include "finder.h"
#include "../util/GSFileParser.h"
#include "gameset.h"
#include "../util/util.h"
#include "../server.h"
#include "../client.h"
#include "CommonEval.h"
#include "../NNSearch.h"
#include "ScriptContext.h"

std::vector<ClientGameObject*> ObjectFinder::eval(ClientGameObject * self)
{
	ferr("eval(ClientGameObject*) unimplemented for this FINDER!");
	return std::vector<ClientGameObject*>();
}

struct FinderUnknown : ObjectFinder {
	virtual std::vector<ServerGameObject*> eval(ServerGameObject *self) override {
		ferr("Unknown object finder called from the Server!");
		return {};
	}
	virtual std::vector<ClientGameObject*> eval(ClientGameObject *self) override {
		ferr("Unknown object finder called from the Client!");
		return {};
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
	}
};

struct FinderSelf : ObjectFinder {
	virtual std::vector<ServerGameObject*> eval(ServerGameObject *self) override {
		return { self };
	}
	virtual std::vector<ClientGameObject*> eval(ClientGameObject *self) override {
		return { self };
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
	}
};

struct FinderSpecificId : ObjectFinder {
	uint32_t objid;
	virtual std::vector<ServerGameObject*> eval(ServerGameObject *self) override {
		return { Server::instance->findObject(objid) };
	}
	virtual std::vector<ClientGameObject*> eval(ClientGameObject *self) override {
		return { Client::instance->findObject(objid) };
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		objid = gsf.nextInt();
	}
	FinderSpecificId() {}
	FinderSpecificId(uint32_t objid) : objid(objid) {}
};

struct FinderPlayer : ObjectFinder {
	virtual std::vector<ServerGameObject*> eval(ServerGameObject *self) override {
		return { self->getPlayer() };
	}
	virtual std::vector<ClientGameObject*> eval(ClientGameObject *self) override {
		return { self->getPlayer() };
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
	}
};

struct FinderAlias : ObjectFinder {
	int aliasIndex;
	virtual std::vector<ServerGameObject*> eval(ServerGameObject *self) override {
		auto &alias = Server::instance->aliases[aliasIndex];
		std::vector<ServerGameObject*> res;
		for (auto &ref : alias)
			if (ref)
				res.push_back(ref);
		return res;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		aliasIndex = gs.aliases.readIndex(gsf);
	}
	FinderAlias() {}
	FinderAlias(int aliasIndex) : aliasIndex(aliasIndex) {}
};

struct FinderController : CommonEval<FinderController, ObjectFinder> {
	template<typename AnyGameObject> std::vector<AnyGameObject*> common_eval(AnyGameObject *self) {
		return { self->parent };
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
	}
};

struct FinderTarget : ObjectFinder {
	virtual std::vector<ServerGameObject*> eval(ServerGameObject *self) override {
		if (Order *order = self->orderConfig.getCurrentOrder())
			if (ServerGameObject *target = order->getCurrentTask()->target.get())
				return { target };
		return {};
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
	}
};

struct FinderResults : CommonEval<FinderResults, ObjectFinder> {
	int ofd;
	template<typename AnyGameObject> std::vector<AnyGameObject*> common_eval(AnyGameObject *self) {
		return AnyGameObject::Program::instance->gameSet->objectFinderDefinitions[ofd]->eval(self);
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		ofd = gs.objectFinderDefinitions.readIndex(gsf);
	}
};

struct FinderAssociates : ObjectFinder {
	int category;
	virtual std::vector<ServerGameObject*> eval(ServerGameObject *self) override {
		const auto &set = self->associates[category];
		return { set.begin(), set.end() };
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		category = gs.associations.readIndex(gsf);
	}
};

struct FinderAssociators : ObjectFinder {
	int category;
	virtual std::vector<ServerGameObject*> eval(ServerGameObject *self) override {
		const auto &set = self->associators[category];
		return { set.begin(), set.end() };
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		category = gs.associations.readIndex(gsf);
	}
};

struct FinderPlayers : ObjectFinder {
	int equation;
	virtual std::vector<ServerGameObject*> eval(ServerGameObject *self) override {
		Server *server = Server::instance;
		std::vector<ServerGameObject*> res;
		for (auto &it : server->level->children) {
			if ((it.first & 63) == Tags::GAMEOBJCLASS_PLAYER)
				for (ServerGameObject *player : it.second)
					if (auto _ = SrvScriptContext::candidate.change(player))
						if (server->gameSet->equations[equation]->eval(self))
							res.push_back(player);
		}
		return res;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		equation = gs.equations.readIndex(gsf);
	}
};

struct FinderCandidate : CommonEval<FinderCandidate, ObjectFinder> {
	template<typename AnyGameObject> std::vector<AnyGameObject*> common_eval(AnyGameObject *self) {
		auto obj = ScriptContext<AnyGameObject::Program, AnyGameObject>::candidate.get();
		if (obj) return { obj };
		else return {};
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {}
};

struct FinderCreator : CommonEval<FinderCreator, ObjectFinder> {
	template<typename AnyGameObject> std::vector<AnyGameObject*> common_eval(AnyGameObject *self) {
		auto obj = ScriptContext<AnyGameObject::Program, AnyGameObject>::creator.get();
		if (obj) return { obj };
		else return {};
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {}
};

struct FinderPackageSender : ObjectFinder {
	virtual std::vector<ServerGameObject*> eval(ServerGameObject* self) override {
		auto obj = SrvScriptContext::packageSender.get();
		if (obj) return { obj };
		else return {};
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {}
};

struct FinderSequenceExecutor : ObjectFinder {
	virtual std::vector<ServerGameObject*> eval(ServerGameObject* self) override {
		auto obj = SrvScriptContext::sequenceExecutor.get();
		if (obj) return { obj };
		else return {};
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {}
};

struct FinderNearestToSatisfy : ObjectFinder {
	std::unique_ptr<ValueDeterminer> vdcond, vdradius;
	virtual std::vector<ServerGameObject*> eval(ServerGameObject *self) override {
		float radius = vdradius->eval(self);
		NNSearch search;
		search.start(Server::instance, self->position, radius);
		std::vector<ServerGameObject*> res;
		while (ServerGameObject *obj = search.next()) {
			auto _ = SrvScriptContext::candidate.change(obj);
			if (vdcond->eval(self) > 0.0f)
				res.push_back(obj);
		}
		return res;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		vdcond.reset(ReadValueDeterminer(gsf, gs));
		vdradius.reset(ReadValueDeterminer(gsf, gs));
	}
};

ObjectFinder *ReadFinder(GSFileParser &gsf, const GameSet &gs)
{
	std::string strtag = gsf.nextString();
	if (strtag.substr(0, 7) != "FINDER_")
		return new FinderUnknown; // no need to call parse for unknown finders
	std::string findername = strtag.substr(7);
	ObjectFinder *finder;
	switch (Tags::FINDER_tagDict.getTagID(findername.c_str())) {
	case Tags::FINDER_SELF: finder = new FinderSelf; break;
	case Tags::FINDER_SPECIFIC_ID: finder = new FinderSpecificId; break;
	case Tags::FINDER_PLAYER: finder = new FinderPlayer; break;
	case Tags::FINDER_ALIAS: finder = new FinderAlias; break;
	case Tags::FINDER_CONTROLLER: finder = new FinderController; break;
	case Tags::FINDER_TARGET: finder = new FinderTarget; break;
	case Tags::FINDER_RESULTS: finder = new FinderResults; break;
	case Tags::FINDER_ASSOCIATES: finder = new FinderAssociates; break;
	case Tags::FINDER_ASSOCIATORS: finder = new FinderAssociators; break;
	case Tags::FINDER_PLAYERS: finder = new FinderPlayers; break;
	case Tags::FINDER_CANDIDATE: finder = new FinderCandidate; break;
	case Tags::FINDER_CREATOR: finder = new FinderCreator; break;
	case Tags::FINDER_PACKAGE_SENDER: finder = new FinderPackageSender; break;
	case Tags::FINDER_SEQUENCE_EXECUTOR: finder = new FinderSequenceExecutor; break;
	case Tags::FINDER_NEAREST_TO_SATISFY: finder = new FinderNearestToSatisfy; break;
	default: finder = new FinderUnknown; break;
	}
	finder->parse(gsf, const_cast<GameSet&>(gs));
	return finder;
}

struct FinderSubordinates : ObjectFinder {
	std::unique_ptr<ObjectFinder> finder;
	int equation = -1;
	int bpclass = -1;
	GameObjBlueprint *objbp = nullptr;
	bool immediateLevel = false;
	static /*thread_local*/ std::vector<ServerGameObject*> results;
	bool eligible(ServerGameObject *obj) {
		if (!objbp || (objbp == obj->blueprint))
			if ((bpclass == -1) || (bpclass == obj->blueprint->bpClass))
				if ((equation == -1) || (Server::instance->gameSet->equations[equation]->eval(obj) > 0.0f))
					return true;
		return false;
	}
	void walk(ServerGameObject *obj) {
		for (auto &it : obj->children) {
			for (ServerGameObject *child : it.second) {
				if (eligible(child))
					results.push_back(child);
				if (!immediateLevel)
					walk(child);
			}
		}
	}
	virtual std::vector<ServerGameObject*> eval(ServerGameObject *self) override {
		auto vec = finder->eval(self);
		results.clear();
		for (ServerGameObject *par : vec) {
			static const auto scl = { Tags::GAMEOBJCLASS_BUILDING, Tags::GAMEOBJCLASS_CHARACTER, Tags::GAMEOBJCLASS_CONTAINER, Tags::GAMEOBJCLASS_MARKER, Tags::GAMEOBJCLASS_PROP };
			if (std::find(scl.begin(), scl.end(), par->blueprint->bpClass) != scl.end()) {
				if (eligible(par))
					results.push_back(par);
			}
			walk(par);
		}
		return std::move(results);
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		std::string bpname;
		while (!gsf.eol) {
			auto arg = gsf.nextString();
			if (arg == "BY_BLUEPRINT")
				bpname = gsf.nextString(true);
			else if (arg == "BY_EQUATION")
				equation = gs.equations.readIndex(gsf);
			else if (arg == "OF_CLASS")
				bpclass = Tags::GAMEOBJCLASS_tagDict.getTagID(gsf.nextString().c_str());
			else if (arg == "IMMEDIATE_LEVEL")
				immediateLevel = true;
		}
		if (bpname != "" && bpclass != -1)
			objbp = gs.findBlueprint(bpclass, bpname);
		finder.reset(ReadFinderNode(gsf, gs));
	}
};

decltype(FinderSubordinates::results) FinderSubordinates::results; // this could be avoided using "inline" static members from C++17

struct FinderNSubs : ObjectFinder {
	DynArray<std::unique_ptr<ObjectFinder>> finders;
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		finders.resize(gsf.nextInt());
		for (auto &finder : finders)
			finder.reset(ReadFinderNode(gsf, gs));
	}
};

struct FinderUnion : FinderNSubs {
	virtual std::vector<ServerGameObject*> eval(ServerGameObject *self) override {
		std::unordered_set<ServerGameObject*> set;
		for (auto &finder : finders) {
			auto vec = finder->eval(self);
			set.insert(vec.begin(), vec.end());
		}
		return { set.begin(), set.end() };
	}
};

struct FinderChain : FinderNSubs {
	virtual std::vector<ServerGameObject*> eval(ServerGameObject *self) override {
		std::vector<ServerGameObject*> vec = { self };
		for (auto &finder : finders) {
			vec = finder->eval(vec[0]);
			if (vec.empty())
				break;
		}
		return vec;
	}
};

struct FinderAlternative : FinderNSubs {
	virtual std::vector<ServerGameObject*> eval(ServerGameObject *self) override {
		for (auto &finder : finders) {
			auto vec = finder->eval(self);
			if (!vec.empty())
				return vec;
		}
		return {};
	}
};

struct FinderFilterFirst : ObjectFinder {
	int equation;
	std::unique_ptr<ValueDeterminer> count;
	std::unique_ptr<ObjectFinder> finder;
	virtual std::vector<ServerGameObject*> eval(ServerGameObject *self) override {
		auto vec = finder->eval(self);
		int limit = (int)count->eval(self), num = 0;
		decltype(vec) res;
		for (ServerGameObject *obj : vec) {
			auto _ = SrvScriptContext::candidate.change(obj);
			if (Server::instance->gameSet->equations[equation]->eval(obj) > 0.0f) {
				res.push_back(obj);
				if (++num >= limit)
					break;
			}
		}
		return res;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		equation = gs.equations.readIndex(gsf);
		count.reset(ReadValueDeterminer(gsf, gs));
		finder.reset(ReadFinderNode(gsf, gs));
	}
};

struct FinderFilter : ObjectFinder {
	int equation;
	std::unique_ptr<ObjectFinder> finder;
	virtual std::vector<ServerGameObject*> eval(ServerGameObject *self) override {
		auto vec = finder->eval(self);
		decltype(vec) res;
		for (ServerGameObject *obj : vec) {
			auto _ = SrvScriptContext::candidate.change(obj);
			if (Server::instance->gameSet->equations[equation]->eval(self) > 0.0f) {
				res.push_back(obj);
			}
		}
		return res;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		equation = gs.equations.readIndex(gsf);
		finder.reset(ReadFinderNode(gsf, gs));
	}
};

struct FinderMetreRadius : ObjectFinder {
	std::unique_ptr<ValueDeterminer> vdradius;
	virtual std::vector<ServerGameObject*> eval(ServerGameObject *self) override {
		float radius = vdradius->eval(self);
		NNSearch search;
		search.start(Server::instance, self->position, radius);
		std::vector<ServerGameObject*> res;
		while (ServerGameObject *obj = search.next())
			res.push_back(obj);
		return res;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		vdradius.reset(ReadValueDeterminer(gsf, gs));
		// ...
	}
};

struct FinderGradeSelect : ObjectFinder {
	bool byHighest;
	int equation;
	std::unique_ptr<ValueDeterminer> vdCount;
	std::unique_ptr<ObjectFinder> finder;
	virtual std::vector<ServerGameObject*> eval(ServerGameObject *self) override {
		int count = (int)vdCount->eval(self);
		auto vec = finder->eval(self);
		ValueDeterminer *vd = Server::instance->gameSet->equations[equation];

		std::vector<std::pair<float,ServerGameObject*>> values(vec.size());
		for (size_t i = 0; i < vec.size(); i++) {
			auto _ = SrvScriptContext::candidate.change(vec[i]);
			values[i] = { vd->eval(self), vec[i] };
		}
		std::sort(values.begin(), values.end(), [this](auto & a, auto & b) {return (a.first < b.first) != byHighest; });
		if (count <= 0 || count > vec.size())
			count = vec.size();
		std::vector<ServerGameObject*> res(count);
		for (size_t i = 0; i < res.size(); i++)
			res[i] = values[i].second;
		return res;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		auto arg = gsf.nextString();
		if (arg == "BY_HIGHEST")
			byHighest = true;
		else if (arg == "BY_LOWEST")
			byHighest = false;
		else
			ferr("Invalid grading %s", arg.c_str());
		equation = gs.equations.readIndex(gsf);
		vdCount.reset(ReadValueDeterminer(gsf, gs));
		finder.reset(ReadFinderNode(gsf, gs));
	}
};

struct FinderNearestCandidate : ObjectFinder {
	std::unique_ptr<ObjectFinder> finder;
	virtual std::vector<ServerGameObject*> eval(ServerGameObject *self) override {
		auto vec = finder->eval(self);
		float bestDist = INFINITY;
		ServerGameObject *bestObj = nullptr;
		for (ServerGameObject *obj : vec) {
			float dist = (obj->position - self->position).sqlen2xz();
			if (dist < bestDist) {
				bestDist = dist;
				bestObj = obj;
			}
		}
		if (bestObj)
			return { bestObj };
		else
			return {};
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		finder.reset(ReadFinderNode(gsf, gs));
	}
};

ObjectFinder *ReadFinderNode(::GSFileParser &gsf, const ::GameSet &gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		const char *oldcur = gsf.cursor;
		std::string strtag = gsf.nextTag();
		if (strtag == "END_OBJECT_FINDER_DEFINITION")
			return new FinderUnknown;
		else if(strtag.substr(0, 7) == "FINDER_") {
			//if (strtag.substr(0, 7) != "FINDER_")
			//	return new FinderUnknown; // no need to call parse for unknown finders
			ObjectFinder *finder;
			switch (Tags::FINDER_tagDict.getTagID(strtag.substr(7).c_str())) {
			case Tags::FINDER_SUBORDINATES: finder = new FinderSubordinates; break;
			case Tags::FINDER_UNION: finder = new FinderUnion; break;
			case Tags::FINDER_CHAIN: finder = new FinderChain; break;
			case Tags::FINDER_ALTERNATIVE: finder = new FinderAlternative; break;
			case Tags::FINDER_FILTER_FIRST: finder = new FinderFilterFirst; break;
			case Tags::FINDER_FILTER: finder = new FinderFilter; break;
			case Tags::FINDER_METRE_RADIUS: finder = new FinderMetreRadius; break;
			case Tags::FINDER_GRADE_SELECT: finder = new FinderGradeSelect; break;
			case Tags::FINDER_NEAREST_CANDIDATE: finder = new FinderNearestCandidate; break;
			default:
				gsf.cursor = oldcur;
				return ReadFinder(gsf, gs);
			}
			finder->parse(gsf, const_cast<GameSet&>(gs));
			return finder;
		}
		gsf.advanceLine();
	}
	ferr("Object finder definition reached end of file without END_OBJECT_FINDER_DEFINITION!");
	return nullptr;
}
