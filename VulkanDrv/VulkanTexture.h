#pragma once

#include "VulkanObjects.h"

class Renderer;
struct FTextureInfo;

class VulkanTexture
{
public:
	VulkanTexture(Renderer* renderer, const FTextureInfo& Info, DWORD PolyFlags);
	~VulkanTexture();

	void Update(Renderer* renderer, const FTextureInfo& Info, DWORD PolyFlags);

	float UMult = 1.0f;
	float VMult = 1.0f;

	std::unique_ptr<VulkanImage> image;
	std::unique_ptr<VulkanImageView> imageView;
};
