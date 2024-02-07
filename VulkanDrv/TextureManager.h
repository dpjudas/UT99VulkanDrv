#pragma once

#include "SceneTextures.h"

struct FTextureInfo;
class UVulkanRenderDevice;
class CachedTexture;

class TextureManager
{
public:
	TextureManager(UVulkanRenderDevice* renderer);
	~TextureManager();

	void UpdateTextureRect(FTextureInfo* info, int x, int y, int w, int h);
	CachedTexture* GetTexture(FTextureInfo* info, bool masked);

	void ClearCache();
	void ClearAllBindlessIndexes();

	std::unique_ptr<VulkanImage> NullTexture;
	std::unique_ptr<VulkanImageView> NullTextureView;

	std::unique_ptr<VulkanImage> DitherImage;
	std::unique_ptr<VulkanImageView> DitherImageView;

	std::unique_ptr<SceneTextures> Scene;

	int GetTexturesInCache() { return TextureCache[0].size() + TextureCache[1].size(); }

private:
	void CreateNullTexture();
	void CreateDitherTexture();

	UVulkanRenderDevice* renderer = nullptr;
	std::unordered_map<QWORD, std::unique_ptr<CachedTexture>> TextureCache[2];
};
