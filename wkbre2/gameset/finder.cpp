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

ObjectFinderResult ObjectFinder::fail(ScriptContext* ctx) {
	ferr("Invalid object finder call from %s", ctx->gameState->getProgramName());
	return {};
}

struct FinderUnknown : ObjectFinder {
	std::string name;
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		ferr("Unknown object finder %s called from the %s!", name.c_str(), ctx->gameState->getProgramName());
		return {};
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
	}
	FinderUnknown(const std::string& name) : name(name) {}
};

struct FinderSelf : ObjectFinder {
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		auto obj = ctx->getSelf();
		if (obj) return { obj };
		else return {};
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
	}
};

struct FinderSpecificId : ObjectFinder {
	uint32_t objid;
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		auto obj = ctx->gameState->findObject(objid);
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
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		auto obj = ctx->getSelf()->getPlayer();
		if (obj) return { obj };
		else return {};
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
	}
};

struct FinderAlias : ObjectFinder {
	int aliasIndex;
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		auto &alias = ctx->gameState->aliases[aliasIndex];
		ObjectFinderResult res;
		for (auto &ref : alias)
			if (auto* obj = ref.getFrom<Server>(); obj && obj->isInteractable())
				res.push_back(obj);
		return res;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		aliasIndex = gs.aliases.readIndex(gsf);
	}
	FinderAlias() {}
	FinderAlias(int aliasIndex) : aliasIndex(aliasIndex) {}
};

struct FinderController : ObjectFinder {
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		return { ctx->getSelf()->getParent() };
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
	}
};

struct FinderTarget : ObjectFinder {
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		if (ctx->isServer()) {
			SrvScriptContext* sctx = (SrvScriptContext*)ctx;
			if (auto obj = sctx->get(ctx->target))
				return { obj };
			if (Order* order = sctx->getSelf()->orderConfig.getCurrentOrder())
				if (CommonGameObject* target = order->getCurrentTask()->target.get())
					return { target };
			return {};
		}
		else if(ctx->isClient()) {
			auto obj = ctx->get(ctx->target);
			if (obj) return { obj };
			else return {};
		}
		return {};
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
	}
};

struct FinderResults : ObjectFinder {
	int ofd;
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		if (ofd == -1) return {}; // TODO: Remove this after implementing exception handling
		return ctx->gameState->gameSet->objectFinderDefinitions[ofd]->eval(ctx);
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		ofd = gs.objectFinderDefinitions.readIndex(gsf);
	}
};

struct FinderAssociates : ObjectFinder {
	int category;
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		if (!ctx->isServer()) return fail(ctx);
		SrvScriptContext* sctx = (SrvScriptContext*)ctx;
		const auto &set = sctx->getSelf()->associates[category];
		ObjectFinderResult vec;
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
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		if (!ctx->isServer()) return fail(ctx);
		SrvScriptContext* sctx = (SrvScriptContext*)ctx;
		const auto &set = sctx->getSelf()->associators[category];
		ObjectFinderResult vec;
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
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		Server *server = Server::instance;
		ObjectFinderResult res;
		for (auto &it : server->getLevel()->children) {
			if ((it.first & 63) == Tags::GAMEOBJCLASS_PLAYER)
				for (CommonGameObject *player : it.second)
					if (auto _ = ctx->change(ctx->candidate, player->dyncast<CommonGameObject>()))
						if (server->gameSet->equations[equation]->eval(ctx))
							res.push_back(player->dyncast<CommonGameObject>());
		}
		return res;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		equation = gs.equations.readIndex(gsf);
	}
};

struct FinderCandidate : ObjectFinder {
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		auto obj = ctx->get(ctx->candidate);
		if (obj) return { obj };
		else return {};
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {}
};

struct FinderCreator : ObjectFinder {
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		auto obj = ctx->get(ctx->creator);
		if (obj) return { obj };
		else return {};
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {}
};

struct FinderPackageSender : ObjectFinder {
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		auto obj = ctx->get(ctx->packageSender);
		if (obj) return { obj };
		else return {};
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {}
};

struct FinderSequenceExecutor : ObjectFinder {
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		auto obj = ctx->get(ctx->sequenceExecutor);
		if (obj) return { obj };
		else return {};
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {}
};

