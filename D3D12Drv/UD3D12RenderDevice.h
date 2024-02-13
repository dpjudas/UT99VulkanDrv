#pragma once

#include "vec.h"
#include "mat.h"

#include "CachedTexture.h"

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
	float Padding;
	vec4 GammaCorrection;
};

struct BloomPushConstants
{
	float SampleWeights[8];
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

#if defined(OLDUNREAL469SDK)
	// URenderDeviceOldUnreal469 extensions
	void DrawGouraudTriangles(const FSceneNode* Frame, const FTextureInfo& Info, FTransTexture* const Pts, INT NumPts, DWORD PolyFlags, DWORD DataFlags, FSpanBuffer* Span) override;
	UBOOL SupportsTextureFormat(ETextureFormat Format) override;
	void UpdateTextureRect(FTextureInfo& Info, INT U, INT V, INT UL, INT VL) override;
#endif

	int InterfacePadding[64]; // For allowing URenderDeviceOldUnreal469 interface to add things

	HWND WindowHandle = 0;
	ComPtr<ID3D12Debug> DebugController;
	ComPtr<ID3D12Device> Device;
	ComPtr<ID3D12CommandQueue> GraphicsQueue;
	ComPtr<ID3D12RootSignature> RootSignature;
	D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_11_0; // To do: find out how to discover this
	ComPtr<IDXGISwapChain1> SwapChain1;
	int BufferCount = 2;
	int BackBufferIndex = 0;
	ComPtr<ID3D12DescriptorHeap> FrameBufferHeap;
	std::vector<ComPtr<ID3D12Resource>> FrameBuffers;
	UINT RtvHandleSize = 0;
	UINT SamplerHandleSize = 0;

	struct PPBlurLevel
	{
		ComPtr<ID3D12Resource> VTexture;
		// ID3D12RenderTargetView* VTextureRTV = nullptr;
		// ID3D12ShaderResourceView* VTextureSRV = nullptr;
		ComPtr<ID3D12Resource> HTexture;
		// ID3D12RenderTargetView* HTextureRTV = nullptr;
		// ID3D12ShaderResourceView* HTextureSRV = nullptr;
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
		/*
		ID3D12RenderTargetView* ColorBufferView = nullptr;
		ID3D12RenderTargetView* HitBufferView = nullptr;
		ID3D12DepthStencilView* DepthBufferView = nullptr;
		ID3D12RenderTargetView* PPHitBufferView = nullptr;
		ID3D12RenderTargetView* PPImageView[2] = {};
		ID3D12ShaderResourceView* HitBufferShaderView = nullptr;
		ID3D12ShaderResourceView* PPImageShaderView[2] = {};
		*/
		enum { NumBloomLevels = 4 };
		PPBlurLevel BlurLevels[NumBloomLevels];
		int Width = 0;
		int Height = 0;
		int Multisample = 1;
	} SceneBuffers;

	struct
	{
		ComPtr<ID3D12Resource> VertexBuffer;
		ComPtr<ID3D12Resource> IndexBuffer;
		ComPtr<ID3D12Resource> ConstantBuffer;
		ComPtr<ID3D12DescriptorHeap> SamplersHeap;
		ComPtr<ID3D12PipelineState> Pipelines[32];
		ComPtr<ID3D12PipelineState> LinePipeline[2];
		ComPtr<ID3D12PipelineState> PointPipeline;
		FLOAT LODBias = 0.0f;
	} ScenePass;

	static const int SceneVertexBufferSize = 16 * 1024;
	static const int SceneIndexBufferSize = 32 * 1024;

	struct DrawBatchEntry
	{
		size_t SceneIndexStart = 0;
		size_t SceneIndexEnd = 0;
		ID3D12PipelineState* Pipeline = nullptr;
		CachedTexture* Tex = nullptr;
		CachedTexture* Lightmap = nullptr;
		CachedTexture* Detailtex = nullptr;
		CachedTexture* Macrotex = nullptr;
		uint32_t TexSamplerMode = 0;
		uint32_t DetailtexSamplerMode = 0;
		uint32_t MacrotexSamplerMode = 0;
	} Batch;
	std::vector<DrawBatchEntry> QueuedBatches;

