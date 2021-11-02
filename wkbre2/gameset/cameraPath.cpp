// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "cameraPath.h"
#include "gameset.h"
#include "../util/GSFileParser.h"

void CameraPath::parse(GSFileParser& gsf, GameSet& gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		auto tag = gsf.nextTag();
		if (tag == "INTERPOLATE_TO") {
			PathNode pn;
			pn.position.reset(PositionDeterminer::createFrom(gsf, gs));
			pn.duration.reset(ReadValueDeterminer(gsf, gs));
			pathNodes.push_back(std::move(pn));
		}
		else if (tag == "START_AT_CURRENT_CAMERA_POSITION") {
			startAtCurrentCameraPosition = true;
		}
		else if (tag == "LOOP_CAMERA_PATH") {
			loopCameraPath = true;
		}
		else if (tag == "POST_PLAY_SEQUENCE") {
			postPlaySequence.init(gsf, gs, "END_POST_PLAY_SEQUENCE");
		}
		else if (tag == "END_CAMERA_PATH")
			break;
		gsf.advanceLine();
	}
}
