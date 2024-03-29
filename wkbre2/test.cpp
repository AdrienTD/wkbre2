// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "wkbre2.h"
#include "platform.h"

#include <map>
#include <string>
#include <cstring>
//#include <direct.h>
#include <utility>
#include <thread>
#include "gameset/gameset.h"
#include "file.h"
#include "gameset/values.h"
#include "server.h"
#include "window.h"
#include "gfx/renderer.h"
#include "imgui/imgui.h"
#include "imguiimpl.h"
#include "gfx/TextureCache.h"
#include "TrnTextureDb.h"
#include "terrain.h"
#include <SDL2/SDL_keycode.h>
#include "mesh.h"
#include "client.h"
#include "network.h"
#include "netenetlink.h"
#include "gfx/SceneRenderer.h"
#include "gfx/DefaultSceneRenderer.h"
#include "interface/ServerDebugger.h"
#include "interface/ClientDebugger.h"
#include "anim.h"
#include "Camera.h"
#include "gfx/DefaultTerrainRenderer.h"
#include "interface/ClientInterface.h"
#include "scene.h"
#include <SDL2/SDL_timer.h>
#include "SoundPlayer.h"
#include "Pathfinding.h"
#include "ParticleSystem.h"
#include "gfx/DefaultParticleRenderer.h"
#include "ParticleContainer.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <enet/enet.h>

void Test_GameSet()
{
	LoadBCP("data.bcp"); // C:\\Apps\\Warrior Kings - Battles\\

	GameSet gameSet("Warrior Kings Game Set\\multi-player extensions.cpp", GameSet::GSVERSION_UNKNOWN);

	printf("List of %i items:\n", gameSet.items.names.size());
	for (const std::string &item : gameSet.items.names)
		printf("%s\n", item.c_str());

	printf("List of %i defined values:\n", gameSet.definedValues.size());
	for (auto &dv : gameSet.definedValues)
		printf("%s: %f\n", dv.first.c_str(), dv.second);

	//printf("List of %i equations are their evaluations:\n", gameSet.equationNames.size());
	//for (int i = 0; i < gameSet.equationNames.size(); i++) {
	//	float v = gameSet.equations[i]->eval(nullptr);
	//	printf("%s -> %f\n", gameSet.equationNames[i].c_str(), v);
	//}

	if (GameObjBlueprint *bp = gameSet.findBlueprint(Tags::GAMEOBJCLASS_CHARACTER, "Peasant")) {
		printf("%p", bp);
	}

	getchar();
}

void Test_GSFileParser()
{
	char *test; int testsize;
	LoadBCP("data.bcp");
	LoadFile("Warrior Kings Game Set\\Interrogate.cpp", &test, &testsize, 1);
	test[testsize] = 0;

	GSFileParser gsf(test);
	int linecnt = 1;
	while (!gsf.eof)
	{
		printf("Line %i\n", linecnt++);
		int tagcnt = 1;
		while (!gsf.eol)
		{
			printf("Tag %i: \"%s\"\n", tagcnt++, gsf.nextString().c_str());
		}
		gsf.advanceLine();
		getchar();
	}

	printf("End of file");
	getchar();
}

void Test_Server()
{
	LoadBCP("data.bcp");
	Server server;
	printf("Loading savegame\n");
	server.loadSaveGame("Save_Games\\Interactive Tutorial.lvl");
	printf("Savegame loaded!\n");
	getchar();
}

void Test_Actions()
{
	LoadBCP("data.bcp");
	Server server;
	server.loadSaveGame("Save_Games\\heymap.sav");
	printf("Savegame loaded!\n");

	int actseq = server.gameSet->actionSequences.names.getIndex("My act seq");
	//server.gameSet->actionSequences[actseq].run(server.level);

	getchar();
}

void Test_SDL()
{
	InitWindow();
	IRenderer *gfx = CreateRenderer();
	gfx->Init();
	while (!g_windowQuit) {
		gfx->ClearFrame();
		gfx->BeginDrawing();
		gfx->InitRectDrawing();
		gfx->DrawFrame(64, 64, 128, 128, -1);
		gfx->EndDrawing();
		HandleWindow();
	}
}

