#pragma once

#include "UVulkanClient.h"
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

enum EVulkanViewportStatus
{
	Vulkan_ViewportOpening = 0, // Viewport is opening.
	Vulkan_ViewportNormal = 1, // Viewport is operating normally.
	Vulkan_ViewportClosing = 2, // Viewport is closing and CloseViewport has been called.
};

struct ReceivedWindowMessage
{
	UINT msg;
	WPARAM wparam;
	LPARAM lparam;
};

class UVulkanViewport : public UViewport
{
public:
	DECLARE_CLASS(UVulkanViewport, UViewport, CLASS_Transient, VulkanDrv)
	DECLARE_WITHIN(UVulkanClient)

	// Constructor.
	UVulkanViewport();

	// UObject interface.
	void Destroy() override;
	void ShutdownAfterError() override;

	// UViewport interface.
	UBOOL Lock(FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData = NULL, INT* HitSize = 0) override;
	UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar) override;
	UBOOL ResizeViewport(DWORD BlitFlags, INT NewX = INDEX_NONE, INT NewY = INDEX_NONE, INT NewColorBytes = INDEX_NONE) override;
	UBOOL IsFullscreen() override;
	void Unlock(UBOOL Blit) override;
	void Repaint(UBOOL Blit) override;
	void SetModeCursor() override;
	void UpdateWindowFrame() override;
	void OpenWindow(DWORD ParentWindow, UBOOL Temporary, INT NewX, INT NewY, INT OpenX, INT OpenY) override;
	void CloseWindow() override;
	void UpdateInput(UBOOL Reset) override;
	void* GetWindow() override;
	void* GetServer() override;
	void SetMouseCapture(UBOOL Capture, UBOOL Clip, UBOOL FocusOnly) override;

	// UVulkanViewport interface.
	void ToggleFullscreen();
	void EndFullscreen();
	UBOOL CauseInputEvent(INT iKey, EInputAction Action, FLOAT Delta = 0.0);
	void TryRenderDevice(const TCHAR* ClassName, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen);
	void Tick();
	void PauseGame();
	void ResumeGame();

	LRESULT OnWindowMessage(UINT msg, WPARAM wparam, LPARAM lparam);
	static LRESULT CALLBACK WndProc(HWND windowhandle, UINT msg, WPARAM wparam, LPARAM lparam);

	void FreeResources();

	void SubmitCommands(bool present);
	VulkanCommandBuffer* GetTransferCommands();
	VulkanCommandBuffer* GetDrawCommands();
	void DeleteFrameObjects();

	static std::unique_ptr<VulkanShader> CreateVertexShader(VulkanDevice* device, const std::string& name, const std::string& defines = {});
	static std::unique_ptr<VulkanShader> CreateFragmentShader(VulkanDevice* device, const std::string& name, const std::string& defines = {});
	static std::unique_ptr<VulkanShader> CreateComputeShader(VulkanDevice* device, const std::string& name, const std::string& defines = {});
	static std::string LoadShaderCode(const std::string& filename, const std::string& defines);

	void CreateScenePipelineLayout();
	void CreateSceneDescriptorSetLayout();
	void CreateSceneVertexBuffer();
	void CreateNullTexture();

	VulkanTexture* GetTexture(FTextureInfo* texture, DWORD polyFlags);
	VulkanDescriptorSet* GetTextureDescriptorSet(VulkanTexture* tex, VulkanTexture* lightmap = nullptr);
	void ClearTextureCache();

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
		TexDescriptorKey(VulkanTexture* tex, VulkanTexture* lightmap) : tex(tex), lightmap(lightmap) { }
		bool operator<(const TexDescriptorKey& other) const { return tex != other.tex ? tex < other.tex : lightmap < other.lightmap; }

		VulkanTexture* tex;
		VulkanTexture* lightmap;
	};

	std::map<QWORD, VulkanTexture*> TextureCache;
	std::map<TexDescriptorKey, VulkanDescriptorSet*> TextureDescriptorSets;

	std::vector<ReceivedWindowMessage> ReceivedMessages;

	// Variables.
	HWND WindowHandle = 0;
	EVulkanViewportStatus ViewportStatus;
	INT HoldCount;
	DWORD BlitFlags;
	bool Paused = false;

	// Mouse.
	int MouseMoveX = 0;
	int MouseMoveY = 0;
};
