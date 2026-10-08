// wkbre2 - WK Engine Reimplementation
// (C) 2021 AdrienTD
// Licensed under the GNU General Public License 3

#ifdef _WIN32

#include "VulkanEnhancedSceneRenderer.h"
#include "../Model.h"
#include "../scene.h"
#include "../Camera.h"
#include <algorithm>
#include <iterator>
#include <map>
#include <set>
#include <vector>

#include "renderer_vulkan.h"

static constexpr uint32_t Vec3ToR10G10B10A2(const Vector3& vec) {
	uint32_t res = (uint32_t)(vec.x * 1023.0f);
	res |= (uint32_t)(vec.y * 1023.0f) << 10;
	res |= (uint32_t)(vec.z * 1023.0f) << 20;
	return res;
}
static constexpr uint32_t NormalToR10G10B10A2(const Vector3& vec) {
	return Vec3ToR10G10B10A2((vec + Vector3(1, 1, 1)) * 0.5f);
}

struct AllocatedBuffer {
	vk::Buffer buffer;
	VmaAllocation allocation;
};

AllocatedBuffer CreateAndInitializeBuffer(VulkanRenderer* gfx, vk::BufferUsageFlags usage, void* data, size_t length)
{
	// Create buffer

	VmaAllocationCreateInfo allocationCreateInfo;
	memset(&allocationCreateInfo, 0, sizeof(allocationCreateInfo));
	allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	vk::BufferCreateInfo bufferCreateInfo;
	bufferCreateInfo.size = length;
	bufferCreateInfo.usage = usage | vk::BufferUsageFlagBits::eTransferDst;
	bufferCreateInfo.sharingMode = vk::SharingMode::eExclusive;

	VmaAllocationInfo allocInfo;
	VkBuffer buffer;
	VmaAllocation allocation;
	vmaCreateBuffer(gfx->m_vmaAllocator, &(VkBufferCreateInfo&)bufferCreateInfo, &allocationCreateInfo, &buffer, &allocation, &allocInfo);
	
	// Create Stage buffer

	VmaAllocationCreateInfo stageAllocationCreateInfo;
	memset(&stageAllocationCreateInfo, 0, sizeof(stageAllocationCreateInfo));
	stageAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT
		| VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	stageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

	vk::BufferCreateInfo stageBufferCreateInfo;
	stageBufferCreateInfo.size = length;
	stageBufferCreateInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
	stageBufferCreateInfo.sharingMode = vk::SharingMode::eExclusive;

	VmaAllocationInfo stageAllocInfo;
	VkBuffer stageBuffer;
	VmaAllocation stageAllocation;
	vmaCreateBuffer(gfx->m_vmaAllocator, &(VkBufferCreateInfo&)stageBufferCreateInfo, &stageAllocationCreateInfo, &stageBuffer, &stageAllocation, &stageAllocInfo);

	// Fill Stage buffer

	memcpy(stageAllocInfo.pMappedData, data, length);

	// Copy to buffer

	vk::BufferCopy copy;
	copy.srcOffset = 0;
	copy.dstOffset = 0;
	copy.size = length;

	vk::CommandBufferBeginInfo beginInfo;
	beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
	gfx->m_texCommandBuffer.begin(beginInfo);
	gfx->m_texCommandBuffer.copyBuffer(stageBuffer, buffer, copy);
	gfx->m_texCommandBuffer.end();

	vk::SubmitInfo submit;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &gfx->m_texCommandBuffer;
	gfx->m_vkQueue.submit(submit);

	gfx->m_vkQueue.waitIdle();
	gfx->m_vkDevice.resetCommandPool(gfx->m_texCommandPool);

	vmaDestroyBuffer(gfx->m_vmaAllocator, stageBuffer, stageAllocation);

	AllocatedBuffer ab;
	ab.buffer = vk::Buffer(buffer);
	ab.allocation = allocation;
	return ab;
}

