// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include "util/vecmat.h"

struct Server;
struct ServerGameObject;

// Find all objects in the requested circle
template<typename Program, typename PrgGameObject>
struct NNSearch {
	Program* server;

	int minX, minZ, maxX, maxZ;

	int tx, tz;
	int it;

	// Starts the search.
	void start(Program* server, const Vector3& center, float radius);
	// Returns the next found object, or nullptr if no more objects found.
	PrgGameObject* next();
};