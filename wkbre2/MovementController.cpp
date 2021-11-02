// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "MovementController.h"
#include "server.h"
#include "Pathfinding.h"
#include "terrain.h"
#include <cassert>

Vector3 MovementController::startMovement(const Vector3& destination)
{
	//m_object->startMovement(destination);
	//m_pathNodes = { destination };

	using namespace Pathfinding;
	Terrain* trn = Server::instance->terrain;
	auto& tiles = Server::instance->tiles;
	auto trnsize = trn->getNumPlayableTiles();
	auto pred = [&](PFPos pfp) -> bool {
		if (pfp.x >= 0 && pfp.x < trnsize.first && pfp.z >= 0 && pfp.z < trnsize.second) {
			const auto& tile = tiles[pfp.z * trnsize.first + pfp.x];
			if (tile.building && !tile.buildingPassable)
				return true;
			else
				return false;
		}
		return true;
	};

	PFPos posStart{ (int)(m_object->position.x / 5.0f), (int)(m_object->position.z / 5.0f) };
	PFPos posEnd{ (int)(destination.x / 5.0f), (int)(destination.z / 5.0f) };

	// if start is blocked...
	if (pred(posStart)) {
		stopMovement();
		return destination;
	}

	// if destination if blocked, find non-blocked tile nearest to destination
	Vector3 realDestination = destination;
	if (pred(posEnd)) {
		Vector3 dir = (m_object->position - realDestination).normal2xz();
		Vector3 test = realDestination;
		while (true) {
			test += dir;
			PFPos near = { (int)(test.x / 5.0f), (int)(test.z / 5.0f) };
			if (!pred(near)) {
				posEnd = near;
				realDestination = test;
				break;
			}
			assert(near != posStart);
		}
	}

	auto tileList = DoPathfinding(posStart, posEnd, pred, ManhattanDiagHeuristic);

	if (tileList.size() >= 1) {
		m_pathNodes.clear();
		m_pathNodes.emplace_back(realDestination);
		for (PFPos& pfp : tileList) {
			m_pathNodes.emplace_back(pfp.x * 5.0f + 2.5f, m_object->position.y, pfp.z * 5.0f + 2.5f);
		}
		m_object->startMovement(m_pathNodes[m_pathNodes.size() - 2]);
		m_started = true;
	}
	else
		stopMovement();
	return realDestination;
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
	if ((m_object->position - m_pathNodes[m_pathNodes.size() - 2]).sqlen2xz() < 0.01f) {
		m_pathNodes.pop_back();
		if (m_pathNodes.size() >= 2) {
			m_object->startMovement(m_pathNodes[m_pathNodes.size() - 2]);
		}
		else
			stopMovement();
	}
}
