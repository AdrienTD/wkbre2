#include "TerrainSpriteRenderer.h"

#include "TerrainSpriteContainer.h"
#include "renderer.h"
#include "../terrain.h"
#include "../util/vecmat.h"

#include <algorithm>

void TerrainSpriteRenderer::init()
{
	const int tilesPerBatch = 64;
	batch = gfx->CreateBatch(tilesPerBatch * 4, tilesPerBatch * 6);
}

void TerrainSpriteRenderer::render()
{
	using Point = TerrainSpriteContainer::Point;
	static const float tileSize = 5.0f;
	const float tileOffset = terrain->edge * tileSize;

	gfx->BeginParticles();
	gfx->BeginBatchDrawing();
	gfx->SetBlendColor(-1);
	batch->begin();
	for (const auto& [texptr, spriteList] : spriteContainer->m_spriteListMap) {
		gfx->SetTexture(0, (texture)texptr);
		for (const auto& sprite : spriteList) {
			const auto& p1 = sprite.p1;
			const auto& p2 = sprite.p2;
			const auto& p3 = sprite.p3;
			Point p4;
			p4.x = p2.x + p3.x - p1.x;
			p4.y = p2.y + p3.y - p1.y;
			
			auto [xMin, xMax] = std::minmax({p1.x, p2.x, p3.x, p4.x});
			auto [yMin, yMax] = std::minmax({p1.y, p2.y, p3.y, p4.y});

			int tileStartX = std::max((int)(xMin / tileSize), 0);
			int tileStartZ = std::max((int)(yMin / tileSize), 0);
			int tileEndX = std::min((int)(xMax / tileSize), terrain->width - 1);
			int tileEndZ = std::min((int)(yMax / tileSize), terrain->height - 1);

			Matrix matUv2World = Matrix::getIdentity();
			matUv2World._11 = p2.x - p1.x;
			matUv2World._12 = p2.y - p1.y;
			matUv2World._21 = p3.x - p1.x;
			matUv2World._22 = p3.y - p1.y;
			matUv2World._41 = p1.x;
			matUv2World._42 = p1.y;
			const float det = matUv2World._11 * matUv2World._22 - matUv2World._21 * matUv2World._12;
			Matrix matWorld2Uv = Matrix::getIdentity();
			matWorld2Uv._11 = matUv2World._22 / det;
			matWorld2Uv._12 = -matUv2World._12 / det;
			matWorld2Uv._21 = -matUv2World._21 / det;
			matWorld2Uv._22 = matUv2World._11 / det;
			matWorld2Uv = Matrix::getTranslationMatrix(-Vector3(p1.x, p1.y, 0.0f)) * matWorld2Uv;

			for (int tileZ = tileStartZ; tileZ <= tileEndZ; ++tileZ) {
				for (int tileX = tileStartX; tileX <= tileEndX; ++tileX) {
					batchVertex* verts;
					uint16_t* indices;
					uint32_t startIndex;
					batch->next(4, 6, &verts, &indices, &startIndex);

					for (int i = 0; i < 4; ++i) {
						int px = (i == 1 || i == 2) ? 1 : 0;
						int pz = (i == 2 || i == 3) ? 1 : 0;

						verts[i].x = (tileX + px) * tileSize - tileOffset;
						verts[i].y = terrain->getVertex(tileX + px, tileZ + pz);
						verts[i].z = (tileZ + pz) * tileSize - tileOffset;

						Vector3 t = Vector3((tileX + px) * tileSize, (tileZ + pz) * tileSize, 0.0f).transform(matWorld2Uv);
						verts[i].u = t.x;
						verts[i].v = t.y;
						verts[i].color = 0xFFFFFFFF;
					}
					
					uint16_t* oix = indices;
					for (const int& c : { 0,3,1,1,3,2 })
						*(oix++) = startIndex + c;
				}
			}
		}
		batch->flush();
	}
	batch->end();
}
