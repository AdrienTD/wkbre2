#include "reaction.h"
#include "../util/GSFileParser.h"
#include "gameset.h"
#include "actions.h"
#include "../server.h"
#include "ScriptContext.h"

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
				int prt = gs.prTriggers.readIndex(gsf);
				if (prt != -1)
					prTriggers.push_back(&gs.prTriggers[prt]);
			}
		}
		else if (tag == "END_REACTION")
			break;
		gsf.advanceLine();
	}
}

bool Reaction::canBeTriggeredBy(int evt, ServerGameObject *obj, ServerGameObject* sender)
{
	if (std::find(events.begin(), events.end(), evt) != events.end())
		return true;
	for (PackageReceiptTrigger *prt : prTriggers)
		if (prt->canBeTriggeredBy(evt, obj, sender))
			return true;
	return false;
}

void PackageReceiptTrigger::parse(GSFileParser & gsf, GameSet & gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		auto tag = gsf.nextTag();
		if (tag == "ASSESSMENT") {
			int ass = gs.equations.readIndex(gsf);
			if (ass != -1)
				assessments.push_back(ass);
			else
				printf("ASSESSMENT equation not found\n");
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

bool PackageReceiptTrigger::canBeTriggeredBy(int evt, ServerGameObject *obj, ServerGameObject* sender)
{
	if(std::find(events.begin(), events.end(), evt) == events.end())
		return false;
	SrvScriptContext ctx(Server::instance, obj);
	auto senderChange = ctx.packageSender.change(sender);
	for (int ass : assessments)
		if (Server::instance->gameSet->equations[ass]->eval(&ctx) <= 0.0f)
			return false;
	return true;
}
