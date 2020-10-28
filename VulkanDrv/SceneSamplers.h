#pragma once

#include "VulkanObjects.h"

class SceneSamplers
{
public:
	SceneSamplers(VulkanDevice *device);

	std::unique_ptr<VulkanSampler> clamp;
	std::unique_ptr<VulkanSampler> repeat;
	std::unique_ptr<VulkanSampler> mirror;
};
