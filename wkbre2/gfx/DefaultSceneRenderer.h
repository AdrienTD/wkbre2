// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

#include "SceneRenderer.h"
#include <map>
#include <utility>
#include <string>
#include <cstdint>
#include <vector>

//struct ClientGameObject;
struct RBatch;

struct DefaultSceneRenderer : SceneRenderer {
private:
	//std::map<std::pair<std::string, uint8_t>, std::vector<ClientGameObject*>> dic;
	RBatch* batch = nullptr;
public:
	DefaultSceneRenderer(IRenderer* gfx, Scene* scene, Camera* camera) : SceneRenderer(gfx, scene, camera) {}
	void render() override;
};