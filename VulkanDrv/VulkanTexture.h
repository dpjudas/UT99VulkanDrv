#pragma once

#include "VulkanObjects.h"
#include <functional>

class Renderer;
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
	VulkanTexture(Renderer* renderer, const FTextureInfo& Info, DWORD PolyFlags);
	~VulkanTexture();

	void Update(Renderer* renderer, const FTextureInfo& Info, DWORD PolyFlags);

	float UMult = 1.0f;
	float VMult = 1.0f;
	FPlane MaxColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	std::unique_ptr<VulkanImage> image;
	std::unique_ptr<VulkanImageView> imageView;

private:
	UploadedData UploadData(Renderer* renderer, const FTextureInfo& Info, DWORD PolyFlags, VkFormat imageFormat, std::function<int(FMipmapBase* mip)> calcMipSize, std::function<void(FMipmapBase* mip, void* dst)> copyMip = {});
	UploadedData UploadWhite(Renderer* renderer, const FTextureInfo& Info, DWORD PolyFlags);
};
