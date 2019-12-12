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

struct Model {
	virtual ~Model() {}
	virtual const Material *getMaterials() const = 0;
	virtual const uint32_t *getMaterialIds() const = 0;
	virtual size_t getNumMaterials() const = 0;
	virtual StaticModel *getStaticModel() = 0;
};

struct StaticModel : Model {
	Mesh mesh;
	DynArray<uint32_t> matIds;

	const Material *getMaterials() const override;
	const uint32_t *getMaterialIds() const override;
	size_t getNumMaterials() const override;
	StaticModel *getStaticModel() override;
};

struct AnimatedModel : Model {
	Anim anim;
	StaticModel *staticModel;

	const Material *getMaterials() const override;
	const uint32_t *getMaterialIds() const override;
	size_t getNumMaterials() const override;
	StaticModel *getStaticModel() override;
};

struct ModelCache {
	std::map<std::string, std::unique_ptr<Model>, StriCompare> models;
	int numCalls = 0, numHits = 0;
	std::map<uint32_t, Material> materials;
	IndexedStringList textureFilenames;

	Model *getModel(const std::string &filename);
};