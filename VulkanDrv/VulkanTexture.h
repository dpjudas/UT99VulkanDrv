#pragma once

#include "VulkanObjects.h"

class UVulkanViewport;
struct FTextureInfo;

class VulkanTexture
{
public:
	VulkanTexture(UVulkanViewport* renderer, const FTextureInfo& Info, DWORD PolyFlags);
	~VulkanTexture();

	void Update(UVulkanViewport* renderer, const FTextureInfo& Info, DWORD PolyFlags);

	float UMult = 1.0f;
	float VMult = 1.0f;

	std::unique_ptr<VulkanImage> image;
	std::unique_ptr<VulkanImageView> imageView;
};
