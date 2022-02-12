// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "finder.h"
#include "../util/GSFileParser.h"
#include "gameset.h"
#include "../util/util.h"
#include "../server.h"
#include "../client.h"
#include "CommonEval.h"
#include "../NNSearch.h"
#include "ScriptContext.h"
#include <cassert>
#include <iterator>

std::vector<ClientGameObject*> ObjectFinder::eval(CliScriptContext* ctx)
{
	ferr("eval(ClientGameObject*) unimplemented for this FINDER!");
	return std::vector<ClientGameObject*>();
}

struct FinderUnknown : ObjectFinder {
	std::string name;
	virtual std::vector<ServerGameObject*> eval(SrvScriptContext* ctx) override {
		ferr("Unknown object finder %s called from the Server!", name.c_str());
		return {};
	}
	virtual std::vector<ClientGameObject*> eval(CliScriptContext* ctx) override {
		ferr("Unknown object finder %s called from the Client!", name.c_str());
		return {};
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
	}
	FinderUnknown(const std::string& name) : name(name) {}
};

struct FinderSelf : ObjectFinder {
	virtual std::vector<ServerGameObject*> eval(SrvScriptContext* ctx) override {
		auto obj = ctx->getSelf();
		if (obj) return { obj };
		else return {};
	}
	virtual std::vector<ClientGameObject*> eval(CliScriptContext* ctx) override {
		auto obj = ctx->getSelf();
		if (obj) return { obj };
		else return {};
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
	}
};

struct FinderSpecificId : ObjectFinder {
	uint32_t objid;
	virtual std::vector<ServerGameObject*> eval(SrvScriptContext* ctx) override {
		auto obj = ctx->server->findObject(objid);
		if (obj) return { obj };
		else return {};
	}
	virtual std::vector<ClientGameObject*> eval(CliScriptContext* ctx) override {
		auto obj = ctx->client->findObject(objid);
		if (obj) return { obj };
		else return {};
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		objid = gsf.nextInt();
	}
	FinderSpecificId() {}
	FinderSpecificId(uint32_t objid) : objid(objid) {}
};

struct FinderPlayer : ObjectFinder {
	virtual std::vector<ServerGameObject*> eval(SrvScriptContext* ctx) override {
		auto obj = ctx->getSelf()->getPlayer();
		if (obj) return { obj };
		else return {};
	}
	virtual std::vector<ClientGameObject*> eval(CliScriptContext* ctx) override {
		auto obj = ctx->getSelf()->getPlayer();
		if (obj) return { obj };
		else return {};
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
	}
};

struct FinderAlias : ObjectFinder {
	int aliasIndex;
	virtual std::vector<ServerGameObject*> eval(SrvScriptContext* ctx) override {
		auto &alias = Server::instance->aliases[aliasIndex];
		std::vector<ServerGameObject*> res;
		for (auto &ref : alias)
			if (auto* obj = ref.getFrom<Server>(); obj && obj->isInteractable())
				res.push_back(obj);
		return res;
	}
	virtual std::vector<ClientGameObject*> eval(CliScriptContext* ctx) override {
		// TODO when Alias is received by Client
		return {};
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		aliasIndex = gs.aliases.readIndex(gsf);
	}
	FinderAlias() {}
	FinderAlias(int aliasIndex) : aliasIndex(aliasIndex) {}
};

struct FinderController : CommonEval<FinderController, ObjectFinder> {
	template<typename CTX> std::vector<typename CTX::AnyGO*> common_eval(CTX* ctx) {
		return { ctx->getSelf()->getParent() };
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
	}
};

struct FinderTarget : ObjectFinder {
	virtual std::vector<ServerGameObject*> eval(SrvScriptContext* ctx) override {
		if (auto obj = ctx->get(ctx->target))
			return { obj };
		if (Order *order = ctx->getSelf()->orderConfig.getCurrentOrder())
			if (ServerGameObject *target = order->getCurrentTask()->target.get())
				return { target };
		return {};
	}
	virtual std::vector<ClientGameObject*> eval(CliScriptContext* ctx) override {
		auto obj = ctx->get(ctx->target);
		if (obj) return { obj };
		else return {};
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
	}
};

struct FinderResults : CommonEval<FinderResults, ObjectFinder> {
	int ofd;
	template<typename CTX> std::vector<typename CTX::AnyGO*> common_eval(CTX* ctx) {
		if (ofd == -1) return {}; // TODO: Remove this after implementing exception handling
		return CTX::Program::instance->gameSet->objectFinderDefinitions[ofd]->eval(ctx);
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		ofd = gs.objectFinderDefinitions.readIndex(gsf);
	}
};

