#include "renderer.h"
#include "../settings.h"
#include <nlohmann/json.hpp>

IRenderer* CreateRenderer() {
	return (g_settings.value("renderer", "d3d9") == "d3d11") ? CreateD3D11Renderer() : CreateD3D9Renderer();
}