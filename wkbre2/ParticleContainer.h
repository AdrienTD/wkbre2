#pragma once

#include <map>
#include <vector>
#include "util/vecmat.h"

struct ParticleSystem;

struct Particle
{
	ParticleSystem* system;
	float startTime;
	Vector3 startPosition;
	Vector3 startVelocity;
	uint32_t objid;

	Vector3 getPosition(float time);
	uint32_t getColor(float time);
};

struct ParticleContainer {
	struct Trail {
		struct Part {
			Vector3 position;
			float startTime;
		};
		std::vector<Part> parts;
	};

	std::vector<Particle> particles;
	std::map<uint32_t, Trail> trails;

	void clearParticles() { particles.clear(); }

	void generate(ParticleSystem* system, const Vector3& position, uint32_t objid, float prevTime, float nextTime);
	void generateTrail(const Vector3& position, uint32_t objid, float prevTime, float nextTime);

	void update(float nextTime);
};