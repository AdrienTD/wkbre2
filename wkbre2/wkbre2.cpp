// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

// wkbre2.cpp : définit le point d'entrée de l'application.
//

#include "wkbre2.h"
#include <cstdio>
#include "settings.h"
#include "file.h"
#include <nlohmann/json.hpp>

#include "interface/QuickStartMenu.h"
#include "gfx/renderer.h"
#include "window.h"
#include "imguiimpl.h"
#include "gfx/bitmap.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#undef LoadBitmap
#endif

void LaunchTest();

void LaunchQSM() {
	LoadBCP("data.bcp");
	InitWindow();
	ImGuiImpl_Init();
	IRenderer* gfx = CreateRenderer();
	gfx->Init();
	SetRenderer(gfx);
	ImGuiImpl_CreateFontsTexture(gfx);

	texture bgtex = gfx->CreateTexture(Bitmap::loadBitmap("Interface\\Startup Screen.tga"), 1);

	QuickStartMenu qsm{ gfx };

	while (!g_windowQuit) {
		ImGuiImpl_NewFrame();
		qsm.draw();

		gfx->ClearFrame();
		gfx->BeginDrawing();
		gfx->InitRectDrawing();
		gfx->SetTexture(0, bgtex);
		gfx->DrawRect(0, 0, g_windowWidth, g_windowHeight);
		gfx->InitImGuiDrawing();
		ImGuiImpl_Render(gfx);
		gfx->EndDrawing();
		HandleWindow();

		qsm.launchGame();
	}
}

int main()
//int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR cmdArgs, int showMode)
{
	printf("Hello! :)\n");

	LoadSettings();
	strcpy_s(gamedir, g_settings["game_path"].get<std::string>().c_str());

	if (g_settings.value<bool>("test", false))
		LaunchTest();
	else
		LaunchQSM();
	
	return 0;
}
