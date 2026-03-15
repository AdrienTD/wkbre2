// wkbre2 - WK Engine Reimplementation
// (C) 2026 AdrienTD
// Licensed under the GNU General Public License 3

#include "renderer.h"
#include "../window.h"
#include "../file.h"
#include "../util/util.h"
#include "bitmap.h"
#include "../settings.h"

#include <array>
#include <cassert>
#include <chrono>
#include <deque>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

#include <nlohmann/json.hpp>

#define WIN32_LEAN_AND_MEAN
#define VULKAN_HPP_NO_SETTERS
#include <vulkan/vulkan.hpp>
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#include <SDL.h>
#include <SDL_syswm.h>
#include <SDL_system.h>
#include <SDL_vulkan.h>
extern SDL_Window* g_sdlWindow;

struct VulkanRenderer;
struct RVertexBufferVulkan;
struct RIndexBufferVulkan;

template<int NumSubs = 1>
struct DynamicBuffer {
	VulkanRenderer* const gfx;

	struct BufferInstance {
		vk::Fence fence;
		vk::Buffer buffer[NumSubs];
		VmaAllocation allocation[NumSubs];
		void* mappedPtr[NumSubs];
	};
	std::vector<BufferInstance> buffers;
	int lastBuffer = 0;

	VmaAllocationCreateInfo allocationCreateInfo;
	vk::BufferCreateInfo bufferCreateInfo[NumSubs];

	DynamicBuffer(VulkanRenderer* gfx) : gfx(gfx)
	{
		memset(&allocationCreateInfo, 0, sizeof(allocationCreateInfo));
		allocationCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT
			| VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
	}
	DynamicBuffer(VulkanRenderer* gfx, size_t bufferSize, vk::BufferUsageFlags usage)
		: DynamicBuffer(gfx)
	{
		this->setSubbufferInfo<0>(bufferSize, usage);
	}
	~DynamicBuffer() { reset(); }

	template<int I>
	void setSubbufferInfo(size_t bufferSize, vk::BufferUsageFlags usage)
	{
		static_assert(I >= 0 && I < NumSubs);
		bufferCreateInfo[I].size = bufferSize;
		bufferCreateInfo[I].usage = usage;
		bufferCreateInfo[I].sharingMode = vk::SharingMode::eExclusive;
	}

	void addNewBuffer()
	{
		auto& buffer = buffers.emplace_back();
		vk::FenceCreateInfo fci;
		buffer.fence = gfx->m_vkDevice.createFence(fci);
		for (int i = 0; i < NumSubs; ++i) {
			VkBuffer vkbuf;
			VmaAllocation alloc;
			VmaAllocationInfo allocInfo;
			auto result = vmaCreateBuffer(gfx->m_vmaAllocator, bufferCreateInfo[i],
				&allocationCreateInfo, &vkbuf, &alloc, &allocInfo);
			assert(result == VK_SUCCESS);
			buffer.buffer[i] = vkbuf;
			buffer.allocation[i] = alloc;
			buffer.mappedPtr[i] = allocInfo.pMappedData;
			assert(buffer.mappedPtr);
		}
	}

	void reset()
	{
		for (auto& inst : buffers) {
			auto res = gfx->m_vkDevice.waitForFences(1, &inst.fence, VK_TRUE, 10'000'000'000u);
			assert(res == vk::Result::eSuccess);
			gfx->m_vkDevice.destroyFence(inst.fence);
			for(int i = 0; i < NumSubs; ++i)
				vmaDestroyBuffer(gfx->m_vmaAllocator, inst.buffer[i], inst.allocation[i]);
		}
		buffers.clear();
	}

	// once received, the fence HAS TO be given to a queue submission ASAP
	// otherwise calling reset() (by destructor) will deadlock
	const BufferInstance* getCurrentBuffer()
	{
		if (buffers.empty())
			nextBuffer();
		return &buffers[lastBuffer];
	}

	void nextBuffer()
	{
		for (size_t i = 0; i < buffers.size(); ++i) {
			size_t b = (lastBuffer + i) % buffers.size();
			auto result = gfx->m_vkDevice.getFenceStatus(buffers[b].fence);
			if (result == vk::Result::eSuccess) {
				// signaled -> done, can be reused
				auto result = gfx->m_vkDevice.resetFences(1, &buffers[b].fence);
				assert(result == vk::Result::eSuccess);
				lastBuffer = b;
				return;
			}
			else {
				// unsignaled -> being used
			}
		}
		// all buffers are occupied -> create a new one
		addNewBuffer();
		lastBuffer = buffers.size() - 1;
	}
};

struct VulkanRenderer : IRenderer {

	vk::Instance m_vkInstance;
	vk::PhysicalDevice m_vkPhysicalDevice;
	vk::Device m_vkDevice;
	vk::CommandPool m_vkCommandPool;
	int m_queueFamilyIndex = -1;
	vk::Queue m_vkQueue;
	vk::Semaphore m_mainSemaphore;

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
	};
	std::vector<SwapchainImage> m_vkSwapchainImages;
	uint32_t m_currentSwapchainImageIndex;
	vk::Semaphore m_swapchainSemaphore;
	vk::Fence m_swapchainFence;
	int m_surfaceWidth = 800, m_surfaceHeight = 600;

	vk::RenderPass m_vkRenderPass;

	vk::PipelineCache m_vkPipelineCache;
	vk::Pipeline m_pipeline2D, m_pipeline2DLines;
	vk::Pipeline m_pipeline3D, m_pipeline3DAlphaTest, m_pipeline3DAlphaBlend, m_pipeline3DAlphaBlendLines;
	vk::Pipeline m_pipelineTerrain, m_pipelineLake;

	vk::Sampler m_vkSampler;
	vk::DescriptorSetLayout m_vkDescSetLayout;
	vk::PipelineLayout m_vkPipelineLayout;
	vk::DescriptorPool m_vkDescriptorPool;
	vk::DescriptorSet m_vkMainDescriptorSet;

	VmaAllocator m_vmaAllocator;

	//ID3D11RenderTargetView* ddRenderTargetView = nullptr;

	//ID3D11InputLayout* ddInputLayout;
	//ID3D11VertexShader* ddVertexShader, * ddFogVertexShader;
	//ID3D11PixelShader* ddPixelShader, * ddAlphaTestPixelShader, * ddMapPixelShader, * ddLakePixelShader;

	// Shapes
	//ID3D11BlendState* alphaBlendState = nullptr;

	// Depth
	//ID3D11Texture2D* depthBuffer = nullptr;
	//ID3D11DepthStencilView* depthView = nullptr;
	//ID3D11DepthStencilState* depthOnState = nullptr;
	//ID3D11DepthStencilState* depthOffState = nullptr;
	//ID3D11DepthStencilState* depthTestOnlyState = nullptr;

	// Constant buffers
	struct GlobalBuffers {
		DynamicBuffer<1> shapeVertexBuffer;
		DynamicBuffer<1> transformBuffer;
		DynamicBuffer<1> fogBuffer;
		GlobalBuffers(VulkanRenderer* gfx)
			: shapeVertexBuffer(gfx, 64 * sizeof(batchVertex), vk::BufferUsageFlagBits::eVertexBuffer),
			transformBuffer(gfx, 64, vk::BufferUsageFlagBits::eTransferSrc),
			fogBuffer(gfx, 32, vk::BufferUsageFlagBits::eTransferSrc)
		{
		}
	};
	std::unique_ptr<GlobalBuffers> globalBuffers;
	vk::Buffer m_currentTransformBuffer; VmaAllocation m_currentTransformBufferAlloc;
	vk::Buffer m_currentFogBuffer; VmaAllocation m_currentFogBufferAlloc;
	std::map<vk::ImageView, vk::Image> m_imageViewToImageMap;

	texture whiteTexture;
	int msaaNumSamples = 1;

	// State
	bool fogEnabled = false;
	RVertexBufferVulkan* currentVertexBuffer = nullptr;
	RIndexBufferVulkan* currentIndexBuffer = nullptr;
	vk::Fence m_nonDynamicBufferFence;
	vk::Viewport m_viewport;
	vk::Rect2D m_scissorRect;
	vk::PrimitiveTopology m_primitiveTopology = vk::PrimitiveTopology::eTriangleList;
	vk::Pipeline m_currentPipeline = nullptr;

	std::deque<vk::CommandBuffer> activeCommandBuffers;

	vk::ShaderModule loadShader(const char* name, const char* func);

	vk::CommandBuffer createCommandBufferAndBegin()
	{
		vk::CommandBufferAllocateInfo cbaInfo;
		cbaInfo.commandPool = m_vkCommandPool;
		cbaInfo.level = vk::CommandBufferLevel::ePrimary;
		cbaInfo.commandBufferCount = 1;
		vk::CommandBuffer cmdBuffer = m_vkDevice.allocateCommandBuffers(cbaInfo).at(0);
		vk::CommandBufferBeginInfo beginInfo;
		beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
		cmdBuffer.begin(beginInfo);
		activeCommandBuffers.push_back(cmdBuffer);
		return cmdBuffer;
	}

