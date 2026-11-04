// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include "TerrainRenderer.h"
#include "renderer.h"
#include <vector>
#include <memory>

struct RBatch;
struct TextureCache;
struct TerrainTexture;
struct TerrainTile;

struct VulkanEnhancedTerrainRenderer : TerrainRenderer {
public:
	VulkanEnhancedTerrainRenderer(IRenderer *gfx, Terrain *terrain, Camera *camera) : TerrainRenderer(gfx, terrain, camera) { init(); }
	~VulkanEnhancedTerrainRenderer();
	void render() override;
	void init();

	bool m_bumpOn = true;
private:
	struct TexAtlas {
		texture diffuseMap;
		texture normalMap;
		std::vector<const TerrainTile*> tiles;
	};
	struct Impl;
	Impl* _impl;
	TextureCache *texcache;
	std::vector<TexAtlas> texAtlases;
};