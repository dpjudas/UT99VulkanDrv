#pragma once

#include "vec.h"
#include "mat.h"
#include "Descriptors.h"
#include "TextureManager.h"
#include "UploadManager.h"
#include "CachedTexture.h"
#include "D3D12MemAlloc/D3D12MemAlloc.h"
#include <functional>

struct SceneVertex
{
	uint32_t Flags;
	vec3 Position;
	vec2 TexCoord;
	vec2 TexCoord2;
	vec2 TexCoord3;
	vec2 TexCoord4;
	vec4 Color;
};

struct ScenePushConstants
{
	mat4 ObjectToProjection;
	vec4 NearClip;
	int HitIndex;
	int Padding1, Padding2, Padding3;
};

struct PresentPushConstants
{
	float Contrast;
	float Saturation;
	float Brightness;
	float HdrScale;
	vec4 GammaCorrection;
};

struct BloomPushConstants
{
	float SampleWeights[8];
};

struct TexDescriptorKey
{
	TexDescriptorKey(CachedTexture* tex, CachedTexture* lightmap, CachedTexture* detailtex, CachedTexture* macrotex) : tex(tex), lightmap(lightmap), detailtex(detailtex), macrotex(macrotex) { }

	bool operator==(const TexDescriptorKey& other) const
	{
		return tex == other.tex && lightmap == other.lightmap && detailtex == other.detailtex && macrotex == other.macrotex;
	}

	bool operator<(const TexDescriptorKey& other) const
	{
		if (tex != other.tex)
			return tex < other.tex;
		else if (lightmap != other.lightmap)
			return lightmap < other.lightmap;
		else if (detailtex != other.detailtex)
			return detailtex < other.detailtex;
		return macrotex < other.macrotex;
	}

	CachedTexture* tex;
	CachedTexture* lightmap;
	CachedTexture* detailtex;
	CachedTexture* macrotex;
};

template<> struct std::hash<TexDescriptorKey>
{
	std::size_t operator()(const TexDescriptorKey& k) const
	{
		return (((std::size_t)k.tex ^ (std::size_t)k.lightmap));
	}
};

#if defined(OLDUNREAL469SDK)
class UD3D12RenderDevice : public URenderDeviceOldUnreal469
{
public:
	DECLARE_CLASS(UD3D12RenderDevice, URenderDeviceOldUnreal469, CLASS_Config, D3D12Drv)
#else
class UD3D12RenderDevice : public URenderDevice
{
public:
	DECLARE_CLASS(UD3D12RenderDevice, URenderDevice, CLASS_Config)
#endif

	UD3D12RenderDevice();
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

	void SetHitLocation();

#if defined(OLDUNREAL469SDK)
	// URenderDeviceOldUnreal469 extensions
	void DrawGouraudTriangles(const FSceneNode* Frame, const FTextureInfo& Info, FTransTexture* const Pts, INT NumPts, DWORD PolyFlags, DWORD DataFlags, FSpanBuffer* Span) override;
	UBOOL SupportsTextureFormat(ETextureFormat Format) override;
	void UpdateTextureRect(FTextureInfo& Info, INT U, INT V, INT UL, INT VL) override;
#endif

	int GetWantedSwapChainBufferCount() { return 2;/* UseVSync ? 2 : 3;*/ }

	int InterfacePadding[64]; // For allowing URenderDeviceOldUnreal469 interface to add things

	HWND WindowHandle = 0;
	ComPtr<ID3D12Debug> DebugController;
	ComPtr<ID3D12Device> Device;
	ComPtr<ID3D12InfoQueue1> InfoQueue1;
	ComPtr<ID3D12CommandQueue> GraphicsQueue;
	D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_11_0; // To do: find out how to discover this
	ComPtr<IDXGISwapChain3> SwapChain3;
	int BackBufferIndex = 0;
	bool DxgiSwapChainAllowTearing = false;
	int BufferCount = 2;
	std::vector<UINT64> FrameFenceValues;
	std::vector<ComPtr<ID3D12Resource>> FrameBuffers;
	DescriptorSet FrameBufferRTVs;

	D3D12MA::Allocator* MemAllocator = nullptr;

	struct
	{
		std::unique_ptr<DescriptorHeap> Common;
		std::unique_ptr<DescriptorHeap> Sampler;
		std::unique_ptr<DescriptorHeap> RTV;
		std::unique_ptr<DescriptorHeap> DSV;
	} Heaps;

	struct
	{
		std::unordered_map<TexDescriptorKey, DescriptorSet> Tex;
		std::unordered_map<uint32_t, DescriptorSet> Sampler;
	} Descriptors;

