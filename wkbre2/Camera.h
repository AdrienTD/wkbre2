#pragma once

#include "util/vecmat.h"

struct Camera {
	Vector3 position, orientation;
	Matrix sceneMatrix;
	Vector3 direction;
	float nearDist = 1.0f, farDist = 500.0f;
	void updateMatrix();
};