#include "position.h"
#include "../util/vecmat.h"
#include "../server.h"
#include "finder.h"
#include <memory>
#include "../util/util.h"
#include "../util/GSFileParser.h"
#include "../tags.h"

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
		auto vec = finder->eval(self);
		OrientedPosition sum;
		for (ServerGameObject *obj : vec)
			sum += {obj->position};
		return sum / vec.size();
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

PositionDeterminer * PositionDeterminer::createFrom(GSFileParser & gsf, GameSet & gs)
{
	PositionDeterminer *pos;
	std::string tag = gsf.nextString();
	switch (Tags::POSITION_tagDict.getTagID(tag.c_str())) {
	case Tags::POSITION_LOCATION_OF: pos = new PDLocationOf; break;
	case Tags::POSITION_ABSOLUTE_POSITION: pos = new PDAbsolutePosition; break;
	default: pos = new PDUnknown; break;
	}
	pos->parse(gsf, gs);
	return pos;
}
