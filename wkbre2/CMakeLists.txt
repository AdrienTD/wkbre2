# CMakeList.txt : projet CMake pour wkbre2, incluez la source et définissez
# la logique spécifique au projet ici.
#
cmake_minimum_required (VERSION 3.13)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory ("imgui")

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    add_compile_options("-no-pie")

    set (TARGET "wkbre2")

    find_package (BZip2 REQUIRED)       #libbiz2-dev .
    find_package (SDL2 CONFIG REQUIRED) #libsdl2-dev https://discourse.libsdl.org/t/how-is-sdl2-supposed-to-be-used-with-cmake/31275 .
    find_package (GLEW REQUIRED)        #libglew-dev .
    find_package (OpenGL REQUIRED)      #mesa-utils
    find_package (OpenAL REQUIRED)      #libopenal-dev .
    find_package (MPG123 REQUIRED)      #libmpg123-dev .
    find_package (nlohmann_json REQUIRED)#nlohmann-json3-dev .

    find_path(ENET_INCLUDE_DIR enet/enet.h) #libenet-dev .
    #message(${ENET_INCLUDE_DIR})
    find_library(ENET_LIBRARY enet)
    #message(${ENET_LIBRARY})

    if(OPENAL_FOUND)
        message(STATUS "found!")
    endif()

    add_executable (${TARGET}
        "wkbre2.cpp" "wkbre2.h"
        "file.cpp" "file.h"
        "lzrw3.c" "lzrw_headers.h"
        "tags.cpp" "tags.h"
        "test.cpp" "server.cpp" "server.h"
        "client.cpp" "client.h" "common.h"
        "window.cpp" "window.h"
        "imguiimpl.cpp" "imguiimpl.h"
        "terrain.cpp" "terrain.h"
        "TrnTextureDb.cpp" "TrnTextureDb.h"
        "mesh.cpp" "mesh.h"
        "network.cpp" "network.h"
        "netenetlink.cpp" "netenetlink.h"
        "anim.cpp" "anim.h" "gfx/TerrainRenderer.h"
        "Camera.cpp" "Camera.h"
        "Model.cpp" "Model.h"
        "scene.cpp" "scene.h"
        "TimeManager.cpp" "TimeManager.h"
        "Order.cpp" "Order.h"
        "Movement.cpp" "Movement.h"
        "GameObjectRef.h"
        "NNSearch.cpp" "NNSearch.h"
        "settings.cpp" "settings.h"
        "Language.h" "Language.cpp"
        "Trajectory.h" "Trajectory.cpp"
        "SoundPlayer.h" "SoundPlayer.cpp"
        "WavDocument.h" "WavDocument.cpp"
        "resources.rc"
        "Pathfinding.h"
        "MovementController.h" "MovementController.cpp"
        "ParticleSystem.h" "ParticleSystem.cpp"
        "ParticleContainer.h" "ParticleContainer.cpp"
        "AIController.h" "AIController.cpp"
        "common.cpp"
        "platform.h" "platform.cpp")

    #sources from subfolders
    add_subdirectory ("gameset")
    add_subdirectory ("gfx")
    add_subdirectory ("interface")
    add_subdirectory ("util")

    target_link_libraries(${TARGET} PRIVATE
        SDL2::SDL2
        GLEW::GLEW
        OpenGL::OpenGL
        BZip2::BZip2
        imgui
        ${ENET_LIBRARY}
        ${OPENAL_LIBRARY}
        ${MPG123_LIBRARIES}
        nlohmann_json::nlohmann_json)
    target_include_directories(${TARGET} PRIVATE
        "/usr/include/stb"
        ${ENET_INCLUDE_DIR}
        ${OPENAL_INCLUDE_DIR}
        ${MPG123_INCLUDE_DIR})

