// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "Movement.h"
#include <cassert>

void Movement::startMovement(const Vector3 & source, const Vector3 & destination, float startTime, float speed)
{
	m_srcPoint = source;
	m_destPoint = destination;
	m_startTime = startTime;
	m_speed = speed;
	m_distance = (m_destPoint - m_srcPoint).len2xz();

	m_started = m_distance > MIN_DISTANCE;
}

void Movement::stopMovement()
{
	m_started = false;
}

Vector3 Movement::getNewPosition(float time) const
{
	assert(isMoving());
	float d = (time - m_startTime) * m_speed / m_distance;
	if (d < 0.0f) d = 0.0f;
	if (d > 1.0f) d = 1.0f;
	return m_srcPoint + (m_destPoint - m_srcPoint) * d;
}
