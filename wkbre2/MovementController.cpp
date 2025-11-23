// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "MovementController.h"
#include "server.h"
#include "Pathfinding.h"
#include "terrain.h"
#include <cassert>

namespace {
	template<typename Predicate> Pathfinding::PFPos findNearestUnblockedPos(Pathfinding::PFPos pfpos, Predicate pred, int maxr) {
		int ox = pfpos.x, oz = pfpos.z;
		if (!pred(pfpos))
			return { ox, oz };
		for (int r = 1; r < maxr; r++) {
			int x = ox + r;
			int z = oz;
			for (int i = 0; i < r; i++) {
				if (!pred({ x,z }))
					return { x, z };
				x--; z++;
			}
			for (int i = 0; i < r; i++) {
				if (!pred({ x,z }))
					return { x, z };
				x--; z--;
			}
			for (int i = 0; i < r; i++) {
				if (!pred({ x,z }))
					return { x, z };
				x++; z--;
			}
			for (int i = 0; i < r; i++) {
				if (!pred({ x,z }))
					return { x, z };
				x++; z++;
			}
		}
		return { -1, -1 };
	}
}

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
			if (tile.building.getFrom<Server>() && !tile.buildingPassable)
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
		// find "nearest" blocked tile, if unit is currently inside blocked tile
		auto area = trn->getNumPlayableTiles();
		int maxr = std::max(area.first, area.second);
		posStart = findNearestUnblockedPos(posStart, pred, maxr); // WARNING: Might cause infinite loop if there is no unblocked tile!
		if (pred(posStart)) {
			stopMovement();
			return destination;
		}
	}

	// if destination if blocked, find non-blocked tile nearest to destination
	Vector3 realDestination = destination;
	if (pred(posEnd)) {
		Vector3 dir = (m_object->position - realDestination).normal2xz();
		if (!(dir.x == 0.0f && dir.z == 0.0f)) {
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
	}

	auto rtres = SegmentTraversal(m_object->position.x / 5.0f, m_object->position.z / 5.0f, realDestination.x / 5.0f, realDestination.z / 5.0f, pred);
	if (!rtres) {
		m_pathNodes = { realDestination, m_object->position };
		m_object->startMovement(m_pathNodes[0]);
		m_started = true;
	}
	else {
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
	}
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
