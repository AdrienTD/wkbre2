#include "DefaultSceneRenderer.h"
#include "renderer.h"
#include "../scene.h"
#include "../mesh.h"
#include "../Model.h"

void DefaultSceneRenderer::render()
{
	gfx->BeginMeshDrawing();
	gfx->BeginBatchDrawing();
	if (!batch)
		batch = gfx->CreateBatch(16384, 25000);
	batch->begin();

	bool cur_alphatest = false;
	texture cur_texture = 0;
	gfx->DisableAlphaTest();
	gfx->NoTexture(0);

	Vector3 sunNormal = scene->sunDirection.normal();

	for (const auto &it : scene->matInsts) {
		const Material &mat = scene->modelCache->getMaterial(it.first.first);
		bool next_alphatest = mat.alphaTest;
		texture next_texture = it.second.tex;
		if (cur_alphatest != next_alphatest || cur_texture != next_texture)
			batch->flush();
		if (cur_alphatest != next_alphatest) {
			if (next_alphatest)
				gfx->EnableAlphaTest();
			else
				gfx->DisableAlphaTest();
		}
		if (cur_texture != next_texture)
			gfx->SetTexture(0, next_texture);
		cur_alphatest = next_alphatest;
		cur_texture = next_texture;

		for (SceneEntity *ent : it.second.list) {
			StaticModel *model = ent->model->getStaticModel();
			Mesh &mesh = model->mesh;
			PolygonList &polylist = mesh.polyLists[0];
			int color = ent->color % mesh.uvLists.size();

			const float* verts = ent->model->interpolate(ent->animTime);
			const Vector3* norms = ent->model->interpolateNormals(ent->animTime);

			for (int g = 0; g < polylist.groups.size(); g++) {
				if (model->matIds[g] != it.first.first)
					continue;
				batchVertex *bver; uint16_t *bind; uint32_t bstart;
				PolyListGroup &group = polylist.groups[g];
				batch->next(mesh.groupIndices[g].size(), group.tupleIndex.size() * 3, &bver, &bind, &bstart);
				for (size_t i = 0; i < mesh.groupIndices[g].size(); i++) {
					IndexTuple &tind = mesh.groupIndices[g][i];
					const float *fl = &verts[tind.vertex * 3];
					Vector3 prever(fl[0], fl[1], fl[2]);
					Vector3 postver = prever.transform(ent->transform);
					bver[i].x = postver.x;
					bver[i].y = postver.y;
					bver[i].z = postver.z;
					if (ent->flags & ent->SEFLAG_NOLIGHTING)
						bver[i].color = 0xFFFFFFFF;
					else
						bver[i].color = (int)(std::max(160.0f, std::min(255.0f, (norms[tind.normal].transformNormal(ent->transform).normal().dot(sunNormal) + 1.0f) * 255 / 2))) * 0x010101 + 0xFF000000;
					bver[i].u = mesh.uvLists[color][2 * tind.uv];
					bver[i].v = mesh.uvLists[color][2 * tind.uv + 1];
				}
				int nxtind = 0;
				for (auto &tuple : group.tupleIndex) {
					for (int i = 0; i < 3; i++) {
						uint16_t triver = tuple[i];
						bind[nxtind++] = bstart + triver;
					}
				}
				break;
			}
		}
	}
	
	batch->flush();
	batch->end();
	//delete batch;
}