#include "ParticleContainer.h"
#include "ParticleSystem.h"

Vector3 Particle::getPosition(float time) {
	float dt = time - startTime;
	Vector3 pos = startPosition;
	pos += startVelocity * dt;
	for (auto& force : system->Particles.Forces) {
		if (force.direction == PSForceType::Gravity)
			pos.y += 0.5f * (force.intensity * (-9.81f)) * dt * dt;
	}
	return pos;
}

uint32_t Particle::getColor(float time)
{
	std::array<int, 4> colour;
	auto& colour_fn = system->Particles.Colour_Fn;
	if (colour_fn.isDynamic) {
		std::array<int, 4> a, b; float p;
		std::tie(a, b, p) = colour_fn.cdynamic.Gradient.getAtTime(time - startTime, { 255,255,255,255 });
		for (size_t i = 0; i < 4; i++)
			colour[i] = a[i] + (int)((float)(b[i] - a[i]) * p);
	}
	else {
		colour = colour_fn.cstatic.colour;
	}
	uint32_t packed = ((colour[0] & 255) << 24) | ((colour[1] & 255) << 16) | ((colour[2] & 255) << 8) | ((colour[3] & 255) << 0);
	return packed;
}

void ParticleContainer::generate(ParticleSystem* system, const Vector3& position, uint32_t objid, float prevTime, float nextTime) {
	float interval = system->Generation_Decision.Interval;
	if (std::floor(prevTime / interval) < std::floor(nextTime / interval)) {
		// create the particle
		auto& partVector = particles[system];
		partVector.emplace_back();
		Particle& particle = partVector.back();
		particle.system = system;
		particle.startTime = nextTime;
		particle.startPosition = position;

		float maxAngle = system->State_Generation.Velocity.Direction.Cone * 3.141f / 180.0f;
		float angX = (((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f) * maxAngle;
		float angY = (((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f) * maxAngle;
		particle.startVelocity = Vector3(0,1,0).transformNormal(Matrix::getRotationXMatrix(angX)).transformNormal(Matrix::getRotationYMatrix(angY));
		auto& magRange = system->State_Generation.Velocity.Magnitude_Range;
		particle.startVelocity *= ((float)rand() / (float)RAND_MAX) * (magRange[1] - magRange[0]) + magRange[0];
		particle.objid = objid;

		auto& maxAgeRange = system->Particles.Max_Age_Range;
		particle.maxAge = ((float)rand() / (float)RAND_MAX) * (maxAgeRange[1] - maxAgeRange[0]) + maxAgeRange[0];
	}
}

void ParticleContainer::generateTrail(const Vector3& position, uint32_t objid, float prevTime, float nextTime)
{
	trails[objid].parts.push_back({ position, nextTime });
}

void ParticleContainer::update(float nextTime)
{
	for (auto& kv : particles) {
		auto& partVec = kv.second;
		for (size_t i = 0; i < partVec.size(); ++i) {
			if (nextTime - partVec[i].startTime >= partVec[i].maxAge) {
				partVec[i] = std::move(partVec.back());
				partVec.pop_back();
				--i;
			}
		}
	}

	for (auto next = trails.begin(); next != trails.end();) {
		auto it = next++;
		auto& trail = it->second;
		if (nextTime - trail.parts.back().startTime >= 2.0f) {
			trails.erase(it);
		}
	}
}
