
#include "Precomp.h"
#include "FramebufferManager.h"
#include "UVulkanRenderDevice.h"
#include "VulkanBuilders.h"
#include "VulkanSwapChain.h"

FramebufferManager::FramebufferManager(UVulkanRenderDevice* renderer) : renderer(renderer)
{
}

void FramebufferManager::CreateSceneFramebuffer()
{
	sceneFramebuffer = FramebufferBuilder()
		.RenderPass(renderer->RenderPasses->SceneRenderPass.get())
		.Size(renderer->Textures->Scene->width, renderer->Textures->Scene->height)
		.AddAttachment(renderer->Textures->Scene->colorBufferView.get())
		.AddAttachment(renderer->Textures->Scene->depthBufferView.get())
		.DebugName("SceneFramebuffer")
		.Create(renderer->Device);
}

void FramebufferManager::DestroySceneFramebuffer()
{
	sceneFramebuffer.reset();
}

VulkanFramebuffer* FramebufferManager::GetSwapChainFramebuffer()
{
	swapChainFramebuffer.reset();

	swapChainFramebuffer = FramebufferBuilder()
		.RenderPass(renderer->RenderPasses->PresentRenderPass.get())
		.Size(renderer->Textures->Scene->width, renderer->Textures->Scene->height)
		.AddAttachment(renderer->Commands->SwapChain->swapChainImageViews[renderer->Commands->PresentImageIndex])
		.DebugName("SwapChainFramebuffer")
		.Create(renderer->Device);
	return swapChainFramebuffer.get();
}
