// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "Package.h"
#include "../util/GSFileParser.h"
#include "gameset.h"
#include "finder.h"
#include "../server.h"
#include "ScriptContext.h"

void GSPackage::parse(GSFileParser& gsf, const GameSet& gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		auto tag = gsf.nextTag();
		if (tag == "ITEM_MODIFICATION") {
			int mode = 0;
			auto smode = gsf.nextString();
			if (smode == "REPLACE") mode = 0;
			else if (smode == "INCREASE") mode = 1;
			else if (smode == "REDUCE") mode = 2;
			int item = gs.items.readIndex(gsf);
			ValueDeterminer* value = ReadValueDeterminer(gsf, gs);
			itemModifications.push_back({ mode, item, value });
		}
		else if (tag == "GAME_EVENT") {
			gameEvents.push_back(gs.events.readIndex(gsf));
		}
		else if (tag == "RELATED_PARTY") {
			relatedParty = ReadFinder(gsf, gs);
		}
		else if (tag == "END_PACKAGE")
			break;
		gsf.advanceLine();
	}
}

void GSPackage::send(ServerGameObject* obj, SrvScriptContext* ctx) const
{
	for (auto& mod : itemModifications) {
		float val = mod.value->eval(ctx);
		if (mod.mode == 0) {
			obj->setItem(mod.item, val);
		}
		else if (mod.mode == 1) {
			obj->setItem(mod.item, obj->getItem(mod.item) + val);
		}
		else if (mod.mode == 2) {
			obj->setItem(mod.item, obj->getItem(mod.item) - val);
		}
	}
	for (int evt : gameEvents)
		obj->sendEvent(evt, ctx->getSelf());
}
