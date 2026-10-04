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
#ifdef _WIN32
#include <vma/vk_mem_alloc.h>
#else
#ifdef __ANDROID__
#define VMA_VULKAN_VERSION 1001000
#endif
#include <vk_mem_alloc.h>
#endif

#include <SDL.h>
#include <SDL_vulkan.h>
extern SDL_Window* g_sdlWindow;

struct VulkanRenderer;
struct RVertexBufferVulkan;
struct RIndexBufferVulkan;

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
		GlobalBuffers(VulkanRenderer* gfx)
			: shapeVertexBuffer(gfx, 64 * sizeof(batchVertex), vk::BufferUsageFlagBits::eVertexBuffer),
			transformBuffer(gfx, 64, vk::BufferUsageFlagBits::eTransferSrc),
			fogBuffer(gfx, 32, vk::BufferUsageFlagBits::eTransferSrc)
		{
		}
		GlobalBuffers(const GlobalBuffers&) = delete;
	};
	std::unique_ptr<GlobalBuffers> m_globalBuffers;
	vk::Buffer m_currentTransformBuffer; VmaAllocation m_currentTransformBufferAlloc;
	vk::Buffer m_currentFogBuffer; VmaAllocation m_currentFogBufferAlloc;
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

	void setDescriptors(std::optional<vk::Buffer> transformBuffer, std::optional<vk::Buffer> fogBuffer) {
		vk::DescriptorBufferInfo dbInfoTransform;
		vk::DescriptorBufferInfo dbInfoFog;

		vk::WriteDescriptorSet writes[2];
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

		m_vkDevice.updateDescriptorSets(index, writes, 0, nullptr);
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

		if(pipeline != m_currentPassPipeline)
		{
			if(!pipeline) {
				cmdBuffer.endRenderPass();
			}
			else if(!m_currentPassPipeline) {
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

		if(pipeline) {
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

struct RGeneralBufferVulkan
{
	VulkanRenderer* const gfx;

	vk::Buffer buffer;
	VmaAllocation allocation;
	
	std::unique_ptr<DynamicBuffer<1>> stageBuffer;

	RGeneralBufferVulkan(VulkanRenderer* gfx, int size, vk::BufferUsageFlags usage)
		: gfx(gfx), stageBuffer(std::make_unique<DynamicBuffer<1>>(gfx, size, vk::BufferUsageFlagBits::eTransferSrc))
	{
		VmaAllocationCreateInfo allocationCreateInfo;
		memset(&allocationCreateInfo, 0, sizeof(allocationCreateInfo));
		allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

		vk::BufferCreateInfo bufferCreateInfo;
		bufferCreateInfo.size = size;
		bufferCreateInfo.usage = usage | vk::BufferUsageFlagBits::eTransferDst;
		bufferCreateInfo.sharingMode = vk::SharingMode::eExclusive;

		VmaAllocationInfo allocInfo;
		VkBuffer tbuffer;
		vmaCreateBuffer(gfx->m_vmaAllocator, &(VkBufferCreateInfo&)bufferCreateInfo, &allocationCreateInfo, &tbuffer, &allocation, &allocInfo);
		buffer = vk::Buffer(tbuffer);
	}

	~RGeneralBufferVulkan()
	{
		gfx->currentFrameObject().buffersToDelete.push_back({ buffer, allocation, std::move(stageBuffer)});
	}
};

struct RVertexBufferVulkan : public RVertexBuffer, RGeneralBufferVulkan
{
	RVertexBufferVulkan(VulkanRenderer* gfx, int size)
		: RGeneralBufferVulkan(gfx, size, vk::BufferUsageFlagBits::eVertexBuffer) {
		this->size = size;
	}

	batchVertex* lock()
	{
		if (!dirty) {
			stageBuffer->nextBuffer();
			dirty = true;
		}
		return (batchVertex*)stageBuffer->getCurrentBuffer()->mappedPtr[0];
	}
	void unlock()
	{
		vmaFlushAllocation(gfx->m_vmaAllocator, stageBuffer->getCurrentBuffer()->allocation[0], 0, size);
	}

	bool dirty = false;
};

struct RIndexBufferVulkan : public RIndexBuffer, RGeneralBufferVulkan
{
	RIndexBufferVulkan(VulkanRenderer* gfx, int size)
		: RGeneralBufferVulkan(gfx, size, vk::BufferUsageFlagBits::eIndexBuffer) {
		this->size = size;
	}

	uint16_t* lock()
	{
		if (!dirty) {
			stageBuffer->nextBuffer();
			dirty = true;
		}
		return (uint16_t*)stageBuffer->getCurrentBuffer()->mappedPtr[0];
	}
	void unlock()
	{
		vmaFlushAllocation(gfx->m_vmaAllocator, stageBuffer->getCurrentBuffer()->allocation[0], 0, size);
	}

	bool dirty = false;
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
		if (!curverts || !curindis) {
			curverts = curindis = 0;
			return;
		}
		if (locked) unlock();

		auto* currentBuf = dynamicBuffer.getCurrentBuffer();
		
		vk::DeviceSize flushOffsets[2] = { 0, 0 };
		vk::DeviceSize flushSizes[2] = { curverts * sizeof(batchVertex), curindis * 2 };
		vmaFlushAllocations(gfx->m_vmaAllocator, 2, currentBuf->allocation, flushOffsets, flushSizes);

		vk::DeviceSize offset = 0;
		auto& frame = gfx->currentFrameObject();
		gfx->togglePass(gfx->m_currentPipeline);
		frame.mainCommandBuffer.bindVertexBuffers(0, 1, &currentBuf->buffer[0], &offset);
		frame.mainCommandBuffer.bindIndexBuffer(currentBuf->buffer[1], 0, vk::IndexType::eUint16);
		frame.mainCommandBuffer.drawIndexed(curindis, 1, 0, 0, 0);
		//gfx->togglePass(nullptr);

		curverts = curindis = 0;
	}

};

template<int NumSubs>
void DynamicBuffer<NumSubs>::addNewBuffer()
{
	auto& frame = frames[gfx->m_currentSwapchainImageIndex];
	auto& buffer = frame.buffers.emplace_back();
	vk::FenceCreateInfo fci;
	for (int i = 0; i < NumSubs; ++i) {
		VkBuffer vkbuf;
		VmaAllocation alloc;
		VmaAllocationInfo allocInfo;
		auto result = vmaCreateBuffer(gfx->m_vmaAllocator, &(VkBufferCreateInfo&)bufferCreateInfo[i],
			&allocationCreateInfo, &vkbuf, &alloc, &allocInfo);
		assert(result == VK_SUCCESS);
		buffer.buffer[i] = vkbuf;
		buffer.allocation[i] = alloc;
		buffer.mappedPtr[i] = allocInfo.pMappedData;
		assert(buffer.mappedPtr);
	}
}

template<int NumSubs>
void DynamicBuffer<NumSubs>::reset()
{
	for (auto& frame : frames) {
		for (auto& inst : frame.buffers) {
			for (int i = 0; i < NumSubs; ++i)
				vmaDestroyBuffer(gfx->m_vmaAllocator, inst.buffer[i], inst.allocation[i]);
		}
		frame.buffers.clear();
	}
}


// once received, the fence HAS TO be given to a queue submission ASAP
// otherwise calling reset() (by destructor) will deadlock
template<int NumSubs>
const typename DynamicBuffer<NumSubs>::BufferInstance* DynamicBuffer<NumSubs>::getCurrentBuffer()
{
	const auto& frame = frames[gfx->m_currentSwapchainImageIndex];
	if (frame.buffers.empty())
		nextBuffer();
	return &frame.buffers[lastBuffer];
}

template<int NumSubs>
void DynamicBuffer<NumSubs>::nextBuffer()
{
	// all buffers are occupied -> create a new one
	lastBuffer += 1;
	if (lastBuffer == frames[gfx->m_currentSwapchainImageIndex].buffers.size())
		addNewBuffer();
}

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

	vk::CommandPoolCreateInfo commandPoolCreateInfo;
	commandPoolCreateInfo.queueFamilyIndex = m_queueFamilyIndex;
	commandPoolCreateInfo.flags = vk::CommandPoolCreateFlagBits::eTransient;
	m_texCommandPool = m_vkDevice.createCommandPool(commandPoolCreateInfo);

	vk::CommandBufferAllocateInfo cbaInfo;
	cbaInfo.commandPool = m_texCommandPool;
	cbaInfo.level = vk::CommandBufferLevel::ePrimary;
	cbaInfo.commandBufferCount = 1;
	m_texCommandBuffer = m_vkDevice.allocateCommandBuffers(cbaInfo).at(0);

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
	scInfo.compareOp = vk::CompareOp::eNever;
	scInfo.minLod = 0.0f;
	scInfo.maxLod = VK_LOD_CLAMP_NONE;
	scInfo.borderColor = vk::BorderColor::eFloatTransparentBlack;
	scInfo.unnormalizedCoordinates = vk::False;
	m_vkSampler = m_vkDevice.createSampler(scInfo);

	vk::DescriptorSetLayoutBinding bindingsSet0[2];
	bindingsSet0[0].binding = 0;
	bindingsSet0[0].descriptorType = vk::DescriptorType::eUniformBuffer;
	bindingsSet0[0].descriptorCount = 1;
	bindingsSet0[0].stageFlags = vk::ShaderStageFlagBits::eVertex;
	bindingsSet0[1].binding = 1;
	bindingsSet0[1].descriptorType = vk::DescriptorType::eUniformBuffer;
	bindingsSet0[1].descriptorCount = 1;
	bindingsSet0[1].stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
	vk::DescriptorSetLayoutBinding bindingsSet1[1];
	bindingsSet1[0].binding = 0;
	bindingsSet1[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
	bindingsSet1[0].descriptorCount = 1;
	bindingsSet1[0].stageFlags = vk::ShaderStageFlagBits::eFragment;
	bindingsSet1[0].pImmutableSamplers = &m_vkSampler;

	vk::DescriptorSetLayoutCreateInfo dslcInfo;
	dslcInfo.bindingCount = std::size(bindingsSet0);
	dslcInfo.pBindings = bindingsSet0;
	m_vkDescSetLayout0 = m_vkDevice.createDescriptorSetLayout(dslcInfo);
	dslcInfo.bindingCount = std::size(bindingsSet1);
	dslcInfo.pBindings = bindingsSet1;
	m_vkDescSetLayout1 = m_vkDevice.createDescriptorSetLayout(dslcInfo);

	vk::PipelineLayoutCreateInfo plcInfo;
	vk::DescriptorSetLayout setLayouts[2] = { m_vkDescSetLayout0, m_vkDescSetLayout1 };
	plcInfo.setLayoutCount = std::size(setLayouts);
	plcInfo.pSetLayouts = setLayouts;
	m_vkPipelineLayout = m_vkDevice.createPipelineLayout(plcInfo);

	vk::DescriptorPoolSize descPoolSizes[] = {
		{vk::DescriptorType::eUniformBuffer, 2},
		{vk::DescriptorType::eCombinedImageSampler, 2048},
	};
	vk::DescriptorPoolCreateInfo dpcInfo;
	dpcInfo.maxSets = 2048;
	dpcInfo.poolSizeCount = std::size(descPoolSizes);
	dpcInfo.pPoolSizes = descPoolSizes;
	m_vkDescriptorPool = m_vkDevice.createDescriptorPool(dpcInfo);

	vk::DescriptorSetAllocateInfo dsaInfo;
	dsaInfo.descriptorPool = m_vkDescriptorPool;
	dsaInfo.descriptorSetCount = 1;
	dsaInfo.pSetLayouts = &m_vkDescSetLayout0;
	m_vkMainDescriptorSet = m_vkDevice.allocateDescriptorSets(dsaInfo).at(0);


	VmaAllocationCreateInfo uniformAllocCreateInfo;
	memset(&uniformAllocCreateInfo, 0, sizeof(uniformAllocCreateInfo));
	uniformAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	vk::BufferCreateInfo uniformCreateInfo;
	uniformCreateInfo.size = 64;
	uniformCreateInfo.usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst;
	uniformCreateInfo.sharingMode = vk::SharingMode::eExclusive;

	VkBuffer bufferTemp;
	vmaCreateBuffer(m_vmaAllocator, &(VkBufferCreateInfo&)uniformCreateInfo, &uniformAllocCreateInfo, &bufferTemp, &m_currentTransformBufferAlloc, nullptr);
	m_currentTransformBuffer = bufferTemp;
	uniformCreateInfo.size = 32;
	vmaCreateBuffer(m_vmaAllocator, &(VkBufferCreateInfo&)uniformCreateInfo, &uniformAllocCreateInfo, &bufferTemp, &m_currentFogBufferAlloc, nullptr);
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

	// Buffers
	m_globalBuffers = std::make_unique<GlobalBuffers>(this);

	Bitmap whiteBmp;
	whiteBmp.width = 1;
	whiteBmp.height = 1;
	whiteBmp.format = BMFORMAT_R8G8B8A8;
	whiteBmp.pixels.resize(4);
	*(uint32_t*)whiteBmp.pixels.data() = 0xFFFFFFFF;
	m_whiteTexture = CreateTexture(whiteBmp, 1);

	setDescriptors(m_currentTransformBuffer, m_currentFogBuffer);
	m_currentTextureDescriptorSet = m_imageViewToDescriptorSetMap.at(VkImageView(m_whiteTexture));

	Reset();
}

void VulkanRenderer::Reset() {
	m_vkDevice.waitIdle();

	m_surfaceWidth = g_windowWidth;
	m_surfaceHeight = g_windowHeight;

	// Destroy old stuff

	for (const auto& swapchainImg : m_vkSwapchainImages) {
		for (const auto& [buffer, allocation, stageDynBuffer] : swapchainImg.buffersToDelete) {
			vmaDestroyBuffer(m_vmaAllocator, buffer, allocation);
		}

		m_vkDevice.destroyCommandPool(swapchainImg.commandPool);

		m_vkDevice.destroyFramebuffer(swapchainImg.framebuffer);
		m_vkDevice.destroyImageView(swapchainImg.imageView);
		m_vkDevice.destroyImageView(swapchainImg.depthImageView);
		vmaDestroyImage(m_vmaAllocator, swapchainImg.depthImage, swapchainImg.depthAllocation);
	}
	m_vkSwapchainImages.clear();

	if (m_vkSwapchain)
		m_vkDevice.destroySwapchainKHR(m_vkSwapchain);

	m_vkSwapchain = nullptr;

	for (const auto& semaphore : m_swapchainSemaphorePool)
		m_vkDevice.destroySemaphore(semaphore);
	m_swapchainSemaphorePool.clear();

	// Create swapchain

	vk::SurfaceCapabilitiesKHR surfCaps = m_vkPhysicalDevice.getSurfaceCapabilitiesKHR(m_vkSurface);

	vk::SwapchainCreateInfoKHR swapchainCreateInfo;
	swapchainCreateInfo.surface = m_vkSurface;
	swapchainCreateInfo.minImageCount = 3;
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
	assert(vkSwapchainImages.size() <= MAX_SWAPCHAIN_IMAGES);

	vk::SemaphoreCreateInfo semaInfo;

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
		auto result = vmaCreateImage(m_vmaAllocator, &(VkImageCreateInfo&)ici, &allocCreateInfo, &depthImage, &si.depthAllocation, nullptr);
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

		si.commandSemaphore = m_vkDevice.createSemaphore(semaInfo);

		vk::FenceCreateInfo fenceInfo;
		fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;
		si.fence = m_vkDevice.createFence(fenceInfo);

		vk::CommandPoolCreateInfo commandPoolCreateInfo;
		commandPoolCreateInfo.queueFamilyIndex = m_queueFamilyIndex;
		commandPoolCreateInfo.flags = vk::CommandPoolCreateFlagBits::eTransient;
		si.commandPool = m_vkDevice.createCommandPool(commandPoolCreateInfo);

		vk::CommandBufferAllocateInfo cbaInfo;
		cbaInfo.commandPool = si.commandPool;
		cbaInfo.level = vk::CommandBufferLevel::ePrimary;
		cbaInfo.commandBufferCount = 1;
		si.mainCommandBuffer = m_vkDevice.allocateCommandBuffers(cbaInfo).at(0);
	}

	for (size_t i = 0; i < vkSwapchainImages.size() + 1; ++i) {
		m_swapchainSemaphorePool.push_back(m_vkDevice.createSemaphore(semaInfo));
	}
}

void VulkanRenderer::BeginDrawing() {

	DynamicBuffer<1>::onFrameBegin();
	DynamicBuffer<2>::onFrameBegin();

	vk::Semaphore swapchainSemaphore = m_swapchainSemaphorePool[m_nextSwapchainSemaphoreIndex];
	m_nextSwapchainSemaphoreIndex = (m_nextSwapchainSemaphoreIndex + 1) % m_swapchainSemaphorePool.size();

	auto nextImageRes = m_vkDevice.acquireNextImageKHR(m_vkSwapchain, UINT64_MAX, swapchainSemaphore, {});
	assert((int)nextImageRes.result >= 0);
	m_currentSwapchainImageIndex = nextImageRes.value;

	auto& frame = currentFrameObject();
	frame.swapchainSemaphore = swapchainSemaphore;

	auto rr = m_vkDevice.waitForFences(1, &(frame.fence), vk::True, 10'000'000'000);
	assert(rr == vk::Result::eSuccess);
	m_vkDevice.resetFences(frame.fence);

	// Clean stuff from previous frame

	m_vkDevice.resetCommandPool(frame.commandPool, vk::CommandPoolResetFlags());

	for (const auto& [buffer, allocation, _] : frame.buffersToDelete) {
		vmaDestroyBuffer(m_vmaAllocator, buffer, allocation);
	}
	frame.buffersToDelete.clear();

	// ---

	m_viewport.x = 0.0f;
	m_viewport.y = (float)m_surfaceHeight;
	m_viewport.width = (float)m_surfaceWidth;
	m_viewport.height = -(float)m_surfaceHeight;
	m_viewport.minDepth = 0.0f;
	m_viewport.maxDepth = 1.0f;

	DisableScissor();

	m_currentTextureDescriptorSet = m_imageViewToDescriptorSetMap.at(VkImageView(m_whiteTexture));

	m_fogEnabled = false;
	m_currentPipeline = nullptr; // it needs to be decided by the caller!
	m_primitiveTopology = vk::PrimitiveTopology::eTriangleList;

	m_currentPassPipeline = nullptr;

	// ---

	vk::ImageMemoryBarrier imgBarrier0;
	imgBarrier0.srcAccessMask = vk::AccessFlagBits::eNone;
	imgBarrier0.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead;
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
	imgBarrier1.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentRead;
	imgBarrier1.oldLayout = vk::ImageLayout::eUndefined;
	imgBarrier1.newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
	imgBarrier1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imgBarrier1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imgBarrier1.image = m_vkSwapchainImages[m_currentSwapchainImageIndex].depthImage;
	imgBarrier1.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;

	vk::CommandBufferBeginInfo beginInfo;
	beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

	frame.mainCommandBuffer.begin(beginInfo);

	frame.mainCommandBuffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::DependencyFlags(),
		0, nullptr, 0, nullptr, 1, &imgBarrier0);
	frame.mainCommandBuffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eEarlyFragmentTests, vk::DependencyFlags(),
		0, nullptr, 0, nullptr, 1, &imgBarrier1);

	vk::ClearAttachment clearAttachments[2];
	uint32_t numClearAttachments = 0;
	if (m_clearColor) {
		const uint32_t color = *m_clearColor;
		auto& clearAttach = clearAttachments[numClearAttachments++];
		clearAttach.aspectMask = vk::ImageAspectFlagBits::eColor;
		clearAttach.colorAttachment = 0;
		auto& cc = clearAttach.clearValue.color.float32;
		cc[0] = (float)((color >> 16) & 255) / 255.0f;
		cc[1] = (float)((color >> 8) & 255) / 255.0f;
		cc[2] = (float)((color >> 0) & 255) / 255.0f;
		cc[3] = (float)((color >> 24) & 255) / 255.0f;
	}
	if (m_clearDepth) {
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
	togglePass(m_pipeline2D);
	frame.mainCommandBuffer.clearAttachments(numClearAttachments, clearAttachments, 1, &clearRect);
	togglePass(nullptr);
}

void VulkanRenderer::EndDrawing() {
	auto& frame = currentFrameObject();

	togglePass(nullptr);

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

	frame.mainCommandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &imgBarrier1);
	frame.mainCommandBuffer.end();

	const vk::PipelineStageFlags stageToWaitOn[1] = { vk::PipelineStageFlagBits::eAllCommands };

	vk::SubmitInfo submit;
	submit.waitSemaphoreCount = 1;
	submit.pWaitSemaphores = &frame.swapchainSemaphore;
	submit.pWaitDstStageMask = stageToWaitOn;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &frame.mainCommandBuffer;
	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores = &frame.commandSemaphore;
	m_vkQueue.submit(submit, frame.fence);

	vk::PresentInfoKHR pinfo;
	pinfo.waitSemaphoreCount = 1;
	pinfo.pWaitSemaphores = &frame.commandSemaphore;
	pinfo.swapchainCount = 1;
	pinfo.pSwapchains = &m_vkSwapchain;
	pinfo.pImageIndices = &m_currentSwapchainImageIndex;
	m_vkQueue.presentKHR(pinfo);
}

void VulkanRenderer::ClearFrame(bool clearColors, bool clearDepth, uint32_t color) {
	// TEMP
	// We cannot clear immediately outside Begin/EndDrawing.
	// Instead we store the values and do the actual clearing inside BeginDrawing.
	m_clearColor = clearColors ? std::make_optional(color) : std::nullopt;
	m_clearDepth = clearDepth;
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
	auto res = vmaCreateImage(m_vmaAllocator, &(VkImageCreateInfo&)desc, &acInfo, &imageHandle, &allocationHandle, &allocationInfo);
	assert(res == VK_SUCCESS);

	//

	acInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT
		| VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	acInfo.usage = VMA_MEMORY_USAGE_AUTO;

	VkBuffer stageBuffer;
	VmaAllocation stageAllocation;
	VmaAllocationInfo stageAllocationInfo;

	vmaCreateBuffer(m_vmaAllocator, &(VkBufferCreateInfo&)bcInfo, &acInfo, &stageBuffer, &stageAllocation, &stageAllocationInfo);

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

		vk::CommandBufferBeginInfo beginInfo;
		beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
		m_texCommandBuffer.begin(beginInfo);

		m_texCommandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &imgBarrier1);
		m_texCommandBuffer.copyBufferToImage(stageBuffer, imageHandle, vk::ImageLayout::eTransferDstOptimal, 1, &bic);
		m_texCommandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &imgBarrier2);
		m_texCommandBuffer.end();
		
		vk::SubmitInfo submit;
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &m_texCommandBuffer;
		m_vkQueue.submit(submit);

		m_vkQueue.waitIdle();
		m_vkDevice.resetCommandPool(m_texCommandPool);
	}

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

	vk::DescriptorSetAllocateInfo dsaInfo;
	dsaInfo.descriptorPool = m_vkDescriptorPool;
	dsaInfo.descriptorSetCount = 1;
	dsaInfo.pSetLayouts = &m_vkDescSetLayout1;
	vk::DescriptorSet imgDescSet = m_vkDevice.allocateDescriptorSets(dsaInfo).at(0);

	vk::DescriptorImageInfo dbInfoTexture;
	dbInfoTexture.imageView = imageView;
	dbInfoTexture.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

	vk::WriteDescriptorSet wds;
	wds.dstSet = imgDescSet;
	wds.dstBinding = 0;
	wds.dstArrayElement = 0;
	wds.descriptorCount = 1;
	wds.descriptorType = vk::DescriptorType::eCombinedImageSampler;
	wds.pImageInfo = &dbInfoTexture;
	m_vkDevice.updateDescriptorSets(wds, {});

	m_imageViewToImageMap[imageView] = imageHandle;
	m_imageViewToDescriptorSetMap[imageView] = imgDescSet;
	return imageView;
}