struct FinderNearestToSatisfy : ObjectFinder {
	std::unique_ptr<ValueDeterminer> vdcond, vdradius;
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		using AnyGO = CommonGameObject;
		float radius = vdradius->eval(ctx);
		NNSearch search;
		search.start(ctx->gameState, ctx->getSelf()->position, radius);
		ObjectFinderResult res;
		while (AnyGO* obj = (AnyGO*)search.next()) {
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

struct FinderLevel : ObjectFinder {
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		return { ctx->gameState->getLevel() };
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {}
};

struct FinderDisabledAssociates : ObjectFinder {
	int category;
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		if (!ctx->isServer()) return fail(ctx);
		SrvScriptContext* sctx = (SrvScriptContext*)ctx;
		const auto& set = sctx->getSelf()->associates[category];
		ObjectFinderResult vec;
		for (auto& ref : set)
			if (ref && ref->disableCount > 0) // terminated?
				vec.push_back(ref);
		return vec;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		category = gs.associations.readIndex(gsf);
	}
};

struct FinderReferencers : ObjectFinder {
	int category;
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		if (!ctx->isServer()) return fail(ctx);
		SrvScriptContext* sctx = (SrvScriptContext*)ctx;
		std::set<CommonGameObject*> vec;
		for (CommonGameObject* obj : sctx->getSelf()->referencers) {
			if (obj) {
				Order* order = ((ServerGameObject*)obj)->orderConfig.getCurrentOrder();
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
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		auto obj = ctx->get(ctx->orderGiver);
		if (obj) return { obj };
		else return {};
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {}
};

struct FinderBeingTransferredToMe : ObjectFinder {
	std::unique_ptr<ObjectFinder> finder;
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		return {};
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct FinderCollisionSubject : ObjectFinder {
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		auto obj = ctx->get(ctx->collisionSubject);
		if (obj) return { obj };
		else return {};
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {}
};

struct FinderUser : ObjectFinder {
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		return {};
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {}
};

struct FinderSelectedObject : ObjectFinder {
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		auto obj = ctx->get(ctx->selectedObject);
		if (obj) return { obj };
		else return {};
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {}
};

struct FinderAgAllOfType : ObjectFinder {
	GameObjBlueprint* blueprint;
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		uint32_t bpid = blueprint->getFullId();
		ObjectFinderResult vec;
		auto walk = [bpid, &vec](CommonGameObject* obj, auto& rec) -> void {
			for (const auto& subords : obj->children) {
				if (subords.first == bpid) {
					for (CommonGameObject* sub : subords.second) {
						vec.push_back((CommonGameObject*)sub);
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
	static thread_local ObjectFinderResult results;
	bool eligible(CommonGameObject *obj, ScriptContext *ctx) {
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
	void walk(CommonGameObject *obj, ScriptContext* ctx) {
		for (auto &it : obj->children) {
			for (CommonGameObject* cchild : it.second) {
				CommonGameObject* child = cchild->dyncast<CommonGameObject>();
				if (eligible(child, ctx))
					results.push_back(child);
				if (!immediateLevel)
					walk(child, ctx);
			}
		}
	}
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		auto vec = finder->eval(ctx);
		results.clear();
		for (CommonGameObject *par : vec) {
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

thread_local decltype(FinderSubordinates::results) FinderSubordinates::results; // this could be avoided using "inline" static members from C++17

struct FinderNSubs : ObjectFinder {
	DynArray<std::unique_ptr<ObjectFinder>> finders;
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		finders.resize(gsf.nextInt());
		for (auto &finder : finders)
			finder.reset(ReadFinderNode(gsf, gs));
	}
};

struct FinderUnion : FinderNSubs {
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		std::unordered_set<CommonGameObject*> set;
		for (auto &finder : finders) {
			auto vec = finder->eval(ctx);
			set.insert(vec.begin(), vec.end());
		}
		return { set.begin(), set.end() };
	}
};

struct FinderIntersection : FinderNSubs {
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
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

struct FinderChain : FinderNSubs {
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		auto _ = ctx->change(ctx->chainOriginalSelf, ctx->getSelf());
		ObjectFinderResult vec = { ctx->getSelf() };
		for (auto &finder : finders) {
			auto _ = ctx->changeSelf(vec[0]);
			vec = finder->eval(ctx);
			if (vec.empty())
				break;
		}
		return vec;
	}
};

struct FinderAlternative : FinderNSubs {
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		for (auto &finder : finders) {
			auto vec = finder->eval(ctx);
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
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		auto vec = finder->eval(ctx);
		int limit = (int)count->eval(ctx), num = 0;
		decltype(vec) res;
		for (CommonGameObject *obj : vec) {
			auto _ = ctx->change(ctx->candidate, obj);
			if (ctx->gameState->gameSet->equations[equation]->eval(ctx) > 0.0f) {
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
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		auto vec = finder->eval(ctx);
		decltype(vec) res;
		for (CommonGameObject *obj : vec) {
			auto _ = ctx->change(ctx->candidate, obj);
			if (ctx->gameState->gameSet->equations[equation]->eval(ctx) > 0.0f) {
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
	bool useOriginalSelf = false;
	int relationship = 0;
	int classFilter = 0;

	template<typename AnyGO> bool eligible(AnyGO* obj, AnyGO* refplayer, ScriptContext* ctx) {
		if (!obj->isInteractable()) return false;
		AnyGO* objplayer = obj->getPlayer();
		switch (relationship) {
		case 1: if (objplayer != refplayer) return false; break;
		case 2: if (objplayer != refplayer && ctx->gameState->getDiplomaticStatus(objplayer, refplayer) >= 1) return false; break;
		case 3: if (objplayer == refplayer || ctx->gameState->getDiplomaticStatus(objplayer, refplayer) < 2) return false; break;
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
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		using AnyGO = CommonGameObject;
		float radius = vdradius->eval(ctx);
		AnyGO* player = (useOriginalSelf ? ctx->get(ctx->chainOriginalSelf) : ctx->getSelf())->getPlayer();
		NNSearch search;
		search.start(ctx->gameState, ctx->getSelf()->position, radius);
		ObjectFinderResult res;
		while (AnyGO* obj = (AnyGO*)search.next())
			if (eligible(obj, player, ctx))
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
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		int count = 0;
		if (vdCount)
			count = (int)vdCount->eval(ctx);
		auto vec = finder->eval(ctx);
		ValueDeterminer *vd = Server::instance->gameSet->equations[equation];

		std::vector<std::pair<float,CommonGameObject*>> values(vec.size());
		for (size_t i = 0; i < vec.size(); i++) {
			auto _ = ctx->change(ctx->candidate, vec[i]);
			values[i] = { vd->eval(ctx), vec[i] };
		}
		std::sort(values.begin(), values.end(), [this](auto & a, auto & b) {return byHighest ? (a.first > b.first) : (a.first < b.first); });
		if (count <= 0 || (size_t)count > vec.size())
			count = vec.size();
		ObjectFinderResult res;
		res.reserve(count);
		for (int i = 0; i < count; i++)
			res.push_back(values[i].second);
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
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		auto vec = finder->eval(ctx);
		float bestDist = INFINITY;
		CommonGameObject *bestObj = nullptr;
		for (CommonGameObject *obj : vec) {
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

struct FinderTileRadius : ObjectFinder {
	std::unique_ptr<ValueDeterminer> vdradius;
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		using AnyGO = CommonGameObject;
		float radius = vdradius->eval(ctx);
		NNSearch search;
		search.start(ctx->gameState, ctx->getSelf()->position, radius);
		ObjectFinderResult res;
		while (AnyGO* obj = (AnyGO*)search.next())
			if (obj->isInteractable())
				res.push_back(obj);
		return res;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		vdradius.reset(ReadValueDeterminer(gsf, gs));
	}
};

struct FinderDiscoveredUnits : ObjectFinder {
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		// TODO
		return {};
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {}
};

struct FinderRandomSelection : ObjectFinder {
	std::unique_ptr<ValueDeterminer> vCount;
	std::unique_ptr<ObjectFinder> finder;
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
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

struct FinderFilterCandidates : ObjectFinder {
	std::unique_ptr<ValueDeterminer> condition;
	std::unique_ptr<ObjectFinder> finder;
	virtual ObjectFinderResult eval(ScriptContext* ctx) override {
		auto vec = finder->eval(ctx);
		decltype(vec) res;
		for (CommonGameObject* obj : vec) {
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
