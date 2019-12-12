// wkbre - WK (Battles) recreated game engine
// Copyright (C) 2015-2016 Adrien Geets
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "global.h"
#include <gl/gl.h>
#include <gl/glu.h>

#define glprocvalid(x) ( (((scpuint)(x)) < -1) || (((scpuint)(x)) > 3) )
#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif

typedef BOOL (APIENTRY *gli_wglSwapIntervalEXT)(int n);
typedef int (APIENTRY *gli_wglGetSwapIntervalEXT)();

#ifndef DONT_USE_OWN_OPENGL_DEFINITIONS
#define GLsizeiptrARB ucpuint
#define GL_ARRAY_BUFFER_ARB 0x8892
#define GL_ELEMENT_ARRAY_BUFFER_ARB 0x8893
#define GL_STREAM_DRAW_ARB 0x88E0
#define GL_STREAM_READ_ARB 0x88E1
#define GL_STREAM_COPY_ARB 0x88E2
#define GL_STATIC_DRAW_ARB 0x88E4
#define GL_STATIC_READ_ARB 0x88E5
#define GL_STATIC_COPY_ARB 0x88E6
#define GL_DYNAMIC_DRAW_ARB 0x88E8
#define GL_DYNAMIC_READ_ARB 0x88E9
#define GL_DYNAMIC_COPY_ARB 0x88EA
#define GL_READ_ONLY_ARB 0x88B8
#define GL_WRITE_ONLY_ARB 0x88B9
#define GL_READ_WRITE_ARB 0x88BA
#endif

typedef void  (APIENTRY *gli_glBindBufferARB)(GLenum target, GLuint buffer);
typedef void  (APIENTRY *gli_glDeleteBuffersARB)(GLsizei n, /*const*/ GLuint *buffers);
typedef void  (APIENTRY *gli_glGenBuffersARB)(GLsizei n, GLuint *buffers);
typedef void  (APIENTRY *gli_glBufferDataARB)(GLenum target, GLsizeiptrARB size, /*const*/ void *data, GLenum usage);
typedef void* (APIENTRY *gli_glMapBufferARB)(GLenum target, GLenum access);
typedef GLboolean (APIENTRY *gli_glUnmapBufferARB)(GLenum target);

gli_glBindBufferARB glBindBufferARB;
gli_glDeleteBuffersARB glDeleteBuffersARB;
gli_glGenBuffersARB glGenBuffersARB;
gli_glBufferDataARB glBufferDataARB;
gli_glMapBufferARB glMapBufferARB;
gli_glUnmapBufferARB glUnmapBufferARB;

HDC whdc; HGLRC glrc;
bool rglUseBufferObjects = 0;

void OGL1Quit()
{
	if(fullscreen)
		ChangeDisplaySettings(NULL, 0);
}

#define BGRA_TO_RGBA(x) ( (((x)&0xFF)<<16) | (((x)&0xFF0000)>>16) | ((x)&0xFF00FF00) )

struct RBatchOGL1 : public RBatch
{
	batchVertex *vbuf; ushort *ibuf;

	~RBatchOGL1() {delete [] vbuf; delete [] ibuf;}

	void begin() {}
	void end() {}

	void next(uint nverts, uint nindis, batchVertex **vpnt, ushort **ipnt, uint *fi)
	{
		if(nverts > maxverts) ferr("Too many vertices to fit in the batch.");
		if(nindis > maxindis) ferr("Too many indices to fit in the batch.");
		if((curverts + nverts > maxverts) || (curindis + nindis > maxindis))
			flush();

		*vpnt = vbuf + curverts;
		*ipnt = ibuf + curindis;
		*fi = curverts;

		curverts += nverts; curindis += nindis;
	}

	void flush()
	{
		if(!curverts) return;
		//for(int i = 0; i < curverts; i++)
		//	vbuf[i].color = BGRA_TO_RGBA(vbuf[i].color);
		glVertexPointer(3, GL_FLOAT, sizeof(batchVertex), &vbuf->x);
		glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(batchVertex), &vbuf->color);
		glTexCoordPointer(2, GL_FLOAT, sizeof(batchVertex), &vbuf->u);
		glDrawElements(GL_TRIANGLES, curindis, GL_UNSIGNED_SHORT, ibuf);
		curverts = curindis = 0;
	}
};

void chkglerr()
{/*
	GLenum e = glGetError();
	if(e != GL_NO_ERROR)
		ferr("OpenGL error %04X.", e);*/
}

