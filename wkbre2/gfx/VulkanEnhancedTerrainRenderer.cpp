// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#include "VulkanEnhancedTerrainRenderer.h"
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

#include "renderer_vulkan.h"

struct D11NTRVtx {
	Vector3 pos;
	uint32_t normal, tangent, bitangent;
	float u, v;
};

static const uint32_t Vec3ToR10G10B10A2(const Vector3& vec) {
	uint32_t res = (uint32_t)(vec.x * 1023.0f);
	res |= (uint32_t)(vec.y * 1023.0f) << 10;
	res |= (uint32_t)(vec.z * 1023.0f) << 20;
	return res;
}
static const uint32_t NormalToR10G10B10A2(const Vector3& vec) {
	return Vec3ToR10G10B10A2((vec + Vector3(1, 1, 1)) * 0.5f);
}

template<typename Idx> struct RIndexBatchVulkan
{
	static_assert(sizeof(Idx) == 2 || sizeof(Idx) == 4, "Invalid index integer size for RIndexBatchVulkan");
	static constexpr vk::IndexType format = (sizeof(Idx) == 2) ? vk::IndexType::eUint16 : vk::IndexType::eUint32;

	VulkanRenderer* gfx;
	DynamicBuffer<1> ibuf;
	uint32_t maxindis;
	uint32_t curindis;
	bool locked = false;

	RIndexBatchVulkan(int mi, VulkanRenderer* gfx) :
		gfx(gfx),
		ibuf(gfx, mi * sizeof(Idx), vk::BufferUsageFlagBits::eIndexBuffer),
		maxindis(mi),
		curindis(0)
	{}

	void lock()
	{
		if (locked) return;
		locked = true;
		ibuf.nextBuffer();
	}
	void unlock()
	{
		if (!locked) return;
		locked = false;
	}

	void next(uint32_t nindis, Idx** ipnt)
	{
		if (curindis + nindis > maxindis)
			flush();

		lock();
		*ipnt = (Idx*)ibuf.getCurrentBuffer()->mappedPtr[0] + curindis;

		curindis += nindis;
	}

	void flush()
	{
		unlock();
		if (curindis == 0) return;
		auto& frame = gfx->currentFrameObject();
		frame.mainCommandBuffer.bindIndexBuffer(ibuf.getCurrentBuffer()->buffer[0], 0, format);
		frame.mainCommandBuffer.drawIndexed(curindis, 1, 0, 0, 0);
		curindis = 0;
	}
};

/*
vk::PipelineLayout CreateTerrainPipelineLayout(VulkanRenderer* gfx)
{
	// ==== Pipeline layout ====

	vk::DescriptorSetLayoutBinding bindingsSet0[3];
	bindingsSet0[0].binding = 0;
	bindingsSet0[0].descriptorType = vk::DescriptorType::eUniformBuffer;
	bindingsSet0[0].descriptorCount = 1;
	bindingsSet0[0].stageFlags = vk::ShaderStageFlagBits::eVertex;
	bindingsSet0[1].binding = 1;
	bindingsSet0[1].descriptorType = vk::DescriptorType::eUniformBuffer;
	bindingsSet0[1].descriptorCount = 1;
	bindingsSet0[1].stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
	bindingsSet0[2].binding = 2;
	bindingsSet0[2].descriptorType = vk::DescriptorType::eUniformBuffer;
	bindingsSet0[2].descriptorCount = 1;
	bindingsSet0[2].stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
	vk::DescriptorSetLayoutBinding bindingsSet1[1];
	bindingsSet1[0].binding = 0;
	bindingsSet1[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
	bindingsSet1[0].descriptorCount = 1;
	bindingsSet1[0].stageFlags = vk::ShaderStageFlagBits::eFragment;
	bindingsSet1[0].pImmutableSamplers = &m_vkSampler;
	vk::DescriptorSetLayoutBinding bindingsSet2[1];
	bindingsSet2[0].binding = 0;
	bindingsSet2[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
	bindingsSet2[0].descriptorCount = 1;
	bindingsSet2[0].stageFlags = vk::ShaderStageFlagBits::eFragment;
	bindingsSet1[0].pImmutableSamplers = &m_vkSampler;

	vk::DescriptorSetLayoutCreateInfo dslcInfo;
	dslcInfo.bindingCount = std::size(bindingsSet0);
	dslcInfo.pBindings = bindingsSet0;
	m_vkDescSetLayout0 = m_vkDevice.createDescriptorSetLayout(dslcInfo);
	dslcInfo.bindingCount = std::size(bindingsSet1);
	dslcInfo.pBindings = bindingsSet1;
	m_vkDescSetLayout1 = m_vkDevice.createDescriptorSetLayout(dslcInfo);
	dslcInfo.bindingCount = std::size(bindingsSet2);
	dslcInfo.pBindings = bindingsSet2;
	m_vkDescSetLayout2 = m_vkDevice.createDescriptorSetLayout(dslcInfo);

	vk::PushConstantRange pushConstRanges[1];
	pushConstRanges[0].stageFlags = vk::ShaderStageFlagBits::eVertex;
	pushConstRanges[0].offset = 0;
	pushConstRanges[0].size = 8;

	vk::PipelineLayoutCreateInfo plcInfo;
	vk::DescriptorSetLayout setLayouts[] = { m_vkDescSetLayout0, m_vkDescSetLayout1, m_vkDescSetLayout2 };
	plcInfo.setLayoutCount = std::size(setLayouts);
	plcInfo.pSetLayouts = setLayouts;
	plcInfo.pushConstantRangeCount = std::size(pushConstRanges);
	plcInfo.pPushConstantRanges = pushConstRanges;
	m_vkPipelineLayout = m_vkDevice.createPipelineLayout(plcInfo);

}
*/

