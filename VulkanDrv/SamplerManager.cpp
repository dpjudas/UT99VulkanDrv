
#include "Precomp.h"
#include "SamplerManager.h"
#include "UVulkanRenderDevice.h"
#include "VulkanBuilders.h"

SamplerManager::SamplerManager(UVulkanRenderDevice* renderer) : renderer(renderer)
{
	for (int i = 0; i < 4; i++)
	{
		SamplerBuilder builder;
		builder.Anisotropy(8.0f);
		builder.MipLodBias(-0.5f);

		if (i & 1)
		{
			builder.MinFilter(VK_FILTER_NEAREST);
			builder.MagFilter(VK_FILTER_NEAREST);
			builder.MipmapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST);
		}
		else
		{
			builder.MinFilter(VK_FILTER_LINEAR);
			builder.MagFilter(VK_FILTER_LINEAR);
			builder.MipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR);
		}

		if (i & 2)
		{
			builder.AddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
		}
		else
		{
			builder.AddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT);
		}

		builder.DebugName("SceneSampler");

		samplers[i] = builder.Create(renderer->Device);
	}

	// To do: detail texture needs a zbias of 15

	ppNearestRepeat = SamplerBuilder()
		.MinFilter(VK_FILTER_NEAREST)
		.MagFilter(VK_FILTER_NEAREST)
		.MipmapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST)
		.AddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
		.DebugName("ppNearestRepeat")
		.Create(renderer->Device);

	ppLinearClamp = SamplerBuilder()
		.MinFilter(VK_FILTER_LINEAR)
		.MagFilter(VK_FILTER_LINEAR)
		.MipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
		.AddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
		.DebugName("ppLinearClamp")
		.Create(renderer->Device);
}