struct RBatchOGL1_BO : public RBatch
{
	GLuint vbuf, ibuf;
	batchVertex *vlock;
	ushort *ilock;
	bool locked;
	//DynListEntry<RBatchOGL1_BO*> *rble;

	~RBatchOGL1_BO()
	{
/*		glBindBufferARB(GL_ARRAY_BUFFER_ARB, vbuf);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, ibuf);
*/
		if(locked) unlock();
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
		glDeleteBuffersARB(1, &vbuf);
		glDeleteBuffersARB(1, &ibuf);
		//rblist.remove(rble);
	}

	void lock()
	{
/*		glBindBufferARB(GL_ARRAY_BUFFER_ARB, vbuf);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, ibuf);
*/
		glBufferDataARB(GL_ARRAY_BUFFER_ARB, maxverts * sizeof(batchVertex), NULL, GL_STREAM_DRAW_ARB);
		glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, maxindis * 2, NULL, GL_STREAM_DRAW_ARB);

		vlock = (batchVertex*)glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB); chkglerr();
		ilock = (ushort*)glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB); chkglerr();
		if(!vlock) ferr("glbo vlock is 0");
		if(!ilock) ferr("glbo ilock is 0");
		locked = 1;
	}
	void unlock()
	{
		glUnmapBufferARB(GL_ARRAY_BUFFER_ARB); chkglerr();
		glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB); chkglerr();
		locked = 0;
	}

	void begin()
	{
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, vbuf);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, ibuf);
	}
	void end()
	{
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	}

	void next(uint nverts, uint nindis, batchVertex **vpnt, ushort **ipnt, uint *fi)
	{
/*		glBindBufferARB(GL_ARRAY_BUFFER_ARB, vbuf);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, ibuf);
*/
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
/*		glBindBufferARB(GL_ARRAY_BUFFER_ARB, vbuf);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, ibuf);
*/
		if(locked) unlock();
		if(!curverts) goto flshend;
		glVertexPointer(3, GL_FLOAT, sizeof(batchVertex), &((batchVertex*)0)->x);
		glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(batchVertex), &((batchVertex*)0)->color);
		glTexCoordPointer(2, GL_FLOAT, sizeof(batchVertex), &((batchVertex*)0)->u);
		glDrawElements(GL_TRIANGLES, curindis, GL_UNSIGNED_SHORT, 0);

flshend:	curverts = curindis = 0;
/*		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
*/
	}
};

struct RVertexBufferOGL1 : public RVertexBuffer
{
	batchVertex *m;
	~RVertexBufferOGL1() {delete [] m;}
	batchVertex *lock() {return m;}
	void unlock()
	{
		for(int i = 0; i < size; i++)
			m[i].color = BGRA_TO_RGBA(m[i].color);
	}
};

struct RIndexBufferOGL1 : public RIndexBuffer
{
	ushort *m;
	~RIndexBufferOGL1() {delete [] m;}
	ushort *lock() {return m;}
	void unlock() {}
};