void UpdateUniformBuffer(VulkanRenderer* gfx, vk::Buffer uniformBuffer, DynamicBuffer<1>& stageBuffer, void* data, size_t length)
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

struct EnhMeshVk
{
	AllocatedBuffer vertices;
	AllocatedBuffer normals;
	std::vector<AllocatedBuffer> texcoords;
	AllocatedBuffer indices;
	uint32_t totVertices = 0, totIndices = 0;

	static EnhMeshVk createEnhMesh(StaticModel* model, VulkanRenderer* gfx) {
		EnhMeshVk enh;
		model->prepare();
		const Mesh& mesh = model->mesh;
		enh.totVertices = 0;
		enh.totIndices = 0;
		for (auto& mat : mesh.groupIndices)
			enh.totVertices += mat.size();

		// prepare buffer content
		// create buffers
		
		std::vector<Vector3> pVertices;
		pVertices.reserve(enh.totVertices);
		for (auto& mat : mesh.groupIndices)
			for (auto& grp : mat)
				pVertices.push_back(*(const Vector3*)(mesh.vertices.data() + 3 * grp.vertex));
		enh.vertices = CreateAndInitializeBuffer(
			gfx, vk::BufferUsageFlagBits::eVertexBuffer, pVertices.data(), enh.totVertices * 12);

		std::vector<uint32_t> pNormals;
		pNormals.reserve(enh.totVertices);
		for (auto& mat : mesh.groupIndices)
			for (auto& grp : mat)
				pNormals.push_back(NormalToR10G10B10A2(Mesh::s_normalTable[mesh.normals[grp.normal]].normal()));
		enh.normals = CreateAndInitializeBuffer(
			gfx, vk::BufferUsageFlagBits::eVertexBuffer, pNormals.data(), enh.totVertices * 4);

		enh.texcoords.resize(mesh.uvLists.size());
		std::vector<float> pUvs;
		for (size_t i = 0; i < mesh.uvLists.size(); i++) {
			auto& uvlist = mesh.uvLists[i];
			pUvs.clear();
			pUvs.reserve(enh.totVertices);
			for (auto& mat : mesh.groupIndices)
				for (auto& grp : mat)
					pUvs.insert(pUvs.end(), { uvlist[2 * grp.uv], uvlist[2 * grp.uv + 1] });
			enh.texcoords[i] = CreateAndInitializeBuffer(
				gfx, vk::BufferUsageFlagBits::eVertexBuffer, pUvs.data(), enh.totVertices * 8);
		}

		std::vector<uint16_t> pIndices;
		for (auto& mat : mesh.polyLists[0].groups) {
			for (auto& tri : mat.tupleIndex) {
				// index offset for mats????????????
				pIndices.insert(pIndices.end(), tri.begin(), tri.end());
			}
			enh.totIndices += 3 * mat.tupleIndex.size();
		}
		enh.indices = CreateAndInitializeBuffer(
			gfx, vk::BufferUsageFlagBits::eIndexBuffer, pIndices.data(), pIndices.size() * 2);

		return enh;
	}
};

struct EnhAnimVk
{
	EnhMeshVk* enhMesh;
	std::vector<uint32_t> animTimes;
	std::vector<AllocatedBuffer> animVertices;
	std::vector<AllocatedBuffer> animNormals;

	static EnhAnimVk createEnhAnim(AnimatedModel* model, VulkanRenderer* gfx) {
		EnhAnimVk enh;

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
			enh.animVertices[frame] = CreateAndInitializeBuffer(
				gfx, vk::BufferUsageFlagBits::eVertexBuffer, pVertices.data(), 12 * pVertices.size());
			enh.animNormals[frame] = CreateAndInitializeBuffer(
				gfx, vk::BufferUsageFlagBits::eVertexBuffer, pNormals.data(), 4 * pNormals.size());
		}
		return enh;
	}
};

