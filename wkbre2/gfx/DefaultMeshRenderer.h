#pragma once

#include "MeshRenderer.h"

struct IRenderer;

struct DefaultMeshRenderer : MeshRenderer {
	IRenderer *gfx;
	void render(Mesh *mesh) override;
	DefaultMeshRenderer(IRenderer *gfx) : gfx(gfx) {}
};