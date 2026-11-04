// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include "TerrainRenderer.h"
#include "renderer.h"
#include <vector>

struct RBatch;
struct TextureCache;
struct TerrainTexture;
struct TerrainTile;

struct DefaultTerrainRenderer : TerrainRenderer {
public:
	DefaultTerrainRenderer(IRenderer *gfx, Terrain *terrain, Camera *camera) : TerrainRenderer(gfx, terrain, camera) { init(); }
	~DefaultTerrainRenderer();
	void render() override;
	void init();
private:
	struct TexAtlas
	{
		texture tex;
		std::vector<const TerrainTile*> tiles;
	};

	RBatch *batch;
	TextureCache *texcache;
	std::vector<TexAtlas> texAtlases;
};