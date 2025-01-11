
#include "Precomp.h"
#include "UD3D11RenderDevice.h"
#include "CachedTexture.h"
#include "UTF16.h"
#include "FileResource.h"
#include "halffloat.h"

#ifdef USE_SSE2
// Unfortunately this code is slower than what the compiler generates on its own ;(
#undef USE_SSE2
#endif

IMPLEMENT_CLASS(UD3D11RenderDevice);

#include <initguid.h>
DEFINE_GUID(WKPDID_D3DDebugObjectName, 0x429b8c22, 0x9188, 0x4b0c, 0x87, 0x42, 0xac, 0xb0, 0xbf, 0x85, 0xc2, 0x00);

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
#if !defined(UNREALGOLD)
	DetailTextures = 1;
#endif
	HighDetailActors = 1;
	VolumetricLighting = 1;

#if defined(OLDUNREAL469SDK)
	UseLightmapAtlas = 0; // Note: do not turn this on. It does not work and generates broken fogmaps.
	SupportsUpdateTextureRect = 1;
	SupportsDrawTileList = 1;
	MaxTextureSize = 4096;
	NeedsMaskedFonts = 0;
	DescFlags |= RDDESCF_Certified;
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
	HdrScale = 128;
#if !defined(OLDUNREAL469SDK)
	OccludeLines = 0;
#endif
	Bloom = 0;
	BloomAmount = 128;

	LODBias = 0.0f;
	LightMode = 0;
	RefreshRate = 0;

	GammaCorrectScreenshots = 1;
	UseDebugLayer = 0;

#if defined(OLDUNREAL469SDK)
	new(GetClass(), TEXT("UseLightmapAtlas"), RF_Public) UBoolProperty(CPP_PROPERTY(UseLightmapAtlas), TEXT("Display"), CPF_Config);
#endif

	new(GetClass(), TEXT("UseVSync"), RF_Public) UBoolProperty(CPP_PROPERTY(UseVSync), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("UsePrecache"), RF_Public) UBoolProperty(CPP_PROPERTY(UsePrecache), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("GammaCorrectScreenshots"), RF_Public) UBoolProperty(CPP_PROPERTY(GammaCorrectScreenshots), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("UseDebugLayer"), RF_Public) UBoolProperty(CPP_PROPERTY(UseDebugLayer), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("GammaOffset"), RF_Public) UFloatProperty(CPP_PROPERTY(GammaOffset), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("GammaOffsetRed"), RF_Public) UFloatProperty(CPP_PROPERTY(GammaOffsetRed), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("GammaOffsetGreen"), RF_Public) UFloatProperty(CPP_PROPERTY(GammaOffsetGreen), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("GammaOffsetBlue"), RF_Public) UFloatProperty(CPP_PROPERTY(GammaOffsetBlue), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("LinearBrightness"), RF_Public) UByteProperty(CPP_PROPERTY(LinearBrightness), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("Contrast"), RF_Public) UByteProperty(CPP_PROPERTY(Contrast), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("Saturation"), RF_Public) UByteProperty(CPP_PROPERTY(Saturation), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("GrayFormula"), RF_Public) UIntProperty(CPP_PROPERTY(GrayFormula), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("Hdr"), RF_Public) UBoolProperty(CPP_PROPERTY(Hdr), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("HdrScale"), RF_Public) UByteProperty(CPP_PROPERTY(HdrScale), TEXT("Display"), CPF_Config);
#if !defined(OLDUNREAL469SDK)
	new(GetClass(), TEXT("OccludeLines"), RF_Public) UBoolProperty(CPP_PROPERTY(OccludeLines), TEXT("Display"), CPF_Config);
#endif
	new(GetClass(), TEXT("Bloom"), RF_Public) UBoolProperty(CPP_PROPERTY(Bloom), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("BloomAmount"), RF_Public) UByteProperty(CPP_PROPERTY(BloomAmount), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("LODBias"), RF_Public) UFloatProperty(CPP_PROPERTY(LODBias), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("RefreshRate"), RF_Public) UIntProperty(CPP_PROPERTY(RefreshRate), TEXT("Display"), CPF_Config);

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
	BufferCount = UseVSync ? 2 : 3;

	HDC screenDC = GetDC(0);
	DesktopResolution.Width = GetDeviceCaps(screenDC, HORZRES);
	DesktopResolution.Height = GetDeviceCaps(screenDC, VERTRES);
	ReleaseDC(0, screenDC);

	try
	{
		std::vector<D3D_FEATURE_LEVEL> featurelevels =
		{
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0
		};

		UINT deviceFlags = D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT;
		if (UseDebugLayer)
			deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;

		// First try use a more recent way of creating the device and swap chain
		HRESULT result = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			0,
			deviceFlags,
			featurelevels.data(), (UINT)featurelevels.size(),
			D3D11_SDK_VERSION,
			Device.TypedInitPtr(),
			&FeatureLevel,
			Context.TypedInitPtr());
		if (FAILED(result))
			debugf(TEXT("D3D11Drv: Could not create a modern D3D11 device"));

		// Wonderful API you got here, Microsoft. Good job.
		ComPtr<IDXGIDevice2> dxgiDevice;
		ComPtr<IDXGIAdapter> dxgiAdapter;
		ComPtr<IDXGIFactory2> dxgiFactory;

		if (SUCCEEDED(result))
			result = Device->QueryInterface(dxgiDevice.GetIID(), dxgiDevice.InitPtr());
		else
			debugf(TEXT("D3D11Drv: Could not get IDXGIDevice2 interface for the D3D11 device"));

		if (SUCCEEDED(result))
			result = dxgiDevice->GetParent(dxgiAdapter.GetIID(), dxgiAdapter.InitPtr());
		else
			debugf(TEXT("D3D11Drv: Could not get IDXGIAdapter interface for the D3D11 device"));

		if (SUCCEEDED(result))
			result = dxgiAdapter->GetParent(dxgiFactory.GetIID(), dxgiFactory.InitPtr());
		else
			debugf(TEXT("D3D11Drv: Could not get IDXGIFactory2 interface for the D3D11 device"));

		if (SUCCEEDED(result))
		{
			ComPtr<IDXGIFactory5> dxgiFactory5;
			result = dxgiFactory->QueryInterface(dxgiFactory5.GetIID(), dxgiFactory5.InitPtr());
			if (SUCCEEDED(result))
			{
				INT support = 0;
				result = dxgiFactory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &support, sizeof(INT));
				if (SUCCEEDED(result))
				{
					DxgiSwapChainAllowTearing = support != 0;
				}
				else
				{
					debugf(TEXT("D3D11Drv: Device does not support DXGI_FEATURE_PRESENT_ALLOW_TEARING"));
				}
			}
			else
			{
				debugf(TEXT("D3D11Drv: Could not get IDXGIFactory5 interface for the D3D11 device"));
			}

			UINT swapChainFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
			if (DxgiSwapChainAllowTearing)
				swapChainFlags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

			DXGI_SWAP_CHAIN_DESC1 swapDesc = {};
			swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapDesc.Width = NewX;
			swapDesc.Height = NewY;
			swapDesc.Format = ActiveHdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
			swapDesc.BufferCount = BufferCount;
			swapDesc.SampleDesc.Count = 1;
			swapDesc.Scaling = DXGI_SCALING_STRETCH;
			swapDesc.SwapEffect = GIsEditor ? DXGI_SWAP_EFFECT_DISCARD : DXGI_SWAP_EFFECT_FLIP_DISCARD;
			swapDesc.Flags = swapChainFlags;
			swapDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
			result = dxgiFactory->CreateSwapChainForHwnd(Device, (HWND)Viewport->GetWindow(), &swapDesc, nullptr, nullptr, SwapChain1.TypedInitPtr());
			if (SUCCEEDED(result))
			{
				dxgiFactory->MakeWindowAssociation((HWND)Viewport->GetWindow(), DXGI_MWA_NO_ALT_ENTER);
			}
			else
			{
				debugf(TEXT("D3D11Drv: CreateSwapChainForHwnd failed"));
				DxgiSwapChainAllowTearing = false;
			}
		}
		if (SUCCEEDED(result))
		{
			result = SwapChain1->QueryInterface(SwapChain.GetIID(), SwapChain.InitPtr());
			if (FAILED(result))
				SwapChain1.reset();
		}
		else
		{
			Context.reset();
			Device.reset();
		}
		dxgiFactory.reset();
		dxgiAdapter.reset();
		dxgiDevice.reset();

		// We still don't have a swap chain. Let's try the older Windows 7 API
		if (!SwapChain)
		{
			debugf(TEXT("D3D11Drv: Modern D3D11 device creation failed. Falling back to Windows 7"));

			DXGI_SWAP_CHAIN_DESC swapDesc = {};
			swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapDesc.BufferDesc.Width = NewX;
			swapDesc.BufferDesc.Height = NewY;
			swapDesc.BufferDesc.Format = ActiveHdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
			swapDesc.BufferCount = BufferCount;
			swapDesc.SampleDesc.Count = 1;
			swapDesc.OutputWindow = (HWND)Viewport->GetWindow();
			swapDesc.Windowed = TRUE;
			swapDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
			if (RefreshRate != 0)
			{
				swapDesc.BufferDesc.RefreshRate.Numerator = RefreshRate;
				swapDesc.BufferDesc.RefreshRate.Denominator = 1;
			}
			else
			{
				DEVMODE devmode = {};
				devmode.dmSize = sizeof(DEVMODE);
				if (EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devmode) && devmode.dmDisplayFrequency > 1)
				{
					swapDesc.BufferDesc.RefreshRate.Numerator = devmode.dmDisplayFrequency;
					swapDesc.BufferDesc.RefreshRate.Denominator = 1;
				}
			}

			// First try create a swap chain for Windows 8 and newer. If that fails, try the old for Windows 7
			HRESULT result = E_FAIL;
			for (DXGI_SWAP_EFFECT swapeffect : { GIsEditor ? DXGI_SWAP_EFFECT_DISCARD : DXGI_SWAP_EFFECT_FLIP_DISCARD, DXGI_SWAP_EFFECT_DISCARD })
			{
				swapDesc.SwapEffect = swapeffect;

				result = D3D11CreateDeviceAndSwapChain(
					nullptr,
					D3D_DRIVER_TYPE_HARDWARE,
					0,
					deviceFlags,
					featurelevels.data(), (UINT)featurelevels.size(),
					D3D11_SDK_VERSION,
					&swapDesc,
					SwapChain.TypedInitPtr(),
					Device.TypedInitPtr(),
					&FeatureLevel,
					Context.TypedInitPtr());
				if (SUCCEEDED(result))
					break;

				debugf(TEXT("D3D11Drv: Could not use DXGI_SWAP_EFFECT_FLIP_DISCARD. Falling back to DXGI_SWAP_EFFECT_DISCARD"));
			}
			ThrowIfFailed(result, "D3D11CreateDeviceAndSwapChain failed");
		}

		if (UseDebugLayer)
		{
			result = Device->QueryInterface(DebugLayer.GetIID(), DebugLayer.InitPtr());
			if (SUCCEEDED(result))
			{
				result = DebugLayer->QueryInterface(InfoQueue.GetIID(), InfoQueue.InitPtr());
				if (SUCCEEDED(result))
				{
					std::initializer_list<D3D11_MESSAGE_ID> denyList =
					{
						D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS
					};
					D3D11_INFO_QUEUE_FILTER filter = {};
					filter.DenyList.NumIDs = (UINT)denyList.size();
					filter.DenyList.pIDList = const_cast<D3D11_MESSAGE_ID*>(denyList.begin());
					result = InfoQueue->AddStorageFilterEntries(&filter);

					// result = InfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
					// result = InfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
				}
			}
			else
			{
				debugf(TEXT("D3D11Drv: Could not get ID3D11Debug interface"));
			}
		}

		SetDebugName(Device, "D3D11Drv.Device");
		SetDebugName(Context, "D3D11Drv.Context");

		SetColorSpace();

		CreateScenePass();
		CreatePresentPass();
		CreateBloomPass();

		Textures.reset(new TextureManager(this));
		Uploads.reset(new UploadManager(this));
	}
	catch (_com_error error)
	{
		debugf(TEXT("Could not create d3d11 renderer: [_com_error] %s"), error.ErrorMessage());
		Exit();
		return 0;
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

void UD3D11RenderDevice::PrintDebugLayerMessages()
{
	if (InfoQueue)
	{
		UINT64 count = InfoQueue->GetNumStoredMessages();
		for (UINT64 i = 0; i < count; i++)
		{
			SIZE_T msgLength = 0;
			HRESULT result = InfoQueue->GetMessage(i, nullptr, &msgLength);
			if (msgLength >= sizeof(D3D11_MESSAGE))
			{
				std::vector<uint8_t> buffer(msgLength);
				D3D11_MESSAGE* msg = (D3D11_MESSAGE*)buffer.data();
				result = InfoQueue->GetMessage(i, msg, &msgLength);
				if (SUCCEEDED(result))
				{
					std::string description = msg->pDescription;
					bool found = SeenDebugMessages.find(description) != SeenDebugMessages.end();
					if (!found)
					{
						if (TotalSeenDebugMessages < 20)
						{
							TotalSeenDebugMessages++;
							SeenDebugMessages.insert(description);

							const char* severitystr = "unknown";
							switch (msg->Severity)
							{
							case D3D11_MESSAGE_SEVERITY_CORRUPTION: severitystr = "corruption"; break;
							case D3D11_MESSAGE_SEVERITY_ERROR: severitystr = "error"; break;
							case D3D11_MESSAGE_SEVERITY_WARNING: severitystr = "warning"; break;
							case D3D11_MESSAGE_SEVERITY_INFO: severitystr = "info"; break;
							case D3D11_MESSAGE_SEVERITY_MESSAGE: severitystr = "message"; break;
							}
							debugf(TEXT("D3D11Drv: [%s] %s"), to_utf16(severitystr).c_str(), to_utf16(msg->pDescription).c_str());
						}
					}
				}
			}
		}
		InfoQueue->ClearStoredMessages();
	}
}

void UD3D11RenderDevice::SetColorSpace()
{
	if (ActiveHdr)
	{
		ComPtr<IDXGISwapChain3> swapChain3;
		HRESULT result = SwapChain->QueryInterface(swapChain3.GetIID(), swapChain3.InitPtr());
		if (SUCCEEDED(result))
		{
			UINT support = 0;
			result = swapChain3->CheckColorSpaceSupport(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020, &support);
			if (SUCCEEDED(result) && (support & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) == DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT)
			{
				result = swapChain3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
				if (FAILED(result))
				{
					debugf(TEXT("D3D11Drv: IDXGISwapChain3.SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020) failed"));
				}
			}
			else
			{
				debugf(TEXT("D3D11Drv: Swap chain does not support DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020"));
			}
		}
	}
}

class SetResCallLock
{
public:
	SetResCallLock(bool &value) : value(value)
	{
		value = true;
	}
	~SetResCallLock()
	{
		value = false;
	}
	bool& value;
};

UBOOL UD3D11RenderDevice::SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen)
{
	guard(UD3D11RenderDevice::SetRes);

	debugf(TEXT("D3D11Drv: SetRes(%d, %d, %d, %d) called"), NewX, NewY, NewColorBytes, Fullscreen);

	if (InSetResCall)
	{
		debugf(TEXT("D3D11Drv: Ignoring recursive SetRes(%d, %d, %d, %d) call"), NewX, NewY, NewColorBytes, Fullscreen);
		return TRUE;
	}
	SetResCallLock lock(InSetResCall);

	ReleaseSwapChainResources();

	// Resize the swap chain buffers before doing the mode switch. Shouldn't really make any difference but you never know!
	if (Fullscreen)
	{
		UINT flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		if (DxgiSwapChainAllowTearing)
			flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
		SwapChain->ResizeBuffers(UseVSync ? 2 : 3, NewX, NewY, ActiveHdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM, flags);
	}

	HRESULT result;

	DXGI_MODE_DESC modeDesc = {};
	modeDesc.Width = NewX;
	modeDesc.Height = NewY;
	modeDesc.Format = ActiveHdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
	if (RefreshRate != 0)
	{
		modeDesc.RefreshRate.Numerator = RefreshRate;
		modeDesc.RefreshRate.Denominator = 1;
	}
	else
	{
		DEVMODE devmode = {};
		devmode.dmSize = sizeof(DEVMODE);
		if (EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devmode) && devmode.dmDisplayFrequency > 1)
		{
			modeDesc.RefreshRate.Numerator = devmode.dmDisplayFrequency;
			modeDesc.RefreshRate.Denominator = 1;
		}
	}

	if (Fullscreen)
	{
		IDXGIOutput* output = nullptr;
		result = SwapChain->GetContainingOutput(&output);
		if (SUCCEEDED(result))
		{
			DXGI_MODE_DESC modeToMatch = modeDesc;
			DXGI_MODE_DESC modeFound = {};
			result = output->FindClosestMatchingMode(&modeToMatch, &modeFound, Device);
			if (SUCCEEDED(result))
			{
				if (modeToMatch.Width == modeFound.Width && modeToMatch.Height == modeFound.Height)
				{
					modeDesc = modeFound;
				}
				else
				{
					debugf(TEXT("FindClosestMatchingMode could not find a mode with the specified resolution (%d, %d, %d, %d)"), NewX, NewY, NewColorBytes, (INT)Fullscreen);
				}
			}
			else
			{
				debugf(TEXT("FindClosestMatchingMode failed (%d, %d, %d, %d)"), NewX, NewY, NewColorBytes, (INT)Fullscreen);
			}
			NewX = modeDesc.Width;
			NewY = modeDesc.Height;
			output->Release();
		}
		else
		{
			debugf(TEXT("GetContainingOutput failed (%d, %d, %d, %d)"), NewX, NewY, NewColorBytes, (INT)Fullscreen);
		}

		debugf(TEXT("D3D11Drv: Calling Viewport->ResizeViewport(BLIT_Fullscreen | BLIT_Direct3D, %d, %d, %d)"), NewX, NewY, NewColorBytes);

		// If we are going fullscreen we want to resize the window *prior* to entering fullscreen.
		if (!Viewport->ResizeViewport(BLIT_Fullscreen | BLIT_Direct3D, NewX, NewY, NewColorBytes))
		{
			debugf(TEXT("Viewport.ResizeViewport failed (%d, %d, %d, %d)"), NewX, NewY, NewColorBytes, (INT)Fullscreen);
			return FALSE;
		}

		result = SwapChain->SetFullscreenState(TRUE, nullptr);
		if (FAILED(result))
		{
			debugf(TEXT("SwapChain.SetFullscreenState failed (%d, %d, %d, %d)"), NewX, NewY, NewColorBytes, (INT)Fullscreen);
			// Don't fail this as it can happen if the application isn't the foreground process
		}

		result = SwapChain->ResizeTarget(&modeDesc);
		if (FAILED(result))
		{
			debugf(TEXT("SwapChain.ResizeTarget failed (%d, %d, %d, %d)"), NewX, NewY, NewColorBytes, (INT)Fullscreen);
		}
	}
	else
	{
		if (CurrentFullscreen)
		{
			result = SwapChain->SetFullscreenState(FALSE, nullptr);
			if (FAILED(result))
			{
				debugf(TEXT("SwapChain.SetFullscreenState failed (%d, %d, %d, %d)"), NewX, NewY, NewColorBytes, (INT)Fullscreen);
				// Don't fail this as it can happen if the application isn't the foreground process
			}

			result = SwapChain->ResizeTarget(&modeDesc);
			if (FAILED(result))
			{
				debugf(TEXT("SwapChain.ResizeTarget failed (%d, %d, %d, %d)"), NewX, NewY, NewColorBytes, (INT)Fullscreen);
			}
		}

		debugf(TEXT("D3D11Drv: Calling Viewport->ResizeViewport(BLIT_HardwarePaint | BLIT_Direct3D, %d, %d, %d)"), NewX, NewY, NewColorBytes);

		// If we exiting fullscreen, we want to resize/reposition the window *after* exiting fullscreen
		if (!Viewport->ResizeViewport(BLIT_HardwarePaint | BLIT_Direct3D, NewX, NewY, NewColorBytes))
		{
			debugf(TEXT("Viewport.ResizeViewport failed (%d, %d, %d, %d)"), NewX, NewY, NewColorBytes, (INT)Fullscreen);
			return FALSE;
		}
	}

	CurrentSizeX = NewX;
	CurrentSizeY = NewY;
	CurrentFullscreen = Fullscreen;

	BufferCount = UseVSync ? 2 : 3;
	if (!UpdateSwapChain())
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