void Test_ImGui()
{
	LoadBCP("data.bcp");
	InitWindow();
	ImGuiImpl_Init();

	IRenderer *gfx = CreateRenderer();
	gfx->Init();
	ImGuiImpl_CreateFontsTexture(gfx);

	TextureCache texcache(gfx);
	texture mytex = texcache.getTexture("default_create.tga");

	while (!g_windowQuit) {
		ImGuiImpl_NewFrame();
		ImGui::Text("Hello guys and gals! :)");
		ImGui::Image(mytex, ImVec2(64, 64));

		gfx->ClearFrame();
		gfx->BeginDrawing();
		gfx->InitImGuiDrawing();
		ImGuiImpl_Render(gfx);
		gfx->EndDrawing();

		HandleWindow();
	}
}

void Test_ServerExplorer()
{
	LoadBCP("data.bcp");
	Server server;
	server.loadSaveGame("Save_Games\\Interactive Tutorial.lvl");
	printf("Savegame loaded!\n");

	InitWindow();
	ImGuiImpl_Init();

	IRenderer *gfx = CreateRenderer();
	gfx->Init();
	ImGuiImpl_CreateFontsTexture(gfx);

	ServerDebugger serverdbg(&server);

	while (!g_windowQuit) {
		ImGuiImpl_NewFrame();
		//for (auto ole : server.idmap)
		//	ImGui::Text("%i: %s \"%s\"", ole.first, Tags::GAMEOBJCLASS_tagDict.getStringFromID(ole.second->blueprint->bpClass), ole.second->blueprint->name.c_str());
		serverdbg.draw();

		gfx->ClearFrame();
		gfx->BeginDrawing();
		gfx->InitImGuiDrawing();
		ImGuiImpl_Render(gfx);
		gfx->EndDrawing();

		HandleWindow();
	}
}

void Test_TrnTexDb() {
	LoadBCP("data.bcp");
	TerrainTextureDatabase tdb("Maps\\Map_Textures\\All_Textures.dat");
	for (const auto& grp : tdb.groups) {
		printf("%s\n", grp.second->name.c_str());
	}
	getchar();
}

void Test_Terrain() {
	LoadBCP("data.bcp");
	Terrain trn;
	trn.readFromFile("Maps\\128_4Peaks\\128_4Peaks.bcm");
	//trn.readFromFile("Maps\\rotfliptest\\rotfliptest.bcm");
	printf("yeah!\n");

	InitWindow();
	ImGuiImpl_Init();
	IRenderer *gfx = CreateRenderer();
	gfx->Init();
	SetRenderer(gfx);
	ImGuiImpl_CreateFontsTexture(gfx);

	Camera camera;
	camera.position = Vector3(256, 100, 256);
	camera.orientation = Vector3(0, 0, 0);
	DefaultTerrainRenderer trnrender(gfx, &trn, &camera);

	while (!g_windowQuit)
	{
		ImGuiImpl_NewFrame();
		ImGui::DragFloat3("Position", &camera.position.x);
		ImGui::DragFloat2("Orientation", &camera.orientation.x, 0.1f);
		ImGui::DragFloat3("Sun Vector", &trn.sunVector.x);
		camera.updateMatrix();

		gfx->ClearFrame();
		gfx->BeginDrawing();
		gfx->SetTransformMatrix(&camera.sceneMatrix);

		trnrender.render();

		gfx->InitImGuiDrawing();
		ImGuiImpl_Render(gfx);
		gfx->EndDrawing();

		HandleWindow();
		if (g_keyDown[SDL_SCANCODE_UP])
			camera.position.z += 0.5;
		if (g_keyDown[SDL_SCANCODE_DOWN])
			camera.position.z -= 0.5;
		if (g_keyDown[SDL_SCANCODE_RIGHT])
			camera.position.x += 0.5;
		if (g_keyDown[SDL_SCANCODE_LEFT])
			camera.position.x -= 0.5;
		if (g_keyDown[SDL_SCANCODE_0])
			;

	}

	//getchar();
}



std::string t29curdir = ".", feselfile;
std::vector<std::string> *t29curfiles = nullptr;

