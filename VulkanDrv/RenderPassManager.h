#pragma once

class UVulkanRenderDevice;

class RenderPassManager
{
public:
	RenderPassManager(UVulkanRenderDevice* renderer);
	~RenderPassManager();

	void CreateRenderPass();
	void CreatePipelines();

	void CreatePresentRenderPass();
	void CreatePresentPipeline();
	void CreateScreenshotPipeline();

	void CreatePostprocessRenderPass();
	void CreateBloomPipeline();

	void BeginScene(VulkanCommandBuffer* cmdbuffer, float r, float g, float b, float a);
	void ContinueScene(VulkanCommandBuffer* cmdbuffer);
	void EndScene(VulkanCommandBuffer* cmdbuffer);

	void BeginPresent(VulkanCommandBuffer* cmdbuffer);
	void EndPresent(VulkanCommandBuffer* cmdbuffer);

	VulkanPipeline* GetPipeline(DWORD polyflags);
	VulkanPipeline* GetEndFlashPipeline();
	VulkanPipeline* GetLinePipeline(bool occludeLines) { return Scene.LinePipeline[occludeLines].get(); }
	VulkanPipeline* GetPointPipeline(bool occludeLines) { return Scene.PointPipeline[occludeLines].get(); }

	struct
	{
		std::unique_ptr<VulkanPipelineLayout> BindlessPipelineLayout;
		std::unique_ptr<VulkanRenderPass> RenderPass;
		std::unique_ptr<VulkanRenderPass> RenderPassContinue;
		std::unique_ptr<VulkanPipeline> Pipeline[32];
		std::unique_ptr<VulkanPipeline> LinePipeline[2];
		std::unique_ptr<VulkanPipeline> PointPipeline[2];
	} Scene;

	struct
	{
		std::unique_ptr<VulkanPipelineLayout> PipelineLayout;
		std::unique_ptr<VulkanRenderPass> RenderPass;
		std::unique_ptr<VulkanPipeline> Pipeline[16];
		std::unique_ptr<VulkanPipeline> ScreenshotPipeline[16];
	} Present;

	struct
	{
		std::unique_ptr<VulkanPipelineLayout> PipelineLayout;
		std::unique_ptr<VulkanPipeline> Extract;
		std::unique_ptr<VulkanPipeline> Combine;
		std::unique_ptr<VulkanPipeline> Scale;
		std::unique_ptr<VulkanPipeline> BlurVertical;
		std::unique_ptr<VulkanPipeline> BlurHorizontal;
	} Bloom;

	struct
	{
		std::unique_ptr<VulkanRenderPass> RenderPass;
		std::unique_ptr<VulkanRenderPass> RenderPassCombine;
	} Postprocess;

private:
	void CreateSceneBindlessPipelineLayout();
	void CreatePresentPipelineLayout();
	void CreateBloomPipelineLayout();

	UVulkanRenderDevice* renderer = nullptr;
};