struct OGL1Renderer : public IRenderer
{

batchVertex *curvtxmem; ushort *curidxmem;

void InitGL()
{
	glShadeModel(GL_SMOOTH);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClearDepth(1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_TEXTURE_2D);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
}

void ResizeGL()
{
	glViewport(0, 0, scrw, scrh);
}

void Init()
{
	whdc = GetDC(hWindow);

	// Set the pixel format
	PIXELFORMATDESCRIPTOR pfd;
	memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR)); // Be sure that pfd is filled with 0.
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cDepthBits = 32;
	pfd.dwLayerMask = PFD_MAIN_PLANE;
	int i = ChoosePixelFormat(whdc, &pfd);
	SetPixelFormat(whdc, i, &pfd);

	glrc = wglCreateContext(whdc);
	wglMakeCurrent(whdc, glrc);
	//char *glexts = (char*)glGetString(GL_EXTENSIONS);
	//printf("%s\n", glexts);

	if(fullscreen)
	{
		DEVMODE dm;
		memset(&dm, 0, sizeof(dm));
		dm.dmSize = sizeof(dm);
		dm.dmPelsWidth = scrw;
		dm.dmPelsHeight = scrh;
		//dm.dmBitsPerPel = 32;
		dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT; // | DM_BITSPERPEL;
		ChangeDisplaySettings(&dm, CDS_FULLSCREEN);
	}

	// Set VSYNC if enabled
	if(VSYNCenabled)
	{
		gli_wglSwapIntervalEXT wglSwapIntervalEXT = (gli_wglSwapIntervalEXT)wglGetProcAddress("wglSwapIntervalEXT");
		//gli_wglGetSwapIntervalEXT wglGetSwapIntervalEXT = (gli_wglGetSwapIntervalEXT)wglGetProcAddress("wglGetSwapIntervalEXT");
		if(glprocvalid(wglSwapIntervalEXT))
		{
			//__asm int 3
			//printf(":)\n");
			if(!wglSwapIntervalEXT(1))
				printf("wglSwapIntervalEXT returned FALSE.\n");
			//if(wglGetSwapIntervalEXT)
			//	printf("wglGetSwapIntervalEXT returns %i.\n", wglGetSwapIntervalEXT());
		}
		else printf("wglSwapIntervalEXT unsupported.\n");
	}

	if(rglUseBufferObjects)
	{
		glBindBufferARB = (gli_glBindBufferARB)wglGetProcAddress("glBindBufferARB");
		glDeleteBuffersARB = (gli_glDeleteBuffersARB)wglGetProcAddress("glDeleteBuffersARB");
		glGenBuffersARB = (gli_glGenBuffersARB)wglGetProcAddress("glGenBuffersARB");
		glBufferDataARB = (gli_glBufferDataARB)wglGetProcAddress("glBufferDataARB");
		glMapBufferARB = (gli_glMapBufferARB)wglGetProcAddress("glMapBufferARB");
		glUnmapBufferARB = (gli_glUnmapBufferARB)wglGetProcAddress("glUnmapBufferARB");
		if(!glBindBufferARB || !glDeleteBuffersARB || !glGenBuffersARB ||
		   !glBufferDataARB || !glMapBufferARB || !glUnmapBufferARB)
			ferr("GL_ARB_vertex_buffer_object functions not found.");
	}

	atexit(OGL1Quit);
	InitGL(); ResizeGL();
}

void Reset()
{
	ResizeGL();
}

