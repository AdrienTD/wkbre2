#include "finder.h"
#include "../util/GSFileParser.h"
#include "gameset.h"
#include "../util/util.h"
#include "../server.h"
#include "../client.h"
#include "CommonEval.h"

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
	case Tags::FINDER_TARGET: finder = new FinderController; break;
	case Tags::FINDER_RESULTS: finder = new FinderResults; break;
	case Tags::FINDER_ASSOCIATES: finder = new FinderAssociates; break;
	case Tags::FINDER_ASSOCIATORS: finder = new FinderAssociators; break;
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
			if (Server::instance->gameSet->equations[equation]->eval(obj) > 0.0f) {
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
