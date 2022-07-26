#pragma once

#include "VulkanObjects.h"

class UVulkanRenderDevice;

class SamplerManager
{
public:
	SamplerManager(UVulkanRenderDevice* renderer);

	std::unique_ptr<VulkanSampler> samplers[4];

	std::unique_ptr<VulkanSampler> ppNearestRepeat;
	std::unique_ptr<VulkanSampler> ppLinearClamp;

private:
	UVulkanRenderDevice* renderer = nullptr;
};
