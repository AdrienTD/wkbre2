#pragma once

struct IRenderer;
struct Scene;

struct SceneRenderer {
	IRenderer *gfx;
	Scene *scene;
	SceneRenderer(IRenderer *gfx, Scene *scene) : gfx(gfx), scene(scene) {}
	virtual void render() = 0;
};
