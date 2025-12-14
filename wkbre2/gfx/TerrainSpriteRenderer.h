#pragma once

struct IRenderer;
struct Terrain;
struct TerrainSpriteContainer;
struct RBatch;

struct TerrainSpriteRenderer
{
public:
	TerrainSpriteRenderer(IRenderer* gfx, Terrain* terrain, TerrainSpriteContainer* spriteContainer)
		: gfx(gfx), terrain(terrain), spriteContainer(spriteContainer)
	{
		init();
	}
	void render();

protected:
	void init();

	IRenderer* gfx;
	Terrain* terrain;
	TerrainSpriteContainer* spriteContainer;

	RBatch* batch;
};