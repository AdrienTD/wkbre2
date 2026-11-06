// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include "util/vecmat.h"
#include <array>

struct Camera {
	Vector3 position, orientation;
	float aspect = 4.0f/3.0f, nearDist = 0.1f, farDist = 250.0f;
	bool orthoMode = false;
	Matrix sceneMatrix, viewMatrix, projMatrix;
	Vector3 direction;

	struct Plane {
		Vector3 normal;
		float cst;
	};
	std::array<Plane, 6> frustumPlanes;

	enum class Visibility {
		Invisible,
		PartiallyVisible,
		CompletelyVisible
	};

	Camera() = default;
	Camera(const Vector3 &position, const Vector3 &orientation) : position(position), orientation(orientation) {}

	void updateMatrix();

	//bool isPointVisible(const Vector3& point) const;
	Visibility isAABBVisible(const Vector3& loPoint, const Vector3& hiPoint) const;
};