
#include "Precomp.h"
#include "FramebufferManager.h"
#include "UVulkanRenderDevice.h"

FramebufferManager::FramebufferManager(UVulkanRenderDevice* renderer) : renderer(renderer)
{
}

void FramebufferManager::CreateSceneFramebuffer()
{
	sceneFramebuffer = FramebufferBuilder()
		.RenderPass(renderer->RenderPasses->SceneRenderPass.get())
		.Size(renderer->Textures->Scene->width, renderer->Textures->Scene->height)
		.AddAttachment(renderer->Textures->Scene->ColorBufferView.get())
		.AddAttachment(renderer->Textures->Scene->HitBufferView.get())
		.AddAttachment(renderer->Textures->Scene->DepthBufferView.get())
		.DebugName("SceneFramebuffer")
		.Create(renderer->Device.get());
}

void FramebufferManager::DestroySceneFramebuffer()
{
	sceneFramebuffer.reset();
}

void FramebufferManager::CreateSwapChainFramebuffers()
{
	renderer->RenderPasses->CreatePresentRenderPass();
	renderer->RenderPasses->CreatePresentPipeline();

	auto swapchain = renderer->Commands->SwapChain.get();
	for (int i = 0; i < swapchain->ImageCount(); i++)
	{
		swapChainFramebuffers.push_back(
			FramebufferBuilder()
				.RenderPass(renderer->RenderPasses->PresentRenderPass.get())
				.Size(renderer->Textures->Scene->width, renderer->Textures->Scene->height)
				.AddAttachment(swapchain->GetImageView(i))
				.DebugName("SwapChainFramebuffer")
				.Create(renderer->Device.get()));
	}
}

void FramebufferManager::DestroySwapChainFramebuffers()
{
	swapChainFramebuffers.clear();
}

VulkanFramebuffer* FramebufferManager::GetSwapChainFramebuffer()
{
	return swapChainFramebuffers[renderer->Commands->PresentImageIndex].get();
}
