// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "Sound.h"
#include "gameset.h"
#include "util/GSFileParser.h"

void GSSound::parse(GSFileParser& gsf, GameSet& gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		auto tag = gsf.nextTag();
		if (tag == "SOUND_FILE")
			files.push_back(gsf.nextString(true));
		else if (tag == "START_DIST")
			startDist = gsf.nextFloat();
		else if (tag == "MUTE_DIST")
			muteDist = gsf.nextFloat();
		else if (tag == "GRADIENT_START_DIST")
			gradientStartDist = gsf.nextFloat();
		else if (tag == "VELOCITY")
			velocity = gsf.nextFloat();
		else if (tag == "ROLLOFF")
			rolloff = gsf.nextFloat();
		else if (tag == "END_SOUND")
			break;
		gsf.advanceLine();
	}
}