void VulkanRenderer::FreeTexture(texture t) {}

void VulkanRenderer::UpdateTexture(texture t, const Bitmap& bmp) {}

// State changes

void VulkanRenderer::SetTransformMatrix(const Matrix* m) {
	m_globalBuffers->transformBuffer.nextBuffer();
	auto* stage = m_globalBuffers->transformBuffer.getCurrentBuffer();

	void* transformBytes = stage->mappedPtr[0];
	*(Matrix*)transformBytes = m->getTranspose();
	vmaFlushAllocation(m_vmaAllocator, stage->allocation[0], 0, 64);

	vk::BufferCopy copy;
	copy.srcOffset = 0;
	copy.dstOffset = 0;
	copy.size = 64;

	vk::BufferMemoryBarrier barrier;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer = m_currentTransformBuffer;
	barrier.offset = 0;
	barrier.size = vk::WholeSize;

	auto& frame = currentFrameObject();

	togglePass(nullptr);

	barrier.srcAccessMask = vk::AccessFlagBits::eUniformRead | vk::AccessFlagBits::eTransferWrite;
	barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
	frame.mainCommandBuffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(),
		0, nullptr, 1, &barrier, 0, nullptr);

	frame.mainCommandBuffer.copyBuffer(stage->buffer[0], m_currentTransformBuffer, 1, &copy);

	barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
	barrier.dstAccessMask = vk::AccessFlagBits::eUniformRead;
	frame.mainCommandBuffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexShader, vk::DependencyFlags(),
		0, nullptr, 1, &barrier, 0, nullptr);
}

