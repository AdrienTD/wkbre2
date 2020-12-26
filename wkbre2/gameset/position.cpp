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

namespace {
	OrientedPosition PosFromObjVec(const std::vector<ServerGameObject*> &vec) {
		OrientedPosition sum;
		for (ServerGameObject *obj : vec)
			sum += {obj->position};
		return sum / vec.size();
	}
}

struct PDUnknown : PositionDeterminer {
	virtual OrientedPosition eval(ServerGameObject * self) override
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
	virtual OrientedPosition eval(ServerGameObject * self) override
	{
		return PosFromObjVec(finder->eval(self));
	}
	virtual void parse(GSFileParser &gsf, GameSet &gs) override
	{
		finder.reset(ReadFinder(gsf, gs));
	}
};

struct PDAbsolutePosition : PositionDeterminer {
	OrientedPosition opos;
	virtual OrientedPosition eval(ServerGameObject * self) override
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
		opos = { Vector3(x, y, z), a, b };
	}
};

struct PDCentreOfMap : PositionDeterminer {
	virtual OrientedPosition eval(ServerGameObject * self) override {
		Vector3 pos;
		std::tie(pos.x, pos.z) = Server::instance->terrain->getPlayableArea();
		pos.y = Server::instance->terrain->getHeight(pos.x, pos.z);
		return { pos, 0.0f, 0.0f };
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
	}
};

struct PDThisSideOf : PositionDeterminer {
	std::unique_ptr<ObjectFinder> a, b;
	std::unique_ptr<ValueDeterminer> v;
	virtual OrientedPosition eval(ServerGameObject * self) override {
		OrientedPosition o = PosFromObjVec(a->eval(self)), p = PosFromObjVec(b->eval(self));
		Vector3 d = (o.position - p.position).normal();
		return { p.position + d * v->eval(self), atan2f(d.x, -d.z), 0.0f };
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
	virtual OrientedPosition eval(ServerGameObject * self) override {
		// TODO
		return p->eval(self);
	}
	virtual void parse(GSFileParser & gsf, GameSet & gs) override {
		a.reset(ReadFinder(gsf, gs));
		p.reset(PositionDeterminer::createFrom(gsf, gs));
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
	case Tags::POSITION_NEAREST_VALID_POSITION_FOR: pos = new PDNearestValidPositionFor; break;
	default: pos = new PDUnknown; break;
	}
	pos->parse(gsf, gs);
	return pos;
}