	void submitSingleCommandBuffer(vk::CommandBuffer buffer, vk::Fence fence = nullptr)
	{
		vk::SubmitInfo submit;
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &buffer;
		//submit.signalSemaphoreCount = 1;
		//submit.pSignalSemaphores = &m_mainSemaphore;
		//submit.waitSemaphoreCount = 1;
		//submit.pWaitSemaphores = &m_mainSemaphore;
		m_vkQueue.submit(submit, fence);
	}

	void submitSignalSemaphore(vk::Semaphore semaphore)
	{
		vk::SubmitInfo submit;
		submit.signalSemaphoreCount = 1;
		submit.pSignalSemaphores = &semaphore;
		m_vkQueue.submit(submit);
	}

	void setDescriptors(std::optional<vk::Buffer> transformBuffer, std::optional<vk::Buffer> fogBuffer, std::optional<vk::ImageView> texture) {
		vk::DescriptorBufferInfo dbInfoTransform;
		vk::DescriptorBufferInfo dbInfoFog;
		vk::DescriptorImageInfo dbInfoTexture;

		vk::WriteDescriptorSet writes[3];
		int index = 0;
		if (transformBuffer) {
			dbInfoTransform.buffer = *transformBuffer;
			dbInfoTransform.offset = 0;
			dbInfoTransform.range = VK_WHOLE_SIZE;

			auto& wds = writes[index++];
			wds.dstSet = m_vkMainDescriptorSet;
			wds.dstBinding = 0;
			wds.dstArrayElement = 0;
			wds.descriptorCount = 1;
			wds.descriptorType = vk::DescriptorType::eUniformBuffer;
			wds.pBufferInfo = &dbInfoTransform;
		}
		if (fogBuffer) {
			dbInfoFog.buffer = *fogBuffer;
			dbInfoFog.offset = 0;
			dbInfoFog.range = VK_WHOLE_SIZE;

			auto& wds = writes[index++];
			wds.dstSet = m_vkMainDescriptorSet;
			wds.dstBinding = 1;
			wds.dstArrayElement = 0;
			wds.descriptorCount = 1;
			wds.descriptorType = vk::DescriptorType::eUniformBuffer;
			wds.pBufferInfo = &dbInfoFog;
		}
		if (texture) {
			dbInfoTexture.imageView = *texture;
			dbInfoTexture.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

			auto& wds = writes[index++];
			wds.dstSet = m_vkMainDescriptorSet;
			wds.dstBinding = 2;
			wds.dstArrayElement = 0;
			wds.descriptorCount = 1;
			wds.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			wds.pImageInfo = &dbInfoTexture;
		}

		m_vkQueue.waitIdle();
		m_vkDevice.updateDescriptorSets(index, writes, 0, nullptr);
	}

	void beginPass(vk::CommandBuffer cmdBuffer, vk::Pipeline pipeline = nullptr)
	{
		if (pipeline == nullptr)
			pipeline = m_currentPipeline;
		assert(pipeline != nullptr);

		if (m_primitiveTopology == vk::PrimitiveTopology::eLineList) {
			if (pipeline == m_pipeline2D)
				pipeline = m_pipeline2DLines;
			else if (pipeline == m_pipeline3DAlphaBlend)
				pipeline = m_pipeline3DAlphaBlendLines;
			else
				assert(false && "No line variant for the current pipeline");
		}

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
		cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_vkPipelineLayout, 0, m_vkMainDescriptorSet, {});
		cmdBuffer.setViewport(0, m_viewport);
		cmdBuffer.setScissor(0, m_scissorRect);
	}

	void endPass(vk::CommandBuffer cmdBuffer)
	{
		cmdBuffer.endRenderPass();
	}

	void nextFrame()
	{
		auto nextImageRes = m_vkDevice.acquireNextImageKHR(m_vkSwapchain, UINT64_MAX, m_swapchainSemaphore, {});
		assert((int)nextImageRes.result >= 0);
		m_currentSwapchainImageIndex = nextImageRes.value;
		
		vk::ImageMemoryBarrier imgBarrier0;
		imgBarrier0.srcAccessMask = vk::AccessFlagBits::eNone;
		imgBarrier0.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
		imgBarrier0.oldLayout = vk::ImageLayout::eUndefined;
		imgBarrier0.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
		imgBarrier0.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imgBarrier0.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imgBarrier0.image = m_vkSwapchainImages[m_currentSwapchainImageIndex].image;
		imgBarrier0.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		imgBarrier0.subresourceRange.baseArrayLayer = 0;
		imgBarrier0.subresourceRange.baseMipLevel = 0;
		imgBarrier0.subresourceRange.layerCount = 1;
		imgBarrier0.subresourceRange.levelCount = 1;

		vk::ImageMemoryBarrier imgBarrier1 = imgBarrier0;
		imgBarrier1.srcAccessMask = vk::AccessFlagBits::eNone;
		imgBarrier1.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
		imgBarrier1.oldLayout = vk::ImageLayout::eUndefined;
		imgBarrier1.newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
		imgBarrier1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imgBarrier1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imgBarrier1.image = m_vkSwapchainImages[m_currentSwapchainImageIndex].depthImage;
		imgBarrier1.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;

		vk::CommandBuffer cmdBuffer = createCommandBufferAndBegin();
		cmdBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::DependencyFlags(),
			0, nullptr, 0, nullptr, 1, &imgBarrier0);
		cmdBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eEarlyFragmentTests, vk::DependencyFlags(),
			0, nullptr, 0, nullptr, 1, &imgBarrier1);
		cmdBuffer.end();

		const vk::PipelineStageFlags stageToWaitOn[1] = { vk::PipelineStageFlagBits::eBottomOfPipe };

		vk::SubmitInfo submit;
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &cmdBuffer;
		submit.waitSemaphoreCount = 1;
		submit.pWaitSemaphores = &m_swapchainSemaphore;
		submit.pWaitDstStageMask = stageToWaitOn;
		m_vkDevice.resetFences(m_swapchainFence);
		m_vkQueue.submit(submit, m_swapchainFence);
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

struct RGeneralBufferVulkan
{
	VulkanRenderer* const gfx;
	vk::Buffer buffer;
	VmaAllocation allocation;
	void* mappedPtr;
	RGeneralBufferVulkan(VulkanRenderer* gfx, int size, vk::BufferUsageFlags usage) : gfx(gfx)
	{
		VmaAllocationCreateInfo allocationCreateInfo;
		memset(&allocationCreateInfo, 0, sizeof(allocationCreateInfo));
		allocationCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT
			| VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

		vk::BufferCreateInfo bufferCreateInfo;
		bufferCreateInfo.size = size;
		bufferCreateInfo.usage = usage;
		bufferCreateInfo.sharingMode = vk::SharingMode::eExclusive;

		VmaAllocationInfo allocInfo;
		VkBuffer tbuffer;
		vmaCreateBuffer(gfx->m_vmaAllocator, bufferCreateInfo, &allocationCreateInfo, &tbuffer, &allocation, &allocInfo);
		buffer = vk::Buffer(tbuffer);
		mappedPtr = allocInfo.pMappedData;
	}
};

struct RVertexBufferVulkan : public RVertexBuffer, RGeneralBufferVulkan
{
	RVertexBufferVulkan(VulkanRenderer* gfx, int size)
		: RGeneralBufferVulkan(gfx, size, vk::BufferUsageFlagBits::eVertexBuffer) {}

	batchVertex* lock()
	{
		auto res = gfx->m_vkDevice.waitForFences(1, &gfx->m_nonDynamicBufferFence, vk::True, 10'000'000'000u);
		assert(res == vk::Result::eSuccess);
		return (batchVertex*)mappedPtr;
	}
	void unlock()
	{
		vmaFlushAllocation(gfx->m_vmaAllocator, allocation, 0, size);
	}
};

struct RIndexBufferVulkan : public RIndexBuffer, RGeneralBufferVulkan
{
	RIndexBufferVulkan(VulkanRenderer* gfx, int size)
		: RGeneralBufferVulkan(gfx, size, vk::BufferUsageFlagBits::eIndexBuffer) {
	}

	uint16_t* lock()
	{
		auto res = gfx->m_vkDevice.waitForFences(1, &gfx->m_nonDynamicBufferFence, vk::True, 10'000'000'000u);
		assert(res == vk::Result::eSuccess);
		return (uint16_t*)mappedPtr;
	}
	void unlock()
	{
		vmaFlushAllocation(gfx->m_vmaAllocator, allocation, 0, size);
	}
};

struct RBatchVulkan : public RBatch
{
	VulkanRenderer* gfx;
	DynamicBuffer<2> dynamicBuffer;
	bool locked = false;

	RBatchVulkan(VulkanRenderer* gfx, int maxVertices, int maxIndices) :
		gfx(gfx),
		dynamicBuffer(gfx)
	{
		dynamicBuffer.setSubbufferInfo<0>(maxVertices * sizeof(batchVertex), vk::BufferUsageFlagBits::eVertexBuffer);
		dynamicBuffer.setSubbufferInfo<1>(maxIndices * 2, vk::BufferUsageFlagBits::eIndexBuffer);
		this->curverts = this->curindis = 0;
		this->maxverts = maxVertices; this->maxindis = maxIndices;
	}

