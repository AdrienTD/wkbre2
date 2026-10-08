#pragma once

#include "renderer.h"

#include "../util/DynArray.h"

#include <map>
#include <optional>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define VULKAN_HPP_NO_SETTERS
#include <vulkan/vulkan.hpp>

#ifdef _WIN32
#include <vma/vk_mem_alloc.h>
#else
#ifdef __ANDROID__
#define VMA_VULKAN_VERSION 1001000
#endif
#include <vk_mem_alloc.h>
#endif

struct RIndexBufferVulkan;
struct RVertexBufferVulkan;
struct VulkanRenderer;

static constexpr int MAX_SWAPCHAIN_IMAGES = 6;

template<int NumSubs = 1>
struct DynamicBuffer {
	VulkanRenderer* const gfx;

	struct BufferInstance {
		vk::Buffer buffer[NumSubs];
		VmaAllocation allocation[NumSubs];
		void* mappedPtr[NumSubs];
	};
	struct Frame {
		std::vector<BufferInstance> buffers;
	};
	std::array<Frame, MAX_SWAPCHAIN_IMAGES> frames;
	int lastBuffer = -1;

	VmaAllocationCreateInfo allocationCreateInfo;
	vk::BufferCreateInfo bufferCreateInfo[NumSubs];

	DynamicBuffer(VulkanRenderer* gfx) : gfx(gfx)
	{
		memset(&allocationCreateInfo, 0, sizeof(allocationCreateInfo));
		allocationCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT
			| VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

		allDynamicBuffers.push_back(this);
	}
	DynamicBuffer(VulkanRenderer* gfx, size_t bufferSize, vk::BufferUsageFlags usage)
		: DynamicBuffer(gfx)
	{
		this->setSubbufferInfo<0>(bufferSize, usage);
	}
	~DynamicBuffer() {
		reset();
		const auto it = std::find(allDynamicBuffers.begin(), allDynamicBuffers.end(), this);
		allDynamicBuffers.erase(it);
	}

	template<int I>
	void setSubbufferInfo(size_t bufferSize, vk::BufferUsageFlags usage)
	{
		static_assert(I >= 0 && I < NumSubs);
		bufferCreateInfo[I].size = bufferSize;
		bufferCreateInfo[I].usage = usage;
		bufferCreateInfo[I].sharingMode = vk::SharingMode::eExclusive;
	}

	// once received, the fence HAS TO be given to a queue submission ASAP
	// otherwise calling reset() (by destructor) will deadlock
	const BufferInstance* getCurrentBuffer();

	void nextBuffer();

	static void onFrameBegin() {
		for (auto* dynBuf : allDynamicBuffers)
			dynBuf->lastBuffer = -1;
	}

private:
	void addNewBuffer();
	void reset();

	inline static std::vector<DynamicBuffer*> allDynamicBuffers;
};

struct VulkanRenderer : IRenderer {

	vk::Instance m_vkInstance;
	vk::PhysicalDevice m_vkPhysicalDevice;
	vk::Device m_vkDevice;
	int m_queueFamilyIndex = -1;
	vk::Queue m_vkQueue;

	vk::Format m_surfaceFormat;
	vk::ColorSpaceKHR m_surfaceColorSpace;
	vk::SurfaceKHR m_vkSurface = nullptr;
	vk::SwapchainKHR m_vkSwapchain = nullptr;

	struct SwapchainImage {
		vk::Image image;
		vk::ImageView imageView;
		VmaAllocation depthAllocation;
		vk::Image depthImage;
		vk::ImageView depthImageView;
		vk::Framebuffer framebuffer;

		vk::Semaphore swapchainSemaphore;
		vk::Semaphore commandSemaphore;

		vk::Fence fence;
		vk::CommandPool commandPool;
		vk::CommandBuffer mainCommandBuffer;
		std::vector<std::tuple<vk::Buffer, VmaAllocation, std::unique_ptr<DynamicBuffer<1>>>> buffersToDelete;

		SwapchainImage() = default;
		SwapchainImage(const SwapchainImage&) = delete;
		SwapchainImage(SwapchainImage&&) = default;
	};
	std::vector<SwapchainImage> m_vkSwapchainImages;
	uint32_t m_currentSwapchainImageIndex = 0;
	int m_surfaceWidth = 800, m_surfaceHeight = 600;

	std::vector<vk::Semaphore> m_swapchainSemaphorePool;
	int m_nextSwapchainSemaphoreIndex = 0;

	vk::CommandPool m_texCommandPool;
	vk::CommandBuffer m_texCommandBuffer;

	vk::RenderPass m_vkRenderPass;

	vk::PipelineCache m_vkPipelineCache;
	vk::Pipeline m_pipeline2D, m_pipeline2DLines;
	vk::Pipeline m_pipeline3D, m_pipeline3DAlphaTest, m_pipeline3DAlphaBlend, m_pipeline3DAlphaBlendLines;
	vk::Pipeline m_pipelineTerrain, m_pipelineLake;

	vk::Sampler m_vkSampler;
	vk::DescriptorSetLayout m_vkDescSetLayout0, m_vkDescSetLayout1;
	vk::PipelineLayout m_vkPipelineLayout;
	vk::DescriptorPool m_vkDescriptorPool;
	vk::DescriptorSet m_vkMainDescriptorSet;

	VmaAllocator m_vmaAllocator;

