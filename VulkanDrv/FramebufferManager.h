#pragma once

class UVulkanRenderDevice;

class FramebufferManager
{
public:
	FramebufferManager(UVulkanRenderDevice* renderer);

	void CreateSceneFramebuffer();
	void DestroySceneFramebuffer();

	void CreateSwapChainFramebuffers();
	void DestroySwapChainFramebuffers();

	VulkanFramebuffer* GetSwapChainFramebuffer();

	std::unique_ptr<VulkanFramebuffer> sceneFramebuffer;

private:
	UVulkanRenderDevice* renderer = nullptr;
	std::vector<std::unique_ptr<VulkanFramebuffer>> swapChainFramebuffers;
};
