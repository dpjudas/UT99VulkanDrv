
#include "Precomp.h"
#include "SceneBuffers.h"
#include "VulkanBuilders.h"

SceneBuffers::SceneBuffers(VulkanDevice *device, int width, int height) : width(width), height(height)
{
	BufferBuilder bufbuild;
	bufbuild.setSize(sizeof(SceneUniforms));

	bufbuild.setUsage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	sceneUniforms = bufbuild.create(device);

	bufbuild.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	stagingSceneUniforms = bufbuild.create(device);

	createImage(colorBuffer, colorBufferView, device, width, height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
	createImage(depthBuffer, depthBufferView, device, width, height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
}

SceneBuffers::~SceneBuffers()
{
}

void SceneBuffers::createImage(std::unique_ptr<VulkanImage> &image, std::unique_ptr<VulkanImageView> &view, VulkanDevice *device, int width, int height, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect)
{
	ImageBuilder imgbuild;
	imgbuild.setFormat(format);
	imgbuild.setUsage(usage);
	imgbuild.setSize(width, height);
	image = imgbuild.create(device);

	ImageViewBuilder viewbuild;
	viewbuild.setImage(image.get(), format, aspect);
	view = viewbuild.create(device);
}
