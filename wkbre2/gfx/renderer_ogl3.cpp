#include "renderer.h"
#include "../window.h"
#include "bitmap.h"
#include "../util/util.h"
#include <cassert>
#include <string>

#include <GL/glew.h>
#ifdef _WIN32
#include <GL/wglew.h>
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_system.h>
extern SDL_Window* g_sdlWindow;

static const char* glsl_common = R"---(
#version 150
uniform FogBuffer {
	vec4 FogColor;
	float FogStartDist;
	float FogEndDist;
};
)---";

static const char* glsl_vertex = R"---(
in vec3 inPosition;
in vec4 inColor;
in vec2 inTexCoords;
out vec4 vtxColor;
out vec2 vtxTexCoords;
out float vtxFog;
uniform Transform {
	mat4 transformMatrix;
};
void main() {
	gl_Position = vec4(inPosition, 1) * transformMatrix;
	vtxColor = inColor.bgra;
	vtxTexCoords = inTexCoords;
	vtxFog = clamp((gl_Position.w-FogStartDist) / (FogEndDist-FogStartDist), 0, 1);
}
)---";

static const char* glsl_fragment = R"---(
in vec4 vtxColor;
in vec2 vtxTexCoords;
in float vtxFog;
out vec4 FragColor;
uniform sampler2D texSampler;
void main() {
	vec4 texColor = texture(texSampler, vtxTexCoords);
	FragColor = mix(vtxColor * texColor, FogColor, vtxFog);
}
)---";

static const char* glsl_fragment_alphaTest = R"---(
const float ALPHA_REF = 0.94;
in vec4 vtxColor;
in vec2 vtxTexCoords;
in float vtxFog;
out vec4 FragColor;
uniform sampler2D texSampler;
void main() {
	vec4 texColor = texture(texSampler, vtxTexCoords);
	if(texColor.a < ALPHA_REF)
		discard;
	FragColor = mix(vtxColor * texColor, FogColor, vtxFog);
}
)---";

static GLenum g_glCurrentTopology = GL_TRIANGLES;

struct RVertexBufferOGL3 : public RVertexBuffer
{
	GLuint buffer;
	GLuint vao;
	~RVertexBufferOGL3() { glDeleteBuffers(1, &buffer); glDeleteVertexArrays(1, &vao); }
	batchVertex* lock()
	{
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
		void* ptr = glMapBufferRange(GL_ARRAY_BUFFER, 0, size*sizeof(batchVertex), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
		return (batchVertex*)ptr;
	}
	void unlock()
	{
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
		glUnmapBuffer(GL_ARRAY_BUFFER);
	}
};

struct RIndexBufferOGL3 : public RIndexBuffer
{
	GLuint buffer;
	~RIndexBufferOGL3() { glDeleteBuffers(1, &buffer); }
	uint16_t* lock()
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
		void* ptr = glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, size * 2, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
		return (uint16_t*)ptr;
	}
	void unlock()
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
		glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
	}
};


struct RBatchOGL3 : public RBatch {
	GLuint vtxBuffer;
	GLuint indBuffer;
	GLuint vao;

	batchVertex* vlock;
	uint16_t* ilock;
	bool locked = false;

	~RBatchOGL3() {
		if (locked) unlock();
		glDeleteBuffers(1, &vtxBuffer);
		glDeleteBuffers(1, &indBuffer);
		glDeleteVertexArrays(1, &vao);
	}

	void lock() {
		glBindBuffer(GL_ARRAY_BUFFER, vtxBuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indBuffer);
		vlock = (batchVertex*)glMapBufferRange(GL_ARRAY_BUFFER, 0, maxverts * sizeof(batchVertex), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
		ilock = (uint16_t*)glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, maxindis * 2, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
		locked = true;
	}

	void unlock() {
		glBindBuffer(GL_ARRAY_BUFFER, vtxBuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indBuffer);
		glUnmapBuffer(GL_ARRAY_BUFFER);
		glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
		locked = false;
	}

	void begin() {}
	void end() {}

	void next(uint32_t nverts, uint32_t nindis, batchVertex** vpnt, uint16_t** ipnt, uint32_t* fi)
	{
		if (nverts > maxverts) ferr("Too many vertices to fit in the batch.");
		if (nindis > maxindis) ferr("Too many indices to fit in the batch.");

		if ((curverts + nverts > maxverts) || (curindis + nindis > maxindis))
			flush();

		if (!locked) lock();
		*vpnt = vlock + curverts;
		*ipnt = ilock + curindis;
		*fi = curverts;

		curverts += nverts; curindis += nindis;
	}

	void flush()
	{
		if (locked) unlock();
		if (!curverts) return;
		glBindVertexArray(vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indBuffer);
		glDrawElements(g_glCurrentTopology, curindis, GL_UNSIGNED_SHORT, (void*)0);
		glBindVertexArray(0);
		curverts = curindis = 0;
	}
};


struct OGL3Renderer : IRenderer {
#ifdef _WIN32
	HGLRC glContext;
	HDC windowHdc;
#endif

