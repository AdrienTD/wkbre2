// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "values.h"
#include "gameset.h"
#include "../util/GSFileParser.h"
#include "../tags.h"
#include "../util/util.h"
#include <string>
#include "finder.h"
#include <algorithm>
#include "../server.h"
#include "../client.h"
#include "CommonEval.h"
#include "position.h"
#include "ScriptContext.h"
#include "../terrain.h"

namespace {
	float RandomFromZeroToOne() { return (float)(rand() & 0xFFFF) / 32768.0f; }
}

//namespace Script
//{

float ValueDeterminer::fail(ScriptContext* ctx) {
	ferr("Invalid value determiner call from %s", ctx->gameState->getProgramName());
	return 0.0f;
}

struct ValueUnknown : ValueDeterminer {
	std::string name;
	virtual float eval(ScriptContext* ctx) override { ferr("Unknown value determiner %s called from the %s!", name.c_str(), ctx->gameState->getProgramName()); return 0.0f; }
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {}
	ValueUnknown(const std::string& name) : name(name) {}
};

struct ValueConstant : ValueDeterminer {
	float value;
	virtual float eval(ScriptContext* ctx) override { return value; }
	virtual void parse(GSFileParser &gsf, GameSet &gs) override { value = gsf.nextFloat(); }
	ValueConstant() {}
	ValueConstant(float value) : value(value) {}
};

