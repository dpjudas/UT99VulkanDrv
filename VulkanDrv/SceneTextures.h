#pragma once

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
	VkSampleCountFlagBits SceneSamples = VK_SAMPLE_COUNT_1_BIT;

	// Scene framebuffer color image
	std::unique_ptr<VulkanImage> ColorBuffer;
	std::unique_ptr<VulkanImageView> ColorBufferView;

	// Scene framebuffer hit results
	std::unique_ptr<VulkanImage> HitBuffer;
	std::unique_ptr<VulkanImageView> HitBufferView;

	// Scene framebuffer depth buffer
	std::unique_ptr<VulkanImage> DepthBuffer;
	std::unique_ptr<VulkanImageView> DepthBufferView;

	// Post processing image buffers
	std::unique_ptr<VulkanImage> PPImage;
	std::unique_ptr<VulkanImageView> PPImageView;

	// Texture and buffer used to download the hitbuffer
	std::unique_ptr<VulkanImage> PPHitBuffer;
	std::unique_ptr<VulkanBuffer> StagingHitBuffer;

	// Size of the scene framebuffer
	int width = 0;
	int height = 0;
	int multisample = 0;

private:
	static VkSampleCountFlagBits GetBestSampleCount(VulkanDevice* device, int multisample);
};
