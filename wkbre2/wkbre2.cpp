// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

// wkbre2.cpp : définit le point d'entrée de l'application.
//

#include "wkbre2.h"
#include <cstdio>
#include <filesystem>
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
	printf("Welcome to wkbre2 ! :)\n\n");

	LoadSettings();
	g_gamePath = std::filesystem::u8path(g_settings.value<std::string>("gamePath", "."));

	if (!std::filesystem::exists(g_gamePath / "data.bcp")) {
		std::string message = "Could not find the file \"data.bcp\".\n\nYou need to copy \"data.bcp\" from Warrior Kings (Battles) game to the following path:\n\n";
		message += std::filesystem::absolute(g_gamePath).u8string();
		message += "\n\nYou can also change the entire path in the configuration file \"wkconfig.json\".";
		printf("%s\n", message.c_str());
#ifdef _WIN32
		MessageBoxA(NULL, message.c_str(), "wkbre2", 16);
#endif
		return -5;
	}

	if (g_settings.value<bool>("test", false))
		LaunchTest();
	else
		LaunchQSM();
	
	return 0;
}
