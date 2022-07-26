
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
	FramebufferBuilder builder;
	builder.setRenderPass(renderer->RenderPasses->SceneRenderPass.get());
	builder.setSize(renderer->Textures->Scene->width, renderer->Textures->Scene->height);
	builder.addAttachment(renderer->Textures->Scene->colorBufferView.get());
	builder.addAttachment(renderer->Textures->Scene->depthBufferView.get());
	sceneFramebuffer = builder.create(renderer->Device);
}

void FramebufferManager::DestroySceneFramebuffer()
{
	sceneFramebuffer.reset();
}

VulkanFramebuffer* FramebufferManager::GetSwapChainFramebuffer()
{
	swapChainFramebuffer.reset();

	FramebufferBuilder builder;
	builder.setRenderPass(renderer->RenderPasses->PresentRenderPass.get());
	builder.setSize(renderer->Textures->Scene->width, renderer->Textures->Scene->height);
	builder.addAttachment(renderer->Commands->SwapChain->swapChainImageViews[renderer->Commands->PresentImageIndex]);
	swapChainFramebuffer = builder.create(renderer->Device);
	return swapChainFramebuffer.get();
}
