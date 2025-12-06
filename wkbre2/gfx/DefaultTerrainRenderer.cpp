// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

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
#include <algorithm>

void DefaultTerrainRenderer::init()
{
	batch = gfx->CreateBatch(4*256, 6*256);
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

	constexpr float tilesize = 5.0f;
	Vector3 sunNormal = terrain->sunVector.normal();

	// camera space bounding box
	const Vector3& camstart = camera->position;
	Vector3 camend = camera->position + camera->direction * camera->farDist;
	float farheight = std::tan(0.9f) * camera->farDist;
	float farwidth = farheight * camera->aspect;
	Vector3 camside = camera->direction.cross(Vector3(0, 1, 0)).normal();
	Vector3 campup = camside.cross(camera->direction).normal();
	Vector3 farleft = camend + camside * farwidth;
	Vector3 farright = camend - camside * farwidth;
	Vector3 farup = camend + campup * farheight;
	Vector3 fardown = camend - campup * farheight;
	float bbx1, bbz1, bbx2, bbz2;
	std::tie(bbx1, bbx2) = std::minmax({ camstart.x, farleft.x, farright.x, farup.x, fardown.x });
	std::tie(bbz1, bbz2) = std::minmax({ camstart.z, farleft.z, farright.z, farup.z, fardown.z });
	auto clamp = [](auto val, auto low, auto high) {return std::max(low, std::min(high, val)); };
	int tlsx = clamp((int)std::floor(bbx1 / tilesize) + (int)terrain->edge, 0, (int)terrain->width - 1);
	int tlsz = clamp((int)std::floor(bbz1 / tilesize) + (int)terrain->edge, 0, (int)terrain->height - 1);
	int tlex = clamp((int)std::ceil(bbx2 / tilesize) + (int)terrain->edge, 0, (int)terrain->width - 1);
	int tlez = clamp((int)std::ceil(bbz2 / tilesize) + (int)terrain->edge, 0, (int)terrain->height - 1);
	//int tlsx = 0, tlsz = 0, tlex = terrain->width, tlez = terrain->height;

	gfx->BeginBatchDrawing();
	gfx->SetBlendColor(-1);
	batch->begin();
	texture oldgfxtex = 0;
	for (int z = tlsz; z <= tlez; z++) {
		for (int x = tlsx; x <= tlex; x++) {
			int lx = x - terrain->edge, lz = z - terrain->edge;
			Vector3 pp((lx + 0.5f)*tilesize, terrain->getVertex(x, terrain->height - z), (lz + 0.5f)*tilesize);
			Vector3 camcenter = camera->position + camera->direction * 125.0f;
			pp += (camcenter - pp).normal() * tilesize * sqrtf(2.0f);
			//TransformVector3(&ttpp, &pp, &camera->sceneMatrix);
			//float oriz = ttpp.z;
			//ttpp /= ttpp.z;
			Vector3 ttpp = pp.transformScreenCoords(camera->sceneMatrix);
			if (ttpp.x < -1 || ttpp.x > 1 || ttpp.y < -1 || ttpp.y > 1 || ttpp.z < -1 || ttpp.z > 1)
				continue;
			//zoccurs[(int)(oriz * 100)]++;
			const TerrainTile* tile = terrain->getTile(x, z);
			TerrainTexture *trntex = tile->texture;
			texture newgfxtex = ttexmap[trntex];
			tilesPerTex[newgfxtex].push_back(tile);
		}
	}

	for(auto &pack : tilesPerTex) {
		if (pack.second.empty())
			continue;
		gfx->SetTexture(0, pack.first);
		for (const TerrainTile* tile : pack.second) {
			unsigned int x = tile->x;
			unsigned int z = terrain->height - 1 - tile->z;
			int lx = x - terrain->edge, lz = z - terrain->edge;

			TerrainTexture *trntex = tile->texture;
			float sx = trntex->startx / 256.0f;
			float sy = trntex->starty / 256.0f;
			float tw = trntex->width / 256.0f;
			float th = trntex->height / 256.0f;
			std::array<std::pair<float, float>, 4> uvs{ { {sx, sy}, {sx + tw, sy}, { sx + tw, sy + th }, {sx, sy + th } } };
			bool xflip = (bool)tile->xflip != (bool)(tile->rot & 2);
			bool zflip = (bool)tile->zflip != (bool)(tile->rot & 2);
			bool rot = tile->rot & 1;
			if (!xflip) {
				std::swap(uvs[0], uvs[3]);
				std::swap(uvs[1], uvs[2]);
			}
			if (zflip) {
				std::swap(uvs[0], uvs[1]);
				std::swap(uvs[2], uvs[3]);
			}
			if (rot) {
				auto tmp = uvs[0];
				uvs[0] = uvs[1];
				uvs[1] = uvs[2];
				uvs[2] = uvs[3];
				uvs[3] = tmp;
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
			for (const int &c : { 0,3,1,1,3,2 })
				*(oix++) = firstindex + c;
		}
		batch->flush();
		//pack.second.clear();
	}
	batch->end();

	// ----- Lakes -----
	gfx->NoTexture(0);
	batch->begin();
	gfx->BeginLakeDrawing();
	const uint32_t color = (terrain->fogColor & 0xFFFFFF) | 0x80000000;
	for (int z = tlsz; z <= tlez; z++) {
		for (int x = tlsx; x <= tlex; x++) {
			const TerrainTile* tile = terrain->getTile(x, z);
			if (tile && tile->fullOfWater) {
				unsigned int x = tile->x;
				unsigned int z = terrain->height - 1 - tile->z;
				int lx = x - terrain->edge, lz = z - terrain->edge;

				// is water tile visible on screen?
				Vector3 pp((lx + 0.5f) * tilesize, tile->waterLevel, (lz + 0.5f) * tilesize);
				Vector3 camcenter = camera->position + camera->direction * 125.0f;
				pp += (camcenter - pp).normal() * tilesize * sqrtf(2.0f);
				Vector3 ttpp = pp.transformScreenCoords(camera->sceneMatrix);
				if (ttpp.x < -1 || ttpp.x > 1 || ttpp.y < -1 || ttpp.y > 1 || ttpp.z < -1 || ttpp.z > 1)
					continue;

				batchVertex* outvert; uint16_t* outindices; unsigned int firstindex;
				batch->next(4, 6, &outvert, &outindices, &firstindex);
				outvert[0].x = lx * tilesize; outvert[0].y = tile->waterLevel ; outvert[0].z = lz * tilesize;
				outvert[0].color = color; outvert[0].u = 0.0f; outvert[0].v = 0.0f;
				outvert[1].x = (lx + 1) * tilesize; outvert[1].y = tile->waterLevel; outvert[1].z = lz * tilesize;
				outvert[1].color = color; outvert[1].u = 0.0f; outvert[1].v = 0.0f;
				outvert[2].x = (lx + 1) * tilesize; outvert[2].y = tile->waterLevel; outvert[2].z = (lz + 1) * tilesize;
				outvert[2].color = color; outvert[2].u = 0.0f; outvert[2].v = 0.0f;
				outvert[3].x = lx * tilesize; outvert[3].y = tile->waterLevel; outvert[3].z = (lz + 1) * tilesize;
				outvert[3].color = color; outvert[3].u = 0.0f; outvert[3].v = 0.0f;
				uint16_t* oix = outindices;
				for (const int& c : { 0,3,1,1,3,2 })
					*(oix++) = firstindex + c;
			}
		}
	}
	batch->flush();
	batch->end();

	for (auto& pack : tilesPerTex)
		pack.second.clear();
}