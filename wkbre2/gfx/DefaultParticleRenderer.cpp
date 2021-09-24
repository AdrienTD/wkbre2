#include "DefaultParticleRenderer.h"
#include "../ParticleContainer.h"
#include "renderer.h"
#include "../client.h"
#include "../ParticleSystem.h"

void DefaultParticleRenderer::render(float prevTime, float nextTime, const Camera& camera)
{
	RBatch* batch = gfx->CreateBatch(4 * 100, 6 * 100);
	static constexpr float TRISIZE = 1.0f / 30.0f;
	texture curTexture = nullptr;
	gfx->NoTexture(0);
	gfx->SetTransformMatrix(&camera.sceneMatrix);
	for (auto& particle : particleContainer->particles) {
		uint32_t color = particle.getColor(nextTime);
		if (color >> 24 == 0) continue; // don't render particles with alpha 0

		Vector3 pos = particle.getPosition(nextTime);
		Vector3 sc = pos.transformScreenCoords(camera.sceneMatrix);

		if (particle.system->Particles.type == PSType::Sprites) {
			texture tex = texCache.getTexture(particle.system->Particles.Sprites.c_str(), true);
			if (tex != curTexture) {
				batch->flush();
				gfx->SetTexture(0, tex);
				curTexture = tex;
			}
		}

		float ss1, ss2, ssi;
		std::tie(ss1, ss2, ssi) = particle.system->Particles.Scale_Gradient.getAtTime(nextTime - particle.startTime, 1.0f);
		float spriteSize = ss1 + (ss2 - ss1) * ssi;
		//float spriteSize = 1.0f;
		Vector3 sphereEnd = pos + Vector3(0, spriteSize, 0);

		Vector3 camside = camera.direction.cross(Vector3(0, 1, 0)).normal();
		Vector3 camhor = camside.cross(camera.direction).normal();
		Vector3 points[4];
		points[0] = pos + (camside + camhor) * spriteSize * 0.5f;
		points[1] = pos + (camside - camhor) * spriteSize * 0.5f;
		points[2] = pos + (-camside - camhor) * spriteSize * 0.5f;
		points[3] = pos + (-camside + camhor) * spriteSize * 0.5f;

		batchVertex* verts; uint16_t* indices; uint32_t firstIndex;
		batch->next(4, 6, &verts, &indices, &firstIndex);
		verts[0].x = points[0].x; verts[0].y = points[0].y; verts[0].z = points[0].z;
		verts[0].color = color; verts[0].u = 1.0f; verts[0].v = 1.0f;
		verts[1].x = points[1].x; verts[1].y = points[1].y; verts[1].z = points[1].z;
		verts[1].color = color; verts[1].u = 1.0f; verts[1].v = 0.0f;
		verts[2].x = points[2].x; verts[2].y = points[2].y; verts[2].z = points[2].z;
		verts[2].color = color; verts[2].u = 0.0f; verts[2].v = 0.0f;
		verts[3].x = points[3].x; verts[3].y = points[3].y; verts[3].z = points[3].z;
		verts[3].color = color; verts[3].u = 0.0f; verts[3].v = 1.0f;
		indices[0] = firstIndex;
		indices[1] = firstIndex + 3;
		indices[2] = firstIndex + 1;
		indices[3] = firstIndex + 1;
		indices[4] = firstIndex + 3;
		indices[5] = firstIndex + 2;
	}
	batch->flush();
	delete batch;
}
