#pragma once

#include "VulkanObjects.h"

class UVulkanRenderDevice;

class RenderPassManager
{
public:
	RenderPassManager(UVulkanRenderDevice* renderer);
	~RenderPassManager();

	VulkanPipelineLayout* ScenePipelineLayout = nullptr;

	void createRenderPass();
	void createPipelines();

	void begin(VulkanCommandBuffer* cmdbuffer);
	void end(VulkanCommandBuffer* cmdbuffer);

	VulkanPipeline* getPipeline(DWORD polyflags);
	VulkanPipeline* getEndFlashPipeline();

	std::unique_ptr<VulkanRenderPass> renderPass;

private:
	void CreateScenePipelineLayout();

	UVulkanRenderDevice* renderer = nullptr;
	std::unique_ptr<VulkanPipeline> pipeline[32];
};