void T29_UpdateFiles()
{
	if (t29curfiles) delete t29curfiles;
	t29curfiles = ListFiles(t29curdir.c_str());
}

void T29_TreeDir(const char *s)
{
	std::vector<std::string>* dirs = ListDirectories(s);
	for (std::string &dir : *dirs)
	{
		bool b_open, b_click;
		b_open = ImGui::TreeNodeEx(dir.c_str(), ImGuiTreeNodeFlags_OpenOnArrow);
		b_click = ImGui::IsItemClicked();
		if(b_open)
		{
			std::string tb = s;
			tb.push_back('\\');
			tb += dir;
			T29_TreeDir(tb.c_str());
			ImGui::TreePop();
		}
		if (b_click)
		{
			t29curdir = s;
			t29curdir.push_back('\\');
			t29curdir += dir;
			T29_UpdateFiles();
		}
	}
	delete dirs;
}

void FileExplorer()
{
	ImGui::Begin("File explorer");
	ImGui::Columns(2, "DirsFilesCols", true);

	ImGui::Text("Directory");
	ImGui::NextColumn();
	ImGui::TextUnformatted(t29curdir.c_str());
	ImGui::Separator();

	ImGui::NextColumn();
	ImGui::BeginChild("DirCol");
	T29_TreeDir(".");
	ImGui::EndChild();

	ImGui::NextColumn();
	ImGui::BeginChild("FileCol");
	if (t29curfiles)
		for (const std::string &filename : *t29curfiles) {
			const char *ext = strrchr(filename.c_str(), '.');
			if (!StrCICompare(ext, ".mesh3")) {
				if (ImGui::Selectable(filename.c_str())) {
					feselfile = t29curdir + "\\" + filename;
					printf("Selected file: %s\n", feselfile.c_str());
				}
			}
			else {
				ImGui::TextDisabled("%s", filename.c_str());
			}
		}
	ImGui::EndChild();

	ImGui::Columns(1);
	ImGui::End();
}

void Test_Mesh()
{
	LoadBCP("data.bcp");
	Mesh mesh;
	//mesh.newload("Warrior Kings Game Set\\Buildings\\Houses\\house01\\DEFAULT.MESH3");
	//mesh.newload("Warrior Kings Game Set\\Characters\\Peasant\\Male\\Peasant1.MESH3");
	//mesh.newload("Warrior Kings Game Set\\Characters\\Peasant\\Female\\Mrs1.MESH3");
	//mesh.newload("Warrior Kings Game Set\\Characters\\Add On - Wood Elemental\\WOODELEMENTAL.MESH3");
	mesh.load("MINI_MAP_ARROW5.mesh3");

	InitWindow();
	ImGuiImpl_Init();
	IRenderer *gfx = CreateRenderer();
	gfx->Init();
	SetRenderer(gfx);
	ImGuiImpl_CreateFontsTexture(gfx);

	RBatch *batch = gfx->CreateBatch(400, 600);

	Matrix pers, cammat, persandcam;
	Vector3 campos(0, 0, -20), camdir(0.0, 0, 1);

	TextureCache texcache(gfx, "Warrior Kings Game Set\\Textures\\");
	std::map<std::string, texture> texmap;
	for (Material &mat : mesh.materials) {
		texmap[mat.textureFilename] = texcache.getTexture(mat.textureFilename.c_str());
	}

	while (!g_windowQuit) {
		ImGuiImpl_NewFrame();
		FileExplorer();
		if (!feselfile.empty()) {
			mesh = Mesh();
			mesh.load(feselfile.c_str());
			for (Material &mat : mesh.materials) {
				texmap[mat.textureFilename] = texcache.getTexture(mat.textureFilename.c_str());
			}
			feselfile.clear();
		}

		ImGui::DragFloat3("Position", &campos.x);
		ImGui::DragFloat3("Direction", &camdir.x);
		static int requvlist = 0;
		ImGui::InputInt("UV List", &requvlist);
		requvlist %= mesh.uvLists.size();

		gfx->ClearFrame();
		gfx->BeginDrawing();
		//gfx->InitRectDrawing();
		//gfx->DrawFrame(64, 64, 128, 128, -1);

		gfx->BeginMeshDrawing();
		gfx->BeginBatchDrawing();
		batch->begin();

		pers = Matrix::getLHPerspectiveMatrix(0.9f, (float)g_windowWidth / (float)g_windowHeight, 1.0f, 400.0f);
		cammat = Matrix::getLHLookAtViewMatrix(campos, campos + camdir, Vector3(0, 1, 0));
		persandcam = cammat * pers;
		gfx->SetTransformMatrix(&persandcam);

		std::string curtex;

		int plcnt = 0;
		for (PolygonList &polylist : mesh.polyLists) {
			for (size_t g = 0; g < polylist.groups.size(); g++) {
				PolyListGroup &group = polylist.groups[g];
				if (curtex != mesh.materials[g].textureFilename) {
					batch->flush();
					curtex = mesh.materials[g].textureFilename;
					gfx->SetTexture(0, texmap[curtex]);
				}
				for (auto &tuple : group.tupleIndex) {
					batchVertex *bver; uint16_t *bind; uint32_t bstart;
					batch->next(3, 3, &bver, &bind, &bstart);
					for (int i = 0; i < 3; i++) {
						uint16_t triver = tuple[i];
						IndexTuple &tind = mesh.groupIndices[g][triver];
						bver[i].x = mesh.vertices[3 * tind.vertex] + 6 * plcnt;
						bver[i].y = mesh.vertices[3 * tind.vertex + 1];
						bver[i].z = mesh.vertices[3 * tind.vertex + 2];
						bver[i].u = mesh.uvLists[requvlist][2 * tind.uv];
						bver[i].v = mesh.uvLists[requvlist][2 * tind.uv + 1];
						bver[i].color = -1;
						bind[i] = bstart + i;
					}
				}
			}
			plcnt++;
		}

		batch->flush();
		batch->end();

		gfx->InitImGuiDrawing();
		ImGuiImpl_Render(gfx);
		gfx->EndDrawing();
		HandleWindow();
	}

	//getchar();
}

