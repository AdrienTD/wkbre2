#include "DefaultSceneRenderer.h"
#include "renderer.h"
#include "../scene.h"
#include "../mesh.h"
#include "../Model.h"

void DefaultSceneRenderer::render()
{
	gfx->BeginMeshDrawing();
	gfx->BeginBatchDrawing();
	RBatch *batch = gfx->CreateBatch(16384, 25000);
	batch->begin();

	bool cur_alphatest = false;
	texture cur_texture = 0;
	gfx->DisableAlphaTest();
	gfx->NoTexture(0);

	for (const auto &it : scene->matInsts) {
		Material &mat = scene->modelCache->materials[it.first];
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
			///
			//float *trvtx = (float*)_malloca(mesh.vertices.size() * sizeof(float));
			//for (size_t i = 0; i < mesh.vertices.size(); i += 3) {
			//	Vector3 vp(mesh.vertices[i], mesh.vertices[i + 1], mesh.vertices[i + 2]), tvp;
			//	TransformVector3(&tvp, &vp, &ent->transform);
			//	trvtx[i] = tvp.x; trvtx[i + 1] = tvp.y; trvtx[i + 2] = tvp.z;
			//}
			///
			for (int g = 0; g < polylist.groups.size(); g++) {
				if (model->matIds[g] != it.first)
					continue;
				batchVertex *bver; uint16_t *bind; uint32_t bstart;
				PolyListGroup &group = polylist.groups[g];
				batch->next(mesh.groupIndices[g].size(), group.tupleIndex.size() * 3, &bver, &bind, &bstart);
				for (size_t i = 0; i < mesh.groupIndices[g].size(); i++) {
					IndexTuple &tind = mesh.groupIndices[g][i];
					//float *fl = trvtx + (tind.vertex * 3);
					//bver[i].x = fl[0];
					//bver[i].y = fl[1];
					//bver[i].z = fl[2];
					float *fl = &mesh.vertices[tind.vertex * 3];
					Vector3 prever(fl[0], fl[1], fl[2]), postver;
					TransformVector3(&postver, &prever, &ent->transform);
					bver[i].x = postver.x;
					bver[i].y = postver.y;
					bver[i].z = postver.z;
					bver[i].u = mesh.uvLists[color][2 * tind.uv];
					bver[i].v = mesh.uvLists[color][2 * tind.uv + 1];
					bver[i].color = -1;
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
			//_freea(trvtx);
		}
	}
	
	batch->flush();
	batch->end();
	delete batch;
}