	struct CommandBatch
	{
		ComPtr<ID3D12CommandAllocator> TransferAllocator;
		ComPtr<ID3D12CommandAllocator> DrawAllocator;
		ComPtr<ID3D12GraphicsCommandList> Transfer;
		ComPtr<ID3D12GraphicsCommandList> Draw;
		UINT64 FenceValue = 0;
	};

	struct
	{
		UINT64 FenceValue = 0;
		ComPtr<ID3D12Fence> Fence;
		HANDLE FenceHandle = INVALID_HANDLE_VALUE;
		CommandBatch Batches[2];
		CommandBatch* Current = nullptr;
		D3D_PRIMITIVE_TOPOLOGY PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	} Commands;

	struct PPBlurLevel
	{
		ComPtr<ID3D12Resource> VTexture;
		DescriptorSet VTextureRTV;
		DescriptorSet VTextureSRV;
		ComPtr<ID3D12Resource> HTexture;
		DescriptorSet HTextureRTV;
		DescriptorSet HTextureSRV;
		int Width = 0;
		int Height = 0;
	};

	struct
	{
		ComPtr<ID3D12Resource> ColorBuffer;
		ComPtr<ID3D12Resource> HitBuffer;
		ComPtr<ID3D12Resource> DepthBuffer;
		ComPtr<ID3D12Resource> PPImage[2];
		ComPtr<ID3D12Resource> PPHitBuffer;
		ComPtr<ID3D12Resource> StagingHitBuffer;

		DescriptorSet SceneRTVs; // ColorBuffer, HitBuffer
		DescriptorSet SceneDSV;  // DepthBuffer
		DescriptorSet PPHitBufferRTV;
		DescriptorSet PPImageRTV[2];

		DescriptorSet HitBufferSRV;
		DescriptorSet PPImageSRV[2];

		DescriptorSet PresentSRVs;

		enum { NumBloomLevels = 4 };
		PPBlurLevel BlurLevels[NumBloomLevels];
		int Width = 0;
		int Height = 0;
		int Multisample = 1;
	} SceneBuffers;

	struct ScenePipelineState
	{
		ComPtr<ID3D12PipelineState> Pipeline;
		float MinDepth = 0.1f;
		float MaxDepth = 1.0f;
	};

	struct
	{
		ComPtr<ID3D12Resource> VertexBuffer;
		ComPtr<ID3D12Resource> IndexBuffer;
		D3D12_VERTEX_BUFFER_VIEW VertexBufferView = {};
		D3D12_INDEX_BUFFER_VIEW IndexBufferView = {};
		D3D12_SAMPLER_DESC Samplers[16] = {};
		ComPtr<ID3D12RootSignature> RootSignature;
		ScenePipelineState Pipelines[32];
		ScenePipelineState LinePipeline[2];
		ScenePipelineState PointPipeline[2];
		FLOAT LODBias = 0.0f;
		int Multisample = 1;
		SceneVertex* VertexData = nullptr;
		size_t VertexBase = 0;
		size_t VertexPos = 0;
		uint32_t* IndexData = nullptr;
		size_t IndexBase = 0;
		size_t IndexPos = 0;
		static const int VertexBufferSize = 512 * 1024;
		static const int IndexBufferSize = 1024 * 1024;
	} ScenePass;

	struct DrawBatchEntry
	{
		size_t SceneIndexStart = 0;
		size_t SceneIndexEnd = 0;
		D3D_PRIMITIVE_TOPOLOGY PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		ScenePipelineState* Pipeline = nullptr;
		CachedTexture* Tex = nullptr;
		CachedTexture* Lightmap = nullptr;
		CachedTexture* Detailtex = nullptr;
		CachedTexture* Macrotex = nullptr;
		uint32_t TexSamplerMode = 0;
		uint32_t DetailtexSamplerMode = 0;
		uint32_t MacrotexSamplerMode = 0;
	} Batch;
	std::vector<DrawBatchEntry> QueuedBatches;

	struct
	{
		ComPtr<ID3D12Resource> Buffer;
		uint8_t* Data = nullptr;
		UINT64 Base = 0;
		UINT64 Pos = 0;
		const UINT64 Size = 32 * 1024 * 1024;
		struct
		{
			std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> Footprints;
			std::vector<UINT> NumRows;
			std::vector<UINT64> RowSizeInBytes;
		} Transfer;
	} Upload;

