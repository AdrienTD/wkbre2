// wkbre2 - WK Engine Reimplementation
// (C) 2021-2022 AdrienTD
// Licensed under the GNU General Public License 3

#include "common.h"
#include "gameset/GameObjBlueprint.h"

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
