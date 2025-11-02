// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "imguiimpl.h"
#include "imgui/imgui.h"
#include "gfx/renderer.h"
#include "gfx/bitmap.h"
#include "window.h"
#include <cstdint>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>


uint32_t iglasttime;
char *imguiFontFile = 0;
float imguiFontSize = 12;

#define BGRA_TO_RGBA(x) ( (((x)&0xFF)<<16) | (((x)&0xFF0000)>>16) | ((x)&0xFF00FF00) )

void ImGuiImpl_RenderDrawLists(ImDrawData *dr, IRenderer *renderer)
{
	ImGuiIO &io = ImGui::GetIO();
	//if(winMinimized) return;

	renderer->BeginBatchDrawing();
	renderer->EnableScissor();

	for(int i = 0; i < dr->CmdListsCount; i++)
	{
		ImDrawList *cl = dr->CmdLists[i];

		RVertexBuffer *vb = renderer->CreateVertexBuffer(cl->VtxBuffer.size());
		RIndexBuffer *ib = renderer->CreateIndexBuffer(cl->IdxBuffer.size());
		batchVertex *vm = vb->lock();
		for(int j = 0; j < cl->VtxBuffer.size(); j++)
		{
			ImDrawVert *a = &cl->VtxBuffer[j];
			vm[j].x = a->pos.x;
			vm[j].y = a->pos.y;
			vm[j].z = 0;
			vm[j].color = BGRA_TO_RGBA(a->col);
			vm[j].u = a->uv.x;
			vm[j].v = a->uv.y;
		}
		vb->unlock();
		uint16_t *im = ib->lock();
		for(int j = 0; j < cl->IdxBuffer.size(); j++)
			im[j] = cl->IdxBuffer[j];
		ib->unlock();
		renderer->SetVertexBuffer(vb);
		renderer->SetIndexBuffer(ib);

		int e = 0;
		for(int j = 0; j < cl->CmdBuffer.size(); j++)
		{
			ImDrawCmd *cmd = &cl->CmdBuffer[j];
			//if(cmd->TextureId)
				renderer->SetTexture(0, (texture)cmd->GetTexID());
			//else
			//	renderer->NoTexture(0);
			renderer->SetScissorRect((int)cmd->ClipRect.x, (int)cmd->ClipRect.y,
						(int)(cmd->ClipRect.z - cmd->ClipRect.x),
						(int)(cmd->ClipRect.w - cmd->ClipRect.y));
			renderer->DrawBuffer(e, cmd->ElemCount);
			e += cmd->ElemCount;
		}

		delete vb; delete ib;
	}

	renderer->DisableScissor();
}

void ImGuiImpl_CreateFontsTexture(IRenderer *gfx)
{
	ImGuiIO &io = ImGui::GetIO();
	uint8_t *pix; int w, h, bpp;
	if(imguiFontFile)
		io.Fonts->AddFontFromFileTTF(imguiFontFile, imguiFontSize);
	io.Fonts->GetTexDataAsRGBA32(&pix, &w, &h, &bpp);
	Bitmap bm;
	bm.width = w; bm.height = h; bm.format = BMFORMAT_B8G8R8A8;
	bm.pixels.resize(w * h * 4);
	memcpy(bm.pixels.data(), pix, w * h * 4);
	texture t = gfx->CreateTexture(bm, 1);
	io.Fonts->TexID = (void*)t;
}

void ImGuiImpl_Init()
{
	ImGui::CreateContext();
	//imguienabled = true;
	ImGuiIO &io = ImGui::GetIO();

#ifdef _WIN32
	SDL_SysWMinfo syswm;
	SDL_GetWindowWMInfo(g_sdlWindow, &syswm);
	HWND hWindow = syswm.info.win.window;
	//io.ImeWindowHandle = hWindow; // FIXME (as in XXL Editor)
#endif

	iglasttime = SDL_GetTicks();
}

void ImGuiImpl_NewFrame()
{
	ImGuiIO &io = ImGui::GetIO();
	io.DisplaySize = ImVec2((float)g_windowWidth, (float)g_windowHeight);

	uint32_t newtime = SDL_GetTicks();
	io.DeltaTime = (newtime - iglasttime) / 1000.f;
	iglasttime = newtime;

	SDL_Keymod kmod = SDL_GetModState();
	io.KeyCtrl = kmod  & KMOD_CTRL;
	io.KeyShift = kmod & KMOD_SHIFT;
	io.KeyAlt = kmod & KMOD_ALT;
	io.KeySuper = false;

	ImGui::NewFrame();
}

void ImGuiImpl_Render(IRenderer *gfx)
{
	ImGui::Render();
	ImGuiImpl_RenderDrawLists(ImGui::GetDrawData(), gfx);
}