struct VulkanEnhancedSceneRenderer::Impl {
	static const size_t MAX_INSTANCES = 16;
	
	std::map<StaticModel*, EnhMeshVk> enhMeshMap;
	std::map<AnimatedModel*, EnhAnimVk> enhAnimMap;

	

	//
	//ComPtr<ID3D11InputLayout> meshIA, animIA;
	//ComPtr<ID3D11VertexShader> meshVS, animVS;
	//ComPtr<ID3D11PixelShader> defPS, alphaPS;
	//ComPtr<ID3D11Buffer> sceneConstBuffer, sunConstBuffer;
	//ComPtr<ID3D11Buffer> instanceBuffer;
	//
	vk::Pipeline scenePipelines[2][2]; // mesh/anim, opaque/alphaTest
	vk::Buffer sceneConstBuffer, sunConstBuffer;
	//
	vk::DescriptorSetLayout m_sceneDescLayout;
	//vk::PipelineLayout m_scenePipelineLayout;
	DynamicBuffer<1> m_instanceBuffer;

	Impl(VulkanRenderer* gfx)
		: m_instanceBuffer(gfx, 64 * MAX_INSTANCES, vk::BufferUsageFlagBits::eVertexBuffer) {}
};

VulkanEnhancedSceneRenderer::~VulkanEnhancedSceneRenderer()
{
	delete _impl;
}

