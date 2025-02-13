// wkbre2 - WK Engine Reimplementation
// (C) 2021-2022 AdrienTD
// Licensed under the GNU General Public License 3

#include "common.h"
#include "gameset/GameObjBlueprint.h"
#include "gameset/gameset.h"
#include "terrain.h"

float CommonGameObject::getItem(int item) const
{
	auto it = items.find(item);
	if (it != items.end())
		return it->second;
	auto bt = blueprint->startItemValues.find(item);
	if (bt != blueprint->startItemValues.end())
		return bt->second;
	return 0.0f;
}

float CommonGameObject::getIndexedItem(int item, int index) const {
	auto it = indexedItems.find(std::make_pair(item, index));
	if (it != indexedItems.end())
		return it->second;
	return 0.0f;
}

CommonGameObject* CommonGameObject::getPlayer() const {
	for (const CommonGameObject* obj = this; obj; obj = obj->parent) {
		if (obj->blueprint->bpClass == Tags::GAMEOBJCLASS_PLAYER)
			return (CommonGameObject*)obj;
	}
	return nullptr;
}

Model* CommonGameObject::getModel() const {
	return blueprint->getModel(subtype, appearance, animationIndex, animationVariant);
}

int CommonGameState::getDiplomaticStatus(CommonGameObject* a, CommonGameObject* b) const {
	if (a == b) return 0; // player is always friendly with itself :)
	auto it = (a->id <= b->id) ? diplomaticStatuses.find({ a->id, b->id }) : diplomaticStatuses.find({ b->id, a->id });
	if (it != diplomaticStatuses.end())
		return it->second;
	else
		return gameSet->defaultDiplomaticStatus;
}

void CommonGameState::updateOccupiedTiles(CommonGameObject* object, const Vector3& oldposition, const Vector3& oldorientation, const Vector3& newposition, const Vector3& neworientation)
{
	if (!this->tiles) return;
	int trnNumX, trnNumZ;
	std::tie(trnNumX, trnNumZ) = this->terrain->getNumPlayableTiles();
	if (object->blueprint->bpClass == Tags::GAMEOBJCLASS_BUILDING && object->blueprint->footprint) {
		// free tiles from old position
		auto rotOrigin = object->blueprint->footprint->rotateOrigin(oldorientation.y);
		int ox = (int)((oldposition.x - rotOrigin.first) / 5.0f);
		int oz = (int)((oldposition.z - rotOrigin.second) / 5.0f);
		for (auto& to : object->blueprint->footprint->tiles) {
			if (true /*!to.mode*/) {
				auto ro = to.rotate(oldorientation.y);
				int px = ox + ro.first, pz = oz + ro.second;
				if (px >= 0 && px < trnNumX && pz >= 0 && pz < trnNumZ) {
					auto& tile = this->tiles[pz * trnNumX + px];
					if (tile.building == object->id)
						tile.building = nullptr;
				}
			}
		}
		// take tiles from new position
		rotOrigin = object->blueprint->footprint->rotateOrigin(neworientation.y);
		ox = (int)((newposition.x - rotOrigin.first) / 5.0f);
		oz = (int)((newposition.z - rotOrigin.second) / 5.0f);
		for (auto& to : object->blueprint->footprint->tiles) {
			if (true /*!to.mode*/) {
				auto ro = to.rotate(neworientation.y);
				int px = ox + ro.first, pz = oz + ro.second;
				if (px >= 0 && px < trnNumX && pz >= 0 && pz < trnNumZ) {
					auto& tile = this->tiles[pz * trnNumX + px];
					if (!tile.building.getFrom(this) || tile.buildingPassable) {
						tile.building = object->id;
						tile.buildingPassable = to.mode;
					}
				}
			}
		}
	}
}
