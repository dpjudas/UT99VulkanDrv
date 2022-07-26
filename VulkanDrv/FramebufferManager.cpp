
#include "Precomp.h"
#include "FramebufferManager.h"
#include "UVulkanRenderDevice.h"
#include "VulkanBuilders.h"

FramebufferManager::FramebufferManager(UVulkanRenderDevice* renderer) : renderer(renderer)
{
}

void FramebufferManager::createSceneFramebuffer()
{
	FramebufferBuilder builder;
	builder.setRenderPass(renderer->RenderPasses->renderPass.get());
	builder.setSize(renderer->Textures->Scene->width, renderer->Textures->Scene->height);
	builder.addAttachment(renderer->Textures->Scene->colorBufferView.get());
	builder.addAttachment(renderer->Textures->Scene->depthBufferView.get());
	sceneFramebuffer = builder.create(renderer->Device);
}

void FramebufferManager::destroySceneFramebuffer()
{
	sceneFramebuffer.reset();
}
