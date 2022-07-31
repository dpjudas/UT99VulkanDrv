#pragma once

#include "VulkanObjects.h"
#include "CommandBufferManager.h"
#include "BufferManager.h"
#include "DescriptorSetManager.h"
#include "FramebufferManager.h"
#include "RenderPassManager.h"
#include "SamplerManager.h"
#include "ShaderManager.h"
#include "TextureManager.h"
#include "UploadManager.h"
#include "vec.h"
#include "mat.h"

class CachedTexture;

class UVulkanRenderDevice : public URenderDeviceOldUnreal469
{
public:
	DECLARE_CLASS(UVulkanRenderDevice, URenderDeviceOldUnreal469, CLASS_Config, VulkanDrv)

	UVulkanRenderDevice();
	void StaticConstructor();

	UBOOL Init(UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen) override;
	UBOOL SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen) override;
	void Exit() override;
	void Flush(UBOOL AllowPrecache) override;
	UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar);
	void Lock(FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData, INT* HitSize) override;
	void Unlock(UBOOL Blit) override;
	void DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet) override;
	void DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, int NumPts, DWORD PolyFlags, FSpanBuffer* Span) override;
	void DrawTile(FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags) override;
	void Draw3DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector OrigP, FVector OrigQ);
	void Draw2DClippedLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2);
	void Draw2DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2) override;
	void Draw2DPoint(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z) override;
	void ClearZ(FSceneNode* Frame) override;
	void PushHit(const BYTE* Data, INT Count) override;
	void PopHit(INT Count, UBOOL bForce) override;
	void GetStats(TCHAR* Result) override;
	void ReadPixels(FColor* Pixels) override;
	void EndFlash() override;
	void SetSceneNode(FSceneNode* Frame) override;
	void PrecacheTexture(FTextureInfo& Info, DWORD PolyFlags) override;

	// URenderDeviceOldUnreal469 extensions
	// void DrawGouraudTriangles(const FSceneNode* Frame, const FTextureInfo& Info, FTransTexture* const Pts, INT NumPts, DWORD PolyFlags, DWORD DataFlags, FSpanBuffer* Span) override;
	UBOOL SupportsTextureFormat(ETextureFormat Format) override;
	void UpdateTextureRect(FTextureInfo& Info, INT U, INT V, INT UL, INT VL) override;

	int InterfacePadding[64]; // For allowing URenderDeviceOldUnreal469 interface to add things

	HWND WindowHandle = 0;
	VulkanDevice* Device = nullptr;

	std::unique_ptr<CommandBufferManager> Commands;

	std::unique_ptr<SamplerManager> Samplers;
	std::unique_ptr<TextureManager> Textures;
	std::unique_ptr<BufferManager> Buffers;
	std::unique_ptr<ShaderManager> Shaders;
	std::unique_ptr<UploadManager> Uploads;

	std::unique_ptr<DescriptorSetManager> DescriptorSets;
	std::unique_ptr<RenderPassManager> RenderPasses;
	std::unique_ptr<FramebufferManager> Framebuffers;

	bool SupportsBindless = false;
	bool UsesBindless = false;

	// Configuration.
	BITFIELD UseVSync;
	INT VkDeviceIndex;
	BITFIELD VkDebug;
	INT Multisample;

	void DrawPresentTexture(int x, int y, int width, int height);

	struct
	{
		int ComplexSurfaces = 0;
		int GouraudPolygons = 0;
		int Tiles = 0;
		int DrawCalls = 0;
		int Uploads = 0;
		int RectUploads = 0;
	} Stats;


private:
	void ClearTextureCache();
	void BlitSceneToPostprocess();

	UBOOL UsePrecache;
	FPlane FlashScale;
	FPlane FlashFog;
	FSceneNode* CurrentFrame = nullptr;
	float Aspect;
	float RProjZ;
	float RFX2;
	float RFY2;

	bool IsLocked = false;
	bool StatMemory = false;
	bool StatResources = false;
	bool StatDraw = false;

	void SetPipeline(VulkanPipeline* pipeline);
	void SetDescriptorSet(VulkanDescriptorSet* descriptorSet, bool bindless);
	void DrawBatch(VulkanCommandBuffer* cmdbuffer);
	void SubmitAndWait(bool present, int presentWidth, int presentHeight);

	struct
	{
		size_t SceneIndexStart = 0;
		VulkanPipeline* Pipeline = nullptr;
		VulkanDescriptorSet* DescriptorSet = nullptr;
		bool Bindless = false;
	} Batch;

	ScenePushConstants pushconstants;

	size_t SceneVertexPos = 0;
	size_t SceneIndexPos = 0;
};

inline void UVulkanRenderDevice::SetPipeline(VulkanPipeline* pipeline)
{
	if (pipeline != Batch.Pipeline)
	{
		DrawBatch(Commands->GetDrawCommands());
		Batch.Pipeline = pipeline;
	}
}

inline void UVulkanRenderDevice::SetDescriptorSet(VulkanDescriptorSet* descriptorSet, bool bindless)
{
	if (descriptorSet != Batch.DescriptorSet)
	{
		DrawBatch(Commands->GetDrawCommands());
		Batch.DescriptorSet = descriptorSet;
		Batch.Bindless = bindless;
	}
}

inline float GetUMult(const FTextureInfo& Info) { return 1.0f / (Info.UScale * Info.USize); }
inline float GetVMult(const FTextureInfo& Info) { return 1.0f / (Info.VScale * Info.VSize); }
