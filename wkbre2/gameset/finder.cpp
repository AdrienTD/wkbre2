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
	default: finder = new FinderUnknown; break;
	}
	finder->parse(gsf, const_cast<GameSet&>(gs));
	return finder;
}