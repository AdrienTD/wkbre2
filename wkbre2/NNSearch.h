#pragma once

#include "util/vecmat.h"

struct Server;
struct ServerGameObject;

// Find all objects in the requested circle
struct NNSearch {
	Server *server;

	int minX, minZ, maxX, maxZ;

	int tx, tz;
	int it;

	// Starts the search.
	void start(Server *server, const Vector3 &center, float radius);
	// Returns the next found object, or nullptr if no more objects found.
	ServerGameObject *next();
};