void Test_Network()
{
	LoadBCP("data.bcp");
	Server server; Client client(true);
	NetLocalBuffer queue1, queue2;
	NetLocalLink srv2cli(&queue1,&queue2), cli2srv(&queue2,&queue1);
	server.addClient(&srv2cli);
	client.serverLink = &cli2srv;

	InitWindow();
	ImGuiImpl_Init();
	IRenderer *gfx = CreateRenderer();
	gfx->Init();
	SetRenderer(gfx);
	ImGuiImpl_CreateFontsTexture(gfx);

	std::vector<std::string> *gsl = ListFiles("Save_Games");

	bool savchosen = false;
	int savselected = 0;
	while (!savchosen) {
		ImGuiImpl_NewFrame();
		ImGui::Begin("Select sav");
		ImGui::ListBoxHeader("##SaveBox");
		for (int i = 0; i < (int)gsl->size(); i++)
			if (ImGui::Selectable(gsl->at(i).c_str(), savselected == i))
				savselected = i;
		ImGui::ListBoxFooter();
		if (ImGui::Button("Let me in"))
			savchosen = true;
		ImGui::End();

		gfx->ClearFrame();
		gfx->BeginDrawing();
		gfx->InitImGuiDrawing();
		ImGuiImpl_Render(gfx);
		gfx->EndDrawing();
		HandleWindow();
	}

	std::string savfile = std::string("Save_Games\\") + gsl->at(savselected);
	std::mutex srvMutex;
	std::thread srvThread([&server, savfile, &srvMutex]() {
		srvMutex.lock();
		server.loadSaveGame(savfile.c_str());
		srvMutex.unlock();
		while (!g_windowQuit) {
			srvMutex.lock();
			server.tick();
			srvMutex.unlock();
			_sleep(1000/60);
		}
	});

	ClientInterface cliui(&client, gfx);
	client.attachInterface(&cliui);
	ServerDebugger srvdbg(&server);
	bool onServerUi = false;

	while (!g_windowQuit) {
		client.tick();
		if (g_keyPressed[SDL_SCANCODE_F1]) {
			onServerUi = !onServerUi;
			if (onServerUi)
				srvMutex.lock();
			else
				srvMutex.unlock();
		}
		if (onServerUi) {
			server.tick();
			ImGuiImpl_NewFrame();
			srvdbg.draw();
			gfx->ClearFrame();
			gfx->BeginDrawing();
			gfx->InitImGuiDrawing();
			ImGuiImpl_Render(gfx);
			gfx->EndDrawing();
			HandleWindow();
		}
		else
			cliui.iter();
	}
	if (onServerUi)
		srvMutex.unlock();

	srvThread.join();
}

