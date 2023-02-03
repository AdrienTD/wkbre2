// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include "SceneRenderer.h"
#include "renderer.h"

struct D3D11EnhancedSceneRenderer : SceneRenderer
{
	D3D11EnhancedSceneRenderer(IRenderer* gfx, Scene* scene, Camera* camera) : SceneRenderer(gfx, scene, camera) { init(); }
	~D3D11EnhancedSceneRenderer();
	void init();
	void render() override;
	texture getShadowMapTexture();
	void enterShadowMapMode();
	void leaveShadowMapMode();
private:
	struct Impl;
	Impl* _impl;
};