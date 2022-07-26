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

	VkSampleCountFlagBits sceneSamples = VK_SAMPLE_COUNT_1_BIT;

	std::unique_ptr<VulkanImage> colorBuffer;
	std::unique_ptr<VulkanImageView> colorBufferView;
	VkImageLayout colorBufferLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	std::unique_ptr<VulkanImage> depthBuffer;
	std::unique_ptr<VulkanImageView> depthBufferView;

	int width = 0;
	int height = 0;

private:
	static void createImage(std::unique_ptr<VulkanImage> &image, std::unique_ptr<VulkanImageView> &view, VulkanDevice *device, int width, int height, VkSampleCountFlagBits samples, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect);
	static VkSampleCountFlagBits getBestSampleCount(VulkanDevice* device, int multisample);
};
