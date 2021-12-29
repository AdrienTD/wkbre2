// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

struct IRenderer;
struct Scene;
struct Camera;

struct SceneRenderer {
	IRenderer* gfx;
	Scene* scene;
	Camera* camera;
	SceneRenderer(IRenderer* gfx, Scene* scene, Camera* camera) : gfx(gfx), scene(scene), camera(camera) {}
	virtual void render() = 0;
};
