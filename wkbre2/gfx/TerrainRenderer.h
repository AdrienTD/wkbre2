// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

struct IRenderer;
struct Terrain;
struct Camera;

struct TerrainRenderer {
	IRenderer *gfx;
	Terrain *terrain;
	Camera *camera;
	TerrainRenderer(IRenderer *gfx, Terrain *terrain, Camera *camera) : gfx(gfx), terrain(terrain), camera(camera) {}
	virtual void render() = 0;
};