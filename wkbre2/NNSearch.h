// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include "util/vecmat.h"

struct CommonGameState;
struct CommonGameObject;

// Find all objects in the requested circle
struct NNSearch {
	CommonGameState* gameState;

	int minX, minZ, maxX, maxZ;

	int tx, tz;
	int it;

	// Starts the search.
	void start(CommonGameState* server, const Vector3& center, float radius);
	// Returns the next found object, or nullptr if no more objects found.
	CommonGameObject* next();
};