void Test_EnetServer() {
	LoadBCP("data.bcp");
	Server server;

	enet_initialize();
	ENetAddress address;
	ENetHost *eserver;
	address.host = ENET_HOST_ANY;
	address.port = 1234;
	eserver = enet_host_create(&address, 16, 2, 0, 0);

	InitWindow();
	ImGuiImpl_Init();
	IRenderer *gfx = CreateRenderer();
	gfx->Init();
	SetRenderer(gfx);
	ImGuiImpl_CreateFontsTexture(gfx);
	ServerDebugger serverdbg(&server);

	while (!g_windowQuit) {
		server.tick();

		ENetEvent event;
		while (enet_host_service(eserver, &event, 0) > 0) {
			switch (event.type) {
			case ENET_EVENT_TYPE_CONNECT: {
				printf("connection\n");
				NetEnetLink *link = new NetEnetLink(eserver, event.peer);
				server.addClient(link);
				event.peer->data = link;
				break;
			}
			case ENET_EVENT_TYPE_DISCONNECT: {
				printf("disconnected\n");
				server.removeClient((NetEnetLink*)event.peer->data);
				break;
			}
			case ENET_EVENT_TYPE_RECEIVE: {
				printf("message: %zu\n", event.packet->dataLength);
				NetEnetLink *link = (NetEnetLink*)event.peer->data;
				link->packets.push(event.packet);
				break;
			}
			}
		}

		ImGuiImpl_NewFrame();
		serverdbg.draw();

		gfx->ClearFrame();
		gfx->BeginDrawing();
		gfx->InitImGuiDrawing();
		ImGuiImpl_Render(gfx);
		gfx->EndDrawing();
		HandleWindow();

	}
	enet_host_destroy(eserver);
	enet_deinitialize();
}

void Test_EnetClient() {
	LoadBCP("data.bcp");
	Client client;

	enet_initialize();
	ENetHost *eclient;
	eclient = enet_host_create(nullptr, 1, 2, 0, 0);

	ENetAddress address;
	enet_address_set_host(&address, "localhost");
	address.port = 1234;

	ENetPeer *pserver = enet_host_connect(eclient, &address, 2, 0);

	InitWindow();
	ImGuiImpl_Init();
	IRenderer *gfx = CreateRenderer();
	gfx->Init();
	SetRenderer(gfx);
	ImGuiImpl_CreateFontsTexture(gfx);

	//DefaultSceneRenderer *scenerender = new DefaultSceneRenderer(gfx, &client);
	//DefaultSceneRenderer scenerender(gfx, &client);

	//ClientDebugger clidbg(&client);
	ClientInterface cliui = ClientInterface(&client, gfx);
	client.attachInterface(&cliui);

	while (!g_windowQuit) {
		client.tick();

		ENetEvent event;
		while (enet_host_service(eclient, &event, 0) > 0) {
			switch (event.type) {
			case ENET_EVENT_TYPE_CONNECT: {
				printf("connection\n");
				NetEnetLink *link = new NetEnetLink(eclient, event.peer);
				client.serverLink = link;
				event.peer->data = link;
				break;
			}
			case ENET_EVENT_TYPE_DISCONNECT: {
				printf("disconnected\n");
				client.serverLink = nullptr;
				break;
			}
			case ENET_EVENT_TYPE_RECEIVE: {
				printf("message: %zu\n", event.packet->dataLength);
				NetEnetLink *link = (NetEnetLink*)event.peer->data;
				assert(link == client.serverLink);
				link->packets.push(event.packet);
				break;
			}
			}
		}

		//ImGuiImpl_NewFrame();
		//clidbg.draw();

		//gfx->BeginDrawing();
		//scenerender.render();
		//gfx->InitImGuiDrawing();
		//ImGuiImpl_Render(gfx);
		//gfx->EndDrawing();
		//HandleWindow();

		cliui.iter();

	}

	enet_host_destroy(eclient);
	enet_deinitialize();
}

