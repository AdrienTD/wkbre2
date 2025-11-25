// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#ifdef _WIN32

#include "renderer.h"
#include "util/util.h"
#include "util/vecmat.h"
#include "bitmap.h"
#include "../window.h"

#include <algorithm>
#include <vector>
#include <d3d9.h>

#include <SDL.h>
#include <SDL_syswm.h>
#include <SDL_system.h>
extern SDL_Window *g_sdlWindow;

typedef unsigned int uint;

struct RBatchD3D9;

const int VSYNCenabled = 1;
const int numBackBuffers = 0;
const int HWVPenabled = 1;
const int fullscreen = 0;
const int mapfogcolor = 0;
const float farzvalue = 250.0f;

IDirect3D9 *d3d9; IDirect3DDevice9 *ddev = 0;
D3DPRESENT_PARAMETERS dpp = {0, 0, D3DFMT_UNKNOWN, 0, D3DMULTISAMPLE_NONE, 0,
	D3DSWAPEFFECT_DISCARD, 0, TRUE, TRUE, D3DFMT_D24X8, D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL, 0, 0/*D3DPRESENT_INTERVAL_IMMEDIATE*/};

D3DPRIMITIVETYPE g_d3d9currentTopology = D3DPT_TRIANGLELIST;

///////////////
// Rect
typedef struct {float x, y, z, rhw; DWORD color; float u, v;} MYVERTEX;
#define FVF_MYVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1)
MYVERTEX verts[] = 
{
    {   0.0f,   0.0f, 0.5f, 1.0f, 0, 0.0f, 0.0f},
    { 639.0f,   0.0f, 0.5f, 1.0f, 0, 1.0f, 0.0f},
    {   0.0f, 479.0f, 0.5f, 1.0f, 0, 0.0f, 1.0f},
    { 639.0f, 479.0f, 0.5f, 1.0f, 0, 1.0f, 1.0f},
    { 639.0f, 479.0f, 0.5f, 1.0f, 0, 1.0f, 1.0f},
};