void VulkanRenderer::SetTexture(uint32_t x, texture t) {
	if (t == nullptr)
		NoTexture(x);
	else
		m_currentTextureDescriptorSet = m_imageViewToDescriptorSetMap.at(VkImageView(t));
}

void VulkanRenderer::NoTexture(uint32_t x) {
	m_currentTextureDescriptorSet = m_imageViewToDescriptorSetMap.at(VkImageView(m_whiteTexture));
}

void VulkanRenderer::SetFog(uint32_t color, float farz) {
	m_globalBuffers->fogBuffer.nextBuffer();
	auto* stage = m_globalBuffers->fogBuffer.getCurrentBuffer();

	float* cc = (float*)stage->mappedPtr[0];
	cc[0] = (float)((color >> 16) & 255) / 255.0f;
	cc[1] = (float)((color >> 8) & 255) / 255.0f;
	cc[2] = (float)((color >> 0) & 255) / 255.0f;
	cc[3] = (float)((color >> 24) & 255) / 255.0f;
	cc[4] = farz * 0.5f;
	cc[5] = farz;

	vmaFlushAllocation(m_vmaAllocator, stage->allocation[0], 0, 24);

	vk::BufferCopy copy;
	copy.srcOffset = 0;
	copy.dstOffset = 0;
	copy.size = 32;

	vk::BufferMemoryBarrier barrier;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer = m_currentFogBuffer;
	barrier.offset = 0;
	barrier.size = vk::WholeSize;

	auto& frame = currentFrameObject();

	togglePass(nullptr);

	barrier.srcAccessMask = vk::AccessFlagBits::eUniformRead | vk::AccessFlagBits::eTransferWrite;
	barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
	frame.mainCommandBuffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(),
		0, nullptr, 1, &barrier, 0, nullptr);

	frame.mainCommandBuffer.copyBuffer(stage->buffer[0], m_currentFogBuffer, 1, &copy);

	barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
	barrier.dstAccessMask = vk::AccessFlagBits::eUniformRead;
	frame.mainCommandBuffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexShader, vk::DependencyFlags(),
		0, nullptr, 1, &barrier, 0, nullptr);

	m_fogEnabled = true;
}

