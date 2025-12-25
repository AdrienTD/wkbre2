// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "3DClip.h"
#include "gameset.h"
#include "util/GSFileParser.h"

void GS3DClip::parse(GSFileParser& gsf, GameSet& gs)
{
	gsf.advanceLine();
	while (!gsf.eof) {
		auto tag = gsf.nextTag();
		if (tag == "POST_CLIP_SEQUENCE") {
			postClipSequence.init(gsf, gs, "END_POST_CLIP_SEQUENCE");
		}
		else if (tag == "END_3D_CLIP")
			break;
		gsf.advanceLine();
	}
}
