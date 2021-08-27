# CMakeList.txt : projet CMake pour wkbre2, incluez la source et définissez
# la logique spécifique au projet ici.
#
cmake_minimum_required (VERSION 3.8)

add_subdirectory ("imgui")

find_package (BZip2 REQUIRED)
find_package (SDL2 CONFIG REQUIRED)
find_package (nlohmann_json CONFIG REQUIRED)
find_package(OpenAL CONFIG REQUIRED)
find_package(mpg123 CONFIG REQUIRED)

# Find enet
find_path(ENET_INCLUDE_DIR enet/enet.h)
find_library(ENET_LIBRARY enet)
message(${ENET_LIBRARY})
message(${ENET_INCLUDE_DIR})

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
"gameset/Sound.cpp" "interface/QuickStartMenu.h" "interface/QuickStartMenu.cpp" "resources.rc" "gameset/Footprint.h" "gameset/Footprint.cpp" "gameset/GSTerrain.h" "gameset/GSTerrain.cpp")
target_link_libraries (wkbre2 imgui SDL2::SDL2 d3d9 ${ENET_LIBRARY} ws2_32 BZip2::BZip2 nlohmann_json::nlohmann_json OpenAL::OpenAL winmm MPG123::libmpg123)
target_include_directories(wkbre2 PRIVATE ${ENET_INCLUDE_DIR})

# TODO: Ajoutez des tests et installez des cibles si nécessaire.
