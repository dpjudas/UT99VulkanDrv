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
	UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar) override;
	void Lock(FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData, INT* HitSize) override;
	void Unlock(UBOOL Blit) override;
	void DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet) override;
	void DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, int NumPts, DWORD PolyFlags, FSpanBuffer* Span) override;
	void DrawTile(FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags) override;
	void Draw3DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector OrigP, FVector OrigQ) override;
	void Draw2DClippedLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2) override;
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

	void SetHitLocation();

#if defined(OLDUNREAL469SDK)
	// URenderDeviceOldUnreal469 extensions
	void DrawGouraudTriangles(const FSceneNode* Frame, const FTextureInfo& Info, FTransTexture* const Pts, INT NumPts, DWORD PolyFlags, DWORD DataFlags, FSpanBuffer* Span) override;
	UBOOL SupportsTextureFormat(ETextureFormat Format) override;
	void UpdateTextureRect(FTextureInfo& Info, INT U, INT V, INT UL, INT VL) override;
#endif

	int InterfacePadding[64]; // For allowing URenderDeviceOldUnreal469 interface to add things

#ifdef WIN32
	HWND WindowHandle = 0;
#endif

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
	BYTE HdrScale;
#if !defined(OLDUNREAL469SDK)
	BITFIELD OccludeLines;
#endif
	BITFIELD Bloom;
	BYTE BloomAmount;
	FLOAT LODBias;
	BYTE AntialiasMode;
	BYTE GammaMode;
	BYTE LightMode;
	BITFIELD GammaCorrectScreenshots;

	INT VkDeviceIndex;
	BITFIELD VkDebug;
	BITFIELD VkExclusiveFullscreen;

	void RunBloomPass();
	void BloomStep(VulkanCommandBuffer* cmdbuffer, VulkanPipeline* pipeline, VulkanDescriptorSet* input, VulkanFramebuffer* output, int width, int height, const BloomPushConstants &pushconstants);
	static float ComputeBlurGaussian(float n, float theta);
	static void ComputeBlurSamples(int sampleCount, float blurAmount, float* sampleWeights);

	void DrawPresentTexture(int width, int height);
	PresentPushConstants GetPresentPushConstants();

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

	struct VertexReserveInfo
	{
		SceneVertex* vptr;
		uint32_t* iptr;
		uint32_t vpos;
	};

	VertexReserveInfo ReserveVertices(size_t vcount, size_t icount)
	{
		// If buffers are full, flush and wait for room.
		if (SceneVertexPos + vcount > (size_t)BufferManager::SceneVertexBufferSize || SceneIndexPos + icount > (size_t)BufferManager::SceneIndexBufferSize)
		{
			// If the request is larger than our buffers we can't draw this.
			if (vcount > (size_t)BufferManager::SceneVertexBufferSize || icount > (size_t)BufferManager::SceneIndexBufferSize)
				return { nullptr, nullptr, 0 };

			FlushDrawBatchAndWait();
		}

		return { Buffers->SceneVertices + SceneVertexPos, Buffers->SceneIndexes + SceneIndexPos, (uint32_t)SceneVertexPos };
	}

	void FlushDrawBatchAndWait();

	void UseVertices(size_t vcount, size_t icount)
	{
		SceneVertexPos += vcount;
		SceneIndexPos += icount;
	}

	VkViewport viewportdesc = {};

	UBOOL UsePrecache;
	FPlane FlashScale;
	FPlane FlashFog;
	FSceneNode* CurrentFrame = nullptr;
	float Aspect;
	float RProjZ;
	float RFX2;
	float RFY2;

	bool IsLocked = false;

	void SetPipeline(PipelineState* pipeline);
	ivec4 GetTextureIndexes(DWORD PolyFlags, CachedTexture* tex, bool clamp = false);
	ivec4 GetTextureIndexes(DWORD PolyFlags, CachedTexture* tex, CachedTexture* lightmap, CachedTexture* macrotex, CachedTexture* detailtex);
	void DrawBatch(VulkanCommandBuffer* cmdbuffer);
	void SubmitAndWait(bool present, int presentWidth, int presentHeight, bool presentFullscreen);

	vec4 ApplyInverseGamma(vec4 color);

	struct
	{
		size_t SceneIndexStart = 0;
		PipelineState* Pipeline = nullptr;
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

#ifdef WIN32
	struct
	{
		RECT WindowPos = {};
		LONG Style = 0;
		LONG ExStyle = 0;
		bool Enabled = false;
	} FullscreenState;

	bool InSetResCall = false;
#endif
};

inline void UVulkanRenderDevice::SetPipeline(PipelineState* pipeline)
{
	if (pipeline != Batch.Pipeline)
	{
		DrawBatch(Commands->GetDrawCommands());
		Batch.Pipeline = pipeline;
	}
}

inline ivec4 UVulkanRenderDevice::GetTextureIndexes(DWORD PolyFlags, CachedTexture* tex, bool clamp)
{
	return ivec4(DescriptorSets->GetTextureArrayIndex(PolyFlags, tex, clamp), 0, 0, 0);
}

inline ivec4 UVulkanRenderDevice::GetTextureIndexes(DWORD PolyFlags, CachedTexture* tex, CachedTexture* lightmap, CachedTexture* macrotex, CachedTexture* detailtex)
{
	if (DescriptorSets->IsTextureArrayFull())
	{
		FlushDrawBatchAndWait();
		DescriptorSets->ClearCache();
		Textures->ClearAllBindlessIndexes();
	}

	ivec4 textureBinds;
	textureBinds.x = DescriptorSets->GetTextureArrayIndex(PolyFlags, tex);
	textureBinds.y = DescriptorSets->GetTextureArrayIndex(0, macrotex);
	textureBinds.z = DescriptorSets->GetTextureArrayIndex(0, detailtex);
	textureBinds.w = DescriptorSets->GetTextureArrayIndex(0, lightmap);
	return textureBinds;
}

inline float GetUMult(const FTextureInfo& Info) { return 1.0f / (Info.UScale * Info.USize); }
inline float GetVMult(const FTextureInfo& Info) { return 1.0f / (Info.VScale * Info.VSize); }

inline DWORD ApplyPrecedenceRules(DWORD PolyFlags)
{
	// Adjust PolyFlags according to Unreal's precedence rules.
	if (!(PolyFlags & (PF_Translucent | PF_Modulated)))
		PolyFlags |= PF_Occlude;
	else if (PolyFlags & PF_Translucent)
		PolyFlags &= ~PF_Masked;
	return PolyFlags;
}
