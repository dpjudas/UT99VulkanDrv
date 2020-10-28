
#include "Precomp.h"
#include "SceneSamplers.h"
#include "VulkanBuilders.h"

SceneSamplers::SceneSamplers(VulkanDevice *device)
{
	SamplerBuilder builder;

	builder.setAddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	clamp = builder.create(device);

	builder.setAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT);
	repeat = builder.create(device);

	builder.setAddressMode(VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT);
	mirror = builder.create(device);
}
