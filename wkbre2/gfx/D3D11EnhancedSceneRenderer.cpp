// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "D3D11EnhancedSceneRenderer.h"
#include "../Model.h"
#include "../scene.h"
#include "../Camera.h"
#include <algorithm>
#include <iterator>
#include <map>
#include <set>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "renderer_d3d11.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

static constexpr uint32_t Vec3ToR10G10B10A2(const Vector3& vec) {
	uint32_t res = (uint32_t)(vec.x * 1023.0f);
	res |= (uint32_t)(vec.y * 1023.0f) << 10;
	res |= (uint32_t)(vec.z * 1023.0f) << 20;
	return res;
}
static constexpr uint32_t NormalToR10G10B10A2(const Vector3& vec) {
	return Vec3ToR10G10B10A2((vec + Vector3(1, 1, 1)) * 0.5f);
}

static const char esShaderCode[] = R"---(
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

cbuffer SunBuffer : register(b2)
{
	float3 SunDirection;
	float3 LampPos;
	int BumpOn;
	int LampOn;
};

cbuffer SceneBuffer : register(b3)
{
	matrix OldEntityTransform;
	int EntityFlags;
	float EntityTime;
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float3 Norm: NORMAL;
	float4 Color : COLOR0;
	float2 Texcoord : TEXCOORD0;
	float Fog : FOG;
};

VS_OUTPUT VS(float3 Pos : POSITION, float3 Normal : NORMAL, float2 Texcoord : TEXCOORD, float4x4 EntityTransform : TRANSFORM)
{
	VS_OUTPUT output = (VS_OUTPUT)0;
	float4x4 trans = mul(EntityTransform, Transform);
	output.Pos = mul(float4(Pos, 1), trans);
	if(EntityFlags & 1) {
		output.Norm = float3(0,0,0);
		output.Color = float4(1,1,1,1);
	}
	else {
		output.Norm = normalize(mul(float4(Normal * 2 - float3(1,1,1), 0), EntityTransform).xyz);
		output.Color = float4( clamp((dot(output.Norm, normalize(SunDirection)) + 1.0) * 0.5, 160.0/255.0, 1.0) * float3(1,1,1), 1 );
	}
	output.Texcoord = Texcoord;
	output.Fog = clamp((output.Pos.w-FogStartDist) / (FogEndDist-FogStartDist), 0, 1);
	return output;
};

VS_OUTPUT VS_Anim(float3 Pos1 : POSITION0, float3 Pos2 : POSITION1, float3 Normal1 : NORMAL0, float3 Normal2 : NORMAL1, float2 Texcoord : TEXCOORD, float4x4 EntityTransform : TRANSFORM)
{
	return VS(lerp(Pos1, Pos2, EntityTime), lerp(Normal1, Normal2, EntityTime), Texcoord, EntityTransform);
};

float4 PS(VS_OUTPUT input) : SV_Target
{
	float4 tex = inpTexture.Sample(inpSampler, input.Texcoord);
	return lerp(input.Color * tex, FogColor, input.Fog);
}

static const float ALPHA_REF = 0.94;
float4 PS_Alpha(VS_OUTPUT input) : SV_Target
{
	float4 tex = inpTexture.Sample(inpSampler, input.Texcoord);
	if(tex.a < ALPHA_REF) discard;
	return lerp(input.Color * tex, FogColor, input.Fog);
}
)---";

