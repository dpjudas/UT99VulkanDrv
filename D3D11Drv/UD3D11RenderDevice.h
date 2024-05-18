#pragma once

#include "vec.h"
#include "mat.h"

#include "TextureManager.h"
#include "UploadManager.h"
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
class UD3D11RenderDevice : public URenderDeviceOldUnreal469
{
public:
	DECLARE_CLASS(UD3D11RenderDevice, URenderDeviceOldUnreal469, CLASS_Config, D3D11Drv)
#else
class UD3D11RenderDevice : public URenderDevice
{
public:
	DECLARE_CLASS(UD3D11RenderDevice, URenderDevice, CLASS_Config)
#endif

	UD3D11RenderDevice();
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
	ID3D11Device* Device = nullptr;
	D3D_FEATURE_LEVEL FeatureLevel = {};
	ID3D11DeviceContext* Context = nullptr;
	IDXGISwapChain* SwapChain = nullptr;
	IDXGISwapChain1* SwapChain1 = nullptr;
	ID3D11Texture2D* BackBuffer = nullptr;
	ID3D11RenderTargetView* BackBufferView = nullptr;

	struct PPBlurLevel
	{
		ID3D11Texture2D* VTexture = nullptr;
		ID3D11RenderTargetView* VTextureRTV = nullptr;
		ID3D11ShaderResourceView* VTextureSRV = nullptr;
		ID3D11Texture2D* HTexture = nullptr;
		ID3D11RenderTargetView* HTextureRTV = nullptr;
		ID3D11ShaderResourceView* HTextureSRV = nullptr;
		int Width = 0;
		int Height = 0;
	};

	struct
	{
		ID3D11Texture2D* ColorBuffer = nullptr;
		ID3D11Texture2D* HitBuffer = nullptr;
		ID3D11Texture2D* DepthBuffer = nullptr;
		ID3D11Texture2D* PPImage[2] = {};
		ID3D11Texture2D* PPHitBuffer = nullptr;
		ID3D11Texture2D* StagingHitBuffer = nullptr;
		ID3D11RenderTargetView* ColorBufferView = nullptr;
		ID3D11RenderTargetView* HitBufferView = nullptr;
		ID3D11DepthStencilView* DepthBufferView = nullptr;
		ID3D11RenderTargetView* PPHitBufferView = nullptr;
		ID3D11RenderTargetView* PPImageView[2] = {};
		ID3D11ShaderResourceView* HitBufferShaderView = nullptr;
		ID3D11ShaderResourceView* PPImageShaderView[2] = {};
		enum { NumBloomLevels = 4 };
		PPBlurLevel BlurLevels[NumBloomLevels];
		int Width = 0;
		int Height = 0;
		int Multisample = 0;
	} SceneBuffers;

	struct ScenePipelineState
	{
		D3D11_PRIMITIVE_TOPOLOGY PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		ID3D11PixelShader* PixelShader = nullptr;
		ID3D11BlendState* BlendState = nullptr;
		ID3D11DepthStencilState* DepthStencilState = nullptr;
	};

	struct
	{
		ID3D11VertexShader* VertexShader = nullptr;
		ID3D11InputLayout* InputLayout = nullptr;
		ID3D11Buffer* VertexBuffer = nullptr;
		ID3D11Buffer* IndexBuffer = nullptr;
		ID3D11Buffer* ConstantBuffer = nullptr;
		ID3D11RasterizerState* RasterizerState[2] = {};
		ID3D11PixelShader* PixelShader = {};
		ID3D11PixelShader* PixelShaderAlphaTest = {};
		ID3D11SamplerState* Samplers[16] = {};
		ScenePipelineState Pipelines[32];
		ScenePipelineState LinePipeline[2];
		ScenePipelineState PointPipeline;
		FLOAT LODBias = 0.0f;
	} ScenePass;

	static const int SceneVertexBufferSize = 16 * 1024;
	static const int SceneIndexBufferSize = 32 * 1024;

	struct DrawBatchEntry
	{
		size_t SceneIndexStart = 0;
		size_t SceneIndexEnd = 0;
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

	SceneVertex* SceneVertices = nullptr;
	size_t SceneVertexPos = 0;

	uint32_t* SceneIndexes = nullptr;
	size_t SceneIndexPos = 0;