void VulkanRenderer::DisableFog() {
	m_fogEnabled = false;
}

void VulkanRenderer::EnableAlphaTest() {
	if (m_currentPipeline == m_pipeline3D)
		m_currentPipeline = m_pipeline3DAlphaTest;
}

void VulkanRenderer::DisableAlphaTest() {
	if (m_currentPipeline == m_pipeline3DAlphaTest)
		m_currentPipeline = m_pipeline3D;
}

void VulkanRenderer::EnableColorBlend() {}

void VulkanRenderer::DisableColorBlend() {}

void VulkanRenderer::SetBlendColor(int c) {}

void VulkanRenderer::EnableAlphaBlend() {}

void VulkanRenderer::DisableAlphaBlend() {}

void VulkanRenderer::EnableScissor() {
}

void VulkanRenderer::DisableScissor() {
	SetScissorRect(0, 0, m_surfaceWidth, m_surfaceHeight);
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
}

void VulkanRenderer::DrawRect(int x, int y, int w, int h, int c, float u, float v, float o, float p) {
	m_globalBuffers->shapeVertexBuffer.nextBuffer();
	const auto* buffer = m_globalBuffers->shapeVertexBuffer.getCurrentBuffer();

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
	auto& frame = currentFrameObject();
	togglePass(m_pipeline2D);
	frame.mainCommandBuffer.bindVertexBuffers(0, 1, &buffer->buffer[0], &offset);
	frame.mainCommandBuffer.draw(6, 1, 0, 0);
}

