// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

struct IRenderer;
struct Scene;

struct SceneRenderer {
	IRenderer *gfx;
	Scene *scene;
	SceneRenderer(IRenderer *gfx, Scene *scene) : gfx(gfx), scene(scene) {}
	virtual void render() = 0;
};