///////////////
// Mesh
D3DVERTEXELEMENT9 meshddve[] = {
{0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
{1, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
D3DDECL_END()
};
IDirect3DVertexDeclaration9 *meshddvd;

///////////////
// Map
D3DVERTEXELEMENT9 mapvdecli[] = {
 {0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
 {0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
 D3DDECL_END()
};
IDirect3DVertexDeclaration9 *dvdmap;

///////////////
// Batch
D3DVERTEXELEMENT9 batchvdecl[] = {
 {0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
 {0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
 {0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
 D3DDECL_END()
};
IDirect3DVertexDeclaration9 *dvdbatch;
std::vector<RBatchD3D9*> rblist;

////////////////////////////////
////////////////////////////////

void ReleaseDevice()
{
	if(ddev) ddev->Release();
}

struct RVertexBufferD3D9 : public RVertexBuffer
{
	IDirect3DVertexBuffer9 *b;
	~RVertexBufferD3D9() {b->Release();}
	batchVertex *lock()
	{
		batchVertex *r;
		b->Lock(0, 0, (void**)&r, 0);
		return r;
	}
	void unlock()
	{
		b->Unlock();
	}
};

struct RIndexBufferD3D9 : public RIndexBuffer
{
	IDirect3DIndexBuffer9 *b;
	~RIndexBufferD3D9() {b->Release();}
	uint16_t *lock()
	{
		uint16_t *r;
		b->Lock(0, 0, (void**)&r, 0);
		return r;
	}
	void unlock()
	{
		b->Unlock();
	}
};

struct RBatchD3D9 : public RBatch
{
	IDirect3DVertexBuffer9 *vbuf;
	IDirect3DIndexBuffer9 *ibuf;
	batchVertex *vlock;
	uint16_t *ilock;
	bool locked;
	//RBatchD3D9 *rble;

	~RBatchD3D9()
	{
		if(locked) unlock();
		vbuf->Release();
		ibuf->Release();
		rblist.erase(std::remove(rblist.begin(), rblist.end(), this), rblist.end());
	}

	void lock()
	{
		vbuf->Lock(0, maxverts * sizeof(batchVertex), (void**)&vlock, D3DLOCK_DISCARD);
		ibuf->Lock(0, maxindis * 2, (void**)&ilock, D3DLOCK_DISCARD);
		locked = 1;
	}
	void unlock()
	{
		vbuf->Unlock(); ibuf->Unlock(); locked = 0;
	}

	void begin() {}
	void end() {}

	void next(uint nverts, uint nindis, batchVertex **vpnt, uint16_t **ipnt, uint *fi)
	{
		if(nverts > maxverts) ferr("Too many vertices to fit in the batch.");
		if(nindis > maxindis) ferr("Too many indices to fit in the batch.");

		if((curverts + nverts > maxverts) || (curindis + nindis > maxindis))
			flush();

		if(!locked) lock();
		*vpnt = vlock + curverts;
		*ipnt = ilock + curindis;
		*fi = curverts;

		curverts += nverts; curindis += nindis;
	}

	void flush()
	{
		if(locked) unlock();
		if(!curverts) return;
		ddev->SetStreamSource(0, vbuf, 0, sizeof(batchVertex));
		ddev->SetIndices(ibuf);
		int vertsPerPrim = (g_d3d9currentTopology == D3DPT_LINELIST) ? 2 : 3;
		ddev->DrawIndexedPrimitive(g_d3d9currentTopology, 0, 0, curverts, 0, curindis / vertsPerPrim);
		curverts = curindis = 0;
	}
	
};

struct D3D9Renderer : public IRenderer
{

	int maxvbi;

	void Init() override
	{
		SDL_SysWMinfo syswm;
		SDL_VERSION(&syswm.version);
		SDL_GetWindowWMInfo(g_sdlWindow, &syswm);
		HWND hWindow = syswm.info.win.window;
		// Initializing Direct3D 9
		dpp.hDeviceWindow = hWindow;
		dpp.PresentationInterval = VSYNCenabled ? ((VSYNCenabled==2)?D3DPRESENT_INTERVAL_ONE:D3DPRESENT_INTERVAL_DEFAULT) : D3DPRESENT_INTERVAL_IMMEDIATE;
		dpp.BackBufferCount = numBackBuffers;
		dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
		if(fullscreen)
		{
			dpp.BackBufferWidth = g_windowWidth;
			dpp.BackBufferHeight = g_windowHeight;
			dpp.Windowed = 0;
			//dpp.FullScreen_RefreshRateInHz = 60;
		}

		d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
		if(!d3d9) ferr("Direct3D 9 init failed.");
		atexit(ReleaseDevice);

		if(HWVPenabled)
		{
			if(d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWindow,
			   D3DCREATE_HARDWARE_VERTEXPROCESSING /*| D3DCREATE_PUREDEVICE*/, &dpp, &ddev) == D3D_OK)
				goto devok;
			if(d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWindow,
			   D3DCREATE_HARDWARE_VERTEXPROCESSING /*| D3DCREATE_PUREDEVICE*/, &dpp, &ddev) == D3D_OK)
				goto devok;
		}
		if(d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWindow,
		   D3DCREATE_SOFTWARE_VERTEXPROCESSING /*| D3DCREATE_PUREDEVICE*/, &dpp, &ddev) == D3D_OK)
			goto devok;
		if(d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWindow,
		   D3DCREATE_SOFTWARE_VERTEXPROCESSING /*| D3DCREATE_PUREDEVICE*/, &dpp, &ddev) != D3D_OK)
			ferr("Direct3D 9 Device creation failed.");

		//SDL_Renderer *renderer = SDL_CreateRenderer(g_sdlWindow, -1, 0);
		//IDirect3DDevice9 *ddev = SDL_RenderGetD3D9Device(renderer);

	devok:
		// InitMeshDrawing
		ddev->CreateVertexDeclaration(meshddve, &meshddvd);

		// InitMap
		ddev->CreateVertexDeclaration(mapvdecli, &dvdmap);

		ddev->CreateVertexDeclaration(batchvdecl, &dvdbatch);
	}

	void Reset() override
	{
		// Release batches
		for(RBatchD3D9 *b : rblist)
		{
			b->vbuf->Release(); b->ibuf->Release();
			b->curverts = b->curindis = 0;
			b->locked = 0;
		}

		// Reset device
		dpp.BackBufferWidth = g_windowWidth;
		dpp.BackBufferHeight = g_windowHeight;
		HRESULT r = ddev->Reset(&dpp);
		//if(FAILED(r)) ferr("D3D9 device reset failed.");
		//lostdev = 0;

		// Recreate batches
		for (RBatchD3D9 *b : rblist)
		{
			ddev->CreateVertexBuffer(b->maxverts * sizeof(batchVertex), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &b->vbuf, NULL);
			ddev->CreateIndexBuffer(b->maxindis * 2, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &b->ibuf, NULL);
		}
	}

	//***********************************//

	void BeginMeshDrawing() override
	{
		ddev->SetVertexDeclaration(meshddvd);
		ddev->SetRenderState(D3DRS_LIGHTING, FALSE);
		ddev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
		ddev->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
		ddev->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
		ddev->SetRenderState(D3DRS_ALPHAREF, 240);
		ddev->SetRenderState(D3DRS_ZENABLE, TRUE);
		ddev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
		ddev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
		ddev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		ddev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		ddev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		ddev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
	}

	//***********************************//

	void SetTransformMatrix(const Matrix *m) override
	{
		//ddev->SetTransform(D3DTS_VIEW, (D3DMATRIX*)m);
		ddev->SetTransform(D3DTS_PROJECTION, (const D3DMATRIX*)m);
	}

	void SetTexture(uint x, texture t) override
	{
		ddev->SetTexture(x, (IDirect3DTexture9*)t);
	}

	void NoTexture(uint x) override
	{
		ddev->SetTexture(x, 0);
	}

	void BeginDrawing() override
	{
		static D3DMATRIX idmx = { 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 };
		ddev->BeginScene();
		ddev->SetTransform(D3DTS_WORLD, (D3DMATRIX*)&idmx);
		ddev->SetTransform(D3DTS_VIEW, (D3DMATRIX*)&idmx);
		//ddev->SetTransform(D3DTS_PROJECTION, (D3DMATRIX*)&idmx);
	}

	void EndDrawing() override
	{
		ddev->EndScene();
		//drawframes++;
		HRESULT r = ddev->Present(NULL, NULL, NULL, NULL);
		if (r == D3DERR_DEVICELOST) {
			//printf("Device lost\n");
			if (ddev->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
				Reset();
			else
				Sleep(50);
		}
	}

	void ClearFrame(bool clearColors, bool clearDepth, uint32_t color) override {
		DWORD flags = 0;
		if (clearColors) flags |= D3DCLEAR_TARGET;
		if (clearDepth) flags |= D3DCLEAR_ZBUFFER;
		ddev->Clear(NULL, NULL, flags, color, 1.0f, 0);
	}

	void InitRectDrawing() override
	{
		ddev->SetFVF(FVF_MYVERTEX);
		ddev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		ddev->SetRenderState(D3DRS_LIGHTING, FALSE);
		ddev->SetRenderState(D3DRS_ZENABLE, FALSE);
		ddev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		ddev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		ddev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		//ddev->SetRenderState(D3DRS_ZFUNC, D3DCMP_NOTEQUAL);
		ddev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		ddev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		ddev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
	}

	void DrawRect(int x, int y, int w, int h, int c, float u, float v, float o, float p) override
	{
		auto f = [](int n) {return (float)n; };
		verts[0].x = f(x);		verts[0].y = f(y);		verts[0].u = u;		verts[0].v = v;
		verts[1].x = f(x + w);	verts[1].y = f(y);		verts[1].u = u+o;	verts[1].v = v;
		verts[2].x = f(x);		verts[2].y = f(y + h);	verts[2].u = u;		verts[2].v = v+p;
		verts[3].x = f(x + w);	verts[3].y = f(y + h);	verts[3].u = u+o;	verts[3].v = v+p;
		for(int i = 0; i < 4; i++) {verts[i].color = c; verts[i].x -= 0.5f; verts[i].y -= 0.5f;}
		ddev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, verts, sizeof(MYVERTEX));
	}

	void DrawGradientRect(int x, int y, int w, int h, int c0, int c1, int c2, int c3) override
	{
		auto f = [](int n) {return (float)n; };
		verts[0].x = f(x);		verts[0].y = f(y);		verts[0].u = 0.0f;	verts[0].v = 0.0f;
		verts[1].x = f(x + w);	verts[1].y = f(y);		verts[1].u = 1.0f;	verts[1].v = 0.0f;
		verts[2].x = f(x);		verts[2].y = f(y + h);	verts[2].u = 0.0f;	verts[2].v = 1.0f;
		verts[3].x = f(x + w);	verts[3].y = f(y + h);	verts[3].u = 1.0f;	verts[3].v = 1.0f;
		for(int i = 0; i < 4; i++) {verts[i].x -= 0.5f; verts[i].y -= 0.5f;}
		verts[0].color = c0;	verts[1].color = c1;
		verts[2].color = c2;	verts[3].color = c3;
		ddev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, verts, sizeof(MYVERTEX));
	}

	void DrawFrame(int x, int y, int w, int h, int c) override
	{
		auto f = [](int n) {return (float)n; };
		verts[0].x = f(x);		verts[0].y = f(y);
		verts[1].x = f(x + w);	verts[1].y = f(y);
		verts[2].x = f(x + w);	verts[2].y = f(y + h);
		verts[3].x = f(x);		verts[3].y = f(y + h);
		verts[4].x = f(x);		verts[4].y = f(y);
		for(int i = 0; i < 5; i++) {verts[i].color = c; verts[i].x -= 0.5f; verts[i].y -= 0.5f;}
		ddev->DrawPrimitiveUP(D3DPT_LINESTRIP, 4, verts, sizeof(MYVERTEX));
	}

	//***********************************//

	texture CreateTexture(const Bitmap& bm, int mipmaps) override
	{
		const Bitmap *c = &bm;
		Bitmap cvtbmp;
		if (bm.format != BMFORMAT_B8G8R8A8) {
			cvtbmp = bm.convertToB8G8R8A8();
			c = &cvtbmp;
		}

		// D3D9 specific
		IDirect3DTexture9 *dt; D3DLOCKED_RECT lore;
		if(FAILED(ddev->CreateTexture(c->width, c->height, mipmaps, (mipmaps!=1)?D3DUSAGE_AUTOGENMIPMAP:0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &dt, NULL)))
			ferr("Failed to create a D3D9 texture.");
		dt->LockRect(0, &lore, NULL, 0);
		for(int y = 0; y < c->height; y++)
			memcpy( ((char*)(lore.pBits)) + y * lore.Pitch, c->pixels.data() + y * c->width * 4, c->width * 4);
		dt->UnlockRect(0);

		return (texture)dt;
	}

	void FreeTexture(texture t) override
	{
		((IDirect3DTexture9*)t)->Release();
	}

	//***********************************//

	void BeginMapDrawing() override
	{
		ddev->SetRenderState(D3DRS_LIGHTING, FALSE);
		ddev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
		ddev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		ddev->SetRenderState(D3DRS_ZENABLE, TRUE);
		ddev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
		ddev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
		ddev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		ddev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		ddev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		ddev->SetVertexDeclaration(dvdmap);
		ddev->SetVertexShader(0);
		ddev->SetPixelShader(0);
	}

	//***********************************//

	void SetFog(uint32_t color, float farz) override
	{
		float f;
		ddev->SetRenderState(D3DRS_FOGENABLE, TRUE);
		ddev->SetRenderState(D3DRS_FOGCOLOR, color);
		ddev->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
		f = farz/2; ddev->SetRenderState(D3DRS_FOGSTART, *(int*)&f);
		f = farz-8; ddev->SetRenderState(D3DRS_FOGEND, *(int*)&f);
	}

	void DisableFog() override
	{
		ddev->SetRenderState(D3DRS_FOGENABLE, FALSE);
	}

	void EnableAlphaTest() override
	{
		ddev->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
	}

	void DisableAlphaTest() override
	{
		ddev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	}

	void EnableColorBlend() override
	{
		ddev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		ddev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_BLENDFACTOR);
		ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
	}

	void DisableColorBlend() override
	{
		ddev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	}

	void SetBlendColor(int c) override
	{
		ddev->SetRenderState(D3DRS_BLENDFACTOR, c);
	}

	void EnableAlphaBlend() override
	{
		ddev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		ddev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	}

	void DisableAlphaBlend() override
	{
		ddev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	}

	RBatch *CreateBatch(int mv, int mi) override
	{
		RBatchD3D9* b = new RBatchD3D9;
		b->maxverts = mv; b->maxindis = mi;
		b->curverts = b->curindis = 0;
		ddev->CreateVertexBuffer(mv * sizeof(batchVertex), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &b->vbuf, NULL);
		ddev->CreateIndexBuffer(mi * 2, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &b->ibuf, NULL);
		b->locked = 0;
		rblist.push_back(b);
		return b;
	}

	void BeginBatchDrawing() override
	{
		ddev->SetVertexDeclaration(dvdbatch);
	}

	RVertexBuffer *CreateVertexBuffer(int nv) override
	{
		RVertexBufferD3D9 *r = new RVertexBufferD3D9;
		r->size = nv;
		ddev->CreateVertexBuffer(nv*sizeof(batchVertex), D3DUSAGE_DYNAMIC, 0, D3DPOOL_DEFAULT, &r->b, NULL);
		return r;
	}

	RIndexBuffer *CreateIndexBuffer(int ni) override
	{
		RIndexBufferD3D9 *r = new RIndexBufferD3D9;
		r->size = ni;
		ddev->CreateIndexBuffer(ni*sizeof(uint16_t), D3DUSAGE_DYNAMIC, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &r->b, NULL);
		return r;
	}

	void SetVertexBuffer(RVertexBuffer *_rv) override
	{
		maxvbi = ((RVertexBufferD3D9*)_rv)->size;
		ddev->SetStreamSource(0, ((RVertexBufferD3D9*)_rv)->b, 0, sizeof(batchVertex));
	}

	void SetIndexBuffer(RIndexBuffer *_ri) override
	{
		ddev->SetIndices(((RIndexBufferD3D9*)_ri)->b);
	}

	void DrawBuffer(int first, int count) override
	{
		int vertsPerPrim = (g_d3d9currentTopology == D3DPT_LINELIST) ? 2 : 3;
		ddev->DrawIndexedPrimitive(g_d3d9currentTopology, 0, 0, maxvbi, first, count / vertsPerPrim);
	}

	void EnableScissor() override
	{
		ddev->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
	}

	void DisableScissor() override
	{
		ddev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	}

	void SetScissorRect(int x, int y, int w, int h) override
	{
		RECT r = {x, y, x+w, y+h};
		ddev->SetScissorRect(&r);
	}

	void InitImGuiDrawing() override
	{
		ddev->SetVertexDeclaration(dvdbatch);

		ddev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		ddev->SetRenderState(D3DRS_LIGHTING, FALSE);
		ddev->SetRenderState(D3DRS_ZENABLE, FALSE);
		ddev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		ddev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		ddev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		ddev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		ddev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		//ddev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

		ddev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);

		ddev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		ddev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		ddev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		ddev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		ddev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

		Matrix m = Matrix::getZeroMatrix();
		m._11 = 2.0f / g_windowWidth;
		m._22 = -2.0f / g_windowHeight;
		m._41 = -1 - 1.f/g_windowWidth;
		m._42 = 1 + 1.f/g_windowHeight;
		m._44 = 1;
		SetTransformMatrix(&m);
	}

	void BeginLakeDrawing() override
	{
		ddev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		ddev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	}

	int ConvertColor(int c) override
	{
		return c;
	}

	void UpdateTexture(texture t, const Bitmap& bmp) override
	{
		D3DLOCKED_RECT lr;
		((IDirect3DTexture9*)t)->LockRect(0, &lr, NULL, 0);
		//assert(lr.Pitch == bmp->width * 4);
		memcpy(lr.pBits, bmp.pixels.data(), bmp.width * bmp.height * 4);
		((IDirect3DTexture9*)t)->UnlockRect(0);
	}

	void EnableDepth() override
	{
		ddev->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
	}

	void DisableDepth() override
	{
		ddev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
	}

	virtual void BeginParticles() override {
		ddev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		ddev->SetRenderState(D3DRS_LIGHTING, FALSE);
		ddev->SetRenderState(D3DRS_ZENABLE, TRUE);
		ddev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
		ddev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		ddev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		ddev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		ddev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		ddev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		ddev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		ddev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		ddev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
	};

	virtual void SetLineTopology() override {
		g_d3d9currentTopology = D3DPT_LINELIST;
	}

	virtual void SetTriangleTopology() override {
		g_d3d9currentTopology = D3DPT_TRIANGLELIST;
	}
};

IRenderer *CreateD3D9Renderer() {return new D3D9Renderer;}

#endif // _WIN32
