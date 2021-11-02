// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include "MeshRenderer.h"

struct IRenderer;

struct DefaultMeshRenderer : MeshRenderer {
	IRenderer *gfx;
	void render(Mesh *mesh) override;
	DefaultMeshRenderer(IRenderer *gfx) : gfx(gfx) {}
};