#include "Trajectory.h"

Vector3 Trajectory::getPosition(float time)
{
	float t = time - m_startTime;
	Vector3 pos = m_initialPosition + m_initialVelocity * t;
	pos.y += 0.5f * gravity * t * t;
	return pos;
}

void Trajectory::start(const Vector3& initPos, const Vector3& initVel, float startTime)
{
	m_initialPosition = initPos;
	m_initialVelocity = initVel;
	m_startTime = startTime;
	m_started = true;
}