vk::Pipeline CreateTerrainPipeline(
	VulkanRenderer* gfx,
	vk::ShaderModule vsModule, const char* vsName,
	vk::ShaderModule psModule, const char* psName,
	const vk::PipelineVertexInputStateCreateInfo& vertexInputs)
{
	vk::PipelineShaderStageCreateInfo stageInfo[2];
	stageInfo[0].stage = vk::ShaderStageFlagBits::eVertex;
	stageInfo[0].module = vsModule;
	stageInfo[0].pName = vsName;
	stageInfo[1].stage = vk::ShaderStageFlagBits::eFragment;
	stageInfo[1].module = psModule;
	stageInfo[1].pName = psName;

	vk::PipelineInputAssemblyStateCreateInfo iaStateInfo;
	iaStateInfo.topology = vk::PrimitiveTopology::eTriangleList;

	vk::PipelineViewportStateCreateInfo vpStateInfo;
	vpStateInfo.viewportCount = 1;
	vpStateInfo.pViewports = nullptr; // dynamic
	vpStateInfo.scissorCount = 1;
	vpStateInfo.pScissors = nullptr; // dynamic

	vk::PipelineRasterizationStateCreateInfo rsStateInfo_3D;
	rsStateInfo_3D.depthClampEnable = vk::False;
	rsStateInfo_3D.rasterizerDiscardEnable = vk::False;
	rsStateInfo_3D.polygonMode = vk::PolygonMode::eFill;
	rsStateInfo_3D.cullMode = vk::CullModeFlagBits::eBack;
	rsStateInfo_3D.frontFace = vk::FrontFace::eClockwise;
	rsStateInfo_3D.depthBiasEnable = vk::False;
	rsStateInfo_3D.lineWidth = 1.0f;

	vk::PipelineMultisampleStateCreateInfo msStateInfo;
	msStateInfo.rasterizationSamples = vk::SampleCountFlagBits::e1;

	vk::PipelineDepthStencilStateCreateInfo dsStateInfo_On;
	dsStateInfo_On.depthTestEnable = vk::True;
	dsStateInfo_On.depthWriteEnable = vk::True;
	dsStateInfo_On.depthCompareOp = vk::CompareOp::eLessOrEqual;

	vk::PipelineColorBlendAttachmentState cbAttach_NoBlend;
	cbAttach_NoBlend.blendEnable = vk::False;
	cbAttach_NoBlend.srcColorBlendFactor = vk::BlendFactor(VK_BLEND_FACTOR_ONE);
	cbAttach_NoBlend.dstColorBlendFactor = vk::BlendFactor(VK_BLEND_FACTOR_ZERO);
	cbAttach_NoBlend.colorBlendOp = vk::BlendOp(VK_BLEND_OP_ADD);
	cbAttach_NoBlend.srcAlphaBlendFactor = vk::BlendFactor(VK_BLEND_FACTOR_ONE);
	cbAttach_NoBlend.dstAlphaBlendFactor = vk::BlendFactor(VK_BLEND_FACTOR_ZERO);
	cbAttach_NoBlend.alphaBlendOp = vk::BlendOp(VK_BLEND_OP_ADD);
	cbAttach_NoBlend.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
		| vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	vk::PipelineColorBlendStateCreateInfo cbStateInfo;
	cbStateInfo.attachmentCount = 1;
	cbStateInfo.pAttachments = &cbAttach_NoBlend;

	vk::DynamicState dynamicStates[] = {
		vk::DynamicState::eViewport, vk::DynamicState::eScissor
	};
	vk::PipelineDynamicStateCreateInfo pdStateInfo;
	pdStateInfo.dynamicStateCount = std::size(dynamicStates);
	pdStateInfo.pDynamicStates = dynamicStates;

	vk::GraphicsPipelineCreateInfo gpcInfo;
	gpcInfo.layout = gfx->m_vkPipelineLayout;
	gpcInfo.stageCount = std::size(stageInfo);
	gpcInfo.pStages = stageInfo;
	gpcInfo.pVertexInputState = &vertexInputs;
	gpcInfo.pInputAssemblyState = &iaStateInfo;
	gpcInfo.pTessellationState = nullptr; // none
	gpcInfo.pViewportState = &vpStateInfo; // dynamic
	gpcInfo.pRasterizationState = &rsStateInfo_3D;
	gpcInfo.pMultisampleState = &msStateInfo;
	gpcInfo.pDepthStencilState = &dsStateInfo_On;
	gpcInfo.pColorBlendState = &cbStateInfo;
	gpcInfo.pDynamicState = &pdStateInfo;
	gpcInfo.renderPass = gfx->m_vkRenderPass;
	gpcInfo.subpass = 0;

	return gfx->m_vkDevice.createGraphicsPipeline(gfx->m_vkPipelineCache, gpcInfo).value;
}