struct EnhMesh
{
	ComPtr<ID3D11Buffer> vertices;
	ComPtr<ID3D11Buffer> normals;
	std::vector<ComPtr<ID3D11Buffer>> texcoords;
	ComPtr<ID3D11Buffer> indices;
	uint32_t totVertices = 0, totIndices = 0;
	static EnhMesh createEnhMesh(StaticModel* model, ID3D11Device* ddev) {
		EnhMesh enh;
		model->prepare();
		const Mesh& mesh = model->mesh;
		enh.totVertices = 0;
		enh.totIndices = 0;
		for (auto& mat : mesh.groupIndices)
			enh.totVertices += mat.size();

		// prepare buffer content
		// create buffers

		D3D11_BUFFER_DESC bdesc;
		bdesc.Usage = D3D11_USAGE_DEFAULT;
		bdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bdesc.CPUAccessFlags = 0;
		bdesc.MiscFlags = 0;
		bdesc.StructureByteStride = 0;
		D3D11_SUBRESOURCE_DATA binit;
		binit.SysMemPitch = binit.SysMemSlicePitch = 0;
		
		std::vector<Vector3> pVertices;
		pVertices.reserve(enh.totVertices);
		for (auto& mat : mesh.groupIndices)
			for (auto& grp : mat)
				pVertices.push_back(*(const Vector3*)(mesh.vertices.data() + 3 * grp.vertex));
		bdesc.ByteWidth = enh.totVertices * 12;
		binit.pSysMem = pVertices.data();
		ddev->CreateBuffer(&bdesc, &binit, &enh.vertices);

		std::vector<uint32_t> pNormals;
		pNormals.reserve(enh.totVertices);
		for (auto& mat : mesh.groupIndices)
			for (auto& grp : mat)
				pNormals.push_back(NormalToR10G10B10A2(Mesh::s_normalTable[mesh.normals[grp.normal]].normal()));
		bdesc.ByteWidth = enh.totVertices * 4;
		binit.pSysMem = pNormals.data();
		ddev->CreateBuffer(&bdesc, &binit, &enh.normals);

		enh.texcoords.resize(mesh.uvLists.size());
		std::vector<float> pUvs;
		for (size_t i = 0; i < mesh.uvLists.size(); i++) {
			auto& uvlist = mesh.uvLists[i];
			pUvs.clear();
			pUvs.reserve(enh.totVertices);
			for (auto& mat : mesh.groupIndices)
				for (auto& grp : mat)
					pUvs.insert(pUvs.end(), { uvlist[2 * grp.uv], uvlist[2 * grp.uv + 1] });
			bdesc.ByteWidth = enh.totVertices * 8;
			binit.pSysMem = pUvs.data();
			ddev->CreateBuffer(&bdesc, &binit, &enh.texcoords[i]);
		}

		std::vector<uint16_t> pIndices;
		for (auto& mat : mesh.polyLists[0].groups) {
			for (auto& tri : mat.tupleIndex) {
				// index offset for mats????????????
				pIndices.insert(pIndices.end(), tri.begin(), tri.end());
			}
			enh.totIndices += 3 * mat.tupleIndex.size();
		}
		bdesc.ByteWidth = pIndices.size() * 2;
		bdesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		binit.pSysMem = pIndices.data();
		ddev->CreateBuffer(&bdesc, &binit, &enh.indices);

		return enh;
	}
};

struct EnhAnim
{
	EnhMesh* enhMesh;
	std::vector<uint32_t> animTimes;
	std::vector<ComPtr<ID3D11Buffer>> animVertices;
	std::vector<ComPtr<ID3D11Buffer>> animNormals;
	static EnhAnim createEnhAnim(AnimatedModel* model, ID3D11Device* ddev) {
		EnhAnim enh;

		D3D11_BUFFER_DESC bdesc;
		bdesc.Usage = D3D11_USAGE_DEFAULT;
		bdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bdesc.CPUAccessFlags = 0;
		bdesc.MiscFlags = 0;
		bdesc.StructureByteStride = 0;
		D3D11_SUBRESOURCE_DATA binit;
		binit.SysMemPitch = binit.SysMemSlicePitch = 0;

		model->prepare();
		const auto& anim = model->anim;
		const auto staticModel = model->getStaticModel();
		staticModel->prepare();
		const auto& mesh = staticModel->mesh;
		std::set<uint32_t> animTimesSet;
		for (int c = 0; c < 3; ++c) {
			animTimesSet.insert(anim.coords[c].frameTimes.begin(), anim.coords[c].frameTimes.end());
		}
		enh.animTimes.assign(animTimesSet.begin(), animTimesSet.end());
		enh.animTimes.back() -= 1; // prevents last frame from becoming first frame
		enh.animVertices.resize(enh.animTimes.size());
		enh.animNormals.resize(enh.animTimes.size());
		for (size_t frame = 0; frame < enh.animTimes.size(); ++frame) {
			const Vector3* intVertices = (const Vector3*)model->interpolate(enh.animTimes[frame]);
			const Vector3* intNormals = model->interpolateNormals(enh.animTimes[frame]);
			std::vector<Vector3> pVertices; std::vector<uint32_t> pNormals;
			for (auto& mat : mesh.groupIndices) {
				for (auto& grp : mat) {
					pVertices.push_back(intVertices[grp.vertex]);
					pNormals.push_back(NormalToR10G10B10A2(intNormals[grp.normal].normal()));
				}
			}
			bdesc.ByteWidth = 12 * pVertices.size();
			binit.pSysMem = pVertices.data();
			ddev->CreateBuffer(&bdesc, &binit, &enh.animVertices[frame]);
			bdesc.ByteWidth = 4 * pNormals.size();
			binit.pSysMem = pNormals.data();
			ddev->CreateBuffer(&bdesc, &binit, &enh.animNormals[frame]);
		}
		return enh;
	}
};

