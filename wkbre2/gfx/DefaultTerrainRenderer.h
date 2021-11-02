// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include "TerrainRenderer.h"
#include "renderer.h"
#include <map>
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
	RBatch *batch;
	TextureCache *texcache;
	std::map<TerrainTexture*, texture> ttexmap;
	std::map<texture, std::vector<TerrainTile*>> tilesPerTex;
};