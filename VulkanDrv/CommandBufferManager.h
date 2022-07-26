#pragma once

#include "VulkanObjects.h"

class UVulkanRenderDevice;
class Postprocess;
class VulkanPostprocess;

class CommandBufferManager
{
public:
	CommandBufferManager(UVulkanRenderDevice* renderer);
	~CommandBufferManager();

	void BeginFrame();
	void EndFrame();

	void SubmitCommands(bool present, int presentWidth, int presentHeight);
	VulkanCommandBuffer* GetTransferCommands();
	VulkanCommandBuffer* GetDrawCommands();
	void DeleteFrameObjects();

	void CopyScreenToBuffer(int w, int h, void* data, float gamma);

	struct DeleteList
	{
		std::vector<std::unique_ptr<VulkanImage>> images;
		std::vector<std::unique_ptr<VulkanImageView>> imageViews;
		std::vector<std::unique_ptr<VulkanBuffer>> buffers;
		std::vector<std::unique_ptr<VulkanDescriptorSet>> descriptors;
	};
	DeleteList* FrameDeleteList = nullptr;

	Postprocess* PostprocessModel = nullptr;
	VulkanPostprocess* Postprocess = nullptr;

	VulkanSwapChain* SwapChain = nullptr;
	uint32_t PresentImageIndex = 0xffffffff;

private:
	UVulkanRenderDevice* renderer = nullptr;

	VulkanSemaphore* ImageAvailableSemaphore = nullptr;
	VulkanSemaphore* RenderFinishedSemaphore = nullptr;
	VulkanSemaphore* TransferSemaphore = nullptr;
	VulkanFence* RenderFinishedFence = nullptr;
	VulkanCommandPool* CommandPool = nullptr;
	VulkanCommandBuffer* DrawCommands = nullptr;
	VulkanCommandBuffer* TransferCommands = nullptr;
};
