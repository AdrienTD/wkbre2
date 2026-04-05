// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include "ParticleRenderer.h"
#include "TextureCache.h"
#include <memory>

struct RBatch;

struct DefaultParticleRenderer : ParticleRenderer {
	DefaultParticleRenderer(IRenderer* gfx, ParticleContainer* particleContainer);
	~DefaultParticleRenderer();
	virtual void render(float prevTime, float nextTime, const Camera& camera) override;

	TextureCache texCache;
	std::unique_ptr<RBatch> batch;
};