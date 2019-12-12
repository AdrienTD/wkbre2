#include "command.h"
#include "gameset.h"
#include "../util/util.h"

void Command::parse(GSFileParser &gsf, const GameSet &gs) {
	gsf.advanceLine();
	while (!gsf.eof) {
		std::string strtag = gsf.nextTag();

		if (strtag == "START_SEQUENCE")
			startSequence.init(gsf, gs, "END_START_SEQUENCE");
		else if (strtag == "END_COMMAND")
			return;
		gsf.advanceLine();
	}
	ferr("Command reached end of file without END_COMMAND!");
}

void Command::execute(ServerGameObject *self, ServerGameObject *target, int assignmentMode) {
	this->startSequence.run(self);
}