#include "command.h"
#include "gameset.h"
#include "../util/util.h"
#include "../server.h"
#include "../window.h"
#include "../gameset/ScriptContext.h"

void Command::parse(GSFileParser &gsf, GameSet &gs) {
	gsf.advanceLine();
	while (!gsf.eof) {
		std::string strtag = gsf.nextTag();

		if (strtag == "START_SEQUENCE")
			startSequence.init(gsf, gs, "END_START_SEQUENCE");
		else if (strtag == "USE_ORDER")
			order = gs.orders.readPtr(gsf);
		else if (strtag == "CURSOR") {
			auto path = gsf.nextString(true);
			if (path == "CURSOR") // needs to bypass one oddity
				path = gsf.nextString(true);
			cursor = WndCreateCursor(path.c_str());
		}
		else if (strtag == "CURSOR_CONDITION")
			cursorConditions.push_back(gs.equations.readIndex(gsf));
		else if (strtag == "CURSOR_AVAILABLE") {
			GSCondition *cond = gs.conditions.readPtr(gsf);
			Cursor *cursor = WndCreateCursor(gsf.nextString(true).c_str());
			cursorAvailable.push_back({ cond, cursor });
		}
		else if (strtag == "END_COMMAND")
			return;
		gsf.advanceLine();
	}
	ferr("Command reached end of file without END_COMMAND!");
}

void Command::execute(ServerGameObject *self, ServerGameObject *target, int assignmentMode, const Vector3 &destination) {
	SrvScriptContext ctx(Server::instance, self);
	this->startSequence.run(&ctx);
	// TODO: order (what to do first?)
	if (this->order) {
		self->orderConfig.addOrder(this->order, assignmentMode, target, destination);
	}
}