struct FinderAssociates : ObjectFinder {
	int category;
	virtual std::vector<ServerGameObject*> eval(SrvScriptContext* ctx) override {
		const auto &set = ctx->getSelf()->associates[category];
		std::vector<ServerGameObject*> vec;
		for (auto& ref : set)
			if (ref && ref->isInteractable())
				vec.push_back(ref);
		return vec;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		category = gs.associations.readIndex(gsf);
	}
};

struct FinderAssociators : ObjectFinder {
	int category;
	virtual std::vector<ServerGameObject*> eval(SrvScriptContext* ctx) override {
		const auto &set = ctx->getSelf()->associators[category];
		std::vector<ServerGameObject*> vec;
		for (auto& ref : set)
			if (ref && ref->isInteractable())
				vec.push_back(ref);
		return vec;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		category = gs.associations.readIndex(gsf);
	}
};

struct FinderPlayers : ObjectFinder {
	int equation;
	virtual std::vector<ServerGameObject*> eval(SrvScriptContext* ctx) override {
		Server *server = Server::instance;
		std::vector<ServerGameObject*> res;
		for (auto &it : server->getLevel()->children) {
			if ((it.first & 63) == Tags::GAMEOBJCLASS_PLAYER)
				for (CommonGameObject *player : it.second)
					if (auto _ = ctx->change(ctx->candidate, player->dyncast<ServerGameObject>()))
						if (server->gameSet->equations[equation]->eval(ctx))
							res.push_back(player->dyncast<ServerGameObject>());
		}
		return res;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		equation = gs.equations.readIndex(gsf);
	}
};

