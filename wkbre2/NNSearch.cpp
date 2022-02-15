// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "NNSearch.h"
#include "server.h"
#include "client.h"
#include "terrain.h"
#include <cassert>

template<typename Program, typename PrgGameObject>
void NNSearch<Program, PrgGameObject>::start(Program *server, const Vector3 &center, float radius)
{
	this->server = server;
	int tileradius = static_cast<int>(radius / 5.0f + 1.5f);
	int ctx = static_cast<int>(center.x / 5.0f), ctz = static_cast<int>(center.z / 5.0f);
	minX = ctx - tileradius; minZ = ctz - tileradius;
	maxX = ctx + tileradius; maxZ = ctz + tileradius;
	auto area = server->terrain->getNumPlayableTiles();
	auto clamp = [](int &x, int lim) {if (x < 0) x = 0; else if (x >= lim) x = lim - 1; };
	clamp(minX, area.first); clamp(maxX, area.first); clamp(minZ, area.second), clamp(maxZ, area.second);
	tx = minX; tz = minZ; it = 0;
}

template<typename Program, typename PrgGameObject>
PrgGameObject* NNSearch<Program, PrgGameObject>::next()
{
	while (true) {
		const auto& vec = server->tiles[tz * server->terrain->getNumPlayableTiles().first + tx].objList;
		if ((size_t)it < vec.size()) {
			PrgGameObject* obj = vec[it++].getFrom<Program>();
			if (obj)
				return obj;
			else
				continue;
		}

		tx += 1;
		if (tx > maxX) { tz += 1; tx = minX; }
		if (tz > maxZ) return nullptr;
		it = 0;
	}
}

template struct NNSearch<Server, ServerGameObject>;
template struct NNSearch<Client, ClientGameObject>;