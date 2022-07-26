#pragma once

#include "VulkanObjects.h"

class UVulkanRenderDevice;

class SamplerManager
{
public:
	SamplerManager(UVulkanRenderDevice* renderer);

	std::unique_ptr<VulkanSampler> samplers[4];

private:
	UVulkanRenderDevice* renderer = nullptr;
};