void VulkanRenderer::DrawGradientRect(int x, int y, int w, int h, int c0, int c1, int c2, int c3) {

}

void VulkanRenderer::DrawFrame(int x, int y, int w, int h, int c) {
	m_globalBuffers->shapeVertexBuffer.nextBuffer();
	const auto* buffer = m_globalBuffers->shapeVertexBuffer.getCurrentBuffer();
	
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
	auto& frame = currentFrameObject();
	togglePass(m_pipeline2DLines);
	frame.mainCommandBuffer.bindVertexBuffers(0, 1, &buffer->buffer[0], &offset);
	frame.mainCommandBuffer.draw(8, 1, 0, 0);
}

// 3D Landscape/Heightmap drawing

void VulkanRenderer::BeginMapDrawing() {
	m_currentPipeline = m_pipelineTerrain;
}

void VulkanRenderer::BeginLakeDrawing() {
	m_currentPipeline = m_pipelineLake;
}

// 3D Mesh drawing

void VulkanRenderer::BeginMeshDrawing() {
	m_currentPipeline = m_pipeline3D;
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
	this->m_currentVertexBuffer = (RVertexBufferVulkan*)_rv;

}

void VulkanRenderer::SetIndexBuffer(RIndexBuffer* _ri) {
	this->m_currentIndexBuffer = (RIndexBufferVulkan*)_ri;
}