	struct
	{
		ComPtr<ID3D12RootSignature> RootSignature;
		ComPtr<ID3D12PipelineState> HitResolve;
		ComPtr<ID3D12PipelineState> Present[32];
		ComPtr<ID3D12Resource> PPStepVertexBuffer;
		ComPtr<ID3D12Resource> DitherTexture;
		D3D12_VERTEX_BUFFER_VIEW PPStepVertexBufferView = {};
	} PresentPass;

	struct
	{
		ComPtr<ID3D12RootSignature> RootSignature;
		ComPtr<ID3D12PipelineState> Extract;
		ComPtr<ID3D12PipelineState> Combine;
		ComPtr<ID3D12PipelineState> CombineAdditive;
		ComPtr<ID3D12PipelineState> BlurVertical;
		ComPtr<ID3D12PipelineState> BlurHorizontal;
	} BloomPass;

	std::unique_ptr<TextureManager> Textures;
	std::unique_ptr<UploadManager> Uploads;

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
	BITFIELD UseDebugLayer;

	struct
	{
		int ComplexSurfaces = 0;
		int GouraudPolygons = 0;
		int Tiles = 0;
		int DrawCalls = 0;
		int Uploads = 0;
		int RectUploads = 0;
		int BuffersUsed = 0;
	} Stats;

	void UploadTexture(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter, int x, int y, int width, int height, int firstSubresource, int numSubresources, const std::function<void(uint8_t* dest, int subresource, const D3D12_SUBRESOURCE_FOOTPRINT& footprint)>& onWriteSubresource);

private:
	void ReleaseSwapChainResources();
	bool UpdateSwapChain();
	void SetColorSpace();
	void ResizeSceneBuffers(int width, int height, int multisample);
	void ClearTextureCache();

	void CreateUploadBuffer();
	void ReleaseUploadBuffer();

	void CreatePresentPass();
	void CreateBloomPass();
	void CreateScenePass();
	void ReleaseScenePass();
	void ReleaseBloomPass();
	void ReleasePresentPass();

	void CreateSceneSamplers();
	void ReleaseSceneSamplers();
	void UpdateScenePass();

	void ReleaseSceneBuffers();

	void RunBloomPass();
	void BlurStep(const DescriptorSet& input, const DescriptorSet& output, ID3D12Resource* outputResource, bool vertical);
	float ComputeBlurGaussian(float n, float theta);
	void ComputeBlurSamples(int sampleCount, float blurAmount, float* sampleWeights);

