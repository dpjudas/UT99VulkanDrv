#pragma once

#include "VulkanObjects.h"
#include "SceneTextures.h"

struct FTextureInfo;
class UVulkanRenderDevice;
class VulkanTexture;

class TextureManager
{
public:
	TextureManager(UVulkanRenderDevice* renderer);
	~TextureManager();

	void UpdateTextureRect(FTextureInfo* texture, int x, int y, int w, int h);
	VulkanTexture* GetTexture(FTextureInfo* texture, bool masked);

	void ClearCache();

	std::unique_ptr<VulkanImage> NullTexture;
	std::unique_ptr<VulkanImageView> NullTextureView;

	std::unique_ptr<VulkanImage> DitherImage;
	std::unique_ptr<VulkanImageView> DitherImageView;

	std::unique_ptr<SceneTextures> Scene;

private:
	void CreateNullTexture();
	void CreateDitherTexture();

	UVulkanRenderDevice* renderer = nullptr;
	std::map<QWORD, VulkanTexture*> TextureCache[2];
};
