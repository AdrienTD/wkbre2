// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "Camera.h"
#include "window.h"
#include <algorithm>

void Camera::autoManage(float terrainHeight)
{
	static const float minHeightToTerrain = 3.0f;
	static const float maxHeightToTerrain = 150.0f;

	if (freeLook)
		return;

	position.y = std::clamp(position.y, terrainHeight + minHeightToTerrain, terrainHeight + maxHeightToTerrain);

	const float cameraHeightToTerrain = position.y - terrainHeight;
	const float r = std::clamp(cameraHeightToTerrain / maxHeightToTerrain, 0.0f, 1.0f);

	orientation.x = -r * 1.56f;
}

void Camera::updateMatrix() {
	aspect = (float)g_windowWidth / g_windowHeight;
	this->direction = Vector3(
		-sin(orientation.y)*cos(orientation.x),
		sin(orientation.x),
		cos(orientation.y)*cos(orientation.x)
	);
	if (orthoMode)
		projMatrix = Matrix::getLHOrthoMatrix(position.y*aspect, position.y, nearDist, farDist);
	else
		projMatrix = Matrix::getLHPerspectiveMatrix(0.9f, aspect, nearDist, farDist);
	viewMatrix = Matrix::getLHLookAtViewMatrix(position, position + direction, Vector3(0, 1, 0));
	this->sceneMatrix = viewMatrix * projMatrix;

	const Matrix& sm = this->sceneMatrix;
	frustumPlanes = {
		Plane{Vector3(sm._14 + sm._11, sm._24 + sm._21, sm._34 + sm._31), sm._44 + sm._41},
		Plane{Vector3(sm._14 - sm._11, sm._24 - sm._21, sm._34 - sm._31), sm._44 - sm._41},
		Plane{Vector3(sm._14 + sm._12, sm._24 + sm._22, sm._34 + sm._32), sm._44 + sm._42},
		Plane{Vector3(sm._14 - sm._12, sm._24 - sm._22, sm._34 - sm._32), sm._44 - sm._42},
		Plane{Vector3(sm._13, sm._23, sm._33), sm._43},
		Plane{Vector3(sm._14 - sm._13, sm._24 - sm._23, sm._34 - sm._33), sm._44 - sm._43},
	};
}

Camera::Visibility Camera::isAABBVisible(const Vector3& loPoint, const Vector3& hiPoint) const
{
	const Vector3 aabbPoints[8] = {
		Vector3(loPoint.x, loPoint.y, loPoint.z),
		Vector3(hiPoint.x, loPoint.y, loPoint.z),
		Vector3(loPoint.x, loPoint.y, hiPoint.z),
		Vector3(hiPoint.x, loPoint.y, hiPoint.z),
		Vector3(loPoint.x, hiPoint.y, loPoint.z),
		Vector3(hiPoint.x, hiPoint.y, loPoint.z),
		Vector3(loPoint.x, hiPoint.y, hiPoint.z),
		Vector3(hiPoint.x, hiPoint.y, hiPoint.z)
	};

	bool allPointsInside = true;
	for (int p = 0; p < 6; ++p) {
		bool allBehindPlane = true;
		for (int v = 0; v < 8; ++v) {
			const float dist = aabbPoints[v].dot(frustumPlanes[p].normal) + frustumPlanes[p].cst;
			allBehindPlane = allBehindPlane && (dist < 0.0f);
			allPointsInside = allPointsInside && (dist >= 0.0f);
		}
		if (allBehindPlane) {
			// box completely outside the frustum -> skip quad node entirely
			return Visibility::Invisible;
		}
	}

	if (allPointsInside) {
		return Visibility::CompletelyVisible;
	}

	// NOTE: At this point, we actually do not know whether the box is invisible or partially visible.
	// We return PartiallyVisible anyway. False positives from invisible boxes may happen but rarely.
	return Visibility::PartiallyVisible;
}