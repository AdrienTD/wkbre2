#pragma once

#include <vector>
#include "util/vecmat.h"
#include "Movement.h"

struct ServerGameObject;

struct MovementController {
	Vector3 startMovement(const Vector3& destination);
	void stopMovement();
	void updateMovement();
	//Vector3 getPosition(float time) const;
	bool isMoving() const { return m_started; }
	//Vector3 getDirection() const { return (m_pathNodes[m_pathNodes.size()-1] - m_pathNodes.back()).normal2xz(); }
	Vector3 getDestination() const { return m_pathNodes.front(); }

	MovementController(ServerGameObject* object) : m_object(object) {}

	bool m_started = false;
	std::vector<Vector3> m_pathNodes;
	ServerGameObject* m_object;
};