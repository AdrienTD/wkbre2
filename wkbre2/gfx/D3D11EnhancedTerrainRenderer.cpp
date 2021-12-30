// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#ifdef _WIN32

#include "D3D11EnhancedTerrainRenderer.h"
#include "../util/vecmat.h"
#include "renderer.h"
#include "../window.h"
#include "../terrain.h"
#include <utility>
#include <array>
#include "TextureCache.h"
#include "../Camera.h"
#include <cassert>
#include <algorithm>
#include "../util/util.h"
#include "bitmap.h"
#include "../settings.h"
#include <nlohmann/json.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "renderer_d3d11.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

struct D11NTRVtx {
	Vector3 pos;
	uint32_t normal, tangent, bitangent;
	float u, v;
};

static constexpr uint32_t Vec3ToR10G10B10A2(const Vector3& vec) {
	uint32_t res = (uint32_t)(vec.x * 1023.0f);
	res |= (uint32_t)(vec.y * 1023.0f) << 10;
	res |= (uint32_t)(vec.z * 1023.0f) << 20;
	return res;
}
static constexpr uint32_t NormalToR10G10B10A2(const Vector3& vec) {
	return Vec3ToR10G10B10A2((vec + Vector3(1, 1, 1)) * 0.5f);
}

const char tshaderCode[] = R"---(
Texture2D inpTexture : register(t0);
Texture2D normalTexture : register(t1);
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

cbuffer SunBuffer : register(b2)
{
	float3 SunDirection;
	float3 LampPos;
	int BumpOn;
	int LampOn;
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float3 FragPos : COLOR1;
	float3 Norm: NORMAL;
	float4 Color : COLOR0;
	float2 Texcoord : TEXCOORD0;
	float Fog : FOG;
	float3 Tangent : TANGENT0;
	float3 Bitangent : TANGENT1;
};

[maxvertexcount(3)]
void GS( triangle VS_OUTPUT input[3], inout TriangleStream<VS_OUTPUT> TriStream )
{
	for(int i = 0; i < 3; i++)
		TriStream.Append(input[i]);
	TriStream.RestartStrip();
}

VS_OUTPUT VS(float4 Pos : POSITION, float3 Normal : NORMAL, float3 Tangent : TANGENT, float3 Bitangent : BITANGENT, float2 Texcoord : TEXCOORD)
{
	VS_OUTPUT output = (VS_OUTPUT)0;
	output.Pos = mul(Pos, Transform);
	output.FragPos = Pos.xyz;
	output.Norm = Normal * 2 - float3(1,1,1);
	output.Color = float4(1,1,1,1);
	output.Texcoord = Texcoord;
	output.Fog = clamp((output.Pos.w-FogStartDist) / (FogEndDist-FogStartDist), 0, 1);
	output.Tangent = Tangent * 2 - float3(1,1,1);
	output.Bitangent = Bitangent * 2 - float3(1,1,1);
	return output;
};

float4 PS(VS_OUTPUT input) : SV_Target
{
	// Compute the length of a half pixel in the current mipmap
	float lod = inpTexture.CalculateLevelOfDetail(inpSampler, input.Texcoord);
	float ha = 0.5 / (256 >> (int)ceil(lod));

	float2 stc = clamp(input.Texcoord, float2(ha,ha), float2(1.0-ha,1.0-ha));
	float2 tile = floor(stc / float2(0.25,0.25)); // tile index in the 4x4 atlas
	float2 subuv = fmod(stc, float2(0.25,0.25)); // coords inside the used tile

	float2 fuv = clamp(subuv, float2(ha,ha), float2(0.25-ha,0.25-ha)) + tile*0.25;

	float4 tex = inpTexture.Sample(inpSampler, fuv);
	//float4 tex = float4(1,1,1,1);

	float lum = 0.2;

	float3 lampDir = LampPos - input.FragPos;
	const float lampRadius = 5;
	if(BumpOn) {
		float3 nrm = normalTexture.Sample(inpSampler, fuv).xyz * 2 - 1;

		float3 m1 = input.Tangent;
		float3 m2 = input.Norm;
		float3 m3 = input.Bitangent;
		float3 actnorm = nrm.xxx * m1 + nrm.yyy * m2 + nrm.zzz * m3;
		lum += clamp(dot(normalize(actnorm), SunDirection), 0, 1);
		lum = clamp(lum, 0, 1);

		float lampLum = clamp(dot(normalize(actnorm), normalize(lampDir)), 0, 1);
		float lampDist = length(lampDir);
		if(lampDist < lampRadius)
			lum += lampLum;
		//lum += lampLum * clamp(1 / (1 + 0.14*lampDist + 0.07*lampDist*lampDist), 0, 1);
	}
	else {
		lum += clamp(dot(input.Norm, SunDirection), 0, 1);
		lum = clamp(lum, 0, 1);

		float lampLum = clamp(dot(input.Norm, normalize(lampDir)), 0, 1);
		float lampDist = length(lampDir);
		if(lampDist < lampRadius)
			lum += lampLum;
	}
	//lum = clamp(lum, 0, 1);
	return lerp(float4(lum.xxx, 1) * input.Color * tex, FogColor, input.Fog);
}

