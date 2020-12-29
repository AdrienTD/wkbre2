#include "condition.h"
#include "gameset.h"
#include "../util/GSFileParser.h"

void GSCondition::parse(GSFileParser & gsf, GameSet & gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		auto tag = gsf.nextTag();
		if (tag == "TEST") {
			test = ReadValueDeterminer(gsf, gs);
		}
		else if (tag == "HINT") {
			hint = gsf.nextString();
		}
		else if (tag == "END_CONDITION")
			break;
		gsf.advanceLine();
	}
}
