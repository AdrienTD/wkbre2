#ifdef _WIN32

#include "renderer.h"
#include "../window.h"
#include "../util/util.h"
#include "bitmap.h"
#include "../settings.h"

#include <array>
#include <cassert>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

#include <SDL.h>
#include <SDL_syswm.h>
#include <SDL_system.h>
extern SDL_Window* g_sdlWindow;

#include <nlohmann/json.hpp>

const char shaderCode[] = R"---(
Texture2D inpTexture : register(t0);
SamplerState inpSampler : register(s0);

cbuffer ConstantBuffer : register(b0)
{
	matrix Transform;
};

cbuffer FogBuffer : register(b1)
{
	float4 FogColor;
	float FogStartDist;
	float FogEndDist;
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float4 Color : COLOR0;
	float2 Texcoord : TEXCOORD0;
	float Fog : FOG;
};

static const float ALPHA_REF = 0.94;

[maxvertexcount(3)]
void GS( triangle VS_OUTPUT input[3], inout TriangleStream<VS_OUTPUT> TriStream )
{
	for(int i = 0; i < 3; i++)
		TriStream.Append(input[i]);
	TriStream.RestartStrip();
}

VS_OUTPUT VS(float4 Pos : POSITION, float4 Color : COLOR, float2 Texcoord : TEXCOORD)
{
	VS_OUTPUT output = (VS_OUTPUT)0;
	output.Pos = mul(Pos, Transform);
	output.Color = Color.bgra;
	output.Texcoord = Texcoord;
	output.Fog = 0.0;
	return output;
};

VS_OUTPUT VS_Fog(float4 Pos : POSITION, float4 Color : COLOR, float2 Texcoord : TEXCOORD)
{
	VS_OUTPUT output = (VS_OUTPUT)0;
	output.Pos = mul(Pos, Transform);
	output.Color = Color.bgra;
	output.Texcoord = Texcoord;
	output.Fog = clamp((output.Pos.w-FogStartDist) / (FogEndDist-FogStartDist), 0, 1);
	return output;
};

float4 PS(VS_OUTPUT input) : SV_Target
{
	float4 tex = inpTexture.Sample(inpSampler, input.Texcoord);
	return lerp(input.Color * tex, FogColor, input.Fog);
};

float4 PS_AlphaTest(VS_OUTPUT input) : SV_Target
{
	float4 tex = inpTexture.Sample(inpSampler, input.Texcoord);
	if(tex.a < ALPHA_REF) discard;
	return lerp(input.Color * tex, FogColor, input.Fog);
};

float4 PS_Map(VS_OUTPUT input) : SV_Target
{
	// Compute the length of a half pixel in the current mipmap
	float lod = inpTexture.CalculateLevelOfDetail(inpSampler, input.Texcoord);
	float ha = 0.5 / (256 >> (int)ceil(lod));

	float2 stc = clamp(input.Texcoord, float2(ha,ha), float2(1.0-ha,1.0-ha));
	float2 tile = floor(stc / float2(0.25,0.25)); // tile index in the 4x4 atlas
	float2 subuv = fmod(stc, float2(0.25,0.25)); // coords inside the used tile

	float2 fuv = clamp(subuv, float2(ha,ha), float2(0.25-ha,0.25-ha)) + tile*0.25;
	float4 tex = inpTexture.Sample(inpSampler, fuv);
	return lerp(input.Color * tex, FogColor, input.Fog);
}

float4 PS_Lake(VS_OUTPUT input) : SV_Target
{
	return lerp(input.Color, FogColor, input.Fog);
};
)---";

struct RVertexBufferD3D11 : public RVertexBuffer
{
	ID3D11Buffer* buf;
	ID3D11DeviceContext* ddImmediateContext;
	~RVertexBufferD3D11() { buf->Release(); }
	batchVertex* lock()
	{
		D3D11_MAPPED_SUBRESOURCE ms;
		ddImmediateContext->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
		return (batchVertex*)ms.pData;
	}
	void unlock()
	{
		ddImmediateContext->Unmap(buf, 0);
	}
};