struct ValueItemValue : ValueDeterminer {
	int item;
	std::unique_ptr<ObjectFinder> finder;
	float eval(ScriptContext *ctx) override {
		if (CommonGameObject* obj = finder->getFirst(ctx))
			return obj->getItem(item);
		return 0.0f;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		item = gs.items.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
	ValueItemValue() {}
	ValueItemValue(int item, ObjectFinder *finder) : item(item), finder(finder) {}
};

struct ValueObjectId_WKO : ValueDeterminer {
	std::unique_ptr<ObjectFinder> finder1, finder2;
	virtual float eval(ScriptContext* ctx) override {
		CommonGameObject* obj1 = finder1->getFirst(ctx);
		CommonGameObject* obj2 = finder2->getFirst(ctx);
		if (obj1 && obj2)
			return (obj1 == obj2) ? 1.0f : 0.0f;
		return 0.0f;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder1.reset(ReadFinder(gsf, gs));
		finder2.reset(ReadFinder(gsf, gs));
	}
};

struct ValueObjectId_Battles : ValueDeterminer {
	std::unique_ptr<ObjectFinder> finder;
	virtual float eval(ScriptContext* ctx) override {
		if (CommonGameObject *obj = finder->getFirst(ctx))
			return (float)obj->id;
		return 0.0f;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
	ValueObjectId_Battles() {}
	ValueObjectId_Battles(ObjectFinder *finder) : finder(finder) {}
};

struct ValueObjectClass : ValueDeterminer {
	int objclass;
	std::unique_ptr<ObjectFinder> finder;
	virtual float eval(ScriptContext* ctx) override {
		auto vec = finder->eval(ctx);
		return !vec.empty() && std::all_of(vec.begin(), vec.end(), [this](CommonGameObject *obj) {return obj->blueprint->bpClass == objclass; });
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		objclass = Tags::GAMEOBJCLASS_tagDict.getTagID(gsf.nextString().c_str());
		finder.reset(ReadFinder(gsf, gs));
	}
	ValueObjectClass() {}
	ValueObjectClass(int objclass, ObjectFinder *finder) : objclass(objclass), finder(finder) {}
};

struct ValueEquationResult : ValueDeterminer {
	int equation;
	std::unique_ptr<ObjectFinder> finder;
	virtual float eval(ScriptContext* ctx) override {
		CommonGameObject *obj = finder->getFirst(ctx);
		if (!obj) return 0.0f;
		auto _ = ctx->changeSelf(obj);
		return ctx->gameState->gameSet->equations[equation]->eval(ctx);
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		equation = gs.equations.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
	//ValueEquationResult() {}
	//ValueEquationResult(int equation, ObjectFinder *finder) : equation(equation), finder(finder) {}
};

struct ValueIsSubsetOf : ValueDeterminer {
	std::unique_ptr<ObjectFinder> fnd_sub, fnd_super;
	virtual float eval(ScriptContext* ctx) override {
		auto sub = fnd_sub->eval(ctx);
		auto super = fnd_super->eval(ctx);
		if (sub.empty() || super.empty())
			return 0.0f;
		for (auto *obj : sub)
			if (std::find(super.begin(), super.end(), obj) == super.end())
				return 0.0f;
		return 1.0f;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		fnd_sub.reset(ReadFinder(gsf, gs));
		fnd_super.reset(ReadFinder(gsf, gs));
	}
};

struct ValueNumObjects : ValueDeterminer {
	std::unique_ptr<ObjectFinder> finder;
	virtual float eval(ScriptContext* ctx) override {
		return (float)finder->eval(ctx).size();
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ValueCanReach : ValueDeterminer {
	std::unique_ptr<ObjectFinder> finder;
	std::unique_ptr<PositionDeterminer> source;
	virtual float eval(ScriptContext* ctx) override {
		// everything is reachable from anywhere for now
		return 1.0f;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		finder.reset(ReadFinder(gsf, gs));
		source.reset(PositionDeterminer::createFrom(gsf, gs));
	}
};

struct ValueIsAccessible : ValueDeterminer {
	std::unique_ptr<ObjectFinder> finder1, finder2;
	virtual float eval(ScriptContext* ctx) override {
		// everything is reachable from anywhere for now
		return 1.0f;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder1.reset(ReadFinder(gsf, gs));
		finder2.reset(ReadFinder(gsf, gs));
	}
};

struct ValueHasAppearance : ValueDeterminer {
	std::unique_ptr<ObjectFinder> finder;
	int appear;
	virtual float eval(ScriptContext* ctx) override {
		for (CommonGameObject *obj : finder->eval(ctx))
			if (obj->appearance != appear)
				return 0.0f;
		return 1.0f;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		finder.reset(ReadFinder(gsf, gs));
		appear = gs.appearances.readIndex(gsf);
	}
};

struct ValueSamePlayer : ValueDeterminer {
	std::unique_ptr<ObjectFinder> fnd1, fnd2;
	virtual float eval(ScriptContext* ctx) override {
		auto vec1 = fnd1->eval(ctx);
		auto vec2 = fnd2->eval(ctx);
		if (vec1.empty() || vec2.empty()) return 0.0f;
		for (CommonGameObject *b : vec2) {
			CommonGameObject *bp = b->getPlayer();
			if (std::find_if(vec1.begin(), vec1.end(), [&bp](CommonGameObject * a) {return a->getPlayer() == bp;}) == vec1.end())
				return 0.0f;
		}
		return 1.0f;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		fnd1.reset(ReadFinder(gsf, gs));
		fnd2.reset(ReadFinder(gsf, gs));
	}
};

struct ValueObjectType : ValueDeterminer {
	GameObjBlueprint *type;
	std::unique_ptr<ObjectFinder> finder;
	virtual float eval(ScriptContext* ctx) override {
		auto vec = finder->eval(ctx);
		if (vec.empty()) return 0.0f;
		for (auto *obj : vec)
			if (obj->blueprint != type)
				return 0.0f;
		return 1.0f;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		type = gs.readObjBlueprintPtr(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ValueDistanceBetween : ValueDeterminer {
	bool takeHorizontal, takeVertical;
	std::unique_ptr<ObjectFinder> fnd1, fnd2;
	Vector3 centre(const ObjectFinderResult& vec) {
		if (vec.empty()) return {};
		Vector3 sum;
		for (CommonGameObject* obj : vec)
			sum += obj->position;
		return sum / (float)vec.size();
	}
	virtual float eval(ScriptContext* ctx) override {
		Vector3 v = centre(fnd1->eval(ctx)) - centre(fnd2->eval(ctx));
		float dist = 0.0f;
		if (takeHorizontal) dist += v.x * v.x + v.z * v.z;
		if (takeVertical) dist += v.y * v.y;
		return std::sqrt(dist);
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		auto tag = gsf.nextString();
		if (tag == "HORIZONTAL")	{ takeHorizontal = true;  takeVertical = false; }
		else if (tag == "3D")		{ takeHorizontal = true;  takeVertical = true; }
		else if (tag == "VERTICAL")	{ takeHorizontal = false; takeVertical = true; }
		fnd1.reset(ReadFinder(gsf, gs));
		fnd2.reset(ReadFinder(gsf, gs));
	}
};

struct ValueValueTagInterpretation : ValueDeterminer {
	int valueTag;
	std::unique_ptr<ObjectFinder> fnd;
	virtual float eval(ScriptContext* ctx) override {
		CommonGameObject *obj = fnd->getFirst(ctx);
		if (!obj) return 0.0f;
		// TODO: Type specific interpretations
		ValueDeterminer* vd = nullptr;
		auto it = obj->blueprint->mappedValueTags.find(valueTag);
		if (it != obj->blueprint->mappedValueTags.end())
			vd = it->second;
		else
			vd = ctx->gameState->gameSet->valueTags[valueTag].get();
		if (!vd) return 0.0f;
		auto _ = ctx->changeSelf(obj);
		return vd->eval(ctx);
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		valueTag = gs.valueTags.readIndex(gsf);
		fnd.reset(ReadFinder(gsf, gs));
	}
};

struct ValueDiplomaticStatusAtLeast : ValueDeterminer {
	int status;
	std::unique_ptr<ObjectFinder> a, b;
	virtual float eval(ScriptContext* ctx) override {
		auto objlist1 = a->eval(ctx);
		auto objlist2 = b->eval(ctx);
		if (objlist1.empty() || objlist2.empty())
			return 0.0f; // TODO: WARNING
		for (auto* obj1 : objlist1) {
			for (auto* obj2 : objlist2) {
				if (ctx->gameState->getDiplomaticStatus(obj1->getPlayer(), obj2->getPlayer()) > status)
					return 0.0f;
			}
		}
		return 1.0f;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		status = gs.diplomaticStatuses.readIndex(gsf);
		a.reset(ReadFinder(gsf, gs));
		b.reset(ReadFinder(gsf, gs));
	}
};

struct ValueAreAssociated : ValueDeterminer {
	int category;
	std::unique_ptr<ObjectFinder> a, b;
	virtual float eval(ScriptContext* ctx) override {
		if (!ctx->isServer()) return fail(ctx);
		ServerGameObject *x = (ServerGameObject*)a->getFirst(ctx), *y = (ServerGameObject*)b->getFirst(ctx);
		if (!(x && y)) return 0.0f;
		return x->associates[category].count(y) ? 1.0f : 0.0f;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		a.reset(ReadFinder(gsf, gs));
		category = gs.associations.readIndex(gsf);
		b.reset(ReadFinder(gsf, gs));
	}
};

struct ValueBlueprintItemValue : ValueDeterminer {
	int item;
	GameObjBlueprint *type;
	virtual float eval(ScriptContext* ctx) override {
		auto it = type->startItemValues.find(item);
		if (it != type->startItemValues.end())
			return it->second;
		else return 0.0f;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		item = gs.items.readIndex(gsf);
		type = gs.readObjBlueprintPtr(gsf);
	}
};

struct ValueTotalItemValue : ValueDeterminer {
	int item;
	std::unique_ptr<ObjectFinder> finder;
	virtual float eval(ScriptContext* ctx) override {
		float sum = 0.0f;
		for (CommonGameObject* obj : finder->eval(ctx))
			sum += obj->getItem(item);
		return sum;
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		item = gs.items.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ValueIsIdle : ValueDeterminer {
	std::unique_ptr<ObjectFinder> finder;
	virtual float eval(ScriptContext* ctx) override {
		if (!ctx->isServer()) return fail(ctx);
		auto vec = finder->eval(ctx);
		return std::all_of(vec.begin(), vec.end(), [](CommonGameObject* obj) {return ((ServerGameObject*)obj)->orderConfig.orders.empty(); });
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ValueFinderResultsCount : ValueDeterminer {
	int ofd;
	std::unique_ptr<ObjectFinder> finder;
	virtual float eval(ScriptContext* ctx) override {
		CommonGameObject* obj = finder->getFirst(ctx);
		if (!obj) return 0.0f;
		auto _ = ctx->changeSelf(obj);
		return (float)Server::instance->gameSet->objectFinderDefinitions[ofd]->eval(ctx).size();
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		ofd = gs.objectFinderDefinitions.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ValueHasDirectLineOfSightTo : ValueDeterminer {
	std::unique_ptr<ObjectFinder> finder1;
	std::unique_ptr<PositionDeterminer> pos;
	std::unique_ptr<ObjectFinder> finder2;
	virtual float eval(ScriptContext* ctx) override {
		// TODO
		return 1.0f;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder1.reset(ReadFinder(gsf, gs));
		pos.reset(PositionDeterminer::createFrom(gsf, gs));
		finder2.reset(ReadFinder(gsf, gs));
	}
};

struct ValueWaterBeneath : ValueDeterminer {
	std::unique_ptr<ObjectFinder> finder;
	virtual float eval(ScriptContext* ctx) override {
		CommonGameObject* obj = finder->getFirst(ctx);
		int tx = (int)(obj->position.x / 5.0f), tz = (int)(obj->position.z / 5.0f);
		if (auto* tile = ctx->gameState->terrain->getPlayableTile(tx, tz)) {
			if (tile->fullOfWater)
				return 1.0f;
		}
		return 0.0f;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ValueAngleBetween : ValueDeterminer {
	int mode;
	std::unique_ptr<ObjectFinder> a, b;
	virtual float eval(ScriptContext* ctx) override {
		// TODO (apparently it always returns 0?)
		return 0.0f;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		gsf.nextString();
		a.reset(ReadFinder(gsf, gs));
		b.reset(ReadFinder(gsf, gs));
	}
};

struct ValueIsMusicPlaying : ValueDeterminer {
	std::unique_ptr<ObjectFinder> finder;
	virtual float eval(ScriptContext* ctx) override {
		if (!ctx->isServer()) return fail(ctx);
		CommonGameObject* obj = finder->getFirst(ctx);
		if (!obj) return 0.0f;
		ServerGameObject* player = (ServerGameObject*)obj->getPlayer();
		return player->isMusicPlaying ? 1.0f : 0.0f;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ValueCurrentlyDoingOrder : ValueDeterminer {
	int category;
	std::unique_ptr<ObjectFinder> finder;
	virtual float eval(ScriptContext* ctx) override {
		if (ctx->isServer()) {
			SrvScriptContext* sctx = (SrvScriptContext*)ctx;
			auto vec = finder->eval(sctx);
			if (vec.empty()) return 0.0f;
			return std::all_of(vec.begin(), vec.end(), [this](ServerGameObject* obj) {
				auto* order = obj->orderConfig.getCurrentOrder();
				return order && (order->blueprint->category == category);
				});
		}
		else if (ctx->isClient()) {
			CliScriptContext* cctx = (CliScriptContext*)ctx;
			auto vec = finder->eval(cctx);
			if (vec.empty()) return 0.0f;
			return std::all_of(vec.begin(), vec.end(), [this,ctx](ClientGameObject* obj) {
				return obj->reportedCurrentOrder != -1 && ctx->gameState->gameSet->orders[obj->reportedCurrentOrder].category == category;
			});
		}
		return 0.0f;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		category = gs.orderCategories.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ValueCurrentlyDoingTask : ValueDeterminer {
	int category;
	std::unique_ptr<ObjectFinder> finder;
	virtual float eval(ScriptContext* ctx) override {
		if (!ctx->isServer()) return fail(ctx);
		SrvScriptContext* sctx = (SrvScriptContext*)ctx;
		auto vec = finder->eval(sctx);
		if (vec.empty()) return 0.0f;
		return std::all_of(vec.begin(), vec.end(), [this](ServerGameObject* obj) {
			auto order = obj->orderConfig.getCurrentOrder();
			return order && (order->getCurrentTask()->blueprint->category == category);
			});
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		category = gs.taskCategories.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ValueTileItem : ValueDeterminer {
	int item;
	std::unique_ptr<ObjectFinder> finder;
	virtual float eval(ScriptContext* ctx) override {
		if (auto* obj = finder->getFirst(ctx)) {
			int tx = (int)(obj->position.x / 5.0f), tz = (int)(obj->position.z / 5.0f);
			if (auto* tile = ctx->gameState->terrain->getPlayableTile(tx, tz)) {
				auto& attg = ctx->gameState->gameSet->associatedTileTexGroups;
				auto it = attg.find(tile->texture->grp->name);
				if (it != attg.end()) {
					auto& vals = it->second->itemValues;
					auto vt = vals.find(item);
					if (vt != vals.end())
						return vt->second;
				}
			}
		}
		return 0.0f;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		item = gs.items.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ValueNumAssociates : ValueDeterminer {
	int category;
	std::unique_ptr<ObjectFinder> finder;
	virtual float eval(ScriptContext* ctx) override {
		if (!ctx->isServer()) return fail(ctx);
		ServerGameObject* obj = (ServerGameObject*)finder->getFirst(ctx);
		if (!obj) return 0.0f;
		auto it = obj->associates.find(category);
		if (it == obj->associates.end())
			return 0.0f;
		return (float)it->second.size();
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		category = gs.associations.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ValueNumAssociators : ValueDeterminer {
	int category;
	std::unique_ptr<ObjectFinder> finder;
	virtual float eval(ScriptContext* ctx) override {
		if (!ctx->isServer()) return fail(ctx);
		ServerGameObject* obj = (ServerGameObject*)finder->getFirst(ctx);
		if (!obj) return 0.0f;
		auto it = obj->associators.find(category);
		if (it == obj->associators.end())
			return 0.0f;
		return (float)it->second.size();
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		category = gs.associations.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ValueBuildingType : ValueDeterminer {
	int type;
	std::unique_ptr<ObjectFinder> finder;
	virtual float eval(ScriptContext* ctx) override {
		// TODO
		return 0.0f;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		type = Tags::BUILDINGTYPE_tagDict.getTagID(gsf.nextString().c_str());
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ValueNumReferencers : ValueDeterminer {
	int taskCategory;
	std::unique_ptr<ObjectFinder> finder;
	virtual float eval(ScriptContext* ctx) override {
		if (!ctx->isServer()) return fail(ctx);
		ServerGameObject* obj = (ServerGameObject*)finder->getFirst(ctx);
		if (!obj) return 0.0f;
		int count = 0;
		// FIXME risk of duplicates!!!
		for (CommonGameObject* ref : obj->referencers) {
			if (ref) {
				Order* order = ((ServerGameObject*)ref)->orderConfig.getCurrentOrder();
				if (!order) continue;
				Task* task = order->getCurrentTask();
				if (!task) continue;
				if (task->blueprint->category == taskCategory)
					count++;
			}
		}
		return (float)count;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		taskCategory = gs.taskCategories.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ValueIndexedItemValue : ValueDeterminer {
	int item;
	std::unique_ptr<ValueDeterminer> index;
	std::unique_ptr<ObjectFinder> finder;
	virtual float eval(ScriptContext* ctx) override {
		if (CommonGameObject* obj = finder->getFirst(ctx))
			return obj->getIndexedItem(item, (int)index->eval(ctx));
		return 0.0f;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		item = gs.items.readIndex(gsf);
		index.reset(ReadValueDeterminer(gsf, gs));
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ValueWithinForwardArc : ValueDeterminer {
	std::unique_ptr<ObjectFinder> fCenter;
	std::unique_ptr<ObjectFinder> fTarget;
	std::unique_ptr<ValueDeterminer> vArcAngle;
	std::unique_ptr<ValueDeterminer> vArcRadius;
	virtual float eval(ScriptContext* ctx) override {
		auto* oCenter = fCenter->getFirst(ctx);
		auto* oTarget = fTarget->getFirst(ctx);
		float arcAngle = vArcAngle->eval(ctx);
		float arcRadius = vArcRadius->eval(ctx);
		Vector3 centerDir{ sinf(oCenter->orientation.y), 0.0f, -cosf(oCenter->orientation.y) };
		Vector3 centerToTarget = oTarget->position - oCenter->position;
		float dist = centerToTarget.len2xz();
		if (dist > arcRadius)
			return 0.0f;
		if (dist <= 0.0f)
			return 1.0f; // :thinking:
		float alpha = std::acos(centerDir.dot(centerToTarget/dist));
		return std::abs(alpha) < arcAngle;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		fCenter.reset(ReadFinder(gsf, gs));
		fTarget.reset(ReadFinder(gsf, gs));
		vArcAngle.reset(ReadValueDeterminer(gsf, gs));
		vArcRadius.reset(ReadValueDeterminer(gsf, gs));
	}
};

struct ValueMapWidth : ValueDeterminer {
	virtual float eval(ScriptContext* ctx) override {
		return ctx->gameState->terrain->getPlayableArea().first;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {}
};

struct ValueMapDepth : ValueDeterminer {
	virtual float eval(ScriptContext* ctx) override {
		return ctx->gameState->terrain->getPlayableArea().second;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {}
};

struct ValueIsDisabled : ValueDeterminer {
	std::unique_ptr<ObjectFinder> finder;
	virtual float eval(ScriptContext* ctx) override {
		auto* obj = finder->getFirst(ctx);
		if (!obj) return 0.0f;
		return (obj->disableCount > 0) ? 1.0f : 0.0f;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ValueIsDiscovered : ValueDeterminer {
	std::unique_ptr<ObjectFinder> fObjects, fPlayer;
	virtual float eval(ScriptContext* ctx) override {
		// TODO
		return 1.0f;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		fObjects.reset(ReadFinder(gsf, gs));
		fPlayer.reset(ReadFinder(gsf, gs));
	}
};
struct ValueIsVisible : ValueDeterminer {
	std::unique_ptr<ObjectFinder> fObjects, fPlayer;
	virtual float eval(ScriptContext* ctx) override {
		// TODO
		return 1.0f;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		fObjects.reset(ReadFinder(gsf, gs));
		fPlayer.reset(ReadFinder(gsf, gs));
	}
};
struct ValueCouldReach : ValueDeterminer {
	GameObjBlueprint* objtype;
	std::unique_ptr<PositionDeterminer> pStart, pEnd;
	virtual float eval(ScriptContext* ctx) override {
		// TODO
		return 1.0f;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		objtype = gs.objBlueprints[Tags::GAMEOBJCLASS_CHARACTER].readPtr(gsf);
		pStart.reset(PositionDeterminer::createFrom(gsf, gs));
		pEnd.reset(PositionDeterminer::createFrom(gsf, gs));
	}
};

struct ValueAiControlled : ValueDeterminer {
	std::unique_ptr<ObjectFinder> fPlayer;
	virtual float eval(ScriptContext* ctx) override {
		// TODO
		return 0.0f;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		fPlayer.reset(ReadFinder(gsf, gs));
	}
};

struct ValueGradientInFront : ValueDeterminer {
	std::unique_ptr<ObjectFinder> fObject;
	virtual float eval(ScriptContext* ctx) override {
		// TODO
		return 0.0f;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		fObject.reset(ReadFinder(gsf, gs));
	}
};

struct ValueAverageEquationResult : ValueDeterminer {
	int equation;
	std::unique_ptr<ObjectFinder> finder;
	virtual float eval(ScriptContext* ctx) override {
		auto vec = finder->eval(ctx);
		if (vec.empty())
			return 0.0f;
		float sum = 0.0f;
		for (auto* obj : vec) {
			auto _ = ctx->changeSelf(obj);
			sum += ctx->gameState->gameSet->equations[equation]->eval(ctx);
		}
		return sum / (float)vec.size();
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		equation = gs.equations.readIndex(gsf);
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct ValueCanAffordCommission : ValueDeterminer {
	std::unique_ptr<ObjectFinder> fPlayer;
	virtual float eval(ScriptContext* ctx) override {
		// TODO
		return 1.0f;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		gsf.nextString(true);
		fPlayer.reset(ReadFinder(gsf, gs));
	}
};

ValueDeterminer *ReadValueDeterminer(::GSFileParser &gsf, const ::GameSet &gs)
{
	ValueDeterminer *vd;
	auto strtag = gsf.nextTag();
	switch (Tags::VALUE_tagDict.getTagID(strtag.c_str())) {
	case Tags::VALUE_CONSTANT: vd = new ValueConstant(gsf.nextFloat()); return vd;
	case Tags::VALUE_DEFINED_VALUE: vd = new ValueConstant(gs.definedValues.at(gsf.nextString(true))); return vd;
	case Tags::VALUE_ITEM_VALUE: vd = new ValueItemValue; break;
	case Tags::VALUE_OBJECT_ID: if (gs.version >= gs.GSVERSION_WKBATTLES) vd = new ValueObjectId_Battles; else vd = new ValueObjectId_WKO; break;
	case Tags::VALUE_OBJECT_CLASS: vd = new ValueObjectClass; break;
	case Tags::VALUE_EQUATION_RESULT: vd = new ValueEquationResult; break;
	case Tags::VALUE_IS_SUBSET_OF: vd = new ValueIsSubsetOf; break;
	case Tags::VALUE_NUM_OBJECTS: vd = new ValueNumObjects; break;
	case Tags::VALUE_CAN_REACH: vd = new ValueCanReach; break;
	case Tags::VALUE_IS_ACCESSIBLE: vd = new ValueIsAccessible; break;
	case Tags::VALUE_HAS_APPEARANCE: vd = new ValueHasAppearance; break;
	case Tags::VALUE_SAME_PLAYER: vd = new ValueSamePlayer; break;
	case Tags::VALUE_OBJECT_TYPE: vd = new ValueObjectType; break;
	case Tags::VALUE_DISTANCE_BETWEEN: vd = new ValueDistanceBetween; break;
	case Tags::VALUE_DISTANCE_BETWEEN_INCLUDING_RADIUS: vd = new ValueDistanceBetween; break; // TODO
	case Tags::VALUE_VALUE_TAG_INTERPRETATION: vd = new ValueValueTagInterpretation; break;
	case Tags::VALUE_DIPLOMATIC_STATUS_AT_LEAST: vd = new ValueDiplomaticStatusAtLeast; break;
	case Tags::VALUE_ARE_ASSOCIATED: vd = new ValueAreAssociated; break;
	case Tags::VALUE_BLUEPRINT_ITEM_VALUE: vd = new ValueBlueprintItemValue; break;
	case Tags::VALUE_TOTAL_ITEM_VALUE: vd = new ValueTotalItemValue; break;
	case Tags::VALUE_IS_IDLE: vd = new ValueIsIdle; break;
	case Tags::VALUE_FINDER_RESULTS_COUNT: vd = new ValueFinderResultsCount; break;
	case Tags::VALUE_HAS_DIRECT_LINE_OF_SIGHT_TO: vd = new ValueHasDirectLineOfSightTo; break;
	case Tags::VALUE_WATER_BENEATH: vd = new ValueWaterBeneath; break;
	case Tags::VALUE_ANGLE_BETWEEN: vd = new ValueAngleBetween; break;
	case Tags::VALUE_IS_MUSIC_PLAYING: vd = new ValueIsMusicPlaying; break;
	case Tags::VALUE_CURRENTLY_DOING_ORDER: vd = new ValueCurrentlyDoingOrder; break;
	case Tags::VALUE_CURRENTLY_DOING_TASK: vd = new ValueCurrentlyDoingTask; break;
	case Tags::VALUE_TILE_ITEM: vd = new ValueTileItem; break;
	case Tags::VALUE_NUM_ASSOCIATES: vd = new ValueNumAssociates; break;
	case Tags::VALUE_NUM_ASSOCIATORS: vd = new ValueNumAssociators; break;
	case Tags::VALUE_BUILDING_TYPE: vd = new ValueBuildingType; break;
	case Tags::VALUE_BUILDING_TYPE_OPERAND: vd = new ValueBuildingType; break;
	case Tags::VALUE_NUM_REFERENCERS: vd = new ValueNumReferencers; break;
	case Tags::VALUE_INDEXED_ITEM_VALUE: vd = new ValueIndexedItemValue; break;
	case Tags::VALUE_WITHIN_FORWARD_ARC: vd = new ValueWithinForwardArc; break;
	case Tags::VALUE_MAP_WIDTH: vd = new ValueMapWidth; break;
	case Tags::VALUE_MAP_DEPTH: vd = new ValueMapDepth; break;
	case Tags::VALUE_IS_DISABLED: vd = new ValueIsDisabled; break;
	case Tags::VALUE_IS_DISCOVERED: vd = new ValueIsDiscovered; break;
	case Tags::VALUE_IS_VISIBLE: vd = new ValueIsVisible; break;
	case Tags::VALUE_COULD_REACH: vd = new ValueCouldReach; break;
	case Tags::VALUE_AI_CONTROLLED: vd = new ValueAiControlled; break;
	case Tags::VALUE_GRADIENT_IN_FRONT: vd = new ValueGradientInFront; break;
	case Tags::VALUE_AVERAGE_EQUATION_RESULT: vd = new ValueAverageEquationResult; break;
	case Tags::VALUE_CAN_AFFORD_COMMISSION: vd = new ValueCanAffordCommission; break;
	default: vd = new ValueUnknown(strtag); break;
	}
	vd->parse(gsf, const_cast<GameSet&>(gs));
	return vd;
}

// Unary equation nodes

struct UnaryEnode : ValueDeterminer {
	std::unique_ptr<ValueDeterminer> a;
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		a.reset(ReadEquationNode(gsf, gs));
	}
};

struct EnodeNot : UnaryEnode {
	virtual float eval(ScriptContext* ctx) override { return a->eval(ctx) <= 0.0f; }
};

struct EnodeIsZero : UnaryEnode {
	virtual float eval(ScriptContext* ctx) override { return a->eval(ctx) == 0.0f; }
};

struct EnodeIsPositive : UnaryEnode {
	virtual float eval(ScriptContext* ctx) override { return a->eval(ctx) > 0.0f; }
};

struct EnodeIsNegative : UnaryEnode {
	virtual float eval(ScriptContext* ctx) override { return a->eval(ctx) < 0.0f; }
};

struct EnodeAbsoluteValue : UnaryEnode {
	virtual float eval(ScriptContext* ctx) override { return std::abs(a->eval(ctx)); }
};

struct EnodeNegate : UnaryEnode {
	virtual float eval(ScriptContext* ctx) override { return -a->eval(ctx); }
};

struct EnodeRandomUpTo : UnaryEnode {
	virtual float eval(ScriptContext* ctx) override { return RandomFromZeroToOne() * a->eval(ctx); }
};

struct EnodeRound : UnaryEnode {
	virtual float eval(ScriptContext* ctx) override { return std::round(a->eval(ctx)); }
};

// Binary equation nodes

struct BinaryEnode : ValueDeterminer {
	std::unique_ptr<ValueDeterminer> a, b;
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		a.reset(ReadEquationNode(gsf, gs));
		b.reset(ReadEquationNode(gsf, gs));
	}
};

struct EnodeAddition : BinaryEnode {
	virtual float eval(ScriptContext* ctx) override { return a->eval(ctx) + b->eval(ctx); }
};

struct EnodeSubtraction : BinaryEnode {
	virtual float eval(ScriptContext* ctx) override { return a->eval(ctx) - b->eval(ctx); }
};

struct EnodeMultiplication : BinaryEnode {
	virtual float eval(ScriptContext* ctx) override { return a->eval(ctx) * b->eval(ctx); }
};

struct EnodeDivision : BinaryEnode {
	virtual float eval(ScriptContext* ctx) override { return a->eval(ctx) / b->eval(ctx); }
};

struct EnodeAnd : BinaryEnode {
	virtual float eval(ScriptContext* ctx) override { return (a->eval(ctx) > 0.0f) && (b->eval(ctx) > 0.0f); }
};

struct EnodeOr : BinaryEnode {
	virtual float eval(ScriptContext* ctx) override { return (a->eval(ctx) > 0.0f) || (b->eval(ctx) > 0.0f); }
};

struct EnodeLessThan : BinaryEnode {
	virtual float eval(ScriptContext* ctx) override { return a->eval(ctx) < b->eval(ctx); }
};

struct EnodeLessThanOrEqualTo : BinaryEnode {
	virtual float eval(ScriptContext* ctx) override { return a->eval(ctx) <= b->eval(ctx); }
};

struct EnodeGreaterThan : BinaryEnode {
	virtual float eval(ScriptContext* ctx) override { return a->eval(ctx) > b->eval(ctx); }
};

struct EnodeGreaterThanOrEqualTo : BinaryEnode {
	virtual float eval(ScriptContext* ctx) override { return a->eval(ctx) >= b->eval(ctx); }
};

struct EnodeEquals : BinaryEnode {
	virtual float eval(ScriptContext* ctx) override { return a->eval(ctx) == b->eval(ctx); }
};

struct EnodeMax : BinaryEnode {
	virtual float eval(ScriptContext* ctx) override { return std::max(a->eval(ctx), b->eval(ctx)); }
};

struct EnodeMin : BinaryEnode {
	virtual float eval(ScriptContext* ctx) override { return std::min(a->eval(ctx), b->eval(ctx)); }
};

struct EnodeRandomInteger : BinaryEnode {
	virtual float eval(ScriptContext* ctx) override {
		int x = (int)a->eval(ctx), y = (int)b->eval(ctx);
		return (float)(rand() % (y - x + 1) + x);
	}
};

struct EnodeRandomRange : BinaryEnode {
	virtual float eval(ScriptContext* ctx) override {
		float x = a->eval(ctx), y = b->eval(ctx);
		return RandomFromZeroToOne() * (y - x) + x;
	}
};

// Ternary equation nodes

struct TernaryEnode : ValueDeterminer {
	std::unique_ptr<ValueDeterminer> a, b, c;
	virtual void parse(GSFileParser &gsf, GameSet &gs) override {
		a.reset(ReadEquationNode(gsf, gs));
		b.reset(ReadEquationNode(gsf, gs));
		c.reset(ReadEquationNode(gsf, gs));
	}
};

struct EnodeIfThenElse : TernaryEnode {
	virtual float eval(ScriptContext* ctx) override {
		if (a->eval(ctx) > 0.0f)
			return b->eval(ctx);
		else
			return c->eval(ctx);
	}
};

struct EnodeIsBetween : TernaryEnode {
	virtual float eval(ScriptContext* ctx) override {
		float x = a->eval(ctx), y = b->eval(ctx), z = c->eval(ctx);
		return (x > y && x < z) ? 1.0f : 0.0f;
	}
};

// Quaternary equation nodes

struct QuaternaryEnode : ValueDeterminer {
	std::unique_ptr<ValueDeterminer> a, b, c, d;
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		a.reset(ReadEquationNode(gsf, gs));
		b.reset(ReadEquationNode(gsf, gs));
		c.reset(ReadEquationNode(gsf, gs));
		d.reset(ReadEquationNode(gsf, gs));
	}
};

struct EnodeFrontBackLeftRight : QuaternaryEnode {
	std::unique_ptr<ObjectFinder> f_unit, f_target;
	virtual float eval(ScriptContext* ctx) override {
		CommonGameObject* unit = f_unit->getFirst(ctx);
		CommonGameObject* target = f_unit->getFirst(ctx);
		if (!(unit && target))
			return 0.0f;
		Vector3 vec = unit->position - target->position;
		if (std::abs(vec.z) > std::abs(vec.x)) {
			// Front/back
			if (vec.z >= 0.0f) return a->eval(ctx);
			else return b->eval(ctx);
		}
		else {
			// Left/right
			if (vec.x < 0.0f) return c->eval(ctx);
			else return d->eval(ctx);
		}

	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		f_unit.reset(ReadFinder(gsf, gs));
		f_target.reset(ReadFinder(gsf, gs));
		QuaternaryEnode::parse(gsf, gs);
	}
};


ValueDeterminer *ReadEquationNode(::GSFileParser &gsf, const ::GameSet &gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		std::string strtag = gsf.nextTag();
		if (strtag == "END_EQUATION")
			return new ValueUnknown("<empty EQUATION>");
		else if (strtag == "ENODE") {
			const char *oldcur = gsf.cursor;
			ValueDeterminer *vd;
			switch (Tags::ENODE_tagDict.getTagID(gsf.nextString().c_str())) {
			case Tags::ENODE_ADDITION: vd = new EnodeAddition; break;
			case Tags::ENODE_NEGATE: vd = new EnodeNegate; break;
			case Tags::ENODE_SUBTRACTION: vd = new EnodeSubtraction; break;
			case Tags::ENODE_MULTIPLICATION: vd = new EnodeMultiplication; break;
			case Tags::ENODE_DIVISION: vd = new EnodeDivision; break;
			case Tags::ENODE_NOT: vd = new EnodeNot; break;
			case Tags::ENODE_IS_ZERO: vd = new EnodeIsZero; break;
			case Tags::ENODE_IS_POSITIVE: vd = new EnodeIsPositive; break;
			case Tags::ENODE_IS_NEGATIVE: vd = new EnodeIsNegative; break;
			case Tags::ENODE_ABSOLUTE_VALUE: vd = new EnodeAbsoluteValue; break;
			case Tags::ENODE_RANDOM_UP_TO: vd = new EnodeRandomUpTo; break;
			case Tags::ENODE_ROUND: vd = new EnodeRound; break;
			case Tags::ENODE_AND: vd = new EnodeAnd; break;
			case Tags::ENODE_OR: vd = new EnodeOr; break;
			case Tags::ENODE_LESS_THAN: vd = new EnodeLessThan; break;
			case Tags::ENODE_LESS_THAN_OR_EQUAL_TO: vd = new EnodeLessThanOrEqualTo; break;
			case Tags::ENODE_GREATER_THAN: vd = new EnodeGreaterThan; break;
			case Tags::ENODE_GREATER_THAN_OR_EQUAL_TO: vd = new EnodeGreaterThanOrEqualTo; break;
			case Tags::ENODE_EQUALS: vd = new EnodeEquals; break;
			case Tags::ENODE_MAX: vd = new EnodeMax; break;
			case Tags::ENODE_MIN: vd = new EnodeMin; break;
			case Tags::ENODE_RANDOM_INTEGER: vd = new EnodeRandomInteger; break;
			case Tags::ENODE_RANDOM_RANGE: vd = new EnodeRandomRange; break;
			case Tags::ENODE_IF_THEN_ELSE: vd = new EnodeIfThenElse; break;
			case Tags::ENODE_IS_BETWEEN: vd = new EnodeIsBetween; break;
			case Tags::ENODE_FRONT_BACK_LEFT_RIGHT: vd = new EnodeFrontBackLeftRight; break;
			default: 
				gsf.cursor = oldcur;
				return ReadValueDeterminer(gsf, gs);
			}
			vd->parse(gsf, const_cast<GameSet&>(gs));
			return vd;
		}
		gsf.advanceLine();
	}
	ferr("Equation reached end of file without END_EQUATION!");
	return nullptr;
}

//}