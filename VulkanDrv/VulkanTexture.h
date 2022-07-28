#pragma once

#include "VulkanObjects.h"
#include <functional>

class UVulkanRenderDevice;
struct FTextureInfo;

struct UploadedData
{
	VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
	int width = 1;
	int height = 1;
	std::unique_ptr<VulkanBuffer> stagingbuffer;
	std::vector<VkBufferImageCopy> miplevels;
};

class VulkanTexture
{
public:
	VulkanTexture(UVulkanRenderDevice* renderer, const FTextureInfo& Info, bool masked);
	~VulkanTexture();

	void Update(UVulkanRenderDevice* renderer, const FTextureInfo& Info, bool masked);
	void UpdateRect(UVulkanRenderDevice* renderer, FTextureInfo& Info, int x, int y, int w, int h);

	std::unique_ptr<VulkanImage> image;
	std::unique_ptr<VulkanImageView> imageView;

	int BindlessIndex[4] = { -1, -1, -1, -1 };

private:
	UploadedData UploadData(UVulkanRenderDevice* renderer, const FTextureInfo& Info, bool masked, VkFormat imageFormat, std::function<int(FMipmapBase* mip)> calcMipSize, std::function<void(FMipmapBase* mip, void* dst)> copyMip = {});
	UploadedData UploadWhite(UVulkanRenderDevice* renderer, const FTextureInfo& Info, bool masked);
};