else()

    find_package (BZip2 REQUIRED)
    find_package (SDL2 CONFIG REQUIRED)
    find_package (nlohmann_json CONFIG REQUIRED)
    find_package(OpenAL CONFIG REQUIRED)
    find_package(mpg123 CONFIG REQUIRED)
    find_package(glew CONFIG REQUIRED)

    # Find enet
    find_path(ENET_INCLUDE_DIR enet/enet.h)
    find_library(ENET_LIBRARY enet)
    message(${ENET_LIBRARY})
    message(${ENET_INCLUDE_DIR})

    # Find Discord-RPC
    find_path(DISCORDRPC_INCLUDE_DIR discord_rpc.h)
    find_library(DISCORDRPC_LIBRARY discord-rpc)

    # Ajoutez une source à l'exécutable de ce projet.
    add_executable (wkbre2 "wkbre2.cpp" "wkbre2.h" "file.cpp" "file.h" "lzrw3.c" "lzrw_headers.h" "util/util.cpp" "util/util.h" "util/GSFileParser.cpp" "util/GSFileParser.h"
      "gameset/gameset.cpp" "gameset/gameset.h" "tags.cpp" "tags.h" "util/TagDict.h" "gameset/GameObjBlueprint.cpp" "gameset/GameObjBlueprint.h"
    "util/IndexedStringList.h" "gameset/values.cpp" "gameset/values.h" "test.cpp" "server.cpp" "server.h" "client.cpp" "client.h" "common.h" "gameset/actions.cpp" "gameset/actions.h"
    "gameset/finder.cpp" "gameset/finder.h" "window.cpp" "window.h" "util/vecmat.cpp" "util/vecmat.h" "gfx/bitmap.cpp" "gfx/bitmap.h" "gfx/renderer.h" "gfx/renderer_d3d9.cpp"
    "imguiimpl.cpp" "imguiimpl.h" "terrain.cpp" "terrain.h" "TrnTextureDb.cpp" "TrnTextureDb.h" "gfx/TextureCache.cpp" "gfx/TextureCache.h" "mesh.cpp" "mesh.h" "network.cpp" "network.h"
    "util/DynArray.h" "netenetlink.cpp" "netenetlink.h" "gfx/SceneRenderer.h" "gfx/DefaultSceneRenderer.cpp" "gfx/DefaultSceneRenderer.h" "gameset/command.cpp" "gameset/command.h"
    "interface/ServerDebugger.cpp" "interface/ServerDebugger.h" "interface/ClientDebugger.cpp" "interface/ClientDebugger.h" "anim.cpp" "anim.h" "gfx/TerrainRenderer.h"
    "gfx/DefaultTerrainRenderer.cpp" "gfx/DefaultTerrainRenderer.h" "Camera.cpp" "Camera.h" "interface/ClientInterface.cpp" "interface/ClientInterface.h" "Model.cpp" "Model.h"
    "scene.cpp" "scene.h" "TimeManager.cpp" "TimeManager.h" "gameset/OrderBlueprint.cpp" "gameset/OrderBlueprint.h" "order.cpp" "order.h" "Movement.cpp" "Movement.h" "GameObjectRef.h"
    "gameset/CommonEval.h" "gameset/position.cpp" "gameset/position.h" "gameset/reaction.cpp" "gameset/reaction.h" "gameset/ObjectCreation.cpp" "gameset/ObjectCreation.h"
    "NNSearch.cpp" "NNSearch.h" "gameset/ScriptContext.cpp" "gameset/ScriptContext.h" "gameset/condition.cpp" "gameset/condition.h" "gameset/GameTextWindow.h" "gameset/GameTextWindow.cpp"
    "gameset/Package.h" "gameset/Package.cpp" "gameset/3DClip.h" "gameset/3DClip.cpp" "settings.cpp" "settings.h" "gameset/cameraPath.h" "gameset/cameraPath.cpp" "Language.h" "Language.cpp"
    "Trajectory.h" "Trajectory.cpp" "interface/GameSetDebugger.h" "interface/GameSetDebugger.cpp" "SoundPlayer.h" "SoundPlayer.cpp" "WavDocument.h" "WavDocument.cpp" "gameset/Sound.h"
    "gameset/Sound.cpp" "interface/QuickStartMenu.h" "interface/QuickStartMenu.cpp" "resources.rc" "gameset/Footprint.h" "gameset/Footprint.cpp" "gameset/GSTerrain.h" "gameset/GSTerrain.cpp"
    "gfx/renderer_d3d11.cpp" "gfx/renderer.cpp" "Pathfinding.h" "MovementController.h" "MovementController.cpp" "ParticleSystem.h" "ParticleSystem.cpp" "ParticleContainer.h"
    "ParticleContainer.cpp" "gfx/ParticleRenderer.h" "gfx/DefaultParticleRenderer.h" "gfx/DefaultParticleRenderer.cpp" "gfx/renderer_ogl3.cpp" "gfx/D3D11EnhancedTerrainRenderer.cpp"
    "gfx/D3D11EnhancedTerrainRenderer.h" "gfx/renderer_d3d11.h" "gfx/D3D11EnhancedSceneRenderer.h" "gfx/D3D11EnhancedSceneRenderer.cpp" "gameset/Plan.cpp" "gameset/Plan.h"  "AIController.h" "AIController.cpp" "gameset/ArmyCreationSchedule.h" "gameset/ArmyCreationSchedule.cpp" "gameset/WorkOrder.h" "gameset/WorkOrder.cpp" "common.cpp")
    target_link_libraries (wkbre2 imgui SDL2::SDL2 d3d9 ${ENET_LIBRARY} ws2_32 BZip2::BZip2 nlohmann_json::nlohmann_json OpenAL::OpenAL winmm MPG123::libmpg123 d3d11 d3dcompiler ${DISCORDRPC_LIBRARY} opengl32 GLEW::GLEW dxgi)
    target_include_directories(wkbre2 PRIVATE ${ENET_INCLUDE_DIR} ${DISCORDRPC_INCLUDE_DIR})

    # TODO: Ajoutez des tests et installez des cibles si nécessaire.

endif()