void Test_Anim()
{
	LoadBCP("data.bcp");
	Anim anim;
	anim.load("Warrior Kings Game Set\\Characters\\Peasant\\Male\\MOVE1.ANIM3");
	getchar();
}

void Test_Scene()
{
	LoadBCP("data.bcp");

	InitWindow();
	ImGuiImpl_Init();
	IRenderer *gfx = CreateRenderer();
	gfx->Init();
	SetRenderer(gfx);
	ImGuiImpl_CreateFontsTexture(gfx);

	Camera camera;
	camera.position = Vector3(0, 0, -6);
	camera.orientation = Vector3(0, 0, 0);

	ModelCache modcache;
	Model* models[2];
	models[0] = modcache.getModel("Warrior Kings Game Set\\Characters\\Peasant\\Male\\Dance1.ANIM3");
	models[1] = modcache.getModel("Warrior Kings Game Set\\Characters\\Add On - Arch Druid\\Summon.ANIM3");

	const int numents = 1024;

	DynArray<SceneEntity> scents(numents);
	for (SceneEntity &se : scents) {
		se.model = models[rand() & 1];
		se.color = rand() & 7;
	}
	float scalestart[numents], xpos[numents], zpos[numents];
	for (int i = 0; i < numents; i++) {
		scalestart[i] = (rand() % 628) / 100.0f;
		xpos[i] = (float)(rand() % 40 - 20);
		zpos[i] = (float)(rand() % 40 - 20);
	}

	Scene scene(gfx, &modcache);

	SceneRenderer *sceneRenderer = new DefaultSceneRenderer(gfx, &scene, &camera);

	int fps = 0, nextfps = 0;
	uint32_t lastfpscheck = SDL_GetTicks();
	while (!g_windowQuit)
	{
		uint32_t ticks = SDL_GetTicks();
		if (ticks - lastfpscheck >= 1000) {
			fps = nextfps;
			nextfps = 0;
			lastfpscheck = ticks;
		}
		nextfps++;
		ImGuiImpl_NewFrame();
		ImGui::Text("FPS: %i", fps);
		ImGui::DragFloat3("Position", &camera.position.x);
		ImGui::DragFloat2("Orientation", &camera.orientation.x, 0.1f);
		camera.updateMatrix();

		for (int i = 0; i < numents; i++) {
			SceneEntity &se = scents[i];
			float s = (sinf(ticks / 1000.0f + scalestart[i]) + 1.0f) * 0.5f;
			se.transform = Matrix::getScaleMatrix(Vector3(s, s, s));
			se.transform._41 = xpos[i];
			se.transform._43 = zpos[i];
			se.animTime = ticks;
		}

		gfx->ClearFrame();
		gfx->BeginDrawing();
		gfx->SetTransformMatrix(&camera.sceneMatrix);
		for(SceneEntity &se : scents)
			scene.add(&se);
		sceneRenderer->render();
		scene.clear();

		gfx->InitImGuiDrawing();
		ImGuiImpl_Render(gfx);
		gfx->EndDrawing();

		HandleWindow();
		if (g_keyDown[SDL_SCANCODE_UP])
			camera.position.z += 0.5;
		if (g_keyDown[SDL_SCANCODE_DOWN])
			camera.position.z -= 0.5;
		if (g_keyDown[SDL_SCANCODE_RIGHT])
			camera.position.x += 0.5;
		if (g_keyDown[SDL_SCANCODE_LEFT])
			camera.position.x -= 0.5;
		if (g_keyDown[SDL_SCANCODE_0])
			;

	}
}