void VulkanRenderer::DrawBuffer(int first, int count) {
	assert(this->m_currentVertexBuffer);
	assert(this->m_currentIndexBuffer);

	auto& frame = currentFrameObject();

	vk::BufferCopy bc;
	bc.srcOffset = 0;
	bc.dstOffset = 0;

	vk::BufferMemoryBarrier barriers[2];
	int barrierIndex = 0;

	if (m_currentVertexBuffer->dirty) {
		togglePass(nullptr);
		bc.size = m_currentVertexBuffer->size;
		frame.mainCommandBuffer.copyBuffer(m_currentVertexBuffer->stageBuffer->getCurrentBuffer()->buffer[0], m_currentVertexBuffer->buffer, bc);
		m_currentVertexBuffer->dirty = false;

		auto& barrier = barriers[barrierIndex++];
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eVertexAttributeRead;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.buffer = m_currentVertexBuffer->buffer;
		barrier.offset = 0;
		barrier.size = vk::WholeSize;
	}
	if (m_currentIndexBuffer->dirty) {
		togglePass(nullptr);
		bc.size = m_currentIndexBuffer->size;
		frame.mainCommandBuffer.copyBuffer(m_currentIndexBuffer->stageBuffer->getCurrentBuffer()->buffer[0], m_currentIndexBuffer->buffer, bc);
		m_currentIndexBuffer->dirty = false;
		
		auto& barrier = barriers[barrierIndex++];
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eIndexRead;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.buffer = m_currentIndexBuffer->buffer;
		barrier.offset = 0;
		barrier.size = vk::WholeSize;
	}

	if (barrierIndex > 0) {
		frame.mainCommandBuffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexInput, vk::DependencyFlags(),
			0, nullptr, barrierIndex, barriers, 0, nullptr);
	}

	VkDeviceSize offset = 0;
	togglePass(m_currentPipeline);
	frame.mainCommandBuffer.bindVertexBuffers(0, 1, &m_currentVertexBuffer->buffer, &offset);
	frame.mainCommandBuffer.bindIndexBuffer(m_currentIndexBuffer->buffer, 0, vk::IndexType::eUint16);
	frame.mainCommandBuffer.drawIndexed(count, 1, first, 0, 0);
}

// ImGui

void VulkanRenderer::InitImGuiDrawing() {
	InitRectDrawing();
	SetTriangleTopology();
}

void VulkanRenderer::BeginParticles() {
	m_currentPipeline = m_pipeline3DAlphaBlend;
}

void VulkanRenderer::SetLineTopology() {
	m_primitiveTopology = vk::PrimitiveTopology::eLineList;
}

void VulkanRenderer::SetTriangleTopology() {
	m_primitiveTopology = vk::PrimitiveTopology::eTriangleList;
}

IRenderer* CreateVulkanRenderer() { return new VulkanRenderer; }