	~RBatchVulkan()
	{
		if (locked) unlock();
	}

	void lock()
	{
		locked = true;
		dynamicBuffer.nextBuffer();
	}
	void unlock()
	{
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
		const auto& curBuffer = dynamicBuffer.getCurrentBuffer();
		*vpnt = (batchVertex*)curBuffer->mappedPtr[0] + curverts;
		*ipnt = (uint16_t*)curBuffer->mappedPtr[1] + curindis;
		*fi = curverts;

		curverts += nverts; curindis += nindis;
	}

	void flush()
	{
		if (locked) unlock();
		if (!curverts) return;

		auto* currentBuf = dynamicBuffer.getCurrentBuffer();
		vk::DeviceSize offset = 0;
		
		auto cmdBuffer = gfx->createCommandBufferAndBegin();
		//auto pipeline = gfx->m_primitiveTopology == vk::PrimitiveTopology::eLineList ? gfx->m_pipeline2DLines : gfx->m_pipeline2D;
		gfx->beginPass(cmdBuffer);
		cmdBuffer.bindVertexBuffers(0, 1, &currentBuf->buffer[0], &offset);
		cmdBuffer.bindIndexBuffer(currentBuf->buffer[1], 0, vk::IndexType::eUint16);
		cmdBuffer.drawIndexed(curindis, 1, 0, 0, 0);
		gfx->endPass(cmdBuffer);
		cmdBuffer.end();

		// submit to queue
		gfx->submitSingleCommandBuffer(cmdBuffer, currentBuf->fence);

		curverts = curindis = 0;
	}

};

vk::ShaderModule VulkanRenderer::loadShader(const char* name, const char* func) {
	std::string fileName = std::string(name) + '_' + func + ".spirv";
	char* buffer = nullptr; int bufferSize = 0;
	LoadFile(fileName.c_str(), &buffer, &bufferSize, 0);
	assert(buffer && bufferSize > 0);
	assert((((uintptr_t)buffer) & 3) == 0);
	vk::ShaderModuleCreateInfo info;
	info.pCode = (const uint32_t*)buffer;
	info.codeSize = bufferSize;
	vk::ShaderModule module = m_vkDevice.createShaderModule(info);
	free(buffer);
	return module;
}

// Initialisation

void VulkanRenderer::Init() {
	// Get SDL's required Vulkan instance extensions to enable
	unsigned int numExtensions = 0;
	auto sdlResult = SDL_Vulkan_GetInstanceExtensions(g_sdlWindow, &numExtensions, nullptr);
	assert(sdlResult == SDL_TRUE);
	std::vector<const char*> extensionNames(numExtensions);
	sdlResult = SDL_Vulkan_GetInstanceExtensions(g_sdlWindow, &numExtensions, extensionNames.data());
	assert(sdlResult == SDL_TRUE);

	// Vulkan Instance

	vk::ApplicationInfo appInfo;
	appInfo.pApplicationName = "wkbre2";
	appInfo.applicationVersion = 0;
	appInfo.pEngineName = "wkbre2";
	appInfo.engineVersion = 0;
	appInfo.apiVersion = VK_API_VERSION_1_0;

	const char* instanceExtensions[] = { "VK_LAYER_LUNARG_core_validation" };

	vk::InstanceCreateInfo createInfo;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.ppEnabledExtensionNames = extensionNames.data();
	createInfo.enabledExtensionCount = extensionNames.size();
	//createInfo.enabledLayerCount = std::size(instanceExtensions);
	//createInfo.ppEnabledLayerNames = instanceExtensions;
	
	m_vkInstance = vk::createInstance(createInfo);

	// Device creation

	const std::vector<vk::PhysicalDevice> devices = m_vkInstance.enumeratePhysicalDevices();
	assert(devices.size() > 0);
	int physicalDeviceIndex = g_settings.value<int>("d3d11AdapterIndex", 0);
	if (physicalDeviceIndex < 0 || physicalDeviceIndex >= (int)devices.size())
		physicalDeviceIndex = 0;
	m_vkPhysicalDevice = devices[physicalDeviceIndex];

	const std::vector<vk::QueueFamilyProperties> queueFamilyProps = m_vkPhysicalDevice.getQueueFamilyProperties();
	m_queueFamilyIndex = -1;
	for (size_t i = 0; i < queueFamilyProps.size(); ++i) {
		if (queueFamilyProps[i].queueFlags & vk::QueueFlagBits::eGraphics) {
			m_queueFamilyIndex = (int)i;
		}
	}
	assert(m_queueFamilyIndex != -1);

	vk::DeviceQueueCreateInfo queueCreateInfo;
	float queuePriorities[1] = { 0.0 };
	queueCreateInfo.queueFamilyIndex = m_queueFamilyIndex;
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = queuePriorities;

	vk::PhysicalDeviceFeatures supportedFeatures = m_vkPhysicalDevice.getFeatures();
	const bool hasAnisotropy = supportedFeatures.samplerAnisotropy;
	vk::PhysicalDeviceFeatures featuresToEnable;
	featuresToEnable.samplerAnisotropy = hasAnisotropy;

	const char* deviceExtensionNames[] = { "VK_KHR_swapchain", "VK_KHR_maintenance1" };

	vk::DeviceCreateInfo deviceCreateInfo;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.enabledExtensionCount = std::size(deviceExtensionNames);
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensionNames;
	deviceCreateInfo.pEnabledFeatures = &featuresToEnable;

	m_vkDevice = m_vkPhysicalDevice.createDevice(deviceCreateInfo);

	m_vkQueue = m_vkDevice.getQueue(m_queueFamilyIndex, 0);

	vk::SemaphoreCreateInfo semaInfo;
	m_mainSemaphore = m_vkDevice.createSemaphore(semaInfo);
	m_swapchainSemaphore = m_vkDevice.createSemaphore(semaInfo);

	vk::FenceCreateInfo swapFenceInfo;
	m_swapchainFence = m_vkDevice.createFence(swapFenceInfo);

	vk::CommandPoolCreateInfo commandPoolCreateInfo;
	commandPoolCreateInfo.queueFamilyIndex = m_queueFamilyIndex;
	m_vkCommandPool = m_vkDevice.createCommandPool(commandPoolCreateInfo);

	// ==== Surface ====

	VkSurfaceKHR windowSurface;
	sdlResult = SDL_Vulkan_CreateSurface(g_sdlWindow, m_vkInstance, &windowSurface);
	assert(sdlResult == SDL_TRUE);
	m_vkSurface = windowSurface;

	vk::Bool32 swapchainSurfaceSupported = m_vkPhysicalDevice.getSurfaceSupportKHR(m_queueFamilyIndex, m_vkSurface);
	assert(swapchainSurfaceSupported == vk::True);

	// only to make validator happy
	vk::SurfaceCapabilitiesKHR surfCaps = m_vkPhysicalDevice.getSurfaceCapabilitiesKHR(m_vkSurface);

	static const vk::Format preferedSurfaceFormats[]{
		vk::Format::eB8G8R8A8Unorm,
		vk::Format::eR8G8B8A8Unorm,
	};

	size_t surfaceFormatFound = SIZE_MAX;
	auto surfaceFormatsSupported = m_vkPhysicalDevice.getSurfaceFormatsKHR(m_vkSurface);
	for (const auto& surfFormat : surfaceFormatsSupported) {
		if (surfFormat.colorSpace != vk::ColorSpaceKHR::eSrgbNonlinear)
			continue;
		const auto it = std::find(std::begin(preferedSurfaceFormats), std::end(preferedSurfaceFormats), surfFormat.format);
		if (it != std::end(preferedSurfaceFormats)) {
			surfaceFormatFound = std::min(surfaceFormatFound, size_t(it - std::begin(preferedSurfaceFormats)));
		}
	}
	assert(surfaceFormatFound != SIZE_MAX);
	m_surfaceFormat = preferedSurfaceFormats[surfaceFormatFound];

	//msaaNumSamples = g_settings.value<int>("msaaNumSamples", 1);

	// ==== Shaders ====

	auto vsBlob = loadShader("EnhBasicShader", "VS");
	auto vsFogBlob = loadShader("EnhBasicShader", "VS_Fog");
	auto psBlob = loadShader("EnhBasicShader", "PS");
	auto psAlphaTestBlob = loadShader("EnhBasicShader", "PS_AlphaTest");
	auto psMapBlob = loadShader("EnhBasicShader", "PS_Map");
	auto psLakeBlob = loadShader("EnhBasicShader", "PS_Lake");

	// ==== Memory ====

	VmaAllocatorCreateInfo vmaInfo;
	memset(&vmaInfo, 0, sizeof(vmaInfo));
	vmaInfo.physicalDevice = m_vkPhysicalDevice;
	vmaInfo.device = m_vkDevice;
	vmaInfo.instance = m_vkInstance;
	vmaCreateAllocator(&vmaInfo, &m_vmaAllocator);

	// ==== Render pass ====

	vk::AttachmentDescription attachmentDesc[2];
	attachmentDesc[0].format = m_surfaceFormat;
	attachmentDesc[0].samples = vk::SampleCountFlagBits::e1;
	attachmentDesc[0].loadOp = vk::AttachmentLoadOp::eLoad;
	attachmentDesc[0].storeOp = vk::AttachmentStoreOp::eStore;
	attachmentDesc[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	attachmentDesc[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	attachmentDesc[0].initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
	attachmentDesc[0].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
	attachmentDesc[1].format = vk::Format(VK_FORMAT_D24_UNORM_S8_UINT);
	attachmentDesc[1].samples = vk::SampleCountFlagBits::e1;
	attachmentDesc[1].loadOp = vk::AttachmentLoadOp::eLoad;
	attachmentDesc[1].storeOp = vk::AttachmentStoreOp::eStore;
	attachmentDesc[1].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	attachmentDesc[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	attachmentDesc[1].initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
	attachmentDesc[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	vk::AttachmentReference colorAttachmentRef;
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;
	vk::AttachmentReference depthAttachmentRef;
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	vk::SubpassDescription subpassDesc;
	subpassDesc.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	subpassDesc.colorAttachmentCount = 1;
	subpassDesc.pColorAttachments = &colorAttachmentRef;
	subpassDesc.pDepthStencilAttachment = &depthAttachmentRef;

	vk::RenderPassCreateInfo renderPassInfo;
	renderPassInfo.attachmentCount = std::size(attachmentDesc);
	renderPassInfo.pAttachments = attachmentDesc;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDesc;
	m_vkRenderPass = m_vkDevice.createRenderPass(renderPassInfo);

	// framebuffers created in Reset()

	// ==== Pipeline layout ====

	vk::SamplerCreateInfo scInfo;
	// default D3D11 values
	scInfo.magFilter = vk::Filter::eLinear;
	scInfo.minFilter = vk::Filter::eLinear;
	scInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
	scInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
	scInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
	scInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
	scInfo.mipLodBias = 0.0f;
	scInfo.anisotropyEnable = hasAnisotropy ? vk::True : vk::False;
	scInfo.maxAnisotropy = 16;
	scInfo.compareEnable = vk::False;
	//scInfo.compareOp = 0;
	scInfo.minLod = 0.0f;
	scInfo.maxLod = VK_LOD_CLAMP_NONE;
	scInfo.borderColor = vk::BorderColor::eFloatTransparentBlack;
	scInfo.unnormalizedCoordinates = vk::False;
	m_vkSampler = m_vkDevice.createSampler(scInfo);

	vk::DescriptorSetLayoutBinding bindings[3];
	bindings[0].binding = 0;
	bindings[0].descriptorType = vk::DescriptorType::eUniformBuffer;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = vk::ShaderStageFlagBits::eVertex;
	bindings[1].binding = 1;
	bindings[1].descriptorType = vk::DescriptorType::eUniformBuffer;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
	bindings[2].binding = 2;
	bindings[2].descriptorType = vk::DescriptorType::eCombinedImageSampler;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = vk::ShaderStageFlagBits::eFragment;
	bindings[2].pImmutableSamplers = &m_vkSampler;

	vk::DescriptorSetLayoutCreateInfo dslcInfo;
	dslcInfo.bindingCount = std::size(bindings);
	dslcInfo.pBindings = bindings;
	m_vkDescSetLayout = m_vkDevice.createDescriptorSetLayout(dslcInfo);

	vk::PipelineLayoutCreateInfo plcInfo;
	plcInfo.setLayoutCount = 1;
	plcInfo.pSetLayouts = &m_vkDescSetLayout;
	m_vkPipelineLayout = m_vkDevice.createPipelineLayout(plcInfo);

	vk::DescriptorPoolSize descPoolSizes[] = {
		{vk::DescriptorType::eUniformBuffer, 2},
		{vk::DescriptorType::eCombinedImageSampler, 1},
	};
	vk::DescriptorPoolCreateInfo dpcInfo;
	dpcInfo.maxSets = 1;
	dpcInfo.poolSizeCount = std::size(descPoolSizes);
	dpcInfo.pPoolSizes = descPoolSizes;
	m_vkDescriptorPool = m_vkDevice.createDescriptorPool(dpcInfo);

	vk::DescriptorSetAllocateInfo dsaInfo;
	dsaInfo.descriptorPool = m_vkDescriptorPool;
	dsaInfo.descriptorSetCount = 1;
	dsaInfo.pSetLayouts = &m_vkDescSetLayout;

	m_vkMainDescriptorSet = m_vkDevice.allocateDescriptorSets(dsaInfo).at(0);


	VmaAllocationCreateInfo uniformAllocCreateInfo;
	memset(&uniformAllocCreateInfo, 0, sizeof(uniformAllocCreateInfo));
	uniformAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	vk::BufferCreateInfo uniformCreateInfo;
	uniformCreateInfo.size = 64;
	uniformCreateInfo.usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst;
	uniformCreateInfo.sharingMode = vk::SharingMode::eExclusive;

	VkBuffer bufferTemp;
	vmaCreateBuffer(m_vmaAllocator, uniformCreateInfo, &uniformAllocCreateInfo, &bufferTemp, &m_currentTransformBufferAlloc, nullptr);
	m_currentTransformBuffer = bufferTemp;
	uniformCreateInfo.size = 32;
	vmaCreateBuffer(m_vmaAllocator, uniformCreateInfo, &uniformAllocCreateInfo, &bufferTemp, &m_currentFogBufferAlloc, nullptr);
	m_currentFogBuffer = bufferTemp;

	// ==== Pipeline ====

	vk::PipelineShaderStageCreateInfo stageInfo[2];
	stageInfo[0].stage = vk::ShaderStageFlagBits::eVertex;
	stageInfo[0].module = vsBlob;
	stageInfo[0].pName = "VS";
	stageInfo[1].stage = vk::ShaderStageFlagBits::eFragment;
	stageInfo[1].module = psBlob;
	stageInfo[1].pName = "PS";

	vk::VertexInputBindingDescription viBindings[1] = {
		{0, sizeof(batchVertex), vk::VertexInputRate::eVertex}
	};

	vk::VertexInputAttributeDescription viAttributes[3] = {
		{0, 0, vk::Format(VK_FORMAT_R32G32B32_SFLOAT), 0},
		{1, 0, vk::Format(VK_FORMAT_R8G8B8A8_UNORM), 12},
		{2, 0, vk::Format(VK_FORMAT_R32G32_SFLOAT), 16}
	};

	vk::PipelineVertexInputStateCreateInfo viStateInfo;
	viStateInfo.vertexBindingDescriptionCount = std::size(viBindings);
	viStateInfo.pVertexBindingDescriptions = viBindings;
	viStateInfo.vertexAttributeDescriptionCount = std::size(viAttributes);
	viStateInfo.pVertexAttributeDescriptions = viAttributes;

	vk::PipelineInputAssemblyStateCreateInfo iaStateInfo;
	iaStateInfo.topology = vk::PrimitiveTopology::eTriangleList;

	vk::PipelineViewportStateCreateInfo vpStateInfo;
	vpStateInfo.viewportCount = 1;
	vpStateInfo.pViewports = nullptr; // dynamic
	vpStateInfo.scissorCount = 1;
	vpStateInfo.pScissors = nullptr; // dynamic

	vk::PipelineRasterizationStateCreateInfo rsStateInfo_2D;
	rsStateInfo_2D.depthClampEnable = vk::False;
	rsStateInfo_2D.rasterizerDiscardEnable = vk::False;
	rsStateInfo_2D.polygonMode = vk::PolygonMode::eFill;
	rsStateInfo_2D.cullMode = vk::CullModeFlagBits::eNone;
	rsStateInfo_2D.frontFace = vk::FrontFace::eClockwise;
	rsStateInfo_2D.depthBiasEnable = vk::False;
	rsStateInfo_2D.lineWidth = 1.0f;
	vk::PipelineRasterizationStateCreateInfo rsStateInfo_3D = rsStateInfo_2D;
	rsStateInfo_3D.cullMode = vk::CullModeFlagBits::eBack;

	vk::PipelineMultisampleStateCreateInfo msStateInfo;
	msStateInfo.rasterizationSamples = vk::SampleCountFlagBits::e1;

	vk::PipelineDepthStencilStateCreateInfo dsStateInfo_On;
	dsStateInfo_On.depthTestEnable = vk::True;
	dsStateInfo_On.depthWriteEnable = vk::True;
	dsStateInfo_On.depthCompareOp = vk::CompareOp::eLessOrEqual;
	vk::PipelineDepthStencilStateCreateInfo dsStateInfo_Off;
	dsStateInfo_Off.depthTestEnable = vk::False;
	dsStateInfo_Off.depthWriteEnable = vk::False;
	dsStateInfo_Off.depthCompareOp = vk::CompareOp::eAlways;
	vk::PipelineDepthStencilStateCreateInfo dsStateInfo_TestOnly;
	dsStateInfo_TestOnly.depthTestEnable = vk::True;
	dsStateInfo_TestOnly.depthWriteEnable = vk::False;
	dsStateInfo_TestOnly.depthCompareOp = vk::CompareOp::eLessOrEqual;

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
	vk::PipelineColorBlendAttachmentState cbAttach_Blend;
	cbAttach_Blend.blendEnable = vk::True;
	cbAttach_Blend.srcColorBlendFactor = vk::BlendFactor(VK_BLEND_FACTOR_SRC_ALPHA);
	cbAttach_Blend.dstColorBlendFactor = vk::BlendFactor(VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
	cbAttach_Blend.colorBlendOp = vk::BlendOp(VK_BLEND_OP_ADD);
	cbAttach_Blend.srcAlphaBlendFactor = vk::BlendFactor(VK_BLEND_FACTOR_SRC_ALPHA);
	cbAttach_Blend.dstAlphaBlendFactor = vk::BlendFactor(VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
	cbAttach_Blend.alphaBlendOp = vk::BlendOp(VK_BLEND_OP_ADD);
	cbAttach_Blend.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
		| vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

	vk::PipelineColorBlendStateCreateInfo cbStateInfo;
	cbStateInfo.attachmentCount = 1;
	cbStateInfo.pAttachments = &cbAttach_Blend;

	vk::DynamicState dynamicStates[] = {
		vk::DynamicState::eViewport, vk::DynamicState::eScissor
	};
	vk::PipelineDynamicStateCreateInfo pdStateInfo;
	pdStateInfo.dynamicStateCount = std::size(dynamicStates);
	pdStateInfo.pDynamicStates = dynamicStates;

	vk::PipelineCacheCreateInfo pccInfo;
	m_vkPipelineCache = m_vkDevice.createPipelineCache(pccInfo);

	vk::GraphicsPipelineCreateInfo gpcInfo;
	gpcInfo.layout = m_vkPipelineLayout;
	gpcInfo.stageCount = std::size(stageInfo);
	gpcInfo.pStages = stageInfo;
	gpcInfo.pVertexInputState = &viStateInfo;
	gpcInfo.pInputAssemblyState = &iaStateInfo;
	gpcInfo.pTessellationState = nullptr; // none
	gpcInfo.pViewportState = &vpStateInfo; // dynamic
	gpcInfo.pRasterizationState = &rsStateInfo_2D;
	gpcInfo.pMultisampleState = &msStateInfo;
	gpcInfo.pDepthStencilState = &dsStateInfo_Off;
	gpcInfo.pColorBlendState = &cbStateInfo;
	gpcInfo.pDynamicState = &pdStateInfo;
	gpcInfo.layout = m_vkPipelineLayout;
	gpcInfo.renderPass = m_vkRenderPass;
	gpcInfo.subpass = 0;

	iaStateInfo.topology = vk::PrimitiveTopology::eTriangleList;
	m_pipeline2D = m_vkDevice.createGraphicsPipeline(m_vkPipelineCache, gpcInfo).value;

	iaStateInfo.topology = vk::PrimitiveTopology::eLineList;
	m_pipeline2DLines = m_vkDevice.createGraphicsPipeline(m_vkPipelineCache, gpcInfo).value;

	gpcInfo.pRasterizationState = &rsStateInfo_3D;
	stageInfo[0].module = vsFogBlob;
	stageInfo[0].pName = "VS_Fog";

	gpcInfo.pDepthStencilState = &dsStateInfo_On;
	cbStateInfo.pAttachments = &cbAttach_NoBlend;
	stageInfo[1].module = psBlob;
	stageInfo[1].pName = "PS";
	iaStateInfo.topology = vk::PrimitiveTopology::eTriangleList;
	m_pipeline3D = m_vkDevice.createGraphicsPipeline(m_vkPipelineCache, gpcInfo).value;
	
	gpcInfo.pDepthStencilState = &dsStateInfo_On;
	cbStateInfo.pAttachments = &cbAttach_NoBlend;
	stageInfo[1].module = psAlphaTestBlob;
	stageInfo[1].pName = "PS_AlphaTest";
	iaStateInfo.topology = vk::PrimitiveTopology::eTriangleList;
	m_pipeline3DAlphaTest = m_vkDevice.createGraphicsPipeline(m_vkPipelineCache, gpcInfo).value;

	gpcInfo.pDepthStencilState = &dsStateInfo_TestOnly;
	cbStateInfo.pAttachments = &cbAttach_Blend;
	stageInfo[1].module = psBlob;
	stageInfo[1].pName = "PS";
	iaStateInfo.topology = vk::PrimitiveTopology::eTriangleList;
	m_pipeline3DAlphaBlend = m_vkDevice.createGraphicsPipeline(m_vkPipelineCache, gpcInfo).value;

	iaStateInfo.topology = vk::PrimitiveTopology::eLineList;
	m_pipeline3DAlphaBlendLines = m_vkDevice.createGraphicsPipeline(m_vkPipelineCache, gpcInfo).value;

	gpcInfo.pDepthStencilState = &dsStateInfo_On;
	cbStateInfo.pAttachments = &cbAttach_NoBlend;
	stageInfo[1].module = psMapBlob;
	stageInfo[1].pName = "PS_Map";
	iaStateInfo.topology = vk::PrimitiveTopology::eTriangleList;
	m_pipelineTerrain = m_vkDevice.createGraphicsPipeline(m_vkPipelineCache, gpcInfo).value;

	gpcInfo.pDepthStencilState = &dsStateInfo_On;
	cbStateInfo.pAttachments = &cbAttach_Blend;
	stageInfo[1].module = psLakeBlob;
	stageInfo[1].pName = "PS_Lake";
	iaStateInfo.topology = vk::PrimitiveTopology::eTriangleList;
	m_pipelineLake = m_vkDevice.createGraphicsPipeline(m_vkPipelineCache, gpcInfo).value;

	// Fence

	vk::FenceCreateInfo fcInfo;
	fcInfo.flags = vk::FenceCreateFlagBits::eSignaled;
	m_nonDynamicBufferFence = m_vkDevice.createFence(fcInfo);

	// Buffers
	globalBuffers = std::make_unique<GlobalBuffers>(this);

	Bitmap whiteBmp;
	whiteBmp.width = 1;
	whiteBmp.height = 1;
	whiteBmp.format = BMFORMAT_R8G8B8A8;
	whiteBmp.pixels.resize(4);
	*(uint32_t*)whiteBmp.pixels.data() = 0xFFFFFFFF;
	whiteTexture = CreateTexture(whiteBmp, 1);

	setDescriptors(m_currentTransformBuffer, m_currentFogBuffer, VkImageView(whiteTexture));

	Reset();
}

void VulkanRenderer::Reset() {
	//ddImmediateContext->ClearState();
	//if (depthView) depthView->Release();
	//if (depthBuffer) depthBuffer->Release();
	//if (ddRenderTargetView) ddRenderTargetView->Release();

	//HRESULT hres = dxgiSwapChain->ResizeBuffers(1, g_windowWidth, g_windowHeight, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
	//assert(!FAILED(hres));

	//ID3D11Texture2D* backBuffer;
	//hres = dxgiSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
	//assert(backBuffer != nullptr);
	//ddDevice->CreateRenderTargetView(backBuffer, NULL, &ddRenderTargetView);
	//backBuffer->Release();

	//D3D11_TEXTURE2D_DESC dsdesc;
	//dsdesc.Width = g_windowWidth;
	//dsdesc.Height = g_windowHeight;
	//dsdesc.MipLevels = 1;
	//dsdesc.ArraySize = 1;
	//dsdesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	//dsdesc.SampleDesc.Count = msaaNumSamples;
	//dsdesc.SampleDesc.Quality = 0;
	//dsdesc.Usage = D3D11_USAGE_DEFAULT;
	//dsdesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	//dsdesc.CPUAccessFlags = 0;
	//dsdesc.MiscFlags = 0;
	//hres = ddDevice->CreateTexture2D(&dsdesc, nullptr, &depthBuffer);
	//assert(!FAILED(hres));

	//D3D11_DEPTH_STENCIL_VIEW_DESC dsviewdesc;
	//dsviewdesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	//dsviewdesc.ViewDimension = (msaaNumSamples > 1) ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
	//dsviewdesc.Flags = 0;
	//dsviewdesc.Texture2D.MipSlice = 0;
	//hres = ddDevice->CreateDepthStencilView(depthBuffer, &dsviewdesc, &depthView);
	//assert(!FAILED(hres));
	
	m_vkDevice.waitIdle();

	m_surfaceWidth = g_windowWidth;
	m_surfaceHeight = g_windowHeight;

	// Destroy old stuff

	for (const auto& swapchainImg : m_vkSwapchainImages) {
		m_vkDevice.destroyFramebuffer(swapchainImg.framebuffer);
		m_vkDevice.destroyImageView(swapchainImg.imageView);
		m_vkDevice.destroyImageView(swapchainImg.depthImageView);
		vmaDestroyImage(m_vmaAllocator, swapchainImg.depthImage, swapchainImg.depthAllocation);
	}
	m_vkSwapchainImages.clear();

	if (m_vkSwapchain)
		m_vkDevice.destroySwapchainKHR(m_vkSwapchain);
	//if (m_vkSurface)
	//	m_vkInstance.destroySurfaceKHR(m_vkSurface);

	m_vkSwapchain = nullptr;
	//m_vkSurface = nullptr;

	// Create swapchain

	vk::SurfaceCapabilitiesKHR surfCaps = m_vkPhysicalDevice.getSurfaceCapabilitiesKHR(m_vkSurface);

	vk::SwapchainCreateInfoKHR swapchainCreateInfo;
	swapchainCreateInfo.surface = m_vkSurface;
	swapchainCreateInfo.minImageCount = 2;
	swapchainCreateInfo.imageFormat = m_surfaceFormat;
	swapchainCreateInfo.imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
	swapchainCreateInfo.imageExtent = surfCaps.currentExtent;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
	swapchainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
	swapchainCreateInfo.preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
	swapchainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	swapchainCreateInfo.presentMode = vk::PresentModeKHR::eFifo;
	swapchainCreateInfo.clipped = vk::True;

	m_vkSwapchain = m_vkDevice.createSwapchainKHR(swapchainCreateInfo);
	auto vkSwapchainImages = m_vkDevice.getSwapchainImagesKHR(m_vkSwapchain);

	for (size_t i = 0; i < vkSwapchainImages.size(); ++i) {
		auto& si = m_vkSwapchainImages.emplace_back();
		si.image = vkSwapchainImages[i];

		vk::ImageViewCreateInfo vci;
		vci.image = si.image;
		vci.viewType = vk::ImageViewType::e2D;
		vci.format = m_surfaceFormat;
		vci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		vci.subresourceRange.baseMipLevel = 0;
		vci.subresourceRange.levelCount = 1;
		vci.subresourceRange.baseArrayLayer = 0;
		vci.subresourceRange.layerCount = 1;
		si.imageView = m_vkDevice.createImageView(vci);

		VmaAllocationCreateInfo allocCreateInfo;
		memset(&allocCreateInfo, 0, sizeof(allocCreateInfo));
		vk::ImageCreateInfo ici;
		ici.imageType = vk::ImageType::e2D;
		ici.extent.width = m_surfaceWidth;
		ici.extent.height = m_surfaceHeight;
		ici.extent.depth = 1;
		ici.mipLevels = 1;
		ici.arrayLayers = 1;
		ici.format = vk::Format(VK_FORMAT_D24_UNORM_S8_UINT);
		ici.samples = vk::SampleCountFlagBits::e1;
		ici.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
		ici.tiling = vk::ImageTiling::eOptimal;
		allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		VkImage depthImage;
		auto result = vmaCreateImage(m_vmaAllocator, ici, &allocCreateInfo, &depthImage, &si.depthAllocation, nullptr);
		assert(result == VK_SUCCESS);
		si.depthImage = depthImage;

		vci.image = si.depthImage;
		vci.viewType = vk::ImageViewType::e2D;
		vci.format = vk::Format(VK_FORMAT_D24_UNORM_S8_UINT);
		vci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
		si.depthImageView = m_vkDevice.createImageView(vci);

		vk::ImageView attachments[2] = { si.imageView, si.depthImageView };

		vk::FramebufferCreateInfo fci;
		fci.renderPass = m_vkRenderPass;
		fci.attachmentCount = std::size(attachments);
		fci.pAttachments = attachments;
		fci.width = m_surfaceWidth;
		fci.height = m_surfaceHeight;
		fci.layers = 1;
		si.framebuffer = m_vkDevice.createFramebuffer(fci);
	}

	nextFrame();
}

void VulkanRenderer::BeginDrawing() {

	m_viewport.x = 0.0f;
	m_viewport.y = (float)m_surfaceHeight;
	m_viewport.width = (float)m_surfaceWidth;
	m_viewport.height = -(float)m_surfaceHeight;
	m_viewport.minDepth = 0.0f;
	m_viewport.maxDepth = 1.0f;

	auto rr = m_vkDevice.waitForFences(1, &m_swapchainFence, vk::True, 10'000'000'000);
	assert(rr == vk::Result::eSuccess);

	//ddImmediateContext->ClearState();
	//ddImmediateContext->OMSetRenderTargets(1, &ddRenderTargetView, depthView);
	////ddImmediateContext->RSSetViewports(1, &vp);
	//ddImmediateContext->IASetInputLayout(ddInputLayout);
	//ddImmediateContext->VSSetShader(ddVertexShader, nullptr, 0);
	//ddImmediateContext->PSSetShader(ddPixelShader, nullptr, 0);
	//ddImmediateContext->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&whiteTexture);

	setDescriptors(std::nullopt, std::nullopt, VkImageView(whiteTexture));

	fogEnabled = false;
	m_currentPipeline = nullptr; // it needs to be decided by the caller!
	m_primitiveTopology = vk::PrimitiveTopology::eTriangleList;
}

void VulkanRenderer::EndDrawing() {
	vk::ImageMemoryBarrier imgBarrier1;
	imgBarrier1.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
	imgBarrier1.dstAccessMask = vk::AccessFlagBits::eMemoryRead;
	imgBarrier1.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
	imgBarrier1.newLayout = vk::ImageLayout::ePresentSrcKHR;
	imgBarrier1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imgBarrier1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imgBarrier1.image = m_vkSwapchainImages[m_currentSwapchainImageIndex].image;
	imgBarrier1.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	imgBarrier1.subresourceRange.baseArrayLayer = 0;
	imgBarrier1.subresourceRange.baseMipLevel = 0;
	imgBarrier1.subresourceRange.layerCount = 1;
	imgBarrier1.subresourceRange.levelCount = 1;

	auto cmdBuffer = createCommandBufferAndBegin();
	cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &imgBarrier1);
	cmdBuffer.end();
	submitSingleCommandBuffer(cmdBuffer);
	//submitSignalSemaphore(m_swapchainSemaphore);

	vk::PresentInfoKHR pinfo;
	pinfo.swapchainCount = 1;
	pinfo.pSwapchains = &m_vkSwapchain;
	pinfo.pImageIndices = &m_currentSwapchainImageIndex;
	//pinfo.waitSemaphoreCount = 1;
	//pinfo.pWaitSemaphores = &m_swapchainSemaphore;
	m_vkQueue.presentKHR(pinfo);

	//std::this_thread::sleep_for(std::chrono::milliseconds(20));
	//if (hres != S_OK) {
	//	Sleep(100);
	//}

	m_vkQueue.waitIdle();

	for (const vk::CommandBuffer& cmdBuffer : activeCommandBuffers) {
		m_vkDevice.freeCommandBuffers(m_vkCommandPool, 1, &cmdBuffer);
	}
	activeCommandBuffers.clear();

	nextFrame();
}

void VulkanRenderer::ClearFrame(bool clearColors, bool clearDepth, uint32_t color) {
	vk::ClearAttachment clearAttachments[2];
	uint32_t numClearAttachments = 0;
	if (clearColors) {
		auto& clearAttach = clearAttachments[numClearAttachments++];
		clearAttach.aspectMask = vk::ImageAspectFlagBits::eColor;
		clearAttach.colorAttachment = 0;
		auto& cc = clearAttach.clearValue.color.float32;
		cc[0] = (float)((color >> 16) & 255) / 255.0f;
		cc[1] = (float)((color >> 8) & 255) / 255.0f;
		cc[2] = (float)((color >> 0) & 255) / 255.0f;
		cc[3] = (float)((color >> 24) & 255) / 255.0f;
	}
	if (clearDepth) {
		auto& clearAttach = clearAttachments[numClearAttachments++];
		clearAttach.aspectMask = vk::ImageAspectFlagBits::eDepth;
		clearAttach.clearValue.depthStencil.depth = 1.0f;
		clearAttach.clearValue.depthStencil.stencil = 0;
	}
	vk::ClearRect clearRect;
	clearRect.rect.offset.x = 0;
	clearRect.rect.offset.y = 0;
	clearRect.rect.extent.width = m_surfaceWidth;
	clearRect.rect.extent.height = m_surfaceHeight;
	clearRect.baseArrayLayer = 0;
	clearRect.layerCount = 1;
	auto cmdBuffer = createCommandBufferAndBegin();
	beginPass(cmdBuffer, m_pipeline2D);
	cmdBuffer.clearAttachments(numClearAttachments, clearAttachments, 1, &clearRect);
	endPass(cmdBuffer);
	cmdBuffer.end();

	auto rr = m_vkDevice.waitForFences(1, &m_swapchainFence, vk::True, 10'000'000'000);
	assert(rr == vk::Result::eSuccess);

	submitSingleCommandBuffer(cmdBuffer);
}

// Textures management

texture VulkanRenderer::CreateTexture(const Bitmap& bm, int mipmaps) {
	Bitmap cvtbmp = bm.convertToR8G8B8A8();

	int mmwidth = cvtbmp.width,
		mmheight = cvtbmp.height,
		numMipmaps = 0;
	Bitmap mmbmp[32];
	mmbmp[numMipmaps++] = cvtbmp;
	if (mipmaps <= 0) {
		mmwidth /= 2;
		mmheight /= 2;
		while (mmwidth && mmheight) {
			assert(numMipmaps < std::size(mmbmp));
			if (!mmwidth) mmwidth = 1;
			if (!mmheight) mmheight = 1;
			mmbmp[numMipmaps++] = cvtbmp.resize(mmwidth, mmheight);
			mmwidth /= 2;
			mmheight /= 2;
			// no mipmaps of 8x8 or below (to prevent tiles from being merged in the terrain atlas!)
			if (mmwidth <= 8 || mmheight <= 8)
				break;
		}
	}

	vk::ImageCreateInfo desc;
	desc.imageType = vk::ImageType::e2D;
	desc.extent.width = cvtbmp.width;
	desc.extent.height = cvtbmp.height;
	desc.extent.depth = 1;
	desc.mipLevels = numMipmaps;
	desc.arrayLayers = 1;
	desc.format = vk::Format(VK_FORMAT_R8G8B8A8_UNORM);
	desc.samples = vk::SampleCountFlagBits::e1;
	desc.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
	desc.tiling = vk::ImageTiling::eOptimal;

	// Stage buffer

	vk::BufferCreateInfo bcInfo;
	bcInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
	bcInfo.size = cvtbmp.width * cvtbmp.height * 4;

	VmaAllocationCreateInfo acInfo{};
	memset(&acInfo, 0, sizeof(acInfo));
	acInfo.flags = 0;
	acInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	VkImage imageHandle;
	VmaAllocation allocationHandle;
	VmaAllocationInfo allocationInfo;
	auto res = vmaCreateImage(m_vmaAllocator, desc, &acInfo, &imageHandle, &allocationHandle, &allocationInfo);
	assert(res == VK_SUCCESS);

	//

	acInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT
		| VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	acInfo.usage = VMA_MEMORY_USAGE_AUTO;

	VkBuffer stageBuffer;
	VmaAllocation stageAllocation;
	VmaAllocationInfo stageAllocationInfo;

	vmaCreateBuffer(m_vmaAllocator, bcInfo, &acInfo, &stageBuffer, &stageAllocation, &stageAllocationInfo);

	//

	for (int mipmapLevel = 0; mipmapLevel < numMipmaps; ++mipmapLevel) {
		const auto& mipmapBitmap = mmbmp[mipmapLevel];
		memcpy(stageAllocationInfo.pMappedData, mipmapBitmap.pixels.data(), mipmapBitmap.width * mipmapBitmap.height * 4);
		
		vk::BufferImageCopy bic{};
		bic.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
		bic.imageSubresource.mipLevel = mipmapLevel;
		bic.imageSubresource.baseArrayLayer = 0;
		bic.imageSubresource.layerCount = 1;
		bic.imageExtent.width = mipmapBitmap.width;
		bic.imageExtent.height = mipmapBitmap.height;
		bic.imageExtent.depth = 1;

		vk::ImageMemoryBarrier imgBarrier1;
		imgBarrier1.srcAccessMask = vk::AccessFlagBits::eNone;
		imgBarrier1.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
		imgBarrier1.oldLayout = vk::ImageLayout::eUndefined;
		imgBarrier1.newLayout = vk::ImageLayout::eTransferDstOptimal;
		imgBarrier1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imgBarrier1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imgBarrier1.image = imageHandle;
		imgBarrier1.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		imgBarrier1.subresourceRange.baseArrayLayer = 0;
		imgBarrier1.subresourceRange.baseMipLevel = mipmapLevel;
		imgBarrier1.subresourceRange.layerCount = 1;
		imgBarrier1.subresourceRange.levelCount = 1;

		vk::ImageMemoryBarrier imgBarrier2 = imgBarrier1;
		imgBarrier2.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		imgBarrier2.dstAccessMask = vk::AccessFlagBits::eShaderRead;
		imgBarrier2.oldLayout = vk::ImageLayout::eTransferDstOptimal;
		imgBarrier2.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

		vk::CommandBuffer cmdBuffer = createCommandBufferAndBegin();
		cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &imgBarrier1);
		cmdBuffer.copyBufferToImage(stageBuffer, imageHandle, vk::ImageLayout::eTransferDstOptimal, 1, &bic);
		cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &imgBarrier2);
		cmdBuffer.end();
		submitSingleCommandBuffer(cmdBuffer);
		m_vkQueue.waitIdle();
	}

	//D3D11_SUBRESOURCE_DATA sub[std::size(mmbmp)];
	//for (size_t i = 0; i < (size_t)numMipmaps; i++) {
	//	sub[i].pSysMem = mmbmp[i].pixels.data();
	//	sub[i].SysMemPitch = mmbmp[i].width * 4u;
	//	sub[i].SysMemSlicePitch = sub[i].SysMemPitch * mmbmp[i].height;
	//}

	vk::ImageViewCreateInfo ivcInfo;
	ivcInfo.image = imageHandle;
	ivcInfo.viewType = vk::ImageViewType::e2D;
	ivcInfo.format = vk::Format(VK_FORMAT_R8G8B8A8_UNORM);
	ivcInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	ivcInfo.subresourceRange.baseArrayLayer = 0;
	ivcInfo.subresourceRange.baseMipLevel = 0;
	ivcInfo.subresourceRange.layerCount = 1;
	ivcInfo.subresourceRange.levelCount = 1;
	vk::ImageView imageView = m_vkDevice.createImageView(ivcInfo);

	vmaDestroyBuffer(m_vmaAllocator, stageBuffer, stageAllocation);

	m_imageViewToImageMap[imageView] = imageHandle;
	return imageView;
}

void VulkanRenderer::FreeTexture(texture t) {}

void VulkanRenderer::UpdateTexture(texture t, const Bitmap& bmp) {}

// State changes

void VulkanRenderer::SetTransformMatrix(const Matrix* m) {
	globalBuffers->transformBuffer.nextBuffer();
	auto* stage = globalBuffers->transformBuffer.getCurrentBuffer();

	void* transformBytes = stage->mappedPtr[0];
	*(Matrix*)transformBytes = m->getTranspose();

	vk::BufferCopy copy;
	copy.srcOffset = 0;
	copy.dstOffset = 0;
	copy.size = 64;

	vk::CommandBuffer cmdBuffer = createCommandBufferAndBegin();
	cmdBuffer.copyBuffer(stage->buffer[0], m_currentTransformBuffer, 1, &copy);
	cmdBuffer.end();
	submitSingleCommandBuffer(cmdBuffer, stage->fence);
}

void VulkanRenderer::SetTexture(uint32_t x, texture t) {
	if (t == nullptr)
		NoTexture(x);
	else
		setDescriptors(std::nullopt, std::nullopt, VkImageView(t));
}

void VulkanRenderer::NoTexture(uint32_t x) {
	setDescriptors(std::nullopt, std::nullopt, VkImageView(whiteTexture));
}

void VulkanRenderer::SetFog(uint32_t color, float farz) {
	globalBuffers->fogBuffer.nextBuffer();
	auto* stage = globalBuffers->fogBuffer.getCurrentBuffer();

	float* cc = (float*)stage->mappedPtr[0];
	cc[0] = (float)((color >> 16) & 255) / 255.0f;
	cc[1] = (float)((color >> 8) & 255) / 255.0f;
	cc[2] = (float)((color >> 0) & 255) / 255.0f;
	cc[3] = (float)((color >> 24) & 255) / 255.0f;
	cc[4] = farz * 0.5f;
	cc[5] = farz;

	vk::BufferCopy copy;
	copy.srcOffset = 0;
	copy.dstOffset = 0;
	copy.size = 32;

	vk::CommandBuffer cmdBuffer = createCommandBufferAndBegin();
	cmdBuffer.copyBuffer(stage->buffer[0], m_currentFogBuffer, 1, &copy);
	cmdBuffer.end();
	submitSingleCommandBuffer(cmdBuffer, stage->fence);

	//ddImmediateContext->VSSetShader(ddFogVertexShader, nullptr, 0);

	fogEnabled = true;
}

void VulkanRenderer::DisableFog() {
	//ddImmediateContext->VSSetShader(ddVertexShader, nullptr, 0);
	fogEnabled = false;
}

void VulkanRenderer::EnableAlphaTest() {
	if (m_currentPipeline == m_pipeline3D)
		m_currentPipeline = m_pipeline3DAlphaTest;
	//ddImmediateContext->PSSetShader(ddAlphaTestPixelShader, nullptr, 0);
}

void VulkanRenderer::DisableAlphaTest() {
	if (m_currentPipeline == m_pipeline3DAlphaTest)
		m_currentPipeline = m_pipeline3D;
	//ddImmediateContext->PSSetShader(ddPixelShader, nullptr, 0);
}

void VulkanRenderer::EnableColorBlend() {}

void VulkanRenderer::DisableColorBlend() {}

void VulkanRenderer::SetBlendColor(int c) {}

void VulkanRenderer::EnableAlphaBlend() {}

void VulkanRenderer::DisableAlphaBlend() {}

void VulkanRenderer::EnableScissor() {
	//ddImmediateContext->RSSetState(scissorRasterizerState);
}

void VulkanRenderer::DisableScissor() {
	//ddImmediateContext->RSSetState(nullptr);
	SetScissorRect(0, 0, 8192, 8192);
}

void VulkanRenderer::SetScissorRect(int x, int y, int w, int h) {
	m_scissorRect.offset.x = x;
	m_scissorRect.offset.y = y;
	m_scissorRect.extent.width = w;
	m_scissorRect.extent.height = h;
}

void VulkanRenderer::EnableDepth() {}

void VulkanRenderer::DisableDepth() {}

void VulkanRenderer::InitRectDrawing() {
	Matrix m = Matrix::getZeroMatrix();
	m._11 = 2.0f / m_surfaceWidth;
	m._22 = -2.0f / m_surfaceHeight;
	m._41 = -1;
	m._42 = 1;
	m._44 = 1;
	SetTransformMatrix(&m);
	SetFog();
	
	m_currentPipeline = m_pipeline2D;
	//ddImmediateContext->OMSetBlendState(alphaBlendState, {}, 0xFFFFFFFF);
	//ddImmediateContext->OMSetDepthStencilState(depthOffState, 0);
	//ddImmediateContext->PSSetShader(ddPixelShader, nullptr, 0);
	//ddImmediateContext->VSSetShader(ddVertexShader, nullptr, 0);
}

void VulkanRenderer::DrawRect(int x, int y, int w, int h, int c, float u, float v, float o, float p) {
	globalBuffers->shapeVertexBuffer.nextBuffer();
	const auto* buffer = globalBuffers->shapeVertexBuffer.getCurrentBuffer();

	//batchVertex* verts = (batchVertex*)buffer->mappedPtr;
	batchVertex verts[6];
	auto f = [](int n) {return (float)n; };
	verts[0].x = f(x);		verts[0].y = f(y);		verts[0].u = u;		verts[0].v = v;
	verts[1].x = f(x + w);	verts[1].y = f(y);		verts[1].u = u + o;	verts[1].v = v;
	verts[2].x = f(x);		verts[2].y = f(y + h);	verts[2].u = u;		verts[2].v = v + p;
	verts[3] = verts[2];
	verts[4] = verts[1];
	verts[5].x = f(x + w);	verts[5].y = f(y + h);	verts[5].u = u + o;	verts[5].v = v + p;
	for (int i = 0; i < 6; i++) { verts[i].color = c; verts[i].x -= 0.5f; verts[i].y -= 0.5f; verts[i].z = 0.0f; }

	vmaCopyMemoryToAllocation(m_vmaAllocator, verts, buffer->allocation[0], 0, 6 * sizeof(batchVertex));
	//vmaFlushAllocation(m_vmaAllocator, buffer->allocation[0], 0, 6 * sizeof(batchVertex));

	vk::DeviceSize offset = 0;
	vk::CommandBuffer cmdBuffer = createCommandBufferAndBegin();
	beginPass(cmdBuffer, m_pipeline2D);
	cmdBuffer.bindVertexBuffers(0, 1, &buffer->buffer[0], &offset);
	cmdBuffer.draw(6, 1, 0, 0);
	endPass(cmdBuffer);
	cmdBuffer.end();
	submitSingleCommandBuffer(cmdBuffer, buffer->fence);
}

void VulkanRenderer::DrawGradientRect(int x, int y, int w, int h, int c0, int c1, int c2, int c3) {

}

void VulkanRenderer::DrawFrame(int x, int y, int w, int h, int c) {
	globalBuffers->shapeVertexBuffer.nextBuffer();
	const auto* buffer = globalBuffers->shapeVertexBuffer.getCurrentBuffer();
	
	//batchVertex* verts = (batchVertex*)buffer->mappedPtr;
	batchVertex verts[8];
	auto f = [](int n) {return (float)n; };
	verts[0].x = f(x); verts[0].y = f(y); verts[0].z = 0.0f; verts[0].color = c; verts[0].u = 0.0f; verts[0].v = 0.0f;
	verts[1].x = f(x + w); verts[1].y = f(y); verts[1].z = 0.0f; verts[1].color = c; verts[1].u = 0.0f; verts[1].v = 0.0f;
	verts[2] = verts[1];
	verts[3].x = f(x + w); verts[3].y = f(y + h); verts[3].z = 0.0f; verts[3].color = c; verts[3].u = 0.0f; verts[3].v = 0.0f;
	verts[4] = verts[3];
	verts[5].x = f(x); verts[5].y = f(y + h); verts[5].z = 0.0f; verts[5].color = c; verts[5].u = 0.0f; verts[5].v = 0.0f;
	verts[6] = verts[5];
	verts[7] = verts[0];

	vmaCopyMemoryToAllocation(m_vmaAllocator, verts, buffer->allocation[0], 0, 8 * sizeof(batchVertex));

	vk::DeviceSize offset = 0;
	vk::CommandBuffer cmdBuffer = createCommandBufferAndBegin();
	beginPass(cmdBuffer, m_pipeline2DLines);
	cmdBuffer.bindVertexBuffers(0, 1, &buffer->buffer[0], &offset);
	cmdBuffer.draw(8, 1, 0, 0);
	endPass(cmdBuffer);
	cmdBuffer.end();
	submitSingleCommandBuffer(cmdBuffer, buffer->fence);
}

// 3D Landscape/Heightmap drawing

void VulkanRenderer::BeginMapDrawing() {
	m_currentPipeline = m_pipelineTerrain;
	//ddImmediateContext->PSSetShader(ddMapPixelShader, nullptr, 0);
	//ddImmediateContext->OMSetBlendState(nullptr, {}, 0xFFFFFFFF);
}

void VulkanRenderer::BeginLakeDrawing() {
	m_currentPipeline = m_pipelineLake;
	//ddImmediateContext->PSSetShader(ddLakePixelShader, nullptr, 0);
	//ddImmediateContext->OMSetBlendState(alphaBlendState, {}, 0xFFFFFFFF);
}

// 3D Mesh drawing

void VulkanRenderer::BeginMeshDrawing() {
	m_currentPipeline = m_pipeline3D;
	//ddImmediateContext->OMSetBlendState(nullptr, {}, 0xFFFFFFFF);
	//ddImmediateContext->OMSetDepthStencilState(depthOnState, 0);
	//ddImmediateContext->PSSetShader(ddAlphaTestPixelShader, nullptr, 0);
}

// Batch drawing

RBatch* VulkanRenderer::CreateBatch(int mv, int mi) {
	return new RBatchVulkan(this, mv, mi);
}

void VulkanRenderer::BeginBatchDrawing() {
	SetTriangleTopology();
}

int VulkanRenderer::ConvertColor(int c) { return c; }

// Buffer drawing

RVertexBuffer* VulkanRenderer::CreateVertexBuffer(int nv) {
	RVertexBufferVulkan* vb11 = new RVertexBufferVulkan(this, nv * sizeof(batchVertex));
	return vb11;
}

RIndexBuffer* VulkanRenderer::CreateIndexBuffer(int ni) {
	RIndexBufferVulkan* ib11 = new RIndexBufferVulkan(this, ni * 2);
	return ib11;
}

void VulkanRenderer::SetVertexBuffer(RVertexBuffer* _rv) {
	this->currentVertexBuffer = (RVertexBufferVulkan*)_rv;

}

void VulkanRenderer::SetIndexBuffer(RIndexBuffer* _ri) {
	this->currentIndexBuffer = (RIndexBufferVulkan*)_ri;
}

void VulkanRenderer::DrawBuffer(int first, int count) {
	assert(this->currentVertexBuffer);
	assert(this->currentIndexBuffer);
	vk::CommandBuffer cmdBuffer = createCommandBufferAndBegin();
	VkDeviceSize offset = 0;
	beginPass(cmdBuffer);
	cmdBuffer.bindVertexBuffers(0, 1, &currentVertexBuffer->buffer, &offset);
	cmdBuffer.bindIndexBuffer(currentIndexBuffer->buffer, 0, vk::IndexType::eUint16);
	cmdBuffer.drawIndexed(count, 1, first, 0, 0);
	endPass(cmdBuffer);
	cmdBuffer.end();
	m_vkDevice.resetFences(1, &m_nonDynamicBufferFence);
	submitSingleCommandBuffer(cmdBuffer, m_nonDynamicBufferFence);
}

// ImGui

void VulkanRenderer::InitImGuiDrawing() {
	InitRectDrawing();
	SetTriangleTopology();
}

void VulkanRenderer::BeginParticles() {
	m_currentPipeline = m_pipeline3DAlphaBlend;
	//ddImmediateContext->OMSetBlendState(alphaBlendState, {}, 0xFFFFFFFF);
	//ddImmediateContext->OMSetDepthStencilState(depthTestOnlyState, 0);
	//ddImmediateContext->PSSetShader(ddPixelShader, nullptr, 0);
	//ddImmediateContext->VSSetShader(fogEnabled ? ddFogVertexShader : ddVertexShader, nullptr, 0);
}

void VulkanRenderer::SetLineTopology() {
	m_primitiveTopology = vk::PrimitiveTopology::eLineList;
}

void VulkanRenderer::SetTriangleTopology() {
	m_primitiveTopology = vk::PrimitiveTopology::eTriangleList;
}

IRenderer* CreateVulkanRenderer() { return new VulkanRenderer; }