static void UpdateUniformBuffer(VulkanRenderer* gfx, vk::Buffer uniformBuffer, DynamicBuffer<1>& stageBuffer, void* data, size_t length)
{
	stageBuffer.nextBuffer();
	auto* stage = stageBuffer.getCurrentBuffer();

	void* transformBytes = stage->mappedPtr[0];
	memcpy(transformBytes, data, length);
	vmaFlushAllocation(gfx->m_vmaAllocator, stage->allocation[0], 0, length);

	vk::BufferCopy copy;
	copy.srcOffset = 0;
	copy.dstOffset = 0;
	copy.size = length;

	vk::BufferMemoryBarrier barrier;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer = uniformBuffer;
	barrier.offset = 0;
	barrier.size = vk::WholeSize;

	auto& frame = gfx->currentFrameObject();

	gfx->togglePass(nullptr);

	barrier.srcAccessMask = vk::AccessFlagBits::eUniformRead | vk::AccessFlagBits::eTransferWrite;
	barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
	frame.mainCommandBuffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eTransfer,
		vk::PipelineStageFlagBits::eTransfer,
		vk::DependencyFlags(),
		0, nullptr, 1, &barrier, 0, nullptr);

	frame.mainCommandBuffer.copyBuffer(stage->buffer[0], uniformBuffer, 1, &copy);

	barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
	barrier.dstAccessMask = vk::AccessFlagBits::eUniformRead;
	frame.mainCommandBuffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer,
		vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader,
		vk::DependencyFlags(),
		0, nullptr, 1, &barrier, 0, nullptr);
}

struct VulkanEnhancedTerrainRenderer::Impl {
	//std::unique_ptr<RCustomBatchD3D11<D11NTRVtx>> batch;
	//DynamicBuffer<2> batch;
	std::unique_ptr<RBatch> lakeBatch;
	DynArray<uint32_t> trnNormals;
	vk::ShaderModule shaderPix;
	vk::ShaderModule shaderVtx;

	VulkanRenderer::StaticBuffer trnVertexBuffer;
	RIndexBatchVulkan<uint32_t> trnIndexBatch;

	vk::Pipeline terrainPipeline;

	Impl(VulkanRenderer* gfx) :
		//batch(gfx),
		trnIndexBatch(4 * 256, gfx) { }
};

