
#include "Precomp.h"
#include "SceneSamplers.h"
#include "VulkanBuilders.h"

SceneSamplers::SceneSamplers(VulkanDevice *device)
{
	SamplerBuilder builder;

	builder.setAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT);
	builder.setMinFilter(VK_FILTER_LINEAR);
	builder.setMagFilter(VK_FILTER_LINEAR);
	builder.setMipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR);
	builder.setAnisotropy(8.0);
	repeat = builder.create(device);

	builder.setAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT);
	builder.setMinFilter(VK_FILTER_NEAREST);
	builder.setMagFilter(VK_FILTER_NEAREST);
	builder.setMipmapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST);
	builder.setAnisotropy(8.0);
	nosmooth = builder.create(device);
}
