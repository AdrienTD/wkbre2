#include "Model.h"
#include <mutex>

static std::recursive_mutex g_modelPreparationMutex;

Model * ModelCache::getModel(const std::string & filename) {
	std::lock_guard<decltype(g_modelPreparationMutex)> lock(g_modelPreparationMutex);
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

const Material& ModelCache::getMaterial(int id)
{
	std::lock_guard<decltype(g_modelPreparationMutex)> lock(g_modelPreparationMutex);
	return materials.at(id);
}

const Material* StaticModel::getMaterials() { prepare();  return this->mesh.materials.begin(); }
const uint32_t* StaticModel::getMaterialIds() { prepare(); return this->matIds.begin(); }
size_t StaticModel::getNumMaterials() { prepare(); return this->mesh.materials.size(); }
StaticModel* StaticModel::getStaticModel() { prepare(); return this; }
Vector3 StaticModel::getSphereCenter() { prepare(); return mesh.sphereCenter; }
float StaticModel::getSphereRadius() { prepare(); return mesh.sphereRadius; }
const float* StaticModel::interpolate(uint32_t animTime) { prepare(); return mesh.vertices.data(); }
size_t StaticModel::getNumAPs() { prepare(); return mesh.attachPoints.size(); }
const AttachmentPoint& StaticModel::getAPInfo(size_t index) { prepare(); return mesh.attachPoints[index]; }
AttachmentPointState StaticModel::getAPState(size_t index, uint32_t animTime) { prepare(); return mesh.attachPoints[index].staticState; }

void StaticModel::prepare() {
	if (ready) return;
	std::lock_guard<decltype(g_modelPreparationMutex)> lock(g_modelPreparationMutex);
	if (ready) return;

	//printf("Preparing mesh %s\n", fileName.c_str());
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

const Material* AnimatedModel::getMaterials() { prepare();  return this->staticModel->getMaterials(); }
const uint32_t* AnimatedModel::getMaterialIds() { prepare(); return this->staticModel->getMaterialIds(); }
size_t AnimatedModel::getNumMaterials() { prepare(); return this->staticModel->getNumMaterials(); }
StaticModel* AnimatedModel::getStaticModel() { prepare(); return this->staticModel; }
Vector3 AnimatedModel::getSphereCenter() { prepare(); return this->staticModel->getSphereCenter(); }
float AnimatedModel::getSphereRadius() { prepare(); return this->staticModel->getSphereRadius(); }
float AnimatedModel::getDuration() { prepare(); return (float)this->anim.duration / 1000.0f; }
const float* AnimatedModel::interpolate(uint32_t animTime) { prepare(); staticModel->prepare(); return anim.interpolate(animTime, staticModel->mesh); }
size_t AnimatedModel::getNumAPs() { prepare(); return anim.attachPoints.size(); }
const AttachmentPoint& AnimatedModel::getAPInfo(size_t index) { prepare(); return staticModel->getAPInfo(index); }
AttachmentPointState AnimatedModel::getAPState(size_t index, uint32_t animTime) { prepare(); return anim.getAPState(index, animTime); }

std::pair<bool, bool> AnimatedModel::hasAPFlagSwitched(size_t index, uint32_t prevAnimTime, uint32_t nextAnimTime)
{
	prepare();
	auto& aap = anim.attachPoints[index];
	if ((int32_t)prevAnimTime < 0) prevAnimTime = 0; else prevAnimTime %= anim.duration;
	if ((int32_t)nextAnimTime < 0) nextAnimTime = 0; else nextAnimTime %= anim.duration;
	auto prevFrame = aap.getAPFrame(prevAnimTime);
	auto nextFrame = aap.getAPFrame(nextAnimTime);
	return { aap.states[prevFrame].on != aap.states[nextFrame].on, aap.states[nextFrame].on };
}

void AnimatedModel::prepare() {
	if (ready) return;
	std::lock_guard<decltype(g_modelPreparationMutex)> lock(g_modelPreparationMutex);
	if (ready) return;

	//printf("Preparing anim %s\n", fileName.c_str());
	anim.load(fileName.c_str());

	std::string dir;
	if (const char *lastbs = strrchr(fileName.c_str(), '\\'))
		dir = std::string(fileName.c_str(), lastbs + 1);

	staticModel = (StaticModel*)cache->getModel(dir + anim.meshname);
	ready = true;
}
