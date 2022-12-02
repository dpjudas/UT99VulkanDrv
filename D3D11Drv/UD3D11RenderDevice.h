#pragma once

#include "vec.h"
#include "mat.h"

#include "TextureManager.h"
#include "UploadManager.h"

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
	mat4 objectToProjection;
};

struct PresentPushConstants
{
	float InvGamma;
	float Contrast;
	float Saturation;
	float Brightness;
	int GrayFormula;
	int32_t padding1;
	int32_t padding2;
	int32_t padding3;
};

class UD3D11RenderDevice : public URenderDeviceOldUnreal469
{
public:
	DECLARE_CLASS(UD3D11RenderDevice, URenderDeviceOldUnreal469, CLASS_Config, D3D11Drv)

	UD3D11RenderDevice();
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
	ID3D11Device* Device = nullptr;
	D3D_FEATURE_LEVEL FeatureLevel = {};
	ID3D11DeviceContext* Context = nullptr;
	IDXGISwapChain* SwapChain = nullptr;
	ID3D11Texture2D* BackBuffer = nullptr;
	ID3D11RenderTargetView* BackBufferView = nullptr;

	struct
	{
		ID3D11Texture2D* ColorBuffer = nullptr;
		ID3D11Texture2D* DepthBuffer = nullptr;
		ID3D11Texture2D* PPImage = nullptr;
		ID3D11RenderTargetView* ColorBufferView = nullptr;
		ID3D11DepthStencilView* DepthBufferView = nullptr;
		ID3D11RenderTargetView* PPImageView = nullptr;
		ID3D11ShaderResourceView* PPImageShaderView = nullptr;
		int Width = 0;
		int Height = 0;
	} SceneBuffers;

	struct ScenePipelineState
	{
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
		ID3D11RasterizerState* RasterizerState = nullptr;
		ID3D11PixelShader* PixelShader = {};
		ID3D11PixelShader* PixelShaderAlphaTest = {};
		ID3D11SamplerState* Samplers[4] = {};
		ScenePipelineState Pipelines[32];
	} ScenePass;

	static const int SceneVertexBufferSize = 1 * 1024 * 1024;
	static const int SceneIndexBufferSize = 1 * 1024 * 1024;

	struct
	{
		ID3D11VertexShader* PPStep = nullptr;
		ID3D11InputLayout* PPStepLayout = nullptr;
		ID3D11Buffer* PPStepVertexBuffer = nullptr;
		ID3D11PixelShader* Present = nullptr;
		ID3D11Buffer* PresentConstantBuffer = nullptr;
		ID3D11Texture2D* DitherTexture = nullptr;
		ID3D11ShaderResourceView* DitherTextureView = nullptr;
		ID3D11BlendState* BlendState = nullptr;
		ID3D11DepthStencilState* DepthStencilState = nullptr;
		ID3D11RasterizerState* RasterizerState = nullptr;
	} PresentPass;

	std::unique_ptr<TextureManager> Textures;
	std::unique_ptr<UploadManager> Uploads;

	// Configuration.
	BITFIELD UseVSync;
	INT Multisample;
	FLOAT D3DBrightness;
	FLOAT D3DContrast;
	FLOAT D3DSaturation;
	INT D3DGrayFormula;

private:
	void ResizeSceneBuffers(int width, int height);
	void ClearTextureCache();
	void CreatePresentPass();
	void CreateScenePass();

	ScenePipelineState* GetPipeline(DWORD PolyFlags);

	std::vector<uint8_t> CompileHlsl(const std::string& filename, const std::string& shadertype, const std::vector<std::string> defines = {});

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

	bool IsLocked = false;
};

inline void ThrowIfFailed(HRESULT result, const char* msg) { if (FAILED(result)) throw std::runtime_error(msg); }

inline float GetUMult(const FTextureInfo& Info) { return 1.0f / (Info.UScale * Info.USize); }
inline float GetVMult(const FTextureInfo& Info) { return 1.0f / (Info.VScale * Info.VSize); }
