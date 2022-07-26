#pragma once

#include "VulkanObjects.h"

class UVulkanRenderDevice;

class FramebufferManager
{
public:
	FramebufferManager(UVulkanRenderDevice* renderer);

	void CreateSceneFramebuffer();
	void DestroySceneFramebuffer();

	VulkanFramebuffer* GetSwapChainFramebuffer();

	std::unique_ptr<VulkanFramebuffer> sceneFramebuffer;

private:
	UVulkanRenderDevice* renderer = nullptr;
	std::unique_ptr<VulkanFramebuffer> swapChainFramebuffer;
};
