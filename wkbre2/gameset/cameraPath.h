#pragma once

#include <memory>
#include <vector>
#include "actions.h"
#include "position.h"
#include "values.h"

struct GameSet;
struct GSFileParser;

struct CameraPath {
	bool startAtCurrentCameraPosition = false;
	bool loopCameraPath = false;
	struct PathNode {
		std::unique_ptr<PositionDeterminer> position;
		std::unique_ptr<ValueDeterminer> duration;
	};
	std::vector<PathNode> pathNodes;
	ActionSequence postPlaySequence;

	void parse(GSFileParser& gsf, GameSet& gs);
};