	// Constant buffers
	struct GlobalBuffers {
		DynamicBuffer<1> shapeVertexBuffer;
		DynamicBuffer<1> transformBuffer;
		DynamicBuffer<1> fogBuffer;
		DynamicBuffer<1> sunBuffer;
		DynamicBuffer<1> sceneBuffer;
		GlobalBuffers(VulkanRenderer* gfx)
			: shapeVertexBuffer(gfx, 64 * sizeof(batchVertex), vk::BufferUsageFlagBits::eVertexBuffer),
			transformBuffer(gfx, 64, vk::BufferUsageFlagBits::eTransferSrc),
			fogBuffer(gfx, 32, vk::BufferUsageFlagBits::eTransferSrc),
			sunBuffer(gfx, 16, vk::BufferUsageFlagBits::eTransferSrc),
			sceneBuffer(gfx, 16, vk::BufferUsageFlagBits::eTransferSrc)
		{
		}
		GlobalBuffers(const GlobalBuffers&) = delete;
	};
	std::unique_ptr<GlobalBuffers> m_globalBuffers;
	vk::Buffer m_currentTransformBuffer; VmaAllocation m_currentTransformBufferAlloc;
	vk::Buffer m_currentFogBuffer; VmaAllocation m_currentFogBufferAlloc;
	vk::Buffer m_currentSunBuffer; VmaAllocation m_currentSunBufferAlloc;
	vk::Buffer m_currentSceneBuffer; VmaAllocation m_currentSceneBufferAlloc;
	std::map<vk::ImageView, vk::Image> m_imageViewToImageMap;
	std::map<vk::ImageView, vk::DescriptorSet> m_imageViewToDescriptorSetMap;

	texture m_whiteTexture;
	int m_msaaNumSamples = 1;

	// State
	bool m_fogEnabled = false;
	RVertexBufferVulkan* m_currentVertexBuffer = nullptr;
	RIndexBufferVulkan* m_currentIndexBuffer = nullptr;
	vk::Viewport m_viewport;
	vk::Rect2D m_scissorRect;
	vk::PrimitiveTopology m_primitiveTopology = vk::PrimitiveTopology::eTriangleList;
	vk::Pipeline m_currentPipeline = nullptr;
	vk::DescriptorSet m_currentTextureDescriptorSet = nullptr;

	std::optional<uint32_t> m_clearColor = 0;
	bool m_clearDepth = true;

	vk::Pipeline m_currentPassPipeline;

	auto& currentFrameObject() {
		return m_vkSwapchainImages[m_currentSwapchainImageIndex];
	}

	vk::ShaderModule loadShader(const char* name, const char* func);

	void setUniformDescriptors(std::vector<vk::Buffer> buffers)
	{
		std::vector<vk::DescriptorBufferInfo> dbInfos;
		std::vector<vk::WriteDescriptorSet> writes;

		dbInfos.resize(buffers.size());
		writes.resize(buffers.size());

		for (size_t i = 0; i < buffers.size(); ++i) {
			dbInfos[i].buffer = buffers[i];
			dbInfos[i].offset = 0;
			dbInfos[i].range = VK_WHOLE_SIZE;

			auto& wds = writes[i];
			wds.dstSet = m_vkMainDescriptorSet;
			wds.dstBinding = i;
			wds.dstArrayElement = 0;
			wds.descriptorCount = 1;
			wds.descriptorType = vk::DescriptorType::eUniformBuffer;
			wds.pBufferInfo = &dbInfos[i];
		}
	
		m_vkDevice.updateDescriptorSets(writes.size(), writes.data(), 0, nullptr);
	}

	void togglePass(vk::Pipeline pipeline)
	{
		if (m_primitiveTopology == vk::PrimitiveTopology::eLineList) {
			if (pipeline == m_pipeline2D)
				pipeline = m_pipeline2DLines;
			else if (pipeline == m_pipeline3DAlphaBlend)
				pipeline = m_pipeline3DAlphaBlendLines;
			else
				assert(false && "No line variant for the current pipeline");
		}

		auto cmdBuffer = currentFrameObject().mainCommandBuffer;

		if (pipeline != m_currentPassPipeline)
		{
			if (!pipeline) {
				cmdBuffer.endRenderPass();
			}
			else if (!m_currentPassPipeline) {
				vk::RenderPassBeginInfo rpbi;
				rpbi.renderPass = m_vkRenderPass;
				rpbi.framebuffer = m_vkSwapchainImages[m_currentSwapchainImageIndex].framebuffer;
				rpbi.renderArea.offset.x = 0;
				rpbi.renderArea.offset.y = 0;
				rpbi.renderArea.extent.width = m_surfaceWidth;
				rpbi.renderArea.extent.height = m_surfaceHeight;
				rpbi.clearValueCount = 0;
				rpbi.pClearValues = nullptr;

				cmdBuffer.beginRenderPass(rpbi, vk::SubpassContents::eInline);
			}
		}

		if (pipeline) {
			vk::DescriptorSet descSets[2] = { m_vkMainDescriptorSet, m_currentTextureDescriptorSet };

			cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
			cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_vkPipelineLayout, 0, descSets, {});
			cmdBuffer.setViewport(0, m_viewport);
			cmdBuffer.setScissor(0, m_scissorRect);
		}

		m_currentPassPipeline = pipeline;
	}

	// Initialisation
	virtual void Init() override;

	virtual void Reset() override;

	// Frame begin/end

	virtual void BeginDrawing() override;

	virtual void EndDrawing() override;
	virtual void ClearFrame(bool clearColors = true, bool clearDepth = true, uint32_t color = 0) override;

	// Textures management
	virtual texture CreateTexture(const Bitmap& bm, int mipmaps) override;
	virtual void FreeTexture(texture t) override;
	virtual void UpdateTexture(texture t, const Bitmap& bmp) override;

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
	virtual void BeginMeshDrawing() override;

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
