// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include "TerrainRenderer.h"
#include "renderer.h"
#include <map>
#include <vector>
#include <memory>

struct RBatch;
struct TextureCache;
struct TerrainTexture;
struct TerrainTile;

struct D3D11EnhancedTerrainRenderer : TerrainRenderer {
public:
	D3D11EnhancedTerrainRenderer(IRenderer *gfx, Terrain *terrain, Camera *camera) : TerrainRenderer(gfx, terrain, camera) { init(); }
	~D3D11EnhancedTerrainRenderer();
	void render() override;
	void init();

	Vector3 m_lampPos;
	bool m_bumpOn = true;

	texture m_shadowMap = nullptr;
	Matrix m_sunViewProjMatrix;
private:
	struct LoadedTexture {
		texture diffuseMap;
		texture normalMap;
		bool operator<(const LoadedTexture& lt) const { return (diffuseMap == lt.diffuseMap) ? (normalMap < lt.normalMap) : (diffuseMap < lt.diffuseMap); }
	};
	struct Impl;
	Impl* _impl;
	TextureCache *texcache;
	std::map<TerrainTexture*, LoadedTexture> ttexmap;
	std::map<LoadedTexture, std::vector<TerrainTile*>> tilesPerTex;
};