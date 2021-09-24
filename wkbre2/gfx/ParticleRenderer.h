#pragma once

struct IRenderer;
struct ParticleContainer;
struct Camera;

struct ParticleRenderer {
	IRenderer* gfx;
	ParticleContainer* particleContainer;
	ParticleRenderer(IRenderer* gfx, ParticleContainer* particleContainer) : gfx(gfx), particleContainer(particleContainer) {}
	virtual void render(float prevTime, float nextTime, const Camera& camera) = 0;
};
