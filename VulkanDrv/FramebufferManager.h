#pragma once

#include "VulkanObjects.h"

class UVulkanRenderDevice;

class FramebufferManager
{
public:
	FramebufferManager(UVulkanRenderDevice* renderer);

	void createSceneFramebuffer();
	void destroySceneFramebuffer();

	std::unique_ptr<VulkanFramebuffer> sceneFramebuffer;

private:
	UVulkanRenderDevice* renderer = nullptr;
};
