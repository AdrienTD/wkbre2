#pragma once

struct SDL_Window;
struct IRenderer;

extern SDL_Window *g_sdlWindow;
extern bool g_windowQuit;
extern int g_windowWidth, g_windowHeight;
extern bool g_keyDown[512], g_keyPressed[512];
extern bool g_modCtrl, g_modShift, g_modAlt;
extern IRenderer *g_gfxRenderer;
extern bool g_mouseDown[16], g_mousePressed[16];
extern int g_mouseX, g_mouseY, g_mouseWheel;

void InitWindow();
void HandleWindow();
void SetRenderer(IRenderer *gfx);

struct WndCursor;

WndCursor *WndCreateCursor(const char *path);
WndCursor *WndCreateSystemCursor(int id);
void WndSetCursor(WndCursor *cursor);
