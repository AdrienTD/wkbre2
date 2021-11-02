// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#pragma once

struct IRenderer;

extern char *imguiFontFile;
extern float imguiFontSize;

void ImGuiImpl_Init();
void ImGuiImpl_CreateFontsTexture(IRenderer *gfx);
void ImGuiImpl_NewFrame();
void ImGuiImpl_Render(IRenderer *gfx);
