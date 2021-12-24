#pragma once

#ifdef _WIN32

#include "renderer.h"

#define WIN32_LEAN_AND_MEAN
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

struct D3D11Renderer : IRenderer {

	IDXGISwapChain* dxgiSwapChain = nullptr;
	ID3D11Device* ddDevice = nullptr;
	D3D_FEATURE_LEVEL featureLevel;
	ID3D11DeviceContext* ddImmediateContext = nullptr;

	ID3D11RenderTargetView* ddRenderTargetView = nullptr;

	ID3D11InputLayout* ddInputLayout;
	ID3D11VertexShader* ddVertexShader, * ddFogVertexShader;
	ID3D11PixelShader* ddPixelShader, * ddAlphaTestPixelShader, * ddMapPixelShader, * ddLakePixelShader;
	ID3D11GeometryShader* ddGeometryShader;

	// Shapes
	ID3D11Buffer* shapeVertexBuffer = nullptr;
	ID3D11BlendState* alphaBlendState = nullptr;

	// Depth
	ID3D11Texture2D* depthBuffer = nullptr;
	ID3D11DepthStencilView* depthView = nullptr;
	ID3D11DepthStencilState* depthOnState = nullptr;
	ID3D11DepthStencilState* depthOffState = nullptr;
	ID3D11DepthStencilState* depthTestOnlyState = nullptr;

	// Constant buffers
	ID3D11Buffer* transformBuffer = nullptr;
	ID3D11Buffer* fogBuffer = nullptr;

	ID3D11RasterizerState* scissorRasterizerState = nullptr;
	texture whiteTexture;
	int msaaNumSamples = 1;

	static ComPtr<ID3DBlob> compileShader(const char* shaderCode, size_t shaderSize, const char* func, const char* model);
	ID3D11VertexShader* getVertexShader(ID3DBlob* blob);
	ID3D11PixelShader* getPixelShader(ID3DBlob* blob);

	// Initialisation
	virtual void Init() override;

	virtual void Reset() override;

	// Frame begin/end

	virtual void BeginDrawing() override;

	virtual void EndDrawing() override;
	virtual void ClearFrame(bool clearColors = true, bool clearDepth = true, uint32_t color = 0) override;

	// Textures management
	virtual texture CreateTexture(Bitmap* bm, int mipmaps) override;
	virtual void FreeTexture(texture t) override;
	virtual void UpdateTexture(texture t, Bitmap* bmp) override;

	// State changes
	virtual void SetTransformMatrix(const Matrix* m) override;
	virtual void SetTexture(uint32_t x, texture t) override;
	virtual void NoTexture(uint32_t x) override;
	virtual void SetFog(uint32_t color = 0, float farz = 250.0f) override;
	virtual void DisableFog() override;
	virtual void EnableAlphaTest() override;
	virtual void DisableAlphaTest() override;
	virtual void EnableColorBlend() override;
	virtual void DisableColorBlend() override;
	virtual void SetBlendColor(int c) override;
	virtual void EnableAlphaBlend() override;
	virtual void DisableAlphaBlend() override;
	virtual void EnableScissor() override;
	virtual void DisableScissor() override;
	virtual void SetScissorRect(int x, int y, int w, int h) override;
	virtual void EnableDepth() override;
	virtual void DisableDepth() override;

	// 2D Rectangles drawing

	virtual void InitRectDrawing() override;

	virtual void DrawRect(int x, int y, int w, int h, int c = -1, float u = 0.0f, float v = 0.0f, float o = 1.0f, float p = 1.0f) override;

	virtual void DrawGradientRect(int x, int y, int w, int h, int c0, int c1, int c2, int c3) override;

	virtual void DrawFrame(int x, int y, int w, int h, int c) override;

	// 3D Landscape/Heightmap drawing
	virtual void BeginMapDrawing() override;
	virtual void BeginLakeDrawing() override;

	// 3D Mesh drawing
	virtual void BeginMeshDrawing();

	// Batch drawing
	virtual RBatch* CreateBatch(int mv, int mi) override;
	virtual void BeginBatchDrawing() override;
	virtual int ConvertColor(int c) override;

	// Buffer drawing
	virtual RVertexBuffer* CreateVertexBuffer(int nv) override;
	virtual RIndexBuffer* CreateIndexBuffer(int ni) override;
	virtual void SetVertexBuffer(RVertexBuffer* _rv) override;
	virtual void SetIndexBuffer(RIndexBuffer* _ri) override;
	virtual void DrawBuffer(int first, int count) override;

	// ImGui
	virtual void InitImGuiDrawing() override;

	virtual void BeginParticles() override;;

	virtual void SetLineTopology() override;

	virtual void SetTriangleTopology() override;
};

#endif // _WIN32
