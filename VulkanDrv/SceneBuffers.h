#pragma once

#include "VulkanObjects.h"
#include "mat.h"
#include "vec.h"

struct SceneUniforms
{
	mat4 worldToView;
	mat4 viewToProjection;
};

struct SceneVertex
{
	float x, y, z;
	float u, v, lu, lv;
	float r, g, b, a;
};

struct ScenePushConstants
{
	mat4 objectToProjection;
};

class SceneBuffers
{
public:
	SceneBuffers(VulkanDevice *device, int width, int height);
	~SceneBuffers();

	VkSampleCountFlagBits sceneSamples = VK_SAMPLE_COUNT_1_BIT;

	std::unique_ptr<VulkanBuffer> sceneUniforms;
	std::unique_ptr<VulkanBuffer> stagingSceneUniforms;

	std::unique_ptr<VulkanImage> colorBuffer;
	std::unique_ptr<VulkanImageView> colorBufferView;
	VkImageLayout colorBufferLayout = VK_IMAGE_LAYOUT_GENERAL;

	std::unique_ptr<VulkanImage> depthBuffer;
	std::unique_ptr<VulkanImageView> depthBufferView;

	int width = 0;
	int height = 0;
	SceneUniforms uniforms;

private:
	SceneBuffers(const SceneBuffers &) = delete;
	SceneBuffers &operator=(const SceneBuffers &) = delete;

	void createImage(std::unique_ptr<VulkanImage> &image, std::unique_ptr<VulkanImageView> &view, VulkanDevice *device, int width, int height, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect);
};