struct RIndexBufferD3D11 : public RIndexBuffer
{
	ID3D11Buffer* buf;
	ID3D11DeviceContext* ddImmediateContext;
	~RIndexBufferD3D11() { buf->Release(); }
	uint16_t* lock()
	{
		D3D11_MAPPED_SUBRESOURCE ms;
		ddImmediateContext->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
		return (uint16_t*)ms.pData;
	}
	void unlock()
	{
		ddImmediateContext->Unmap(buf, 0);
	}
};

struct RBatchD3D11 : public RBatch
{
	ID3D11Buffer* vbuf;
	ID3D11Buffer* ibuf;
	batchVertex* vlock;
	uint16_t* ilock;
	bool locked;
	ID3D11DeviceContext* ddImmediateContext;

	~RBatchD3D11()
	{
		if (locked) unlock();
		vbuf->Release();
		ibuf->Release();
	}

	void lock()
	{
		D3D11_MAPPED_SUBRESOURCE ms;
		ddImmediateContext->Map(vbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
		vlock = (batchVertex*)ms.pData;
		ddImmediateContext->Map(ibuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
		ilock = (uint16_t*)ms.pData;
		locked = true;
	}
	void unlock()
	{
		ddImmediateContext->Unmap(vbuf, 0);
		ddImmediateContext->Unmap(ibuf, 0);
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
		UINT stride = sizeof(batchVertex); UINT offset = 0;
		ddImmediateContext->IASetVertexBuffers(0, 1, &vbuf, &stride, &offset);
		ddImmediateContext->IASetIndexBuffer(ibuf, DXGI_FORMAT_R16_UINT, 0);
		ddImmediateContext->DrawIndexed(curindis, 0, 0);
		curverts = curindis = 0;
	}

};

struct D3D11Renderer : IRenderer {
	IDXGISwapChain* dxgiSwapChain = nullptr;
	ID3D11Device* ddDevice = nullptr;
	D3D_FEATURE_LEVEL featureLevel;
	ID3D11DeviceContext* ddImmediateContext = nullptr;

	ID3D11RenderTargetView* ddRenderTargetView = nullptr;

	ID3D11InputLayout* ddInputLayout;
	ID3D11VertexShader* ddVertexShader, *ddFogVertexShader;
	ID3D11PixelShader* ddPixelShader, *ddAlphaTestPixelShader, *ddMapPixelShader, *ddLakePixelShader;
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

	static ComPtr<ID3DBlob> compileShader(const char* func, const char* model) {
		ComPtr<ID3DBlob> blob, errorBlob;
		D3DCompile(shaderCode, sizeof(shaderCode), nullptr, nullptr, nullptr, func, model, D3DCOMPILE_ENABLE_STRICTNESS, 0, &blob, &errorBlob);
		if (errorBlob) {
			std::string err((const char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize());
			char title[80];
			sprintf_s(title, "Shader %s (%s) compilation failed!", func, model);
			MessageBoxA(nullptr, err.c_str(), title, 16);
			ferr("Failed to compile shader for %s (%s)", func, model);
		}
		return blob;
	};

	ID3D11VertexShader* getVertexShader(ID3DBlob* blob) {
		ID3D11VertexShader* shader;
		HRESULT hres = ddDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &shader);
		assert(!FAILED(hres));
		return shader;
	}
	ID3D11PixelShader* getPixelShader(ID3DBlob* blob) {
		ID3D11PixelShader* shader;
		HRESULT hres = ddDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &shader);
		assert(!FAILED(hres));
		return shader;
	}

	// Initialisation
	virtual void Init() override {
		SDL_SysWMinfo syswm;
		SDL_VERSION(&syswm.version);
		SDL_GetWindowWMInfo(g_sdlWindow, &syswm);
		HWND hWindow = syswm.info.win.window;

		msaaNumSamples = g_settings.value<int>("msaaNumSamples", 1);

		ComPtr<IDXGIFactory> dxgiFactory0;
		ComPtr<IDXGIAdapter> dxgiAdapter0;
		std::vector<ComPtr<IDXGIAdapter>> dxgiAdapters;
		HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), &dxgiFactory0);
		assert(!FAILED(hr));
		int adapterIndex = 0;
		while (dxgiFactory0->EnumAdapters(adapterIndex++, &dxgiAdapter0) != DXGI_ERROR_NOT_FOUND) {
			DXGI_ADAPTER_DESC desc;
			dxgiAdapter0->GetDesc(&desc);
			dxgiAdapters.push_back(std::move(dxgiAdapter0));
		}

