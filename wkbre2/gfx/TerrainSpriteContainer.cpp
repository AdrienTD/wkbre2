#include "TerrainSpriteContainer.h"

void TerrainSpriteContainer::clear()
{
	for (auto& [tex, vec] : m_spriteListMap)
		vec.clear();
}

void TerrainSpriteContainer::addSprite(void* tex, Point p1, Point p2, Point p3)
{
	auto& vec = m_spriteListMap[tex];
	Sprite& sprite = vec.emplace_back();
	sprite.p1 = p1;
	sprite.p2 = p2;
	sprite.p3 = p3;
}
