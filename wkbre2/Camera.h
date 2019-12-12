#pragma once

#include "util/vecmat.h"

struct Camera {
	Vector3 position, orientation;
	Matrix sceneMatrix;
	Vector3 direction;
	void updateMatrix();
};