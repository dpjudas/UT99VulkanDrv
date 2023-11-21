
#include "Precomp.h"
#include "UD3D11RenderDevice.h"
#include "CachedTexture.h"
#include "UTF16.h"
#include "FileResource.h"
#include "halffloat.h"
#include <set>
#include <emmintrin.h>

IMPLEMENT_CLASS(UD3D11RenderDevice);

UD3D11RenderDevice::UD3D11RenderDevice()
{
}

void UD3D11RenderDevice::StaticConstructor()
{
	guard(UD3D11RenderDevice::StaticConstructor);

	SpanBased = 0;
	FullscreenOnly = 0;
	SupportsFogMaps = 1;
	SupportsDistanceFog = 0;
	SupportsTC = 1;
	SupportsLazyTextures = 0;
	PrefersDeferredLoad = 0;
	UseVSync = 1;
	AntialiasMode = 0;
	UsePrecache = 1;
	Coronas = 1;
	ShinySurfaces = 1;
	DetailTextures = 1;
	HighDetailActors = 1;
	VolumetricLighting = 1;

#if defined(OLDUNREAL469SDK)
	UseLightmapAtlas = 0; // Note: do not turn this on. It does not work and generates broken fogmaps.
	SupportsUpdateTextureRect = 1;
	MaxTextureSize = 4096;
	NeedsMaskedFonts = 0;
#endif

	GammaMode = 0;
	GammaOffset = 0.0f;
	GammaOffsetRed = 0.0f;
	GammaOffsetGreen = 0.0f;
	GammaOffsetBlue = 0.0f;

	LinearBrightness = 128; // 0.0f;
	Contrast = 128; // 1.0f;
	Saturation = 255; // 1.0f;
	GrayFormula = 1;

	Hdr = 0;
	OccludeLines = 0;

	LODBias = -0.5f;
	LightMode = 0;

#if defined(OLDUNREAL469SDK)
	new(GetClass(), TEXT("UseLightmapAtlas"), RF_Public) UBoolProperty(CPP_PROPERTY(UseLightmapAtlas), TEXT("Display"), CPF_Config);
#endif

	new(GetClass(), TEXT("UseVSync"), RF_Public) UBoolProperty(CPP_PROPERTY(UseVSync), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("UsePrecache"), RF_Public) UBoolProperty(CPP_PROPERTY(UsePrecache), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("GammaOffset"), RF_Public) UFloatProperty(CPP_PROPERTY(GammaOffset), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("GammaOffsetRed"), RF_Public) UFloatProperty(CPP_PROPERTY(GammaOffsetRed), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("GammaOffsetGreen"), RF_Public) UFloatProperty(CPP_PROPERTY(GammaOffsetGreen), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("GammaOffsetBlue"), RF_Public) UFloatProperty(CPP_PROPERTY(GammaOffsetBlue), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("LinearBrightness"), RF_Public) UByteProperty(CPP_PROPERTY(LinearBrightness), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("Contrast"), RF_Public) UByteProperty(CPP_PROPERTY(Contrast), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("Saturation"), RF_Public) UByteProperty(CPP_PROPERTY(Saturation), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("GrayFormula"), RF_Public) UIntProperty(CPP_PROPERTY(GrayFormula), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("Hdr"), RF_Public) UBoolProperty(CPP_PROPERTY(Hdr), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("OccludeLines"), RF_Public) UBoolProperty(CPP_PROPERTY(OccludeLines), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("LODBias"), RF_Public) UFloatProperty(CPP_PROPERTY(LODBias), TEXT("Display"), CPF_Config);

	UEnum* AntialiasModes = new(GetClass(), TEXT("AntialiasModes"))UEnum(nullptr);
	new(AntialiasModes->Names)FName(TEXT("Off"));
	new(AntialiasModes->Names)FName(TEXT("MSAA_2x"));
	new(AntialiasModes->Names)FName(TEXT("MSAA_4x"));
	new(GetClass(), TEXT("AntialiasMode"), RF_Public) UByteProperty(CPP_PROPERTY(AntialiasMode), TEXT("Display"), CPF_Config, AntialiasModes);

	UEnum* GammaModes = new(GetClass(), TEXT("GammaModes"))UEnum(nullptr);
	new(GammaModes->Names)FName(TEXT("D3D9"));
	new(GammaModes->Names)FName(TEXT("XOpenGL"));
	new(GetClass(), TEXT("GammaMode"), RF_Public) UByteProperty(CPP_PROPERTY(GammaMode), TEXT("Display"), CPF_Config, GammaModes);

	UEnum* LightModes = new(GetClass(), TEXT("LightModes"))UEnum(nullptr);
	new(LightModes->Names)FName(TEXT("Normal"));
	new(LightModes->Names)FName(TEXT("OneXBlending"));
	new(LightModes->Names)FName(TEXT("BrighterActors"));
	new(GetClass(), TEXT("LightMode"), RF_Public) UByteProperty(CPP_PROPERTY(LightMode), TEXT("Display"), CPF_Config, LightModes);

	unguard;
}

int UD3D11RenderDevice::GetSettingsMultisample()
{
	switch (AntialiasMode)
	{
	default:
	case 0: return 0;
	case 1: return 2;
	case 2: return 4;
	}
}

UBOOL UD3D11RenderDevice::Init(UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen)
{
	guard(UD3D11RenderDevice::Init);

	Viewport = InViewport;
	ActiveHdr = Hdr;

	try
	{
		std::vector<D3D_FEATURE_LEVEL> featurelevels =
		{
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0
		};

		// First try use a more recent way of creating the device and swap chain
		HRESULT result = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			0,
			D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT,
			featurelevels.data(), (UINT)featurelevels.size(),
			D3D11_SDK_VERSION,
			&Device,
			&FeatureLevel,
			&Context);

		// Wonderful API you got here, Microsoft. Good job.
		IDXGIDevice2* dxgiDevice = nullptr;
		IDXGIAdapter* dxgiAdapter = nullptr;
		IDXGIFactory2* dxgiFactory = nullptr;
		if (SUCCEEDED(result))
			result = Device->QueryInterface(__uuidof(IDXGIDevice2), (void**)&dxgiDevice);
		if (SUCCEEDED(result))
			result = dxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&dxgiAdapter);
		if (SUCCEEDED(result))
			result = dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), (void**)&dxgiFactory);
		if (SUCCEEDED(result))
		{
			DXGI_SWAP_CHAIN_DESC1 swapDesc = {};
			swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapDesc.Width = NewX;
			swapDesc.Height = NewY;
			swapDesc.Format = ActiveHdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
			swapDesc.BufferCount = UseVSync ? 2 : 3;
			swapDesc.SampleDesc.Count = 1;
			swapDesc.Scaling = DXGI_SCALING_STRETCH;
			swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			swapDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
			swapDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
			result = dxgiFactory->CreateSwapChainForHwnd(Device, (HWND)Viewport->GetWindow(), &swapDesc, nullptr, nullptr, &SwapChain1);
		}
		if (SUCCEEDED(result))
		{
			SwapChain = SwapChain1;
		}
		else
		{
			ReleaseObject(Context);
			ReleaseObject(Device);
		}
		ReleaseObject(dxgiFactory);
		ReleaseObject(dxgiAdapter);
		ReleaseObject(dxgiDevice);

		// We still don't have a swap chain. Let's try the older Windows 7 API
		if (!SwapChain)
		{
			DXGI_SWAP_CHAIN_DESC swapDesc = {};
			swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapDesc.BufferDesc.Width = NewX;
			swapDesc.BufferDesc.Height = NewY;
			swapDesc.BufferDesc.Format = ActiveHdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
			swapDesc.BufferCount = 2;
			swapDesc.SampleDesc.Count = 1;
			swapDesc.OutputWindow = (HWND)Viewport->GetWindow();
			swapDesc.Windowed = TRUE;
			swapDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

			// First try create a swap chain for Windows 8 and newer. If that fails, try the old for Windows 7
			HRESULT result = E_FAIL;
			for (DXGI_SWAP_EFFECT swapeffect : { DXGI_SWAP_EFFECT_FLIP_DISCARD, DXGI_SWAP_EFFECT_DISCARD })
			{
				swapDesc.SwapEffect = swapeffect;

				result = D3D11CreateDeviceAndSwapChain(
					nullptr,
					D3D_DRIVER_TYPE_HARDWARE,
					0,
					D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT,
					featurelevels.data(), (UINT)featurelevels.size(),
					D3D11_SDK_VERSION,
					&swapDesc,
					&SwapChain,
					&Device,
					&FeatureLevel,
					&Context);
				if (SUCCEEDED(result))
					break;
			}
			ThrowIfFailed(result, "D3D11CreateDeviceAndSwapChain failed");
		}

		if (ActiveHdr)
		{
			IDXGISwapChain3* swapChain3 = nullptr;
			HRESULT result = SwapChain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&swapChain3);
			if (SUCCEEDED(result))
			{
				result = swapChain3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709);
				ReleaseObject(swapChain3);
			}
		}

		CreateScenePass();
		CreatePresentPass();

		Textures.reset(new TextureManager(this));
		Uploads.reset(new UploadManager(this));
	}
	catch (const std::exception& e)
	{
		debugf(TEXT("Could not create d3d11 renderer: %s"), to_utf16(e.what()).c_str());
		Exit();
		return 0;
	}

	if (!SetRes(NewX, NewY, NewColorBytes, Fullscreen))
	{
		Exit();
		return 0;
	}

	return 1;
	unguard;
}

UBOOL UD3D11RenderDevice::SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen)
{
	guard(UD3D11RenderDevice::SetRes);

	ReleaseObject(BackBuffer);
	ReleaseObject(BackBufferView);

	if (!Viewport->ResizeViewport(Fullscreen ? (BLIT_Fullscreen | BLIT_Direct3D) : (BLIT_HardwarePaint | BLIT_Direct3D), NewX, NewY, NewColorBytes))
	{
		debugf(TEXT("Viewport.ResizeViewport failed (%d, %d, %d, %d)"), NewX, NewY, NewColorBytes, (INT)Fullscreen);
		return FALSE;
	}

	HRESULT result;

	if (Fullscreen)
	{
		DXGI_MODE_DESC modeDesc = {};
		modeDesc.Width = NewX;
		modeDesc.Height = NewY;
		modeDesc.Format = ActiveHdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
		result = SwapChain->ResizeTarget(&modeDesc);
		if (FAILED(result))
		{
			debugf(TEXT("SwapChain.ResizeTarget failed (%d, %d, %d, %d)"), NewX, NewY, NewColorBytes, (INT)Fullscreen);
		}
	}

	result = SwapChain->SetFullscreenState(Fullscreen ? TRUE : FALSE, nullptr);
	if (FAILED(result))
	{
		debugf(TEXT("SwapChain.SetFullscreenState failed (%d, %d, %d, %d)"), NewX, NewY, NewColorBytes, (INT)Fullscreen);
		return FALSE;
	}

	result = SwapChain->ResizeBuffers(2, NewX, NewY, ActiveHdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
	if (FAILED(result))
	{
		debugf(TEXT("SwapChain.ResizeBuffers failed (%d, %d, %d, %d)"), NewX, NewY, NewColorBytes, (INT)Fullscreen);
		return FALSE;
	}

	if (ActiveHdr)
	{
		IDXGISwapChain3* swapChain3 = nullptr;
		HRESULT result = SwapChain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&swapChain3);
		if (SUCCEEDED(result))
		{
			result = swapChain3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709);
			ReleaseObject(swapChain3);
		}
	}

	if (NewX && NewY)
	{
		try
		{
			ResizeSceneBuffers(NewX, NewY, GetSettingsMultisample());
		}
		catch (const std::exception& e)
		{
			debugf(TEXT("Could not resize scene buffers: %s"), to_utf16(e.what()).c_str());
			return FALSE;
		}
	}

	result = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&BackBuffer);
	if (FAILED(result))
		return FALSE;

	result = Device->CreateRenderTargetView(BackBuffer, nullptr, &BackBufferView);
	if (FAILED(result))
		return FALSE;

	SaveConfig();

