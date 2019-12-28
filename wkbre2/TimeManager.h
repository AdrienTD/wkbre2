#pragma once

#include <cstdint>
//#include "util/GSFileParser.h"

typedef float game_time_t;
struct GSFileParser;

struct TimeManager {
	// float is baaad!!
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

	void tick();

	void imgui();
};
