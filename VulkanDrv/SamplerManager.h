#pragma once

#include "VulkanObjects.h"

class UVulkanRenderDevice;

class SamplerManager
{
public:
	SamplerManager(UVulkanRenderDevice* renderer);

	std::unique_ptr<VulkanSampler> Samplers[4];

	std::unique_ptr<VulkanSampler> PPNearestRepeat;
	std::unique_ptr<VulkanSampler> PPLinearClamp;

private:
	UVulkanRenderDevice* renderer = nullptr;
};