void UD3D11RenderDevice::ReleaseSwapChainResources()
{
	BackBuffer.reset();
	BackBufferView.reset();
}

bool UD3D11RenderDevice::UpdateSwapChain()
{
	UINT flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	if (DxgiSwapChainAllowTearing)
		flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

	debugf(TEXT("D3D11Drv: Updating SwapChain size to %d x %d"), CurrentSizeX, CurrentSizeY);

	HRESULT result = SwapChain->ResizeBuffers(BufferCount, CurrentSizeX, CurrentSizeY, ActiveHdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM, flags);
	if (FAILED(result))
	{
		return false;
	}

	SetColorSpace();

	if (CurrentSizeX && CurrentSizeY)
	{
		try
		{
			debugf(TEXT("D3D11Drv: Resizing scene buffers to %d x %d"), CurrentSizeX, CurrentSizeY);

			ResizeSceneBuffers(CurrentSizeX, CurrentSizeY, GetSettingsMultisample());
		}
		catch (const std::exception& e)
		{
			debugf(TEXT("Could not resize scene buffers: %s"), to_utf16(e.what()).c_str());
			return false;
		}
	}

	result = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&BackBuffer);
	if (FAILED(result))
		return false;
	SetDebugName(BackBuffer, "BackBuffer");

	result = Device->CreateRenderTargetView(BackBuffer, nullptr, BackBufferView.TypedInitPtr());
	if (FAILED(result))
		return false;
	SetDebugName(BackBufferView, "BackBufferView");

	return true;
}

