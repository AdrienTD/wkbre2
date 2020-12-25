#include "reaction.h"
#include "../util/GSFileParser.h"
#include "gameset.h"
#include "actions.h"
#include "../server.h"

void Reaction::parse(GSFileParser & gsf, GameSet & gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		auto tag = gsf.nextTag();
		if (tag == "ACTION") {
			actions.actionList.emplace_back(ReadAction(gsf, gs));
		}
		else if (tag == "TRIGGERED_BY") {
			const char *oldcur = gsf.cursor;
			int ex = gs.events.readIndex(gsf);
			if (ex != -1)
				events.push_back(ex);
			else {
				gsf.cursor = oldcur;
				prTriggers.push_back(gs.prTriggers.readPtr(gsf));
			}
		}
		else if (tag == "END_REACTION")
			break;
		gsf.advanceLine();
	}
}

bool Reaction::canBeTriggeredBy(int evt, ServerGameObject *obj)
{
	if (std::find(events.begin(), events.end(), evt) != events.end())
		return true;
	for (PackageReceiptTrigger *prt : prTriggers)
		if (prt->canBeTriggeredBy(evt, obj))
			return true;
	return false;
}

void PackageReceiptTrigger::parse(GSFileParser & gsf, GameSet & gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		auto tag = gsf.nextTag();
		if (tag == "ASSESSMENT") {
			assessments.push_back(gs.equations.readIndex(gsf));
		}
		else if (tag == "TRIGGERED_BY") {
			int evt = gs.events.readIndex(gsf);
			if (evt != -1)
				events.push_back(evt);
		}
		else if (tag == "END_PACKAGE_RECEIPT_TRIGGER")
			break;
		gsf.advanceLine();
	}
}

bool PackageReceiptTrigger::canBeTriggeredBy(int evt, ServerGameObject *obj)
{
	if(std::find(events.begin(), events.end(), evt) == events.end())
		return false;
	for (int ass : assessments)
		if (Server::instance->gameSet->equations[ass]->eval(obj) <= 0.0f)
			return false;
	return true;
}
