#pragma once

#include <map>
#include <string>
#include <memory>

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
	virtual float getDuration() { return 1.5f; }
	virtual const float* interpolate(uint32_t animTime) = 0;
	virtual size_t getNumAPs() = 0;
	virtual const AttachmentPoint& getAPInfo(size_t index) = 0;
	virtual AttachmentPointState getAPState(size_t index, uint32_t animTime) = 0;
	virtual std::pair<bool, bool> hasAPFlagSwitched(size_t index, uint32_t prevAnimTime, uint32_t nextAnimTime) { return { false, false }; }
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
	const float* interpolate(uint32_t animTime) override;
	size_t getNumAPs() override;
	const AttachmentPoint& getAPInfo(size_t index) override;
	AttachmentPointState getAPState(size_t index, uint32_t animTime) override;
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
	float getDuration() override;
	const float* interpolate(uint32_t animTime) override;
	size_t getNumAPs() override;
	const AttachmentPoint& getAPInfo(size_t index) override;
	AttachmentPointState getAPState(size_t index, uint32_t animTime) override;
	std::pair<bool, bool> hasAPFlagSwitched(size_t index, uint32_t prevAnimTime, uint32_t nextAnimTime) override;
	void prepare() override;

	Anim &getAnim() {
		prepare();
		return anim;
	}
};

struct ModelCache {
	friend Model;
	friend StaticModel;
	friend AnimatedModel;
private:
	std::map<std::string, std::unique_ptr<Model>, StriCompare> models;
	int numCalls = 0, numHits = 0;
	std::map<uint32_t, Material> materials;
	IndexedStringList textureFilenames;
public:
	Model *getModel(const std::string &filename);
	const Material& getMaterial(int id);
};