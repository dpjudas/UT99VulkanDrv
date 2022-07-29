#pragma once

#include "VulkanObjects.h"
#include "TextureUploader.h"

class UVulkanRenderDevice;
class CachedTexture;
struct FTextureInfo;

class UploadManager
{
public:
	UploadManager(UVulkanRenderDevice* renderer);
	~UploadManager();

	bool SupportsTextureFormat(ETextureFormat Format) const;

	void UploadTexture(CachedTexture* tex, const FTextureInfo& Info, bool masked);
	void UploadTextureRect(CachedTexture* tex, const FTextureInfo& Info, int x, int y, int w, int h);

private:
	struct UploadedData
	{
		VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
		int width = 1;
		int height = 1;
		std::unique_ptr<VulkanBuffer> stagingbuffer;
		std::vector<VkBufferImageCopy> miplevels;
	};

	UploadedData UploadData(const FTextureInfo& Info, bool masked, TextureUploader* uploader);
	UploadedData UploadWhite();

	UVulkanRenderDevice* renderer = nullptr;
};
