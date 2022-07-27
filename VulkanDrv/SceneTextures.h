#pragma once

#include "VulkanObjects.h"
#include "ShaderManager.h"
#include "mat.h"
#include "vec.h"

class UVulkanRenderDevice;

class SceneTextures
{
public:
	SceneTextures(UVulkanRenderDevice* renderer, int width, int height, int multisample);
	~SceneTextures();

	// Current active multisample setting
	VkSampleCountFlagBits sceneSamples = VK_SAMPLE_COUNT_1_BIT;

	// Scene framebuffer color image
	std::unique_ptr<VulkanImage> colorBuffer;
	std::unique_ptr<VulkanImageView> colorBufferView;

	// Scene framebuffer depth buffer
	std::unique_ptr<VulkanImage> depthBuffer;
	std::unique_ptr<VulkanImageView> depthBufferView;

	// Post processing image buffers
	std::unique_ptr<VulkanImage> ppImage;
	std::unique_ptr<VulkanImageView> ppImageView;

	// Size of the scene framebuffer
	int width = 0;
	int height = 0;

private:
	static VkSampleCountFlagBits GetBestSampleCount(VulkanDevice* device, int multisample);
};
