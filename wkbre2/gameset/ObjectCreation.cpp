// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "ObjectCreation.h"
#include "util/GSFileParser.h"
#include "gameset.h"
#include "finder.h"
#include "position.h"
#include "../server.h"
#include "ScriptContext.h"
#include "util/util.h"

void ObjectCreation::parse(GSFileParser & gsf, const GameSet & gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		auto tag = gsf.nextTag();
		if (tag == "TYPE_TO_CREATE") {
			typeToCreate = gs.readObjBlueprintPtr(gsf);
		}
		else if (tag == "MAPPED_TYPE_TO_CREATE") {
			mappedTypeToCreate = gs.typeTags.readIndex(gsf);
			mappedTypeFrom = ReadFinder(gsf, gs);
		}
		else if (tag == "CONTROLLER") {
			controller = ReadFinder(gsf, gs);
		}
		else if (tag == "CREATE_AT") {
			createAt = PositionDeterminer::createFrom(gsf, gs);
		}
		else if (tag == "POST_CREATION_SEQUENCE") {
			postCreationSequence.init(gsf, gs, "END_POST_CREATION_SEQUENCE");
		}
		else if (tag == "SUBORDINATES") {
			subordinates = ReadFinder(gsf, gs);
		}
		else if (tag == "END_OBJECT_CREATION")
			break;
		gsf.advanceLine();
	}
}

void ObjectCreation::run(ServerGameObject * creator, SrvScriptContext* ctx) const
{
	auto _0 = ctx->changeSelf(creator);
	auto _1 = ctx->change(ctx->creator, creator);
	const GameObjBlueprint* type = nullptr;
	if (typeToCreate)
		type = typeToCreate;
	else if (mappedTypeFrom) {
		auto* obj = mappedTypeFrom->getFirst(ctx);
		auto it = obj->blueprint->mappedTypeTags.find(mappedTypeToCreate);
		if (it != obj->blueprint->mappedTypeTags.end())
			type = it->second;
	}
	else
		ferr("OBJECT_CREATION has no object type defined!");
	if (!type)
		return;
	ServerGameObject* ctrl = controller->getFirst(ctx);
	if (!ctrl) {
		printf("WARNING: OBJECT_CREATION failed as no CONTROLLER found.\n");
		return;
	}
	OrientedPosition opos = createAt ? createAt->eval(ctx) : OrientedPosition({ creator->position, creator->orientation });
	ServerGameObject* created = Server::instance->spawnObject(type, ctrl, opos.position, opos.rotation);
	if (subordinates) {
		for (ServerGameObject* sub : subordinates->eval(ctx))
			sub->setParent(created);
	}
	if (type->bpClass == Tags::GAMEOBJCLASS_FORMATION) {
		created->sendEvent(Tags::PDEVENT_ON_SPAWN);
	}
	auto _2 = ctx->changeSelf(created);
	postCreationSequence.run(ctx);
}
