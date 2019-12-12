#include "global.h"

struct RBatchNULL : public RBatch
{
/*	uint maxverts, maxindis;
	uint curverts, curindis;
*/
	batchVertex *mv; ushort *mi;

	~RBatchNULL() {delete [] mv; delete [] mi;}
	void begin() {}
	void end() {}
	void flush() {curverts = curindis = 0;}
	void next(uint nverts, uint nindis, batchVertex **vpnt, ushort **ipnt, uint *fi)
	{
		if(nverts > maxverts) ferr("Too many vertices to fit in the batch.");
		if(nindis > maxindis) ferr("Too many indices to fit in the batch.");

		if((curverts + nverts > maxverts) || (curindis + nindis > maxindis))
			flush();

		*vpnt = mv + curverts;
		*ipnt = mi + curindis;
		*fi = curverts;

		curverts += nverts; curindis += nindis;
	}
};

struct RVertexBufferNULL : public RVertexBuffer
{
	batchVertex *mem;
	~RVertexBufferNULL() {delete [] mem;}
	batchVertex *lock() {return mem;}
	void unlock() {}
};

struct RIndexBufferNULL : public RIndexBuffer
{
	ushort *mem;
	~RIndexBufferNULL() {delete [] mem;}
	ushort *lock() {return mem;}
	void unlock() {}
};

struct NULLRenderer : public IRenderer
{
	// Initialisation
	void Init() {}
	void Reset() {}

	// Frame begin/end
	void BeginDrawing() {}
	void EndDrawing() {drawframes++;}

	// Textures management
	texture CreateTexture(Bitmap *bm, int mipmaps) {return (texture)0;}
	void FreeTexture(texture t) {}
	void UpdateTexture(texture t, Bitmap *bmp) {}

	// State changes
	void SetTransformMatrix(Matrix *m) {}
	void SetTexture(uint x, texture t) {}
	void NoTexture(uint x) {}
	void SetFog() {}
	void DisableFog() {}
	void EnableAlphaTest() {}
	void DisableAlphaTest() {}
	void EnableColorBlend() {}
	void DisableColorBlend() {}
	void SetBlendColor(int c) {}
	void EnableAlphaBlend() {}
	void DisableAlphaBlend() {}
	void EnableScissor() {}
	void DisableScissor() {}
	void SetScissorRect(int x, int y, int w, int h) {}
	void EnableDepth() {};
	void DisableDepth() {};

	// 2D Rectangles drawing
	void InitRectDrawing() {}
	void DrawRect(int x, int y, int w, int h, int c, float u, float v, float o, float p) {}
	void DrawGradientRect(int x, int y, int w, int h, int c0, int c1, int c2, int c3) {}
	void DrawFrame(int x, int y, int w, int h, int c) {}

	// 3D Landscape/Heightmap drawing
	void CreateMapPart(MapPart *p, int x, int y, int w, int h) {}
	void FreeMapPart(MapPart *p) {}
	void BeginMapDrawing() {}
	void DrawPart(MapPart *p) {}
	void BeginLakeDrawing() {}

	// 3D Mesh drawing
	void CreateMesh(Mesh *m) {}
	void BeginMeshDrawing() {}
	void DrawMesh(Mesh *m, int iwtcolor) {}

	// Batch drawing
	RBatch *CreateBatch(int mv, int mi)
	{
		RBatchNULL *r = new RBatchNULL;
		r->maxverts = mv; r->maxindis = mi;
		r->curverts = r->curindis = 0;
		r->mv = new batchVertex[mv];
		r->mi = new ushort[mi];
		return r;
	}
	void BeginBatchDrawing() {}
	int ConvertColor(int c) {return c;}

	// Buffer drawing
	RVertexBuffer *CreateVertexBuffer(int nv)
	{
		RVertexBufferNULL *r = new RVertexBufferNULL;
		r->size = nv;
		r->mem = new batchVertex[nv];
		return r;
	}
	RIndexBuffer *CreateIndexBuffer(int ni)
	{
		RIndexBufferNULL *r = new RIndexBufferNULL;
		r->size = ni;
		r->mem = new ushort[ni];
		return r;
	}
	void SetVertexBuffer(RVertexBuffer *_rv) {}
	void SetIndexBuffer(RIndexBuffer *_ri) {}
	void DrawBuffer(int first, int count) {}

	// ImGui
	void InitImGuiDrawing() {}
};

IRenderer *CreateNULLRenderer() {return new NULLRenderer;}
