
#include "Precomp.h"
#include "SamplerManager.h"
#include "UVulkanRenderDevice.h"
#include "VulkanBuilders.h"

SamplerManager::SamplerManager(UVulkanRenderDevice* renderer) : renderer(renderer)
{
	for (int i = 0; i < 4; i++)
	{
		SamplerBuilder builder;
		builder.setAnisotropy(8.0);
		builder.setMipLodBias(-0.5);

		if (i & 1)
		{
			builder.setMinFilter(VK_FILTER_NEAREST);
			builder.setMagFilter(VK_FILTER_NEAREST);
			builder.setMipmapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST);
		}
		else
		{
			builder.setMinFilter(VK_FILTER_LINEAR);
			builder.setMagFilter(VK_FILTER_LINEAR);
			builder.setMipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR);
		}

		if (i & 2)
		{
			builder.setAddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
		}
		else
		{
			builder.setAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT);
		}

		samplers[i] = builder.create(renderer->Device);
	}

	// To do: detail texture needs a zbias of 15

	SamplerBuilder builder;
	builder.setMinFilter(VK_FILTER_NEAREST);
	builder.setMagFilter(VK_FILTER_NEAREST);
	builder.setMipmapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST);
	builder.setAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT);
	ppNearestRepeat = builder.create(renderer->Device);

	builder.setMinFilter(VK_FILTER_LINEAR);
	builder.setMagFilter(VK_FILTER_LINEAR);
	builder.setMipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR);
	builder.setAddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	ppLinearClamp = builder.create(renderer->Device);
}