	struct
	{
		ID3D11VertexShader* PPStep = nullptr;
		ID3D11InputLayout* PPStepLayout = nullptr;
		ID3D11Buffer* PPStepVertexBuffer = nullptr;
		ID3D11PixelShader* HitResolve = nullptr;
		ID3D11PixelShader* Present[16] = {};
		ID3D11Buffer* PresentConstantBuffer = nullptr;
		ID3D11Texture2D* DitherTexture = nullptr;
		ID3D11ShaderResourceView* DitherTextureView = nullptr;
		ID3D11BlendState* BlendState = nullptr;
		ID3D11DepthStencilState* DepthStencilState = nullptr;
		ID3D11RasterizerState* RasterizerState = nullptr;
	} PresentPass;

	struct
	{
		ID3D11PixelShader* Extract = nullptr;
		ID3D11PixelShader* Combine = nullptr;
		ID3D11PixelShader* BlurVertical = nullptr;
		ID3D11PixelShader* BlurHorizontal = nullptr;
		ID3D11Buffer* ConstantBuffer = nullptr;
		ID3D11BlendState* AdditiveBlendState = nullptr;
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
	BITFIELD OccludeLines;
	BITFIELD Bloom;
	BYTE BloomAmount;
	FLOAT LODBias;
	BYTE AntialiasMode;
	BYTE GammaMode;
	BYTE LightMode;
	INT RefreshRate;
	BITFIELD GammaCorrectScreenshots;

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
	void BlurStep(ID3D11ShaderResourceView* input, ID3D11RenderTargetView* output, bool vertical);
	float ComputeBlurGaussian(float n, float theta);
	void ComputeBlurSamples(int sampleCount, float blurAmount, float* sampleWeights);

	void SetPipeline(ScenePipelineState* pipeline);
	void SetPipeline(DWORD polyflags);
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
		if (!SceneVertices || !SceneIndexes)
			return { nullptr, nullptr, 0 };

		// If buffers are full, flush and wait for room.
		if (SceneVertexPos + vcount > (size_t)SceneVertexBufferSize || SceneIndexPos + icount > (size_t)SceneIndexBufferSize)
		{
			// If the request is larger than our buffers we can't draw this.
			if (vcount > (size_t)SceneVertexBufferSize || icount > (size_t)SceneIndexBufferSize)
				return { nullptr, nullptr, 0 };

			DrawBatches(true);
		}

		return { SceneVertices + SceneVertexPos, SceneIndexes + SceneIndexPos, (uint32_t)SceneVertexPos };
	}

	void UseVertices(size_t vcount, size_t icount)
	{
		SceneVertexPos += vcount;
		SceneIndexPos += icount;
	}

	int GetSettingsMultisample();

	ScenePipelineState* GetPipeline(DWORD PolyFlags);

	void CreateVertexShader(ID3D11VertexShader*& outShader, const std::string& shaderName, ID3D11InputLayout*& outInputLayout, const std::string& inputLayoutName, const std::vector<D3D11_INPUT_ELEMENT_DESC>& elements, const std::string& filename, const std::vector<std::string> defines = {});
	void CreatePixelShader(ID3D11PixelShader*& outShader, const std::string& shaderName, const std::string& filename, const std::vector<std::string> defines = {});
	std::vector<uint8_t> CompileHlsl(const std::string& filename, const std::string& shadertype, const std::vector<std::string> defines = {});

	vec4 ApplyInverseGamma(vec4 color);

	PresentPushConstants GetPresentPushConstants();

	template<typename T>
	void ReleaseObject(T*& obj) { if (obj) { obj->Release(); obj = nullptr; } }

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

	struct
	{
		int Width = 0;
		int Height = 0;
	} DesktopResolution;

	bool InSetResCall = false;
};

inline void ThrowIfFailed(HRESULT result, const char* msg) { if (FAILED(result)) throw std::runtime_error(msg); }

inline float GetUMult(const FTextureInfo& Info) { return 1.0f / (Info.UScale * Info.USize); }
inline float GetVMult(const FTextureInfo& Info) { return 1.0f / (Info.VScale * Info.VSize); }

inline void UD3D11RenderDevice::SetPipeline(ScenePipelineState* pipeline)
{
	if (pipeline != Batch.Pipeline)
	{
		AddDrawBatch();
		Batch.Pipeline = pipeline;
	}
}

inline void UD3D11RenderDevice::SetPipeline(DWORD PolyFlags)
{
	auto pipeline = GetPipeline(PolyFlags);
	if (pipeline != Batch.Pipeline)
	{
		AddDrawBatch();
		Batch.Pipeline = pipeline;
	}
}

inline void UD3D11RenderDevice::SetDescriptorSet(DWORD PolyFlags, CachedTexture* tex, CachedTexture* lightmap, CachedTexture* macrotex, CachedTexture* detailtex, bool clamp)
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