struct D3D11EnhancedSceneRenderer::Impl {
	std::map<StaticModel*, EnhMesh> enhMeshMap;
	std::map<AnimatedModel*, EnhAnim> enhAnimMap;
	ComPtr<ID3D11InputLayout> meshIA, animIA;
	ComPtr<ID3D11VertexShader> meshVS, animVS;
	ComPtr<ID3D11PixelShader> defPS, alphaPS;
	ComPtr<ID3D11Buffer> sceneConstBuffer, sunConstBuffer;
	const size_t MAX_INSTANCES = 16;
	ComPtr<ID3D11Buffer> instanceBuffer;
};

D3D11EnhancedSceneRenderer::~D3D11EnhancedSceneRenderer()
{
	delete _impl;
}

void D3D11EnhancedSceneRenderer::init()
{
	_impl = new Impl;

	D3D11Renderer* d11gfx = (D3D11Renderer*)gfx;
	ID3D11Device* ddev = d11gfx->ddDevice;

	auto vsBlob = d11gfx->compileShader(esShaderCode, sizeof(esShaderCode), "VS", "vs_4_0");
	static const D3D11_INPUT_ELEMENT_DESC iadescMesh[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R10G10B10A2_UNORM, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TRANSFORM", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 3, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1},
		{"TRANSFORM", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 3, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1},
		{"TRANSFORM", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 3, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1},
		{"TRANSFORM", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 3, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1},
	};
	ddev->CreateInputLayout(iadescMesh, std::size(iadescMesh), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &_impl->meshIA);
	ddev->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &_impl->meshVS);

	vsBlob = d11gfx->compileShader(esShaderCode, sizeof(esShaderCode), "VS_Anim", "vs_4_0");
	static const D3D11_INPUT_ELEMENT_DESC iadescAnim[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R10G10B10A2_UNORM, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"POSITION", 1, DXGI_FORMAT_R32G32B32_FLOAT, 3, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL", 1, DXGI_FORMAT_R10G10B10A2_UNORM, 4, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TRANSFORM", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 5, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1},
		{"TRANSFORM", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 5, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1},
		{"TRANSFORM", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 5, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1},
		{"TRANSFORM", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 5, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1},
	};
	ddev->CreateInputLayout(iadescAnim, std::size(iadescAnim), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &_impl->animIA);
	if (!_impl->animIA)
		printf("animIA creation failed\n");
	ddev->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &_impl->animVS);

	auto psBlob = d11gfx->compileShader(esShaderCode, sizeof(esShaderCode), "PS", "ps_4_1");
	ddev->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &_impl->defPS);
	psBlob = d11gfx->compileShader(esShaderCode, sizeof(esShaderCode), "PS_Alpha", "ps_4_1");
	ddev->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &_impl->alphaPS);

	D3D11_BUFFER_DESC bdesc;
	bdesc.ByteWidth = 80;
	bdesc.Usage = D3D11_USAGE_DYNAMIC;
	bdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bdesc.MiscFlags = 0;
	bdesc.StructureByteStride = 0;
	ddev->CreateBuffer(&bdesc, nullptr, &_impl->sceneConstBuffer);
	bdesc.ByteWidth = 48;
	ddev->CreateBuffer(&bdesc, nullptr, &_impl->sunConstBuffer);
	bdesc.ByteWidth = 64 * _impl->MAX_INSTANCES;
	bdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	ddev->CreateBuffer(&bdesc, nullptr, &_impl->instanceBuffer);
}

