#define _USE_MATH_DEFINES
#include "position.h"
#include "../util/vecmat.h"
#include "../server.h"
#include "finder.h"
#include <memory>
#include "../util/util.h"
#include "../util/GSFileParser.h"
#include "../tags.h"
#include "../terrain.h"
#include <tuple>
#include "values.h"
#include <cmath>
#include "ScriptContext.h"
#include "GameObjBlueprint.h"
#include "../Model.h"
#include "gameset.h"

namespace {
	OrientedPosition PosFromObjVec(const std::vector<ServerGameObject*> &vec) {
		OrientedPosition sum;
		for (ServerGameObject *obj : vec)
			sum += {obj->position, obj->orientation};
		return sum / vec.size();
	}
}

struct PDUnknown : PositionDeterminer {
	virtual OrientedPosition eval(SrvScriptContext* ctx) override
	{
		ferr("Unknown position determiner called from Server!");
		return OrientedPosition();
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override
	{
	}
};

struct PDLocationOf : PositionDeterminer {
	std::unique_ptr<ObjectFinder> finder;
	virtual OrientedPosition eval(SrvScriptContext* ctx) override
	{
		return PosFromObjVec(finder->eval(ctx));
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override
	{
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct PDAbsolutePosition : PositionDeterminer {
	OrientedPosition opos;
	virtual OrientedPosition eval(SrvScriptContext* ctx) override
	{
		return opos;
	}
	virtual OrientedPosition eval(CliScriptContext* ctx) override
	{
		return opos;
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override
	{
		float x = gsf.nextFloat();
		float y = gsf.nextFloat();
		float z = gsf.nextFloat();
		float a = gsf.nextFloat();
		float b = gsf.nextFloat();
		opos = { Vector3(x, y, z), Vector3(a, b, 0.0f) };
	}
};

struct PDCentreOfMap : PositionDeterminer {
	virtual OrientedPosition eval(SrvScriptContext* ctx) override {
		Vector3 pos;
		std::tie(pos.x, pos.z) = Server::instance->terrain->getPlayableArea();
		pos.y = Server::instance->terrain->getHeight(pos.x, pos.z);
		return { pos };
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
	}
};

struct PDThisSideOf : PositionDeterminer {
	std::unique_ptr<ObjectFinder> a, b;
	std::unique_ptr<ValueDeterminer> v;
	virtual OrientedPosition eval(SrvScriptContext* ctx) override {
		OrientedPosition o = PosFromObjVec(a->eval(ctx)), p = PosFromObjVec(b->eval(ctx));
		Vector3 d = (o.position - p.position).normal();
		return { p.position + d * v->eval(ctx), atan2f(d.x, -d.z) };
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		a.reset(ReadFinder(gsf, gs));
		b.reset(ReadFinder(gsf, gs));
		v.reset(ReadValueDeterminer(gsf, gs));
	}
};

struct PDThisOtherSideOf : PositionDeterminer {
	std::unique_ptr<ObjectFinder> a, b;
	std::unique_ptr<ValueDeterminer> v;
	virtual OrientedPosition eval(SrvScriptContext* ctx) override {
		OrientedPosition o = PosFromObjVec(a->eval(ctx)), p = PosFromObjVec(b->eval(ctx));
		Vector3 d = (p.position - o.position).normal();
		return { p.position + d * v->eval(ctx), atan2f(d.x, -d.z) };
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		a.reset(ReadFinder(gsf, gs));
		b.reset(ReadFinder(gsf, gs));
		v.reset(ReadValueDeterminer(gsf, gs));
	}
};

struct PDNearestValidPositionFor : PositionDeterminer {
	std::unique_ptr<ObjectFinder> a;
	std::unique_ptr<PositionDeterminer> p;
	virtual OrientedPosition eval(SrvScriptContext* ctx) override {
		// TODO
		return p->eval(ctx);
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		a.reset(ReadFinder(gsf, gs));
		p.reset(PositionDeterminer::createFrom(gsf, gs));
	}
};

struct PDOutAtAngle : public PositionDeterminer
{
	std::unique_ptr<ObjectFinder> f;
	std::unique_ptr<ValueDeterminer> u, v;
	virtual OrientedPosition eval(SrvScriptContext* ctx) override {
		OrientedPosition po = PosFromObjVec(f->eval(ctx));
		po.rotation.y -= u->eval(ctx) * M_PI / 180;
		float l = v->eval(ctx);
		po.position.x += l * sin(po.rotation.y);
		po.position.z -= l * cos(po.rotation.y);
		po.rotation.y += M_PI;
		return po;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		f.reset(ReadFinder(gsf, gs));
		u.reset(ReadValueDeterminer(gsf, gs));
		v.reset(ReadValueDeterminer(gsf, gs));
	}
};

struct PDTowards : public PositionDeterminer {
	std::unique_ptr<ObjectFinder> a, b;
	std::unique_ptr<ValueDeterminer> v;
	virtual OrientedPosition eval(SrvScriptContext* ctx) override {
		OrientedPosition o = PosFromObjVec(a->eval(ctx));
		OrientedPosition p = PosFromObjVec(b->eval(ctx));
		Vector3 d = (p.position - o.position).normal2xz();
		float m = v->eval(ctx);
		return { o.position + d * m, atan2f(d.x, -d.z) };
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		a.reset(ReadFinder(gsf, gs));
		b.reset(ReadFinder(gsf, gs));
		v.reset(ReadValueDeterminer(gsf, gs));
	}
};

struct PDAwayFrom : public PositionDeterminer {
	std::unique_ptr<ObjectFinder> a, b;
	std::unique_ptr<ValueDeterminer> v;
	virtual OrientedPosition eval(SrvScriptContext* ctx) override {
		OrientedPosition o = PosFromObjVec(a->eval(ctx));
		OrientedPosition p = PosFromObjVec(b->eval(ctx));
		Vector3 d = (o.position - p.position).normal2xz();
		float m = v->eval(ctx);
		return { o.position + d * m, atan2f(d.x, -d.z) };
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		a.reset(ReadFinder(gsf, gs));
		b.reset(ReadFinder(gsf, gs));
		v.reset(ReadValueDeterminer(gsf, gs));
	}
};

struct PDInFrontOf : public PositionDeterminer {
	std::unique_ptr<ObjectFinder> f;
	std::unique_ptr<ValueDeterminer> v;
	virtual OrientedPosition eval(SrvScriptContext* ctx) override {
		OrientedPosition op = PosFromObjVec(f->eval(ctx));
		Vector3 d{ sinf(op.rotation.y), 0.0f, -cosf(op.rotation.y) };
		op.position += d * v->eval(ctx);
		return op;
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		f.reset(ReadFinder(gsf, gs));
		v.reset(ReadValueDeterminer(gsf, gs));
	}
};

struct PDOffsetFrom : public PositionDeterminer {
	std::unique_ptr<ObjectFinder> f;
	std::unique_ptr<ValueDeterminer> x, y, z;
	virtual OrientedPosition eval(SrvScriptContext* ctx) override {
		return PosFromObjVec(f->eval(ctx)) + OrientedPosition(Vector3(x->eval(ctx), y->eval(ctx), z->eval(ctx)));
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		f.reset(ReadFinder(gsf, gs));
		x.reset(ReadValueDeterminer(gsf, gs));
		y.reset(ReadValueDeterminer(gsf, gs));
		z.reset(ReadValueDeterminer(gsf, gs));
	}
};

struct PDNearestAttachmentPoint : public PositionDeterminer {
	//int attachTag;
	std::string sAttachTag;
	std::unique_ptr<ObjectFinder> finder;
	std::unique_ptr<PositionDeterminer> pos;
	std::unique_ptr<ValueDeterminer> tochoose;
	virtual OrientedPosition eval(SrvScriptContext* ctx) override {
		ServerGameObject* obj = finder->getFirst(ctx);
		if (Model* model = obj->getModel()) {
			auto target = pos->eval(ctx);
			size_t numAPs = model->getNumAPs();
			size_t bestAP = -1;
			Vector3 bestPos;
			float bestDist = std::numeric_limits<float>::infinity();
			for (size_t i = 0; i < numAPs; i++) {
				const auto& apinfo = model->getAPInfo(i);
				if (apinfo.tag.substr(0, sAttachTag.size()) == sAttachTag) {
					Vector3 appos = apinfo.staticState.position.transform(obj->getWorldMatrix());
					float dist = (target.position - appos).sqlen3();
					if (dist < bestDist) {
						bestDist = dist;
						bestAP = i;
						bestPos = appos;
					}
				}
			}
			if (bestAP != -1)
				return { bestPos };
		}
		return obj->position + Vector3(0.0f, 1.0f, 0.0f);
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		sAttachTag = gsf.nextString(true);
		finder.reset(ReadFinder(gsf, gs));
		pos.reset(PositionDeterminer::createFrom(gsf, gs));
		const char* prevcur = gsf.cursor;
		if (!gsf.eol) {
			if (gsf.nextString() == "CHOOSE_FROM_NEAREST")
				tochoose.reset(ReadValueDeterminer(gsf, gs));
			else
				gsf.cursor = prevcur;
		}
	}
};

struct PDSpawnTilePosition : PositionDeterminer {
	std::unique_ptr<ObjectFinder> finder;
	virtual OrientedPosition eval(SrvScriptContext* ctx) override {
		// TODO
		return PosFromObjVec(finder->eval(ctx));
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct PDMatchingOffset : PositionDeterminer {
	std::unique_ptr<PositionDeterminer> cp;
	std::unique_ptr<PositionDeterminer> cr;
	std::unique_ptr<PositionDeterminer> co;

	virtual OrientedPosition eval(SrvScriptContext* ctx) override {
		OrientedPosition pp = cp->eval(ctx);
		OrientedPosition pr = cr->eval(ctx);
		OrientedPosition po = co->eval(ctx);

		Vector3 v = po.position - pr.position;
		float vo = atan2f(v.x, -v.z);
		float ro = vo - pr.rotation.y;
		float rl = v.len2xz();

		ro += pp.rotation.y;
		Vector3 nv = Vector3(sinf(ro) * rl, 0.0f, cosf(ro) * rl);
		return { pp.position + nv };
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
		cp.reset(PositionDeterminer::createFrom(gsf, gs));
		cr.reset(PositionDeterminer::createFrom(gsf, gs));
		co.reset(PositionDeterminer::createFrom(gsf, gs));
	}
};

struct PDFiringAttachmentPoint : PositionDeterminer {
	virtual OrientedPosition eval(SrvScriptContext* ctx) override {
		ServerGameObject* obj = ctx->self.get();
		Model* model = obj->blueprint->getModel(obj->subtype, obj->appearance, obj->animationIndex, obj->animationVariant);
		if (model) {
			size_t numAPs = model->getNumAPs();
			for (size_t i = 0; i < numAPs; i++) {
				if (model->getAPInfo(i).tag.substr(0, 3) == "SP_") {
					Matrix mat = obj->getWorldMatrix();
					return model->getAPState(i, obj->animStartTime).position.transform(mat);
				}
			}
		}
		return obj->position + Vector3(0, 1, 0);
	}
	virtual void parse(GSFileParser& gsf, GameSet& gs) override {
	}
};

PositionDeterminer * PositionDeterminer::createFrom(GSFileParser & gsf, GameSet & gs)
{
	PositionDeterminer *pos;
	std::string tag = gsf.nextString();
	switch (Tags::POSITION_tagDict.getTagID(tag.c_str())) {
	case Tags::POSITION_LOCATION_OF: pos = new PDLocationOf; break;
	case Tags::POSITION_ABSOLUTE_POSITION: pos = new PDAbsolutePosition; break;
	case Tags::POSITION_CENTRE_OF_MAP: pos = new PDCentreOfMap; break;
	case Tags::POSITION_THIS_SIDE_OF: pos = new PDThisSideOf; break;
	case Tags::POSITION_THE_OTHER_SIDE_OF: pos = new PDThisOtherSideOf; break;
	case Tags::POSITION_NEAREST_VALID_POSITION_FOR: pos = new PDNearestValidPositionFor; break;
	case Tags::POSITION_OUT_AT_ANGLE: pos = new PDOutAtAngle; break;
	case Tags::POSITION_TOWARDS: pos = new PDTowards; break;
	case Tags::POSITION_AWAY_FROM: pos = new PDAwayFrom; break;
	case Tags::POSITION_IN_FRONT_OF: pos = new PDInFrontOf; break;
	case Tags::POSITION_OFFSET_FROM: pos = new PDOffsetFrom; break;
	case Tags::POSITION_NEAREST_ATTACHMENT_POINT: pos = new PDNearestAttachmentPoint; break;
	case Tags::POSITION_SPAWN_TILE_POSITION: pos = new PDSpawnTilePosition; break;
	case Tags::POSITION_MATCHING_OFFSET: pos = new PDMatchingOffset; break;
	case Tags::POSITION_FIRING_ATTACHMENT_POINT: pos = new PDFiringAttachmentPoint; break;
	default: pos = new PDUnknown; break;
	}
	pos->parse(gsf, gs);
	return pos;
}
