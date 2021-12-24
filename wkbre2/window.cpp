// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "window.h"
#include <SDL2/SDL.h>
#include "imgui/imgui.h"
#include "gfx/renderer.h"
#include "gfx/bitmap.h"

SDL_Window *g_sdlWindow = nullptr;
bool g_windowQuit = false;
int g_windowWidth = 1366, g_windowHeight = 768;
bool g_keyDown[SDL_NUM_SCANCODES], g_keyPressed[SDL_NUM_SCANCODES];
bool g_modCtrl = false, g_modShift = false, g_modAlt = false;
IRenderer *g_gfxRenderer = nullptr;
bool g_mouseDown[16], g_mousePressed[16];
int g_mouseX, g_mouseY, g_mouseWheel;

void InitWindow()
{
	SDL_SetMainReady();
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
#ifndef _WIN32
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	auto flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL;
#else
	auto flags = SDL_WINDOW_RESIZABLE;
#endif
	g_sdlWindow = SDL_CreateWindow("wkbre2 \"Next\"", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, g_windowWidth, g_windowHeight, flags);
}

void HandleImGui(const SDL_Event &event)
{
	ImGuiIO &igio = ImGui::GetIO();
	switch (event.type) {
	case SDL_MOUSEMOTION:
		igio.MousePos = ImVec2(event.motion.x, event.motion.y);
		break;
	case SDL_MOUSEBUTTONDOWN:
		igio.MouseDown[0] = true;
		break;
	case SDL_MOUSEBUTTONUP:
		igio.MouseDown[0] = false;
		break;
	case SDL_MOUSEWHEEL:
		igio.MouseWheel += event.wheel.y;
		break;
	case SDL_KEYDOWN:
		igio.KeysDown[event.key.keysym.scancode] = true;
		break;
	case SDL_KEYUP:
		igio.KeysDown[event.key.keysym.scancode] = false;
		break;
	case SDL_TEXTINPUT:
		igio.AddInputCharactersUTF8(event.text.text);
		break;
	}
}

void HandleWindow()
{
	SDL_Event event;
	bool imguion = ImGui::GetCurrentContext() != nullptr;
	for (int i = 0; i < SDL_NUM_SCANCODES; i++)
		g_keyPressed[i] = false;
	for (bool& b : g_mousePressed)
		b = false;
	g_mouseWheel = 0;

	bool igWantsMouse = false;
	if (imguion) {
		ImGuiIO &io = ImGui::GetIO();
		igWantsMouse = io.WantCaptureMouse;
	}

	SDL_Keymod kmod = SDL_GetModState();
	g_modCtrl = kmod & KMOD_CTRL;
	g_modShift = kmod & KMOD_SHIFT;
	g_modAlt = kmod & KMOD_ALT;

	while (SDL_PollEvent(&event)) {
		if (imguion)
			HandleImGui(event);
		switch (event.type) {
		case SDL_QUIT:
			g_windowQuit = true;
			break;
		case SDL_KEYDOWN:
			g_keyDown[event.key.keysym.scancode] = true;
			g_keyPressed[event.key.keysym.scancode] = true; break;
		case SDL_KEYUP:
			g_keyDown[event.key.keysym.scancode] = false; break;
		case SDL_WINDOWEVENT:
			//printf("window event!!!\n");
			switch (event.window.event) {
			case SDL_WINDOWEVENT_SIZE_CHANGED:
				g_windowWidth = event.window.data1;
				g_windowHeight = event.window.data2;
				printf("Resized! %i %i\n", g_windowWidth, g_windowHeight);
				if (g_gfxRenderer)
					g_gfxRenderer->Reset();
				break;
			}
			break;
		case SDL_MOUSEBUTTONDOWN:
			if (!igWantsMouse) {
				g_mouseDown[event.button.button] = true;
				g_mousePressed[event.button.button] = true;
			}
			break;
		case SDL_MOUSEBUTTONUP:
			if(!igWantsMouse)
				g_mouseDown[event.button.button] = false;
			break;
		case SDL_MOUSEMOTION:
			g_mouseX = event.motion.x;
			g_mouseY = event.motion.y;
			break;
		case SDL_MOUSEWHEEL:
			if(!igWantsMouse)
				g_mouseWheel += event.wheel.y;
			//printf("mouse wheel %i\n", g_mouseWheel);
			break;
		}
	}
}

void SetRenderer(IRenderer *gfx)
{
	g_gfxRenderer = gfx;
}

struct WndCursor
{
	SDL_Cursor *sdlCursor = nullptr;
	int w = 0, h = 0;
};

WndCursor *WndCreateCursor(const char *path) {
	Bitmap cvtbmp = Bitmap::loadBitmap(path).convertToR8G8B8A8();
	int width = cvtbmp.width, height = cvtbmp.height;
	SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(cvtbmp.pixels.data(), cvtbmp.width, cvtbmp.height, 32, cvtbmp.width * 4, SDL_PIXELFORMAT_RGBA32);
	SDL_Cursor *cursor = SDL_CreateColorCursor(surface, 0, 0);
	SDL_FreeSurface(surface);
	return new WndCursor { cursor, width, height };
}

WndCursor *WndCreateSystemCursor(int id) {
	return new WndCursor { SDL_CreateSystemCursor((SDL_SystemCursor)id), 24, 24 };
}

void WndSetCursor(WndCursor *cursor) {
	SDL_SetCursor(cursor->sdlCursor);
}