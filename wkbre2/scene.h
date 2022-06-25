// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

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
	uint32_t animTime = 0;
	uint32_t flags = 0;

	enum Flags {
		SEFLAG_NOLIGHTING = 1, // disable light shading on entity
		SEFLAG_ANIM_CLAMP_END = 2, // clamp to last anim frame instead of repeating animation from the beginning
	};
};

struct Scene {
	//struct MaterialLess {
	//	bool operator() (const Material &a, const Material &b) const {
	//		int r = strcasecmp(a.textureFilename.c_str(), b.textureFilename.c_str());
	//		if (r == 0) return a.alphaTest < b.alphaTest;
	//		return r < 0;
	//	}
	//};

	struct MaterialInst {
		std::vector<SceneEntity*> list;
		texture tex;
	};

	std::map<std::pair<uint32_t, Model*>, MaterialInst> matInsts;
	TextureCache texCache;
	ModelCache *modelCache;

	Vector3 sunDirection = Vector3(1,1,1);

	void add(SceneEntity *ent);
	void clear();

	Scene(IRenderer *gfx, ModelCache *modelCache) : texCache(gfx, "Warrior Kings Game Set\\Textures\\"), modelCache(modelCache) {}
};