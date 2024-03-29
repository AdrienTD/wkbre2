// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include <cstdint>
//#include "util/GSFileParser.h"

typedef float game_time_t;
struct GSFileParser;

struct TimeManager {
	// float is baaad!!
	uint32_t psCurrentTime = 0;
	game_time_t currentTime = 0.0f, previousTime = 0.0f, elapsedTime = 0.0f, timeSpeed = 1.0f;
	bool paused = true; uint32_t lockCount = 1;
	
	uint32_t previousSDLTime;

	// synch to clients ...

	void reset(game_time_t initialTime);
	void load(GSFileParser &gsf);

	void lock() { lockCount++; }
	void unlock() { lockCount--; }

	void pause() { paused = true; }
	void unpause() { paused = false; }

	void setSpeed(float nextSpeed) { timeSpeed = nextSpeed; }

	void tick();

	void imgui();
};
