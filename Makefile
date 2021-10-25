srcRoot = wkbre2.o file.o tags.o test.o server.o client.o window.o imguiimpl.o terrain.o TrnTextureDb.o mesh.o network.o netenetlink.o anim.o Camera.o Model.o scene.o TimeManager.o Order.o Movement.o NNSearch.o settings.o Language.o Trajectory.o SoundPlayer.o WavDocument.o MovementController.o ParticleSystem.o ParticleContainer.o platform.o
srcGameset = gameset/gameset.o gameset/GameObjBlueprint.o gameset/values.o gameset/actions.o gameset/finder.o gameset/command.o gameset/OrderBlueprint.o gameset/position.o gameset/reaction.o gameset/ObjectCreation.o gameset/ScriptContext.o gameset/condition.o gameset/GameTextWindow.o gameset/Package.o gameset/3DClip.o gameset/cameraPath.o gameset/Sound.o gameset/Footprint.o gameset/GSTerrain.o
srcUtil = util/util.o util/GSFileParser.o util/vecmat.o
srcGfx = gfx/bitmap.o gfx/renderer_d3d9.o gfx/TextureCache.o gfx/DefaultSceneRenderer.o gfx/DefaultTerrainRenderer.o gfx/renderer_d3d11.o gfx/renderer.o gfx/DefaultParticleRenderer.o gfx/renderer_ogl3.o
srcInterface = interface/ServerDebugger.o interface/ClientDebugger.o interface/ClientInterface.o interface/GameSetDebugger.o interface/QuickStartMenu.o
srcImgui = imgui/imgui.o imgui/imgui_demo.o imgui/imgui_draw.o imgui/imgui_tables.o imgui/imgui_widgets.o
src = $(srcRoot) $(srcGameset) $(srcUtil) $(srcGfx) $(srcInterface) $(srcImgui)
srcC = lzrw3.o

headers = wkbre2.h file.h lzrw_headers.h tags.h server.h client.h common.h window.h imguiimpl.h terrain.h TrnTextureDb.h mesh.h network.h netenetlink.h anim.h Camera.h Model.h scene.h TimeManager.h Order.h Movement.h GameObjectRef.h NNSearch.h settings.h Language.h Trajectory.h SoundPlayer.h WavDocument.h Pathfinding.h MovementController.h ParticleSystem.h ParticleContainer.h platform.h
headersGameset = gameset/gameset.h gameset/GameObjBlueprint.h gameset/values.h gameset/actions.h gameset/finder.h gameset/command.h gameset/OrderBlueprint.h gameset/CommonEval.h gameset/position.h gameset/reaction.h gameset/ObjectCreation.hg ameset/ScriptContext.h gameset/condition.h gameset/GameTextWindow.h gameset/Package.h gameset/3DClip.h gameset/cameraPath.h gameset/Sound.h gameset/Footprint.h gameset/GSTerrain.h 
headersUtil = util/util.h util/GSFileParser.h util/TagDict.h util/IndexedStringList.h util/vecmat.h util/DynArray.h
headersGfx = gfx/bitmap.h gfx/renderer.h gfx/TextureCache.h gfx/SceneRenderer.h gfx/DefaultSceneRenderer.h gfx/TerrainRenderer.h gfx/DefaultTerrainRenderer.h gfx/ParticleRenderer.h gfx/DefaultParticleRenderer.h 
headersInterface = interface/ServerDebugger.h interface/ClientDebugger.h interface/ClientInterface.h interface/GameSetDebugger.h interface/QuickStartMenu.h 

defs = -Dstrcpy_s=strcpy -D_stricmp=strcasecmp -Dsprintf_s=sprintf -Dstrcmpi=strcasecmp
libs = -lenet -pthread -lGLEW -lGLU -lGL -lopenal -lmpg123 -lbz2 `sdl2-config --libs`
cflags = -O3
cppflags = -std=c++17 -I../inc -O3 -w `sdl2-config --cflags`

all: wkbre2

%.o: %.cpp
	g++ -c $^ -o $@ $(cppflags) $(defs)
	
lzrw3.o: lzrw3.c
	gcc -c lzrw3.c -o lzrw3.o $(cflags) $(defs)

wkbre2: $(src) $(srcC)
	g++ $^ -o wkbre2 $(libs)

clean:
	rm $(src) $(srcC) wkbre2