	SceneVertex* SceneVertices = nullptr;
	size_t SceneVertexPos = 0;

	uint32_t* SceneIndexes = nullptr;
	size_t SceneIndexPos = 0;

	struct
	{
		ComPtr<ID3D12PipelineState> HitResolve;
		ComPtr<ID3D12PipelineState> Present[16];
		ComPtr<ID3D12Resource> PPStepVertexBuffer;
		ComPtr<ID3D12Resource> PresentConstantBuffer;
		ComPtr<ID3D12Resource> DitherTexture;
		// ID3D12ShaderResourceView* DitherTextureView = nullptr;
	} PresentPass;

	struct
	{
		ComPtr<ID3D12PipelineState> Extract;
		ComPtr<ID3D12PipelineState> Combine;
		ComPtr<ID3D12PipelineState> CombineAdditive;
		ComPtr<ID3D12PipelineState> BlurVertical;
		ComPtr<ID3D12PipelineState> BlurHorizontal;
		ComPtr<ID3D12Resource> ConstantBuffer;
	} BloomPass;

	//std::unique_ptr<TextureManager> Textures;
	//std::unique_ptr<UploadManager> Uploads;

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
	BITFIELD Bloom;
	BYTE BloomAmount;
	FLOAT LODBias;
	BYTE AntialiasMode;
	BYTE GammaMode;
	BYTE LightMode;
	INT RefreshRate;
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

private:
	void ResizeSceneBuffers(int width, int height, int multisample);
	void ClearTextureCache();

	void CreatePresentPass();
	void CreateBloomPass();
	void CreateScenePass();
	void ReleaseScenePass();
	void ReleaseBloomPass();
	void ReleasePresentPass();

	void CreateSceneSamplers();
	void ReleaseSceneSamplers();
	void UpdateLODBias();

	void ReleaseSceneBuffers();

	void RunBloomPass();
	// void BlurStep(ID3D12ShaderResourceView* input, ID3D12RenderTargetView* output, bool vertical);
	float ComputeBlurGaussian(float n, float theta);
	void ComputeBlurSamples(int sampleCount, float blurAmount, float* sampleWeights);

	void SetPipeline(DWORD polyflags);
	void SetDescriptorSet(DWORD polyflags, CachedTexture* tex = nullptr, CachedTexture* lightmap = nullptr, CachedTexture* macrotex = nullptr, CachedTexture* detailtex = nullptr, bool clamp = false);

	void AddDrawBatch();
	void NextSceneBuffers() { DrawBatches(true); }
	void DrawBatches(bool nextBuffer = false);
	void DrawEntry(const DrawBatchEntry& entry);

	int GetSettingsMultisample();

	ID3D12PipelineState* GetPipeline(DWORD PolyFlags);

	std::vector<uint8_t> CompileHlsl(const std::string& filename, const std::string& shadertype, const std::vector<std::string> defines = {});

	vec4 ApplyInverseGamma(vec4 color);

	PresentPushConstants GetPresentPushConstants();

	UBOOL UsePrecache;
	FPlane FlashScale;
	FPlane FlashFog;
	FSceneNode* CurrentFrame = nullptr;
	float Aspect;
	float RProjZ;
	float RFX2;
	float RFY2;
	ScenePushConstants SceneConstants = {};

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
};

inline void ThrowIfFailed(HRESULT result, const char* msg) { if (FAILED(result)) throw std::runtime_error(msg); }

inline float GetUMult(const FTextureInfo& Info) { return 1.0f / (Info.UScale * Info.USize); }
inline float GetVMult(const FTextureInfo& Info) { return 1.0f / (Info.VScale * Info.VSize); }

inline void UD3D12RenderDevice::SetPipeline(DWORD PolyFlags)
{
	auto pipeline = GetPipeline(PolyFlags);
	if (pipeline != Batch.Pipeline)
	{
		AddDrawBatch();
		Batch.Pipeline = pipeline;
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
