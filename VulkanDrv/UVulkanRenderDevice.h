#pragma once

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

#if defined(OLDUNREAL469SDK)
class UVulkanRenderDevice : public URenderDeviceOldUnreal469
{
public:
	DECLARE_CLASS(UVulkanRenderDevice, URenderDeviceOldUnreal469, CLASS_Config, VulkanDrv)
#else
class UVulkanRenderDevice : public URenderDevice
{
public:
	DECLARE_CLASS(UVulkanRenderDevice, URenderDevice, CLASS_Config)
#endif

	UVulkanRenderDevice();
	void StaticConstructor();

	UBOOL Init(UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen) override;
	UBOOL SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen) override;
	void Exit() override;
#if defined(UNREALGOLD)
	void Flush() override;
#else
	void Flush(UBOOL AllowPrecache) override;
#endif
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
	void DrawStats(FSceneNode* Frame) override;

#if defined(OLDUNREAL469SDK)
	// URenderDeviceOldUnreal469 extensions
	void DrawGouraudTriangles(const FSceneNode* Frame, const FTextureInfo& Info, FTransTexture* const Pts, INT NumPts, DWORD PolyFlags, DWORD DataFlags, FSpanBuffer* Span) override;
	UBOOL SupportsTextureFormat(ETextureFormat Format) override;
	void UpdateTextureRect(FTextureInfo& Info, INT U, INT V, INT UL, INT VL) override;
#endif

	int InterfacePadding[64]; // For allowing URenderDeviceOldUnreal469 interface to add things

	HWND WindowHandle = 0;
	std::shared_ptr<VulkanDevice> Device;

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
	FLOAT GammaOffset;
	FLOAT GammaOffsetRed;
	FLOAT GammaOffsetGreen;
	FLOAT GammaOffsetBlue;
	BYTE LinearBrightness;
	BYTE Contrast;
	BYTE Saturation;
	INT GrayFormula;
	BITFIELD Hdr;
	BITFIELD OccludeLines;
	//BITFIELD Bloom;
	//BYTE BloomAmount;
	FLOAT LODBias;
	BYTE AntialiasMode;
	BYTE GammaMode;
	BYTE LightMode;

	INT VkDeviceIndex;
	BITFIELD VkDebug;
	BITFIELD VkExclusiveFullscreen;

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

	int GetSettingsMultisample()
	{
		switch (AntialiasMode)
		{
		default:
		case 0: return 0;
		case 1: return 2;
		case 2: return 4;
		}
	}

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

	void SetPipeline(VulkanPipeline* pipeline);
	ivec4 SetDescriptorSet(DWORD PolyFlags, CachedTexture* tex, bool clamp = false);
	ivec4 SetDescriptorSet(DWORD PolyFlags, CachedTexture* tex, CachedTexture* lightmap, CachedTexture* macrotex, CachedTexture* detailtex);
	void SetDescriptorSet(VulkanDescriptorSet* descriptorSet, bool bindless);
	void DrawBatch(VulkanCommandBuffer* cmdbuffer);
	void SubmitAndWait(bool present, int presentWidth, int presentHeight, bool presentFullscreen);

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

	struct HitQuery
	{
		INT Start = 0;
		INT Count = 0;
	};

	BYTE* HitData = nullptr;
	INT* HitSize = nullptr;
	std::vector<BYTE> HitQueryStack;
	std::vector<HitQuery> HitQueries;
	std::vector<BYTE> HitBuffer;

	int ForceHitIndex = -1;
	HitQuery ForceHit;
};

inline void UVulkanRenderDevice::SetPipeline(VulkanPipeline* pipeline)
{
	if (pipeline != Batch.Pipeline)
	{
		DrawBatch(Commands->GetDrawCommands());
		Batch.Pipeline = pipeline;
	}
}

inline ivec4 UVulkanRenderDevice::SetDescriptorSet(DWORD PolyFlags, CachedTexture* tex, bool clamp)
{
	if (UsesBindless)
	{
		SetDescriptorSet(DescriptorSets->GetBindlessDescriptorSet(), true);
		return ivec4(DescriptorSets->GetTextureArrayIndex(PolyFlags, tex, clamp), 0, 0, 0);
	}
	else
	{
		SetDescriptorSet(DescriptorSets->GetTextureDescriptorSet(PolyFlags, tex, nullptr, nullptr, nullptr, clamp), false);
		return ivec4(0);
	}
}

inline ivec4 UVulkanRenderDevice::SetDescriptorSet(DWORD PolyFlags, CachedTexture* tex, CachedTexture* lightmap, CachedTexture* macrotex, CachedTexture* detailtex)
{
	ivec4 textureBinds;
	if (UsesBindless)
	{
		textureBinds.x = DescriptorSets->GetTextureArrayIndex(PolyFlags, tex);
		textureBinds.y = DescriptorSets->GetTextureArrayIndex(0, macrotex);
		textureBinds.z = DescriptorSets->GetTextureArrayIndex(0, detailtex);
		textureBinds.w = DescriptorSets->GetTextureArrayIndex(0, lightmap);

		SetDescriptorSet(DescriptorSets->GetBindlessDescriptorSet(), true);
	}
	else
	{
		textureBinds.x = 0.0f;
		textureBinds.y = 0.0f;
		textureBinds.z = 0.0f;
		textureBinds.w = 0.0f;

		SetDescriptorSet(DescriptorSets->GetTextureDescriptorSet(PolyFlags, tex, lightmap, macrotex, detailtex), false);
	}
	return textureBinds;
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
