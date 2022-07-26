
#include "Precomp.h"
#include "DescriptorSetManager.h"
#include "UVulkanRenderDevice.h"
#include "VulkanBuilders.h"
#include "VulkanTexture.h"

DescriptorSetManager::DescriptorSetManager(UVulkanRenderDevice* renderer) : renderer(renderer)
{
	CreateSceneDescriptorSetLayout();
	CreatePresentDescriptorSetLayout();
	CreatePresentDescriptorSet();
}

DescriptorSetManager::~DescriptorSetManager()
{
}

VulkanDescriptorSet* DescriptorSetManager::GetTextureDescriptorSet(DWORD PolyFlags, VulkanTexture* tex, VulkanTexture* lightmap, VulkanTexture* macrotex, VulkanTexture* detailtex, bool clamp)
{
	uint32_t samplermode = 0;
	if (PolyFlags & PF_NoSmooth) samplermode |= 1;
	if (clamp) samplermode |= 2;

	auto& descriptorSet = TextureDescriptorSets[{ tex, lightmap, detailtex, macrotex, samplermode }];
	if (!descriptorSet)
	{
		if (SceneDescriptorPoolSetsLeft == 0)
		{
			DescriptorPoolBuilder builder;
			builder.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 * 4);
			builder.setMaxSets(1000);
			SceneDescriptorPool.push_back(builder.create(renderer->Device));
			SceneDescriptorPoolSetsLeft = 1000;
		}

		descriptorSet = SceneDescriptorPool.back()->allocate(SceneDescriptorSetLayout.get());
		SceneDescriptorPoolSetsLeft--;

		WriteDescriptors writes;
		int i = 0;
		for (VulkanTexture* texture : { tex, lightmap, macrotex, detailtex })
		{
			VulkanSampler* sampler = (i == 0) ? renderer->Samplers->samplers[samplermode].get() : renderer->Samplers->samplers[0].get();

			if (texture)
				writes.addCombinedImageSampler(descriptorSet.get(), i++, texture->imageView.get(), sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			else
				writes.addCombinedImageSampler(descriptorSet.get(), i++, renderer->Textures->NullTextureView.get(), sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
		writes.updateSets(renderer->Device);
	}
	return descriptorSet.get();
}

void DescriptorSetManager::ClearCache()
{
	TextureDescriptorSets.clear();

	SceneDescriptorPool.clear();
	SceneDescriptorPoolSetsLeft = 0;
}

void DescriptorSetManager::CreateSceneDescriptorSetLayout()
{
	DescriptorSetLayoutBuilder builder;
	builder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	builder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	builder.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	builder.addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	SceneDescriptorSetLayout = builder.create(renderer->Device);
}

void DescriptorSetManager::CreatePresentDescriptorSetLayout()
{
	DescriptorSetLayoutBuilder builder;
	builder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	builder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	PresentDescriptorSetLayout = builder.create(renderer->Device);
}

void DescriptorSetManager::CreatePresentDescriptorSet()
{
	DescriptorPoolBuilder builder;
	builder.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2);
	builder.setMaxSets(1);
	PresentDescriptorPool = builder.create(renderer->Device);
	PresentDescriptorSet = PresentDescriptorPool->allocate(PresentDescriptorSetLayout.get());
}