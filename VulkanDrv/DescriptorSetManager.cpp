
#include "Precomp.h"
#include "DescriptorSetManager.h"
#include "UVulkanRenderDevice.h"
#include "CachedTexture.h"

DescriptorSetManager::DescriptorSetManager(UVulkanRenderDevice* renderer) : renderer(renderer)
{
	CreateBindlessTextureSet();
	CreateTextureLayout();
	CreatePresentLayout();
	CreatePresentSet();
	CreateBloomLayout();
	CreateBloomSets();
}

DescriptorSetManager::~DescriptorSetManager()
{
}

VulkanDescriptorSet* DescriptorSetManager::GetTextureSet(DWORD PolyFlags, CachedTexture* tex, CachedTexture* lightmap, CachedTexture* macrotex, CachedTexture* detailtex, bool clamp)
{
	uint32_t samplermode = 0;
	if (PolyFlags & PF_NoSmooth) samplermode |= 1;
	if (clamp) samplermode |= 2;

	auto& descriptorSet = Textures.Sets[{ tex, lightmap, detailtex, macrotex, samplermode }];
	if (!descriptorSet)
	{
		if (Textures.PoolSetsLeft == 0)
		{
			Textures.Pool.push_back(DescriptorPoolBuilder()
				.AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 * 4)
				.MaxSets(1000)
				.DebugName("TexturePool")
				.Create(renderer->Device.get()));
			Textures.PoolSetsLeft = 1000;
		}

		descriptorSet = Textures.Pool.back()->allocate(Textures.Layout.get());
		Textures.PoolSetsLeft--;

		WriteDescriptors writes;
		int i = 0;
		for (CachedTexture* texture : { tex, lightmap, macrotex, detailtex })
		{
			VulkanSampler* sampler = (i == 0) ? renderer->Samplers->Samplers[samplermode].get() : renderer->Samplers->Samplers[0].get();

			if (texture)
				writes.AddCombinedImageSampler(descriptorSet.get(), i++, texture->imageView.get(), sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			else
				writes.AddCombinedImageSampler(descriptorSet.get(), i++, renderer->Textures->NullTextureView.get(), sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
		writes.Execute(renderer->Device.get());
	}
	return descriptorSet.get();
}

void DescriptorSetManager::ClearCache()
{
	Textures.Sets.clear();

	Textures.Pool.clear();
	Textures.PoolSetsLeft = 0;

	Textures.WriteBindless = WriteDescriptors();
	Textures.NextBindlessIndex = 0;
}

int DescriptorSetManager::GetTextureArrayIndex(DWORD PolyFlags, CachedTexture* tex, bool clamp)
{
	if (Textures.NextBindlessIndex == 0)
	{
		Textures.WriteBindless.AddCombinedImageSampler(Textures.BindlessSet.get(), 0, 0, renderer->Textures->NullTextureView.get(), renderer->Samplers->Samplers[0].get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		Textures.NextBindlessIndex = 1;
	}

	if (!tex)
		return 0;

	uint32_t samplermode = 0;
	if (PolyFlags & PF_NoSmooth) samplermode |= 1;
	if (clamp) samplermode |= 2;

	int index = tex->BindlessIndex[samplermode];
	if (index != -1)
		return index;

	index = Textures.NextBindlessIndex++;

	VulkanSampler* sampler = renderer->Samplers->Samplers[samplermode].get();
	Textures.WriteBindless.AddCombinedImageSampler(Textures.BindlessSet.get(), 0, index, tex->imageView.get(), sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	tex->BindlessIndex[samplermode] = index;
	return index;
}

void DescriptorSetManager::UpdateBindlessSet()
{
	Textures.WriteBindless.Execute(renderer->Device.get());
	Textures.WriteBindless = WriteDescriptors();
}

void DescriptorSetManager::CreateBindlessTextureSet()
{
	if (!renderer->SupportsBindless)
		return;

	Textures.BindlessPool = DescriptorPoolBuilder()
		.Flags(VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT)
		.AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MaxBindlessTextures)
		.MaxSets(MaxBindlessTextures)
		.DebugName("TextureBindlessPool")
		.Create(renderer->Device.get());

	Textures.BindlessLayout = DescriptorSetLayoutBuilder()
		.Flags(VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT)
		.AddBinding(
			0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			MaxBindlessTextures,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT)
		.DebugName("TextureBindlessLayout")
		.Create(renderer->Device.get());

	Textures.BindlessSet = Textures.BindlessPool->allocate(Textures.BindlessLayout.get(), MaxBindlessTextures);
}

void DescriptorSetManager::CreateTextureLayout()
{
	Textures.Layout = DescriptorSetLayoutBuilder()
		.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.DebugName("TextureLayout")
		.Create(renderer->Device.get());
}

void DescriptorSetManager::CreatePresentLayout()
{
	Present.Layout = DescriptorSetLayoutBuilder()
		.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.DebugName("PresentLayout")
		.Create(renderer->Device.get());
}

void DescriptorSetManager::CreatePresentSet()
{
	Present.Pool = DescriptorPoolBuilder()
		.AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2)
		.MaxSets(1)
		.DebugName("PresentPool")
		.Create(renderer->Device.get());
	Present.Set = Present.Pool->allocate(Present.Layout.get());
}

void DescriptorSetManager::CreateBloomLayout()
{
	Bloom.Layout = DescriptorSetLayoutBuilder()
		.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
		.DebugName("BloomLayout")
		.Create(renderer->Device.get());
}

void DescriptorSetManager::CreateBloomSets()
{
	Bloom.Pool = DescriptorPoolBuilder()
		.AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (NumBloomLevels * 2 + 1) * 2)
		.MaxSets(NumBloomLevels * 2 + 1)
		.DebugName("BloomPool")
		.Create(renderer->Device.get());

	for (int level = 0; level < NumBloomLevels; level++)
	{
		Bloom.HTextureSets[level] = Bloom.Pool->allocate(Bloom.Layout.get());
		Bloom.VTextureSets[level] = Bloom.Pool->allocate(Bloom.Layout.get());
	}

	Bloom.PPImageSet = Bloom.Pool->allocate(Bloom.Layout.get());
}

void DescriptorSetManager::UpdateFrameDescriptors()
{
	auto textures = renderer->Textures.get();
	auto samplers = renderer->Samplers.get();

	WriteDescriptors write;
	write.AddCombinedImageSampler(Present.Set.get(), 0, textures->Scene->PPImageView[0].get(), samplers->PPLinearClamp.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	write.AddCombinedImageSampler(Present.Set.get(), 1, textures->DitherImageView.get(), samplers->PPNearestRepeat.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	for (int level = 0; level < NumBloomLevels; level++)
	{
		write.AddCombinedImageSampler(GetBloomHTextureSet(level), 0, textures->Scene->BloomBlurLevels[level].HTextureView.get(), samplers->PPLinearClamp.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		write.AddCombinedImageSampler(GetBloomVTextureSet(level), 0, textures->Scene->BloomBlurLevels[level].VTextureView.get(), samplers->PPLinearClamp.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}
	write.AddCombinedImageSampler(Bloom.PPImageSet.get(), 0, textures->Scene->PPImageView[0].get(), samplers->PPLinearClamp.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	write.Execute(renderer->Device.get());
}
