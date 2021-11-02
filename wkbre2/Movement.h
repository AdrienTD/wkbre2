// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include "util/vecmat.h"

struct BinaryReader;
struct NetPacketWriter;

struct Movement {
public:
	void startMovement(const Vector3 &source, const Vector3 &destination, float startTime, float speed);
	void stopMovement();
	Vector3 getNewPosition(float time) const;
	bool isMoving() const { return m_started; }
	Vector3 getDirection() const { return (m_destPoint - m_srcPoint).normal2xz(); }
	Vector3 getDestination() const { return m_destPoint; }
private:
	static constexpr float MIN_DISTANCE = 0.01f; // minimal distance to consider movement
	bool m_started = false;
	Vector3 m_srcPoint, m_destPoint;
	float m_startTime, m_speed;
	float m_distance;
};