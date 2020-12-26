#include "Model.h"

Model * ModelCache::getModel(const std::string & filename) {
	numCalls++;
	auto it = models.find(filename);
	if (it != models.end()) {
		numHits++;
		return it->second.get();
	}

	const char *ext = strrchr(filename.c_str(), '.');
	if (!_stricmp(ext + 1, "mesh3")) {
		StaticModel *m = new StaticModel(this, filename);
		models[filename].reset(m);
		return m;
	}
	else if (!_stricmp(ext + 1, "anim3")) {
		AnimatedModel *m = new AnimatedModel(this, filename);
		models[filename].reset(m);
		return m;
	}
}

const Material* StaticModel::getMaterials() { prepare();  return this->mesh.materials.begin(); }
const uint32_t* StaticModel::getMaterialIds() { prepare(); return this->matIds.begin(); }
size_t StaticModel::getNumMaterials() { prepare(); return this->mesh.materials.size(); }
StaticModel* StaticModel::getStaticModel() { prepare(); return this; }
Vector3 StaticModel::getSphereCenter() { prepare(); return mesh.sphereCenter; }
float StaticModel::getSphereRadius() { prepare(); return mesh.sphereRadius; }

void StaticModel::prepare() {
	//printf("Preparing mesh %s\n", fileName.c_str());
	if (!ready) {
		mesh.load(fileName.c_str());

		matIds.resize(mesh.materials.size());
		for (size_t i = 0; i < matIds.size(); i++) {
			Material &mat = mesh.materials[i];
			cache->textureFilenames.insertString(mat.textureFilename);
			int texid = cache->textureFilenames.getIndex(mat.textureFilename);
			uint32_t matid = (texid << 1) | (mat.alphaTest ? 1 : 0);
			cache->materials[matid] = mat;
			matIds[i] = matid;
		}

		ready = true;
	}
}

const Material* AnimatedModel::getMaterials() { prepare();  return this->staticModel->getMaterials(); }
const uint32_t* AnimatedModel::getMaterialIds() { prepare(); return this->staticModel->getMaterialIds(); }
size_t AnimatedModel::getNumMaterials() { prepare(); return this->staticModel->getNumMaterials(); }
StaticModel* AnimatedModel::getStaticModel() { prepare(); return this->staticModel; }
Vector3 AnimatedModel::getSphereCenter() { prepare(); return this->staticModel->getSphereCenter(); }
float AnimatedModel::getSphereRadius() { prepare(); return this->staticModel->getSphereRadius(); }

void AnimatedModel::prepare() {
	//printf("Preparing anim %s\n", fileName.c_str());
	if (!ready) {
		anim.load(fileName.c_str());

		std::string dir;
		if (const char *lastbs = strrchr(fileName.c_str(), '\\'))
			dir = std::string(fileName.c_str(), lastbs + 1);

		staticModel = (StaticModel*)cache->getModel(dir + anim.meshname);
		ready = true;
	}
}