	void SetPipeline(DWORD polyflags);
	void SetPipeline(ScenePipelineState* pipeline, D3D_PRIMITIVE_TOPOLOGY PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	void SetDescriptorSet(DWORD polyflags, CachedTexture* tex = nullptr, CachedTexture* lightmap = nullptr, CachedTexture* macrotex = nullptr, CachedTexture* detailtex = nullptr, bool clamp = false);

	void AddDrawBatch();
	void DrawBatches(bool nextBuffer = false);
	void DrawEntry(const DrawBatchEntry& entry);

	struct VertexReserveInfo
	{
		SceneVertex* vptr;
		uint32_t* iptr;
		uint32_t vpos;
	};

	VertexReserveInfo ReserveVertices(size_t vcount, size_t icount)
	{
		// If buffers are full, flush and wait for room.
		if (ScenePass.VertexPos + vcount > (size_t)ScenePass.VertexBufferSize || ScenePass.IndexPos + icount > (size_t)ScenePass.IndexBufferSize)
		{
			// If the request is larger than our buffers we can't draw this.
			if (vcount > (size_t)ScenePass.VertexBufferSize || icount > (size_t)ScenePass.IndexBufferSize)
				return { nullptr, nullptr, 0 };

			DrawBatches(true);
		}

		return { ScenePass.VertexData + ScenePass.VertexBase + ScenePass.VertexPos, ScenePass.IndexData + ScenePass.IndexBase + ScenePass.IndexPos, (uint32_t)(ScenePass.VertexBase + ScenePass.VertexPos) };
	}

	void UseVertices(size_t vcount, size_t icount)
	{
		ScenePass.VertexPos += vcount;
		ScenePass.IndexPos += icount;
	}

	int GetSettingsMultisample();

	ScenePipelineState* GetPipeline(DWORD PolyFlags);

	ComPtr<ID3D12RootSignature> CreateRootSignature(const char* name, const std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>>& descriptorTables = {}, const D3D12_ROOT_CONSTANTS& pushConstants = {}, const std::vector<D3D12_STATIC_SAMPLER_DESC>& staticSamplers = {});
	std::vector<uint8_t> CompileHlsl(const std::string& filename, const std::string& shadertype, const std::vector<std::string> defines = {});

	vec4 ApplyInverseGamma(vec4 color);

	PresentPushConstants GetPresentPushConstants();

	void WaitDeviceIdle();
	void WaitForFence(UINT64 value);
	void SubmitCommands(bool present);
	void TransitionResourceBarrier(ID3D12GraphicsCommandList* cmdlist, ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
	void TransitionResourceBarrier(ID3D12GraphicsCommandList* cmdlist, ID3D12Resource* resource0, D3D12_RESOURCE_STATES before0, D3D12_RESOURCE_STATES after0, ID3D12Resource* resource1, D3D12_RESOURCE_STATES before1, D3D12_RESOURCE_STATES after1);

	static void CALLBACK OnDebugMessage(D3D12_MESSAGE_CATEGORY category, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID id, LPCSTR description, void* context);

	bool DebugMessageActive = false;
	DWORD DebugMessageCookie = 0;

	UBOOL UsePrecache;
	FPlane FlashScale;
	FPlane FlashFog;
	FSceneNode* CurrentFrame = nullptr;
	float Aspect;
	float RProjZ;
	float RFX2;
	float RFY2;
	ScenePushConstants SceneConstants = {};
	D3D12_VIEWPORT SceneViewport = {};

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

	bool IsLocked = false;
	bool ActiveHdr = false;

	struct
	{
		int Width = 0;
		int Height = 0;
	} DesktopResolution;

	struct
	{
		RECT WindowPos = {};
		LONG Style = 0;
		LONG ExStyle = 0;
		bool Enabled = false;
	} FullscreenState;

	bool InSetResCall = false;
	int CurrentSizeX = 0;
	int CurrentSizeY = 0;
};

void ThrowError(HRESULT result, const char* msg);
inline void ThrowIfFailed(HRESULT result, const char* msg) { if (FAILED(result)) ThrowError(result, msg); }

inline float GetUMult(const FTextureInfo& Info) { return 1.0f / (Info.UScale * Info.USize); }
inline float GetVMult(const FTextureInfo& Info) { return 1.0f / (Info.VScale * Info.VSize); }

inline void UD3D12RenderDevice::SetPipeline(DWORD PolyFlags)
{
	SetPipeline(GetPipeline(PolyFlags), D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

inline void UD3D12RenderDevice::SetPipeline(ScenePipelineState* pipeline, D3D_PRIMITIVE_TOPOLOGY PrimitiveTopology)
{
	if (pipeline != Batch.Pipeline || PrimitiveTopology != Batch.PrimitiveTopology)
	{
		AddDrawBatch();
		Batch.Pipeline = pipeline;
		Batch.PrimitiveTopology = PrimitiveTopology;
	}
}

inline void UD3D12RenderDevice::SetDescriptorSet(DWORD PolyFlags, CachedTexture* tex, CachedTexture* lightmap, CachedTexture* macrotex, CachedTexture* detailtex, bool clamp)
{
	uint32_t samplermode = 0;
	if (PolyFlags & PF_NoSmooth) samplermode |= 1;
	if (clamp) samplermode |= 2;
	if (tex) samplermode |= (tex->DummyMipmapCount << 2);
	int detailsamplermode = detailtex ? (detailtex->DummyMipmapCount << 2) : 0;
	int macrosamplermode = macrotex ? (macrotex->DummyMipmapCount << 2) : 0;

	if (Batch.Tex != tex || Batch.TexSamplerMode != samplermode || Batch.Lightmap != lightmap || Batch.Detailtex != detailtex || Batch.DetailtexSamplerMode != detailsamplermode || Batch.Macrotex != macrotex || Batch.MacrotexSamplerMode != macrosamplermode)
	{
		AddDrawBatch();
		Batch.Tex = tex;
		Batch.Lightmap = lightmap;
		Batch.Detailtex = detailtex;
		Batch.Macrotex = macrotex;
		Batch.TexSamplerMode = samplermode;
		Batch.DetailtexSamplerMode = detailsamplermode;
		Batch.MacrotexSamplerMode = macrosamplermode;
	}
}

inline DWORD ApplyPrecedenceRules(DWORD PolyFlags)
{
	// Adjust PolyFlags according to Unreal's precedence rules.
	if (!(PolyFlags & (PF_Translucent | PF_Modulated)))
		PolyFlags |= PF_Occlude;
	else if (PolyFlags & PF_Translucent)
		PolyFlags &= ~PF_Masked;
	return PolyFlags;
}