#if defined(UNREALGOLD)
	Flush();
#else
	Flush(1);
#endif

	return 1;
	unguard;
}

void UD3D11RenderDevice::Exit()
{
	guard(UD3D11RenderDevice::Exit);

	if (SceneVertices)
	{
		Context->Unmap(ScenePass.VertexBuffer, 0);
		SceneVertices = nullptr;
	}

	if (SceneIndexes)
	{
		Context->Unmap(ScenePass.IndexBuffer, 0);
		SceneIndexes = nullptr;
	}

	Uploads.reset();
	Textures.reset();
	ReleaseObject(PresentPass.PPStepLayout);
	ReleaseObject(PresentPass.PPStep);
	ReleaseObject(PresentPass.PPStepVertexBuffer);
	ReleaseObject(PresentPass.HitResolve);
	for (auto& shader : PresentPass.Present) ReleaseObject(shader);
	ReleaseObject(PresentPass.PresentConstantBuffer);
	ReleaseObject(PresentPass.DitherTextureView);
	ReleaseObject(PresentPass.DitherTexture);
	ReleaseObject(PresentPass.BlendState);
	ReleaseObject(PresentPass.DepthStencilState);
	ReleaseObject(ScenePass.VertexShader);
	ReleaseObject(ScenePass.InputLayout);
	ReleaseObject(ScenePass.VertexBuffer);
	ReleaseObject(ScenePass.IndexBuffer);
	ReleaseObject(ScenePass.ConstantBuffer);
	ReleaseObject(ScenePass.RasterizerState[0]);
	ReleaseObject(ScenePass.RasterizerState[1]);
	ReleaseObject(ScenePass.PixelShader);
	ReleaseObject(ScenePass.PixelShaderAlphaTest);
	for (auto& sampler : ScenePass.Samplers)
	{
		ReleaseObject(sampler);
	}
	for (auto& pipeline : ScenePass.Pipelines)
	{
		ReleaseObject(pipeline.BlendState);
		ReleaseObject(pipeline.DepthStencilState);
	}
	for (int i = 0; i < 2; i++)
	{
		ReleaseObject(ScenePass.LinePipeline[i].BlendState);
		ReleaseObject(ScenePass.LinePipeline[i].DepthStencilState);
	}
	ReleaseObject(ScenePass.PointPipeline.BlendState);
	ReleaseObject(ScenePass.PointPipeline.DepthStencilState);
	ReleaseObject(SceneBuffers.ColorBufferView);
	ReleaseObject(SceneBuffers.HitBufferView);
	ReleaseObject(SceneBuffers.HitBufferShaderView);
	ReleaseObject(SceneBuffers.PPHitBufferView);
	ReleaseObject(SceneBuffers.DepthBufferView);
	ReleaseObject(SceneBuffers.PPImageShaderView);
	ReleaseObject(SceneBuffers.PPImageView);
	ReleaseObject(SceneBuffers.ColorBuffer);
	ReleaseObject(SceneBuffers.StagingHitBuffer);
	ReleaseObject(SceneBuffers.PPHitBuffer);
	ReleaseObject(SceneBuffers.HitBuffer);
	ReleaseObject(SceneBuffers.DepthBuffer);
	ReleaseObject(SceneBuffers.PPImage);
	ReleaseObject(BackBufferView);
	ReleaseObject(BackBuffer);
	ReleaseObject(SwapChain);
	ReleaseObject(Context);
	ReleaseObject(Device);

	unguard;
}

void UD3D11RenderDevice::ResizeSceneBuffers(int width, int height, int multisample)
{
	multisample = std::max(multisample, 1);

	if (SceneBuffers.Width == width && SceneBuffers.Height == height && multisample == SceneBuffers.Multisample && SceneBuffers.ColorBuffer && SceneBuffers.HitBuffer && SceneBuffers.PPHitBuffer && SceneBuffers.StagingHitBuffer && SceneBuffers.DepthBuffer && SceneBuffers.PPImage)
		return;

	ReleaseObject(SceneBuffers.ColorBufferView);
	ReleaseObject(SceneBuffers.HitBufferView);
	ReleaseObject(SceneBuffers.HitBufferShaderView);
	ReleaseObject(SceneBuffers.PPHitBufferView);
	ReleaseObject(SceneBuffers.DepthBufferView);
	ReleaseObject(SceneBuffers.PPImageShaderView);
	ReleaseObject(SceneBuffers.PPImageView);
	ReleaseObject(SceneBuffers.ColorBuffer);
	ReleaseObject(SceneBuffers.StagingHitBuffer);
	ReleaseObject(SceneBuffers.PPHitBuffer);
	ReleaseObject(SceneBuffers.HitBuffer);
	ReleaseObject(SceneBuffers.DepthBuffer);
	ReleaseObject(SceneBuffers.PPImage);

	SceneBuffers.Width = width;
	SceneBuffers.Height = height;
	SceneBuffers.Multisample = multisample;

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
	texDesc.Width = SceneBuffers.Width;
	texDesc.Height = SceneBuffers.Height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	texDesc.SampleDesc.Count = SceneBuffers.Multisample;
	texDesc.SampleDesc.Quality = SceneBuffers.Multisample > 1 ? D3D11_STANDARD_MULTISAMPLE_PATTERN : 0;
	HRESULT result = Device->CreateTexture2D(&texDesc, nullptr, &SceneBuffers.ColorBuffer);
	ThrowIfFailed(result, "CreateTexture2D(ColorBuffer) failed");

	texDesc = {};
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.Width = SceneBuffers.Width;
	texDesc.Height = SceneBuffers.Height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R32_UINT;
	texDesc.SampleDesc.Count = SceneBuffers.Multisample;
	texDesc.SampleDesc.Quality = SceneBuffers.Multisample > 1 ? D3D11_STANDARD_MULTISAMPLE_PATTERN : 0;
	result = Device->CreateTexture2D(&texDesc, nullptr, &SceneBuffers.HitBuffer);
	ThrowIfFailed(result, "CreateTexture2D(HitBuffer) failed");

	texDesc = {};
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
	texDesc.Width = SceneBuffers.Width;
	texDesc.Height = SceneBuffers.Height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R32_UINT;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	result = Device->CreateTexture2D(&texDesc, nullptr, &SceneBuffers.PPHitBuffer);
	ThrowIfFailed(result, "CreateTexture2D(PPHitBuffer) failed");

	texDesc = {};
	texDesc.Usage = D3D11_USAGE_STAGING;
	texDesc.BindFlags = 0;
	texDesc.Width = SceneBuffers.Width;
	texDesc.Height = SceneBuffers.Height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R32_UINT;
	texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	result = Device->CreateTexture2D(&texDesc, nullptr, &SceneBuffers.StagingHitBuffer);
	ThrowIfFailed(result, "CreateTexture2D(StagingHitBuffer) failed");

	texDesc = {};
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	texDesc.Width = SceneBuffers.Width;
	texDesc.Height = SceneBuffers.Height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_D32_FLOAT;
	texDesc.SampleDesc.Count = SceneBuffers.Multisample;
	texDesc.SampleDesc.Quality = SceneBuffers.Multisample > 1 ? D3D11_STANDARD_MULTISAMPLE_PATTERN : 0;
	result = Device->CreateTexture2D(&texDesc, nullptr, &SceneBuffers.DepthBuffer);
	ThrowIfFailed(result, "CreateTexture2D(DepthBuffer) failed");

	texDesc = {};
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.Width = SceneBuffers.Width;
	texDesc.Height = SceneBuffers.Height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	result = Device->CreateTexture2D(&texDesc, nullptr, &SceneBuffers.PPImage);
	ThrowIfFailed(result, "CreateTexture2D(PPImage) failed");

	result = Device->CreateRenderTargetView(SceneBuffers.ColorBuffer, nullptr, &SceneBuffers.ColorBufferView);
	ThrowIfFailed(result, "CreateRenderTargetView(ColorBuffer) failed");

	result = Device->CreateRenderTargetView(SceneBuffers.HitBuffer, nullptr, &SceneBuffers.HitBufferView);
	ThrowIfFailed(result, "CreateRenderTargetView(HitBuffer) failed");

	result = Device->CreateShaderResourceView(SceneBuffers.HitBuffer, nullptr, &SceneBuffers.HitBufferShaderView);
	ThrowIfFailed(result, "CreateShaderResourceView(HitBuffer) failed");

	result = Device->CreateRenderTargetView(SceneBuffers.PPHitBuffer, nullptr, &SceneBuffers.PPHitBufferView);
	ThrowIfFailed(result, "CreateRenderTargetView(PPHitBuffer) failed");

	result = Device->CreateDepthStencilView(SceneBuffers.DepthBuffer, nullptr, &SceneBuffers.DepthBufferView);
	ThrowIfFailed(result, "CreateDepthStencilView(DepthBuffer) failed");

	result = Device->CreateRenderTargetView(SceneBuffers.PPImage, nullptr, &SceneBuffers.PPImageView);
	ThrowIfFailed(result, "CreateRenderTargetView(PPImage) failed");

	result = Device->CreateShaderResourceView(SceneBuffers.PPImage, nullptr, &SceneBuffers.PPImageShaderView);
	ThrowIfFailed(result, "CreateShaderResourceView(PPImage) failed");
}

