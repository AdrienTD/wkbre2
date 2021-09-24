#include "ParticleContainer.h"
#include "ParticleSystem.h"

Vector3 Particle::getPosition(float time) {
	float dt = time - startTime;
	Vector3 pos = startPosition;
	pos += startVelocity * dt;
	for (auto& force : system->Particles.Forces) {
		if (force.direction == PSForceType::Gravity)
			pos.y += 0.5f * force.intensity * dt * dt;
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
		particles.emplace_back();
		Particle& particle = particles.back();
		particle.system = system;
		particle.startTime = nextTime;
		particle.startPosition = position;

		float ang = system->State_Generation.Velocity.Direction.Cone * 3.141f / 180.0f;
		//particle.startVelocity = Vector3(0,1,0).transform(Matrix::getRotationYMatrix(ang)).transform(Matrix::;
		particle.startVelocity = Vector3(0, 1, 0);
		particle.startVelocity *= system->State_Generation.Velocity.Magnitude_Range[0];
		particle.objid = objid;
	}
}

void ParticleContainer::update(float nextTime)
{
	for (size_t i = 0; i < particles.size(); ++i) {
		if (nextTime - particles[i].startTime >= particles[i].system->Particles.Max_Age_Range[0]) {
			particles[i] = std::move(particles.back());
			particles.pop_back();
			--i;
		}
	}
}
