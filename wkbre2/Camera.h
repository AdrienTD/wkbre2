// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include "util/vecmat.h"

struct Camera {
	Vector3 position, orientation;
	float aspect = 4.0f/3.0f, nearDist = 0.1f, farDist = 250.0f;
	bool orthoMode = false;
	Matrix sceneMatrix, viewMatrix, projMatrix;
	Vector3 direction;
	void updateMatrix();
	Camera() = default;
	Camera(const Vector3 &position, const Vector3 &orientation) : position(position), orientation(orientation) {}
};