void Test_SoundPlayer() {
	LoadBCP("data.bcp");

	SoundPlayer* player = SoundPlayer::getSoundPlayer();
	std::string music = R"(Warrior Kings Game Set\Sounds\Map Briefing\Map Music 1.mp2)";
	for (int i = 1; i <= 4; i++) {
		music[53] = '0' + i;
		player->playMusic(music);
		Sleep(4000);
	}
	player->playSound(R"(Warrior Kings Game Set\Sounds\Gongs\gong_3.wav)");
	Sleep(500);
	player->playSound(R"(Warrior Kings Game Set\Sounds\Cog\cog_1.wav)");
	player->playSound(R"(Warrior Kings Game Set\Sounds\Death\death_1_1.wav)");
	printf("Press enter to stop\n");
	getchar();
}

void Test_Pathfinding()
{
	using namespace Pathfinding;
	static constexpr int WIDTH = 1000, HEIGHT = 1000;
	//static constexpr char map[HEIGHT][WIDTH] = {
	//	{1,1,1,1,1,1,1,1,1,1},
	//	{1,1,0,1,1,1,1,1,1,1},
	//	{1,0,0,1,1,1,1,1,1,1},
	//	{1,1,0,0,0,0,0,1,1,1},
	//	{1,1,1,1,0,1,1,1,1,1},
	//	{1,0,0,0,0,1,1,1,1,1},
	//	{1,1,0,1,1,1,1,1,1,1},
	//	{1,1,0,1,1,1,1,1,1,1},
	//	{1,1,0,1,1,1,1,1,1,1},
	//	{1,1,1,1,1,1,1,1,1,1}
	//};
	static constexpr char map[HEIGHT][WIDTH] = {};
	auto pred = [](PFPos pos) -> bool {
		if (pos.x >= 0 && pos.x < WIDTH && pos.z >= 0 && pos.z < HEIGHT)
			return map[pos.z][pos.x] != 0;
		return true;
	};
	auto vec = DoPathfinding({ 2,2 }, { 800,900 }, pred, ManhattanDiagHeuristic);
	printf("Results:\n");
	for (const PFPos& pfp : vec)
		printf("(%i, %i)\n", pfp.x, pfp.z);
	getchar();
}

void Test_ParticleSystem()
{
	LoadBCP("data.bcp");
	PSCache pscache{ "Warrior Kings Game Set\\Particle_Systems" };
	ParticleSystem* ps = pscache.getPS("HighPriestessSpell");

	std::vector<std::string> psNames;
	ListFiles("Warrior Kings Game Set\\Particle_Systems", &psNames);

	InitWindow();
	ImGuiImpl_Init();
	IRenderer* gfx = CreateRenderer();
	gfx->Init();
	SetRenderer(gfx);
	ImGuiImpl_CreateFontsTexture(gfx);

	Camera camera;
	camera.position = Vector3(0, 0, -6);
	camera.orientation = Vector3(0, 0, 0);

	ParticleContainer particleContainer;
	DefaultParticleRenderer particleRenderer(gfx, &particleContainer);

	uint32_t prevTimeMs = SDL_GetTicks();
	uint32_t nextTimeMs = prevTimeMs;

	int fps = 0, nextfps = 0;
	uint32_t lastfpscheck = SDL_GetTicks();
	while (!g_windowQuit)
	{
		uint32_t ticks = SDL_GetTicks();
		if (ticks - lastfpscheck >= 1000) {
			fps = nextfps;
			nextfps = 0;
			lastfpscheck = ticks;
		}
		nextfps++;
		ImGuiImpl_NewFrame();
		ImGui::Text("FPS: %i", fps);
		ImGui::DragFloat3("Position", &camera.position.x);
		ImGui::DragFloat2("Orientation", &camera.orientation.x, 0.1f);
		ImGui::Text("Num particles: %zu", particleContainer.particles.size());
		if (ImGui::BeginListBox("PSystem")) {
			for (auto& name : psNames) {
				if (ImGui::Selectable(name.c_str())) {
					ps = pscache.getPS(name.substr(0, name.find('.')).c_str());
					particleContainer.clearParticles();
				}
			}
			ImGui::EndListBox();
		}
		camera.updateMatrix();

		//
		prevTimeMs = nextTimeMs;
		nextTimeMs = ticks;
		float prevTimeFlt = (float)prevTimeMs / 1000.0f;
		float nextTimeFlt = (float)nextTimeMs / 1000.0f;
		particleContainer.generate(ps, { 0,0,0 }, 0, prevTimeFlt, nextTimeFlt);
		particleContainer.update(nextTimeFlt);

		gfx->ClearFrame();
		gfx->BeginDrawing();
		//gfx->InitRectDrawing();
		//gfx->DrawRect(10, 10, 20, 20);
		gfx->BeginParticles();
		gfx->BeginBatchDrawing();
		Matrix matId = Matrix::getIdentity();
		gfx->SetTransformMatrix(&matId);
		particleRenderer.render(prevTimeFlt, nextTimeFlt, camera);

		gfx->InitImGuiDrawing();
		ImGuiImpl_Render(gfx);
		gfx->EndDrawing();

		HandleWindow();
		if (g_keyDown[SDL_SCANCODE_UP])
			camera.position.z += 0.5;
		if (g_keyDown[SDL_SCANCODE_DOWN])
			camera.position.z -= 0.5;
		if (g_keyDown[SDL_SCANCODE_RIGHT])
			camera.position.x += 0.5;
		if (g_keyDown[SDL_SCANCODE_LEFT])
			camera.position.x -= 0.5;
	}

}