vk::Pipeline CreateScenePipeline(
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

void VulkanEnhancedSceneRenderer::init()
{
	VulkanRenderer* vkgfx = (VulkanRenderer*)gfx;

	_impl = new Impl(vkgfx);

	// ==

	auto vsBlob = vkgfx->loadShader("EnhSceneShader", "VS");
	auto vsAnimBlob = vkgfx->loadShader("EnhSceneShader", "VS_Anim");
	auto psBlob = vkgfx->loadShader("EnhSceneShader", "PS");
	auto psAlphaBlob = vkgfx->loadShader("EnhSceneShader", "PS_Alpha");

	static const vk::VertexInputBindingDescription viBindings_Mesh[] = {
		{0, 12, vk::VertexInputRate::eVertex},
		{1, 4, vk::VertexInputRate::eVertex},
		{2, 8, vk::VertexInputRate::eVertex},
		{3, 64, vk::VertexInputRate::eInstance},
	};

	static const vk::VertexInputAttributeDescription viAttributes_Mesh[] = {
		{0, 0, vk::Format(VK_FORMAT_R32G32B32_SFLOAT), 0},
		{1, 1, vk::Format(VK_FORMAT_A2B10G10R10_UNORM_PACK32), 0},
		{2, 2, vk::Format(VK_FORMAT_R32G32_SFLOAT), 0},
		{3, 3, vk::Format(VK_FORMAT_R32G32B32A32_SFLOAT), 0},
		{4, 3, vk::Format(VK_FORMAT_R32G32B32A32_SFLOAT), 16},
		{5, 3, vk::Format(VK_FORMAT_R32G32B32A32_SFLOAT), 32},
		{6, 3, vk::Format(VK_FORMAT_R32G32B32A32_SFLOAT), 48},
	};

	vk::PipelineVertexInputStateCreateInfo viStateInfo_Mesh;
	viStateInfo_Mesh.vertexBindingDescriptionCount = std::size(viBindings_Mesh);
	viStateInfo_Mesh.pVertexBindingDescriptions = viBindings_Mesh;
	viStateInfo_Mesh.vertexAttributeDescriptionCount = std::size(viAttributes_Mesh);
	viStateInfo_Mesh.pVertexAttributeDescriptions = viAttributes_Mesh;

	//

	static const vk::VertexInputBindingDescription viBindings_Anim[] = {
		{0, 12, vk::VertexInputRate::eVertex},
		{1, 12, vk::VertexInputRate::eVertex},
		{2, 4, vk::VertexInputRate::eVertex},
		{3, 4, vk::VertexInputRate::eVertex},
		{4, 8, vk::VertexInputRate::eVertex},
		{5, 64, vk::VertexInputRate::eInstance},
	};

	static const vk::VertexInputAttributeDescription viAttributes_Anim[] = {
		{0, 0, vk::Format(VK_FORMAT_R32G32B32_SFLOAT), 0},
		{1, 1, vk::Format(VK_FORMAT_R32G32B32_SFLOAT), 0},
		{2, 2, vk::Format(VK_FORMAT_A2B10G10R10_UNORM_PACK32), 0},
		{3, 3, vk::Format(VK_FORMAT_A2B10G10R10_UNORM_PACK32), 0},
		{4, 4, vk::Format(VK_FORMAT_R32G32_SFLOAT), 0},
		{5, 5, vk::Format(VK_FORMAT_R32G32B32A32_SFLOAT), 0},
		{6, 5, vk::Format(VK_FORMAT_R32G32B32A32_SFLOAT), 16},
		{7, 5, vk::Format(VK_FORMAT_R32G32B32A32_SFLOAT), 32},
		{8, 5, vk::Format(VK_FORMAT_R32G32B32A32_SFLOAT), 48},
	};

	vk::PipelineVertexInputStateCreateInfo viStateInfo_Anim;
	viStateInfo_Anim.vertexBindingDescriptionCount = std::size(viBindings_Anim);
	viStateInfo_Anim.pVertexBindingDescriptions = viBindings_Anim;
	viStateInfo_Anim.vertexAttributeDescriptionCount = std::size(viAttributes_Anim);
	viStateInfo_Anim.pVertexAttributeDescriptions = viAttributes_Anim;

	//

	_impl->scenePipelines[0][0] = CreateScenePipeline(vkgfx, vsBlob, "VS", psBlob, "PS", viStateInfo_Mesh);
	_impl->scenePipelines[1][0] = CreateScenePipeline(vkgfx, vsAnimBlob, "VS_Anim", psBlob, "PS", viStateInfo_Anim);
	_impl->scenePipelines[0][1] = CreateScenePipeline(vkgfx, vsBlob, "VS", psAlphaBlob, "PS_Alpha", viStateInfo_Mesh);
	_impl->scenePipelines[1][1] = CreateScenePipeline(vkgfx, vsAnimBlob, "VS_Anim", psAlphaBlob, "PS_Alpha", viStateInfo_Anim);
}

void VulkanEnhancedSceneRenderer::render()
{
	VulkanRenderer* vkgfx = (VulkanRenderer*)gfx;

	gfx->BeginMeshDrawing();
	gfx->BeginBatchDrawing();
	
	//vkgfx->togglePass(m_pip)

	// Sun constant buffer
	char sunData[16];
	*(Vector3*)(sunData + 0) = scene->sunDirection.normal();
	*(int*)(sunData + 12) = 0;
	UpdateUniformBuffer(vkgfx, vkgfx->m_currentSunBuffer, vkgfx->m_globalBuffers->sunBuffer, sunData, 16);

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
		}
		if (cur_texture != next_texture)
			gfx->SetTexture(0, next_texture);
		cur_alphatest = next_alphatest;
		cur_texture = next_texture;

		auto* model = it.first.second;
		auto* staticModel = model->getStaticModel();
		
		auto mt = _impl->enhMeshMap.find(staticModel);
		const EnhMeshVk* enhMesh;
		if (mt == _impl->enhMeshMap.end()) {
			_impl->enhMeshMap[staticModel] = EnhMeshVk::createEnhMesh(staticModel, vkgfx);
			enhMesh = &_impl->enhMeshMap[staticModel];
		}
		else {
			enhMesh = &mt->second;
		}

		const EnhAnimVk* enhAnim = nullptr;
		if (model != staticModel) {
			auto* animModel = (AnimatedModel*)model;
			auto at = _impl->enhAnimMap.find(animModel);
			if (at == _impl->enhAnimMap.end()) {
				_impl->enhAnimMap[animModel] = EnhAnimVk::createEnhAnim(animModel, vkgfx);
				enhAnim = &_impl->enhAnimMap[animModel];
			}
			else {
				enhAnim = &at->second;
			}
		}

		bool next_isanim = (bool)enhAnim;
		if (next_isanim != cur_isanim) {
			cur_isanim = next_isanim;
		}

		auto nextPipeline = _impl->scenePipelines[next_isanim][next_alphatest];

		auto& frame = vkgfx->currentFrameObject();


		int prevFrame = -1;
		float prevAnimLerp = -1.0f;
		int prevColor = -1;
		uint32_t prevFlags = -1;
		std::vector<Matrix> instTransforms;

		auto flush = [&]() {
			if (instTransforms.empty()) return;

			_impl->m_instanceBuffer.nextBuffer();
			auto* stage = _impl->m_instanceBuffer.getCurrentBuffer();
			memcpy(stage->mappedPtr[0], instTransforms.data(), instTransforms.size() * 64);


			char sceneBufferBytes[16];
			*(uint32_t*)(sceneBufferBytes+0) = prevFlags;
			*(float*)(sceneBufferBytes+4) = prevAnimLerp;
			//UpdateUniformBuffer(vkgfx, vkgfx->m_currentSceneBuffer, vkgfx->m_globalBuffers->sceneBuffer, sceneBufferBytes, 16);
			frame.mainCommandBuffer.pushConstants(vkgfx->m_vkPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, 8, sceneBufferBytes);

			vkgfx->togglePass(nextPipeline);

			if (enhAnim) {
				auto* animModel = (AnimatedModel*)model;

				vk::Buffer iaBuffers[6] = {
					enhAnim->animVertices[prevFrame].buffer,
					enhAnim->animVertices[prevFrame + 1].buffer,
					enhAnim->animNormals[prevFrame].buffer,
					enhAnim->animNormals[prevFrame + 1].buffer,
					enhMesh->texcoords[prevColor].buffer,
					stage->buffer[0],
				};

				static const vk::DeviceSize iaOffsets[std::size(iaBuffers)] = { 0,0,0,0,0,0 };

				frame.mainCommandBuffer.bindVertexBuffers(0, iaBuffers, iaOffsets);
				frame.mainCommandBuffer.bindIndexBuffer(enhMesh->indices.buffer, 0, vk::IndexType::eUint16);
			}
			else {
				vk::Buffer iaBuffers[4] = {
					enhMesh->vertices.buffer,
					enhMesh->normals.buffer,
					enhMesh->texcoords[prevColor].buffer,
					stage->buffer[0],
				};

				static const vk::DeviceSize iaOffsets[std::size(iaBuffers)] = { 0,0,0,0 };

				frame.mainCommandBuffer.bindVertexBuffers(0, iaBuffers, iaOffsets);
				frame.mainCommandBuffer.bindIndexBuffer(enhMesh->indices.buffer, 0, vk::IndexType::eUint16);
			}

			PolygonList& polylist = staticModel->mesh.polyLists[0];
			uint32_t startIndex = 0, startVertex = 0;
			for (size_t g = 0; g < polylist.groups.size(); g++) {
				uint32_t numIndices = 3 * polylist.groups[g].tupleIndex.size();
				uint32_t numVertices = staticModel->mesh.groupIndices[g].size();
				if (staticModel->matIds[g] == it.first.first) {
					frame.mainCommandBuffer.drawIndexed(numIndices, instTransforms.size(), startIndex, startVertex, 0);
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
			instTransforms.push_back(ent->transform);
		}
		flush();
	}

	//dimm->IASetInputLayout(((D3D11Renderer*)gfx)->ddInputLayout);

}

#endif // _WIN32
