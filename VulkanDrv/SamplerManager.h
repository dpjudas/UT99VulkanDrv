#pragma once

class UVulkanRenderDevice;

class SamplerManager
{
public:
	SamplerManager(UVulkanRenderDevice* renderer);

	void CreateSceneSamplers();

	std::unique_ptr<VulkanSampler> Samplers[4];

	std::unique_ptr<VulkanSampler> PPNearestRepeat;
	std::unique_ptr<VulkanSampler> PPLinearClamp;

	FLOAT LODBias = 0.0f;

private:
	UVulkanRenderDevice* renderer = nullptr;
};
