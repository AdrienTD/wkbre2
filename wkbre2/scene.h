#pragma once

#include <map>
#include <vector>
#include "util/vecmat.h"
#include "mesh.h"
#include "gfx/renderer.h"
#include "gfx/TextureCache.h"

struct Model;
struct ModelCache;

struct SceneEntity {
	Matrix transform;
	Model *model = nullptr;
	int color = 0;
};

struct Scene {
	//struct MaterialLess {
	//	bool operator() (const Material &a, const Material &b) const {
	//		int r = _stricmp(a.textureFilename.c_str(), b.textureFilename.c_str());
	//		if (r == 0) return a.alphaTest < b.alphaTest;
	//		return r < 0;
	//	}
	//};

	struct MaterialInst {
		std::vector<SceneEntity*> list;
		texture tex;
	};

	std::map<uint32_t, MaterialInst> matInsts;
	TextureCache texCache;
	ModelCache *modelCache;

	void add(SceneEntity *ent);
	void clear();

	Scene(IRenderer *gfx, ModelCache *modelCache) : texCache(gfx, "Warrior Kings Game Set\\Textures\\"), modelCache(modelCache) {}
};