void UD3D11RenderDevice::Exit()
{
	guard(UD3D11RenderDevice::Exit);

	debugf(TEXT("D3D11Drv: exit called"));

	UnmapVertices();

	ReleaseSwapChainResources();
	if (CurrentFullscreen && SwapChain)
		SwapChain->SetFullscreenState(FALSE, nullptr);

	PrintDebugLayerMessages();

	if (Context)
		Context->ClearState();

	Uploads.reset();
	Textures.reset();
	ReleasePresentPass();
	ReleaseBloomPass();
	ReleaseScenePass();
	ReleaseSceneBuffers();
	BackBufferView.reset();
	BackBuffer.reset();
	SwapChain.reset();
	SwapChain1.reset();
	Context.reset();

	if (DebugLayer)
	{
		DebugLayer->ReportLiveDeviceObjects(/*D3D11_RLDO_SUMMARY |*/ D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
		PrintDebugLayerMessages();
	}

	InfoQueue.reset();
	DebugLayer.reset();

	if (Device)
	{
		Device->AddRef();
		int count = Device->Release();
		Device.reset();
		debugf(TEXT("D3D11Drv: D3D11Drv.Device refcount is now %d"), count - 1);
	}

	unguard;
}

void UD3D11RenderDevice::ResizeSceneBuffers(int width, int height, int multisample)
{
	multisample = std::max(multisample, 1);

	if (SceneBuffers.Width == width && SceneBuffers.Height == height && multisample == SceneBuffers.Multisample && SceneBuffers.ColorBuffer && SceneBuffers.HitBuffer && SceneBuffers.PPHitBuffer && SceneBuffers.StagingHitBuffer && SceneBuffers.DepthBuffer && SceneBuffers.PPImage[0] && SceneBuffers.PPImage[1])
		return;

	SceneBuffers.ColorBufferView.reset();
	SceneBuffers.HitBufferView.reset();
	SceneBuffers.HitBufferShaderView.reset();
	SceneBuffers.PPHitBufferView.reset();
	SceneBuffers.DepthBufferView.reset();
	for (int i = 0; i < 2; i++)
	{
		SceneBuffers.PPImageShaderView[i].reset();
		SceneBuffers.PPImageView[i].reset();
		SceneBuffers.PPImage[i].reset();
	}
	SceneBuffers.ColorBuffer.reset();
	SceneBuffers.StagingHitBuffer.reset();
	SceneBuffers.PPHitBuffer.reset();
	SceneBuffers.HitBuffer.reset();
	SceneBuffers.DepthBuffer.reset();

	for (PPBlurLevel& level : SceneBuffers.BlurLevels)
	{
		level.VTexture.reset();
		level.VTextureRTV.reset();
		level.VTextureSRV.reset();
		level.HTexture.reset();
		level.HTextureRTV.reset();
		level.HTextureSRV.reset();
	}

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
	HRESULT result = Device->CreateTexture2D(&texDesc, nullptr, SceneBuffers.ColorBuffer.TypedInitPtr());
	ThrowIfFailed(result, "CreateTexture2D(ColorBuffer) failed");
	SetDebugName(SceneBuffers.ColorBuffer, "SceneBuffers.ColorBuffer");

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
	result = Device->CreateTexture2D(&texDesc, nullptr, SceneBuffers.HitBuffer.TypedInitPtr());
	ThrowIfFailed(result, "CreateTexture2D(HitBuffer) failed");
	SetDebugName(SceneBuffers.HitBuffer, "SceneBuffers.HitBuffer");

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
	result = Device->CreateTexture2D(&texDesc, nullptr, SceneBuffers.PPHitBuffer.TypedInitPtr());
	ThrowIfFailed(result, "CreateTexture2D(PPHitBuffer) failed");
	SetDebugName(SceneBuffers.PPHitBuffer, "SceneBuffers.PPHitBuffer");

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
	result = Device->CreateTexture2D(&texDesc, nullptr, SceneBuffers.StagingHitBuffer.TypedInitPtr());
	ThrowIfFailed(result, "CreateTexture2D(StagingHitBuffer) failed");
	SetDebugName(SceneBuffers.StagingHitBuffer, "SceneBuffers.StagingHitBuffer");

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
	result = Device->CreateTexture2D(&texDesc, nullptr, SceneBuffers.DepthBuffer.TypedInitPtr());
	ThrowIfFailed(result, "CreateTexture2D(DepthBuffer) failed");
	SetDebugName(SceneBuffers.DepthBuffer, "SceneBuffers.DepthBuffer");

	for (int i = 0; i < 2; i++)
	{
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
		result = Device->CreateTexture2D(&texDesc, nullptr, SceneBuffers.PPImage[i].TypedInitPtr());
		ThrowIfFailed(result, "CreateTexture2D(PPImage) failed");
		SetDebugName(SceneBuffers.PPImage[i], "SceneBuffers.PPImage");
	}

	result = Device->CreateRenderTargetView(SceneBuffers.ColorBuffer, nullptr, SceneBuffers.ColorBufferView.TypedInitPtr());
	ThrowIfFailed(result, "CreateRenderTargetView(ColorBuffer) failed");
	SetDebugName(SceneBuffers.ColorBufferView, "SceneBuffers.ColorBufferView");

	result = Device->CreateRenderTargetView(SceneBuffers.HitBuffer, nullptr, SceneBuffers.HitBufferView.TypedInitPtr());
	ThrowIfFailed(result, "CreateRenderTargetView(HitBuffer) failed");
	SetDebugName(SceneBuffers.HitBufferView, "SceneBuffers.HitBufferView");

	result = Device->CreateShaderResourceView(SceneBuffers.HitBuffer, nullptr, SceneBuffers.HitBufferShaderView.TypedInitPtr());
	ThrowIfFailed(result, "CreateShaderResourceView(HitBuffer) failed");
	SetDebugName(SceneBuffers.HitBufferShaderView, "SceneBuffers.HitBufferShaderView");

	result = Device->CreateRenderTargetView(SceneBuffers.PPHitBuffer, nullptr, SceneBuffers.PPHitBufferView.TypedInitPtr());
	ThrowIfFailed(result, "CreateRenderTargetView(PPHitBuffer) failed");
	SetDebugName(SceneBuffers.PPHitBufferView, "SceneBuffers.PPHitBufferView");

	result = Device->CreateDepthStencilView(SceneBuffers.DepthBuffer, nullptr, SceneBuffers.DepthBufferView.TypedInitPtr());
	ThrowIfFailed(result, "CreateDepthStencilView(DepthBuffer) failed");
	SetDebugName(SceneBuffers.DepthBufferView, "SceneBuffers.DepthBufferView");

	for (int i = 0; i < 2; i++)
	{
		result = Device->CreateRenderTargetView(SceneBuffers.PPImage[i], nullptr, SceneBuffers.PPImageView[i].TypedInitPtr());
		ThrowIfFailed(result, "CreateRenderTargetView(PPImage) failed");
		SetDebugName(SceneBuffers.PPImageView[i], "SceneBuffers.PPImageView");

		result = Device->CreateShaderResourceView(SceneBuffers.PPImage[i], nullptr, SceneBuffers.PPImageShaderView[i].TypedInitPtr());
		ThrowIfFailed(result, "CreateShaderResourceView(PPImage) failed");
		SetDebugName(SceneBuffers.PPImageShaderView[i], "SceneBuffers.PPImageShaderView");
	}

	int bloomWidth = width;
	int bloomHeight = height;
	for (PPBlurLevel& level : SceneBuffers.BlurLevels)
	{
		bloomWidth = (bloomWidth + 1) / 2;
		bloomHeight = (bloomHeight + 1) / 2;

		texDesc = {};
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		texDesc.Width = bloomWidth;
		texDesc.Height = bloomHeight;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;

		result = Device->CreateTexture2D(&texDesc, nullptr, level.VTexture.TypedInitPtr());
		ThrowIfFailed(result, "CreateTexture2D(SceneBuffers.BlurLevels.VTexture) failed");
		SetDebugName(level.VTexture, "SceneBuffers.BlurLevels.VTexture");

		result = Device->CreateTexture2D(&texDesc, nullptr, level.HTexture.TypedInitPtr());
		ThrowIfFailed(result, "CreateTexture2D(SceneBuffers.BlurLevels.HTexture) failed");
		SetDebugName(level.HTexture, "SceneBuffers.BlurLevels.HTexture");

		result = Device->CreateRenderTargetView(level.VTexture, nullptr, level.VTextureRTV.TypedInitPtr());
		ThrowIfFailed(result, "CreateRenderTargetView(SceneBuffers.BlurLevels.VTextureRTV) failed");
		SetDebugName(level.VTextureRTV, "SceneBuffers.BlurLevels.VTextureRTV");

		result = Device->CreateRenderTargetView(level.HTexture, nullptr, level.HTextureRTV.TypedInitPtr());
		ThrowIfFailed(result, "CreateRenderTargetView(SceneBuffers.BlurLevels.HTextureRTV) failed");
		SetDebugName(level.HTextureRTV, "SceneBuffers.BlurLevels.HTextureRTV");

		result = Device->CreateShaderResourceView(level.VTexture, nullptr, level.VTextureSRV.TypedInitPtr());
		ThrowIfFailed(result, "CreateRenderTargetView(SceneBuffers.BlurLevels.VTextureSRV) failed");
		SetDebugName(level.VTextureSRV, "SceneBuffers.BlurLevels.VTextureSRV");

		result = Device->CreateShaderResourceView(level.HTexture, nullptr, level.HTextureSRV.TypedInitPtr());
		ThrowIfFailed(result, "CreateRenderTargetView(SceneBuffers.BlurLevels.HTextureSRV) failed");
		SetDebugName(level.HTextureSRV, "SceneBuffers.BlurLevels.HTextureSRV");

		level.Width = bloomWidth;
		level.Height = bloomHeight;
	}
}

void UD3D11RenderDevice::CreateScenePass()
{
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

	CreateVertexShader(ScenePass.VertexShader, "ScenePass.VertexShader", ScenePass.InputLayout, "ScenePass.InputLayout", elements, "shaders/Scene.vert");
	CreatePixelShader(ScenePass.PixelShader, "ScenePass.PixelShader", "shaders/Scene.frag");
	CreatePixelShader(ScenePass.PixelShaderAlphaTest, "ScenePass.PixelShaderAlphaTest", "shaders/Scene.frag", { "ALPHATEST" });

	CreateSceneSamplers();

	for (int i = 0; i < 2; i++)
	{
		D3D11_RASTERIZER_DESC rasterizerDesc = {};
		rasterizerDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerDesc.CullMode = D3D11_CULL_NONE;
		rasterizerDesc.FrontCounterClockwise = FALSE;
		rasterizerDesc.DepthClipEnable = FALSE; // Avoid clipping the weapon. The UE1 engine clips the geometry anyway.
		rasterizerDesc.MultisampleEnable = i == 1 ? TRUE : FALSE;
		HRESULT result = Device->CreateRasterizerState(&rasterizerDesc, ScenePass.RasterizerState[i].TypedInitPtr());
		ThrowIfFailed(result, "CreateRasterizerState(ScenePass.Pipelines.RasterizerState) failed");
		SetDebugName(ScenePass.RasterizerState[i], "ScenePass.RasterizerState");
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
		HRESULT result = Device->CreateBlendState(&blendDesc, ScenePass.Pipelines[i].BlendState.TypedInitPtr());
		ThrowIfFailed(result, "CreateBlendState(ScenePass.Pipelines.BlendState) failed");
		SetDebugName(ScenePass.Pipelines[i].BlendState, "ScenePass.Pipelines.BlendState");

		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = TRUE;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		if (i & 8) // PF_Occlude
			depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		else
			depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		result = Device->CreateDepthStencilState(&depthStencilDesc, ScenePass.Pipelines[i].DepthStencilState.TypedInitPtr());
		ThrowIfFailed(result, "CreateDepthStencilState(ScenePass.Pipelines.DepthStencilState) failed");
		SetDebugName(ScenePass.Pipelines[i].DepthStencilState, "ScenePass.Pipelines.DepthStencilState");

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
		HRESULT result = Device->CreateBlendState(&blendDesc, ScenePass.LinePipeline[i].BlendState.TypedInitPtr());
		ThrowIfFailed(result, "CreateBlendState(ScenePass.LinePipeline.BlendState) failed");
		SetDebugName(ScenePass.LinePipeline[i].BlendState, "ScenePass.LinePipeline.BlendState");

		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = TRUE;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		result = Device->CreateDepthStencilState(&depthStencilDesc, ScenePass.LinePipeline[i].DepthStencilState.TypedInitPtr());
		ThrowIfFailed(result, "CreateDepthStencilState(ScenePass.LinePipeline.DepthStencilState) failed");
		SetDebugName(ScenePass.LinePipeline[i].DepthStencilState, "ScenePass.LinePipeline.DepthStencilState");

		ScenePass.LinePipeline[i].PixelShader = ScenePass.PixelShader;
		ScenePass.LinePipeline[i].PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;

		if (i == 0)
		{
			ScenePass.LinePipeline[i].MinDepth = 0.0f;
			ScenePass.LinePipeline[i].MaxDepth = 0.1f;
		}
	}

	// Point pipeline
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
		HRESULT result = Device->CreateBlendState(&blendDesc, ScenePass.PointPipeline[i].BlendState.TypedInitPtr());
		ThrowIfFailed(result, "CreateBlendState(ScenePass.LinePipeline.BlendState) failed");
		SetDebugName(ScenePass.PointPipeline[i].BlendState, "ScenePass.PointPipeline.BlendState");

		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = TRUE;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		result = Device->CreateDepthStencilState(&depthStencilDesc, ScenePass.PointPipeline[i].DepthStencilState.TypedInitPtr());
		ThrowIfFailed(result, "CreateDepthStencilState(ScenePass.PointPipeline.DepthStencilState) failed");
		SetDebugName(ScenePass.PointPipeline[i].DepthStencilState, "ScenePass.PointPipeline.DepthStencilState");

		ScenePass.PointPipeline[i].PixelShader = ScenePass.PixelShader;
		ScenePass.PointPipeline[i].PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		if (i == 0)
		{
			ScenePass.PointPipeline[i].MinDepth = 0.0f;
			ScenePass.PointPipeline[i].MaxDepth = 0.1f;
		}
	}

	D3D11_BUFFER_DESC bufDesc = {};
	bufDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufDesc.ByteWidth = SceneVertexBufferSize * sizeof(SceneVertex);
	bufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	HRESULT result = Device->CreateBuffer(&bufDesc, nullptr, ScenePass.VertexBuffer.TypedInitPtr());
	ThrowIfFailed(result, "CreateBuffer(ScenePass.VertexBuffer) failed");
	SetDebugName(ScenePass.VertexBuffer, "ScenePass.VertexBuffer");

	bufDesc = {};
	bufDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufDesc.ByteWidth = SceneIndexBufferSize * sizeof(uint32_t);
	bufDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	result = Device->CreateBuffer(&bufDesc, nullptr, ScenePass.IndexBuffer.TypedInitPtr());
	ThrowIfFailed(result, "CreateBuffer(ScenePass.IndexBuffer) failed");
	SetDebugName(ScenePass.IndexBuffer, "ScenePass.IndexBuffer");

	bufDesc = {};
	bufDesc.Usage = D3D11_USAGE_DEFAULT;
	bufDesc.ByteWidth = sizeof(ScenePushConstants);
	bufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	result = Device->CreateBuffer(&bufDesc, nullptr, ScenePass.ConstantBuffer.TypedInitPtr());
	ThrowIfFailed(result, "CreateBuffer(ScenePass.ConstantBuffer) failed");
	SetDebugName(ScenePass.ConstantBuffer, "ScenePass.ConstantBuffer");
}

void UD3D11RenderDevice::CreateSceneSamplers()
{
	for (int i = 0; i < 16; i++)
	{
		int dummyMipmapCount = (i >> 2) & 3;
		D3D11_FILTER filter = (i & 1) ? D3D11_FILTER_MIN_MAG_MIP_POINT : D3D11_FILTER_ANISOTROPIC;
		D3D11_TEXTURE_ADDRESS_MODE addressmode = (i & 2) ? D3D11_TEXTURE_ADDRESS_MIRROR_ONCE : D3D11_TEXTURE_ADDRESS_WRAP;
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
		HRESULT result = Device->CreateSamplerState(&samplerDesc, ScenePass.Samplers[i].TypedInitPtr());
		ThrowIfFailed(result, "CreateSamplerState(ScenePass.Samplers) failed");
		SetDebugName(ScenePass.Samplers[i], "ScenePass.Samplers");
	}

	ScenePass.LODBias = LODBias;
}

void UD3D11RenderDevice::ReleaseSceneSamplers()
{
	for (auto& sampler : ScenePass.Samplers)
	{
		sampler.reset();
	}
	ScenePass.LODBias = 0.0f;
}

void UD3D11RenderDevice::UpdateLODBias()
{
	if (ScenePass.LODBias != LODBias)
	{
		ReleaseSceneSamplers();
		CreateSceneSamplers();
	}
}

void UD3D11RenderDevice::ReleaseScenePass()
{
	ScenePass.VertexShader.reset();
	ScenePass.InputLayout.reset();
	ScenePass.VertexBuffer.reset();
	ScenePass.IndexBuffer.reset();
	ScenePass.ConstantBuffer.reset();
	ScenePass.RasterizerState[0].reset();
	ScenePass.RasterizerState[1].reset();
	ScenePass.PixelShader.reset();
	ScenePass.PixelShaderAlphaTest.reset();
	ReleaseSceneSamplers();
	for (auto& pipeline : ScenePass.Pipelines)
	{
		pipeline.BlendState.reset();
		pipeline.DepthStencilState.reset();
	}
	for (int i = 0; i < 2; i++)
	{
		ScenePass.LinePipeline[i].BlendState.reset();
		ScenePass.LinePipeline[i].DepthStencilState.reset();
		ScenePass.PointPipeline[i].BlendState.reset();
		ScenePass.PointPipeline[i].DepthStencilState.reset();
	}
}

void UD3D11RenderDevice::ReleaseBloomPass()
{
	BloomPass.Extract.reset();
	BloomPass.Combine.reset();
	BloomPass.BlurVertical.reset();
	BloomPass.BlurHorizontal.reset();
	BloomPass.ConstantBuffer.reset();
	BloomPass.AdditiveBlendState.reset();
}

void UD3D11RenderDevice::ReleasePresentPass()
{
	PresentPass.PPStepLayout.reset();
	PresentPass.PPStep.reset();
	PresentPass.PPStepVertexBuffer.reset();
	PresentPass.HitResolve.reset();
	for (auto& shader : PresentPass.Present) shader.reset();
	PresentPass.PresentConstantBuffer.reset();
	PresentPass.DitherTextureView.reset();
	PresentPass.DitherTexture.reset();
	PresentPass.BlendState.reset();
	PresentPass.DepthStencilState.reset();
	PresentPass.RasterizerState.reset();
}

void UD3D11RenderDevice::ReleaseSceneBuffers()
{
	SceneBuffers.ColorBufferView.reset();
	SceneBuffers.HitBufferView.reset();
	SceneBuffers.HitBufferShaderView.reset();
	SceneBuffers.PPHitBufferView.reset();
	SceneBuffers.DepthBufferView.reset();
	for (int i = 0; i < 2; i++)
	{
		SceneBuffers.PPImageShaderView[i].reset();
		SceneBuffers.PPImageView[i].reset();
		SceneBuffers.PPImage[i].reset();
	}
	SceneBuffers.ColorBuffer.reset();
	SceneBuffers.StagingHitBuffer.reset();
	SceneBuffers.PPHitBuffer.reset();
	SceneBuffers.HitBuffer.reset();
	SceneBuffers.DepthBuffer.reset();
	for (PPBlurLevel& level : SceneBuffers.BlurLevels)
	{
		level.VTexture.reset();
		level.VTextureRTV.reset();
		level.VTextureSRV.reset();
		level.HTexture.reset();
		level.HTextureRTV.reset();
		level.HTextureSRV.reset();
	}
}

