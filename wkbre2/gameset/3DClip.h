#pragma once

#include "actions.h"

struct GSFileParser;
struct GameSet;

struct GS3DClip {
	ActionSequence postClipSequence;
	void parse(GSFileParser& gsf, GameSet& gs);
};