#pragma once

#include "SceneRenderer.h"
#include <map>
#include <utility>
#include <string>
#include <cstdint>
#include <vector>

//struct ClientGameObject;

struct DefaultSceneRenderer : SceneRenderer {
private:
	//std::map<std::pair<std::string, uint8_t>, std::vector<ClientGameObject*>> dic;
public:
	DefaultSceneRenderer(IRenderer *gfx, Scene *scene) : SceneRenderer(gfx, scene) {}
	void render() override;
};