
#include "Precomp.h"
#include "FramebufferManager.h"
#include "UVulkanRenderDevice.h"

FramebufferManager::FramebufferManager(UVulkanRenderDevice* renderer) : renderer(renderer)
{
}

void FramebufferManager::CreateSceneFramebuffer()
{
	SceneFramebuffer = FramebufferBuilder()
		.RenderPass(renderer->RenderPasses->Scene.RenderPass.get())
		.Size(renderer->Textures->Scene->Width, renderer->Textures->Scene->Height)
		.AddAttachment(renderer->Textures->Scene->ColorBufferView.get())
		.AddAttachment(renderer->Textures->Scene->HitBufferView.get())
		.AddAttachment(renderer->Textures->Scene->DepthBufferView.get())
		.DebugName("SceneFramebuffer")
		.Create(renderer->Device.get());

	for (int level = 0; level < NumBloomLevels; level++)
	{
		BloomBlurLevels[level].VTextureFB = FramebufferBuilder()
			.RenderPass(renderer->RenderPasses->Bloom.RenderPass.get())
			.Size(renderer->Textures->Scene->BloomBlurLevels[level].Width, renderer->Textures->Scene->BloomBlurLevels[level].Height)
			.AddAttachment(renderer->Textures->Scene->BloomBlurLevels[level].VTextureView.get())
			.DebugName("VTextureFB")
			.Create(renderer->Device.get());

		BloomBlurLevels[level].HTextureFB = FramebufferBuilder()
			.RenderPass(renderer->RenderPasses->Bloom.RenderPass.get())
			.Size(renderer->Textures->Scene->BloomBlurLevels[level].Width, renderer->Textures->Scene->BloomBlurLevels[level].Height)
			.AddAttachment(renderer->Textures->Scene->BloomBlurLevels[level].HTextureView.get())
			.DebugName("HTextureFB")
			.Create(renderer->Device.get());
	}

	BloomPPImageFB = FramebufferBuilder()
		.RenderPass(renderer->RenderPasses->Bloom.RenderPass.get())
		.Size(renderer->Textures->Scene->Width, renderer->Textures->Scene->Height)
		.AddAttachment(renderer->Textures->Scene->PPImageView.get())
		.DebugName("BloomPPImageFB")
		.Create(renderer->Device.get());
}

void FramebufferManager::DestroySceneFramebuffer()
{
	SceneFramebuffer.reset();
	for (int level = 0; level < NumBloomLevels; level++)
	{
		BloomBlurLevels[level].VTextureFB.reset();
		BloomBlurLevels[level].HTextureFB.reset();
	}
	BloomPPImageFB.reset();
}

void FramebufferManager::CreateSwapChainFramebuffers()
{
	renderer->RenderPasses->CreatePresentRenderPass();
	renderer->RenderPasses->CreatePresentPipeline();

	auto swapchain = renderer->Commands->SwapChain.get();
	for (int i = 0; i < swapchain->ImageCount(); i++)
	{
		SwapChainFramebuffers.push_back(
			FramebufferBuilder()
				.RenderPass(renderer->RenderPasses->Present.RenderPass.get())
				.Size(renderer->Textures->Scene->Width, renderer->Textures->Scene->Height)
				.AddAttachment(swapchain->GetImageView(i))
				.DebugName("SwapChainFramebuffer")
				.Create(renderer->Device.get()));
	}
}

void FramebufferManager::DestroySwapChainFramebuffers()
{
	SwapChainFramebuffers.clear();
}

VulkanFramebuffer* FramebufferManager::GetSwapChainFramebuffer()
{
	return SwapChainFramebuffers[renderer->Commands->PresentImageIndex].get();
}
