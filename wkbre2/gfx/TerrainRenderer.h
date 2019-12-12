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