	GLuint programDefault, programAlphaTest;

	GLuint ub_transformMatrix;
	//GLuint ub_texture;
	GLuint ub_FogBuffer;

	GLuint defaultTexture;

	GLuint shapeBuffer;
	GLuint shapeVAO;

	static GLuint createVAO(GLuint arrayBuffer, GLuint elementArrayBuffer = 0) {
		GLuint vao;
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, arrayBuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementArrayBuffer);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(batchVertex), (void*)0);
		glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(batchVertex), (void*)12);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(batchVertex), (void*)16);
		glBindVertexArray(0); // prevent accidental changes to VAO
		return vao;
	}

	virtual void Init() override {
	#ifdef _WIN32
		SDL_SysWMinfo syswm;
		SDL_VERSION(&syswm.version);
		SDL_GetWindowWMInfo(g_sdlWindow, &syswm);
		HWND hWindow = syswm.info.win.window;
		windowHdc = GetDC(hWindow);

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
		int i = ChoosePixelFormat(windowHdc, &pfd);
		SetPixelFormat(windowHdc, i, &pfd);

		// Context creation
		HGLRC tempCtx = wglCreateContext(windowHdc);
		wglMakeCurrent(windowHdc, tempCtx);
		glewInit();
		static const int ctxAttribs[] = {
			WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
			WGL_CONTEXT_MINOR_VERSION_ARB, 3,
			WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
			0
		};
		glContext = wglCreateContextAttribsARB(windowHdc, 0, ctxAttribs);
		wglMakeCurrent(0, 0);
		wglDeleteContext(tempCtx);
		wglMakeCurrent(windowHdc, glContext);

		// VSync
		wglSwapIntervalEXT(1);
	#else
		SDL_GL_CreateContext(g_sdlWindow);
		glewInit();
	#endif
	// for Linux assume a context has already been created and it set as current (by SDL)

		// Uniform buffers
		glGenBuffers(1, &ub_transformMatrix);
		glBindBuffer(GL_UNIFORM_BUFFER, ub_transformMatrix);
		glBufferData(GL_UNIFORM_BUFFER, 64, nullptr, GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, 1, ub_transformMatrix);
		float fogdef[6] = { 0,0,0,0,0.0f,-1.0f };
		glGenBuffers(1, &ub_FogBuffer);
		glBindBuffer(GL_UNIFORM_BUFFER, ub_FogBuffer);
		glBufferData(GL_UNIFORM_BUFFER, 32, fogdef, GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, 2, ub_FogBuffer);


		// Shaders

		auto compileShader = [](GLuint shaderType, const char* src) {
			GLuint shader = glCreateShader(shaderType);
			const char* cat[2] = { glsl_common, src };
			glShaderSource(shader, 2, cat, nullptr);
			glCompileShader(shader);
			GLint stat = 0;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &stat);
			if (stat == GL_FALSE) {
				std::string log(512, '\0');
				GLsizei len;
				glGetShaderInfoLog(shader, std::size(log), &len, (char*)log.data());
				log.resize(len);
			#ifdef _WIN32
				MessageBoxA(NULL, log.c_str(), "GLSL compilation error", 16);
			#endif
			}
			return shader;
		};

		GLuint psShader = compileShader(GL_FRAGMENT_SHADER, glsl_fragment);
		GLuint psShaderAlphaTest = compileShader(GL_FRAGMENT_SHADER, glsl_fragment_alphaTest);
		GLuint vsShader = compileShader(GL_VERTEX_SHADER, glsl_vertex);

		GLuint prog = glCreateProgram();
		glBindAttribLocation(prog, 0, "inPosition");
		glBindAttribLocation(prog, 1, "inColor");
		glBindAttribLocation(prog, 2, "inTexCoords");
		glAttachShader(prog, psShader);
		glAttachShader(prog, vsShader);
		glLinkProgram(prog);
		glUniformBlockBinding(prog, glGetUniformBlockIndex(prog, "Transform"), 1);
		glUniformBlockBinding(prog, glGetUniformBlockIndex(prog, "FogBuffer"), 2);
		glUseProgram(prog);
		glUniform1i(glGetUniformLocation(prog, "texSampler"), (GLint)0);
		programDefault = prog;

		prog = glCreateProgram();
		glBindAttribLocation(prog, 0, "inPosition");
		glBindAttribLocation(prog, 1, "inColor");
		glBindAttribLocation(prog, 2, "inTexCoords");
		glAttachShader(prog, psShaderAlphaTest);
		glAttachShader(prog, vsShader);
		glLinkProgram(prog);
		glUniformBlockBinding(prog, glGetUniformBlockIndex(prog, "Transform"), 1);
		glUniformBlockBinding(prog, glGetUniformBlockIndex(prog, "FogBuffer"), 2);
		glUseProgram(prog);
		glUniform1i(glGetUniformLocation(prog, "texSampler"), (GLint)0);
		programAlphaTest = prog;

		glUseProgram(programDefault);
		//printf("shaders compiled\n");

		// Shape vertex buffer
		glGenBuffers(1, &shapeBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, shapeBuffer);
		glBufferData(GL_ARRAY_BUFFER, 64 * sizeof(batchVertex), nullptr, GL_DYNAMIC_DRAW);
		shapeVAO = createVAO(shapeBuffer);

		// Default texture
		glGenTextures(1, &defaultTexture);
		glBindTexture(GL_TEXTURE_2D, defaultTexture);
		uint32_t white = 0xFFFFFFFF;
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white);

		// Default state
		glFrontFace(GL_CW);
	}

	virtual void Reset() override {}

	virtual void BeginDrawing() override {
		glViewport(0, 0, g_windowWidth, g_windowHeight);
	}

	virtual void EndDrawing() override {
	#ifdef _WIN32
		SwapBuffers(windowHdc);
	#else
		SDL_GL_SwapWindow(g_sdlWindow);
	#endif
	}

	virtual void ClearFrame(bool clearColors = true, bool clearDepth = true, uint32_t color = 0) override {
		GLbitfield glbf = 0;
		if (clearColors) {
			float cc[4];
			cc[0] = (float)((color >> 16) & 255) / 255.0f;
			cc[1] = (float)((color >> 8) & 255) / 255.0f;
			cc[2] = (float)((color >> 0) & 255) / 255.0f;
			cc[3] = (float)((color >> 24) & 255) / 255.0f;
			glClearColor(cc[0], cc[1], cc[2], cc[3]);
			glbf |= GL_COLOR_BUFFER_BIT;
		}
		if (clearDepth) {
			glClearDepth(1.0f);
			glbf |= GL_DEPTH_BUFFER_BIT;
		}
		if (glbf != 0)
			glClear(glbf);
	}

	virtual texture CreateTexture(Bitmap* bm, int mipmaps) override
	{
		Bitmap* c = bm;
		if (bm->format != BMFORMAT_R8G8B8A8)
			c = ConvertBitmapToR8G8B8A8(bm);

		GLuint gltex;
		glGenTextures(1, &gltex);
		glBindTexture(GL_TEXTURE_2D, gltex);
		if (mipmaps != 1)
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, c->width, c->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, c->pixels);
			glGenerateMipmap(GL_TEXTURE_2D);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_NEAREST);
		}
		else
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, c->width, c->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, c->pixels);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}

		if (bm->format != BMFORMAT_R8G8B8A8)
			FreeBitmap(c);
		return (texture)gltex;
	}
	
	virtual void FreeTexture(texture t) override
	{
		GLuint gltex = (GLuint)(size_t)t;
		glDeleteTextures(1, &gltex);
	}
	virtual void UpdateTexture(texture t, Bitmap* bmp) override {}

	virtual void SetTransformMatrix(const Matrix* m) override {
		// TODO: Use glMapBufferRange, but it doesn't work, and I have no idea why...
		//glBindBuffer(GL_UNIFORM_BUFFER, ub_transformMatrix);
		//Matrix* mapped = (Matrix*)glMapBufferRange(GL_UNIFORM_BUFFER, 0, 64, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
		//Matrix* mapped = (Matrix*)glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY);
		//assert(mapped);
		//*mapped = m->getTranspose();
		//GLboolean unmapResult = glUnmapBuffer(GL_UNIFORM_BUFFER);
		//assert(unmapResult == GL_TRUE);
		Matrix t = m->getTranspose();
		glBindBuffer(GL_UNIFORM_BUFFER, ub_transformMatrix);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, 64, &t);
	}

	virtual void SetTexture(uint32_t x, texture t) override {
		glActiveTexture(GL_TEXTURE0 + x);
		glBindTexture(GL_TEXTURE_2D, t ? (GLuint)(size_t)t : defaultTexture);
	}

	virtual void NoTexture(uint32_t x) override {
		glActiveTexture(GL_TEXTURE0 + x);
		glBindTexture(GL_TEXTURE_2D, defaultTexture);
	}
	virtual void SetFog(uint32_t color = 0, float farz = 250.0f) override {
		glBindBuffer(GL_UNIFORM_BUFFER, ub_FogBuffer);
		//float* cc = (float*)glMapBufferRange(GL_UNIFORM_BUFFER, 0, 32, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
		float cc[8];
		cc[0] = (float)((color >> 16) & 255) / 255.0f;
		cc[1] = (float)((color >> 8) & 255) / 255.0f;
		cc[2] = (float)((color >> 0) & 255) / 255.0f;
		cc[3] = (float)((color >> 24) & 255) / 255.0f;
		cc[4] = farz * 0.5f;
		cc[5] = farz;
		//glUnmapBuffer(GL_UNIFORM_BUFFER);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, 32, cc);
	}
	virtual void DisableFog() override {
		glBindBuffer(GL_UNIFORM_BUFFER, ub_FogBuffer);
		//float* cc = (float*)glMapBufferRange(GL_UNIFORM_BUFFER, 0, 32, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
		float cc[8];
		cc[0] = cc[1] = cc[2] = cc[3] = 0.0f;
		cc[4] = 0.0f;
		cc[5] = -1.0f;
		//glUnmapBuffer(GL_UNIFORM_BUFFER);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, 32, cc);
	}
	virtual void EnableAlphaTest() override {
		glUseProgram(programAlphaTest);

	}
	virtual void DisableAlphaTest() override {
		glUseProgram(programDefault);
	}
	virtual void EnableColorBlend() override {}
	virtual void DisableColorBlend() override {}
	virtual void SetBlendColor(int c) override {}
	virtual void EnableAlphaBlend() override {}
	virtual void DisableAlphaBlend() override {}
	virtual void EnableScissor() override {
		glEnable(GL_SCISSOR_TEST);
	}
	virtual void DisableScissor() override {
		glDisable(GL_SCISSOR_TEST);
	}
	virtual void SetScissorRect(int x, int y, int w, int h) override {
		glScissor(x, g_windowHeight - y - h, w, h);
	}
	virtual void EnableDepth() override {}
	virtual void DisableDepth() override {}

	virtual void InitRectDrawing() override {
		Matrix m = Matrix::getZeroMatrix();
		m._11 = 2.0f / g_windowWidth;
		m._22 = -2.0f / g_windowHeight;
		m._41 = -1;
		m._42 = 1;
		m._44 = 1;
		SetTransformMatrix(&m);
		//ddImmediateContext->OMSetBlendState(alphaBlendState, {}, 0xFFFFFFFF);
		//ddImmediateContext->OMSetDepthStencilState(depthOffState, 0);
		//ddImmediateContext->PSSetShader(ddPixelShader, nullptr, 0);
		//ddImmediateContext->VSSetShader(ddVertexShader, nullptr, 0);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glDisable(GL_CULL_FACE);
		glUseProgram(programDefault);
	}

	virtual void DrawRect(int x, int y, int w, int h, int c = -1, float u = 0.0f, float v = 0.0f, float o = 1.0f, float p = 1.0f) override {
	}
	virtual void DrawGradientRect(int x, int y, int w, int h, int c0, int c1, int c2, int c3) override {
	}
	virtual void DrawFrame(int x, int y, int w, int h, int c) override {
		glBindBuffer(GL_ARRAY_BUFFER, shapeBuffer);
		batchVertex* verts = (batchVertex*)glMapBufferRange(GL_ARRAY_BUFFER, 0, 5*sizeof(batchVertex), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
		assert(verts);
		verts[0].x = x; verts[0].y = y; verts[0].z = 0.0f; verts[0].color = c; verts[0].u = 0.0f; verts[0].v = 0.0f;
		verts[1].x = x + w; verts[1].y = y; verts[1].z = 0.0f; verts[1].color = c; verts[1].u = 0.0f; verts[1].v = 0.0f;
		verts[2].x = x + w; verts[2].y = y + h; verts[2].z = 0.0f; verts[2].color = c; verts[2].u = 0.0f; verts[2].v = 0.0f;
		verts[3].x = x; verts[3].y = y + h; verts[3].z = 0.0f; verts[3].color = c; verts[3].u = 0.0f; verts[3].v = 0.0f;
		verts[4].x = x; verts[4].y = y; verts[4].z = 0.0f; verts[4].color = c; verts[4].u = 0.0f; verts[4].v = 0.0f;
		glUnmapBuffer(GL_ARRAY_BUFFER);
		glBindVertexArray(shapeVAO);
		glDrawArrays(GL_LINE_STRIP, 0, 5);

	}

	// 3D Landscape/Heightmap drawing
	virtual void BeginMapDrawing() override {
		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glEnable(GL_CULL_FACE);
		glUseProgram(programDefault);
	}
	virtual void BeginLakeDrawing() override {
		glEnable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glEnable(GL_CULL_FACE);
		glUseProgram(programDefault);
	}

	// 3D Mesh drawing
	virtual void BeginMeshDrawing() {
		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glEnable(GL_CULL_FACE);
		glUseProgram(programDefault);
	}

	// Batch drawing
	virtual RBatch* CreateBatch(int mv, int mi) override {
		RBatchOGL3* batch = new RBatchOGL3;
		batch->maxverts = mv; batch->maxindis = mi;
		batch->curverts = batch->curindis = 0;

		glBindVertexArray(0);

		glGenBuffers(1, &batch->vtxBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, batch->vtxBuffer);
		glBufferData(GL_ARRAY_BUFFER, mv * sizeof(batchVertex), nullptr, GL_DYNAMIC_DRAW);

		glGenBuffers(1, &batch->indBuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, batch->indBuffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, mi * 2, nullptr, GL_DYNAMIC_DRAW);

		batch->vao = createVAO(batch->vtxBuffer, batch->indBuffer);
		batch->locked = false;

		return batch;
	}
	virtual void BeginBatchDrawing() override {
	}
	virtual int ConvertColor(int c) override { return c; }

	virtual RVertexBuffer* CreateVertexBuffer(int nv) override {
		GLuint buf;
		glGenBuffers(1, &buf);
		glObjectLabel(GL_BUFFER, buf, -1, "Standard Vertex Buffer");
		glBindBuffer(GL_ARRAY_BUFFER, buf);
		glBufferData(GL_ARRAY_BUFFER, nv * sizeof(batchVertex), nullptr, GL_DYNAMIC_DRAW);
		GLuint vao = createVAO(buf);
		RVertexBufferOGL3* rvb = new RVertexBufferOGL3;
		rvb->size = nv;
		rvb->buffer = buf;
		rvb->vao = vao;
		return rvb;
	}

	virtual RIndexBuffer* CreateIndexBuffer(int ni) override {
		GLuint buf;
		glGenBuffers(1, &buf);
		glObjectLabel(GL_BUFFER, buf, -1, "Standard Index Buffer");
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, ni * 2, nullptr, GL_DYNAMIC_DRAW);
		RIndexBufferOGL3* rib = new RIndexBufferOGL3;
		rib->size = ni;
		rib->buffer = buf;
		return rib;
	}
	virtual void SetVertexBuffer(RVertexBuffer* _rv) override {
		RVertexBufferOGL3* rv = (RVertexBufferOGL3*)_rv;
		glBindVertexArray(rv->vao);
		glBindBuffer(GL_ARRAY_BUFFER, rv->buffer);
	}
	virtual void SetIndexBuffer(RIndexBuffer* _ri) override {
		RIndexBufferOGL3* ri = (RIndexBufferOGL3*)_ri;
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ri->buffer);
	}
	virtual void DrawBuffer(int first, int count) override {
		glDrawElements(g_glCurrentTopology, count, GL_UNSIGNED_SHORT, (void*)(first*2));
	}

	virtual void InitImGuiDrawing() override {
		InitRectDrawing();
		//ddImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	virtual void BeginParticles() override {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
	}
	virtual void SetLineTopology() override {
		g_glCurrentTopology = GL_LINES;
	}
	virtual void SetTriangleTopology() override {
		g_glCurrentTopology = GL_TRIANGLES;
	}
};

IRenderer* CreateOGL3Renderer() { return new OGL3Renderer; }