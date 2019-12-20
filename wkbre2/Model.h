#pragma once

#include <map>
#include <string>

#include "anim.h"
#include "mesh.h"
#include "util/StriCompare.h"
#include "util/DynArray.h"
#include "util/IndexedStringList.h"

//struct Mesh;
//struct Anim;
struct StaticModel;
struct ModelCache;

struct Model {
	ModelCache *cache;
	Model(ModelCache *cache) : cache(cache) {}
	virtual ~Model() {}
	virtual const Material *getMaterials() = 0;
	virtual const uint32_t *getMaterialIds() = 0;
	virtual size_t getNumMaterials() = 0;
	virtual StaticModel *getStaticModel() = 0;
	virtual Vector3 getSphereCenter() { return Vector3(0, 0, 0); }
	virtual float getSphereRadius() { return 3.0f; }
	virtual void prepare() = 0;
};

struct StaticModel : Model {
	Mesh mesh;
	DynArray<uint32_t> matIds;
	std::string fileName;
	bool ready = false;

	StaticModel(ModelCache *cache, const std::string &fileName) : Model(cache), fileName(fileName) {}

	const Material *getMaterials() override;
	const uint32_t *getMaterialIds() override;
	size_t getNumMaterials() override;
	StaticModel *getStaticModel() override;
	Vector3 getSphereCenter() override;
	float getSphereRadius() override;
	void prepare() override;

	Mesh &getMesh() {
		prepare();
		return mesh;
	}
};

struct AnimatedModel : Model {
	Anim anim;
	StaticModel *staticModel;
	std::string fileName;
	bool ready = false;

	AnimatedModel(ModelCache *cache, const std::string &fileName) : Model(cache), fileName(fileName) {}

	const Material *getMaterials() override;
	const uint32_t *getMaterialIds() override;
	size_t getNumMaterials() override;
	StaticModel *getStaticModel() override;
	Vector3 getSphereCenter() override;
	float getSphereRadius() override;
	void prepare() override;

	Anim &getAnim() {
		prepare();
		return anim;
	}
};

struct ModelCache {
	std::map<std::string, std::unique_ptr<Model>, StriCompare> models;
	int numCalls = 0, numHits = 0;
	std::map<uint32_t, Material> materials;
	IndexedStringList textureFilenames;

	Model *getModel(const std::string &filename);
};