void VulkanEnhancedTerrainRenderer::init()
{
	auto* vkgfx = (VulkanRenderer*)gfx;
	_impl = new Impl(vkgfx);
	//_impl->batch = std::make_unique<RCustomBatchD3D11<D11NTRVtx>>(4 * 256, 6 * 256, vkgfx->ddDevice, vkgfx->ddImmediateContext);
	_impl->lakeBatch.reset(vkgfx->CreateBatch(4 * 256, 6 * 256));
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
	for (int z = 0; z <= terrain->height; z++) {
		for (int x = 0; x <= terrain->width; x++) {
			_impl->trnNormals[ni++] = NormalToR10G10B10A2(terrain->getNormal(x, z));
		}
	}

	// Terrain vertex buffer
	std::vector<D11NTRVtx> trnVertices;
	assert(terrain != nullptr);
	for (int z = 0; z < terrain->height; z++) {
		for (int x = 0; x < terrain->width; x++) {
			int lx = x - terrain->edge, lz = z - terrain->edge;
			const Terrain::Tile* tile = terrain->getTile(x, z);

			TerrainTexture* trntex = tile->texture;
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

			constexpr float tilesize = 5.0f;
			Vector3 poses[4];
			poses[0] = Vector3(lx * tilesize, terrain->getVertex(x, z), lz * tilesize);
			poses[1] = Vector3((lx + 1) * tilesize, terrain->getVertex(x + 1, z), lz * tilesize);
			poses[2] = Vector3((lx + 1) * tilesize, terrain->getVertex(x + 1, z + 1), (lz + 1) * tilesize);
			poses[3] = Vector3(lx * tilesize, terrain->getVertex(x, z + 1), (lz + 1) * tilesize);

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
			const uint32_t colorBits =
				(x >= terrain->edge && z >= terrain->edge && x < terrain->width - terrain->edge && z < terrain->height - terrain->edge) ?
				0xC000'0000 : 0;

			D11NTRVtx outvert[4];
			outvert[0].pos = poses[0];
			outvert[0].normal = getPrecomputedNormal(x, z) | colorBits;
			outvert[0].tangent = cmprTangent;
			outvert[0].bitangent = cmprBitangent;
			outvert[0].u = uvs[0].first;
			outvert[0].v = uvs[0].second;
			outvert[1].pos = poses[1];
			outvert[1].normal = getPrecomputedNormal(x + 1, z) | colorBits;
			outvert[1].tangent = cmprTangent;
			outvert[1].bitangent = cmprBitangent;
			outvert[1].u = uvs[1].first;
			outvert[1].v = uvs[1].second;
			outvert[2].pos = poses[2];
			outvert[2].normal = getPrecomputedNormal(x + 1, z + 1) | colorBits;
			outvert[2].tangent = cmprTangent;
			outvert[2].bitangent = cmprBitangent;
			outvert[2].u = uvs[2].first;
			outvert[2].v = uvs[2].second;
			outvert[3].pos = poses[3];
			outvert[3].normal = getPrecomputedNormal(x, z + 1) | colorBits;
			outvert[3].tangent = cmprTangent;
			outvert[3].bitangent = cmprBitangent;
			outvert[3].u = uvs[3].first;
			outvert[3].v = uvs[3].second;
			trnVertices.insert(trnVertices.end(), std::begin(outvert), std::end(outvert));
		}
	}

	_impl->trnVertexBuffer = vkgfx->createStaticBuffer(vk::BufferUsageFlagBits::eVertexBuffer, trnVertices.data(), trnVertices.size() * sizeof(D11NTRVtx));

	auto psBlob = vkgfx->loadShader("EnhTerrainShader", "PS");
	auto vsBlob = vkgfx->loadShader("EnhTerrainShader", "VS");

	static const vk::VertexInputBindingDescription viBindings[] = {
		{0, 32, vk::VertexInputRate::eVertex},
	};
	static const vk::VertexInputAttributeDescription viAttributes[] = {
		{0, 0, vk::Format(VK_FORMAT_R32G32B32_SFLOAT), 0},
		{1, 0, vk::Format(VK_FORMAT_A2B10G10R10_UNORM_PACK32), 12},
		{2, 0, vk::Format(VK_FORMAT_A2B10G10R10_UNORM_PACK32), 16},
		{3, 0, vk::Format(VK_FORMAT_A2B10G10R10_UNORM_PACK32), 20},
		{4, 0, vk::Format(VK_FORMAT_R32G32_SFLOAT), 24},
	};

	vk::PipelineVertexInputStateCreateInfo viStateInfo;
	viStateInfo.vertexBindingDescriptionCount = std::size(viBindings);
	viStateInfo.pVertexBindingDescriptions = viBindings;
	viStateInfo.vertexAttributeDescriptionCount = std::size(viAttributes);
	viStateInfo.pVertexAttributeDescriptions = viAttributes;

	_impl->terrainPipeline = CreateTerrainPipeline(vkgfx, vsBlob, "VS", psBlob, "PS", viStateInfo);
}

