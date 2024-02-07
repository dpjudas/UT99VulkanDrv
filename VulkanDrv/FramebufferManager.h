#pragma once

#include "SceneTextures.h"

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

	std::unique_ptr<VulkanFramebuffer> SceneFramebuffer;
	std::unique_ptr<VulkanFramebuffer> PPImageFB[2];

	struct
	{
		std::unique_ptr<VulkanFramebuffer> VTextureFB;
		std::unique_ptr<VulkanFramebuffer> HTextureFB;
	} BloomBlurLevels[NumBloomLevels];

private:
	UVulkanRenderDevice* renderer = nullptr;
	std::vector<std::unique_ptr<VulkanFramebuffer>> SwapChainFramebuffers;
};