void D3D11EnhancedSceneRenderer::render()
{
	D3D11Renderer* d11gfx = (D3D11Renderer*)gfx;
	ID3D11Device* ddev = d11gfx->ddDevice;
	ID3D11DeviceContext* dimm = d11gfx->ddImmediateContext;

	gfx->BeginMeshDrawing();
	gfx->BeginBatchDrawing();
	dimm->IASetInputLayout(_impl->meshIA.Get());
	dimm->VSSetShader(_impl->meshVS.Get(), nullptr, 0);
	dimm->PSSetShader(_impl->defPS.Get(), nullptr, 0);

	// Sun constant buffer
	D3D11_MAPPED_SUBRESOURCE scbm;
	dimm->Map(_impl->sunConstBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &scbm);
	*(Vector3*)scbm.pData = scene->sunDirection.normal();
	dimm->Unmap(_impl->sunConstBuffer.Get(), 0);
	dimm->VSSetConstantBuffers(2, 1, _impl->sunConstBuffer.GetAddressOf());

	bool cur_alphatest = false;
	texture cur_texture = 0;
	bool cur_isanim = false;
	//gfx->DisableAlphaTest();
	gfx->NoTexture(0);
	for (const auto& it : scene->matInsts) {
		// Texture & alpha test change
		const Material& mat = scene->modelCache->getMaterial(it.first.first);
		bool next_alphatest = mat.alphaTest;
		texture next_texture = it.second.tex;
		if (cur_alphatest != next_alphatest) {
			// TODO: Change alpha test
			if (next_alphatest)
				dimm->PSSetShader(_impl->alphaPS.Get(), nullptr, 0);
			else
				dimm->PSSetShader(_impl->defPS.Get(), nullptr, 0);
		}
		if (cur_texture != next_texture)
			gfx->SetTexture(0, next_texture);
		cur_alphatest = next_alphatest;
		cur_texture = next_texture;

		auto* model = it.first.second;
		auto* staticModel = model->getStaticModel();
		
		auto mt = _impl->enhMeshMap.find(staticModel);
		const EnhMesh* enhMesh;
		if (mt == _impl->enhMeshMap.end()) {
			_impl->enhMeshMap[staticModel] = EnhMesh::createEnhMesh(staticModel, ddev);
			enhMesh = &_impl->enhMeshMap[staticModel];
		}
		else {
			enhMesh = &mt->second;
		}

		const EnhAnim* enhAnim = nullptr;
		if (model != staticModel) {
			auto* animModel = (AnimatedModel*)model;
			auto at = _impl->enhAnimMap.find(animModel);
			if (at == _impl->enhAnimMap.end()) {
				_impl->enhAnimMap[animModel] = EnhAnim::createEnhAnim(animModel, ddev);
				enhAnim = &_impl->enhAnimMap[animModel];
			}
			else {
				enhAnim = &at->second;
			}
		}

		bool next_isanim = (bool)enhAnim;
		if (next_isanim != cur_isanim) {
			if (next_isanim) {
				dimm->IASetInputLayout(_impl->animIA.Get());
				dimm->VSSetShader(_impl->animVS.Get(), nullptr, 0);
			}
			else {
				dimm->IASetInputLayout(_impl->meshIA.Get());
				dimm->VSSetShader(_impl->meshVS.Get(), nullptr, 0);
			}
			cur_isanim = next_isanim;
		}

		int prevFrame = -1;
		float prevAnimLerp = -1.0f;
		int prevColor = -1;
		uint32_t prevFlags = -1;
		std::vector<Matrix> instTransforms;

		auto flush = [&]() {
			if (instTransforms.empty()) return;
			D3D11_MAPPED_SUBRESOURCE scbm;
			dimm->Map(_impl->instanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &scbm);
			memcpy(scbm.pData, instTransforms.data(), instTransforms.size() * 64);
			dimm->Unmap(_impl->instanceBuffer.Get(), 0);

			dimm->Map(_impl->sceneConstBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &scbm);
			//*(Matrix*)scbm.pData = ent->transform.getTranspose();
			((uint32_t*)scbm.pData)[16] = prevFlags;
			((float*)scbm.pData)[17] = prevAnimLerp;
			dimm->Unmap(_impl->sceneConstBuffer.Get(), 0);
			dimm->VSSetConstantBuffers(3, 1, _impl->sceneConstBuffer.GetAddressOf());

			if (enhAnim) {
				auto* animModel = (AnimatedModel*)model;
				ID3D11Buffer* iaBuffers[6] = { enhAnim->animVertices[prevFrame].Get(), enhAnim->animNormals[prevFrame].Get(),
												enhMesh->texcoords[prevColor].Get(),
												enhAnim->animVertices[prevFrame + 1].Get(), enhAnim->animNormals[prevFrame + 1].Get(),
												_impl->instanceBuffer.Get() };
				static const UINT iaStrides[std::size(iaBuffers)] = { 12, 4, 8, 12, 4, 64 };
				static const UINT iaOffsets[std::size(iaBuffers)] = { 0,0,0,0,0,0 };
				dimm->IASetVertexBuffers(0, std::size(iaBuffers), std::data(iaBuffers), iaStrides, iaOffsets);
				dimm->IASetIndexBuffer(enhMesh->indices.Get(), DXGI_FORMAT_R16_UINT, 0);
			}
			else {
				ID3D11Buffer* iaBuffers[4] = { enhMesh->vertices.Get(), enhMesh->normals.Get(),
												enhMesh->texcoords[prevColor].Get(),
												_impl->instanceBuffer.Get() };
				static const UINT iaStrides[std::size(iaBuffers)] = { 12, 4, 8, 64 };
				static const UINT iaOffsets[std::size(iaBuffers)] = { 0,0,0,0 };
				dimm->IASetVertexBuffers(0, std::size(iaBuffers), std::data(iaBuffers), iaStrides, iaOffsets);
				dimm->IASetIndexBuffer(enhMesh->indices.Get(), DXGI_FORMAT_R16_UINT, 0);
			}

			PolygonList& polylist = staticModel->mesh.polyLists[0];
			uint32_t startIndex = 0, startVertex = 0;
			for (size_t g = 0; g < polylist.groups.size(); g++) {
				uint32_t numIndices = 3 * polylist.groups[g].tupleIndex.size();
				uint32_t numVertices = staticModel->mesh.groupIndices[g].size();
				if (staticModel->matIds[g] == it.first.first) {
					//dimm->DrawIndexed(numIndices, startIndex, startVertex);
					dimm->DrawIndexedInstanced(numIndices, instTransforms.size(), startIndex, startVertex, 0);
				}
				startIndex += numIndices;
				startVertex += numVertices;
			}

			instTransforms.clear();
		};

		for (const SceneEntity* ent : it.second.list) {
			size_t frame = 0;
			float animLerp = 0.0f;
			if (enhAnim) {
				auto* animModel = (AnimatedModel*)model;
				uint32_t animtime = ent->animTime % animModel->anim.duration;
				for (; frame < enhAnim->animTimes.size() - 1; frame++)
					if (enhAnim->animTimes[frame + 1] >= animtime)
						break;
				animLerp = (float)(animtime - enhAnim->animTimes[frame]) / (float)(enhAnim->animTimes[frame + 1] - enhAnim->animTimes[frame]);
			}
			int actualColor = ent->color % enhMesh->texcoords.size();

			if (instTransforms.size() >= _impl->MAX_INSTANCES || std::tie(prevFrame, prevAnimLerp, prevColor, prevFlags) != std::tie(frame, animLerp, actualColor, ent->flags)) {
				flush();
			}
			std::tie(prevFrame, prevAnimLerp, prevColor, prevFlags) = std::tie(frame, animLerp, actualColor, ent->flags);
			instTransforms.push_back(ent->transform.getTranspose());
		}
		flush();
	}

	dimm->IASetInputLayout(((D3D11Renderer*)gfx)->ddInputLayout);
}
