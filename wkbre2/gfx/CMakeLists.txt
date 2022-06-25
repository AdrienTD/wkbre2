cmake_minimum_required (VERSION 3.13)

target_sources(${TARGET} PRIVATE
    "bitmap.cpp" "bitmap.h"
    "renderer.h" "renderer_d3d9.cpp"
    "TextureCache.cpp" "TextureCache.h"
    "SceneRenderer.h"
    "DefaultSceneRenderer.cpp" "DefaultSceneRenderer.h"
    "DefaultTerrainRenderer.cpp" "DefaultTerrainRenderer.h"
    "renderer_d3d11.cpp" "renderer_d3d11.h"
    "renderer.cpp"
    "ParticleRenderer.h" "DefaultParticleRenderer.h"
    "DefaultParticleRenderer.cpp" "renderer_ogl3.cpp"
)

if(WIN32)
    target_sources(${TARGET} PRIVATE
        "D3D11EnhancedTerrainRenderer.cpp" "D3D11EnhancedTerrainRenderer.h"
        "D3D11EnhancedSceneRenderer.h" "D3D11EnhancedSceneRenderer.cpp")
endif()
