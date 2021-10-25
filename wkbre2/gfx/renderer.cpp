#include "renderer.h"
#include "../settings.h"
#include <nlohmann/json.hpp>

IRenderer* CreateRenderer() {
#ifdef _WIN32
	auto setting = g_settings.value("renderer", "d3d9");
	return (setting == "ogl3") ? CreateOGL3Renderer() : ((setting == "d3d11") ? CreateD3D11Renderer() : CreateD3D9Renderer());
#else
	return CreateOGL3Renderer();
#endif
}