// TODO
//float4 PS_Lake(VS_OUTPUT input) : SV_Target
//{
//	return noise(input.Texcoord);
//}
)---";

template<typename Vtx> struct RCustomBatchD3D11
{
	uint32_t maxverts, maxindis;
	uint32_t curverts, curindis;

	ID3D11Buffer* vbuf;
	ID3D11Buffer* ibuf;
	Vtx* vlock;
	uint16_t* ilock;
	bool locked;
	ID3D11DeviceContext* ddImmediateContext;

	RCustomBatchD3D11(int mv, int mi, ID3D11Device* ddDevice, ID3D11DeviceContext* ddic)
	{
		maxverts = mv; maxindis = mi;
		curverts = curindis = 0;

		D3D11_BUFFER_DESC bd;
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		bd.MiscFlags = 0;
		bd.StructureByteStride = 0;

		bd.ByteWidth = mv * sizeof(Vtx);
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		ddDevice->CreateBuffer(&bd, nullptr, &vbuf);

		bd.ByteWidth = mi * 2;
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		ddDevice->CreateBuffer(&bd, nullptr, &ibuf);

		locked = false;
		ddImmediateContext = ddic;
	}

	~RCustomBatchD3D11()
	{
		if (locked) unlock();
		vbuf->Release();
		ibuf->Release();
	}

	void lock()
	{
		D3D11_MAPPED_SUBRESOURCE ms;
		ddImmediateContext->Map(vbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
		vlock = (Vtx*)ms.pData;
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

	void next(uint32_t nverts, uint32_t nindis, Vtx** vpnt, uint16_t** ipnt, uint32_t* fi)
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
		UINT stride = sizeof(Vtx); UINT offset = 0;
		ddImmediateContext->IASetVertexBuffers(0, 1, &vbuf, &stride, &offset);
		ddImmediateContext->IASetIndexBuffer(ibuf, DXGI_FORMAT_R16_UINT, 0);
		ddImmediateContext->DrawIndexed(curindis, 0, 0);
		curverts = curindis = 0;
	}

};

struct D3D11EnhancedTerrainRenderer::Impl {
	std::unique_ptr<RCustomBatchD3D11<D11NTRVtx>> batch;
	std::unique_ptr<RBatch> lakeBatch;
	DynArray<uint32_t> trnNormals;
	ComPtr<ID3D11PixelShader> shaderPix;
	ComPtr<ID3D11VertexShader> shaderVtx;
	ComPtr<ID3D11InputLayout> ddInputLayout;
	ComPtr<ID3D11Buffer> sunBuffer;
};

void D3D11EnhancedTerrainRenderer::init()
{
	auto* d11gfx = (D3D11Renderer*)gfx;
	_impl = new Impl;
	_impl->batch = std::make_unique<RCustomBatchD3D11<D11NTRVtx>>(4 * 256, 6 * 256, d11gfx->ddDevice, d11gfx->ddImmediateContext);
	_impl->lakeBatch.reset(d11gfx->CreateBatch(4 * 256, 6 * 256));
	texcache = new TextureCache(gfx, "Maps\\Map_Textures\\");

	// Load terrain color textures
	for (const auto &grp : terrain->texDb.groups) {
		printf("%s\n", grp.first.c_str());
		for (const auto &tex : grp.second->textures) {
			LoadedTexture ld;
			ld.diffuseMap = texcache->getTexture(tex.second->file.c_str());

			std::string nrm;
			size_t ext = tex.second->file.rfind('.');
			if (ext != nrm.npos)
				nrm = tex.second->file.substr(0, ext);
			else
				nrm = tex.second->file;
			if (g_settings.value<int>("gameVersion", 0) < 2) {
				nrm.append("_NormalMap.pcx");
				ld.normalMap = texcache->getTexture(nrm.c_str());
			}
			else {
				nrm.append("_BumpMap.pcx");
				ld.normalMap = texcache->getTextureIfCached(nrm.c_str());
				if (!ld.normalMap) {
					Bitmap bmp = Bitmap::loadBitmap((texcache->directory + nrm).c_str());
					Bitmap nmap; nmap.width = bmp.width; nmap.height = bmp.height; nmap.format = BMFORMAT_R8G8B8A8;
					nmap.pixels.resize(nmap.width * nmap.height * 4);
					uint8_t* bmpix = (uint8_t*)bmp.pixels.data();
					uint8_t* nmpix = (uint8_t*)nmap.pixels.data();
					int pitch = nmap.width;
					for (int tz = 0; tz < 4; tz++) {
						for (int tx = 0; tx < 4; tx++) {
							int sx = 64 * tx, sz = 64 * tz;
							for (int cz = 0; cz < 64; cz++) {
								for (int cx = 0; cx < 64; cx++) {
									int ax = (cx + 1) & 63, az = (cz + 1) & 63;
									int bx = (cx - 1) & 63, bz = (cz - 1) & 63;

									int cheight = bmpix[(sz + cz) * pitch + sx + cx];

									int dx1 = 0, dx2 = 0, dz1 = 0, dz2 = 0;
									if(ax != 0)
										dx1 = (int)bmpix[(sz + cz) * pitch + sx + ax] - cheight;
									if(bx != 63)
										dx2 = (int)bmpix[(sz + cz) * pitch + sx + bx] - cheight;
									if(az != 0)
										dz1 = (int)bmpix[(sz + az) * pitch + sx + cx] - cheight;
									if(bz != 63)
										dz2 = (int)bmpix[(sz + bz) * pitch + sx + cx] - cheight;

									float fx = (-dx1+dx2) / 510.0f, fz = (-dz1+dz2) / 510.0f;
									//fx *= 3.0f; fz *= 3.0f;
									//float fy = 1.0f - fx * fx - fz * fz;
									float fy = 0.1f;
									float norm = std::sqrt(fx * fx + fy * fy + fz * fz);
									fx /= norm; fy /= norm; fz /= norm;

									int dx = (int)(fx * 255.0f);
									int dy = (int)(fy * 255.0f);
									int dz = (int)(fz * 255.0f);

									uint8_t* out = nmpix + ((sz + cz) * pitch + sx + cx) * 4;
									out[0] = (uint8_t)((dx + 255) / 2);
									out[1] = (uint8_t)((dy + 255) / 2);
									out[2] = (uint8_t)((dz + 255) / 2);
									out[3] = 255;
								}
							}
						}
					}
					ld.normalMap = texcache->createTexture(nrm.c_str(), 0, nmap);
				}
			}
			ttexmap[tex.second.get()] = ld;
		}
	}

	// Precompute terrain normals
	_impl->trnNormals.resize((terrain->width + 1) * (terrain->height + 1));
	size_t ni = 0;
	for (unsigned int z = 0; z <= terrain->height; z++) {
		for (unsigned int x = 0; x <= terrain->width; x++) {
			_impl->trnNormals[ni++] = NormalToR10G10B10A2(terrain->getNormal(x, z));
		}
	}

	// D3D11 stuff
	auto psBlob = d11gfx->compileShader(tshaderCode, sizeof(tshaderCode), "PS", "ps_4_1");
	auto vsBlob = d11gfx->compileShader(tshaderCode, sizeof(tshaderCode), "VS", "vs_4_0");
	static const D3D11_INPUT_ELEMENT_DESC iadesc[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R10G10B10A2_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TANGENT", 0, DXGI_FORMAT_R10G10B10A2_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"BITANGENT", 0, DXGI_FORMAT_R10G10B10A2_UNORM, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	HRESULT hres;
	hres = d11gfx->ddDevice->CreateInputLayout(iadesc, std::size(iadesc), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &_impl->ddInputLayout);
	assert(!FAILED(hres));
	hres = d11gfx->ddDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &_impl->shaderPix);
	assert(!FAILED(hres));
	hres = d11gfx->ddDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &_impl->shaderVtx);
	assert(!FAILED(hres));

	D3D11_BUFFER_DESC bd;
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bd.MiscFlags = 0;
	bd.StructureByteStride = 0;
	bd.ByteWidth = 48;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	d11gfx->ddDevice->CreateBuffer(&bd, nullptr, &_impl->sunBuffer);
}

