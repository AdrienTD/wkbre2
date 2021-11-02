// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include "actions.h"

struct GSFileParser;
struct GameSet;

struct GS3DClip {
	ActionSequence postClipSequence;
	void parse(GSFileParser& gsf, GameSet& gs);
};