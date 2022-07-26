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

	VulkanImage* NullTexture = nullptr;
	VulkanImageView* NullTextureView = nullptr;
	std::unique_ptr<SceneTextures> Scene;

private:
	void CreateNullTexture();

	UVulkanRenderDevice* renderer = nullptr;
	std::map<QWORD, VulkanTexture*> TextureCache[2];
};
