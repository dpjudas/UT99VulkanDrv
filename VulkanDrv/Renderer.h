#pragma once

#include "VulkanObjects.h"
#include "vec.h"
#include "mat.h"

class SceneBuffers;
class SceneRenderPass;
class SceneSamplers;
class Postprocess;
class VulkanPostprocess;
class VulkanTexture;
struct SceneVertex;

class Renderer
{
public:
	Renderer(HWND windowHandle, bool vsync, int vk_device, bool vk_debug, std::function<void(const char* typestr, const std::string& msg)> printLogCallback);
	~Renderer();

	void SubmitCommands(bool present, int presentWidth, int presentHeight);
	VulkanCommandBuffer* GetTransferCommands();
	VulkanCommandBuffer* GetDrawCommands();
	void DeleteFrameObjects();

	void CopyScreenToBuffer(int w, int h, void* data, float gamma);

	static std::unique_ptr<VulkanShader> CreateVertexShader(VulkanDevice* device, const std::string& name, const std::string& defines = {});
	static std::unique_ptr<VulkanShader> CreateFragmentShader(VulkanDevice* device, const std::string& name, const std::string& defines = {});
	static std::unique_ptr<VulkanShader> CreateComputeShader(VulkanDevice* device, const std::string& name, const std::string& defines = {});
	static std::string LoadShaderCode(const std::string& filename, const std::string& defines);

	void CreateScenePipelineLayout();
	void CreateSceneDescriptorSetLayout();
	void CreateSceneVertexBuffer();
	void CreateNullTexture();

	void UpdateTextureRect(FTextureInfo* texture, int x, int y, int w, int h);
	VulkanTexture* GetTexture(FTextureInfo* texture, bool masked);
	VulkanDescriptorSet* GetTextureDescriptorSet(DWORD PolyFlags, VulkanTexture* tex, VulkanTexture* lightmap = nullptr, VulkanTexture* macrotex = nullptr, VulkanTexture* detailtex = nullptr, bool clamp = false);
	void ClearTextureCache();

	HWND WindowHandle = 0;

	VulkanDevice* Device = nullptr;
	VulkanSwapChain* SwapChain = nullptr;
	VulkanSemaphore* ImageAvailableSemaphore = nullptr;
	VulkanSemaphore* RenderFinishedSemaphore = nullptr;
	VulkanSemaphore* TransferSemaphore = nullptr;
	VulkanFence* RenderFinishedFence = nullptr;
	VulkanCommandPool* CommandPool = nullptr;
	VulkanCommandBuffer* DrawCommands = nullptr;
	VulkanCommandBuffer* TransferCommands = nullptr;
	uint32_t PresentImageIndex = 0xffffffff;

	struct DeleteList
	{
		std::vector<std::unique_ptr<VulkanImage>> images;
		std::vector<std::unique_ptr<VulkanImageView>> imageViews;
		std::vector<std::unique_ptr<VulkanBuffer>> buffers;
		std::vector<std::unique_ptr<VulkanDescriptorSet>> descriptors;
	};
	DeleteList* FrameDeleteList = nullptr;

	VulkanDescriptorSetLayout* SceneDescriptorSetLayout = nullptr;
	VulkanPipelineLayout* ScenePipelineLayout = nullptr;
	std::vector<VulkanDescriptorPool*> SceneDescriptorPool;
	int SceneDescriptorPoolSetsLeft = 0;

	SceneSamplers* SceneSamplers = nullptr;
	VulkanImage* NullTexture = nullptr;
	VulkanImageView* NullTextureView = nullptr;

	Postprocess* PostprocessModel = nullptr;
	VulkanPostprocess* Postprocess = nullptr;
	SceneBuffers* SceneBuffers = nullptr;
	SceneRenderPass* SceneRenderPass = nullptr;

	VulkanBuffer* SceneVertexBuffer = nullptr;
	SceneVertex* SceneVertices = nullptr;
	size_t SceneVertexPos = 0;

	struct TexDescriptorKey
	{
		TexDescriptorKey(VulkanTexture* tex, VulkanTexture* lightmap, VulkanTexture* detailtex, VulkanTexture* macrotex, uint32_t sampler) : tex(tex), lightmap(lightmap), detailtex(detailtex), macrotex(macrotex), sampler(sampler) { }

		bool operator<(const TexDescriptorKey& other) const
		{
			if (tex != other.tex)
				return tex < other.tex;
			else if (lightmap != other.lightmap)
				return lightmap < other.lightmap;
			else if (detailtex != other.detailtex)
				return detailtex < other.detailtex;
			else if (macrotex != other.macrotex)
				return macrotex < other.macrotex;
			else
				return sampler < other.sampler;
		}

		VulkanTexture* tex;
		VulkanTexture* lightmap;
		VulkanTexture* detailtex;
		VulkanTexture* macrotex;
		uint32_t sampler;
	};

	std::map<QWORD, VulkanTexture*> TextureCache[2];
	std::map<TexDescriptorKey, VulkanDescriptorSet*> TextureDescriptorSets;
};