void UD3D11RenderDevice::CreateScenePass()
{
	std::vector<uint8_t> vscode = CompileHlsl("shaders/Scene.vert", "vs");
	HRESULT result = Device->CreateVertexShader(vscode.data(), vscode.size(), nullptr, &ScenePass.VertexShader);
	ThrowIfFailed(result, "CreateVertexShader(ScenePass.VertexShader) failed");

	std::vector<D3D11_INPUT_ELEMENT_DESC> elements =
	{
		{ "AttrFlags", 0, DXGI_FORMAT_R32_UINT, 0, offsetof(SceneVertex, Flags), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "AttrPos", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(SceneVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "AttrTexCoordOne", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(SceneVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "AttrTexCoordTwo", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(SceneVertex, TexCoord2), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "AttrTexCoordThree", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(SceneVertex, TexCoord3), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "AttrTexCoordFour", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(SceneVertex, TexCoord4), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "AttrColor", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(SceneVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	result = Device->CreateInputLayout(elements.data(), (UINT)elements.size(), vscode.data(), vscode.size(), &ScenePass.InputLayout);
	ThrowIfFailed(result, "CreateInputLayout(ScenePass.InputLayout) failed");

	std::vector<uint8_t> pscode = CompileHlsl("shaders/Scene.frag", "ps");
	result = Device->CreatePixelShader(pscode.data(), pscode.size(), nullptr, &ScenePass.PixelShader);
	ThrowIfFailed(result, "CreatePixelShader(ScenePass.PixelShader) failed");

	std::vector<uint8_t> pscodeAT = CompileHlsl("shaders/Scene.frag", "ps", { "ALPHATEST" });
	result = Device->CreatePixelShader(pscodeAT.data(), pscodeAT.size(), nullptr, &ScenePass.PixelShaderAlphaTest);
	ThrowIfFailed(result, "CreatePixelShader(ScenePass.PixelShaderAlphaTest) failed");

	for (int i = 0; i < 16; i++)
	{
		int dummyMipmapCount = (i >> 2) & 3;
		D3D11_FILTER filter = (i & 1) ? D3D11_FILTER_MIN_MAG_MIP_POINT : D3D11_FILTER_ANISOTROPIC;
		D3D11_TEXTURE_ADDRESS_MODE addressmode = (i & 2) ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;
		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.MinLOD = dummyMipmapCount;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		samplerDesc.BorderColor[0] = 1.0f;
		samplerDesc.BorderColor[1] = 1.0f;
		samplerDesc.BorderColor[2] = 1.0f;
		samplerDesc.BorderColor[3] = 1.0f;
		samplerDesc.MaxAnisotropy = 8.0f;
		samplerDesc.MipLODBias = (float)dummyMipmapCount + LODBias;
		samplerDesc.Filter = filter;
		samplerDesc.AddressU = addressmode;
		samplerDesc.AddressV = addressmode;
		samplerDesc.AddressW = addressmode;
		result = Device->CreateSamplerState(&samplerDesc, &ScenePass.Samplers[i]);
		ThrowIfFailed(result, "CreateSamplerState(ScenePass.Samplers) failed");
	}

	for (int i = 0; i < 2; i++)
	{
		D3D11_RASTERIZER_DESC rasterizerDesc = {};
		rasterizerDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerDesc.CullMode = D3D11_CULL_NONE;
		rasterizerDesc.FrontCounterClockwise = FALSE;
		rasterizerDesc.DepthClipEnable = FALSE; // Avoid clipping the weapon. The UE1 engine clips the geometry anyway.
		rasterizerDesc.MultisampleEnable = i == 1 ? TRUE : FALSE;
		result = Device->CreateRasterizerState(&rasterizerDesc, &ScenePass.RasterizerState[i]);
		ThrowIfFailed(result, "CreateRasterizerState(ScenePass.Pipelines.RasterizerState) failed");
	}

	for (int i = 0; i < 32; i++)
	{
		D3D11_BLEND_DESC blendDesc = {};
		blendDesc.IndependentBlendEnable = TRUE;
		blendDesc.RenderTarget[0].BlendEnable = TRUE;
		switch (i & 3)
		{
		case 0: // PF_Translucent
			blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_COLOR;
			blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
			break;
		case 1: // PF_Modulated
			blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_DEST_COLOR;
			blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_DEST_ALPHA;
			blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_SRC_COLOR;
			blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_SRC_ALPHA;
			break;
		case 2: // PF_Highlighted
			blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
			break;
		case 3: // Hmm, is it faster to keep the blend mode enabled or to toggle it?
			blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
			blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
			break;
		}
		if (i & 4) // PF_Invisible
			blendDesc.RenderTarget[0].RenderTargetWriteMask = 0;
		else
			blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		blendDesc.RenderTarget[1].BlendEnable = FALSE;
		blendDesc.RenderTarget[1].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		result = Device->CreateBlendState(&blendDesc, &ScenePass.Pipelines[i].BlendState);
		ThrowIfFailed(result, "CreateBlendState(ScenePass.Pipelines.BlendState) failed");

		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = TRUE;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		if (i & 8) // PF_Occlude
			depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		else
			depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		result = Device->CreateDepthStencilState(&depthStencilDesc, &ScenePass.Pipelines[i].DepthStencilState);
		ThrowIfFailed(result, "CreateDepthStencilState(ScenePass.Pipelines.DepthStencilState) failed");

		if (i & 16) // PF_Masked
			ScenePass.Pipelines[i].PixelShader = ScenePass.PixelShaderAlphaTest;
		else
			ScenePass.Pipelines[i].PixelShader = ScenePass.PixelShader;

		ScenePass.Pipelines[i].PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}

	// Line pipeline
	for (int i = 0; i < 2; i++)
	{
		D3D11_BLEND_DESC blendDesc = {};
		blendDesc.IndependentBlendEnable = TRUE;
		blendDesc.RenderTarget[0].BlendEnable = TRUE;
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		blendDesc.RenderTarget[1].BlendEnable = FALSE;
		blendDesc.RenderTarget[1].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		result = Device->CreateBlendState(&blendDesc, &ScenePass.LinePipeline[i].BlendState);
		ThrowIfFailed(result, "CreateBlendState(ScenePass.LinePipeline.BlendState) failed");

		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = i ? TRUE : FALSE;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		result = Device->CreateDepthStencilState(&depthStencilDesc, &ScenePass.LinePipeline[i].DepthStencilState);
		ThrowIfFailed(result, "CreateDepthStencilState(ScenePass.LinePipeline.DepthStencilState) failed");

		ScenePass.LinePipeline[i].PixelShader = ScenePass.PixelShader;
		ScenePass.LinePipeline[i].PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	}

	// Point pipeline
	{
		D3D11_BLEND_DESC blendDesc = {};
		blendDesc.IndependentBlendEnable = TRUE;
		blendDesc.RenderTarget[0].BlendEnable = TRUE;
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		blendDesc.RenderTarget[1].BlendEnable = FALSE;
		blendDesc.RenderTarget[1].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		result = Device->CreateBlendState(&blendDesc, &ScenePass.PointPipeline.BlendState);
		ThrowIfFailed(result, "CreateBlendState(ScenePass.LinePipeline.BlendState) failed");

		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = FALSE;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		result = Device->CreateDepthStencilState(&depthStencilDesc, &ScenePass.PointPipeline.DepthStencilState);
		ThrowIfFailed(result, "CreateDepthStencilState(ScenePass.PointPipeline.DepthStencilState) failed");

		ScenePass.PointPipeline.PixelShader = ScenePass.PixelShader;
		ScenePass.PointPipeline.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}

	D3D11_BUFFER_DESC bufDesc = {};
	bufDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufDesc.ByteWidth = SceneVertexBufferSize * sizeof(SceneVertex);
	bufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	result = Device->CreateBuffer(&bufDesc, nullptr, &ScenePass.VertexBuffer);
	ThrowIfFailed(result, "CreateBuffer(ScenePass.VertexBuffer) failed");

	bufDesc = {};
	bufDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufDesc.ByteWidth = SceneIndexBufferSize * sizeof(uint32_t);
	bufDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	result = Device->CreateBuffer(&bufDesc, nullptr, &ScenePass.IndexBuffer);
	ThrowIfFailed(result, "CreateBuffer(ScenePass.IndexBuffer) failed");

	bufDesc = {};
	bufDesc.Usage = D3D11_USAGE_DEFAULT;
	bufDesc.ByteWidth = sizeof(ScenePushConstants);
	bufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	result = Device->CreateBuffer(&bufDesc, nullptr, &ScenePass.ConstantBuffer);
	ThrowIfFailed(result, "CreateBuffer(ScenePass.ConstantBuffer) failed");
}

UD3D11RenderDevice::ScenePipelineState* UD3D11RenderDevice::GetPipeline(DWORD PolyFlags)
{
	// Adjust PolyFlags according to Unreal's precedence rules.
	if (!(PolyFlags & (PF_Translucent | PF_Modulated)))
		PolyFlags |= PF_Occlude;
	else if (PolyFlags & PF_Translucent)
		PolyFlags &= ~PF_Masked;

	int index;
	if (PolyFlags & PF_Translucent)
	{
		index = 0;
	}
	else if (PolyFlags & PF_Modulated)
	{
		index = 1;
	}
	else if (PolyFlags & PF_Highlighted)
	{
		index = 2;
	}
	else
	{
		index = 3;
	}

	if (PolyFlags & PF_Invisible)
	{
		index |= 4;
	}
	if (PolyFlags & PF_Occlude)
	{
		index |= 8;
	}
	if (PolyFlags & PF_Masked)
	{
		index |= 16;
	}

	return &ScenePass.Pipelines[index];
}

void UD3D11RenderDevice::CreatePresentPass()
{
	std::vector<vec2> positions =
	{
		vec2(-1.0, -1.0),
		vec2( 1.0, -1.0),
		vec2(-1.0,  1.0),
		vec2(-1.0,  1.0),
		vec2( 1.0, -1.0),
		vec2( 1.0,  1.0)
	};

	D3D11_BUFFER_DESC bufDesc = {};
	bufDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufDesc.ByteWidth = positions.size() * sizeof(vec2);
	bufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = positions.data();

	HRESULT result = Device->CreateBuffer(&bufDesc, &initData, &PresentPass.PPStepVertexBuffer);
	ThrowIfFailed(result, "CreateBuffer(PresentPass.PPStepVertexBuffer) failed");

	bufDesc = {};
	bufDesc.Usage = D3D11_USAGE_DEFAULT;
	bufDesc.ByteWidth = sizeof(PresentPushConstants);
	bufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	result = Device->CreateBuffer(&bufDesc, nullptr, &PresentPass.PresentConstantBuffer);
	ThrowIfFailed(result, "CreateBuffer(PresentPass.PresentConstantBuffer) failed");

	std::vector<uint8_t> ppstep = CompileHlsl("shaders/PPStep.vert", "vs");
	result = Device->CreateVertexShader(ppstep.data(), ppstep.size(), nullptr, &PresentPass.PPStep);
	ThrowIfFailed(result, "CreateVertexShader(PresentPass.PPStep) failed");

	static const char* transferFunctions[2] = { nullptr, "HDR_MODE" };
	static const char* gammaModes[2] = { "GAMMA_MODE_D3D9", "GAMMA_MODE_XOPENGL" };
	static const char* colorModes[4] = { nullptr, "COLOR_CORRECT_MODE0", "COLOR_CORRECT_MODE1", "COLOR_CORRECT_MODE2" };
	for (int i = 0; i < 16; i++)
	{
		std::vector<std::string> defines;
		if (transferFunctions[i & 1]) defines.push_back(transferFunctions[i & 1]);
		if (gammaModes[(i >> 1) & 1]) defines.push_back(gammaModes[(i >> 1) & 1]);
		if (colorModes[(i >> 2) & 3]) defines.push_back(colorModes[(i >> 2) & 3]);

		std::vector<uint8_t> present = CompileHlsl("shaders/Present.frag", "ps", defines);
		result = Device->CreatePixelShader(present.data(), present.size(), nullptr, &PresentPass.Present[i]);
		ThrowIfFailed(result, "CreatePixelShader(PresentPass.Present) failed");
	}

	std::vector<uint8_t> hitresolve = CompileHlsl("shaders/HitResolve.frag", "ps");
	result = Device->CreatePixelShader(hitresolve.data(), hitresolve.size(), nullptr, &PresentPass.HitResolve);
	ThrowIfFailed(result, "CreatePixelShader(PresentPass.HitResolve) failed");

	std::vector<D3D11_INPUT_ELEMENT_DESC> elements =
	{
		{ "AttrPos", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	result = Device->CreateInputLayout(elements.data(), (UINT)elements.size(), ppstep.data(), ppstep.size(), &PresentPass.PPStepLayout);
	ThrowIfFailed(result, "CreateInputLayout(PresentPass.PPStepLayout) failed");

	static const float ditherdata[64] =
	{
		.0078125, .2578125, .1328125, .3828125, .0234375, .2734375, .1484375, .3984375,
		.7578125, .5078125, .8828125, .6328125, .7734375, .5234375, .8984375, .6484375,
		.0703125, .3203125, .1953125, .4453125, .0859375, .3359375, .2109375, .4609375,
		.8203125, .5703125, .9453125, .6953125, .8359375, .5859375, .9609375, .7109375,
		.0390625, .2890625, .1640625, .4140625, .0546875, .3046875, .1796875, .4296875,
		.7890625, .5390625, .9140625, .6640625, .8046875, .5546875, .9296875, .6796875,
		.1015625, .3515625, .2265625, .4765625, .1171875, .3671875, .2421875, .4921875,
		.8515625, .6015625, .9765625, .7265625, .8671875, .6171875, .9921875, .7421875
	};

	initData = {};
	initData.pSysMem = ditherdata;
	initData.SysMemPitch = sizeof(float) * 8;

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Usage = D3D11_USAGE_IMMUTABLE;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texDesc.Format = DXGI_FORMAT_R32_FLOAT;
	texDesc.Width = 8;
	texDesc.Height = 8;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.SampleDesc.Count = 1;
	result = Device->CreateTexture2D(&texDesc, &initData, &PresentPass.DitherTexture);
	ThrowIfFailed(result, "CreateTexture2D(DitherTexture) failed");

	result = Device->CreateShaderResourceView(PresentPass.DitherTexture, nullptr, &PresentPass.DitherTextureView);
	ThrowIfFailed(result, "CreateShaderResourceView(DitherTexture) failed");

	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	result = Device->CreateBlendState(&blendDesc, &PresentPass.BlendState);
	ThrowIfFailed(result, "CreateBlendState(PresentPass.BlendState) failed");

	D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	result = Device->CreateDepthStencilState(&depthStencilDesc, &PresentPass.DepthStencilState);
	ThrowIfFailed(result, "CreateDepthStencilState(PresentPass.DepthStencilState) failed");

	D3D11_RASTERIZER_DESC rasterizerDesc = {};
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	result = Device->CreateRasterizerState(&rasterizerDesc, &PresentPass.RasterizerState);
	ThrowIfFailed(result, "CreateRasterizerState(PresentPass.RasterizerState) failed");
}

#if defined(UNREALGOLD)

void UD3D11RenderDevice::Flush()
{
	guard(UD3D11RenderDevice::Flush);

	DrawBatches();
	ClearTextureCache();

	if (UsePrecache && !GIsEditor)
		PrecacheOnFlip = 1;

	unguard;
}

#else

void UD3D11RenderDevice::Flush(UBOOL AllowPrecache)
{
	guard(UD3D11RenderDevice::Flush);

	DrawBatches();
	ClearTextureCache();

	if (AllowPrecache && UsePrecache && !GIsEditor)
		PrecacheOnFlip = 1;

	unguard;
}

#endif

UBOOL UD3D11RenderDevice::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	guard(UD3D11RenderDevice::Exec);

#if !defined(UNREALGOLD)
	if (URenderDevice::Exec(Cmd, Ar))
	{
		return 1;
	}
#endif
	if (ParseCommand(&Cmd, TEXT("DGL")))
	{
		if (ParseCommand(&Cmd, TEXT("BUFFERTRIS")))
		{
			return 1;
		}
		else if (ParseCommand(&Cmd, TEXT("BUILD")))
		{
			return 1;
		}
		else if (ParseCommand(&Cmd, TEXT("AA")))
		{
			return 1;
		}
		return 0;
	}
	else if (ParseCommand(&Cmd, TEXT("GetRes")))
	{
		struct Resolution
		{
			int X;
			int Y;

			// For sorting highest resolution first
			bool operator<(const Resolution& other) const { if (X != other.X) return X > other.X; else return Y > other.Y; }
		};

		std::set<Resolution> resolutions;

		// Always include what the monitor is currently using
		HDC screenDC = GetDC(0);
		int screenWidth = GetDeviceCaps(screenDC, HORZRES);
		int screenHeight = GetDeviceCaps(screenDC, VERTRES);
		resolutions.insert({ screenWidth, screenHeight });
		ReleaseDC(0, screenDC);

		FString Str;
		for (const Resolution& resolution : resolutions)
		{
			Str += FString::Printf(TEXT("%ix%i "), (INT)resolution.X, (INT)resolution.Y);
		}
		Ar.Log(*Str.LeftChop(1));
		return 1;
	}
	else
	{
		return 0;
	}

	unguard;
}

void UD3D11RenderDevice::Lock(FPlane InFlashScale, FPlane InFlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* InHitData, INT* InHitSize)
{
	guard(UD3D11RenderDevice::Lock);

	if (Viewport->SizeX && Viewport->SizeY)
	{
		try
		{
			ResizeSceneBuffers(SceneBuffers.Width, SceneBuffers.Height, GetSettingsMultisample());
		}
		catch (const std::exception& e)
		{
			debugf(TEXT("Could not resize scene buffers: %s"), to_utf16(e.what()).c_str());
			return;
		}
	}

	HitData = InHitData;
	HitSize = InHitSize;

	FlashScale = InFlashScale;
	FlashFog = InFlashFog;

	FLOAT color[4] = { ScreenClear.X, ScreenClear.Y, ScreenClear.Z, ScreenClear.W };
	FLOAT zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	ID3D11RenderTargetView* views[2] = { SceneBuffers.ColorBufferView, SceneBuffers.HitBufferView };
	Context->ClearRenderTargetView(SceneBuffers.ColorBufferView, color);
	Context->ClearRenderTargetView(SceneBuffers.HitBufferView, zero);
	Context->ClearDepthStencilView(SceneBuffers.DepthBufferView, D3D11_CLEAR_DEPTH, 1.0f, 0);
	Context->OMSetRenderTargets(2, views, SceneBuffers.DepthBufferView);

	UINT stride = sizeof(SceneVertex);
	UINT offset = 0;
	Context->IASetVertexBuffers(0, 1, &ScenePass.VertexBuffer, &stride, &offset);
	Context->IASetIndexBuffer(ScenePass.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	Context->IASetInputLayout(ScenePass.InputLayout);
	Context->VSSetShader(ScenePass.VertexShader, nullptr, 0);
	Context->VSSetConstantBuffers(0, 1, &ScenePass.ConstantBuffer);
	Context->RSSetState(ScenePass.RasterizerState[SceneBuffers.Multisample > 1]);

	D3D11_RECT box = {};
	box.right = Viewport->SizeX;
	box.bottom = Viewport->SizeY;
	Context->RSSetScissorRects(1, &box);

	if (!SceneVertices)
	{
		D3D11_MAPPED_SUBRESOURCE mappedVertexBuffer = {};
		HRESULT result = Context->Map(ScenePass.VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedVertexBuffer);
		if (SUCCEEDED(result))
		{
			SceneVertices = (SceneVertex*)mappedVertexBuffer.pData;
		}
	}

	if (!SceneIndexes)
	{
		D3D11_MAPPED_SUBRESOURCE mappedIndexBuffer = {};
		HRESULT result = Context->Map(ScenePass.IndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedIndexBuffer);
		if (SUCCEEDED(result))
		{
			SceneIndexes = (uint32_t*)mappedIndexBuffer.pData;
		}
	}

	SceneConstants.HitIndex = 0;
	ForceHitIndex = -1;

	IsLocked = true;

	unguard;
}

void UD3D11RenderDevice::Unlock(UBOOL Blit)
{
	guard(UD3D11RenderDevice::Unlock);

	if (Blit || HitData)
		DrawBatches();

	if (Blit)
	{
		if (SceneBuffers.Multisample > 1)
		{
			Context->ResolveSubresource(SceneBuffers.PPImage, 0, SceneBuffers.ColorBuffer, 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
		}
		else
		{
			Context->CopyResource(SceneBuffers.PPImage, SceneBuffers.ColorBuffer);
		}

		Context->OMSetRenderTargets(1, &BackBufferView, nullptr);

		D3D11_VIEWPORT viewport = {};
		viewport.Width = Viewport->SizeX;
		viewport.Height = Viewport->SizeY;
		viewport.MaxDepth = 1.0f;
		Context->RSSetViewports(1, &viewport);

		PresentPushConstants pushconstants;
		if (Viewport->IsOrtho())
		{
			pushconstants.GammaCorrection = { 1.0f };
			pushconstants.Contrast = 1.0f;
			pushconstants.Saturation = 1.0f;
			pushconstants.Brightness = 0.0f;
		}
		else
		{
			float brightness = GIsEditor ? 1.0f : Clamp(Viewport->GetOuterUClient()->Brightness * 2.0, 0.05, 2.99);

			if (GammaMode == 0)
			{
				float invGammaRed = 1.0f / Max(brightness + GammaOffset + GammaOffsetRed, 0.001f);
				float invGammaGreen = 1.0f / Max(brightness + GammaOffset + GammaOffsetGreen, 0.001f);
				float invGammaBlue = 1.0f / Max(brightness + GammaOffset + GammaOffsetBlue, 0.001f);
				pushconstants.GammaCorrection = vec4(invGammaRed, invGammaGreen, invGammaBlue, 0.0f);
			}
			else
			{
				float invGammaRed = (GammaOffset + GammaOffsetRed + 2.0f) > 0.0f ? 1.0f / (GammaOffset + GammaOffsetRed + 1.0f) : 1.0f;
				float invGammaGreen = (GammaOffset + GammaOffsetGreen + 2.0f) > 0.0f ? 1.0f / (GammaOffset + GammaOffsetGreen + 1.0f) : 1.0f;
				float invGammaBlue = (GammaOffset + GammaOffsetBlue + 2.0f) > 0.0f ? 1.0f / (GammaOffset + GammaOffsetBlue + 1.0f) : 1.0f;
				pushconstants.GammaCorrection = vec4(invGammaRed, invGammaGreen, invGammaBlue, brightness);
			}

			// pushconstants.Contrast = clamp(Contrast, 0.1f, 3.f);
			if (Contrast >= 128)
			{
				pushconstants.Contrast = 1.0f + (Contrast - 128) / 127.0f * 3.0f;
			}
			else
			{
				pushconstants.Contrast = Max(Contrast / 128.0f, 0.1f);
			}

			// pushconstants.Saturation = clamp(Saturation, -1.0f, 1.0f);
			pushconstants.Saturation = 1.0f - 2.0f * (255 - Saturation) / 255.0f;

			// pushconstants.Brightness = clamp(LinearBrightness, -1.8f, 1.8f);
			if (LinearBrightness >= 128)
			{
				pushconstants.Brightness = (LinearBrightness - 128) / 127.0f * 1.8f;
			}
			else
			{
				pushconstants.Brightness = (128 - LinearBrightness) / 128.0f * -1.8f;
			}
		}

		// Select present shader based on what the user is actually using
		int presentShader = 0;
		if (ActiveHdr) presentShader |= 1;
		if (GammaMode == 1) presentShader |= 2;
		if (pushconstants.Brightness != 0.0f || pushconstants.Contrast != 1.0f || pushconstants.Saturation != 1.0f) presentShader |= (Clamp(GrayFormula, 0, 2) + 1) << 2;

		UINT stride = sizeof(vec2);
		UINT offset = 0;
		ID3D11ShaderResourceView* psResources[] = { SceneBuffers.PPImageShaderView, PresentPass.DitherTextureView };
		Context->IASetVertexBuffers(0, 1, &PresentPass.PPStepVertexBuffer, &stride, &offset);
		Context->IASetInputLayout(PresentPass.PPStepLayout);
		Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Context->VSSetShader(PresentPass.PPStep, nullptr, 0);
		Context->RSSetState(PresentPass.RasterizerState);
		Context->PSSetShader(PresentPass.Present[presentShader], nullptr, 0);
		Context->PSSetConstantBuffers(0, 1, &PresentPass.PresentConstantBuffer);
		Context->PSSetShaderResources(0, 2, psResources);
		Context->OMSetDepthStencilState(PresentPass.DepthStencilState, 0);
		Context->OMSetBlendState(PresentPass.BlendState, nullptr, 0xffffffff);
		Context->UpdateSubresource(PresentPass.PresentConstantBuffer, 0, nullptr, &pushconstants, 0, 0);
		Context->Draw(6, 0);

		if (SwapChain1)
		{
			DXGI_PRESENT_PARAMETERS presentParams = {};
			SwapChain1->Present1(UseVSync ? 1 : 0, 0, &presentParams);
		}
		else
		{
			SwapChain->Present(UseVSync ? 1 : 0, 0);
		}

		Batch.Pipeline = nullptr;
		Batch.Tex = nullptr;
		Batch.Lightmap = nullptr;
		Batch.Detailtex = nullptr;
		Batch.Macrotex = nullptr;
		Batch.SceneIndexStart = 0;

		if (SceneVertices)
		{
			Context->Unmap(ScenePass.VertexBuffer, 0);
			SceneVertices = nullptr;
		}

		if (SceneIndexes)
		{
			Context->Unmap(ScenePass.IndexBuffer, 0);
			SceneIndexes = nullptr;
		}

		SceneVertexPos = 0;
		SceneIndexPos = 0;
	}

	if (HitData)
	{
		D3D11_BOX box = {};
		box.left = Viewport->HitX;
		box.right = Viewport->HitX + Viewport->HitXL;
		box.top = SceneBuffers.Height - Viewport->HitY - Viewport->HitYL;
		box.bottom = SceneBuffers.Height - Viewport->HitY;
		box.front = 0;
		box.back = 1;

		// Resolve multisampling
		if (SceneBuffers.Multisample > 1)
		{
			Context->OMSetRenderTargets(1, &SceneBuffers.PPHitBufferView, nullptr);

			D3D11_VIEWPORT viewport = {};
			viewport.TopLeftX = box.left;
			viewport.TopLeftY = box.top;
			viewport.Width = box.right - box.left;
			viewport.Height = box.bottom - box.top;
			viewport.MaxDepth = 1.0f;
			Context->RSSetViewports(1, &viewport);

			UINT stride = sizeof(vec2);
			UINT offset = 0;
			Context->IASetVertexBuffers(0, 1, &PresentPass.PPStepVertexBuffer, &stride, &offset);
			Context->IASetInputLayout(PresentPass.PPStepLayout);
			Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			Context->VSSetShader(PresentPass.PPStep, nullptr, 0);
			Context->RSSetState(PresentPass.RasterizerState);
			Context->PSSetShader(PresentPass.HitResolve, nullptr, 0);
			Context->PSSetShaderResources(0, 1, &SceneBuffers.HitBufferShaderView);
			Context->OMSetDepthStencilState(PresentPass.DepthStencilState, 0);
			Context->OMSetBlendState(PresentPass.BlendState, nullptr, 0xffffffff);

			Context->Draw(6, 0);
		}
		else
		{
			Context->CopySubresourceRegion(SceneBuffers.PPHitBuffer, 0, box.left, box.top, 0, SceneBuffers.HitBuffer, 0, &box);
		}

		// Copy the hit buffer to a mappable texture, but only the part we want to examine
		Context->CopySubresourceRegion(SceneBuffers.StagingHitBuffer, 0, 0, 0, 0, SceneBuffers.PPHitBuffer, 0, &box);

		// Lock the buffer and look for the last hit
		int hit = 0;
		D3D11_MAPPED_SUBRESOURCE mapping = {};
		HRESULT result = Context->Map(SceneBuffers.StagingHitBuffer, 0, D3D11_MAP_READ, 0, &mapping);
		if (SUCCEEDED(result))
		{
			int width = Viewport->HitXL;
			int height = Viewport->HitYL;
			for (int y = 0; y < height; y++)
			{
				const INT* line = (const INT*)(((const char*)mapping.pData) + y * mapping.RowPitch);
				for (int x = 0; x < width; x++)
				{
					hit = std::max(hit, line[x]);
				}
			}
			Context->Unmap(SceneBuffers.StagingHitBuffer, 0);
		}
		hit--;

		if (hit < ForceHitIndex)
			hit = ForceHitIndex;

		if (hit >= 0 && hit < (int)HitQueries.size())
		{
			const HitQuery& query = HitQueries[hit];
			memcpy(HitData, HitBuffer.data() + query.Start, query.Count);
			*HitSize = query.Count;
		}
		else
		{
			*HitSize = 0;
		}
	}

	Context->OMSetRenderTargets(0, nullptr, nullptr);

	HitQueryStack.clear();
	HitQueries.clear();
	HitBuffer.clear();
	HitData = nullptr;
	HitSize = nullptr;

	IsLocked = false;

	unguard;
}

void UD3D11RenderDevice::PushHit(const BYTE* Data, INT Count)
{
	guard(UD3D11RenderDevice::PushHit);

	if (Count <= 0) return;
	HitQueryStack.insert(HitQueryStack.end(), Data, Data + Count);

	DrawBatches();

	INT index = HitQueries.size();

	HitQuery query;
	query.Start = HitBuffer.size();
	query.Count = HitQueryStack.size();
	HitQueries.push_back(query);

	HitBuffer.insert(HitBuffer.end(), HitQueryStack.begin(), HitQueryStack.end());

	SceneConstants.HitIndex = index + 1;
	Context->UpdateSubresource(ScenePass.ConstantBuffer, 0, nullptr, &SceneConstants, 0, 0);

	unguard;
}

void UD3D11RenderDevice::PopHit(INT Count, UBOOL bForce)
{
	guard(UD3D11RenderDevice::PopHit);

	if (bForce)
	{
		ForceHitIndex = HitQueries.size();

		HitQuery query;
		query.Start = HitBuffer.size();
		query.Count = HitQueryStack.size();
		HitQueries.push_back(query);
		HitBuffer.insert(HitBuffer.end(), HitQueryStack.begin(), HitQueryStack.end());
	}

	HitQueryStack.resize(HitQueryStack.size() - Count);

	unguard;
}

#if defined(OLDUNREAL469SDK)

UBOOL UD3D11RenderDevice::SupportsTextureFormat(ETextureFormat Format)
{
	guard(UD3D11RenderDevice::SupportsTextureFormat);

	return Uploads->SupportsTextureFormat(Format) ? TRUE : FALSE;

	unguard;
}

void UD3D11RenderDevice::UpdateTextureRect(FTextureInfo& Info, INT U, INT V, INT UL, INT VL)
{
	guard(UD3D11RenderDevice::UpdateTextureRect);

	Textures->UpdateTextureRect(&Info, U, V, UL, VL);

	unguard;
}

#endif

void UD3D11RenderDevice::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet)
{
	guard(UD3D11RenderDevice::DrawComplexSurface);

	if (SceneVertexPos + 1000 > SceneVertexBufferSize || SceneIndexPos + 1000 > SceneIndexBufferSize) NextSceneBuffers();

	CachedTexture* tex = Textures->GetTexture(Surface.Texture, !!(Surface.PolyFlags & PF_Masked));
	CachedTexture* lightmap = Textures->GetTexture(Surface.LightMap, false);
	CachedTexture* macrotex = Textures->GetTexture(Surface.MacroTexture, false);
	CachedTexture* detailtex = Textures->GetTexture(Surface.DetailTexture, false);
	CachedTexture* fogmap = Textures->GetTexture(Surface.FogMap, false);

#if defined(UNREALGOLD)
	if (Surface.DetailTexture && Surface.FogMap) detailtex = nullptr;
#else
	if ((Surface.DetailTexture && Surface.FogMap) || (!DetailTextures)) detailtex = nullptr;
#endif

	float UDot = Facet.MapCoords.XAxis | Facet.MapCoords.Origin;
	float VDot = Facet.MapCoords.YAxis | Facet.MapCoords.Origin;

	float UPan = tex ? UDot + Surface.Texture->Pan.X : 0.0f;
	float VPan = tex ? VDot + Surface.Texture->Pan.Y : 0.0f;
	float UMult = tex ? GetUMult(*Surface.Texture) : 0.0f;
	float VMult = tex ? GetVMult(*Surface.Texture) : 0.0f;
	float LMUPan = lightmap ? UDot + Surface.LightMap->Pan.X - 0.5f * Surface.LightMap->UScale : 0.0f;
	float LMVPan = lightmap ? VDot + Surface.LightMap->Pan.Y - 0.5f * Surface.LightMap->VScale : 0.0f;
	float LMUMult = lightmap ? GetUMult(*Surface.LightMap) : 0.0f;
	float LMVMult = lightmap ? GetVMult(*Surface.LightMap) : 0.0f;
	float MacroUPan = macrotex ? UDot + Surface.MacroTexture->Pan.X : 0.0f;
	float MacroVPan = macrotex ? VDot + Surface.MacroTexture->Pan.Y : 0.0f;
	float MacroUMult = macrotex ? GetUMult(*Surface.MacroTexture) : 0.0f;
	float MacroVMult = macrotex ? GetVMult(*Surface.MacroTexture) : 0.0f;
	float DetailUPan = detailtex ? UDot + Surface.DetailTexture->Pan.X : 0.0f;
	float DetailVPan = detailtex ? VDot + Surface.DetailTexture->Pan.Y : 0.0f;
	float DetailUMult = detailtex ? GetUMult(*Surface.DetailTexture) : 0.0f;
	float DetailVMult = detailtex ? GetVMult(*Surface.DetailTexture) : 0.0f;

	uint32_t flags = 0;
	if (lightmap) flags |= 1;
	if (macrotex) flags |= 2;
	if (detailtex && !fogmap) flags |= 4;
	if (fogmap) flags |= 8;

	if (LightMode == 1) flags |= 64;

	if (fogmap) // if Surface.FogMap exists, use instead of detail texture
	{
		detailtex = fogmap;
		DetailUPan = UDot + Surface.FogMap->Pan.X - 0.5f * Surface.FogMap->UScale;
		DetailVPan = VDot + Surface.FogMap->Pan.Y - 0.5f * Surface.FogMap->VScale;
		DetailUMult = GetUMult(*Surface.FogMap);
		DetailVMult = GetVMult(*Surface.FogMap);
	}

	SetPipeline(Surface.PolyFlags);
	SetDescriptorSet(Surface.PolyFlags, tex, lightmap, macrotex, detailtex);

	vec4 color(1.0f);

	// Draw the surface twice if the editor selected it. Second time highlighted without textures
	int drawcount = (Surface.PolyFlags & PF_Selected) && GIsEditor ? 2 : 1;
	while (drawcount-- > 0)
	{
		if (!SceneVertices || !SceneIndexes) return;

		uint32_t vpos = SceneVertexPos;
		uint32_t ipos = SceneIndexPos;

		SceneVertex* vptr = SceneVertices + vpos;
		uint32_t* iptr = SceneIndexes + ipos;

		uint32_t istart = ipos;
		uint32_t icount = 0;

		for (FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next)
		{
			auto pts = Poly->Pts;
			uint32_t vcount = Poly->NumPts;
			if (vcount < 3) continue;

			for (uint32_t i = 0; i < vcount; i++)
			{
				FVector point = pts[i]->Point;
				FLOAT u = Facet.MapCoords.XAxis | point;
				FLOAT v = Facet.MapCoords.YAxis | point;

				vptr->Flags = flags;
				vptr->Position.x = point.X;
				vptr->Position.y = point.Y;
				vptr->Position.z = point.Z;
				vptr->TexCoord.s = (u - UPan) * UMult;
				vptr->TexCoord.t = (v - VPan) * VMult;
				vptr->TexCoord2.s = (u - LMUPan) * LMUMult;
				vptr->TexCoord2.t = (v - LMVPan) * LMVMult;
				vptr->TexCoord3.s = (u - MacroUPan) * MacroUMult;
				vptr->TexCoord3.t = (v - MacroVPan) * MacroVMult;
				vptr->TexCoord4.s = (u - DetailUPan) * DetailUMult;
				vptr->TexCoord4.t = (v - DetailVPan) * DetailVMult;
				vptr->Color = color;
				vptr++;
			}

			for (uint32_t i = vpos + 2; i < vpos + vcount; i++)
			{
				*(iptr++) = vpos;
				*(iptr++) = i - 1;
				*(iptr++) = i;
			}

			vpos += vcount;
			icount += (vcount - 2) * 3;
		}

		SceneVertexPos = vpos;
		SceneIndexPos = ipos + icount;

		if (drawcount != 0)
		{
			SetPipeline(PF_Highlighted);
			SetDescriptorSet(PF_Highlighted);
			color = vec4(0.0f, 0.0f, 0.05f, 0.20f);
		}
	}

	unguard;
}

void UD3D11RenderDevice::DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, int NumPts, DWORD PolyFlags, FSpanBuffer* Span)
{
	guard(UD3D11RenderDevice::DrawGouraudPolygon);

	if (NumPts < 3) return; // This can apparently happen!!
	if (SceneVertexPos + NumPts > SceneVertexBufferSize || SceneIndexPos + NumPts * 3 > SceneIndexBufferSize) NextSceneBuffers();

	CachedTexture* tex = Textures->GetTexture(&Info, !!(PolyFlags & PF_Masked));

	SetPipeline(PolyFlags);
	SetDescriptorSet(PolyFlags, tex);

	if (!SceneVertices || !SceneIndexes) return;

	float UMult = GetUMult(Info);
	float VMult = GetVMult(Info);
	int flags = (PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated)) == PF_RenderFog ? 16 : 0;

	if ((PolyFlags & (PF_Translucent | PF_Modulated)) == 0 && LightMode == 2) flags |= 32;

	if (PolyFlags & PF_Modulated)
	{
		SceneVertex* vertex = &SceneVertices[SceneVertexPos];
		for (INT i = 0; i < NumPts; i++)
		{
			FTransTexture* P = Pts[i];
			vertex->Flags = flags;
			vertex->Position.x = P->Point.X;
			vertex->Position.y = P->Point.Y;
			vertex->Position.z = P->Point.Z;
			vertex->TexCoord.s = P->U * UMult;
			vertex->TexCoord.t = P->V * VMult;
			vertex->TexCoord2.s = P->Fog.X;
			vertex->TexCoord2.t = P->Fog.Y;
			vertex->TexCoord3.s = P->Fog.Z;
			vertex->TexCoord3.t = P->Fog.W;
			vertex->TexCoord4.s = 0.0f;
			vertex->TexCoord4.t = 0.0f;
			vertex->Color.r = 1.0f;
			vertex->Color.g = 1.0f;
			vertex->Color.b = 1.0f;
			vertex->Color.a = 1.0f;
			vertex++;
		}
	}
	else
	{
		SceneVertex* vertex = &SceneVertices[SceneVertexPos];
		for (INT i = 0; i < NumPts; i++)
		{
			FTransTexture* P = Pts[i];
			vertex->Flags = flags;
			vertex->Position.x = P->Point.X;
			vertex->Position.y = P->Point.Y;
			vertex->Position.z = P->Point.Z;
			vertex->TexCoord.s = P->U * UMult;
			vertex->TexCoord.t = P->V * VMult;
			vertex->TexCoord2.s = P->Fog.X;
			vertex->TexCoord2.t = P->Fog.Y;
			vertex->TexCoord3.s = P->Fog.Z;
			vertex->TexCoord3.t = P->Fog.W;
			vertex->TexCoord4.s = 0.0f;
			vertex->TexCoord4.t = 0.0f;
			vertex->Color.r = P->Light.X;
			vertex->Color.g = P->Light.Y;
			vertex->Color.b = P->Light.Z;
			vertex->Color.a = 1.0f;
			vertex++;
		}
	}

	size_t vstart = SceneVertexPos;
	size_t vcount = NumPts;
	size_t istart = SceneIndexPos;
	size_t icount = (vcount - 2) * 3;

	uint32_t* iptr = SceneIndexes + istart;
	for (uint32_t i = vstart + 2; i < vstart + vcount; i++)
	{
		*(iptr++) = vstart;
		*(iptr++) = i - 1;
		*(iptr++) = i;
	}

	SceneVertexPos += vcount;
	SceneIndexPos += icount;

	unguard;
}

#if defined(OLDUNREAL469SDK)

static void EnviroMap(const FSceneNode* Frame, FTransTexture& P, FLOAT UScale, FLOAT VScale)
{
	FVector T = P.Point.UnsafeNormal().MirrorByVector(P.Normal).TransformVectorBy(Frame->Uncoords);
	P.U = (T.X + 1.0f) * 0.5f * 256.0f * UScale;
	P.V = (T.Y + 1.0f) * 0.5f * 256.0f * VScale;
}

void UD3D11RenderDevice::DrawGouraudTriangles(const FSceneNode* Frame, const FTextureInfo& Info, FTransTexture* const Pts, INT NumPts, DWORD PolyFlags, DWORD DataFlags, FSpanBuffer* Span)
{
	guard(UD3D11RenderDevice::DrawGouraudTriangles);

	// URenderDeviceOldUnreal469::DrawGouraudTriangles(Frame, Info, Pts, NumPts, PolyFlags, DataFlags, Span); return;

	if (NumPts < 3) return; // This can apparently happen!!
	if (SceneVertexPos + NumPts > SceneVertexBufferSize || SceneIndexPos + NumPts * 3 > SceneIndexBufferSize) NextSceneBuffers();

	CachedTexture* tex = Textures->GetTexture(const_cast<FTextureInfo*>(&Info), !!(PolyFlags & PF_Masked));

	SetPipeline(PolyFlags);
	SetDescriptorSet(PolyFlags, tex);

	if (!SceneVertices || !SceneIndexes) return;

	float UMult = GetUMult(Info);
	float VMult = GetVMult(Info);
	int flags = (PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated)) == PF_RenderFog ? 16 : 0;

	if ((PolyFlags & (PF_Translucent | PF_Modulated)) == 0 && LightMode == 2) flags |= 32;

	if (PolyFlags & PF_Environment)
	{
		FLOAT UScale = Info.UScale * Info.USize * (1.0f / 256.0f);
		FLOAT VScale = Info.VScale * Info.VSize * (1.0f / 256.0f);

		for (INT i = 0; i < NumPts; i++)
			::EnviroMap(Frame, Pts[i], UScale, VScale);
	}

	if (PolyFlags & PF_Modulated)
	{
		SceneVertex* vertex = &SceneVertices[SceneVertexPos];
		for (INT i = 0; i < NumPts; i++)
		{
			FTransTexture* P = &Pts[i];
			vertex->Flags = flags;
			vertex->Position.x = P->Point.X;
			vertex->Position.y = P->Point.Y;
			vertex->Position.z = P->Point.Z;
			vertex->TexCoord.s = P->U * UMult;
			vertex->TexCoord.t = P->V * VMult;
			vertex->TexCoord2.s = P->Fog.X;
			vertex->TexCoord2.t = P->Fog.Y;
			vertex->TexCoord3.s = P->Fog.Z;
			vertex->TexCoord3.t = P->Fog.W;
			vertex->TexCoord4.s = 0.0f;
			vertex->TexCoord4.t = 0.0f;
			vertex->Color.r = 1.0f;
			vertex->Color.g = 1.0f;
			vertex->Color.b = 1.0f;
			vertex->Color.a = 1.0f;
			vertex++;
		}
	}
	else
	{
		SceneVertex* vertex = &SceneVertices[SceneVertexPos];
		for (INT i = 0; i < NumPts; i++)
		{
			FTransTexture* P = &Pts[i];
			vertex->Flags = flags;
			vertex->Position.x = P->Point.X;
			vertex->Position.y = P->Point.Y;
			vertex->Position.z = P->Point.Z;
			vertex->TexCoord.s = P->U * UMult;
			vertex->TexCoord.t = P->V * VMult;
			vertex->TexCoord2.s = P->Fog.X;
			vertex->TexCoord2.t = P->Fog.Y;
			vertex->TexCoord3.s = P->Fog.Z;
			vertex->TexCoord3.t = P->Fog.W;
			vertex->TexCoord4.s = 0.0f;
			vertex->TexCoord4.t = 0.0f;
			vertex->Color.r = P->Light.X;
			vertex->Color.g = P->Light.Y;
			vertex->Color.b = P->Light.Z;
			vertex->Color.a = 1.0f;
			vertex++;
		}
	}

	bool mirror = (Frame->Mirror == -1.0);

	size_t vstart = SceneVertexPos;
	size_t vcount = NumPts;
	size_t istart = SceneIndexPos;
	size_t icount = 0;

	uint32_t* iptr = SceneIndexes + istart;
	for (uint32_t i = 2; i < vcount; i += 3)
	{
		// If outcoded, skip it.
		if (Pts[icount].Flags & Pts[icount + 1].Flags & Pts[icount + 2].Flags)
			continue;

		bool backface = (PolyFlags & PF_TwoSided) && FTriple(Pts[icount].Point, Pts[icount + 1].Point, Pts[icount + 2].Point) <= 0.0;
		if (mirror) backface = !backface;
		if (!backface)
		{
			*(iptr++) = vstart + i - 2;
			*(iptr++) = vstart + i - 1;
			*(iptr++) = vstart + i;
			icount += 3;
		}
		else if (PolyFlags & PF_TwoSided)
		{
			*(iptr++) = vstart + i;
			*(iptr++) = vstart + i - 1;
			*(iptr++) = vstart + i - 2;
			icount += 3;
		}
	}

	SceneVertexPos += vcount;
	SceneIndexPos += icount;

	unguard;
}

#endif

void UD3D11RenderDevice::DrawTile(FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags)
{
	guard(UD3D11RenderDevice::DrawTile);

	if (SceneVertexPos + 4 > SceneVertexBufferSize || SceneIndexPos + 6 > SceneIndexBufferSize) NextSceneBuffers();

	// stijn: fix for invisible actor icons in ortho viewports
	if (GIsEditor && Frame->Viewport->Actor && (Frame->Viewport->IsOrtho() || Abs(Z) <= SMALL_NUMBER))
	{
		Z = 1.f;
	}

	if ((PolyFlags & (PF_Modulated)) == (PF_Modulated) && Info.Format == TEXF_P8)
		PolyFlags = PF_Modulated;

	CachedTexture* tex = Textures->GetTexture(&Info, !!(PolyFlags & PF_Masked));

	SetPipeline(PolyFlags);
	SetDescriptorSet(PolyFlags, tex, nullptr, nullptr, nullptr, true);

	if (!SceneVertices || !SceneIndexes) return;

	float UMult = tex ? GetUMult(Info) : 0.0f;
	float VMult = tex ? GetVMult(Info) : 0.0f;

	SceneVertex* v = &SceneVertices[SceneVertexPos];

	float r, g, b, a;
	if (PolyFlags & PF_Modulated)
	{
		r = 1.0f;
		g = 1.0f;
		b = 1.0f;
	}
	else
	{
		r = Color.X;
		g = Color.Y;
		b = Color.Z;
	}
	a = 1.0f;

	if (SceneBuffers.Multisample > 1)
	{
		XL = std::floor(X + XL + 0.5f);
		YL = std::floor(Y + YL + 0.5f);
		X = std::floor(X + 0.5f);
		Y = std::floor(Y + 0.5f);
		XL = XL - X;
		YL = YL - Y;
	}

	v[0] = { 0, vec3(RFX2 * Z * (X - Frame->FX2),      RFY2 * Z * (Y - Frame->FY2),      Z), vec2(U * UMult,        V * VMult),        vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(r, g, b, a), };
	v[1] = { 0, vec3(RFX2 * Z * (X + XL - Frame->FX2), RFY2 * Z * (Y - Frame->FY2),      Z), vec2((U + UL) * UMult, V * VMult),        vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(r, g, b, a), };
	v[2] = { 0, vec3(RFX2 * Z * (X + XL - Frame->FX2), RFY2 * Z * (Y + YL - Frame->FY2), Z), vec2((U + UL) * UMult, (V + VL) * VMult), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(r, g, b, a), };
	v[3] = { 0, vec3(RFX2 * Z * (X - Frame->FX2),      RFY2 * Z * (Y + YL - Frame->FY2), Z), vec2(U * UMult,        (V + VL) * VMult), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(r, g, b, a), };

	size_t vstart = SceneVertexPos;
	size_t vcount = 4;
	size_t istart = SceneIndexPos;
	size_t icount = (vcount - 2) * 3;

	uint32_t* iptr = SceneIndexes + istart;
	for (uint32_t i = vstart + 2; i < vstart + vcount; i++)
	{
		*(iptr++) = vstart;
		*(iptr++) = i - 1;
		*(iptr++) = i;
	}

	SceneVertexPos += vcount;
	SceneIndexPos += icount;

	unguard;
}

void UD3D11RenderDevice::Draw3DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2)
{
	guard(UD3D11RenderDevice::Draw3DLine);

	P1 = P1.TransformPointBy(Frame->Coords);
	P2 = P2.TransformPointBy(Frame->Coords);
	if (Frame->Viewport->IsOrtho())
	{
		P1.X = (P1.X) / Frame->Zoom + Frame->FX2;
		P1.Y = (P1.Y) / Frame->Zoom + Frame->FY2;
		P1.Z = 1;
		P2.X = (P2.X) / Frame->Zoom + Frame->FX2;
		P2.Y = (P2.Y) / Frame->Zoom + Frame->FY2;
		P2.Z = 1;

		if (Abs(P2.X - P1.X) + Abs(P2.Y - P1.Y) >= 0.2)
		{
			Draw2DLine(Frame, Color, LineFlags, P1, P2);
		}
		else if (Frame->Viewport->Actor->OrthoZoom < ORTHO_LOW_DETAIL)
		{
			Draw2DPoint(Frame, Color, LINE_None, P1.X - 1, P1.Y - 1, P1.X + 1, P1.Y + 1, P1.Z);
		}
	}
	else
	{
		auto pipeline = &ScenePass.LinePipeline[OccludeLines];
		if (pipeline != Batch.Pipeline)
		{
			AddDrawBatch();
			Batch.Pipeline = pipeline;
		}

		SetDescriptorSet(PF_Highlighted);

		if (SceneVertexPos + 2 > SceneVertexBufferSize || SceneIndexPos + 2 > SceneIndexBufferSize) NextSceneBuffers();
		if (!SceneVertices || !SceneIndexes) return;

		SceneVertex* v = &SceneVertices[SceneVertexPos];
		uint32_t* iptr = &SceneIndexes[SceneIndexPos];

		v[0] = { 0, vec3(P1.X, P1.Y, P1.Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec4(Color.X, Color.Y, Color.Z, 1.0f) };
		v[1] = { 0, vec3(P2.X, P2.Y, P2.Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec4(Color.X, Color.Y, Color.Z, 1.0f) };

		iptr[0] = SceneVertexPos;
		iptr[1] = SceneVertexPos + 1;

		SceneVertexPos += 2;
		SceneIndexPos += 2;
	}

	unguard;
}

void UD3D11RenderDevice::Draw2DClippedLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2)
{
	guard(UD3D11RenderDevice::Draw2DClippedLine);
	URenderDevice::Draw2DClippedLine(Frame, Color, LineFlags, P1, P2);
	unguard;
}

void UD3D11RenderDevice::Draw2DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2)
{
	guard(UD3D11RenderDevice::Draw2DLine);

	auto pipeline = &ScenePass.LinePipeline[OccludeLines];
	if (pipeline != Batch.Pipeline)
	{
		AddDrawBatch();
		Batch.Pipeline = pipeline;
	}

	SetDescriptorSet(PF_Highlighted);

	if (SceneVertexPos + 2 > SceneVertexBufferSize || SceneIndexPos + 2 > SceneIndexBufferSize) NextSceneBuffers();
	if (!SceneVertices || !SceneIndexes) return;

	SceneVertex* v = &SceneVertices[SceneVertexPos];
	uint32_t* iptr = &SceneIndexes[SceneIndexPos];

	v[0] = { 0, vec3(RFX2 * P1.Z * (P1.X - Frame->FX2), RFY2 * P1.Z * (P1.Y - Frame->FY2), P1.Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec4(Color.X, Color.Y, Color.Z, 1.0f) };
	v[1] = { 0, vec3(RFX2 * P2.Z * (P2.X - Frame->FX2), RFY2 * P2.Z * (P2.Y - Frame->FY2), P2.Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec4(Color.X, Color.Y, Color.Z, 1.0f) };

	iptr[0] = SceneVertexPos;
	iptr[1] = SceneVertexPos + 1;

	SceneVertexPos += 2;
	SceneIndexPos += 2;

	unguard;
}

void UD3D11RenderDevice::Draw2DPoint(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z)
{
	guard(UD3D11RenderDevice::Draw2DPoint);

	// Hack to fix UED selection problem with selection brush
	if (GIsEditor) Z = 1.0f;

	auto pipeline = &ScenePass.PointPipeline;
	if (pipeline != Batch.Pipeline)
	{
		AddDrawBatch();
		Batch.Pipeline = pipeline;
	}

	SetDescriptorSet(PF_Highlighted);

	if (SceneVertexPos + 2 > SceneVertexBufferSize || SceneIndexPos + 2 > SceneIndexBufferSize) NextSceneBuffers();
	if (!SceneVertices || !SceneIndexes) return;

	SceneVertex* v = &SceneVertices[SceneVertexPos];

	v[0] = { 0, vec3(RFX2 * Z * (X1 - Frame->FX2), RFY2 * Z * (Y1 - Frame->FY2), Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec4(Color.X, Color.Y, Color.Z, 1.0f) };
	v[1] = { 0, vec3(RFX2 * Z * (X2 - Frame->FX2), RFY2 * Z * (Y1 - Frame->FY2), Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec4(Color.X, Color.Y, Color.Z, 1.0f) };
	v[2] = { 0, vec3(RFX2 * Z * (X2 - Frame->FX2), RFY2 * Z * (Y2 - Frame->FY2), Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec4(Color.X, Color.Y, Color.Z, 1.0f) };
	v[3] = { 0, vec3(RFX2 * Z * (X1 - Frame->FX2), RFY2 * Z * (Y2 - Frame->FY2), Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec4(Color.X, Color.Y, Color.Z, 1.0f) };

	size_t vstart = SceneVertexPos;
	size_t vcount = 4;
	size_t istart = SceneIndexPos;
	size_t icount = (vcount - 2) * 3;

	uint32_t* iptr = SceneIndexes + istart;
	for (uint32_t i = vstart + 2; i < vstart + vcount; i++)
	{
		*(iptr++) = vstart;
		*(iptr++) = i - 1;
		*(iptr++) = i;
	}

	SceneVertexPos += vcount;
	SceneIndexPos += icount;

	unguard;
}

void UD3D11RenderDevice::ClearZ(FSceneNode* Frame)
{
	guard(UD3D11RenderDevice::ClearZ);

	DrawBatches();

	Context->ClearDepthStencilView(SceneBuffers.DepthBufferView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	unguard;
}

void UD3D11RenderDevice::GetStats(TCHAR* Result)
{
	guard(UD3D11RenderDevice::GetStats);
	Result[0] = 0;
	unguard;
}

void UD3D11RenderDevice::ReadPixels(FColor* Pixels)
{
	guard(UD3D11RenderDevice::GetStats);

	ID3D11Texture2D* stagingTexture = nullptr;

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Usage = D3D11_USAGE_STAGING;
	texDesc.BindFlags = 0;
	texDesc.Width = SceneBuffers.Width;
	texDesc.Height = SceneBuffers.Height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	HRESULT result = Device->CreateTexture2D(&texDesc, nullptr, &stagingTexture);
	if (FAILED(result))
		return;

	Context->CopyResource(stagingTexture, SceneBuffers.PPImage);

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	result = Context->Map(stagingTexture, 0, D3D11_MAP_READ, 0, &mapped);
	if (SUCCEEDED(result))
	{
		uint8_t* srcpixels = (uint8_t*)mapped.pData;
		int w = Viewport->SizeX;
		int h = Viewport->SizeY;
		void* data = Pixels;

		int i = 0;
		for (int y = 0; y < h; y++)
		{
			uint8_t* dest = (uint8_t*)data + (h - y - 1) * w * 4;
			uint16_t* src = (uint16_t*)(srcpixels + y * mapped.RowPitch);
			for (int x = 0; x < w; x++)
			{
				dest[2] = (int)clamp(std::round(halfToFloatSimple(*(src++)) * 255.0f), 0.0f, 255.0f);
				dest[1] = (int)clamp(std::round(halfToFloatSimple(*(src++)) * 255.0f), 0.0f, 255.0f);
				dest[0] = (int)clamp(std::round(halfToFloatSimple(*(src++)) * 255.0f), 0.0f, 255.0f);
				dest[3] = (int)clamp(std::round(halfToFloatSimple(*(src++)) * 255.0f), 0.0f, 255.0f);
				dest += 4;
			}
		}

		Context->Unmap(stagingTexture, 0);
	}

	stagingTexture->Release();

	unguard;
}

void UD3D11RenderDevice::EndFlash()
{
	guard(UD3D11RenderDevice::EndFlash);
	if (FlashScale != FPlane(0.5f, 0.5f, 0.5f, 0.0f) || FlashFog != FPlane(0.0f, 0.0f, 0.0f, 0.0f))
	{
		DrawBatches();

		SceneConstants.ObjectToProjection = mat4::identity();
		SceneConstants.NearClip = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		Context->UpdateSubresource(ScenePass.ConstantBuffer, 0, nullptr, &SceneConstants, 0, 0);

		Batch.Pipeline = &ScenePass.Pipelines[2];
		SetDescriptorSet(0);

		if (SceneVertices && SceneIndexes)
		{
			vec4 color(FlashFog.X, FlashFog.Y, FlashFog.Z, 1.0f - Min(FlashScale.X * 2.0f, 1.0f));
			vec2 zero2(0.0f);

			SceneVertex* v = &SceneVertices[SceneVertexPos];

			v[0] = { 0, vec3(-1.0f, -1.0f, 0.0f), zero2, zero2, zero2, zero2, color };
			v[1] = { 0, vec3(1.0f, -1.0f, 0.0f), zero2, zero2, zero2, zero2, color };
			v[2] = { 0, vec3(1.0f,  1.0f, 0.0f), zero2, zero2, zero2, zero2, color };
			v[3] = { 0, vec3(-1.0f,  1.0f, 0.0f), zero2, zero2, zero2, zero2, color };

			size_t vstart = SceneVertexPos;
			size_t vcount = 4;
			size_t istart = SceneIndexPos;
			size_t icount = (vcount - 2) * 3;

			uint32_t* iptr = SceneIndexes + istart;
			for (uint32_t i = vstart + 2; i < vstart + vcount; i++)
			{
				*(iptr++) = vstart;
				*(iptr++) = i - 1;
				*(iptr++) = i;
			}

			SceneVertexPos += vcount;
			SceneIndexPos += icount;

			AddDrawBatch();
		}

		if (CurrentFrame)
			SetSceneNode(CurrentFrame);
	}
	unguard;
}

void UD3D11RenderDevice::SetSceneNode(FSceneNode* Frame)
{
	guard(UD3D11RenderDevice::SetSceneNode);

	DrawBatches();

	CurrentFrame = Frame;
	Aspect = Frame->FY / Frame->FX;
	RProjZ = (float)appTan(radians(Viewport->Actor->FovAngle) * 0.5);
	RFX2 = 2.0f * RProjZ / Frame->FX;
	RFY2 = 2.0f * RProjZ * Aspect / Frame->FY;

	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = Frame->XB;
	viewport.TopLeftY = SceneBuffers.Height - Frame->YB - Frame->Y;
	viewport.Width = Frame->X;
	viewport.Height = Frame->Y;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	Context->RSSetViewports(1, &viewport);

	SceneConstants.ObjectToProjection = mat4::frustum(-RProjZ, RProjZ, -Aspect * RProjZ, Aspect * RProjZ, 1.0f, 32768.0f, handedness::left, clipzrange::zero_positive_w);
	SceneConstants.NearClip = vec4(Frame->NearClip.X, Frame->NearClip.Y, Frame->NearClip.Z, Frame->NearClip.W);

	Context->UpdateSubresource(ScenePass.ConstantBuffer, 0, nullptr, &SceneConstants, 0, 0);

	unguard;
}

void UD3D11RenderDevice::PrecacheTexture(FTextureInfo& Info, DWORD PolyFlags)
{
	guard(UD3D11RenderDevice::PrecacheTexture);
	Textures->GetTexture(&Info, !!(PolyFlags & PF_Masked));
	unguard;
}

void UD3D11RenderDevice::ClearTextureCache()
{
	Textures->ClearCache();
}

void UD3D11RenderDevice::AddDrawBatch()
{
	if (Batch.SceneIndexStart != SceneIndexPos)
	{
		Batch.SceneIndexEnd = SceneIndexPos;
		QueuedBatches.push_back(Batch);
		Batch.SceneIndexStart = SceneIndexPos;
	}
}

void UD3D11RenderDevice::DrawBatches(bool nextBuffer)
{
	AddDrawBatch();

	Context->Unmap(ScenePass.VertexBuffer, 0); SceneVertices = nullptr;
	Context->Unmap(ScenePass.IndexBuffer, 0); SceneIndexes = nullptr;

	for (const DrawBatchEntry& entry : QueuedBatches)
		DrawEntry(entry);
	QueuedBatches.clear();

	D3D11_MAPPED_SUBRESOURCE mappedVertexBuffer = {};
	HRESULT result = Context->Map(ScenePass.VertexBuffer, 0, nextBuffer ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mappedVertexBuffer);
	if (SUCCEEDED(result))
	{
		SceneVertices = (SceneVertex*)mappedVertexBuffer.pData;
	}

	D3D11_MAPPED_SUBRESOURCE mappedIndexBuffer = {};
	result = Context->Map(ScenePass.IndexBuffer, 0, nextBuffer ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mappedIndexBuffer);
	if (SUCCEEDED(result))
	{
		SceneIndexes = (uint32_t*)mappedIndexBuffer.pData;
	}

	if (nextBuffer)
	{
		SceneVertexPos = 0;
		SceneIndexPos = 0;
	}

	Batch.SceneIndexStart = SceneIndexPos;
}

void UD3D11RenderDevice::DrawEntry(const DrawBatchEntry& entry)
{
	size_t icount = entry.SceneIndexEnd - entry.SceneIndexStart;

	ID3D11ShaderResourceView* views[4] =
	{
		entry.Tex ? entry.Tex->View : Textures->GetNullTexture()->View,
		entry.Lightmap ? entry.Lightmap->View : Textures->GetNullTexture()->View,
		entry.Macrotex ? entry.Macrotex->View : Textures->GetNullTexture()->View,
		entry.Detailtex ? entry.Detailtex->View : Textures->GetNullTexture()->View
	};

	ID3D11SamplerState* samplers[4] =
	{
		ScenePass.Samplers[entry.TexSamplerMode],
		ScenePass.Samplers[0],
		ScenePass.Samplers[entry.MacrotexSamplerMode],
		ScenePass.Samplers[entry.DetailtexSamplerMode]
	};

	Context->PSSetSamplers(0, 4, samplers);
	Context->PSSetShaderResources(0, 4, views);
	Context->PSSetShader(entry.Pipeline->PixelShader, nullptr, 0);

	Context->OMSetBlendState(entry.Pipeline->BlendState, nullptr, 0xffffffff);
	Context->OMSetDepthStencilState(entry.Pipeline->DepthStencilState, 0);

	Context->IASetPrimitiveTopology(entry.Pipeline->PrimitiveTopology);

	Context->DrawIndexed(icount, entry.SceneIndexStart, 0);
}

std::vector<uint8_t> UD3D11RenderDevice::CompileHlsl(const std::string& filename, const std::string& shadertype, const std::vector<std::string> defines)
{
	std::string code = FileResource::readAllText(filename);

	std::string target;
	switch (FeatureLevel)
	{
	default:
	case D3D_FEATURE_LEVEL_11_1: target = shadertype + "_5_0"; break;
	case D3D_FEATURE_LEVEL_11_0: target = shadertype + "_5_0"; break;
	case D3D_FEATURE_LEVEL_10_1: target = shadertype + "_4_1"; break;
	case D3D_FEATURE_LEVEL_10_0: target = shadertype + "_4_0"; break;
	}

	std::vector<D3D_SHADER_MACRO> macros;
	for (const std::string& define : defines)
	{
		D3D_SHADER_MACRO macro = {};
		macro.Name = define.c_str();
		macro.Definition = "1";
		macros.push_back(macro);
	}
	macros.push_back({});

	ID3DBlob* blob = nullptr;
	ID3DBlob* errors = nullptr;
	HRESULT result = D3DCompile(code.data(), code.size(), filename.c_str(), macros.data(), nullptr, "main", target.c_str(), D3D10_SHADER_ENABLE_STRICTNESS | D3D10_SHADER_OPTIMIZATION_LEVEL3, 0, &blob, &errors);
	if (FAILED(result))
	{
		std::string msg((const char*)errors->GetBufferPointer(), errors->GetBufferSize());
		if (!msg.empty() && msg.back() == 0) msg.pop_back();
		ReleaseObject(errors);
		throw std::runtime_error(msg);
	}
	ReleaseObject(errors);
	ThrowIfFailed(result, "D3DCompile failed");

	std::vector<uint8_t> bytecode;
	bytecode.resize(blob->GetBufferSize());
	memcpy(bytecode.data(), blob->GetBufferPointer(), bytecode.size());
	ReleaseObject(blob);
	return bytecode;
}
