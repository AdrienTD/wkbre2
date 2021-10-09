#include "Trajectory.h"

Vector3 Trajectory::getPosition(float time)
{
	float t = time - m_startTime;
	Vector3 pos = m_initialPosition + m_initialVelocity * t;
	pos.y += 0.5f * gravity * t * t;
	return pos;
}

Vector3 Trajectory::getDirection(float time)
{
	float t = time - m_startTime;
	float y_over_t = m_initialVelocity.y + gravity * t;
	return { m_initialVelocity.x, y_over_t, m_initialVelocity.z };
}

Vector3 Trajectory::getRotationAngles(float time)
{
	Vector3 dir = getDirection(time);
	float ry = std::atan2(dir.x, -dir.z);
	float rx = std::atan2(dir.y, -dir.len2xz());
	return Vector3(rx, ry, 0.0f);
}

void Trajectory::start(const Vector3& initPos, const Vector3& initVel, float startTime)
{
	m_initialPosition = initPos;
	m_initialVelocity = initVel;
	m_startTime = startTime;
	m_started = true;
}
