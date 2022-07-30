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

	void SubmitUploads();

private:
	void UploadData(VkImage image, const FTextureInfo& Info, bool masked, TextureUploader* uploader);
	void UploadWhite(VkImage image);
	void WaitIfUploadBufferIsFull(int bytes);

	UVulkanRenderDevice* renderer = nullptr;

	struct UploadedTexture
	{
		VkImage Image = {};
		int Index = 0;
		int Count = 0;
		bool PartialUpdate = false;
	};

	int UploadBufferPos = 0;
	std::vector<UploadedTexture> Uploads;
	std::vector<VkBufferImageCopy> ImageCopies;
	std::unordered_map<VkImage, std::vector<VkBufferImageCopy>> RectUploads;
};
