// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include "util/vecmat.h"

struct Trajectory {
public:
	const float gravity = -9.81f;

	Vector3 getPosition(float time);
	Vector3 getDirection(float time);
	Vector3 getRotationAngles(float time);
	void start(const Vector3& initPos, const Vector3& initVel, float startTime);
	bool isMoving() { return m_started; }

private:
	bool m_started = false;
	Vector3 m_initialPosition;
	Vector3 m_initialVelocity;
	float m_startTime;
};