UD3D11RenderDevice::ScenePipelineState* UD3D11RenderDevice::GetPipeline(DWORD PolyFlags)
{
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

void UD3D11RenderDevice::RunBloomPass()
{
	ID3D11RenderTargetView* rtvs[1] = {};
	ID3D11ShaderResourceView* srvs[1] = {};

	float blurAmount = 0.6f + BloomAmount * (1.9f / 255.0f);
	BloomPushConstants pushconstants;
	ComputeBlurSamples(7, blurAmount, pushconstants.SampleWeights);

	ID3D11Buffer* vertexBuffers[1] = { PresentPass.PPStepVertexBuffer.get() };
	ID3D11Buffer* cbs[1] = { BloomPass.ConstantBuffer.get() };
	UINT stride = sizeof(vec2);
	UINT offset = 0;
	Context->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
	Context->IASetInputLayout(PresentPass.PPStepLayout);
	Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Context->VSSetShader(PresentPass.PPStep, nullptr, 0);
	Context->RSSetState(PresentPass.RasterizerState);
	Context->PSSetConstantBuffers(0, 1, cbs);
	Context->OMSetDepthStencilState(PresentPass.DepthStencilState, 0);
	Context->OMSetBlendState(PresentPass.BlendState, nullptr, 0xffffffff);
	Context->UpdateSubresource(BloomPass.ConstantBuffer, 0, nullptr, &pushconstants, 0, 0);

	D3D11_VIEWPORT viewport = {};
	viewport.MaxDepth = 1.0f;

	// Extract overbright pixels that we want to bloom:
	viewport.Width = SceneBuffers.BlurLevels[0].Width;
	viewport.Height = SceneBuffers.BlurLevels[0].Height;
	rtvs[0] = SceneBuffers.BlurLevels[0].VTextureRTV.get();
	srvs[0] = SceneBuffers.PPImageShaderView[0].get();
	Context->OMSetRenderTargets(1, rtvs, nullptr);
	Context->RSSetViewports(1, &viewport);
	Context->PSSetShader(BloomPass.Extract, nullptr, 0);
	Context->PSSetShaderResources(0, 1, srvs);
	Context->Draw(6, 0);

	// Blur and downscale:
	for (int i = 0; i < SceneBuffers.NumBloomLevels - 1; i++)
	{
		auto& blevel = SceneBuffers.BlurLevels[i];
		auto& next = SceneBuffers.BlurLevels[i + 1];

		viewport.Width = blevel.Width;
		viewport.Height = blevel.Height;
		Context->RSSetViewports(1, &viewport);
		BlurStep(blevel.VTextureSRV, blevel.HTextureRTV, false);
		BlurStep(blevel.HTextureSRV, blevel.VTextureRTV, true);

		// Linear downscale:
		viewport.Width = next.Width;
		viewport.Height = next.Height;
		rtvs[0] = next.VTextureRTV.get();
		srvs[0] = blevel.VTextureSRV.get();
		Context->OMSetRenderTargets(1, rtvs, nullptr);
		Context->RSSetViewports(1, &viewport);
		Context->PSSetShader(BloomPass.Combine, nullptr, 0);
		Context->PSSetShaderResources(0, 1, srvs);
		Context->Draw(6, 0);
	}

	// Blur and upscale:
	for (int i = SceneBuffers.NumBloomLevels - 1; i > 0; i--)
	{
		auto& blevel = SceneBuffers.BlurLevels[i];
		auto& next = SceneBuffers.BlurLevels[i - 1];

		viewport.Width = blevel.Width;
		viewport.Height = blevel.Height;
		Context->RSSetViewports(1, &viewport);
		BlurStep(blevel.VTextureSRV, blevel.HTextureRTV, false);
		BlurStep(blevel.HTextureSRV, blevel.VTextureRTV, true);

		// Linear upscale:
		viewport.Width = next.Width;
		viewport.Height = next.Height;
		rtvs[0] = next.VTextureRTV.get();
		srvs[0] = blevel.VTextureSRV.get();
		Context->OMSetRenderTargets(1, rtvs, nullptr);
		Context->RSSetViewports(1, &viewport);
		Context->PSSetShader(BloomPass.Combine, nullptr, 0);
		Context->PSSetShaderResources(0, 1, srvs);
		Context->Draw(6, 0);
	}

	viewport.Width = SceneBuffers.BlurLevels[0].Width;
	viewport.Height = SceneBuffers.BlurLevels[0].Height;
	Context->RSSetViewports(1, &viewport);
	BlurStep(SceneBuffers.BlurLevels[0].VTextureSRV, SceneBuffers.BlurLevels[0].HTextureRTV, false);
	BlurStep(SceneBuffers.BlurLevels[0].HTextureSRV, SceneBuffers.BlurLevels[0].VTextureRTV, true);

	// Add bloom back to scene post process texture:
	viewport.Width = SceneBuffers.Width;
	viewport.Height = SceneBuffers.Height;
	rtvs[0] = SceneBuffers.PPImageView[0].get();
	srvs[0] = SceneBuffers.BlurLevels[0].VTextureSRV.get();
	Context->OMSetRenderTargets(1, rtvs, nullptr);
	Context->OMSetBlendState(BloomPass.AdditiveBlendState, nullptr, 0xffffffff);
	Context->RSSetViewports(1, &viewport);
	Context->PSSetShader(BloomPass.Combine, nullptr, 0);
	Context->PSSetShaderResources(0, 1, srvs);
	Context->Draw(6, 0);
}

void UD3D11RenderDevice::BlurStep(ID3D11ShaderResourceView* input, ID3D11RenderTargetView* output, bool vertical)
{
	Context->OMSetRenderTargets(1, &output, nullptr);
	Context->PSSetShader(vertical ? BloomPass.BlurVertical : BloomPass.BlurHorizontal, nullptr, 0);
	Context->PSSetShaderResources(0, 1, &input);
	Context->Draw(6, 0);
}

float UD3D11RenderDevice::ComputeBlurGaussian(float n, float theta) // theta = Blur Amount
{
	return (float)((1.0f / std::sqrtf(2 * 3.14159265359f * theta)) * std::expf(-(n * n) / (2.0f * theta * theta)));
}

void UD3D11RenderDevice::ComputeBlurSamples(int sampleCount, float blurAmount, float* sampleWeights)
{
	sampleWeights[0] = ComputeBlurGaussian(0, blurAmount);

	float totalWeights = sampleWeights[0];

	for (int i = 0; i < sampleCount / 2; i++)
	{
		float weight = ComputeBlurGaussian(i + 1.0f, blurAmount);

		sampleWeights[i * 2 + 1] = weight;
		sampleWeights[i * 2 + 2] = weight;

		totalWeights += weight * 2;
	}

	for (int i = 0; i < sampleCount; i++)
	{
		sampleWeights[i] /= totalWeights;
	}
}

void UD3D11RenderDevice::CreateBloomPass()
{
	CreatePixelShader(BloomPass.Extract, "BloomPass.Extract", "shaders/BloomExtract.frag");
	CreatePixelShader(BloomPass.Combine, "BloomPass.Combine", "shaders/BloomCombine.frag");
	CreatePixelShader(BloomPass.BlurVertical, "BloomPass.BlurVertical", "shaders/Blur.frag", { "BLUR_VERTICAL" });
	CreatePixelShader(BloomPass.BlurHorizontal, "BloomPass.BlurHorizontal", "shaders/Blur.frag", { "BLUR_HORIZONTAL" });

	D3D11_BUFFER_DESC bufDesc = {};
	bufDesc.Usage = D3D11_USAGE_DEFAULT;
	bufDesc.ByteWidth = sizeof(BloomPushConstants);
	bufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	HRESULT result = Device->CreateBuffer(&bufDesc, nullptr, BloomPass.ConstantBuffer.TypedInitPtr());
	ThrowIfFailed(result, "CreateBuffer(BloomPass.ConstantBuffer) failed");
	SetDebugName(BloomPass.ConstantBuffer, "BloomPass.ConstantBuffer");

	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	result = Device->CreateBlendState(&blendDesc, BloomPass.AdditiveBlendState.TypedInitPtr());
	ThrowIfFailed(result, "CreateBlendState(BloomPass.AdditiveBlendState) failed");
	SetDebugName(BloomPass.AdditiveBlendState, "BloomPass.AdditiveBlendState");
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

	HRESULT result = Device->CreateBuffer(&bufDesc, &initData, PresentPass.PPStepVertexBuffer.TypedInitPtr());
	ThrowIfFailed(result, "CreateBuffer(PresentPass.PPStepVertexBuffer) failed");
	SetDebugName(PresentPass.PPStepVertexBuffer, "PresentPass.PPStepVertexBuffer");

	bufDesc = {};
	bufDesc.Usage = D3D11_USAGE_DEFAULT;
	bufDesc.ByteWidth = sizeof(PresentPushConstants);
	bufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	result = Device->CreateBuffer(&bufDesc, nullptr, PresentPass.PresentConstantBuffer.TypedInitPtr());
	ThrowIfFailed(result, "CreateBuffer(PresentPass.PresentConstantBuffer) failed");
	SetDebugName(PresentPass.PresentConstantBuffer, "PresentPass.PresentConstantBuffer");

	std::vector<D3D11_INPUT_ELEMENT_DESC> elements =
	{
		{ "AttrPos", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	CreateVertexShader(PresentPass.PPStep, "PresentPass.PPStep", PresentPass.PPStepLayout, "PresentPass.PPStepLayout", elements, "shaders/PPStep.vert");

	static const char* transferFunctions[2] = { nullptr, "HDR_MODE" };
	static const char* gammaModes[2] = { "GAMMA_MODE_D3D9", "GAMMA_MODE_XOPENGL" };
	static const char* colorModes[4] = { nullptr, "COLOR_CORRECT_MODE0", "COLOR_CORRECT_MODE1", "COLOR_CORRECT_MODE2" };
	for (int i = 0; i < 16; i++)
	{
		std::vector<std::string> defines;
		if (transferFunctions[i & 1]) defines.push_back(transferFunctions[i & 1]);
		if (gammaModes[(i >> 1) & 1]) defines.push_back(gammaModes[(i >> 1) & 1]);
		if (colorModes[(i >> 2) & 3]) defines.push_back(colorModes[(i >> 2) & 3]);

		CreatePixelShader(PresentPass.Present[i], "PresentPass.Present", "shaders/Present.frag", defines);
	}

	CreatePixelShader(PresentPass.HitResolve, "PresentPass.HitResolve", "shaders/HitResolve.frag");

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
	result = Device->CreateTexture2D(&texDesc, &initData, PresentPass.DitherTexture.TypedInitPtr());
	ThrowIfFailed(result, "CreateTexture2D(DitherTexture) failed");
	SetDebugName(PresentPass.DitherTexture, "PresentPass.DitherTexture");

	result = Device->CreateShaderResourceView(PresentPass.DitherTexture, nullptr, PresentPass.DitherTextureView.TypedInitPtr());
	ThrowIfFailed(result, "CreateShaderResourceView(DitherTexture) failed");
	SetDebugName(PresentPass.DitherTextureView, "PresentPass.DitherTextureView");

	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	result = Device->CreateBlendState(&blendDesc, PresentPass.BlendState.TypedInitPtr());
	ThrowIfFailed(result, "CreateBlendState(PresentPass.BlendState) failed");
	SetDebugName(PresentPass.BlendState, "PresentPass.BlendState");

	D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	result = Device->CreateDepthStencilState(&depthStencilDesc, PresentPass.DepthStencilState.TypedInitPtr());
	ThrowIfFailed(result, "CreateDepthStencilState(PresentPass.DepthStencilState) failed");
	SetDebugName(PresentPass.DepthStencilState, "PresentPass.DepthStencilState");

	D3D11_RASTERIZER_DESC rasterizerDesc = {};
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	result = Device->CreateRasterizerState(&rasterizerDesc, PresentPass.RasterizerState.TypedInitPtr());
	ThrowIfFailed(result, "CreateRasterizerState(PresentPass.RasterizerState) failed");
	SetDebugName(PresentPass.RasterizerState, "PresentPass.RasterizerState");
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
		resolutions.insert({ DesktopResolution.Width, DesktopResolution.Height });

		IDXGIOutput* output = nullptr;
		HRESULT result = SwapChain->GetContainingOutput(&output);
		if (SUCCEEDED(result))
		{
			UINT numModes = 0;
			result = output->GetDisplayModeList(ActiveHdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM, 0, &numModes, nullptr);
			if (SUCCEEDED(result))
			{
				std::vector<DXGI_MODE_DESC> descs(numModes);
				result = output->GetDisplayModeList(ActiveHdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM, 0, &numModes, descs.data());
				if (SUCCEEDED(result))
				{
					for (const DXGI_MODE_DESC& desc : descs)
					{
						resolutions.insert({ (int)desc.Width, (int)desc.Height });
					}
				}
			}
			output->Release();
		}

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
#if !defined(UNREALGOLD)
		return URenderDevice::Exec(Cmd, Ar);
#else
		return 0;
#endif
	}

	unguard;
}

void UD3D11RenderDevice::MapVertices(bool nextBuffer)
{
	if (!SceneVertices)
	{
		D3D11_MAPPED_SUBRESOURCE mappedVertexBuffer = {};
		HRESULT result = Context->Map(ScenePass.VertexBuffer, 0, nextBuffer ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mappedVertexBuffer);
		if (SUCCEEDED(result))
		{
			SceneVertices = (SceneVertex*)mappedVertexBuffer.pData;
		}
	}

	if (!SceneIndexes)
	{
		D3D11_MAPPED_SUBRESOURCE mappedIndexBuffer = {};
		HRESULT result = Context->Map(ScenePass.IndexBuffer, 0, nextBuffer ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mappedIndexBuffer);
		if (SUCCEEDED(result))
		{
			SceneIndexes = (uint32_t*)mappedIndexBuffer.pData;
		}
	}
}

void UD3D11RenderDevice::UnmapVertices()
{
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
}

void UD3D11RenderDevice::Lock(FPlane InFlashScale, FPlane InFlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* InHitData, INT* InHitSize)
{
	guard(UD3D11RenderDevice::Lock);

	Timers.DrawBatches.Reset();
	Timers.DrawComplexSurface.Reset();
	Timers.DrawGouraudPolygon.Reset();
	Timers.DrawTile.Reset();
	Timers.DrawGouraudTriangles.Reset();
	Timers.TextureCache.Reset();
	Timers.TextureUpload.Reset();

	nulltex = Textures->GetNullTexture();

	int wantedBufferCount = UseVSync ? 2 : 3;
	if (BufferCount != wantedBufferCount)
	{
		BufferCount = wantedBufferCount;
		ReleaseSwapChainResources();
		UpdateSwapChain();
	}

	if (CurrentSizeX && CurrentSizeY)
	{
		try
		{
			ResizeSceneBuffers(CurrentSizeX, CurrentSizeY, GetSettingsMultisample());
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
	ID3D11Buffer* vertexBuffers[1] = { ScenePass.VertexBuffer.get() };
	ID3D11Buffer* cbs[1] = { ScenePass.ConstantBuffer.get() };
	Context->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
	Context->IASetIndexBuffer(ScenePass.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	Context->IASetInputLayout(ScenePass.InputLayout);
	Context->VSSetShader(ScenePass.VertexShader, nullptr, 0);
	Context->VSSetConstantBuffers(0, 1, cbs);
	Context->RSSetState(ScenePass.RasterizerState[SceneBuffers.Multisample > 1]);

	D3D11_RECT box = {};
	box.right = CurrentSizeX;
	box.bottom = CurrentSizeY;
	Context->RSSetScissorRects(1, &box);

	MapVertices(true);

	SceneConstants.HitIndex = 0;
	ForceHitIndex = -1;

	IsLocked = true;

	unguard;
}

void UD3D11RenderDevice::DrawStats(FSceneNode* Frame)
{
	Super::DrawStats(Frame);

	CycleTimer::SetActive(true);

#if defined(OLDUNREAL469SDK)
	GRender->ShowStat(
		CurrentFrame,
		TEXT("D3D11: Draw calls: %d, Complex surfaces: %d, Gouraud polygons: %d, Tiles: %d; Uploads: %d, Rect Uploads: %d, Buffers Used: %d\r\n"),
		Stats.DrawCalls,
		Stats.ComplexSurfaces,
		Stats.GouraudPolygons,
		Stats.Tiles,
		Stats.Uploads,
		Stats.RectUploads,
		Stats.BuffersUsed);

	GRender->ShowStat(
		CurrentFrame,
		TEXT("D3D11: DrawBatches: %f ms, Complex surfaces: %f ms, Polygons: %f ms, Tiles: %f ms, Cache %f ms, Upload %f ms\r\n"),
		Timers.DrawBatches.TimeMS(),
		Timers.DrawComplexSurface.TimeMS(),
		Timers.DrawGouraudPolygon.TimeMS() + Timers.DrawGouraudTriangles.TimeMS(),
		Timers.DrawTile.TimeMS(),
		Timers.TextureCache.TimeMS(),
		Timers.TextureUpload.TimeMS());
#endif

	Stats.DrawCalls = 0;
	Stats.ComplexSurfaces = 0;
	Stats.GouraudPolygons = 0;
	Stats.Tiles = 0;
	Stats.Uploads = 0;
	Stats.RectUploads = 0;
	Stats.BuffersUsed = 1;
}

PresentPushConstants UD3D11RenderDevice::GetPresentPushConstants()
{
	PresentPushConstants pushconstants;
	pushconstants.HdrScale = 0.8f + HdrScale * (3.0f / 255.0f);
	if (Viewport->IsOrtho())
	{
		pushconstants.GammaCorrection = { 1.0f };
		pushconstants.Contrast = 1.0f;
		pushconstants.Saturation = 1.0f;
		pushconstants.Brightness = 0.0f;
	}
	else
	{
		float brightness = Clamp(Viewport->GetOuterUClient()->Brightness * 2.0, 0.05, 2.99);

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
	return pushconstants;
}

void UD3D11RenderDevice::Unlock(UBOOL Blit)
{
	guard(UD3D11RenderDevice::Unlock);

	if (!IsLocked) // Don't trust the engine.
		return;

	DrawBatches();
	UnmapVertices();

	Batch.SceneIndexStart = 0;
	SceneVertexPos = 0;
	SceneIndexPos = 0;

	if (Blit)
	{
		if (SceneBuffers.Multisample > 1)
		{
			Context->ResolveSubresource(SceneBuffers.PPImage[0], 0, SceneBuffers.ColorBuffer, 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
		}
		else
		{
			Context->CopyResource(SceneBuffers.PPImage[0], SceneBuffers.ColorBuffer);
		}

		if (Bloom)
		{
			RunBloomPass();
		}

		ID3D11RenderTargetView* rtvs[1] = { BackBufferView.get() };
		Context->OMSetRenderTargets(1, rtvs, nullptr);

		D3D11_VIEWPORT viewport = {};
		viewport.Width = CurrentSizeX;
		viewport.Height = CurrentSizeY;
		viewport.MaxDepth = 1.0f;
		Context->RSSetViewports(1, &viewport);

		PresentPushConstants pushconstants = GetPresentPushConstants();

		// Select present shader based on what the user is actually using
		int presentShader = 0;
		if (ActiveHdr) presentShader |= 1;
		if (GammaMode == 1) presentShader |= 2;
		if (pushconstants.Brightness != 0.0f || pushconstants.Contrast != 1.0f || pushconstants.Saturation != 1.0f) presentShader |= (Clamp(GrayFormula, 0, 2) + 1) << 2;

		UINT stride = sizeof(vec2);
		UINT offset = 0;
		ID3D11Buffer* vertexBuffers[1] = { PresentPass.PPStepVertexBuffer.get()};
		ID3D11ShaderResourceView* psResources[] = { SceneBuffers.PPImageShaderView[0], PresentPass.DitherTextureView};
		ID3D11Buffer* cbs[1] = { PresentPass.PresentConstantBuffer.get() };
		Context->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
		Context->IASetInputLayout(PresentPass.PPStepLayout);
		Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Context->VSSetShader(PresentPass.PPStep, nullptr, 0);
		Context->RSSetState(PresentPass.RasterizerState);
		Context->PSSetShader(PresentPass.Present[presentShader], nullptr, 0);
		Context->PSSetConstantBuffers(0, 1, cbs);
		Context->PSSetShaderResources(0, 2, psResources);
		Context->OMSetDepthStencilState(PresentPass.DepthStencilState, 0);
		Context->OMSetBlendState(PresentPass.BlendState, nullptr, 0xffffffff);
		Context->UpdateSubresource(PresentPass.PresentConstantBuffer, 0, nullptr, &pushconstants, 0, 0);
		Context->Draw(6, 0);

		if (SwapChain1)
		{
			UINT flags = 0;
			if (!UseVSync && !CurrentFullscreen && DxgiSwapChainAllowTearing)
				flags |= DXGI_PRESENT_ALLOW_TEARING;

			DXGI_PRESENT_PARAMETERS presentParams = {};
			SwapChain1->Present1(UseVSync ? 1 : 0, flags, &presentParams);
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

		UpdateLODBias();
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
			ID3D11RenderTargetView* rtvs[1] = { SceneBuffers.PPHitBufferView.get() };
			Context->OMSetRenderTargets(1, rtvs, nullptr);

			D3D11_VIEWPORT viewport = {};
			viewport.TopLeftX = box.left;
			viewport.TopLeftY = box.top;
			viewport.Width = box.right - box.left;
			viewport.Height = box.bottom - box.top;
			viewport.MaxDepth = 1.0f;
			Context->RSSetViewports(1, &viewport);

			UINT stride = sizeof(vec2);
			UINT offset = 0;
			ID3D11Buffer* vertexBuffers[1] = { PresentPass.PPStepVertexBuffer.get() };
			ID3D11ShaderResourceView* srvs[1] = { SceneBuffers.HitBufferShaderView.get() };
			Context->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
			Context->IASetInputLayout(PresentPass.PPStepLayout);
			Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			Context->VSSetShader(PresentPass.PPStep, nullptr, 0);
			Context->RSSetState(PresentPass.RasterizerState);
			Context->PSSetShader(PresentPass.HitResolve, nullptr, 0);
			Context->PSSetShaderResources(0, 1, srvs);
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

		hit = std::max(hit, ForceHitIndex);

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

	PrintDebugLayerMessages();

	unguard;
}

void UD3D11RenderDevice::PushHit(const BYTE* Data, INT Count)
{
	guard(UD3D11RenderDevice::PushHit);

	if (Count <= 0) return;
	HitQueryStack.insert(HitQueryStack.end(), Data, Data + Count);

	SetHitLocation();

	unguard;
}

void UD3D11RenderDevice::PopHit(INT Count, UBOOL bForce)
{
	guard(UD3D11RenderDevice::PopHit);

	if (bForce) // Force hit what we are popping
		ForceHitIndex = HitQueries.size() - 1;

	HitQueryStack.resize(HitQueryStack.size() - Count);

	SetHitLocation();

	unguard;
}

void UD3D11RenderDevice::SetHitLocation()
{
	DrawBatches();

	if (!HitQueryStack.empty())
	{
		INT index = HitQueries.size();

		HitQuery query;
		query.Start = HitBuffer.size();
		query.Count = HitQueryStack.size();
		HitQueries.push_back(query);

		HitBuffer.insert(HitBuffer.end(), HitQueryStack.begin(), HitQueryStack.end());

		SceneConstants.HitIndex = index + 1;
	}
	else
	{
		SceneConstants.HitIndex = 0;
	}

	Context->UpdateSubresource(ScenePass.ConstantBuffer, 0, nullptr, &SceneConstants, 0, 0);
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
	guardSlow(UD3D11RenderDevice::UpdateTextureRect);

	Textures->UpdateTextureRect(&Info, U, V, UL, VL);

	unguardSlow;
}

#endif

void UD3D11RenderDevice::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet)
{
	guardSlow(UD3D11RenderDevice::DrawComplexSurface);

	Timers.DrawComplexSurface.Clock();
	ActiveTimer = &Timers.DrawComplexSurface;

	DWORD PolyFlags = ApplyPrecedenceRules(Surface.PolyFlags);

	ComplexSurfaceInfo info;
	info.facet = &Facet;
	info.tex = Textures->GetTexture(Surface.Texture, !!(PolyFlags & PF_Masked));
	info.lightmap = Textures->GetTexture(Surface.LightMap, false);
	info.macrotex = Textures->GetTexture(Surface.MacroTexture, false);
	info.detailtex = Textures->GetTexture(Surface.DetailTexture, false);
	info.fogmap = (Surface.FogMap && Surface.FogMap->Mips[0] && Surface.FogMap->Mips[0]->DataPtr) ?
		Textures->GetTexture(Surface.FogMap, false) : nulltex;
	info.editorcolor = nullptr;

#if defined(UNREALGOLD)
	if (Surface.DetailTexture && Surface.FogMap) info.detailtex = nulltex;
#else
	if ((Surface.DetailTexture && Surface.FogMap) || (!DetailTextures)) info.detailtex = nulltex;
#endif

	if (info.fogmap != nulltex)
		info.detailtex = info.fogmap;

	SetPipeline(PolyFlags);
	SetDescriptorSet(PolyFlags, info);

	DrawComplexSurfaceFaces(info);

	Stats.ComplexSurfaces++;
	Timers.DrawComplexSurface.Unclock();
	ActiveTimer = nullptr;

	if (!GIsEditor || (PolyFlags & (PF_Selected | PF_FlatShaded)) == 0)
		return;

	// Editor highlight surface (so stupid this is delegated to the renderdev as the engine could just issue a second call):

	SetPipeline(PF_Highlighted);
	SetDescriptorSet(PF_Highlighted);

	vec4 editorcolor;
	if (PolyFlags & PF_FlatShaded)
	{
		editorcolor.x = Surface.FlatColor.R / 255.0f;
		editorcolor.y = Surface.FlatColor.G / 255.0f;
		editorcolor.z = Surface.FlatColor.B / 255.0f;
		editorcolor.w = 0.85f;
		if (PolyFlags & PF_Selected)
		{
			editorcolor.x *= 1.5f;
			editorcolor.y *= 1.5f;
			editorcolor.z *= 1.5f;
			editorcolor.w = 1.0f;
		}
	}
	else
	{
		editorcolor = vec4(0.0f, 0.0f, 0.05f, 0.20f);
	}
	info.editorcolor = &editorcolor;

	DrawComplexSurfaceFaces(info);

	unguardSlow;
}

#ifdef USE_SSE2

// Calculates dot(vec4, vec4). All elements will hold the result.
inline __m128 sse_dot4(__m128 v0, __m128 v1)
{
	v0 = _mm_mul_ps(v0, v1);

	v1 = _mm_shuffle_ps(v0, v0, _MM_SHUFFLE(2, 3, 0, 1));
	v0 = _mm_add_ps(v0, v1);
	v1 = _mm_shuffle_ps(v0, v0, _MM_SHUFFLE(0, 1, 2, 3));
	v0 = _mm_add_ps(v0, v1);

	return v0;
}

void UD3D11RenderDevice::DrawComplexSurfaceFaces(const ComplexSurfaceInfo& info)
{
	uint32_t flags = 0;
	if (info.lightmap != nulltex) flags |= 1;
	if (info.macrotex != nulltex) flags |= 2;
	if (info.detailtex != nulltex && info.fogmap == nulltex) flags |= 4;
	if (info.fogmap != nulltex) flags |= 8;
	if (LightMode == 1) flags |= 64;

	__m128 mflags = _mm_castsi128_ps(_mm_cvtsi32_si128(flags));
	__m128 maskClearW = _mm_castsi128_ps(_mm_setr_epi32(0xffffffff, 0xffffffff, 0xffffffff, 0));
	__m128 xaxis = _mm_and_ps(_mm_loadu_ps((float*)&info.facet->MapCoords.XAxis), maskClearW);
	__m128 yaxis = _mm_and_ps(_mm_loadu_ps((float*)&info.facet->MapCoords.YAxis), maskClearW);
	__m128 origin = _mm_and_ps(_mm_loadu_ps((float*)&info.facet->MapCoords.Origin), maskClearW);

	__m128 UDot = sse_dot4(xaxis, origin);
	__m128 VDot = sse_dot4(yaxis, origin);
	__m128 UVDot = _mm_shuffle_ps(UDot, VDot, _MM_SHUFFLE(0, 0, 0, 0));
	UVDot = _mm_shuffle_ps(UVDot, UVDot, _MM_SHUFFLE(2, 0, 2, 0));

	float UPan = info.tex->PanX;
	float VPan = info.tex->PanY;
	float LMUPan = info.lightmap->PanX - 0.5f * info.lightmap->UScale;
	float LMVPan = info.lightmap->PanY - 0.5f * info.lightmap->VScale;
	__m128 pan0 = _mm_add_ps(UVDot, _mm_setr_ps(UPan, VPan, LMUPan, LMVPan));

	float MacroUPan = info.macrotex->PanX;
	float MacroVPan = info.macrotex->PanY;
	float DetailUPan = info.fogmap == nulltex ? info.detailtex->PanX : info.fogmap->PanX - 0.5f * info.fogmap->UScale;
	float DetailVPan = info.fogmap == nulltex ? info.detailtex->PanY : info.fogmap->PanY - 0.5f * info.fogmap->VScale;
	__m128 pan1 = _mm_add_ps(UVDot, _mm_setr_ps(MacroUPan, MacroVPan, DetailUPan, DetailVPan));

	float UMult = info.tex->UMult;
	float VMult = info.tex->VMult;
	float LMUMult = info.lightmap->UMult;
	float LMVMult = info.lightmap->VMult;
	__m128 mult0 = _mm_setr_ps(UMult, VMult, LMUMult, LMVMult);

	float MacroUMult = info.macrotex->UMult;
	float MacroVMult = info.macrotex->VMult;
	float DetailUMult = info.fogmap == nulltex ? info.detailtex->UMult : info.fogmap->UMult;
	float DetailVMult = info.fogmap == nulltex ? info.detailtex->VMult : info.fogmap->VMult;
	__m128 mult1 = _mm_setr_ps(MacroUMult, MacroVMult, DetailUMult, DetailVMult);

	__m128 color = info.editorcolor ? _mm_loadu_ps(&info.editorcolor->x) : _mm_set_ps1(1.0f);

	for (FSavedPoly* Poly = info.facet->Polys; Poly; Poly = Poly->Next)
	{
		auto pts = Poly->Pts;
		uint32_t vcount = Poly->NumPts;
		if (vcount < 3) continue;

		uint32_t icount = (vcount - 2) * 3;
		auto alloc = ReserveVertices(vcount, icount);
		if (alloc.vptr)
		{
			SceneVertex* vptr = alloc.vptr;
			uint32_t* iptr = alloc.iptr;
			uint32_t vpos = alloc.vpos;

			for (uint32_t i = 0; i < vcount; i++)
			{
				__m128 point = _mm_and_ps(_mm_loadu_ps((float*)&pts[i]->Point), maskClearW);
				__m128 u = sse_dot4(xaxis, point);
				__m128 v = sse_dot4(yaxis, point);
				__m128 uv = _mm_shuffle_ps(u, v, _MM_SHUFFLE(0, 0, 0, 0));
				uv = _mm_shuffle_ps(uv, uv, _MM_SHUFFLE(2, 0, 2, 0));

				__m128 pos = _mm_or_ps(_mm_shuffle_ps(point, point, _MM_SHUFFLE(2, 1, 0, 3)), mflags);
				__m128 uv0 = _mm_mul_ps(_mm_sub_ps(uv, pan0), mult0);
				__m128 uv1 = _mm_mul_ps(_mm_sub_ps(uv, pan1), mult1);

				_mm_store_ps((float*)vptr, pos);
				_mm_store_ps((float*)vptr + 4, uv0);
				_mm_store_ps((float*)vptr + 8, uv1);
				_mm_store_ps((float*)vptr + 12, color);
				vptr++;
			}

			for (uint32_t i = vpos + 2; i < vpos + vcount; i++)
			{
				*(iptr++) = vpos;
				*(iptr++) = i - 1;
				*(iptr++) = i;
			}

			UseVertices(vcount, icount);
		}
	}
}

#else

void UD3D11RenderDevice::DrawComplexSurfaceFaces(const ComplexSurfaceInfo& info)
{
	uint32_t flags = 0;
	if (info.lightmap != nulltex) flags |= 1;
	if (info.macrotex != nulltex) flags |= 2;
	if (info.detailtex != nulltex && info.fogmap == nulltex) flags |= 4;
	if (info.fogmap != nulltex) flags |= 8;
	if (LightMode == 1) flags |= 64;

	FVector xaxis = info.facet->MapCoords.XAxis;
	FVector yaxis = info.facet->MapCoords.YAxis;
	float UDot = xaxis | info.facet->MapCoords.Origin;
	float VDot = yaxis | info.facet->MapCoords.Origin;

	float UPan = UDot + info.tex->PanX;
	float VPan = VDot + info.tex->PanY;
	float LMUPan = UDot + info.lightmap->PanX - 0.5f * info.lightmap->UScale;
	float LMVPan = VDot + info.lightmap->PanY - 0.5f * info.lightmap->VScale;
	float MacroUPan = UDot + info.macrotex->PanX;
	float MacroVPan = VDot + info.macrotex->PanY;
	float DetailUPan = UDot + (info.fogmap == nulltex ? info.detailtex->PanX : info.fogmap->PanX - 0.5f * info.fogmap->UScale);
	float DetailVPan = VDot + (info.fogmap == nulltex ? info.detailtex->PanY : info.fogmap->PanY - 0.5f * info.fogmap->VScale);

	float UMult = info.tex->UMult;
	float VMult = info.tex->VMult;
	float LMUMult = info.lightmap->UMult;
	float LMVMult = info.lightmap->VMult;
	float MacroUMult = info.macrotex->UMult;
	float MacroVMult = info.macrotex->VMult;
	float DetailUMult = info.fogmap == nulltex ? info.detailtex->UMult : info.fogmap->UMult;
	float DetailVMult = info.fogmap == nulltex ? info.detailtex->VMult : info.fogmap->VMult;

	vec4 color = info.editorcolor ? *info.editorcolor : vec4(1.0f);

	for (FSavedPoly* Poly = info.facet->Polys; Poly; Poly = Poly->Next)
	{
		auto pts = Poly->Pts;
		uint32_t vcount = Poly->NumPts;
		if (vcount < 3) continue;

		uint32_t icount = (vcount - 2) * 3;
		auto alloc = ReserveVertices(vcount, icount);
		if (alloc.vptr)
		{
			SceneVertex* vptr = alloc.vptr;
			uint32_t* iptr = alloc.iptr;
			uint32_t vpos = alloc.vpos;

			for (uint32_t i = 0; i < vcount; i++)
			{
				FVector point = pts[i]->Point;
				FLOAT u = xaxis | point;
				FLOAT v = yaxis | point;

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

			UseVertices(vcount, icount);
		}
	}
}

#endif

void UD3D11RenderDevice::DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, int NumPts, DWORD PolyFlags, FSpanBuffer* Span)
{
	guardSlow(UD3D11RenderDevice::DrawGouraudPolygon);

	if (NumPts < 3) return; // This can apparently happen!!

	Timers.DrawGouraudPolygon.Clock();
	ActiveTimer = &Timers.DrawGouraudPolygon;

	PolyFlags = ApplyPrecedenceRules(PolyFlags);

	CachedTexture* tex = Textures->GetTexture(&Info, !!(PolyFlags & PF_Masked));

	SetPipeline(PolyFlags);
	SetDescriptorSet(PolyFlags, tex);

	float UMult = tex->UMult;
	float VMult = tex->VMult;

	int flags = (PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated)) == PF_RenderFog ? 16 : 0;
	if ((PolyFlags & (PF_Translucent | PF_Modulated)) == 0 && LightMode == 2) flags |= 32;

#ifdef USE_SSE2
	__m128 mflags = _mm_castsi128_ps(_mm_cvtsi32_si128(flags));
	__m128 maskClearW = _mm_castsi128_ps(_mm_setr_epi32(0xffffffff, 0xffffffff, 0xffffffff, 0));
#endif

	auto alloc = ReserveVertices(NumPts, (NumPts - 2) * 3);
	if (alloc.vptr)
	{
		SceneVertex* vptr = alloc.vptr;
		uint32_t* iptr = alloc.iptr;
		uint32_t vpos = alloc.vpos;

#ifdef USE_SSE2
		if (PolyFlags & PF_Modulated)
		{
			SceneVertex* vertex = vptr;
			__m128 color = _mm_set_ps1(1.0f);
			for (INT i = 0; i < NumPts; i++)
			{
				FTransTexture* P = Pts[i];

				__m128 point = _mm_and_ps(_mm_loadu_ps((float*)&P->Point), maskClearW);
				__m128 fog = _mm_loadu_ps((float*)&P->Fog);
				__m128 pos = _mm_or_ps(_mm_shuffle_ps(point, point, _MM_SHUFFLE(2, 1, 0, 3)), mflags);
				__m128 uvzero = _mm_setr_ps(P->U * UMult, P->V * VMult, 0.0f, 0.0f);
				__m128 uv0 = _mm_shuffle_ps(uvzero, fog, _MM_SHUFFLE(1, 0, 1, 0));
				__m128 uv1 = _mm_shuffle_ps(fog, uvzero, _MM_SHUFFLE(1, 0, 1, 0));

				_mm_store_ps((float*)vertex, pos);
				_mm_store_ps((float*)vertex + 4, uv0);
				_mm_store_ps((float*)vertex + 8, uv1);
				_mm_store_ps((float*)vertex + 12, color);
				vertex++;
			}
		}
		else
		{
			SceneVertex* vertex = vptr;
			for (INT i = 0; i < NumPts; i++)
			{
				FTransTexture* P = Pts[i];

				__m128 point = _mm_and_ps(_mm_loadu_ps((float*)&P->Point), maskClearW);
				__m128 fog = _mm_loadu_ps((float*)&P->Fog);
				__m128 pos = _mm_or_ps(_mm_shuffle_ps(point, point, _MM_SHUFFLE(2, 1, 0, 3)), mflags);
				__m128 uvzero = _mm_setr_ps(P->U * UMult, P->V * VMult, 0.0f, 0.0f);
				__m128 uv0 = _mm_shuffle_ps(uvzero, fog, _MM_SHUFFLE(1, 0, 1, 0));
				__m128 uv1 = _mm_shuffle_ps(fog, uvzero, _MM_SHUFFLE(1, 0, 1, 0));
				__m128 color = _mm_and_ps(_mm_loadu_ps((float*)&P->Light), maskClearW);
				color = _mm_or_ps(color, _mm_setr_ps(0.0f, 0.0f, 0.0f, 1.0f));

				_mm_store_ps((float*)vertex, pos);
				_mm_store_ps((float*)vertex + 4, uv0);
				_mm_store_ps((float*)vertex + 8, uv1);
				_mm_store_ps((float*)vertex + 12, color);
				vertex++;
			}
		}
#else
		if (PolyFlags & PF_Modulated)
		{
			SceneVertex* vertex = vptr;

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
			SceneVertex* vertex = vptr;
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
#endif

		uint32_t vstart = vpos;
		uint32_t vcount = NumPts;
		for (uint32_t i = vstart + 2; i < vstart + vcount; i++)
		{
			*(iptr++) = vstart;
			*(iptr++) = i - 1;
			*(iptr++) = i;
		}

		UseVertices(NumPts, (NumPts - 2) * 3);
	}

	Stats.GouraudPolygons++;
	Timers.DrawGouraudPolygon.Unclock();
	ActiveTimer = nullptr;

	unguardSlow;
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
	guardSlow(UD3D11RenderDevice::DrawGouraudTriangles);

	if (NumPts < 3) return; // This can apparently happen!!

	Timers.DrawGouraudTriangles.Clock();
	ActiveTimer = &Timers.DrawGouraudTriangles;

	PolyFlags = ApplyPrecedenceRules(PolyFlags);

	if (PolyFlags & PF_Environment)
	{
		FLOAT UScale = Info.UScale * Info.USize * (1.0f / 256.0f);
		FLOAT VScale = Info.VScale * Info.VSize * (1.0f / 256.0f);

		for (INT i = 0; i < NumPts; i++)
			::EnviroMap(Frame, Pts[i], UScale, VScale);
	}

	CachedTexture* tex = Textures->GetTexture(const_cast<FTextureInfo*>(&Info), !!(PolyFlags & PF_Masked));

	SetPipeline(PolyFlags);
	SetDescriptorSet(PolyFlags, tex);

	float UMult = tex->UMult;
	float VMult = tex->VMult;
	int flags = (PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated)) == PF_RenderFog ? 16 : 0;
	if ((PolyFlags & (PF_Translucent | PF_Modulated)) == 0 && LightMode == 2) flags |= 32;

#ifdef USE_SSE2
	__m128 mflags = _mm_castsi128_ps(_mm_cvtsi32_si128(flags));
	__m128 maskClearW = _mm_castsi128_ps(_mm_setr_epi32(0xffffffff, 0xffffffff, 0xffffffff, 0));
#endif

	auto alloc = ReserveVertices(NumPts, (NumPts - 2) * 3);
	if (alloc.vptr)
	{
		SceneVertex* vptr = alloc.vptr;
		uint32_t* iptr = alloc.iptr;
		uint32_t vpos = alloc.vpos;

#ifdef USE_SSE2
		if (PolyFlags & PF_Modulated)
		{
			SceneVertex* vertex = vptr;
			__m128 color = _mm_set_ps1(1.0f);
			for (INT i = 0; i < NumPts; i++)
			{
				FTransTexture* P = &Pts[i];

				__m128 point = _mm_and_ps(_mm_loadu_ps((float*)&P->Point), maskClearW);
				__m128 fog = _mm_loadu_ps((float*)&P->Fog);
				__m128 pos = _mm_or_ps(_mm_shuffle_ps(point, point, _MM_SHUFFLE(2, 1, 0, 3)), mflags);
				__m128 uvzero = _mm_setr_ps(P->U * UMult, P->V * VMult, 0.0f, 0.0f);
				__m128 uv0 = _mm_shuffle_ps(uvzero, fog, _MM_SHUFFLE(1, 0, 1, 0));
				__m128 uv1 = _mm_shuffle_ps(fog, uvzero, _MM_SHUFFLE(1, 0, 1, 0));

				_mm_store_ps((float*)vertex, pos);
				_mm_store_ps((float*)vertex + 4, uv0);
				_mm_store_ps((float*)vertex + 8, uv1);
				_mm_store_ps((float*)vertex + 12, color);
				vertex++;
			}
		}
		else
		{
			SceneVertex* vertex = vptr;
			for (INT i = 0; i < NumPts; i++)
			{
				FTransTexture* P = &Pts[i];

				__m128 point = _mm_and_ps(_mm_loadu_ps((float*)&P->Point), maskClearW);
				__m128 fog = _mm_loadu_ps((float*)&P->Fog);
				__m128 pos = _mm_or_ps(_mm_shuffle_ps(point, point, _MM_SHUFFLE(2, 1, 0, 3)), mflags);
				__m128 uvzero = _mm_setr_ps(P->U * UMult, P->V * VMult, 0.0f, 0.0f);
				__m128 uv0 = _mm_shuffle_ps(uvzero, fog, _MM_SHUFFLE(1, 0, 1, 0));
				__m128 uv1 = _mm_shuffle_ps(fog, uvzero, _MM_SHUFFLE(1, 0, 1, 0));
				__m128 color = _mm_and_ps(_mm_loadu_ps((float*)&P->Light), maskClearW);
				color = _mm_or_ps(color, _mm_setr_ps(0.0f, 0.0f, 0.0f, 1.0f));

				_mm_store_ps((float*)vertex, pos);
				_mm_store_ps((float*)vertex + 4, uv0);
				_mm_store_ps((float*)vertex + 8, uv1);
				_mm_store_ps((float*)vertex + 12, color);
				vertex++;
			}
		}
#else
		if (PolyFlags & PF_Modulated)
		{
			SceneVertex* vertex = vptr;
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
			SceneVertex* vertex = vptr;
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
#endif

		bool mirror = (Frame->Mirror == -1.0);

		size_t vstart = vpos;
		size_t vcount = NumPts;
		size_t icount = 0;

		if (PolyFlags & PF_TwoSided)
		{
			for (uint32_t i = 2; i < vcount; i += 3)
			{
				// If outcoded, skip it.
				if (Pts[i - 2].Flags & Pts[i - 1].Flags & Pts[i].Flags)
					continue;

				*(iptr++) = vstart + i;
				*(iptr++) = vstart + i - 1;
				*(iptr++) = vstart + i - 2;
				icount += 3;
			}
		}
		else
		{
			for (uint32_t i = 2; i < vcount; i += 3)
			{
				// If outcoded, skip it.
				if (Pts[i - 2].Flags & Pts[i - 1].Flags & Pts[i].Flags)
					continue;

				bool backface = FTriple(Pts[i - 2].Point, Pts[i - 1].Point, Pts[i].Point) <= 0.0;
				if (mirror == backface)
				{
					*(iptr++) = vstart + i - 2;
					*(iptr++) = vstart + i - 1;
					*(iptr++) = vstart + i;
					icount += 3;
				}
			}
		}

		UseVertices(vcount, icount);
	}

	Stats.GouraudPolygons++;
	Timers.DrawGouraudTriangles.Unclock();
	ActiveTimer = nullptr;

	unguardSlow;
}

#endif

#if defined(OLDUNREAL469SDK)

void UD3D11RenderDevice::DrawTileList(const FSceneNode* Frame, const FTextureInfo& Info, const FTileRect* Tiles, INT NumTiles, FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags)
{
	guardSlow(UD3D11RenderDevice::DrawTileList);

	Timers.DrawTile.Clock();
	ActiveTimer = &Timers.DrawTile;

	// stijn: fix for invisible actor icons in ortho viewports
	if (GIsEditor && Frame->Viewport->Actor && (Frame->Viewport->IsOrtho() || Abs(Z) <= SMALL_NUMBER))
	{
		Z = 1.f;
	}

	PolyFlags = ApplyPrecedenceRules(PolyFlags);

	CachedTexture* tex = Textures->GetTexture(const_cast<FTextureInfo*>(&Info), !!(PolyFlags & PF_Masked));
	float UMult = tex->UMult;
	float VMult = tex->VMult;
	int curclampmode = -1;

	SetPipeline(PolyFlags);

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

	float rfx2z = RFX2 * Z;
	float rfy2z = RFY2 * Z;

	for (INT i = 0; i < NumTiles; i++)
	{
		auto alloc = ReserveVertices(4, 6);
		if (!alloc.vptr)
			break;

		SceneVertex* vptr = alloc.vptr;
		uint32_t* iptr = alloc.iptr;
		uint32_t vpos = alloc.vpos;

		FLOAT X = Tiles[i].X;
		FLOAT Y = Tiles[i].Y;
		FLOAT XL = Tiles[i].XL;
		FLOAT YL = Tiles[i].YL;
		FLOAT U = Tiles[i].U;
		FLOAT V = Tiles[i].V;
		FLOAT UL = Tiles[i].UL;
		FLOAT VL = Tiles[i].VL;

		float u0 = U * UMult;
		float v0 = V * VMult;
		float u1 = (U + UL) * UMult;
		float v1 = (V + VL) * VMult;
		int clamp = (u0 >= 0.0f && u1 <= 1.00001f && v0 >= 0.0f && v1 <= 1.00001f);
		if (clamp != curclampmode)
		{
			SetDescriptorSet(PolyFlags, tex, clamp);
			curclampmode = clamp;
		}

		if (SceneBuffers.Multisample > 1)
		{
			XL = std::floor(X + XL + 0.5f);
			YL = std::floor(Y + YL + 0.5f);
			X = std::floor(X + 0.5f);
			Y = std::floor(Y + 0.5f);
			XL = XL - X;
			YL = YL - Y;
		}

		X -= Frame->FX2;
		Y -= Frame->FY2;
		XL += X;
		YL += Y;
		U *= UMult;
		UL = U + UL * UMult;
		V *= VMult;
		VL = V + VL * VMult;

		vptr[0].Flags = 0;
		vptr[0].Position.x = rfx2z * X;
		vptr[0].Position.y = rfy2z * Y;
		vptr[0].Position.z = Z;
		vptr[0].TexCoord.s = U;
		vptr[0].TexCoord.t = V;
		vptr[0].TexCoord2.s = 0.0f;
		vptr[0].TexCoord2.t = 0.0f;
		vptr[0].TexCoord3.s = 0.0f;
		vptr[0].TexCoord3.t = 0.0f;
		vptr[0].TexCoord4.s = 0.0f;
		vptr[0].TexCoord4.t = 0.0f;
		vptr[0].Color.r = r;
		vptr[0].Color.g = g;
		vptr[0].Color.b = b;
		vptr[0].Color.a = a;

		vptr[1].Flags = 0;
		vptr[1].Position.x = rfx2z * XL;
		vptr[1].Position.y = rfy2z * Y;
		vptr[1].Position.z = Z;
		vptr[1].TexCoord.s = UL;
		vptr[1].TexCoord.t = V;
		vptr[1].TexCoord2.s = 0.0f;
		vptr[1].TexCoord2.t = 0.0f;
		vptr[1].TexCoord3.s = 0.0f;
		vptr[1].TexCoord3.t = 0.0f;
		vptr[1].TexCoord4.s = 0.0f;
		vptr[1].TexCoord4.t = 0.0f;
		vptr[1].Color.r = r;
		vptr[1].Color.g = g;
		vptr[1].Color.b = b;
		vptr[1].Color.a = a;

		vptr[2].Flags = 0;
		vptr[2].Position.x = rfx2z * XL;
		vptr[2].Position.y = rfy2z * YL;
		vptr[2].Position.z = Z;
		vptr[2].TexCoord.s = UL;
		vptr[2].TexCoord.t = VL;
		vptr[2].TexCoord2.s = 0.0f;
		vptr[2].TexCoord2.t = 0.0f;
		vptr[2].TexCoord3.s = 0.0f;
		vptr[2].TexCoord3.t = 0.0f;
		vptr[2].TexCoord4.s = 0.0f;
		vptr[2].TexCoord4.t = 0.0f;
		vptr[2].Color.r = r;
		vptr[2].Color.g = g;
		vptr[2].Color.b = b;
		vptr[2].Color.a = a;

		vptr[3].Flags = 0;
		vptr[3].Position.x = rfx2z * X;
		vptr[3].Position.y = rfy2z * YL;
		vptr[3].Position.z = Z;
		vptr[3].TexCoord.s = U;
		vptr[3].TexCoord.t = VL;
		vptr[3].TexCoord2.s = 0.0f;
		vptr[3].TexCoord2.t = 0.0f;
		vptr[3].TexCoord3.s = 0.0f;
		vptr[3].TexCoord3.t = 0.0f;
		vptr[3].TexCoord4.s = 0.0f;
		vptr[3].TexCoord4.t = 0.0f;
		vptr[3].Color.r = r;
		vptr[3].Color.g = g;
		vptr[3].Color.b = b;
		vptr[3].Color.a = a;

		iptr[0] = vpos;
		iptr[1] = vpos + 1;
		iptr[2] = vpos + 2;
		iptr[3] = vpos;
		iptr[4] = vpos + 2;
		iptr[5] = vpos + 3;

		UseVertices(4, 6);
	}

	Stats.Tiles++;
	Timers.DrawTile.Unclock();
	ActiveTimer = nullptr;

	unguardSlow;
}

#endif

void UD3D11RenderDevice::DrawTile(FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags)
{
	guardSlow(UD3D11RenderDevice::DrawTile);

	Timers.DrawTile.Clock();
	ActiveTimer = &Timers.DrawTile;

	// stijn: fix for invisible actor icons in ortho viewports
	if (GIsEditor && Frame->Viewport->Actor && (Frame->Viewport->IsOrtho() || Abs(Z) <= SMALL_NUMBER))
	{
		Z = 1.f;
	}

	PolyFlags = ApplyPrecedenceRules(PolyFlags);

	CachedTexture* tex = Textures->GetTexture(&Info, !!(PolyFlags & PF_Masked));
	float UMult = tex->UMult;
	float VMult = tex->VMult;
	float u0 = U * UMult;
	float v0 = V * VMult;
	float u1 = (U + UL) * UMult;
	float v1 = (V + VL) * VMult;
	bool clamp = (u0 >= 0.0f && u1 <= 1.00001f && v0 >= 0.0f && v1 <= 1.00001f);

	SetPipeline(PolyFlags);
	SetDescriptorSet(PolyFlags, tex, clamp);

	if (SceneBuffers.Multisample > 1)
	{
		XL = std::floor(X + XL + 0.5f);
		YL = std::floor(Y + YL + 0.5f);
		X = std::floor(X + 0.5f);
		Y = std::floor(Y + 0.5f);
		XL = XL - X;
		YL = YL - Y;
	}

	auto alloc = ReserveVertices(4, 6);
	if (alloc.vptr)
	{
		SceneVertex* vptr = alloc.vptr;
		uint32_t* iptr = alloc.iptr;
		uint32_t vpos = alloc.vpos;

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

		float rfx2z = RFX2 * Z;
		float rfy2z = RFY2 * Z;
		X -= Frame->FX2;
		Y -= Frame->FY2;
		XL += X;
		YL += Y;
		U *= UMult;
		UL = U + UL * UMult;
		V *= VMult;
		VL = V + VL * VMult;

		vptr[0].Flags = 0;
		vptr[0].Position.x = rfx2z * X;
		vptr[0].Position.y = rfy2z * Y;
		vptr[0].Position.z = Z;
		vptr[0].TexCoord.s = U;
		vptr[0].TexCoord.t = V;
		vptr[0].TexCoord2.s = 0.0f;
		vptr[0].TexCoord2.t = 0.0f;
		vptr[0].TexCoord3.s = 0.0f;
		vptr[0].TexCoord3.t = 0.0f;
		vptr[0].TexCoord4.s = 0.0f;
		vptr[0].TexCoord4.t = 0.0f;
		vptr[0].Color.r = r;
		vptr[0].Color.g = g;
		vptr[0].Color.b = b;
		vptr[0].Color.a = a;

		vptr[1].Flags = 0;
		vptr[1].Position.x = rfx2z * XL;
		vptr[1].Position.y = rfy2z * Y;
		vptr[1].Position.z = Z;
		vptr[1].TexCoord.s = UL;
		vptr[1].TexCoord.t = V;
		vptr[1].TexCoord2.s = 0.0f;
		vptr[1].TexCoord2.t = 0.0f;
		vptr[1].TexCoord3.s = 0.0f;
		vptr[1].TexCoord3.t = 0.0f;
		vptr[1].TexCoord4.s = 0.0f;
		vptr[1].TexCoord4.t = 0.0f;
		vptr[1].Color.r = r;
		vptr[1].Color.g = g;
		vptr[1].Color.b = b;
		vptr[1].Color.a = a;

		vptr[2].Flags = 0;
		vptr[2].Position.x = rfx2z * XL;
		vptr[2].Position.y = rfy2z * YL;
		vptr[2].Position.z = Z;
		vptr[2].TexCoord.s = UL;
		vptr[2].TexCoord.t = VL;
		vptr[2].TexCoord2.s = 0.0f;
		vptr[2].TexCoord2.t = 0.0f;
		vptr[2].TexCoord3.s = 0.0f;
		vptr[2].TexCoord3.t = 0.0f;
		vptr[2].TexCoord4.s = 0.0f;
		vptr[2].TexCoord4.t = 0.0f;
		vptr[2].Color.r = r;
		vptr[2].Color.g = g;
		vptr[2].Color.b = b;
		vptr[2].Color.a = a;

		vptr[3].Flags = 0;
		vptr[3].Position.x = rfx2z * X;
		vptr[3].Position.y = rfy2z * YL;
		vptr[3].Position.z = Z;
		vptr[3].TexCoord.s = U;
		vptr[3].TexCoord.t = VL;
		vptr[3].TexCoord2.s = 0.0f;
		vptr[3].TexCoord2.t = 0.0f;
		vptr[3].TexCoord3.s = 0.0f;
		vptr[3].TexCoord3.t = 0.0f;
		vptr[3].TexCoord4.s = 0.0f;
		vptr[3].TexCoord4.t = 0.0f;
		vptr[3].Color.r = r;
		vptr[3].Color.g = g;
		vptr[3].Color.b = b;
		vptr[3].Color.a = a;

		iptr[0] = vpos;
		iptr[1] = vpos + 1;
		iptr[2] = vpos + 2;
		iptr[3] = vpos;
		iptr[4] = vpos + 2;
		iptr[5] = vpos + 3;

		UseVertices(4, 6);
	}

	Stats.Tiles++;
	Timers.DrawTile.Unclock();
	ActiveTimer = nullptr;

	unguardSlow;
}

vec4 UD3D11RenderDevice::ApplyInverseGamma(vec4 color)
{
	if (Viewport->IsOrtho())
		return color;
	float brightness = Clamp(Viewport->GetOuterUClient()->Brightness * 2.0, 0.05, 2.99);
	float gammaRed = Max(brightness + GammaOffset + GammaOffsetRed, 0.001f);
	float gammaGreen = Max(brightness + GammaOffset + GammaOffsetGreen, 0.001f);
	float gammaBlue = Max(brightness + GammaOffset + GammaOffsetBlue, 0.001f);
	return vec4(pow(color.r, gammaRed), pow(color.g, gammaGreen), pow(color.b, gammaBlue), color.a);
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
#if defined(OLDUNREAL469SDK)
		bool occlude = !!(LineFlags & LINE_DepthCued);
#else
		bool occlude = OccludeLines;
#endif
		SetPipeline(&ScenePass.LinePipeline[occlude]);
		SetDescriptorSet(PF_Highlighted);
		vec4 color = ApplyInverseGamma(vec4(Color.X, Color.Y, Color.Z, 1.0f));

		auto alloc = ReserveVertices(2, 2);
		if (alloc.vptr)
		{
			SceneVertex* vptr = alloc.vptr;
			uint32_t* iptr = alloc.iptr;
			uint32_t vpos = alloc.vpos;

			vptr[0] = { 0, vec3(P1.X, P1.Y, P1.Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color };
			vptr[1] = { 0, vec3(P2.X, P2.Y, P2.Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color };

			iptr[0] = vpos;
			iptr[1] = vpos + 1;

			UseVertices(2, 2);
		}
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

#if defined(OLDUNREAL469SDK)
	bool occlude = !!(LineFlags & LINE_DepthCued);
#else
	bool occlude = OccludeLines;
#endif
	SetPipeline(&ScenePass.LinePipeline[occlude]);
	SetDescriptorSet(PF_Highlighted);
	vec4 color = ApplyInverseGamma(vec4(Color.X, Color.Y, Color.Z, 1.0f));

	auto alloc = ReserveVertices(2, 2);
	if (alloc.vptr)
	{
		SceneVertex* vptr = alloc.vptr;
		uint32_t* iptr = alloc.iptr;
		uint32_t vpos = alloc.vpos;

		vptr[0] = { 0, vec3(RFX2 * P1.Z * (P1.X - Frame->FX2), RFY2 * P1.Z * (P1.Y - Frame->FY2), P1.Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color };
		vptr[1] = { 0, vec3(RFX2 * P2.Z * (P2.X - Frame->FX2), RFY2 * P2.Z * (P2.Y - Frame->FY2), P2.Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color };

		iptr[0] = vpos;
		iptr[1] = vpos + 1;

		UseVertices(2, 2);
	}

	unguard;
}

void UD3D11RenderDevice::Draw2DPoint(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z)
{
	guard(UD3D11RenderDevice::Draw2DPoint);

	// Hack to fix UED selection problem with selection brush
	if (GIsEditor) Z = 1.0f;

#if defined(OLDUNREAL469SDK)
	bool occlude = !!(LineFlags & LINE_DepthCued);
#else
	bool occlude = OccludeLines;
#endif
	SetPipeline(&ScenePass.PointPipeline[occlude]);
	SetDescriptorSet(PF_Highlighted);
	vec4 color = ApplyInverseGamma(vec4(Color.X, Color.Y, Color.Z, 1.0f));

	auto alloc = ReserveVertices(4, 6);
	if (alloc.vptr)
	{
		SceneVertex* vptr = alloc.vptr;
		uint32_t* iptr = alloc.iptr;
		uint32_t vpos = alloc.vpos;

		vptr[0] = { 0, vec3(RFX2 * Z * (X1 - Frame->FX2 - 0.5f), RFY2 * Z * (Y1 - Frame->FY2 - 0.5f), Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color };
		vptr[1] = { 0, vec3(RFX2 * Z * (X2 - Frame->FX2 + 0.5f), RFY2 * Z * (Y1 - Frame->FY2 - 0.5f), Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color };
		vptr[2] = { 0, vec3(RFX2 * Z * (X2 - Frame->FX2 + 0.5f), RFY2 * Z * (Y2 - Frame->FY2 + 0.5f), Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color };
		vptr[3] = { 0, vec3(RFX2 * Z * (X1 - Frame->FX2 - 0.5f), RFY2 * Z * (Y2 - Frame->FY2 + 0.5f), Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color };

		iptr[0] = vpos;
		iptr[1] = vpos + 1;
		iptr[2] = vpos + 2;
		iptr[3] = vpos;
		iptr[4] = vpos + 2;
		iptr[5] = vpos + 3;

		UseVertices(4, 6);
	}

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

	UnmapVertices();

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
	SetDebugName(stagingTexture, "ReadPixels.StagingTexture");

	if (GammaCorrectScreenshots)
	{
		ID3D11RenderTargetView* rtvs[1] = { SceneBuffers.PPImageView[1].get() };
		Context->OMSetRenderTargets(1, rtvs, nullptr);

		D3D11_VIEWPORT viewport = {};
		viewport.Width = CurrentSizeX;
		viewport.Height = CurrentSizeY;
		viewport.MaxDepth = 1.0f;
		Context->RSSetViewports(1, &viewport);

		PresentPushConstants pushconstants = GetPresentPushConstants();

		// Select present shader based on what the user is actually using
		int presentShader = 0;
		if (ActiveHdr) presentShader |= 1;
		if (GammaMode == 1) presentShader |= 2;
		if (pushconstants.Brightness != 0.0f || pushconstants.Contrast != 1.0f || pushconstants.Saturation != 1.0f) presentShader |= (Clamp(GrayFormula, 0, 2) + 1) << 2;

		UINT stride = sizeof(vec2);
		UINT offset = 0;
		ID3D11Buffer* vertexBuffers[1] = { PresentPass.PPStepVertexBuffer.get() };
		ID3D11Buffer* cbs[1] = { PresentPass.PresentConstantBuffer.get() };
		ID3D11ShaderResourceView* psResources[] = { SceneBuffers.PPImageShaderView[0], PresentPass.DitherTextureView };
		Context->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
		Context->IASetInputLayout(PresentPass.PPStepLayout);
		Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Context->VSSetShader(PresentPass.PPStep, nullptr, 0);
		Context->RSSetState(PresentPass.RasterizerState);
		Context->PSSetShader(PresentPass.Present[presentShader], nullptr, 0);
		Context->PSSetConstantBuffers(0, 1, cbs);
		Context->PSSetShaderResources(0, 2, psResources);
		Context->OMSetDepthStencilState(PresentPass.DepthStencilState, 0);
		Context->OMSetBlendState(PresentPass.BlendState, nullptr, 0xffffffff);
		Context->UpdateSubresource(PresentPass.PresentConstantBuffer, 0, nullptr, &pushconstants, 0, 0);
		Context->Draw(6, 0);

		Context->CopyResource(stagingTexture, SceneBuffers.PPImage[1]);
	}
	else
	{
		Context->CopyResource(stagingTexture, SceneBuffers.PPImage[0]);
	}

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	result = Context->Map(stagingTexture, 0, D3D11_MAP_READ, 0, &mapped);
	if (SUCCEEDED(result))
	{
		uint8_t* srcpixels = (uint8_t*)mapped.pData;
		int w = CurrentSizeX;
		int h = CurrentSizeY;
		void* data = Pixels;

		for (int y = 0; y < h; y++)
		{
			int desty = GammaCorrectScreenshots ? y : (h - y - 1);
			uint8_t* dest = (uint8_t*)data + desty * w * 4;
			uint16_t* src = (uint16_t*)(srcpixels + y * mapped.RowPitch);
			for (int x = 0; x < w; x++)
			{
				float red = halfToFloatSimple(*(src++));
				float green = halfToFloatSimple(*(src++));
				float blue = halfToFloatSimple(*(src++));
				float alpha = halfToFloatSimple(*(src++));

				dest[0] = (int)clamp(std::round(blue * 255.0f), 0.0f, 255.0f);
				dest[1] = (int)clamp(std::round(green * 255.0f), 0.0f, 255.0f);
				dest[2] = (int)clamp(std::round(red * 255.0f), 0.0f, 255.0f);
				dest[3] = (int)clamp(std::round(alpha * 255.0f), 0.0f, 255.0f);
				dest += 4;
			}
		}

		Context->Unmap(stagingTexture, 0);
	}

	stagingTexture->Release();

	if (IsLocked)
		MapVertices(false);

	unguard;
}

void UD3D11RenderDevice::EndFlash()
{
	guard(UD3D11RenderDevice::EndFlash);
	if (FlashScale != FPlane(0.5f, 0.5f, 0.5f, 0.0f) || FlashFog != FPlane(0.0f, 0.0f, 0.0f, 0.0f))
	{
		DrawBatches();

		vec4 color(FlashFog.X, FlashFog.Y, FlashFog.Z, 1.0f - Min(FlashScale.X * 2.0f, 1.0f));
		vec2 zero2(0.0f);

		SceneConstants.ObjectToProjection = mat4::identity();
		SceneConstants.NearClip = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		Context->UpdateSubresource(ScenePass.ConstantBuffer, 0, nullptr, &SceneConstants, 0, 0);

		SetPipeline(PF_Highlighted);
		SetDescriptorSet(0);

		auto alloc = ReserveVertices(4, 6);
		if (alloc.vptr)
		{
			SceneVertex* vptr = alloc.vptr;
			uint32_t* iptr = alloc.iptr;
			uint32_t vpos = alloc.vpos;

			vptr[0] = { 0, vec3(-1.0f, -1.0f, 0.0f), zero2, zero2, zero2, zero2, color };
			vptr[1] = { 0, vec3(1.0f, -1.0f, 0.0f), zero2, zero2, zero2, zero2, color };
			vptr[2] = { 0, vec3(1.0f,  1.0f, 0.0f), zero2, zero2, zero2, zero2, color };
			vptr[3] = { 0, vec3(-1.0f,  1.0f, 0.0f), zero2, zero2, zero2, zero2, color };

			iptr[0] = vpos;
			iptr[1] = vpos + 1;
			iptr[2] = vpos + 2;
			iptr[3] = vpos;
			iptr[4] = vpos + 2;
			iptr[5] = vpos + 3;

			UseVertices(4, 6);
		}

		DrawBatches();
		if (CurrentFrame)
			SetSceneNode(CurrentFrame);
	}
	unguard;
}

void UD3D11RenderDevice::SetSceneNode(FSceneNode* Frame)
{
	guardSlow(UD3D11RenderDevice::SetSceneNode);

	DrawBatches();

	CurrentFrame = Frame;
	Aspect = Frame->FY / Frame->FX;
	RProjZ = (float)appTan(radians(Viewport->Actor->FovAngle) * 0.5);
	RFX2 = 2.0f * RProjZ / Frame->FX;
	RFY2 = 2.0f * RProjZ * Aspect / Frame->FY;

	SceneViewport = {};
	SceneViewport.TopLeftX = Frame->XB;
	SceneViewport.TopLeftY = SceneBuffers.Height - Frame->YB - Frame->Y;
	SceneViewport.Width = Frame->X;
	SceneViewport.Height = Frame->Y;
	SceneViewport.MinDepth = 0.1f;
	SceneViewport.MaxDepth = 1.0f;
	Context->RSSetViewports(1, &SceneViewport);

	SceneConstants.ObjectToProjection = mat4::frustum(-RProjZ, RProjZ, -Aspect * RProjZ, Aspect * RProjZ, 1.0f, 32768.0f, handedness::left, clipzrange::zero_positive_w);
	SceneConstants.NearClip = vec4(Frame->NearClip.X, Frame->NearClip.Y, Frame->NearClip.Z, Frame->NearClip.W);

	Context->UpdateSubresource(ScenePass.ConstantBuffer, 0, nullptr, &SceneConstants, 0, 0);

	unguardSlow;
}

void UD3D11RenderDevice::PrecacheTexture(FTextureInfo& Info, DWORD PolyFlags)
{
	guard(UD3D11RenderDevice::PrecacheTexture);
	PolyFlags = ApplyPrecedenceRules(PolyFlags);
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

	if (ActiveTimer)
		ActiveTimer->Unclock();
	Timers.DrawBatches.Clock();

	UnmapVertices();

	for (const DrawBatchEntry& entry : QueuedBatches)
		DrawEntry(entry);
	QueuedBatches.clear();

	MapVertices(nextBuffer);

	if (nextBuffer)
	{
		SceneVertexPos = 0;
		SceneIndexPos = 0;
		Stats.BuffersUsed++;
	}

	Batch.SceneIndexStart = SceneIndexPos;

	Timers.DrawBatches.Unclock();
	if (ActiveTimer)
		ActiveTimer->Clock();
}

void UD3D11RenderDevice::DrawEntry(const DrawBatchEntry& entry)
{
	size_t icount = entry.SceneIndexEnd - entry.SceneIndexStart;

	ID3D11ShaderResourceView* views[4] =
	{
		entry.Tex->View,
		entry.Lightmap->View,
		entry.Macrotex->View,
		entry.Detailtex->View
	};

	ID3D11SamplerState* samplers[4] =
	{
		ScenePass.Samplers[entry.TexSamplerMode],
		ScenePass.Samplers[0],
		ScenePass.Samplers[entry.MacrotexSamplerMode],
		ScenePass.Samplers[entry.DetailtexSamplerMode]
	};

	if (SceneViewport.MinDepth != entry.Pipeline->MinDepth || SceneViewport.MaxDepth != entry.Pipeline->MaxDepth)
	{
		SceneViewport.MinDepth = entry.Pipeline->MinDepth;
		SceneViewport.MaxDepth = entry.Pipeline->MaxDepth;
		Context->RSSetViewports(1, &SceneViewport);
	}

	Context->PSSetSamplers(0, 4, samplers);
	Context->PSSetShaderResources(0, 4, views);
	Context->PSSetShader(entry.Pipeline->PixelShader, nullptr, 0);

	Context->OMSetBlendState(entry.Pipeline->BlendState, nullptr, 0xffffffff);
	Context->OMSetDepthStencilState(entry.Pipeline->DepthStencilState, 0);

	Context->IASetPrimitiveTopology(entry.Pipeline->PrimitiveTopology);

	Context->DrawIndexed(icount, entry.SceneIndexStart, 0);
	Stats.DrawCalls++;
}

void UD3D11RenderDevice::CreateVertexShader(ComPtr<ID3D11VertexShader>& outShader, const std::string& shaderName, ComPtr<ID3D11InputLayout>& outInputLayout, const std::string& inputLayoutName, const std::vector<D3D11_INPUT_ELEMENT_DESC>& elements, const std::string& filename, const std::vector<std::string> defines)
{
	std::vector<uint8_t> bytecode = CompileHlsl(filename, "vs", defines);
	HRESULT result = Device->CreateVertexShader(bytecode.data(), bytecode.size(), nullptr, outShader.TypedInitPtr());
	ThrowIfFailed(result, ("CreateVertexShader(" + shaderName + ") failed").c_str());
	SetDebugName(outShader, shaderName.c_str());

	result = Device->CreateInputLayout(elements.data(), (UINT)elements.size(), bytecode.data(), bytecode.size(), outInputLayout.TypedInitPtr());
	ThrowIfFailed(result, ("CreateInputLayout(" + inputLayoutName + ") failed").c_str());
	SetDebugName(outInputLayout, inputLayoutName.c_str());
}

void UD3D11RenderDevice::CreatePixelShader(ComPtr<ID3D11PixelShader>& outShader, const std::string& shaderName, const std::string& filename, const std::vector<std::string> defines)
{
	std::vector<uint8_t> bytecode = CompileHlsl(filename, "ps", defines);
	HRESULT result = Device->CreatePixelShader(bytecode.data(), bytecode.size(), nullptr, outShader.TypedInitPtr());
	ThrowIfFailed(result, ("CreatePixelShader(" + shaderName + ") failed").c_str());
	SetDebugName(outShader, shaderName.c_str());
}

void UD3D11RenderDevice::SetDebugName(ID3D11Device* obj, const char* name)
{
	if (UseDebugLayer)
		obj->SetPrivateData(WKPDID_D3DDebugObjectName, strlen(name), name);
}

void UD3D11RenderDevice::SetDebugName(ID3D11DeviceChild* obj, const char* name)
{
	if (UseDebugLayer)
		obj->SetPrivateData(WKPDID_D3DDebugObjectName, strlen(name), name);
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

	ComPtr<ID3DBlob> blob;
	ComPtr<ID3DBlob> errors;
	HRESULT result = D3DCompile(code.data(), code.size(), filename.c_str(), macros.data(), nullptr, "main", target.c_str(), D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, blob.TypedInitPtr(), errors.TypedInitPtr());
	if (FAILED(result))
	{
		std::string msg((const char*)errors->GetBufferPointer(), errors->GetBufferSize());
		if (!msg.empty() && msg.back() == 0) msg.pop_back();
		throw std::runtime_error("Could not compile shader '" + filename + "':" + msg);
	}
	ThrowIfFailed(result, "D3DCompile failed");

	std::vector<uint8_t> bytecode;
	bytecode.resize(blob->GetBufferSize());
	memcpy(bytecode.data(), blob->GetBufferPointer(), bytecode.size());
	return bytecode;
}