VulkanEnhancedTerrainRenderer::~VulkanEnhancedTerrainRenderer()
{
	delete texcache;
}

void VulkanEnhancedTerrainRenderer::render() {
	auto* vkgfx = (VulkanRenderer*)gfx;

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

	char sunData[16];
	*(Vector3*)(sunData + 0) = sunNormal;
	*(int*)(sunData + 12) = m_bumpOn ? 1 : 0;
	UpdateUniformBuffer(vkgfx, vkgfx->m_currentSunBuffer, vkgfx->m_globalBuffers->sunBuffer, sunData, 16);

	texture oldgfxtex = 0;
	for (int z = tlsz; z <= tlez; z++) {
		for (int x = tlsx; x <= tlex; x++) {
			int lx = x - terrain->edge, lz = z - terrain->edge;
			Vector3 pp((lx + 0.5f)*tilesize, terrain->getVertex(x, z), (lz + 0.5f)*tilesize);
			Vector3 camcenter = camera->position + camera->direction * 125.0f;
			pp += (camcenter - pp).normal() * tilesize * sqrtf(2.0f);
			Vector3 ttpp = pp.transformScreenCoords(camera->sceneMatrix);
			if (ttpp.x < -1 || ttpp.x > 1 || ttpp.y < -1 || ttpp.y > 1 || ttpp.z < -1 || ttpp.z > 1)
				continue;
			const Terrain::Tile* tile = terrain->getTile(x, z);
			TerrainTexture *trntex = tile->texture;
			auto& newgfxtex = ttexmap.at(trntex);
			tilesPerTex[newgfxtex].push_back(tile);
		}
	}

	auto& frame = vkgfx->currentFrameObject();

	vk::DeviceSize voffset = 0;
	vkgfx->togglePass(_impl->terrainPipeline);
	frame.mainCommandBuffer.bindVertexBuffers(0, _impl->trnVertexBuffer.buffer, voffset);
	for(auto &pack : tilesPerTex) {
		if (pack.second.empty())
			continue;
		gfx->SetTexture(0, pack.first.diffuseMap);
		gfx->SetTexture(1, pack.first.normalMap);
		vk::DescriptorSet descSets[2] = {
			vkgfx->m_currentTextureDescriptorSet, vkgfx->m_currentSecondaryTextureDescriptorSet
		};
		frame.mainCommandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, vkgfx->m_vkPipelineLayout, 1, descSets, {});

		for (const TerrainTile* tile : pack.second) {
			unsigned int x = tile->x;
			unsigned int z = tile->z;

			uint32_t firstindex = 4 * (z * terrain->width + x);
			uint32_t* oix;
			_impl->trnIndexBatch.next(6, &oix);
			for (const int &c : { 0,3,1,1,3,2 })
				*(oix++) = firstindex + c;
		}
		_impl->trnIndexBatch.flush();
	}

	// ----- Lakes -----
	gfx->BeginBatchDrawing();
	gfx->NoTexture(0);
	auto* lakeBatch = _impl->lakeBatch.get();
	lakeBatch->begin();
	gfx->BeginLakeDrawing();
	const uint32_t color = (terrain->fogColor & 0xFFFFFF) | 0x80000000;
	for (int z = tlsz; z <= tlez; z++) {
		for (int x = tlsx; x <= tlex; x++) {
			const TerrainTile* tile = terrain->getTile(x, z);
			if (tile && tile->fullOfWater) {
				unsigned int x = tile->x;
				unsigned int z = tile->z;
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
