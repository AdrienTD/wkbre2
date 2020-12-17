#include "DefaultTerrainRenderer.h"
#include "../util/vecmat.h"
#include "renderer.h"
#include "../window.h"
#include "../terrain.h"
#include <utility>
#include <array>
#include "TextureCache.h"
#include "../Camera.h"
#include <cassert>

void DefaultTerrainRenderer::init()
{
	batch = gfx->CreateBatch(4*1024, 6*1024);
	texcache = new TextureCache(gfx, "Maps\\Map_Textures\\");

	// Load terrain color textures
	for (const auto &grp : terrain->texDb.groups) {
		printf("%s\n", grp.first.c_str());
		for (const auto &tex : grp.second->textures) {
			ttexmap[tex.second.get()] = texcache->getTexture(tex.second->file.c_str());
		}
	}
}

DefaultTerrainRenderer::~DefaultTerrainRenderer()
{
	delete batch;
	delete texcache;
}

void DefaultTerrainRenderer::render() {
	//static std::map<int, int> zoccurs;

	gfx->BeginMapDrawing();

	const float tilesize = 5.0f;
	Vector3 sunNormal = terrain->sunVector.normal();

	gfx->BeginBatchDrawing();
	gfx->SetBlendColor(-1);
	batch->begin();
	texture oldgfxtex = 0;
	for (int z = 0; z <= terrain->height; z++) {
		for (int x = 0; x <= terrain->width; x++) {
			if (x < 0 || x >= terrain->width || z < 0 || z >= terrain->height)
				continue;

			int lx = x - terrain->edge, lz = z - terrain->edge;
			Vector3 pp((lx + 0.5f)*tilesize, terrain->getVertex(x, terrain->height - z), (lz + 0.5f)*tilesize), ttpp;
			Vector3 camcenter = camera->position + camera->direction * 125.0f;
			pp += (camcenter - pp).normal() * tilesize * sqrtf(2.0f);
			//TransformVector3(&ttpp, &pp, &camera->sceneMatrix);
			//float oriz = ttpp.z;
			//ttpp /= ttpp.z;
			TransformCoord3(&ttpp, &pp, &camera->sceneMatrix);
			if (ttpp.x < -1 || ttpp.x > 1 || ttpp.y < -1 || ttpp.y > 1 || ttpp.z < -1 || ttpp.z > 1)
				continue;
			//zoccurs[(int)(oriz * 100)]++;
			Terrain::Tile *tile = &terrain->tiles[(terrain->height - 1 - z)*terrain->width + x];
			TerrainTexture *trntex = tile->texture;
			texture newgfxtex = ttexmap[trntex];
			tilesPerTex[newgfxtex].push_back(tile);
		}
	}

	for(auto &pack : tilesPerTex) {
		if (pack.second.empty())
			continue;
		gfx->SetTexture(0, pack.first);
		for (TerrainTile *tile : pack.second) {
			unsigned int x = tile->x;
			unsigned int z = terrain->height - 1 - tile->z;
			int lx = x - terrain->edge, lz = z - terrain->edge;

			TerrainTexture *trntex = tile->texture;
			float sx = trntex->startx / 256.0f;
			float sy = trntex->starty / 256.0f;
			float tw = trntex->width / 256.0f;
			float th = trntex->height / 256.0f;
			std::array<std::pair<float, float>, 4> uvs{ { {sx, sy}, {sx + tw, sy}, { sx + tw, sy + th }, {sx, sy + th } } };
			if (!tile->xflip) {
				std::swap(uvs[0], uvs[3]);
				std::swap(uvs[1], uvs[2]);
			}
			if (tile->zflip) {
				std::swap(uvs[0], uvs[1]);
				std::swap(uvs[2], uvs[3]);
			}
			if (tile->rot) {
				std::rotate(uvs.begin(), uvs.begin() + tile->rot, uvs.end());
			}

			batchVertex *outvert; uint16_t *outindices; unsigned int firstindex;
			batch->next(4, 6, &outvert, &outindices, &firstindex);
			outvert[0].x = lx * tilesize; outvert[0].z = lz * tilesize; outvert[0].y = terrain->getVertex(x, terrain->height - z);
			outvert[1].x = (lx + 1) * tilesize; outvert[1].z = lz * tilesize; outvert[1].y = terrain->getVertex(x + 1, terrain->height - z);
			outvert[2].x = (lx + 1) * tilesize; outvert[2].z = (lz + 1) * tilesize; outvert[2].y = terrain->getVertex(x + 1, terrain->height - (z + 1));
			outvert[3].x = lx * tilesize; outvert[3].z = (lz + 1) * tilesize; outvert[3].y = terrain->getVertex(x, terrain->height - (z + 1));
			outvert[0].color = 0xFF000000 | ((unsigned int)((terrain->getNormal(x, terrain->height - z).dot(sunNormal) + 1) * 255 / 2) * 0x010101);
			outvert[1].color = 0xFF000000 | ((unsigned int)((terrain->getNormal(x + 1, terrain->height - z).dot(sunNormal) + 1) * 255 / 2) * 0x010101);
			outvert[2].color = 0xFF000000 | ((unsigned int)((terrain->getNormal(x + 1, terrain->height - z - 1).dot(sunNormal) + 1) * 255 / 2) * 0x010101);
			outvert[3].color = 0xFF000000 | ((unsigned int)((terrain->getNormal(x, terrain->height - z - 1).dot(sunNormal) + 1) * 255 / 2) * 0x010101);

			for (int i = 0; i < 4; i++) {
				outvert[i].u = uvs[i].first;
				outvert[i].v = uvs[i].second;
			}
			uint16_t *oix = outindices;
			for (const int &c : { 0,1,3,1,2,3 })
				*(oix++) = firstindex + c;
		}
		batch->flush();
		pack.second.clear();
	}
	batch->end();
}