void Test_PFRayTraversal() {
	using namespace Pathfinding;
	static constexpr int WIDTH = 10, HEIGHT = 10;
	static constexpr char map[HEIGHT][WIDTH] = {
		{1,1,1,1,1,1,1,1,1,1},
		{1,0,0,0,0,0,0,0,0,1},
		{1,0,0,0,0,0,0,0,0,1},
		{1,0,0,0,0,0,0,0,0,1},
		{1,0,0,0,0,0,0,0,0,1},
		{1,0,0,0,0,0,0,0,0,1},
		{1,0,0,0,0,0,0,0,0,1},
		{1,0,0,0,0,0,0,0,0,1},
		{1,0,0,0,0,0,0,0,0,1},
		{1,1,1,1,1,1,1,1,1,1}
	};
	auto pred = [](PFPos pos) -> bool {
		if (pos.x >= 0 && pos.x < WIDTH && pos.z >= 0 && pos.z < HEIGHT)
			return map[pos.z][pos.x] != 0;
		return true;
	};
	float start[2] = { 2.0f, 2.0f }, end[2] = { 2.0f, 3.0f };
	auto res = SegmentTraversal(start[0], start[1], end[0], end[1], pred);
	printf("Result: %s, (%i, %i)\n", res.first ? "true" : "false", res.second.x, res.second.z);
	res = SegmentTraversal(end[0], end[1], start[0], start[1], pred);
	printf("Result: %s, (%i, %i)\n", res.first ? "true" : "false", res.second.x, res.second.z);
	getchar();
}

const std::vector<std::pair<void(*)(), const char*> > testList = {
{Test_GameSet, "Game set loading"},
{Test_GSFileParser, "GSF Parser"},
{Test_Server, "Server"},
{Test_Actions, "Actions"},
{Test_SDL, "SDL"},
{Test_ImGui, "ImGui"},
{Test_ServerExplorer, "Server Explorer"},
{Test_TrnTexDb, "Terrain Texture Database"},
{Test_Terrain, "Terrain"},
{Test_Mesh, "Mesh"},
{Test_Network, "Network"},
{Test_EnetServer, "Enet Server"},
{Test_EnetClient, "Enet Client"},
{Test_Anim, "Anim"},
{Test_Scene, "Scene"},
{Test_SoundPlayer, "Sound Player"},
{Test_Pathfinding, "Pathfinding"},
{Test_ParticleSystem, "Particle system"},
{Test_PFRayTraversal, "PF Ray Traversal"},
};

void LaunchTest()
{
	printf("Tests available:\n");
	for (size_t i = 0; i < testList.size(); i++) {
		printf(" %2zu: %s\n", i, testList[i].second);
	}
	printf("Launch test number? ");
	char inp[12];
	gets_s(inp);
	printf("\n");
	size_t testnum = (size_t)atoi(inp);
	if (testnum >= testList.size()) testnum = 0;
	testList[testnum].first();
}