struct FinderCandidate : CommonEval<FinderCandidate, ObjectFinder> {
	template<typename CTX> std::vector<typename CTX::AnyGO*> common_eval(CTX* ctx) {
		auto obj = ctx->get(ctx->candidate);
		if (obj) return { obj };
		else return {};
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {}
};

struct FinderCreator : CommonEval<FinderCreator, ObjectFinder> {
	template<typename CTX> std::vector<typename CTX::AnyGO*> common_eval(CTX* ctx) {
		auto obj = ctx->get(ctx->creator);
		if (obj) return { obj };
		else return {};
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {}
};

struct FinderPackageSender : ObjectFinder {
	virtual std::vector<ServerGameObject*> eval(SrvScriptContext* ctx) override {
		auto obj = ctx->get(ctx->packageSender);
		if (obj) return { obj };
		else return {};
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {}
};

struct FinderSequenceExecutor : ObjectFinder {
	virtual std::vector<ServerGameObject*> eval(SrvScriptContext* ctx) override {
		auto obj = ctx->get(ctx->sequenceExecutor);
		if (obj) return { obj };
		else return {};
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {}
};

struct FinderNearestToSatisfy : CommonEval<FinderNearestToSatisfy, ObjectFinder> {
	std::unique_ptr<ValueDeterminer> vdcond, vdradius;
	template<typename CTX> std::vector<typename CTX::AnyGO*> common_eval(CTX* ctx) {
		using Program = typename CTX::Program;
		using AnyGO = typename CTX::AnyGO;
		float radius = vdradius->eval(ctx);
		NNSearch<Program, AnyGO> search;
		search.start(Program::instance, ctx->getSelf()->position, radius);
		std::vector<AnyGO*> res;
		while (AnyGO* obj = search.next()) {
			if (!obj->isInteractable()) continue;
			auto _ = ctx->change(ctx->candidate, obj);
			if (vdcond->eval(ctx) > 0.0f)
				res.push_back(obj);
		}
		return res;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		vdcond.reset(ReadValueDeterminer(gsf, gs));
		vdradius.reset(ReadValueDeterminer(gsf, gs));
	}
};

struct FinderLevel : CommonEval<FinderLevel, ObjectFinder> {
	template<typename CTX> std::vector<typename CTX::AnyGO*> common_eval(CTX* ctx) {
		return { CTX::Program::instance->getLevel() };
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {}
};

struct FinderDisabledAssociates : ObjectFinder {
	int category;
	virtual std::vector<ServerGameObject*> eval(SrvScriptContext* ctx) override {
		const auto& set = ctx->getSelf()->associates[category];
		std::vector<ServerGameObject*> vec;
		for (auto& ref : set)
			if (ref && ref->disableCount > 0) // terminated?
				vec.push_back(ref);
		return vec;
	}
	virtual std::vector<ClientGameObject*> eval(CliScriptContext* ctx) override {
		// TODO
		return {};
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		category = gs.associations.readIndex(gsf);
	}
};

struct FinderReferencers : ObjectFinder {
	int category;
	virtual std::vector<ServerGameObject*> eval(SrvScriptContext* ctx) override {
		std::set<ServerGameObject*> vec;
		for (ServerGameObject* obj : ctx->getSelf()->referencers) {
			if (obj) {
				Order* order = obj->orderConfig.getCurrentOrder();
				if (!order) continue;
				Task* task = order->getCurrentTask();
				if (!task) continue;
				//assert(task->target.get() == ctx->getSelf());
				if (task->target != ctx->getSelf()) continue;
				if (task->blueprint->category == category)
					vec.insert(obj);
			}
		}
		return { vec.begin(), vec.end() };
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		category = gs.taskCategories.readIndex(gsf);
	}
};

struct FinderOrderGiver : ObjectFinder {
	virtual std::vector<ServerGameObject*> eval(SrvScriptContext* ctx) override {
		auto obj = ctx->get(ctx->orderGiver);
		if (obj) return { obj };
		else return {};
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {}
};

struct FinderBeingTransferredToMe : ObjectFinder {
	std::unique_ptr<ObjectFinder> finder;
	virtual std::vector<ServerGameObject*> eval(SrvScriptContext* ctx) override {
		return {};
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct FinderCollisionSubject : ObjectFinder {
	virtual std::vector<ServerGameObject*> eval(SrvScriptContext* ctx) override {
		auto obj = ctx->get(ctx->collisionSubject);
		if (obj) return { obj };
		else return {};
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {}
};

struct FinderUser : CommonEval<FinderUser, ObjectFinder> {
	template<typename CTX> std::vector<typename CTX::AnyGO*> common_eval(CTX* ctx) {
		return {};
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {}
};

struct FinderSelectedObject : CommonEval<FinderSelectedObject, ObjectFinder> {
	template<typename CTX> std::vector<typename CTX::AnyGO*> common_eval(CTX* ctx) {
		auto obj = ctx->get(ctx->selectedObject);
		if (obj) return { obj };
		else return {};
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {}
};

struct FinderAgAllOfType : ObjectFinder {
	GameObjBlueprint* blueprint;
	virtual std::vector<ServerGameObject*> eval(SrvScriptContext* ctx) override {
		uint32_t bpid = blueprint->getFullId();
		std::vector<ServerGameObject*> vec;
		auto walk = [this, bpid, &vec](CommonGameObject* obj, auto& rec) -> void {
			for (const auto& subords : obj->children) {
				if (subords.first == bpid) {
					for (CommonGameObject* sub : subords.second) {
						vec.push_back((ServerGameObject*)sub);
					}
				}
				else {
					for (CommonGameObject* sub : subords.second) {
						rec(sub, rec);
					}
				}
			}
		};
		walk(Server::instance->getLevel(), walk);
		return vec;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		blueprint = gs.readObjBlueprintPtr(gsf);
	}
};

ObjectFinder *ReadFinder(GSFileParser &gsf, const GameSet &gs)
{
	std::string strtag = gsf.nextString();
	if (strtag.substr(0, 7) != "FINDER_")
		return new FinderUnknown(strtag); // no need to call parse for unknown finders
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
	case Tags::FINDER_LEVEL: finder = new FinderLevel; break;
	case Tags::FINDER_DISABLED_ASSOCIATES: finder = new FinderDisabledAssociates; break;
	case Tags::FINDER_REFERENCERS: finder = new FinderReferencers; break;
	case Tags::FINDER_ORDER_GIVER: finder = new FinderOrderGiver; break;
	case Tags::FINDER_BEING_TRANSFERRED_TO_ME: finder = new FinderBeingTransferredToMe; break;
	case Tags::FINDER_COLLISION_SUBJECT: finder = new FinderCollisionSubject; break;
	case Tags::FINDER_USER: finder = new FinderUser; break;
	case Tags::FINDER_SELECTED_OBJECT: finder = new FinderSelectedObject; break;
	default:
		if (findername == "AG_ALL_OF_TYPE") { finder = new FinderAgAllOfType; break; }
		finder = new FinderUnknown(strtag); break;
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
	bool eligible(ServerGameObject *obj, SrvScriptContext *ctx) {
		if (!obj->isInteractable()) return false;
		if (!objbp || (objbp == obj->blueprint)) {
			if ((bpclass == -1) || (bpclass == obj->blueprint->bpClass)) {
				auto _ = ctx->change(ctx->candidate, obj);
				if ((equation == -1) || (Server::instance->gameSet->equations[equation]->eval(ctx) > 0.0f))
					return true;
			}
		}
		return false;
	}
	void walk(ServerGameObject *obj, SrvScriptContext* ctx) {
		for (auto &it : obj->children) {
			for (CommonGameObject* cchild : it.second) {
				ServerGameObject* child = cchild->dyncast<ServerGameObject>();
				if (eligible(child, ctx))
					results.push_back(child);
				if (!immediateLevel)
					walk(child, ctx);
			}
		}
	}
	virtual std::vector<ServerGameObject*> eval(SrvScriptContext* ctx) override {
		auto vec = finder->eval(ctx);
		results.clear();
		for (ServerGameObject *par : vec) {
			static const auto scl = { Tags::GAMEOBJCLASS_BUILDING, Tags::GAMEOBJCLASS_CHARACTER, Tags::GAMEOBJCLASS_CONTAINER, Tags::GAMEOBJCLASS_MARKER, Tags::GAMEOBJCLASS_PROP };
			if (std::find(scl.begin(), scl.end(), par->blueprint->bpClass) != scl.end()) {
				if (eligible(par, ctx))
					results.push_back(par);
			}
			walk(par, ctx);
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

struct FinderUnion : CommonEval<FinderUnion, FinderNSubs> {
	template<typename CTX> std::vector<typename CTX::AnyGO*> common_eval(CTX* ctx) {
		std::unordered_set<typename CTX::AnyGO*> set;
		for (auto &finder : finders) {
			auto vec = finder->eval(ctx);
			set.insert(vec.begin(), vec.end());
		}
		return { set.begin(), set.end() };
	}
};

struct FinderIntersection : CommonEval<FinderIntersection, FinderNSubs> {
	template<typename CTX> std::vector<typename CTX::AnyGO*> common_eval(CTX* ctx) {
		auto objs = finders[0]->eval(ctx);
		decltype(objs) intersected;
		std::sort(objs.begin(), objs.end());
		intersected.reserve(objs.size());
		for (size_t f = 1; f < finders.size(); ++f) {
			auto next = finders[f]->eval(ctx);
			std::sort(next.begin(), next.end());
			std::set_intersection(objs.begin(), objs.end(), next.begin(), next.end(), std::back_inserter(intersected));
			std::swap(objs, intersected);
			intersected.clear();
		}
		return objs;
	}
};

struct FinderChain : CommonEval<FinderChain, FinderNSubs> {
	template<typename CTX> std::vector<typename CTX::AnyGO*> common_eval(CTX* ctx) {
		auto _ = ctx->change(ctx->chainOriginalSelf, ctx->getSelf());
		std::vector<typename CTX::AnyGO*> vec = { ctx->getSelf() };
		for (auto &finder : finders) {
			auto _ = ctx->changeSelf(vec[0]);
			vec = finder->eval(ctx);
			if (vec.empty())
				break;
		}
		return vec;
	}
};

struct FinderAlternative : CommonEval<FinderAlternative, FinderNSubs> {
	template<typename CTX> std::vector<typename CTX::AnyGO*> common_eval(CTX* ctx) {
		for (auto &finder : finders) {
			auto vec = finder->eval(ctx);
			if (!vec.empty())
				return vec;
		}
		return {};
	}
};

struct FinderFilterFirst : CommonEval<FinderFilterFirst, ObjectFinder> {
	int equation;
	std::unique_ptr<ValueDeterminer> count;
	std::unique_ptr<ObjectFinder> finder;
	template<typename CTX> std::vector<typename CTX::AnyGO*> common_eval(CTX* ctx) {
		auto vec = finder->eval(ctx);
		int limit = (int)count->eval(ctx), num = 0;
		decltype(vec) res;
		for (typename CTX::AnyGO *obj : vec) {
			auto _ = ctx->change(ctx->candidate, obj);
			if (CTX::Program::instance->gameSet->equations[equation]->eval(ctx) > 0.0f) {
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

struct FinderFilter : CommonEval<FinderFilter, ObjectFinder> {
	int equation;
	std::unique_ptr<ObjectFinder> finder;
	template<typename CTX> std::vector<typename CTX::AnyGO*> common_eval(CTX* ctx) {
		auto vec = finder->eval(ctx);
		decltype(vec) res;
		for (typename CTX::AnyGO *obj : vec) {
			auto _ = ctx->change(ctx->candidate, obj);
			if (CTX::Program::instance->gameSet->equations[equation]->eval(ctx) > 0.0f) {
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

struct FinderMetreRadius : CommonEval<FinderMetreRadius, ObjectFinder> {
	std::unique_ptr<ValueDeterminer> vdradius;
	bool useOriginalSelf = false;
	int relationship = 0;
	int classFilter = 0;

	template<typename AnyGO> bool eligible(AnyGO* obj, AnyGO* refplayer) {
		if (!obj->isInteractable()) return false;
		AnyGO* objplayer = obj->getPlayer();
		switch (relationship) {
		case 1: if (objplayer != refplayer) return false; break;
		case 2: if (objplayer != refplayer && AnyGO::Program::instance->getDiplomaticStatus(objplayer, refplayer) >= 1) return false; break;
		case 3: if (objplayer == refplayer || AnyGO::Program::instance->getDiplomaticStatus(objplayer, refplayer) < 2) return false; break;
		default:;
		}
		int objclass = obj->blueprint->bpClass;
		switch (classFilter) {
		case 1: if (objclass != Tags::GAMEOBJCLASS_BUILDING) return false; break;
		case 2: if (objclass != Tags::GAMEOBJCLASS_CHARACTER) return false; break;
		case 3: if (objclass != Tags::GAMEOBJCLASS_BUILDING && objclass != Tags::GAMEOBJCLASS_CHARACTER) return false; break;
		default:;
		}
		return true;
	}
	template<typename CTX> std::vector<typename CTX::AnyGO*> common_eval(CTX* ctx) {
		using Program = typename CTX::Program;
		using AnyGO = typename CTX::AnyGO;
		float radius = vdradius->eval(ctx);
		AnyGO* player = (useOriginalSelf ? ctx->get(ctx->chainOriginalSelf) : ctx->getSelf())->getPlayer();
		NNSearch<Program, AnyGO> search;
		search.start(Program::instance, ctx->getSelf()->position, radius);
		std::vector<AnyGO*> res;
		while (AnyGO* obj = search.next())
			if (eligible(obj, player))
				res.push_back(obj);
		return res;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		vdradius.reset(ReadValueDeterminer(gsf, gs));
		// ...
		std::string word = gsf.nextString();
		while (!word.empty()) {
			if (word == "SAME_PLAYER_UNITS") relationship = 1;
			else if (word == "ALLIED_UNITS") relationship = 2;
			else if (word == "ENEMY_UNITS") relationship = 3;
			else if (word == "ORIGINAL_SAME_PLAYER_UNITS") { useOriginalSelf = true; relationship = 1; }
			else if (word == "ORIGINAL_ALLIED_UNITS") { useOriginalSelf = true; relationship = 2; }
			else if (word == "ORIGINAL_ENEMY_UNITS") { useOriginalSelf = true; relationship = 3; }
			else if (word == "BUILDINGS_ONLY") classFilter = 1;
			else if (word == "CHARACTERS_ONLY") classFilter = 2;
			else if (word == "CHARACTERS_AND_BUILDINGS_ONLY") classFilter = 3;
			else printf("Unknown METRE_RADIUS term %s\n", word.c_str());
			word = gsf.nextString();
		}
	}
};

struct FinderGradeSelect : ObjectFinder {
	bool byHighest;
	int equation;
	std::unique_ptr<ValueDeterminer> vdCount;
	std::unique_ptr<ObjectFinder> finder;
	virtual std::vector<ServerGameObject*> eval(SrvScriptContext* ctx) override {
		int count = 0;
		if (vdCount)
			count = (int)vdCount->eval(ctx);
		auto vec = finder->eval(ctx);
		ValueDeterminer *vd = Server::instance->gameSet->equations[equation];

		std::vector<std::pair<float,ServerGameObject*>> values(vec.size());
		for (size_t i = 0; i < vec.size(); i++) {
			auto _ = ctx->change(ctx->candidate, vec[i]);
			values[i] = { vd->eval(ctx), vec[i] };
		}
		std::sort(values.begin(), values.end(), [this](auto & a, auto & b) {return byHighest ? (a.first > b.first) : (a.first < b.first); });
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
		if (!gsf.eol)
			vdCount.reset(ReadValueDeterminer(gsf, gs));
		finder.reset(ReadFinderNode(gsf, gs));
	}
};

struct FinderNearestCandidate : ObjectFinder {
	std::unique_ptr<ObjectFinder> finder;
	virtual std::vector<ServerGameObject*> eval(SrvScriptContext* ctx) override {
		auto vec = finder->eval(ctx);
		float bestDist = INFINITY;
		ServerGameObject *bestObj = nullptr;
		for (ServerGameObject *obj : vec) {
			float dist = (obj->position - ctx->getSelf()->position).sqlen2xz();
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

struct FinderTileRadius : CommonEval<FinderTileRadius, ObjectFinder> {
	std::unique_ptr<ValueDeterminer> vdradius;
	template<typename CTX> std::vector<typename CTX::AnyGO*> common_eval(CTX* ctx) {
		using Program = typename CTX::Program;
		using AnyGO = typename CTX::AnyGO;
		float radius = vdradius->eval(ctx);
		NNSearch<Program, AnyGO> search;
		search.start(Program::instance, ctx->getSelf()->position, radius);
		std::vector<AnyGO*> res;
		while (AnyGO* obj = search.next())
			if (obj->isInteractable())
				res.push_back(obj);
		return res;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		vdradius.reset(ReadValueDeterminer(gsf, gs));
	}
};

struct FinderDiscoveredUnits : ObjectFinder {
	virtual std::vector<ServerGameObject*> eval(SrvScriptContext* ctx) override {
		// TODO
		return {};
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {}
};

struct FinderRandomSelection : ObjectFinder {
	std::unique_ptr<ValueDeterminer> vCount;
	std::unique_ptr<ObjectFinder> finder;
	virtual std::vector<ServerGameObject*> eval(SrvScriptContext* ctx) override {
		auto vec = finder->eval(ctx);
		size_t cnt = std::min(vec.size(), (size_t)vCount->eval(ctx));
		for (size_t i = 0; i < cnt; i++) {
			size_t s = rand() % (vec.size() - i) + i;
			std::swap(vec[i], vec[s]);
		}
		vec.resize(cnt);
		return vec;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		vCount.reset(ReadValueDeterminer(gsf, gs));
		finder.reset(ReadFinderNode(gsf, gs));
	}
};

struct FinderFilterCandidates : CommonEval<FinderFilterCandidates, ObjectFinder> {
	std::unique_ptr<ValueDeterminer> condition;
	std::unique_ptr<ObjectFinder> finder;
	template<typename CTX> std::vector<typename CTX::AnyGO*> common_eval(CTX* ctx) {
		auto vec = finder->eval(ctx);
		decltype(vec) res;
		for (typename CTX::AnyGO* obj : vec) {
			auto _ = ctx->change(ctx->candidate, obj);
			if (condition->eval(ctx) > 0.0f) {
				res.push_back(obj);
			}
		}
		return res;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		condition.reset(ReadValueDeterminer(gsf, gs));
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
			return new FinderUnknown("<empty OBJECT_FINDER_DEFINITION>");
		else if(strtag.substr(0, 7) == "FINDER_") {
			//if (strtag.substr(0, 7) != "FINDER_")
			//	return new FinderUnknown; // no need to call parse for unknown finders
			ObjectFinder *finder;
			switch (Tags::FINDER_tagDict.getTagID(strtag.substr(7).c_str())) {
			case Tags::FINDER_SUBORDINATES: finder = new FinderSubordinates; break;
			case Tags::FINDER_UNION: finder = new FinderUnion; break;
			case Tags::FINDER_INTERSECTION: finder = new FinderIntersection; break;
			case Tags::FINDER_CHAIN: finder = new FinderChain; break;
			case Tags::FINDER_ALTERNATIVE: finder = new FinderAlternative; break;
			case Tags::FINDER_FILTER_FIRST: finder = new FinderFilterFirst; break;
			case Tags::FINDER_FILTER: finder = new FinderFilter; break;
			case Tags::FINDER_METRE_RADIUS: finder = new FinderMetreRadius; break;
			case Tags::FINDER_GRADE_SELECT: finder = new FinderGradeSelect; break;
			case Tags::FINDER_NEAREST_CANDIDATE: finder = new FinderNearestCandidate; break;
			case Tags::FINDER_TILE_RADIUS: finder = new FinderTileRadius; break;
			case Tags::FINDER_DISCOVERED_UNITS: finder = new FinderDiscoveredUnits; break;
			case Tags::FINDER_RANDOM_SELECTION: finder = new FinderRandomSelection; break;
			case Tags::FINDER_FILTER_CANDIDATES: finder = new FinderFilterCandidates; break;
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