void BeginDrawing()
{
	uint c = mapfogcolor;
	glClearColor(((c>>16)&255)/255.0f, ((c>>8)&255)/255.0f, (c&255)/255.0f, ((c>>24)&255)/255.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void EndDrawing()
{
	SwapBuffers(whdc); drawframes++;
	//glFinish();
}

texture CreateTexture(Bitmap *bm, int mipmaps)
{
	Bitmap *c = bm;
	if(bm->form != BMFORMAT_B8G8R8A8)
		c = ConvertBitmapToB8G8R8A8(bm);

	GLuint gltex;
	glGenTextures(1, &gltex);
	glBindTexture(GL_TEXTURE_2D, gltex);
	if(mipmaps != 1)
	{
		gluBuild2DMipmaps(GL_TEXTURE_2D, 4, c->w, c->h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, c->pix);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	}
	else
	{
		glTexImage2D(GL_TEXTURE_2D, 0, 4, c->w, c->h, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, c->pix);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	if(bm->form != BMFORMAT_B8G8R8A8)
		FreeBitmap(c);
	return (texture)gltex;
}

void FreeTexture(texture t)
{
	GLuint gltex = (GLuint)t;
	glDeleteTextures(1, &gltex);
}

void SetTransformMatrix(Matrix *m)
{
	glLoadMatrixf(m->v);
}

void SetTexture(uint x, texture t)
{
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, (GLuint)t);
}

void NoTexture(uint x)
{
	glDisable(GL_TEXTURE_2D);
}

void SetFog()
{
	glEnable(GL_FOG);
	float fc[4]; int c = mapfogcolor;
	fc[0] = ((c>>16)&255)/255.0f;
	fc[1] = ((c>> 8)&255)/255.0f;
	fc[2] = ((c    )&255)/255.0f;
	fc[3] = ((c>>24)&255)/255.0f;
	//fc[0] = fc[1] = fc[2] = fc[3] = 0;
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_START, farzvalue/2);
	glFogf(GL_FOG_END, farzvalue-8);
	glFogfv(GL_FOG_COLOR, fc);
}

void DisableFog()
{
	glDisable(GL_FOG);
}

void EnableAlphaTest()
{
	glEnable(GL_ALPHA_TEST);
}

void DisableAlphaTest()
{
	glDisable(GL_ALPHA_TEST);
}

void EnableColorBlend()
{
	;
}

void DisableColorBlend()
{
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

void SetBlendColor(int c)
{
	glColor4ub((c>>16)&255, (c>>8)&255, c&255, (c>>24)&255);
}

void EnableAlphaBlend()
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void DisableAlphaBlend()
{
	glDisable(GL_BLEND);
}

void InitRectDrawing()
{
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	Matrix m; CreateZeroMatrix(&m);
	m._11 = 2.0f / scrw;
	m._22 = -2.0f / scrh;
	m._41 = -1;
	m._42 = 1;
	m._44 = 1;
	glLoadMatrixf(m.v);
}

void DrawRect(int x, int y, int w, int h, int c, float u, float v, float o, float p)
{
	glColor4ub((c>>16)&255, (c>>8)&255, c&255, (c>>24)&255);
	glBegin(GL_TRIANGLE_STRIP);
		glTexCoord2f(u  , v  ); glVertex3f(x  , y  , 0.5f);
		glTexCoord2f(u+o, v  ); glVertex3f(x+w, y  , 0.5f);
		glTexCoord2f(u  , v+p); glVertex3f(x  , y+h, 0.5f);
		glTexCoord2f(u+o, v+p); glVertex3f(x+w, y+h, 0.5f);
	glEnd();
}

void DrawGradientRect(int x, int y, int w, int h, int c0, int c1, int c2, int c3)
{
	glBegin(GL_TRIANGLE_STRIP);
		glColor4ub((c0>>16)&255, (c0>>8)&255, c0&255, (c0>>24)&255);
		glTexCoord2f(0, 0); glVertex3f(x  , y  , 0.5f);
		glColor4ub((c1>>16)&255, (c1>>8)&255, c1&255, (c1>>24)&255);
		glTexCoord2f(1, 0); glVertex3f(x+w, y  , 0.5f);
		glColor4ub((c2>>16)&255, (c2>>8)&255, c2&255, (c2>>24)&255);
		glTexCoord2f(0, 1); glVertex3f(x  , y+h, 0.5f);
		glColor4ub((c3>>16)&255, (c3>>8)&255, c3&255, (c3>>24)&255);
		glTexCoord2f(1, 1); glVertex3f(x+w, y+h, 0.5f);
	glEnd();
}

void DrawFrame(int x, int y, int w, int h, int c)
{
	glColor4ub((c>>16)&255, (c>>8)&255, c&255, (c>>24)&255);
	glBegin(GL_LINE_STRIP);
		glVertex3f(x  , y  , 0.5f);
		glVertex3f(x+w, y  , 0.5f);
		glVertex3f(x+w, y+h, 0.5f);
		glVertex3f(x  , y+h, 0.5f);
		glVertex3f(x  , y  , 0.5f);
	glEnd();
}

void CreateMapPart(MapPart *p, int x, int y, int w, int h)
{
	p->glverts = new float[(h+1)*(w+1)*5];
	float *vp = p->glverts;
	for(int b = 0; b < h+1; b++)
	for(int a = 0; a < w+1; a++)
	{
		*(vp++) = a+x;
		*(vp++) = himap[(b+y)*(mapwidth+1)+(a+x)];
		*(vp++) = b+y;
		*(vp++) = a+x; *(vp++) = b+y;
	}

	p->glindices = new GLushort[(2*(w+1)+1)*h];
	GLushort *os = p->glindices;
	for(int cy = 0; cy < h; cy++)
	{
		if(!(cy&1))
		{
			for(int cx = 0; cx < w+1; cx++)
			{
				*(os++) = cy*(w+1)+cx;
				*(os++) = (cy+1)*(w+1)+cx;
				if(cx == w) *(os++) = (cy+1)*(w+1)+cx;
			}
		} else {
			for(int cx = w+1-1; cx >= 0; cx--)
			{
				*(os++) = cy*(w+1)+cx;
				*(os++) = (cy+1)*(w+1)+cx;
				if(cx == 0) *(os++) = (cy+1)*(w+1)+cx;
			}
		}
	}
}

void FreeMapPart(MapPart *p)
{
	delete [] p->glverts; delete [] p->glindices;
}

void BeginMapDrawing()
{
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
}

void DrawPart(MapPart *p)
{
	int x = p->x, y = p->y, w = p->w, h = p->h;

	glVertexPointer(3, GL_FLOAT, 5*4, p->glverts);
	glTexCoordPointer(2, GL_FLOAT, 5*4, &(p->glverts[3]));
	glDrawElements(GL_TRIANGLE_STRIP, (2*(w+1)+1)*h, GL_UNSIGNED_SHORT, p->glindices);
}

void CreateMesh(Mesh *m)
{
	;
}

void BeginMeshDrawing()
{
	glEnable(GL_TEXTURE_2D);
	glAlphaFunc(GL_GEQUAL, 240.0f/255.0f);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); //GL_LINEAR_MIPMAP_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); //GL_LINEAR);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
}

void DrawMesh(Mesh *m, int iwtcolor)
{
	glVertexPointer(3, GL_FLOAT, 3*4, m->mverts.gb.memory);
	glTexCoordPointer(2, GL_FLOAT, 2*4, m->muvlist[iwtcolor]->gb.memory);
	for(int g = 0; g < m->ngrp; g++)
	{
		if(m->lstmatflags[g]) glEnable(GL_ALPHA_TEST);
		else glDisable(GL_ALPHA_TEST);
		glBindTexture(GL_TEXTURE_2D, (GLuint)m->lstmattex[g]);
		glDrawElements(GL_TRIANGLES, m->mstartix[g+1]-m->mstartix[g], GL_UNSIGNED_SHORT, ((short*)m->mindices.gb.memory) + m->mstartix[g]);
	}
}

RBatch *CreateBatch(int mv, int mi)
{
	//ferr("Batch unsupported in OpenGL driver.");
	if(rglUseBufferObjects)
	{
		RBatchOGL1_BO *b = new RBatchOGL1_BO;
		b->maxverts = mv; b->maxindis = mi;
		b->curverts = b->curindis = 0;
		glGenBuffersARB(1, &b->vbuf);
		glGenBuffersARB(1, &b->ibuf);
		//glBindBufferARB(GL_ARRAY_BUFFER_ARB, b->vbuf);
		//glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, b->ibuf);
		//glBufferDataARB(GL_ARRAY_BUFFER_ARB, mv * sizeof(batchVertex), NULL, GL_DYNAMIC_DRAW_ARB);
		//glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, mi * 2, NULL, GL_DYNAMIC_DRAW_ARB);
		//glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		//glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
		b->locked = 0;
		return b;
	}
	else
	{
		RBatchOGL1 *b = new RBatchOGL1;
		b->maxverts = mv; b->maxindis = mi;
		b->curverts = b->curindis = 0;
		b->vbuf = new batchVertex[mv];
		b->ibuf = new ushort[mi];
		return b;
	}
}

void BeginBatchDrawing()
{
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	//glColor4ub(255, 255, 255, 255);
}

RVertexBuffer *CreateVertexBuffer(int nv)
{
	RVertexBufferOGL1 *r = new RVertexBufferOGL1;
	r->size = nv;
	r->m = new batchVertex[nv];
	return r;
}

RIndexBuffer *CreateIndexBuffer(int ni)
{
	RIndexBufferOGL1 *r = new RIndexBufferOGL1;
	r->size = ni;
	r->m = new ushort[ni];
	return r;
}

void SetVertexBuffer(RVertexBuffer *_rv)
{
	curvtxmem = ((RVertexBufferOGL1*)_rv)->m;
}

void SetIndexBuffer(RIndexBuffer *_ri)
{
	curidxmem = ((RIndexBufferOGL1*)_ri)->m;
}

void DrawBuffer(int first, int count)
{
	glVertexPointer(3, GL_FLOAT, sizeof(batchVertex), &curvtxmem->x);
	glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(batchVertex), &curvtxmem->color);
	glTexCoordPointer(2, GL_FLOAT, sizeof(batchVertex), &curvtxmem->u);
	glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, curidxmem + first);
}

void EnableScissor()
{
	glEnable(GL_SCISSOR_TEST);
}

void DisableScissor()
{
	glDisable(GL_SCISSOR_TEST);
}

void SetScissorRect(int x, int y, int w, int h)
{
	glScissor(x, scrh-1-(y+h), w, h);
}

void InitImGuiDrawing()
{
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	Matrix m; CreateZeroMatrix(&m);
	m._11 = 2.0f / scrw;
	m._22 = -2.0f / scrh;
	m._41 = -1;
	m._42 = 1;
	m._44 = 1;
	glLoadMatrixf(m.v);
}

void BeginLakeDrawing()
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

int ConvertColor(int c)
{
	return BGRA_TO_RGBA(c);
}

void UpdateTexture(texture t, Bitmap *bmp)
{
	glBindTexture(GL_TEXTURE_2D, (GLuint)t);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, bmp->w, bmp->h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, bmp->pix);
}

void EnableDepth()
{
	glDepthMask(GL_TRUE);
}

void DisableDepth()
{
	glDepthMask(GL_FALSE);
}

};

IRenderer *CreateOGL1Renderer() {return new OGL1Renderer;}