D3D11EnhancedTerrainRenderer::~D3D11EnhancedTerrainRenderer()
{
	delete texcache;
}

Vector3 g_gfxplusLampPos(0.0f, 0.0f, 0.0f);
bool g_gfxplusBumpOn = true;

void D3D11EnhancedTerrainRenderer::render() {
	auto* d11gfx = (D3D11Renderer*)gfx;

	gfx->BeginMapDrawing();

	constexpr float tilesize = 5.0f;
	Vector3 sunNormal = terrain->sunVector.normal();

	// camera space bounding box
	const Vector3& camstart = camera->position;
	Vector3 camend = camera->position + camera->direction * camera->farDist;
	float farheight = std::tan(0.9f) * camera->farDist;
	float farwidth = farheight * camera->aspect;
	Vector3 camside = camera->direction.cross(Vector3(0, 1, 0)).normal();
	Vector3 campup = camside.cross(camera->direction).normal();
	Vector3 farleft = camend + camside * farwidth;
	Vector3 farright = camend - camside * farwidth;
	Vector3 farup = camend + campup * farheight;
	Vector3 fardown = camend - campup * farheight;
	float bbx1, bbz1, bbx2, bbz2;
	std::tie(bbx1, bbx2) = std::minmax({ camstart.x, farleft.x, farright.x, farup.x, fardown.x });
	std::tie(bbz1, bbz2) = std::minmax({ camstart.z, farleft.z, farright.z, farup.z, fardown.z });
	auto clamp = [](auto val, auto low, auto high) {return std::max(low, std::min(high, val)); };
	int tlsx = clamp((int)std::floor(bbx1 / tilesize) + (int)terrain->edge, 0, (int)terrain->width - 1);
	int tlsz = clamp((int)std::floor(bbz1 / tilesize) + (int)terrain->edge, 0, (int)terrain->height - 1);
	int tlex = clamp((int)std::ceil(bbx2 / tilesize) + (int)terrain->edge, 0, (int)terrain->width - 1);
	int tlez = clamp((int)std::ceil(bbz2 / tilesize) + (int)terrain->edge, 0, (int)terrain->height - 1);

	gfx->BeginBatchDrawing();
	auto* dimm = ((D3D11Renderer*)gfx)->ddImmediateContext;
	dimm->PSSetShader(_impl->shaderPix.Get(), nullptr, 0);
	dimm->IASetInputLayout(_impl->ddInputLayout.Get());
	dimm->VSSetShader(_impl->shaderVtx.Get(), nullptr, 0);

	D3D11_MAPPED_SUBRESOURCE mappedRes;
	HRESULT hres = dimm->Map(_impl->sunBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
	assert(!FAILED(hres));
	char* sbdata = (char*)mappedRes.pData;
	*(Vector3*)(sbdata+0) = sunNormal;
	*(Vector3*)(sbdata+16) = m_lampPos;
	*(int*)(sbdata + 28) = m_bumpOn ? 1 : 0;
	*(int*)(sbdata + 32) = 1;
	dimm->Unmap(_impl->sunBuffer.Get(), 0);
	dimm->VSSetConstantBuffers(2, 1, _impl->sunBuffer.GetAddressOf());
	dimm->PSSetConstantBuffers(2, 1, _impl->sunBuffer.GetAddressOf());

	auto* batch = _impl->batch.get();
	batch->begin();
	texture oldgfxtex = 0;
	for (int z = tlsz; z <= tlez; z++) {
		for (int x = tlsx; x <= tlex; x++) {
			int lx = x - terrain->edge, lz = z - terrain->edge;
			Vector3 pp((lx + 0.5f)*tilesize, terrain->getVertex(x, terrain->height - z), (lz + 0.5f)*tilesize);
			Vector3 camcenter = camera->position + camera->direction * 125.0f;
			pp += (camcenter - pp).normal() * tilesize * sqrtf(2.0f);
			Vector3 ttpp = pp.transformScreenCoords(camera->sceneMatrix);
			if (ttpp.x < -1 || ttpp.x > 1 || ttpp.y < -1 || ttpp.y > 1 || ttpp.z < -1 || ttpp.z > 1)
				continue;
			Terrain::Tile *tile = &terrain->tiles[(terrain->height - 1 - z)*terrain->width + x];
			TerrainTexture *trntex = tile->texture;
			auto& newgfxtex = ttexmap.at(trntex);
			tilesPerTex[newgfxtex].push_back(tile);
		}
	}

	for(auto &pack : tilesPerTex) {
		if (pack.second.empty())
			continue;
		gfx->SetTexture(0, pack.first.diffuseMap);
		gfx->SetTexture(1, pack.first.normalMap);
		for (TerrainTile *tile : pack.second) {
			unsigned int x = tile->x;
			unsigned int z = terrain->height - 1 - tile->z;
			int lx = x - terrain->edge, lz = z - terrain->edge;

			TerrainTexture *trntex = tile->texture;
			float sx = trntex->startx / 256.0f;
			float sy = trntex->starty / 256.0f;
			float tw = trntex->width / 256.0f;
			float th = trntex->height / 256.0f;
			std::array<std::pair<float, float>, 4> uvs{ { {sx, sy}, {sx + tw, sy}, { sx + tw, sy + th }, {sx, sy + th } } };
			bool xflip = (bool)tile->xflip != (bool)(tile->rot & 2);
			bool zflip = (bool)tile->zflip != (bool)(tile->rot & 2);
			bool rot = tile->rot & 1;
			if (!xflip) {
				std::swap(uvs[0], uvs[3]);
				std::swap(uvs[1], uvs[2]);
			}
			if (zflip) {
				std::swap(uvs[0], uvs[1]);
				std::swap(uvs[2], uvs[3]);
			}
			if (rot) {
				auto tmp = uvs[0];
				uvs[0] = uvs[1];
				uvs[1] = uvs[2];
				uvs[2] = uvs[3];
				uvs[3] = tmp;
			}

			Vector3 poses[4];
			poses[0] = Vector3(lx * tilesize, terrain->getVertex(x, terrain->height - z), lz * tilesize);
			poses[1] = Vector3((lx + 1) * tilesize, terrain->getVertex(x + 1, terrain->height - z), lz * tilesize);
			poses[2] = Vector3((lx + 1) * tilesize, terrain->getVertex(x + 1, terrain->height - (z + 1)), (lz + 1) * tilesize);
			poses[3] = Vector3(lx * tilesize, terrain->getVertex(x, terrain->height - (z + 1)), (lz + 1) * tilesize);

			float du1 = uvs[3].first - uvs[0].first, dv1 = uvs[3].second - uvs[0].second;
			float du2 = uvs[1].first - uvs[0].first, dv2 = uvs[1].second - uvs[0].second;
			Vector3 edge1 = poses[3] - poses[0];
			Vector3 edge2 = poses[1] - poses[0];
			float idet = 1.0f / (du1 * dv2 - du2 * dv1);
			Vector3 tangent = (edge1 * dv2 - edge2 * dv1) * idet;
			Vector3 bitangent = (edge2 * du1 - edge1 * du2) * idet;
			uint32_t cmprTangent = NormalToR10G10B10A2(tangent.normal());
			uint32_t cmprBitangent = NormalToR10G10B10A2(bitangent.normal());

			auto getPrecomputedNormal = [this](int x, int z) { return _impl->trnNormals[z * (terrain->width + 1) + x]; };
			D11NTRVtx *outvert; uint16_t *outindices; unsigned int firstindex;
			batch->next(4, 6, &outvert, &outindices, &firstindex);
			outvert[0].pos = poses[0];
			outvert[0].normal = getPrecomputedNormal(x, terrain->height - z);
			outvert[0].tangent = cmprTangent;
			outvert[0].bitangent = cmprBitangent;
			outvert[0].u = uvs[0].first;
			outvert[0].v = uvs[0].second;
			outvert[1].pos = poses[1];
			outvert[1].normal = getPrecomputedNormal(x + 1, terrain->height - z);
			outvert[1].tangent = cmprTangent;
			outvert[1].bitangent = cmprBitangent;
			outvert[1].u = uvs[1].first;
			outvert[1].v = uvs[1].second;
			outvert[2].pos = poses[2];
			outvert[2].normal = getPrecomputedNormal(x + 1, terrain->height - z - 1);
			outvert[2].tangent = cmprTangent;
			outvert[2].bitangent = cmprBitangent;
			outvert[2].u = uvs[2].first;
			outvert[2].v = uvs[2].second;
			outvert[3].pos = poses[3];
			outvert[3].normal = getPrecomputedNormal(x, terrain->height - z - 1);
			outvert[3].tangent = cmprTangent;
			outvert[3].bitangent = cmprBitangent;
			outvert[3].u = uvs[3].first;
			outvert[3].v = uvs[3].second;

			uint16_t *oix = outindices;
			for (const int &c : { 0,3,1,1,3,2 })
				*(oix++) = firstindex + c;
		}
		batch->flush();
	}
	batch->end();

	dimm->IASetInputLayout(((D3D11Renderer*)gfx)->ddInputLayout);

	// ----- Lakes -----
	gfx->BeginBatchDrawing();
	gfx->NoTexture(0);
	auto* lakeBatch = _impl->lakeBatch.get();
	lakeBatch->begin();
	gfx->BeginLakeDrawing();
	dimm->PSSetShader(d11gfx->ddLakePixelShader, nullptr, 0);
	dimm->VSSetShader(d11gfx->ddFogVertexShader, nullptr, 0);
	const uint32_t color = (terrain->fogColor & 0xFFFFFF) | 0x80000000;
	for (int z = tlsz; z <= tlez; z++) {
		for (int x = tlsx; x <= tlex; x++) {
			const TerrainTile* tile = terrain->getTile(x, z);
			if (tile && tile->fullOfWater) {
				unsigned int x = tile->x;
				unsigned int z = terrain->height - 1 - tile->z;
				int lx = x - terrain->edge, lz = z - terrain->edge;

				// is water tile visible on screen?
				Vector3 pp((lx + 0.5f) * tilesize, tile->waterLevel, (lz + 0.5f) * tilesize);
				Vector3 camcenter = camera->position + camera->direction * 125.0f;
				pp += (camcenter - pp).normal() * tilesize * sqrtf(2.0f);
				Vector3 ttpp = pp.transformScreenCoords(camera->sceneMatrix);
				if (ttpp.x < -1 || ttpp.x > 1 || ttpp.y < -1 || ttpp.y > 1 || ttpp.z < -1 || ttpp.z > 1)
					continue;

				batchVertex* outvert; uint16_t* outindices; unsigned int firstindex;
				lakeBatch->next(4, 6, &outvert, &outindices, &firstindex);
				outvert[0].x = lx * tilesize; outvert[0].y = tile->waterLevel ; outvert[0].z = lz * tilesize;
				outvert[0].color = color; outvert[0].u = 0.0f; outvert[0].v = 0.0f;
				outvert[1].x = (lx + 1) * tilesize; outvert[1].y = tile->waterLevel; outvert[1].z = lz * tilesize;
				outvert[1].color = color; outvert[1].u = 0.0f; outvert[1].v = 0.0f;
				outvert[2].x = (lx + 1) * tilesize; outvert[2].y = tile->waterLevel; outvert[2].z = (lz + 1) * tilesize;
				outvert[2].color = color; outvert[2].u = 0.0f; outvert[2].v = 0.0f;
				outvert[3].x = lx * tilesize; outvert[3].y = tile->waterLevel; outvert[3].z = (lz + 1) * tilesize;
				outvert[3].color = color; outvert[3].u = 0.0f; outvert[3].v = 0.0f;
				uint16_t* oix = outindices;
				for (const int& c : { 0,3,1,1,3,2 })
					*(oix++) = firstindex + c;
			}
		}
	}
	lakeBatch->flush();
	lakeBatch->end();

	for (auto& pack : tilesPerTex)
		pack.second.clear();
}

#endif // _WIN32