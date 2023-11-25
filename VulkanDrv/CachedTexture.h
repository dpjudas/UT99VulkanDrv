#pragma once

class CachedTexture
{
public:
	std::unique_ptr<VulkanImage> image;
	std::unique_ptr<VulkanImageView> imageView;

	int BindlessIndex[4] = { -1, -1, -1, -1 };
	int RealtimeChangeCount = 0;

	std::vector<VkBufferImageCopy> pendingUploads[2];
};