		dxgiAdapter0.Reset();
		D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_HARDWARE;
		adapterIndex = g_settings.value<int>("d3d11AdapterIndex", -1);
		if (adapterIndex >= 0 && adapterIndex < dxgiAdapters.size()) {
			dxgiAdapter0 = dxgiAdapters[adapterIndex];
			driverType = D3D_DRIVER_TYPE_UNKNOWN;
		}
		
		D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_10_1 };
		DXGI_SWAP_CHAIN_DESC swapChainDesc;
		ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
		swapChainDesc.BufferCount = 1;
		swapChainDesc.BufferDesc.Width = g_windowWidth;
		swapChainDesc.BufferDesc.Height = g_windowHeight;
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
		swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.OutputWindow = hWindow;
		swapChainDesc.SampleDesc.Count = msaaNumSamples;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.Windowed = TRUE;
		HRESULT hres = D3D11CreateDeviceAndSwapChain(dxgiAdapter0.Get(), driverType, NULL, 0, featureLevels, std::size(featureLevels), D3D11_SDK_VERSION, &swapChainDesc, &dxgiSwapChain, &ddDevice, &featureLevel, &ddImmediateContext);
		assert(!FAILED(hres));

		ComPtr<ID3DBlob> vsBlob = compileShader("VS", "vs_4_0");
		ComPtr<ID3DBlob> vsFogBlob = compileShader("VS_Fog", "vs_4_0");
		ComPtr<ID3DBlob> psBlob = compileShader("PS", "ps_4_0");
		ComPtr<ID3DBlob> psAlphaTestBlob = compileShader("PS_AlphaTest", "ps_4_0");
		ComPtr<ID3DBlob> psMapBlob = compileShader("PS_Map", "ps_4_1");
		ComPtr<ID3DBlob> psLakeBlob = compileShader("PS_Lake", "ps_4_0");
		ComPtr<ID3DBlob> gsBlob = compileShader("GS", "gs_4_0");

		static const D3D11_INPUT_ELEMENT_DESC iadesc[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
		};
		hres = ddDevice->CreateInputLayout(iadesc, std::size(iadesc), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &ddInputLayout);
		assert(!FAILED(hres));
		hres = ddDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &ddVertexShader);
		assert(!FAILED(hres));
		hres = ddDevice->CreateVertexShader(vsFogBlob->GetBufferPointer(), vsFogBlob->GetBufferSize(), nullptr, &ddFogVertexShader);
		assert(!FAILED(hres));
		hres = ddDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &ddPixelShader);
		assert(!FAILED(hres));
		hres = ddDevice->CreatePixelShader(psAlphaTestBlob->GetBufferPointer(), psAlphaTestBlob->GetBufferSize(), nullptr, &ddAlphaTestPixelShader);
		assert(!FAILED(hres));
		hres = ddDevice->CreatePixelShader(psMapBlob->GetBufferPointer(), psMapBlob->GetBufferSize(), nullptr, &ddMapPixelShader);
		assert(!FAILED(hres));
		hres = ddDevice->CreatePixelShader(psLakeBlob->GetBufferPointer(), psLakeBlob->GetBufferSize(), nullptr, &ddLakePixelShader);
		assert(!FAILED(hres));
		hres = ddDevice->CreateGeometryShader(gsBlob->GetBufferPointer(), gsBlob->GetBufferSize(), nullptr, &ddGeometryShader);
		assert(!FAILED(hres));

		D3D11_BUFFER_DESC bd;
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		bd.MiscFlags = 0;
		bd.StructureByteStride = 0;

		bd.ByteWidth = 64 * sizeof(batchVertex);
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		ddDevice->CreateBuffer(&bd, nullptr, &shapeVertexBuffer);

		bd.ByteWidth = 64;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		ddDevice->CreateBuffer(&bd, nullptr, &transformBuffer);

		bd.ByteWidth = 32;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		ddDevice->CreateBuffer(&bd, nullptr, &fogBuffer);

		D3D11_BLEND_DESC blendDesc;
		blendDesc.AlphaToCoverageEnable = FALSE;
		blendDesc.IndependentBlendEnable = FALSE;
		blendDesc.RenderTarget[0].BlendEnable = TRUE;
		blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		hres = ddDevice->CreateBlendState(&blendDesc, &alphaBlendState);
		assert(!FAILED(hres));

		D3D11_RASTERIZER_DESC rastDesc = CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT());
		rastDesc.CullMode = D3D11_CULL_NONE;
		rastDesc.ScissorEnable = TRUE;
		hres = ddDevice->CreateRasterizerState(&rastDesc, &scissorRasterizerState);
		assert(!FAILED(hres));

		D3D11_DEPTH_STENCIL_DESC dsdesc = CD3D11_DEPTH_STENCIL_DESC(CD3D11_DEFAULT());
		dsdesc.DepthEnable = TRUE;
		dsdesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		dsdesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		hres = ddDevice->CreateDepthStencilState(&dsdesc, &depthOnState);
		assert(!FAILED(hres));
		dsdesc.DepthEnable = FALSE;
		dsdesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		dsdesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		hres = ddDevice->CreateDepthStencilState(&dsdesc, &depthOffState);
		assert(!FAILED(hres));
		dsdesc.DepthEnable = TRUE;
		dsdesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		dsdesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		hres = ddDevice->CreateDepthStencilState(&dsdesc, &depthTestOnlyState);
		assert(!FAILED(hres));

		Bitmap whiteBmp;
		whiteBmp.width = 1;
		whiteBmp.height = 1;
		whiteBmp.format = BMFORMAT_R8G8B8A8;
		whiteBmp.pixels = (uint8_t*)malloc(4);
		*(uint32_t*)whiteBmp.pixels = 0xFFFFFFFF;
		whiteBmp.palette = nullptr;
		whiteTexture = CreateTexture(&whiteBmp, 1);

		Reset();
	}

	virtual void Reset() override {
		ddImmediateContext->ClearState();
		if (depthView) depthView->Release();
		if (depthBuffer) depthBuffer->Release();
		if (ddRenderTargetView) ddRenderTargetView->Release();

		HRESULT hres = dxgiSwapChain->ResizeBuffers(1, g_windowWidth, g_windowHeight, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
		assert(!FAILED(hres));

		ID3D11Texture2D* backBuffer;
		hres = dxgiSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
		assert(backBuffer != nullptr);
		ddDevice->CreateRenderTargetView(backBuffer, NULL, &ddRenderTargetView);
		backBuffer->Release();

		D3D11_TEXTURE2D_DESC dsdesc;
		dsdesc.Width = g_windowWidth;
		dsdesc.Height = g_windowHeight;
		dsdesc.MipLevels = 1;
		dsdesc.ArraySize = 1;
		dsdesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsdesc.SampleDesc.Count = msaaNumSamples;
		dsdesc.SampleDesc.Quality = 0;
		dsdesc.Usage = D3D11_USAGE_DEFAULT;
		dsdesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		dsdesc.CPUAccessFlags = 0;
		dsdesc.MiscFlags = 0;
		hres = ddDevice->CreateTexture2D(&dsdesc, nullptr, &depthBuffer);
		assert(!FAILED(hres));

		D3D11_DEPTH_STENCIL_VIEW_DESC dsviewdesc;
		dsviewdesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsviewdesc.ViewDimension = (msaaNumSamples > 1) ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
		dsviewdesc.Flags = 0;
		dsviewdesc.Texture2D.MipSlice = 0;
		hres = ddDevice->CreateDepthStencilView(depthBuffer, &dsviewdesc, &depthView);
		assert(!FAILED(hres));
	}

	// Frame begin/end

	virtual void BeginDrawing() override {
		D3D11_VIEWPORT vp;
		vp.Width = g_windowWidth;
		vp.Height = g_windowHeight;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;

		ddImmediateContext->ClearState();
		ddImmediateContext->OMSetRenderTargets(1, &ddRenderTargetView, depthView);
		ddImmediateContext->RSSetViewports(1, &vp);
		ddImmediateContext->IASetInputLayout(ddInputLayout);
		ddImmediateContext->VSSetShader(ddVertexShader, nullptr, 0);
		ddImmediateContext->PSSetShader(ddPixelShader, nullptr, 0);
		ddImmediateContext->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&whiteTexture);
	}

	virtual void EndDrawing() override {
		dxgiSwapChain->Present(1, 0);
	}
	virtual void ClearFrame(bool clearColors = true, bool clearDepth = true, uint32_t color = 0) override {
		if (clearColors) {
			float cc[4];
			cc[0] = (float)((color >> 16) & 255) / 255.0f;
			cc[1] = (float)((color >> 8) & 255) / 255.0f;
			cc[2] = (float)((color >> 0) & 255) / 255.0f;
			cc[3] = (float)((color >> 24) & 255) / 255.0f;
			ddImmediateContext->ClearRenderTargetView(ddRenderTargetView, cc);
		}
		if (clearDepth)
			ddImmediateContext->ClearDepthStencilView(depthView, D3D11_CLEAR_DEPTH, 1.0f, 0);
	}

	// Textures management
	virtual texture CreateTexture(Bitmap* bm, int mipmaps) override {
		Bitmap* cvtbmp = ConvertBitmapToR8G8B8A8(bm);

		int mmwidth = cvtbmp->width,
			mmheight = cvtbmp->height,
			numMipmaps = 0;
		Bitmap* mmbmp[32];
		mmbmp[numMipmaps++] = cvtbmp;
		if (mipmaps <= 0) {
			mmwidth /= 2;
			mmheight /= 2;
			while (mmwidth && mmheight) {
				assert(numMipmaps < std::size(mmbmp));
				if (!mmwidth) mmwidth = 1;
				if (!mmheight) mmheight = 1;
				mmbmp[numMipmaps++] = ResizeBitmap(*cvtbmp, mmwidth, mmheight);
				mmwidth /= 2;
				mmheight /= 2;
				// no mipmaps of 8x8 or below (to prevent tiles from being merged in the terrain atlas!)
				if (mmwidth <= 8 || mmheight <= 8)
					break;
			}
		}

		D3D11_TEXTURE2D_DESC desc;
		desc.Width = cvtbmp->width;
		desc.Height = cvtbmp->height;
		desc.MipLevels = numMipmaps;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		D3D11_SUBRESOURCE_DATA sub[std::size(mmbmp)];
		for (size_t i = 0; i < numMipmaps; i++) {
			sub[i].pSysMem = mmbmp[i]->pixels;
			sub[i].SysMemPitch = mmbmp[i]->width * 4u;
			sub[i].SysMemSlicePitch = sub[i].SysMemPitch * mmbmp[i]->height;
		}
		ID3D11Texture2D* tex = nullptr;
		ddDevice->CreateTexture2D(&desc, sub, &tex);
		assert(tex);

		D3D11_SHADER_RESOURCE_VIEW_DESC rvd;
		rvd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		rvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		rvd.Texture2D.MostDetailedMip = 0;
		rvd.Texture2D.MipLevels = -1;
		ID3D11ShaderResourceView* srview = nullptr;
		ddDevice->CreateShaderResourceView(tex, &rvd, &srview);
		assert(srview);

		tex->Release();
		return (texture)srview;
	}
	virtual void FreeTexture(texture t) override {}
	virtual void UpdateTexture(texture t, Bitmap* bmp) override {}

	// State changes
	virtual void SetTransformMatrix(const Matrix* m) override {
		D3D11_MAPPED_SUBRESOURCE mappedRes;
		HRESULT hres = ddImmediateContext->Map(transformBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
		assert(!FAILED(hres));
		*(Matrix*)mappedRes.pData = m->getTranspose();
		ddImmediateContext->Unmap(transformBuffer, 0);
		ddImmediateContext->VSSetConstantBuffers(0, 1, &transformBuffer);
	}
	virtual void SetTexture(uint32_t x, texture t) override {
		if (t == nullptr)
			NoTexture(x);
		else
			ddImmediateContext->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&t);
	}
	virtual void NoTexture(uint32_t x) override {
		ddImmediateContext->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&whiteTexture);
	}
	virtual void SetFog(uint32_t color = 0, float farz = 250.0f) override {
		D3D11_MAPPED_SUBRESOURCE mappedRes;
		HRESULT hres = ddImmediateContext->Map(fogBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
		assert(!FAILED(hres));

		float* cc = (float*)mappedRes.pData;
		cc[0] = (float)((color >> 16) & 255) / 255.0f;
		cc[1] = (float)((color >> 8) & 255) / 255.0f;
		cc[2] = (float)((color >> 0) & 255) / 255.0f;
		cc[3] = (float)((color >> 24) & 255) / 255.0f;
		cc[4] = farz * 0.5f;
		cc[5] = farz;

		ddImmediateContext->Unmap(fogBuffer, 0);
		ddImmediateContext->VSSetConstantBuffers(1, 1, &fogBuffer);
		ddImmediateContext->PSSetConstantBuffers(1, 1, &fogBuffer);
		ddImmediateContext->VSSetShader(ddFogVertexShader, nullptr, 0);
	}
	virtual void DisableFog() override {
		ddImmediateContext->VSSetShader(ddVertexShader, nullptr, 0);
	}
	virtual void EnableAlphaTest() override {
		ddImmediateContext->PSSetShader(ddAlphaTestPixelShader, nullptr, 0);
	}
	virtual void DisableAlphaTest() override {
		ddImmediateContext->PSSetShader(ddPixelShader, nullptr, 0);
	}
	virtual void EnableColorBlend() override {}
	virtual void DisableColorBlend() override {}
	virtual void SetBlendColor(int c) override {}
	virtual void EnableAlphaBlend() override {}
	virtual void DisableAlphaBlend() override {}
	virtual void EnableScissor() override {
		ddImmediateContext->RSSetState(scissorRasterizerState);
	}
	virtual void DisableScissor() override {
		ddImmediateContext->RSSetState(nullptr);
	}
	virtual void SetScissorRect(int x, int y, int w, int h) override {
		D3D11_RECT rect;
		rect.left = x;
		rect.top = y;
		rect.right = x + w;
		rect.bottom = y + h;
		ddImmediateContext->RSSetScissorRects(1, &rect);
		ddImmediateContext->RSSetState(scissorRasterizerState);
	}
	virtual void EnableDepth() override {}
	virtual void DisableDepth() override {}

	// 2D Rectangles drawing

	virtual void InitRectDrawing() override {
		Matrix m = Matrix::getZeroMatrix();
		m._11 = 2.0f / g_windowWidth;
		m._22 = -2.0f / g_windowHeight;
		m._41 = -1;
		m._42 = 1;
		m._44 = 1;
		SetTransformMatrix(&m);
		ddImmediateContext->OMSetBlendState(alphaBlendState, {}, 0xFFFFFFFF);
		ddImmediateContext->OMSetDepthStencilState(depthOffState, 0);
		ddImmediateContext->PSSetShader(ddPixelShader, nullptr, 0);
		ddImmediateContext->VSSetShader(ddVertexShader, nullptr, 0);
	}

	virtual void DrawRect(int x, int y, int w, int h, int c = -1, float u = 0.0f, float v = 0.0f, float o = 1.0f, float p = 1.0f) override {
		D3D11_MAPPED_SUBRESOURCE ms;
		HRESULT hres;
		hres = ddImmediateContext->Map(shapeVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
		assert(!FAILED(hres));
		batchVertex* verts = (batchVertex*)ms.pData;
		verts[0].x = x;		verts[0].y = y;		verts[0].u = u;		verts[0].v = v;
		verts[1].x = x + w;	verts[1].y = y;		verts[1].u = u + o;	verts[1].v = v;
		verts[2].x = x;		verts[2].y = y + h;	verts[2].u = u;		verts[2].v = v + p;
		verts[3].x = x + w;	verts[3].y = y + h;	verts[3].u = u + o;	verts[3].v = v + p;
		for (int i = 0; i < 4; i++) { verts[i].color = c; verts[i].x -= 0.5f; verts[i].y -= 0.5f; }
		ddImmediateContext->Unmap(shapeVertexBuffer, 0);
		UINT stride = sizeof(batchVertex); UINT offset = 0;
		ddImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		ddImmediateContext->IASetVertexBuffers(0, 1, &shapeVertexBuffer, &stride, &offset);
		ddImmediateContext->Draw(4, 0);
	}

	virtual void DrawGradientRect(int x, int y, int w, int h, int c0, int c1, int c2, int c3) override {

	}

	virtual void DrawFrame(int x, int y, int w, int h, int c) override {
		D3D11_MAPPED_SUBRESOURCE ms;
		HRESULT hres;
		hres = ddImmediateContext->Map(shapeVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
		assert(!FAILED(hres));
		batchVertex* verts = (batchVertex*)ms.pData;
		verts[0].x = x; verts[0].y = y; verts[0].z = 0.0f; verts[0].color = c; verts[0].u = 0.0f; verts[0].v = 0.0f;
		verts[1].x = x + w; verts[1].y = y; verts[1].z = 0.0f; verts[1].color = c; verts[1].u = 0.0f; verts[1].v = 0.0f;
		verts[2].x = x + w; verts[2].y = y + h; verts[2].z = 0.0f; verts[2].color = c; verts[2].u = 0.0f; verts[2].v = 0.0f;
		verts[3].x = x; verts[3].y = y + h; verts[3].z = 0.0f; verts[3].color = c; verts[3].u = 0.0f; verts[3].v = 0.0f;
		verts[4].x = x; verts[4].y = y; verts[4].z = 0.0f; verts[4].color = c; verts[4].u = 0.0f; verts[4].v = 0.0f;
		ddImmediateContext->Unmap(shapeVertexBuffer, 0);
		UINT stride = sizeof(batchVertex); UINT offset = 0;
		ddImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
		ddImmediateContext->IASetVertexBuffers(0, 1, &shapeVertexBuffer, &stride, &offset);
		ddImmediateContext->Draw(5, 0);
	}

	// 3D Landscape/Heightmap drawing
	virtual void BeginMapDrawing() override {
		ddImmediateContext->PSSetShader(ddMapPixelShader, nullptr, 0);
		ddImmediateContext->OMSetBlendState(nullptr, {}, 0xFFFFFFFF);
	}
	virtual void BeginLakeDrawing() override {
		ddImmediateContext->PSSetShader(ddLakePixelShader, nullptr, 0);
		ddImmediateContext->OMSetBlendState(alphaBlendState, {}, 0xFFFFFFFF);
	}

	// 3D Mesh drawing
	virtual void BeginMeshDrawing() {
		ddImmediateContext->OMSetBlendState(nullptr, {}, 0xFFFFFFFF);
		ddImmediateContext->OMSetDepthStencilState(depthOnState, 0);
		ddImmediateContext->PSSetShader(ddAlphaTestPixelShader, nullptr, 0);
	}

	// Batch drawing
	virtual RBatch* CreateBatch(int mv, int mi) override {
		RBatchD3D11* batch = new RBatchD3D11;
		batch->maxverts = mv; batch->maxindis = mi;
		batch->curverts = batch->curindis = 0;

		D3D11_BUFFER_DESC bd;
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		bd.MiscFlags = 0;
		bd.StructureByteStride = 0;

		bd.ByteWidth = mv * sizeof(batchVertex);
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		ddDevice->CreateBuffer(&bd, nullptr, &batch->vbuf);

		bd.ByteWidth = mi * 2;
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		ddDevice->CreateBuffer(&bd, nullptr, &batch->ibuf);

		batch->locked = false;
		batch->ddImmediateContext = ddImmediateContext;
		return batch;
	}
	virtual void BeginBatchDrawing() override {
		ddImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}
	virtual int ConvertColor(int c) override { return c; }

	// Buffer drawing
	virtual RVertexBuffer* CreateVertexBuffer(int nv) override {
		RVertexBufferD3D11* vb11 = new RVertexBufferD3D11;
		D3D11_BUFFER_DESC bd;
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		bd.MiscFlags = 0;
		bd.StructureByteStride = 0;
		bd.ByteWidth = nv * sizeof(batchVertex);
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		ddDevice->CreateBuffer(&bd, nullptr, &vb11->buf);
		vb11->ddImmediateContext = ddImmediateContext;
		vb11->size = nv;
		return vb11;
	}
	virtual RIndexBuffer* CreateIndexBuffer(int ni) override {
		RIndexBufferD3D11* ib11 = new RIndexBufferD3D11;
		D3D11_BUFFER_DESC bd;
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		bd.MiscFlags = 0;
		bd.StructureByteStride = 0;
		bd.ByteWidth = ni * 2;
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		ddDevice->CreateBuffer(&bd, nullptr, &ib11->buf);
		ib11->ddImmediateContext = ddImmediateContext;
		ib11->size = ni;
		return ib11;
	}
	virtual void SetVertexBuffer(RVertexBuffer* _rv) override {
		RVertexBufferD3D11* rv = (RVertexBufferD3D11*)_rv;
		UINT stride = sizeof(batchVertex); UINT offset = 0;
		ddImmediateContext->IASetVertexBuffers(0, 1, &rv->buf, &stride, &offset);
	}
	virtual void SetIndexBuffer(RIndexBuffer* _ri) override {
		RIndexBufferD3D11* ri = (RIndexBufferD3D11*)_ri;
		ddImmediateContext->IASetIndexBuffer(ri->buf, DXGI_FORMAT_R16_UINT, 0);
	}
	virtual void DrawBuffer(int first, int count) override {
		ddImmediateContext->DrawIndexed(count, first, 0);
	}

	// ImGui
	virtual void InitImGuiDrawing() override {
		InitRectDrawing();
		ddImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	virtual void BeginParticles() override {
		ddImmediateContext->OMSetBlendState(alphaBlendState, {}, 0xFFFFFFFF);
		ddImmediateContext->OMSetDepthStencilState(depthTestOnlyState, 0);
		ddImmediateContext->PSSetShader(ddPixelShader, nullptr, 0);
		ddImmediateContext->VSSetShader(ddVertexShader, nullptr, 0);
	};

	virtual void SetLineTopology() override {
		ddImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	}

	virtual void SetTriangleTopology() override {
		ddImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}
};

IRenderer* CreateD3D11Renderer() { return new D3D11Renderer; }

#endif // _WIN32
