#include "Model.h"

Model * ModelCache::getModel(const std::string & filename) {
	numCalls++;
	auto it = models.find(filename);
	if (it != models.end()) {
		numHits++;
		return it->second.get();
	}

	const char *ext = strrchr(filename.c_str(), '.');
	std::string dir;
	if (const char *lastbs = strrchr(filename.c_str(), '\\'))
		dir = std::string(filename.c_str(), lastbs + 1);
	if (!_stricmp(ext + 1, "mesh3")) {
		StaticModel *m = new StaticModel();
		m->mesh.load(filename.c_str());
		models[filename].reset(m);

		m->matIds.resize(m->mesh.materials.size());
		for (size_t i = 0; i < m->matIds.size(); i++) {
			Material &mat = m->mesh.materials[i];
			textureFilenames.insertString(mat.textureFilename);
			int texid = textureFilenames.getIndex(mat.textureFilename);
			uint32_t matid = (texid << 1) | (mat.alphaTest ? 1 : 0);
			this->materials[matid] = mat;
			m->matIds[i] = matid;
		}

		return m;
	}
	else if (!_stricmp(ext + 1, "anim3")) {
		AnimatedModel *m = new AnimatedModel();
		m->anim.load(filename.c_str());
		m->staticModel = (StaticModel*)getModel(dir + m->anim.meshname);
		models[filename].reset(m);
		return m;
	}
}

const Material* StaticModel::getMaterials() const { return this->mesh.materials.begin(); }
const uint32_t* StaticModel::getMaterialIds() const { return this->matIds.begin(); }
size_t StaticModel::getNumMaterials() const { return this->mesh.materials.size(); }
StaticModel* StaticModel::getStaticModel() { return this; }

const Material* AnimatedModel::getMaterials() const { return this->staticModel->getMaterials(); }
const uint32_t* AnimatedModel::getMaterialIds() const { return this->staticModel->getMaterialIds(); }
size_t AnimatedModel::getNumMaterials() const { return this->staticModel->getNumMaterials(); }
StaticModel* AnimatedModel::getStaticModel() { return this->staticModel; }
