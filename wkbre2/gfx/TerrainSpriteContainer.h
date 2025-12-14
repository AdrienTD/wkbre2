#pragma once

#include <unordered_map>
#include <vector>

struct TerrainSpriteContainer
{
public:
	struct Point {
		float x, y;
	};

	void clear();
	void addSprite(void* tex, Point p1, Point p2, Point p3);

private:
	struct Sprite {
		Point p1, p2, p3;
	};

	std::unordered_map<void*, std::vector<Sprite>> m_spriteListMap;

	friend struct TerrainSpriteRenderer;
};