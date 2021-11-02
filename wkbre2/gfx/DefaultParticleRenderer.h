// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include "ParticleRenderer.h"
#include "TextureCache.h"

struct DefaultParticleRenderer : ParticleRenderer {
	virtual void render(float prevTime, float nextTime, const Camera& camera) override;
	DefaultParticleRenderer(IRenderer* gfx, ParticleContainer* particleContainer) : ParticleRenderer(gfx, particleContainer), texCache(gfx) {}

	TextureCache texCache;
};