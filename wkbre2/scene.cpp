#include "scene.h"
#include "Model.h"

void Scene::add(SceneEntity * ent)
{
	const uint32_t *mat = ent->model->getMaterialIds();
	size_t numMats = ent->model->getNumMaterials();
	for (size_t i = 0; i < numMats; i++) {
		MaterialInst &matinst = matInsts[mat[i]];
		matinst.list.push_back(ent);
		if (!matinst.tex)
			matinst.tex = texCache.getTexture(modelCache->materials[mat[i]].textureFilename.c_str());
	}
}

void Scene::clear()
{
	for (auto &it : matInsts)
		it.second.list.clear();
}
