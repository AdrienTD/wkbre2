#include "MovementController.h"
#include "server.h"

void MovementController::startMovement(const Vector3& destination)
{
	m_object->startMovement(destination);
	m_pathNodes = { destination };
	m_started = true;
}

void MovementController::stopMovement()
{
	m_object->stopMovement();
	m_pathNodes.clear();
	m_started = false;
}

void MovementController::updateMovement()
{
	// check if Movement is done, if yes, go to next path node...
	if (!m_started) return;
}
