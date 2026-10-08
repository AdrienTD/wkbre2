// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include "SceneRenderer.h"

struct VulkanEnhancedSceneRenderer : SceneRenderer
{
	VulkanEnhancedSceneRenderer(IRenderer* gfx, Scene* scene, Camera* camera) : SceneRenderer(gfx, scene, camera) { init(); }
	~VulkanEnhancedSceneRenderer();
	void init();
	void render() override;
private:
	struct Impl;
	Impl* _impl;
};