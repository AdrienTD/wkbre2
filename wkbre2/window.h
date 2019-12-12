#pragma once

struct SDL_Window;
struct IRenderer;

extern SDL_Window *g_sdlWindow;
extern bool g_windowQuit;
extern int g_windowWidth, g_windowHeight;
extern bool g_keyDown[512];
extern IRenderer *g_gfxRenderer;
extern bool g_mouseDown[16];
extern int g_mouseX, g_mouseY, g_mouseWheel;

void InitWindow();
void HandleWindow();
void SetRenderer(IRenderer *gfx);
