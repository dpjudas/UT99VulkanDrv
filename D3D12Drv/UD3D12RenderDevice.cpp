
#include "Precomp.h"
#include "UD3D12RenderDevice.h"
#include "CachedTexture.h"
#include "UTF16.h"
#include "FileResource.h"
#include "halffloat.h"

IMPLEMENT_CLASS(UD3D12RenderDevice);

UD3D12RenderDevice::UD3D12RenderDevice()
{
}

void UD3D12RenderDevice::StaticConstructor()
{
	guard(UD3D12RenderDevice::StaticConstructor);

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

int UD3D12RenderDevice::GetSettingsMultisample()
{
	switch (AntialiasMode)
	{
	default:
	case 0: return 0;
	case 1: return 2;
	case 2: return 4;
	}
}

UBOOL UD3D12RenderDevice::Init(UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen)
{
	guard(UD3D12RenderDevice::Init);

	Viewport = InViewport;
	ActiveHdr = Hdr;

	HDC screenDC = GetDC(0);
	DesktopResolution.Width = GetDeviceCaps(screenDC, HORZRES);
	DesktopResolution.Height = GetDeviceCaps(screenDC, VERTRES);
	ReleaseDC(0, screenDC);

	try
	{
		BufferCount = GetWantedSwapChainBufferCount();

		if (UseDebugLayer)
		{
			HRESULT result = D3D12GetDebugInterface(DebugController.GetIID(), DebugController.InitPtr());
			if (SUCCEEDED(result))
			{
				DebugController->EnableDebugLayer();
			}
		}

		ComPtr<IDXGIFactory4> factory;
		HRESULT result = CreateDXGIFactory1(factory.GetIID(), factory.InitPtr());
		ThrowIfFailed(result, "CreateDXGIFactory1 failed");

		ComPtr<IDXGIAdapter1> hardwareAdapter;
		result = factory->EnumAdapters1(0, hardwareAdapter.TypedInitPtr());
		ThrowIfFailed(result, "EnumAdapters1 failed");

		result = D3D12CreateDevice(hardwareAdapter, D3D_FEATURE_LEVEL_11_0, Device.GetIID(), Device.InitPtr());
		ThrowIfFailed(result, "D3D12CreateDevice failed");

		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		result = Device->CreateCommandQueue(&queueDesc, GraphicsQueue.GetIID(), GraphicsQueue.InitPtr());
		ThrowIfFailed(result, "CreateCommandQueue failed");

		if (UseDebugLayer)
		{
			result = Device->QueryInterface(InfoQueue1.GetIID(), InfoQueue1.InitPtr());
			if (SUCCEEDED(result))
			{
				result = InfoQueue1->RegisterMessageCallback(&UD3D12RenderDevice::OnDebugMessage, D3D12_MESSAGE_CALLBACK_FLAG_NONE, this, &DebugMessageCookie);
				if (SUCCEEDED(result))
				{
					DebugMessageActive = true;
				}
				else
				{
					debugf(TEXT("D3D12InfoQueue1.RegisterMessageCallback failed"));
				}
			}
			else
			{
				debugf(TEXT("Could not aquire ID3D12InfoQueue1 object"));
			}
		}

		D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
		allocatorDesc.pDevice = Device.get();
		allocatorDesc.pAdapter = hardwareAdapter.get();
		allocatorDesc.Flags = D3D12MA::ALLOCATOR_FLAG_MSAA_TEXTURES_ALWAYS_COMMITTED | D3D12MA::ALLOCATOR_FLAG_DEFAULT_POOLS_NOT_ZEROED;
		result = D3D12MA::CreateAllocator(&allocatorDesc, &MemAllocator);
		ThrowIfFailed(result, "D3D12MA::CreateAllocator failed");

		ComPtr<IDXGIFactory5> dxgiFactory5;
		result = factory->QueryInterface(dxgiFactory5.GetIID(), dxgiFactory5.InitPtr());
		if (SUCCEEDED(result))
		{
			INT support = 0;
			result = dxgiFactory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &support, sizeof(INT));
			if (SUCCEEDED(result))
			{
				DxgiSwapChainAllowTearing = support != 0;
			}
		}

		UINT flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		if (DxgiSwapChainAllowTearing)
			flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

		DXGI_SWAP_CHAIN_DESC1 swapDesc = {};
		swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapDesc.Width = NewX;
		swapDesc.Height = NewY;
		swapDesc.Format = ActiveHdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
		swapDesc.BufferCount = BufferCount;
		swapDesc.SampleDesc.Count = 1;
		swapDesc.Scaling = DXGI_SCALING_STRETCH;
		swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapDesc.Flags = flags;
		swapDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

		ComPtr<IDXGISwapChain1> swapChain1;
		result = factory->CreateSwapChainForHwnd(GraphicsQueue, (HWND)Viewport->GetWindow(), &swapDesc, nullptr, nullptr, swapChain1.TypedInitPtr());
		ThrowIfFailed(result, "CreateSwapChainForHwnd failed");

		result = swapChain1->QueryInterface(SwapChain3.GetIID(), SwapChain3.InitPtr());
		ThrowIfFailed(result, "QueryInterface(IID_SwapChain3) failed");

		SetColorSpace();

		result = factory->MakeWindowAssociation((HWND)Viewport->GetWindow(), DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER);
		ThrowIfFailed(result, "MakeWindowAssociation failed");

		Heaps.Common = std::make_unique<DescriptorHeap>(Device.get(), 64 * 1024, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
		Heaps.Sampler = std::make_unique<DescriptorHeap>(Device.get(), 64, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
		Heaps.RTV = std::make_unique<DescriptorHeap>(Device.get(), 64, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
		Heaps.DSV = std::make_unique<DescriptorHeap>(Device.get(), 64, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);

		for (int i = 0; i < 2; i++)
		{
			result = Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, Commands.Batches[i].TransferAllocator.GetIID(), Commands.Batches[i].TransferAllocator.InitPtr());
			ThrowIfFailed(result, "CreateCommandAllocator failed");

			result = Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, Commands.Batches[i].DrawAllocator.GetIID(), Commands.Batches[i].DrawAllocator.InitPtr());
			ThrowIfFailed(result, "CreateCommandAllocator failed");

			result = Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, Commands.Batches[i].TransferAllocator, nullptr, Commands.Batches[i].Transfer.GetIID(), Commands.Batches[i].Transfer.InitPtr());
			ThrowIfFailed(result, "CreateCommandList failed");

			result = Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, Commands.Batches[i].DrawAllocator, nullptr, Commands.Batches[i].Draw.GetIID(), Commands.Batches[i].Draw.InitPtr());
			ThrowIfFailed(result, "CreateCommandList failed");

			ID3D12DescriptorHeap* heaps[] = { Heaps.Common->GetHeap(), Heaps.Sampler->GetHeap() };
			Commands.Batches[i].Draw->SetDescriptorHeaps(2, heaps);
		}

		result = Device->CreateFence(Commands.FenceValue, D3D12_FENCE_FLAG_NONE, Commands.Fence.GetIID(), Commands.Fence.InitPtr());
		ThrowIfFailed(result, "CreateFence failed");

		Commands.FenceHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (Commands.FenceHandle == INVALID_HANDLE_VALUE)
			throw std::runtime_error("CreateEvent failed");

		Commands.Current = &Commands.Batches[0];

		CreateUploadBuffer();
		CreatePresentPass();
		CreateBloomPass();

		Textures.reset(new TextureManager(this));
		Uploads.reset(new UploadManager(this));
	}
	catch (_com_error error)
	{
		debugf(TEXT("Could not create d3d12 renderer: [_com_error] %s"), error.ErrorMessage());
		Exit();
		return 0;
	}
	catch (const std::exception& e)
	{
		debugf(TEXT("Could not create d3d12 renderer: %s"), to_utf16(e.what()).c_str());
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

void UD3D12RenderDevice::SetColorSpace()
{
	if (ActiveHdr)
	{
		UINT support = 0;
		HRESULT result = SwapChain3->CheckColorSpaceSupport(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020, &support);
		if (SUCCEEDED(result) && (support & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) == DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT)
		{
			result = SwapChain3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
			if (FAILED(result))
			{
				debugf(TEXT("D3D12Drv: IDXGISwapChain3.SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020) failed"));
			}
		}
		else
		{
			debugf(TEXT("D3D12Drv: Swap chain does not support DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020"));
		}
	}
}

class SetResCallLock
{
public:
	SetResCallLock(bool& value) : value(value)
	{
		value = true;
	}
	~SetResCallLock()
	{
		value = false;
	}
	bool& value;
};

UBOOL UD3D12RenderDevice::SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen)
{
	guard(UD3D12RenderDevice::SetRes);

	if (InSetResCall)
		return TRUE;
	SetResCallLock lock(InSetResCall);

	if (NewX == 0 || NewY == 0)
		return 1;

	SubmitCommands(false);
	WaitDeviceIdle();
	ReleaseSwapChainResources();

	HRESULT result;

	BufferCount = GetWantedSwapChainBufferCount();

	if (!Fullscreen && FullscreenState.Enabled) // Leaving fullscreen
	{
		// Restore old state
		SetWindowLong((HWND)Viewport->GetWindow(), GWL_STYLE, FullscreenState.Style);
		SetWindowLong((HWND)Viewport->GetWindow(), GWL_EXSTYLE, FullscreenState.ExStyle);
		SetWindowPos(
			(HWND)Viewport->GetWindow(),
			HWND_TOP,
			FullscreenState.WindowPos.left,
			FullscreenState.WindowPos.top,
			FullscreenState.WindowPos.right - FullscreenState.WindowPos.left,
			FullscreenState.WindowPos.bottom - FullscreenState.WindowPos.top,
			SWP_FRAMECHANGED | SWP_NOSENDCHANGING | SWP_NOACTIVATE | SWP_NOZORDER);

		FullscreenState.Enabled = false;
	}

	if (!Viewport->ResizeViewport(Fullscreen ? (BLIT_Fullscreen | BLIT_Direct3D) : (BLIT_HardwarePaint | BLIT_Direct3D), NewX, NewY, NewColorBytes))
	{
		debugf(TEXT("Viewport.ResizeViewport failed (%d, %d, %d, %d)"), NewX, NewY, NewColorBytes, (INT)Fullscreen);
		return FALSE;
	}

	CurrentSizeX = NewX;
	CurrentSizeY = NewY;

	if (Fullscreen && !FullscreenState.Enabled) // Entering fullscreen
	{
		// Save old state
		GetWindowRect((HWND)Viewport->GetWindow(), &FullscreenState.WindowPos);
		FullscreenState.Style = GetWindowLong((HWND)Viewport->GetWindow(), GWL_STYLE);
		FullscreenState.ExStyle = GetWindowLong((HWND)Viewport->GetWindow(), GWL_EXSTYLE);

		// Find primary monitor resolution
		HDC screenDC = GetDC(0);
		int screenWidth = GetDeviceCaps(screenDC, HORZRES);
		int screenHeight = GetDeviceCaps(screenDC, VERTRES);
		ReleaseDC(0, screenDC);

		// Create borderless full screen window (our present shader will letterbox any resolution to fit)
		SetWindowLong((HWND)Viewport->GetWindow(), GWL_STYLE, WS_OVERLAPPED | WS_VISIBLE);
		SetWindowLong((HWND)Viewport->GetWindow(), GWL_EXSTYLE, WS_EX_APPWINDOW);
		SetWindowPos((HWND)Viewport->GetWindow(), HWND_TOP, 0, 0, screenWidth, screenHeight, SWP_FRAMECHANGED | SWP_NOSENDCHANGING | SWP_NOACTIVATE | SWP_NOZORDER);

		DXGI_MODE_DESC modeDesc = {};
		modeDesc.Width = screenWidth;
		modeDesc.Height = screenHeight;
		modeDesc.Format = ActiveHdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
		result = SwapChain3->ResizeTarget(&modeDesc);
		if (FAILED(result))
		{
			debugf(TEXT("SwapChain.ResizeTarget failed (%d, %d, %d, %d)"), NewX, NewY, NewColorBytes, (INT)Fullscreen);
		}

		FullscreenState.Enabled = true;
	}

	if (!Fullscreen)
	{
		DXGI_MODE_DESC modeDesc = {};
		modeDesc.Width = CurrentSizeX;
		modeDesc.Height = CurrentSizeY;
		modeDesc.Format = ActiveHdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
		result = SwapChain3->ResizeTarget(&modeDesc);
		if (FAILED(result))
		{
			debugf(TEXT("SwapChain.ResizeTarget failed (%d, %d, %d, %d)"), NewX, NewY, NewColorBytes, (INT)Fullscreen);
		}
	}

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

void UD3D12RenderDevice::ReleaseSwapChainResources()
{
	FrameBuffers.clear();
	FrameBufferRTVs.reset();
	FrameFenceValues.clear();
}

bool UD3D12RenderDevice::UpdateSwapChain()
{
	int width = CurrentSizeX;
	int height = CurrentSizeY;
	if (FullscreenState.Enabled)
	{
		HDC screenDC = GetDC(0);
		width = GetDeviceCaps(screenDC, HORZRES);
		height = GetDeviceCaps(screenDC, VERTRES);
		ReleaseDC(0, screenDC);
	}

	UINT flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	if (DxgiSwapChainAllowTearing)
		flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

	HRESULT result = SwapChain3->ResizeBuffers(BufferCount, width, height, ActiveHdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM, flags);
	if (FAILED(result))
		return false;

	SetColorSpace();

	if (CurrentSizeX && CurrentSizeY)
	{
		try
		{
			ResizeSceneBuffers(CurrentSizeX, CurrentSizeY, GetSettingsMultisample());
		}
		catch (const std::exception& e)
		{
			debugf(TEXT("Could not resize scene buffers: %s"), to_utf16(e.what()).c_str());
			return false;
		}
	}

	for (int i = 0; i < BufferCount; i++)
	{
		ComPtr<ID3D12Resource> buffer;
		HRESULT result = SwapChain3->GetBuffer(i, buffer.GetIID(), buffer.InitPtr());
		if (FAILED(result))
		{
			debugf(TEXT("SwapChain3.GetBuffer failed"));
			return false;
		}
		FrameBuffers.push_back(std::move(buffer));
	}

	FrameBufferRTVs = Heaps.RTV->Alloc(BufferCount);
	for (int i = 0; i < BufferCount; i++)
	{
		Device->CreateRenderTargetView(FrameBuffers[i], nullptr, FrameBufferRTVs.CPUHandle(i));
	}

	FrameFenceValues.clear();
	FrameFenceValues.resize(FrameBuffers.size());
	BackBufferIndex = SwapChain3->GetCurrentBackBufferIndex();

	return true;
}

void UD3D12RenderDevice::SubmitCommands(bool present)
{
	HRESULT result = Commands.Current->Transfer->Close();
	ThrowIfFailed(result, "Could not close transfer command list!");

	result = Commands.Current->Draw->Close();
	ThrowIfFailed(result, "Could not close draw command list!");

	ID3D12CommandList* commandLists[] = { Commands.Current->Transfer.get(), Commands.Current->Draw.get() };
	GraphicsQueue->ExecuteCommandLists(2, commandLists);

	if (present)
	{
		UINT flags = 0;
		if (!UseVSync && DxgiSwapChainAllowTearing)
			flags |= DXGI_PRESENT_ALLOW_TEARING;

		DXGI_PRESENT_PARAMETERS presentParams = {};
		result = SwapChain3->Present1(UseVSync ? 1 : 0, flags, &presentParams);
	}

	result = GraphicsQueue->Signal(Commands.Fence, Commands.FenceValue + 1);
	ThrowIfFailed(result, "GraphicsQueue.Signal failed");
	Commands.FenceValue++;
	Commands.Current->FenceValue = Commands.FenceValue;

	if (present)
	{
		FrameFenceValues[BackBufferIndex] = Commands.FenceValue;
		BackBufferIndex = SwapChain3->GetCurrentBackBufferIndex();
		WaitForFence(FrameFenceValues[BackBufferIndex]);
	}

	UINT64 nextBatch = Commands.FenceValue % 2;
	Commands.Current = &Commands.Batches[nextBatch];

	if (Commands.Current->FenceValue > 0)
	{
		WaitForFence(Commands.Current->FenceValue);
		Commands.Current->TransferAllocator->Reset();
		Commands.Current->DrawAllocator->Reset();
		Commands.Current->Transfer->Reset(Commands.Current->TransferAllocator, nullptr);
		Commands.Current->Draw->Reset(Commands.Current->DrawAllocator, nullptr);

		ID3D12DescriptorHeap* heaps[] = { Heaps.Common->GetHeap(), Heaps.Sampler->GetHeap() };
		Commands.Current->Draw->SetDescriptorHeaps(2, heaps);
	}

	Upload.Pos = 0;
	Upload.Base = nextBatch * Upload.Size;
	ScenePass.VertexPos = 0;
	ScenePass.VertexBase = nextBatch * ScenePass.VertexBufferSize;
	ScenePass.IndexPos = 0;
	ScenePass.IndexBase = nextBatch * ScenePass.IndexBufferSize;
	Stats.BuffersUsed++;
}

void UD3D12RenderDevice::WaitForFence(UINT64 value)
{
	if (!Commands.Fence)
		return;
	UINT64 completedValue = Commands.Fence->GetCompletedValue();
	if (completedValue < value)
	{
		HRESULT result = Commands.Fence->SetEventOnCompletion(value, Commands.FenceHandle);
		ThrowIfFailed(result, "GraphicsQueue.SetEventOnCompletion failed");
		WaitForSingleObjectEx(Commands.FenceHandle, INFINITE, FALSE);
	}
}

void UD3D12RenderDevice::WaitDeviceIdle()
{
	WaitForFence(Commands.FenceValue);
}

void UD3D12RenderDevice::TransitionResourceBarrier(ID3D12GraphicsCommandList* cmdlist, ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = resource;
	barrier.Transition.StateBefore = before;
	barrier.Transition.StateAfter = after;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	cmdlist->ResourceBarrier(1, &barrier);
}

void UD3D12RenderDevice::TransitionResourceBarrier(ID3D12GraphicsCommandList* cmdlist, ID3D12Resource* resource0, D3D12_RESOURCE_STATES before0, D3D12_RESOURCE_STATES after0, ID3D12Resource* resource1, D3D12_RESOURCE_STATES before1, D3D12_RESOURCE_STATES after1)
{
	D3D12_RESOURCE_BARRIER barriers[2] = {};
	barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[0].Transition.pResource = resource0;
	barriers[0].Transition.StateBefore = before0;
	barriers[0].Transition.StateAfter = after0;
	barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[1].Transition.pResource = resource1;
	barriers[1].Transition.StateBefore = before1;
	barriers[1].Transition.StateAfter = after1;
	barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	cmdlist->ResourceBarrier(2, barriers);
}

void UD3D12RenderDevice::Exit()
{
	guard(UD3D12RenderDevice::Exit);

	WaitDeviceIdle();

	Uploads.reset();
	Textures.reset();
	ReleasePresentPass();
	ReleaseBloomPass();
	ReleaseScenePass();
	ReleaseSceneBuffers();
	ReleaseUploadBuffer();
	Heaps.DSV.reset();
	Heaps.RTV.reset();
	Heaps.Sampler.reset();
	Heaps.Common.reset();
	FrameBuffers.clear();
	SwapChain3.reset();
	for (int i = 0; i < 2; i++)
	{
		Commands.Batches[i].Draw.reset();
		Commands.Batches[i].Transfer.reset();
		Commands.Batches[i].DrawAllocator.reset();
		Commands.Batches[i].TransferAllocator.reset();
	}
	Commands.Fence.reset();
	GraphicsQueue.reset();

	if (Commands.FenceHandle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(Commands.FenceHandle);
		Commands.FenceHandle = INVALID_HANDLE_VALUE;
	}

	if (MemAllocator)
		MemAllocator->Release();
	MemAllocator = nullptr;

	if (DebugMessageActive)
		InfoQueue1->UnregisterMessageCallback(DebugMessageCookie);
	InfoQueue1.reset();

	Device.reset();
	DebugController.reset();

	unguard;
}

void UD3D12RenderDevice::ResizeSceneBuffers(int width, int height, int multisample)
{
	multisample = std::max(multisample, 1);

	if (SceneBuffers.Width == width && SceneBuffers.Height == height && multisample == SceneBuffers.Multisample && SceneBuffers.ColorBuffer && SceneBuffers.HitBuffer && SceneBuffers.PPHitBuffer && SceneBuffers.StagingHitBuffer && SceneBuffers.DepthBuffer && SceneBuffers.PPImage[0] && SceneBuffers.PPImage[1])
		return;

	SubmitCommands(false);
	WaitDeviceIdle();
	ReleaseSceneBuffers();

	SceneBuffers.Width = width;
	SceneBuffers.Height = height;
	SceneBuffers.Multisample = multisample;

	D3D12_HEAP_PROPERTIES defaultHeapProps = {};
	defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_CLEAR_VALUE clearValue = {}, clearValueInt = {}, depthValue = {};
	clearValue.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	clearValueInt.Format = DXGI_FORMAT_R32_UINT;
	depthValue.Format = DXGI_FORMAT_D32_FLOAT;
	depthValue.DepthStencil.Depth = 1.0f;

	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	texDesc.Width = SceneBuffers.Width;
	texDesc.Height = SceneBuffers.Height;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.SampleDesc.Count = SceneBuffers.Multisample;
	texDesc.SampleDesc.Quality = 0; // SceneBuffers.Multisample > 1 ? D3D12_STANDARD_MULTISAMPLE_PATTERN : 0;

	texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	HRESULT result = Device->CreateCommittedResource(
		&defaultHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&clearValue,
		SceneBuffers.ColorBuffer.GetIID(),
		SceneBuffers.ColorBuffer.InitPtr());
	ThrowIfFailed(result, "CreateCommittedResource(SceneBuffers.ColorBuffer) failed");
	SceneBuffers.ColorBuffer->SetName(TEXT("SceneBuffers.ColorBuffer"));

	texDesc.Format = DXGI_FORMAT_R32_UINT;
	result = Device->CreateCommittedResource(
		&defaultHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&clearValueInt,
		SceneBuffers.HitBuffer.GetIID(),
		SceneBuffers.HitBuffer.InitPtr());
	ThrowIfFailed(result, "CreateCommittedResource(SceneBuffers.HitBuffer) failed");
	SceneBuffers.HitBuffer->SetName(TEXT("SceneBuffers.HitBuffer"));

	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	texDesc.Format = DXGI_FORMAT_D32_FLOAT;
	result = Device->CreateCommittedResource(
		&defaultHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthValue,
		SceneBuffers.DepthBuffer.GetIID(),
		SceneBuffers.DepthBuffer.InitPtr());
	ThrowIfFailed(result, "CreateCommittedResource(SceneBuffers.DepthBuffer) failed");
	SceneBuffers.DepthBuffer->SetName(TEXT("SceneBuffers.DepthBuffer"));

	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;

	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	texDesc.Format = DXGI_FORMAT_R32_UINT;
	result = Device->CreateCommittedResource(
		&defaultHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		nullptr,
		SceneBuffers.PPHitBuffer.GetIID(),
		SceneBuffers.PPHitBuffer.InitPtr());
	ThrowIfFailed(result, "CreateCommittedResource(SceneBuffers.PPHitBuffer) failed");
	SceneBuffers.PPHitBuffer->SetName(TEXT("SceneBuffers.PPHitBuffer"));

	texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	for (int i = 0; i < 2; i++)
	{
		result = Device->CreateCommittedResource(
			&defaultHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			SceneBuffers.PPImage[i].GetIID(),
			SceneBuffers.PPImage[i].InitPtr());
		ThrowIfFailed(result, "CreateCommittedResource(SceneBuffers.PPImage) failed");
		SceneBuffers.PPImage[i]->SetName(TEXT("SceneBuffers.PPImage"));
	}

	SceneBuffers.SceneRTVs = Heaps.RTV->Alloc(2);
	Device->CreateRenderTargetView(SceneBuffers.ColorBuffer, nullptr, SceneBuffers.SceneRTVs.CPUHandle(0));
	Device->CreateRenderTargetView(SceneBuffers.HitBuffer, nullptr, SceneBuffers.SceneRTVs.CPUHandle(1));

	SceneBuffers.SceneDSV = Heaps.DSV->Alloc(1);
	Device->CreateDepthStencilView(SceneBuffers.DepthBuffer, nullptr, SceneBuffers.SceneDSV.CPUHandle());

	SceneBuffers.HitBufferSRV = Heaps.Common->Alloc(1);
	Device->CreateShaderResourceView(SceneBuffers.HitBuffer, nullptr, SceneBuffers.HitBufferSRV.CPUHandle());

	SceneBuffers.PPHitBufferRTV = Heaps.RTV->Alloc(1);
	Device->CreateRenderTargetView(SceneBuffers.PPHitBuffer, nullptr, SceneBuffers.PPHitBufferRTV.CPUHandle());

	for (int i = 0; i < 2; i++)
	{
		SceneBuffers.PPImageRTV[i] = Heaps.RTV->Alloc(1);
		SceneBuffers.PPImageSRV[i] = Heaps.Common->Alloc(1);
		Device->CreateRenderTargetView(SceneBuffers.PPImage[i], nullptr, SceneBuffers.PPImageRTV[i].CPUHandle());
		Device->CreateShaderResourceView(SceneBuffers.PPImage[i], nullptr, SceneBuffers.PPImageSRV[i].CPUHandle());
	}

	SceneBuffers.PresentSRVs = Heaps.Common->Alloc(2);
	Device->CreateShaderResourceView(SceneBuffers.PPImage[0], nullptr, SceneBuffers.PresentSRVs.CPUHandle(0));
	Device->CreateShaderResourceView(PresentPass.DitherTexture, nullptr, SceneBuffers.PresentSRVs.CPUHandle(1));

	int bloomWidth = width;
	int bloomHeight = height;
	for (PPBlurLevel& level : SceneBuffers.BlurLevels)
	{
		bloomWidth = (bloomWidth + 1) / 2;
		bloomHeight = (bloomHeight + 1) / 2;

		texDesc.Width = bloomWidth;
		texDesc.Height = bloomHeight;

		result = Device->CreateCommittedResource(
			&defaultHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			level.VTexture.GetIID(),
			level.VTexture.InitPtr());
		ThrowIfFailed(result, "CreateCommittedResource(SceneBuffers.BlurLevels.VTexture) failed");
		level.VTexture->SetName(TEXT("SceneBuffers.BlurLevels.VTexture"));

		result = Device->CreateCommittedResource(
			&defaultHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			level.HTexture.GetIID(),
			level.HTexture.InitPtr());
		ThrowIfFailed(result, "CreateCommittedResource(SceneBuffers.BlurLevels.HTexture) failed");
		level.HTexture->SetName(TEXT("SceneBuffers.BlurLevels.HTexture"));

		level.VTextureRTV = Heaps.RTV->Alloc(1);
		level.HTextureRTV = Heaps.RTV->Alloc(1);
		level.VTextureSRV = Heaps.Common->Alloc(1);
		level.HTextureSRV = Heaps.Common->Alloc(1);

		Device->CreateRenderTargetView(level.VTexture, nullptr, level.VTextureRTV.CPUHandle());
		Device->CreateRenderTargetView(level.HTexture, nullptr, level.HTextureRTV.CPUHandle());
		Device->CreateShaderResourceView(level.VTexture, nullptr, level.VTextureSRV.CPUHandle());
		Device->CreateShaderResourceView(level.HTexture, nullptr, level.HTextureSRV.CPUHandle());

		level.Width = bloomWidth;
		level.Height = bloomHeight;
	}

	D3D12_HEAP_PROPERTIES readbackHeapProps = {};
	readbackHeapProps.Type = D3D12_HEAP_TYPE_READBACK;

	D3D12_RESOURCE_DESC desc = SceneBuffers.PPHitBuffer->GetDesc();
	UINT64 totalSize = 0, rowSizeInBytes = 0;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
	UINT numRows = 0;
	Device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalSize);

	D3D12_RESOURCE_DESC bufDesc = {};
	bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufDesc.Width = totalSize;
	bufDesc.Height = 1;
	bufDesc.DepthOrArraySize = 1;
	bufDesc.MipLevels = 1;
	bufDesc.SampleDesc.Count = 1;
	bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	result = Device->CreateCommittedResource(
		&readbackHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		SceneBuffers.StagingHitBuffer.GetIID(),
		SceneBuffers.StagingHitBuffer.InitPtr());
	ThrowIfFailed(result, "CreateCommittedResource(SceneBuffers.StagingHitBuffer) failed");
	SceneBuffers.StagingHitBuffer->SetName(TEXT("SceneBuffers.StagingHitBuffer"));
}

void UD3D12RenderDevice::CreateUploadBuffer()
{
	D3D12_HEAP_PROPERTIES uploadHeapProps = { D3D12_HEAP_TYPE_UPLOAD };

	D3D12_RESOURCE_DESC bufDesc = {};
	bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufDesc.Width = Upload.Size * 2;
	bufDesc.Height = 1;
	bufDesc.DepthOrArraySize = 1;
	bufDesc.MipLevels = 1;
	bufDesc.SampleDesc.Count = 1;
	bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	HRESULT result = Device->CreateCommittedResource(
		&uploadHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		Upload.Buffer.GetIID(),
		Upload.Buffer.InitPtr());
	ThrowIfFailed(result, "CreateCommittedResource(Upload.Buffer) failed");
	Upload.Buffer->SetName(TEXT("Upload.Buffer"));

	D3D12_RANGE readRange = {};
	result = Upload.Buffer->Map(0, &readRange, (void**)&Upload.Data);
	ThrowIfFailed(result, "Map(Upload.Buffer) failed");
}

void UD3D12RenderDevice::ReleaseUploadBuffer()
{
	if (Upload.Data)
	{
		Upload.Buffer->Unmap(0, nullptr);
		Upload.Data = nullptr;
	}

	Upload.Buffer.reset();
}

void UD3D12RenderDevice::CreateScenePass()
{
	std::vector<D3D12_INPUT_ELEMENT_DESC> elements =
	{
		{ "AttrFlags", 0, DXGI_FORMAT_R32_UINT, 0, offsetof(SceneVertex, Flags), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "AttrPos", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(SceneVertex, Position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "AttrTexCoordOne", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(SceneVertex, TexCoord), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "AttrTexCoordTwo", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(SceneVertex, TexCoord2), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "AttrTexCoordThree", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(SceneVertex, TexCoord3), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "AttrTexCoordFour", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(SceneVertex, TexCoord4), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "AttrColor", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(SceneVertex, Color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	auto vertexShader = CompileHlsl("shaders/Scene.vert", "vs");
	auto pixelShader = CompileHlsl("shaders/Scene.frag", "ps");
	auto pixelShaderAlphaTest = CompileHlsl("shaders/Scene.frag", "ps", { "ALPHATEST" });

	CreateSceneSamplers();

	std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> descriptorTables(2);

	D3D12_DESCRIPTOR_RANGE texRange = {};
	texRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	texRange.BaseShaderRegister = 0;
	texRange.NumDescriptors = 4;
	descriptorTables[0].push_back(texRange);

	D3D12_DESCRIPTOR_RANGE samplerRange = {};
	samplerRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
	samplerRange.BaseShaderRegister = 0;
	samplerRange.NumDescriptors = 4;
	descriptorTables[1].push_back(samplerRange);

	D3D12_ROOT_CONSTANTS pushConstants = {};
	pushConstants.ShaderRegister = 0;
	pushConstants.Num32BitValues = sizeof(ScenePushConstants) / sizeof(int32_t);

	ScenePass.RootSignature = CreateRootSignature("ScenePass.RootSignature", descriptorTables, pushConstants);

	D3D12_RASTERIZER_DESC rasterizerState = {};
	rasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerState.FrontCounterClockwise = FALSE;
	rasterizerState.DepthClipEnable = FALSE; // Avoid clipping the weapon. The UE1 engine clips the geometry anyway.
	rasterizerState.MultisampleEnable = SceneBuffers.Multisample > 1 ? TRUE : FALSE;

	for (int i = 0; i < 32; i++)
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = ScenePass.RootSignature;
		psoDesc.InputLayout.NumElements = elements.size();
		psoDesc.InputLayout.pInputElementDescs = elements.data();
		psoDesc.VS.pShaderBytecode = vertexShader.data();
		psoDesc.VS.BytecodeLength = vertexShader.size();

		if (i & 16) // PF_Masked
		{
			psoDesc.PS.pShaderBytecode = pixelShaderAlphaTest.data();
			psoDesc.PS.BytecodeLength = pixelShaderAlphaTest.size();
		}
		else
		{
			psoDesc.PS.pShaderBytecode = pixelShader.data();
			psoDesc.PS.BytecodeLength = pixelShader.size();
		}

		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.RasterizerState = rasterizerState;
		psoDesc.SampleDesc.Count = SceneBuffers.Multisample;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.NumRenderTargets = 2;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
		psoDesc.RTVFormats[1] = DXGI_FORMAT_R32_UINT;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

		psoDesc.BlendState.IndependentBlendEnable = TRUE;
		psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
		switch (i & 3)
		{
		case 0: // PF_Translucent
			psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
			psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_COLOR;
			psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
			break;
		case 1: // PF_Modulated
			psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_DEST_COLOR;
			psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_DEST_ALPHA;
			psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_SRC_COLOR;
			psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_SRC_ALPHA;
			break;
		case 2: // PF_Highlighted
			psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
			psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
			break;
		case 3: // Hmm, is it faster to keep the blend mode enabled or to toggle it?
			psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
			psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
			psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
			break;
		}
		if (i & 4) // PF_Invisible
			psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0;
		else
			psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		psoDesc.BlendState.RenderTarget[1].BlendEnable = FALSE;
		psoDesc.BlendState.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		psoDesc.DepthStencilState.DepthEnable = TRUE;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		if (i & 8) // PF_Occlude
			psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		else
			psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

		HRESULT result = Device->CreateGraphicsPipelineState(&psoDesc, ScenePass.Pipelines[i].Pipeline.GetIID(), ScenePass.Pipelines[i].Pipeline.InitPtr());
		ThrowIfFailed(result, "CreateGraphicsPipelineState failed");
	}

	// Line pipeline
	for (int i = 0; i < 2; i++)
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = ScenePass.RootSignature;
		psoDesc.InputLayout.NumElements = elements.size();
		psoDesc.InputLayout.pInputElementDescs = elements.data();
		psoDesc.VS.pShaderBytecode = vertexShader.data();
		psoDesc.VS.BytecodeLength = vertexShader.size();
		psoDesc.PS.pShaderBytecode = pixelShader.data();
		psoDesc.PS.BytecodeLength = pixelShader.size();
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		psoDesc.RasterizerState = rasterizerState;
		psoDesc.SampleDesc.Count = SceneBuffers.Multisample;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.NumRenderTargets = 2;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
		psoDesc.RTVFormats[1] = DXGI_FORMAT_R32_UINT;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

		psoDesc.BlendState.IndependentBlendEnable = TRUE;
		psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
		psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		psoDesc.BlendState.RenderTarget[1].BlendEnable = FALSE;
		psoDesc.BlendState.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		psoDesc.DepthStencilState.DepthEnable = TRUE;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

		HRESULT result = Device->CreateGraphicsPipelineState(&psoDesc, ScenePass.LinePipeline[i].Pipeline.GetIID(), ScenePass.LinePipeline[i].Pipeline.InitPtr());
		ThrowIfFailed(result, "CreateGraphicsPipelineState failed");

		if (i == 0)
		{
			ScenePass.LinePipeline[i].MinDepth = 0.0f;
			ScenePass.LinePipeline[i].MaxDepth = 0.1f;
		}
	}

	// Point pipeline
	for (int i = 0; i < 2; i++)
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = ScenePass.RootSignature;
		psoDesc.InputLayout.NumElements = elements.size();
		psoDesc.InputLayout.pInputElementDescs = elements.data();
		psoDesc.VS.pShaderBytecode = vertexShader.data();
		psoDesc.VS.BytecodeLength = vertexShader.size();
		psoDesc.PS.pShaderBytecode = pixelShader.data();
		psoDesc.PS.BytecodeLength = pixelShader.size();
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.RasterizerState = rasterizerState;
		psoDesc.SampleDesc.Count = SceneBuffers.Multisample;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.NumRenderTargets = 2;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
		psoDesc.RTVFormats[1] = DXGI_FORMAT_R32_UINT;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

		psoDesc.BlendState.IndependentBlendEnable = TRUE;
		psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
		psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		psoDesc.BlendState.RenderTarget[1].BlendEnable = FALSE;
		psoDesc.BlendState.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		psoDesc.DepthStencilState.DepthEnable = TRUE;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

		HRESULT result = Device->CreateGraphicsPipelineState(&psoDesc, ScenePass.PointPipeline[i].Pipeline.GetIID(), ScenePass.PointPipeline[i].Pipeline.InitPtr());
		ThrowIfFailed(result, "CreateGraphicsPipelineState failed");

		if (i == 0)
		{
			ScenePass.PointPipeline[i].MinDepth = 0.0f;
			ScenePass.PointPipeline[i].MaxDepth = 0.1f;
		}
	}

	D3D12_HEAP_PROPERTIES uploadHeapProps = { D3D12_HEAP_TYPE_UPLOAD };
	D3D12_HEAP_PROPERTIES defaultHeapProps = { D3D12_HEAP_TYPE_DEFAULT };

	D3D12_RESOURCE_DESC bufDesc = {};
	bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufDesc.Width = ScenePass.VertexBufferSize * 2 * sizeof(SceneVertex);
	bufDesc.Height = 1;
	bufDesc.DepthOrArraySize = 1;
	bufDesc.MipLevels = 1;
	bufDesc.SampleDesc.Count = 1;
	bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	HRESULT result = Device->CreateCommittedResource(
		&uploadHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		ScenePass.VertexBuffer.GetIID(),
		ScenePass.VertexBuffer.InitPtr());
	ThrowIfFailed(result, "CreateCommittedResource(ScenePass.VertexBuffer) failed");
	ScenePass.VertexBuffer->SetName(TEXT("ScenePass.VertexBuffer"));

	bufDesc.Width = ScenePass.IndexBufferSize * 2 * sizeof(uint32_t);
	result = Device->CreateCommittedResource(
		&uploadHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		ScenePass.IndexBuffer.GetIID(),
		ScenePass.IndexBuffer.InitPtr());
	ThrowIfFailed(result, "CreateCommittedResource(ScenePass.IndexBuffer) failed");
	ScenePass.IndexBuffer->SetName(TEXT("ScenePass.IndexBuffer"));

	ScenePass.VertexBufferView.StrideInBytes = sizeof(SceneVertex);
	ScenePass.VertexBufferView.BufferLocation = ScenePass.VertexBuffer->GetGPUVirtualAddress();
	ScenePass.VertexBufferView.SizeInBytes = ScenePass.VertexBufferSize * 2 * sizeof(SceneVertex);

	ScenePass.IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	ScenePass.IndexBufferView.BufferLocation = ScenePass.IndexBuffer->GetGPUVirtualAddress();
	ScenePass.IndexBufferView.SizeInBytes = ScenePass.IndexBufferSize * 2 * sizeof(uint32_t);

	D3D12_RANGE readRange = {};
	result = ScenePass.VertexBuffer->Map(0, &readRange, (void**)&ScenePass.VertexData);
	ThrowIfFailed(result, "Map(ScenePass.VertexBuffer) failed");

	result = ScenePass.IndexBuffer->Map(0, &readRange, (void**)&ScenePass.IndexData);
	ThrowIfFailed(result, "Map(ScenePass.IndexBuffer) failed");

	ScenePass.Multisample = SceneBuffers.Multisample;
}

void UD3D12RenderDevice::CreateSceneSamplers()
{
	for (int i = 0; i < 16; i++)
	{
		int dummyMipmapCount = (i >> 2) & 3;
		D3D12_FILTER filter = (i & 1) ? D3D12_FILTER_MIN_MAG_MIP_POINT : D3D12_FILTER_ANISOTROPIC;
		D3D12_TEXTURE_ADDRESS_MODE addressmode = (i & 2) ? D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE : D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		D3D12_SAMPLER_DESC samplerDesc = {};
		samplerDesc.MinLOD = dummyMipmapCount;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
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
		ScenePass.Samplers[i] = samplerDesc;
	}

	ScenePass.LODBias = LODBias;
}

void UD3D12RenderDevice::ReleaseSceneSamplers()
{
	for (auto& it : Descriptors.Sampler)
		it.second.reset();
	Descriptors.Sampler.clear();
	ScenePass.LODBias = 0.0f;
}

void UD3D12RenderDevice::UpdateScenePass()
{
	if (!ScenePass.RootSignature || ScenePass.Multisample != SceneBuffers.Multisample)
	{
		ReleaseScenePass();
		CreateScenePass();
	}

	if (ScenePass.LODBias != LODBias)
	{
		ReleaseSceneSamplers();
		CreateSceneSamplers();
	}
}

void UD3D12RenderDevice::ReleaseScenePass()
{
	if (ScenePass.VertexData)
	{
		ScenePass.VertexBuffer->Unmap(0, nullptr);
		ScenePass.VertexData = nullptr;
	}

	if (ScenePass.IndexData)
	{
		ScenePass.IndexBuffer->Unmap(0, nullptr);
		ScenePass.IndexData = nullptr;
	}

	ScenePass.VertexBuffer.reset();
	ScenePass.IndexBuffer.reset();
	ReleaseSceneSamplers();
	for (auto& pipeline : ScenePass.Pipelines)
	{
		pipeline.Pipeline.reset();
	}
	for (int i = 0; i < 2; i++)
	{
		ScenePass.LinePipeline[i].Pipeline.reset();
		ScenePass.PointPipeline[i].Pipeline.reset();
	}
	ScenePass.RootSignature.reset();
}

void UD3D12RenderDevice::ReleaseBloomPass()
{
	BloomPass.Extract.reset();
	BloomPass.Combine.reset();
	BloomPass.CombineAdditive.reset();
	BloomPass.BlurVertical.reset();
	BloomPass.BlurHorizontal.reset();
	BloomPass.RootSignature.reset();
}

void UD3D12RenderDevice::ReleasePresentPass()
{
	PresentPass.HitResolve.reset();
	for (auto& shader : PresentPass.Present) shader.reset();
	PresentPass.PPStepVertexBuffer.reset();
	PresentPass.DitherTexture.reset();
	PresentPass.RootSignature.reset();
}

void UD3D12RenderDevice::ReleaseSceneBuffers()
{
	SceneBuffers.PresentSRVs.reset();
	SceneBuffers.SceneRTVs.reset();
	SceneBuffers.SceneDSV.reset();
	SceneBuffers.PPHitBufferRTV.reset();
	SceneBuffers.HitBufferSRV.reset();
	for (int i = 0; i < 2; i++)
	{
		SceneBuffers.PPImageRTV[i].reset();
		SceneBuffers.PPImageSRV[i].reset();
		SceneBuffers.PPImage[i].reset();
	}
	SceneBuffers.ColorBuffer.reset();
	SceneBuffers.StagingHitBuffer.reset();
	SceneBuffers.PPHitBuffer.reset();
	SceneBuffers.HitBuffer.reset();
	SceneBuffers.DepthBuffer.reset();
	for (PPBlurLevel& level : SceneBuffers.BlurLevels)
	{
		level.VTextureRTV.reset();
		level.VTextureSRV.reset();
		level.VTexture.reset();
		level.HTextureRTV.reset();
		level.HTextureSRV.reset();
		level.HTexture.reset();
	}
}

UD3D12RenderDevice::ScenePipelineState* UD3D12RenderDevice::GetPipeline(DWORD PolyFlags)
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

void UD3D12RenderDevice::RunBloomPass()
{
	float blurAmount = 0.6f + BloomAmount * (1.9f / 255.0f);
	BloomPushConstants pushconstants;
	ComputeBlurSamples(7, blurAmount, pushconstants.SampleWeights);

	Commands.Current->Draw->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Commands.Current->Draw->IASetVertexBuffers(0, 1, &PresentPass.PPStepVertexBufferView);
	Commands.Current->Draw->SetGraphicsRootSignature(BloomPass.RootSignature);
	Commands.Current->Draw->SetGraphicsRoot32BitConstants(1, sizeof(BloomPushConstants) / sizeof(uint32_t), &pushconstants, 0);

	D3D12_VIEWPORT viewport = {};
	viewport.MaxDepth = 1.0f;

	// Extract overbright pixels that we want to bloom:
	viewport.Width = SceneBuffers.BlurLevels[0].Width;
	viewport.Height = SceneBuffers.BlurLevels[0].Height;
	TransitionResourceBarrier(Commands.Current->Draw, SceneBuffers.BlurLevels[0].VTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D12_CPU_DESCRIPTOR_HANDLE rtv = SceneBuffers.BlurLevels[0].VTextureRTV.CPUHandle();
	Commands.Current->Draw->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
	Commands.Current->Draw->RSSetViewports(1, &viewport);
	Commands.Current->Draw->SetPipelineState(BloomPass.Extract);
	Commands.Current->Draw->SetGraphicsRootDescriptorTable(0, SceneBuffers.PPImageSRV[0].GPUHandle());
	Commands.Current->Draw->DrawInstanced(6, 1, 0, 0);
	TransitionResourceBarrier(Commands.Current->Draw, SceneBuffers.BlurLevels[0].VTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	// Blur and downscale:
	for (int i = 0; i < SceneBuffers.NumBloomLevels - 1; i++)
	{
		auto& blevel = SceneBuffers.BlurLevels[i];
		auto& next = SceneBuffers.BlurLevels[i + 1];

		viewport.Width = blevel.Width;
		viewport.Height = blevel.Height;
		Commands.Current->Draw->RSSetViewports(1, &viewport);
		BlurStep(blevel.VTextureSRV, blevel.HTextureRTV, blevel.HTexture, false);
		BlurStep(blevel.HTextureSRV, blevel.VTextureRTV, blevel.VTexture, true);

		// Linear downscale:
		TransitionResourceBarrier(Commands.Current->Draw, next.VTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		viewport.Width = next.Width;
		viewport.Height = next.Height;
		rtv = next.VTextureRTV.CPUHandle();
		Commands.Current->Draw->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
		Commands.Current->Draw->RSSetViewports(1, &viewport);
		Commands.Current->Draw->SetPipelineState(BloomPass.Combine);
		Commands.Current->Draw->SetGraphicsRootDescriptorTable(0, blevel.VTextureSRV.GPUHandle());
		Commands.Current->Draw->DrawInstanced(6, 1, 0, 0);
		TransitionResourceBarrier(Commands.Current->Draw, next.VTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	// Blur and upscale:
	for (int i = SceneBuffers.NumBloomLevels - 1; i > 0; i--)
	{
		auto& blevel = SceneBuffers.BlurLevels[i];
		auto& next = SceneBuffers.BlurLevels[i - 1];

		viewport.Width = blevel.Width;
		viewport.Height = blevel.Height;
		Commands.Current->Draw->RSSetViewports(1, &viewport);
		BlurStep(blevel.VTextureSRV, blevel.HTextureRTV, blevel.HTexture, false);
		BlurStep(blevel.HTextureSRV, blevel.VTextureRTV, blevel.VTexture, true);

		// Linear upscale:
		TransitionResourceBarrier(Commands.Current->Draw, next.VTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		viewport.Width = next.Width;
		viewport.Height = next.Height;
		rtv = next.VTextureRTV.CPUHandle();
		Commands.Current->Draw->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
		Commands.Current->Draw->RSSetViewports(1, &viewport);
		Commands.Current->Draw->SetPipelineState(BloomPass.Combine);
		Commands.Current->Draw->SetGraphicsRootDescriptorTable(0, blevel.VTextureSRV.GPUHandle());
		Commands.Current->Draw->DrawInstanced(6, 1, 0, 0);
		TransitionResourceBarrier(Commands.Current->Draw, next.VTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	viewport.Width = SceneBuffers.BlurLevels[0].Width;
	viewport.Height = SceneBuffers.BlurLevels[0].Height;
	Commands.Current->Draw->RSSetViewports(1, &viewport);
	BlurStep(SceneBuffers.BlurLevels[0].VTextureSRV, SceneBuffers.BlurLevels[0].HTextureRTV, SceneBuffers.BlurLevels[0].HTexture, false);
	BlurStep(SceneBuffers.BlurLevels[0].HTextureSRV, SceneBuffers.BlurLevels[0].VTextureRTV, SceneBuffers.BlurLevels[0].VTexture, true);

	// Add bloom back to scene post process texture:

	TransitionResourceBarrier(Commands.Current->Draw, SceneBuffers.PPImage[0], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

	viewport.Width = SceneBuffers.Width;
	viewport.Height = SceneBuffers.Height;
	rtv = SceneBuffers.PPImageRTV[0].CPUHandle();
	Commands.Current->Draw->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
	Commands.Current->Draw->RSSetViewports(1, &viewport);
	Commands.Current->Draw->SetPipelineState(BloomPass.CombineAdditive);
	Commands.Current->Draw->SetGraphicsRootDescriptorTable(0, SceneBuffers.BlurLevels[0].VTextureSRV.GPUHandle());
	Commands.Current->Draw->DrawInstanced(6, 1, 0, 0);

	TransitionResourceBarrier(Commands.Current->Draw, SceneBuffers.PPImage[0], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void UD3D12RenderDevice::BlurStep(const DescriptorSet& input, const DescriptorSet& output, ID3D12Resource* outputResource, bool vertical)
{
	TransitionResourceBarrier(Commands.Current->Draw, outputResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

	D3D12_CPU_DESCRIPTOR_HANDLE rtv = output.CPUHandle();
	Commands.Current->Draw->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
	Commands.Current->Draw->SetPipelineState(vertical ? BloomPass.BlurVertical : BloomPass.BlurHorizontal);
	Commands.Current->Draw->SetGraphicsRootDescriptorTable(0, input.GPUHandle());
	Commands.Current->Draw->DrawInstanced(6, 1, 0, 0);

	TransitionResourceBarrier(Commands.Current->Draw, outputResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

float UD3D12RenderDevice::ComputeBlurGaussian(float n, float theta) // theta = Blur Amount
{
	return (float)((1.0f / std::sqrtf(2 * 3.14159265359f * theta)) * std::expf(-(n * n) / (2.0f * theta * theta)));
}

void UD3D12RenderDevice::ComputeBlurSamples(int sampleCount, float blurAmount, float* sampleWeights)
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

void UD3D12RenderDevice::CreateBloomPass()
{
	auto vertexShader = CompileHlsl("shaders/PPStep.vert", "vs");
	auto extractPixelShader = CompileHlsl("shaders/BloomExtract.frag", "ps");
	auto combinePixelShader = CompileHlsl("shaders/BloomCombine.frag", "ps");
	auto blurVertPixelShader = CompileHlsl("shaders/Blur.frag", "ps", {"BLUR_VERTICAL"});
	auto blurHorizontalPixelShader = CompileHlsl("shaders/Blur.frag", "ps", {"BLUR_HORIZONTAL"});

	std::vector<D3D12_INPUT_ELEMENT_DESC> elements =
	{
		{ "AttrPos", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> descriptorTables(1);

	D3D12_DESCRIPTOR_RANGE texRange = {};
	texRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	texRange.BaseShaderRegister = 0;
	texRange.NumDescriptors = 1;
	descriptorTables[0].push_back(texRange);

	D3D12_ROOT_CONSTANTS pushConstants = {};
	pushConstants.ShaderRegister = 0;
	pushConstants.Num32BitValues = sizeof(BloomPushConstants) / sizeof(int32_t);

	std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers;
	D3D12_STATIC_SAMPLER_DESC staticSampler = {};
	staticSampler.ShaderRegister = 0;
	staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
	staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers.push_back(staticSampler);

	BloomPass.RootSignature = CreateRootSignature("BloomPass.RootSignature", descriptorTables, pushConstants, staticSamplers);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = BloomPass.RootSignature;
	psoDesc.InputLayout.NumElements = elements.size();
	psoDesc.InputLayout.pInputElementDescs = elements.data();
	psoDesc.VS.pShaderBytecode = vertexShader.data();
	psoDesc.VS.BytecodeLength = vertexShader.size();
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	psoDesc.PS.pShaderBytecode = extractPixelShader.data();
	psoDesc.PS.BytecodeLength = extractPixelShader.size();
	HRESULT result = Device->CreateGraphicsPipelineState(&psoDesc, BloomPass.Extract.GetIID(), BloomPass.Extract.InitPtr());
	ThrowIfFailed(result, "CreateGraphicsPipelineState(BloomPass.Extract) failed");

	psoDesc.PS.pShaderBytecode = blurVertPixelShader.data();
	psoDesc.PS.BytecodeLength = blurVertPixelShader.size();
	result = Device->CreateGraphicsPipelineState(&psoDesc, BloomPass.BlurVertical.GetIID(), BloomPass.BlurVertical.InitPtr());
	ThrowIfFailed(result, "CreateGraphicsPipelineState(BloomPass.Extract) failed");

	psoDesc.PS.pShaderBytecode = blurHorizontalPixelShader.data();
	psoDesc.PS.BytecodeLength = blurHorizontalPixelShader.size();
	result = Device->CreateGraphicsPipelineState(&psoDesc, BloomPass.BlurHorizontal.GetIID(), BloomPass.BlurHorizontal.InitPtr());
	ThrowIfFailed(result, "CreateGraphicsPipelineState(BloomPass.BlurHorizontal) failed");

	psoDesc.PS.pShaderBytecode = combinePixelShader.data();
	psoDesc.PS.BytecodeLength = combinePixelShader.size();
	result = Device->CreateGraphicsPipelineState(&psoDesc, BloomPass.Combine.GetIID(), BloomPass.Combine.InitPtr());
	ThrowIfFailed(result, "CreateGraphicsPipelineState(BloomPass.Combine) failed");

	D3D12_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;

	psoDesc.BlendState = blendDesc;
	psoDesc.PS.pShaderBytecode = combinePixelShader.data();
	psoDesc.PS.BytecodeLength = combinePixelShader.size();
	result = Device->CreateGraphicsPipelineState(&psoDesc, BloomPass.CombineAdditive.GetIID(), BloomPass.CombineAdditive.InitPtr());
	ThrowIfFailed(result, "CreateGraphicsPipelineState(BloomPass.Combine) failed");
}

void UD3D12RenderDevice::CreatePresentPass()
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

	D3D12_HEAP_PROPERTIES uploadHeapProps = { D3D12_HEAP_TYPE_UPLOAD };

	D3D12_RESOURCE_DESC bufDesc = {};
	bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufDesc.Width = positions.size() * sizeof(vec2);
	bufDesc.Height = 1;
	bufDesc.DepthOrArraySize = 1;
	bufDesc.MipLevels = 1;
	bufDesc.SampleDesc.Count = 1;
	bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	HRESULT result = Device->CreateCommittedResource(
		&uploadHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		PresentPass.PPStepVertexBuffer.GetIID(),
		PresentPass.PPStepVertexBuffer.InitPtr());
	ThrowIfFailed(result, "CreateCommittedResource(PresentPass.PPStepVertexBuffer) failed");
	PresentPass.PPStepVertexBuffer->SetName(TEXT("PresentPass.PPStepVertexBuffer"));

	D3D12_RANGE readRange = {};
	void* dest = nullptr;
	result = PresentPass.PPStepVertexBuffer->Map(0, &readRange, &dest);
	ThrowIfFailed(result, "Map(PresentPass.PPStepVertexBuffer) failed");
	memcpy(dest, positions.data(), positions.size() * sizeof(vec2));
	PresentPass.PPStepVertexBuffer->Unmap(0, nullptr);

	PresentPass.PPStepVertexBufferView.BufferLocation = PresentPass.PPStepVertexBuffer->GetGPUVirtualAddress();
	PresentPass.PPStepVertexBufferView.SizeInBytes = positions.size() * sizeof(vec2);
	PresentPass.PPStepVertexBufferView.StrideInBytes = sizeof(vec2);

	std::vector<D3D12_INPUT_ELEMENT_DESC> elements =
	{
		{ "AttrPos", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> descriptorTables(1);

	D3D12_DESCRIPTOR_RANGE texRange = {};
	texRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	texRange.BaseShaderRegister = 0;
	texRange.NumDescriptors = 2;
	descriptorTables[0].push_back(texRange);

	D3D12_ROOT_CONSTANTS pushConstants = {};
	pushConstants.ShaderRegister = 0;
	pushConstants.Num32BitValues = sizeof(PresentPushConstants) / sizeof(int32_t);

	std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers;
	D3D12_STATIC_SAMPLER_DESC staticSampler = {};
	staticSampler.ShaderRegister = 0;
	staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
	staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers.push_back(staticSampler);
	staticSampler.ShaderRegister = 1;
	staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers.push_back(staticSampler);

	PresentPass.RootSignature = CreateRootSignature("PresentPass.RootSignature", descriptorTables, pushConstants, staticSamplers);

	auto vertexShader = CompileHlsl("shaders/PPStep.vert", "vs");

	static const char* transferFunctions[2] = { nullptr, "HDR_MODE" };
	static const char* gammaModes[2] = { "GAMMA_MODE_D3D9", "GAMMA_MODE_XOPENGL" };
	static const char* colorModes[4] = { nullptr, "COLOR_CORRECT_MODE0", "COLOR_CORRECT_MODE1", "COLOR_CORRECT_MODE2" };
	for (int i = 0; i < 32; i++)
	{
		std::vector<std::string> defines;
		if (transferFunctions[i & 1]) defines.push_back(transferFunctions[i & 1]);
		if (gammaModes[(i >> 1) & 1]) defines.push_back(gammaModes[(i >> 1) & 1]);
		if (colorModes[(i >> 2) & 3]) defines.push_back(colorModes[(i >> 2) & 3]);
		auto pixelShader = CompileHlsl("shaders/Present.frag", "ps", defines);

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = PresentPass.RootSignature;
		psoDesc.InputLayout.NumElements = elements.size();
		psoDesc.InputLayout.pInputElementDescs = elements.data();
		psoDesc.VS.pShaderBytecode = vertexShader.data();
		psoDesc.VS.BytecodeLength = vertexShader.size();
		psoDesc.PS.pShaderBytecode = pixelShader.data();
		psoDesc.PS.BytecodeLength = pixelShader.size();
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = (i & 16) ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;

		HRESULT result = Device->CreateGraphicsPipelineState(&psoDesc, PresentPass.Present[i].GetIID(), PresentPass.Present[i].InitPtr());
		ThrowIfFailed(result, "CreateGraphicsPipelineState(Present) failed");
	}

	auto pixelShader = CompileHlsl("shaders/HitResolve.frag", "ps");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = PresentPass.RootSignature;
	psoDesc.InputLayout.NumElements = elements.size();
	psoDesc.InputLayout.pInputElementDescs = elements.data();
	psoDesc.VS.pShaderBytecode = vertexShader.data();
	psoDesc.VS.BytecodeLength = vertexShader.size();
	psoDesc.PS.pShaderBytecode = pixelShader.data();
	psoDesc.PS.BytecodeLength = pixelShader.size();
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8_UINT;
	result = Device->CreateGraphicsPipelineState(&psoDesc, PresentPass.HitResolve.GetIID(), PresentPass.HitResolve.InitPtr());
	ThrowIfFailed(result, "CreateGraphicsPipelineState(HitResolve) failed");

	D3D12_HEAP_PROPERTIES defaultHeapProps = {};
	defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Format = DXGI_FORMAT_R32_FLOAT;
	texDesc.Width = 8;
	texDesc.Height = 8;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.SampleDesc.Count = 1;

	result = Device->CreateCommittedResource(
		&defaultHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		PresentPass.DitherTexture.GetIID(),
		PresentPass.DitherTexture.InitPtr());
	ThrowIfFailed(result, "CreateCommittedResource(PresentPass.DitherTexture) failed");
	PresentPass.DitherTexture->SetName(TEXT("PresentPass.DitherTexture"));

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

	auto onWriteSubresource = [](uint8_t* dest, int subresource, const D3D12_SUBRESOURCE_FOOTPRINT& footprint)
		{
			const float* src = ditherdata;
			for (int y = 0; y < 8; y++)
			{
				memcpy(dest, src, 8 * sizeof(float));
				src += 8;
				dest += footprint.RowPitch;
			}
		};

	UploadTexture(PresentPass.DitherTexture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0, 8, 8, 0, 1, onWriteSubresource);
}

void UD3D12RenderDevice::UploadTexture(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter, int x, int y, int width, int height, int firstSubresource, int numSubresources, const std::function<void(uint8_t* dest, int subresource, const D3D12_SUBRESOURCE_FOOTPRINT& footprint)>& onWriteSubresource)
{
	if (stateBefore != D3D12_RESOURCE_STATE_COPY_DEST)
	{
		TransitionResourceBarrier(Commands.Current->Transfer, resource, stateBefore, D3D12_RESOURCE_STATE_COPY_DEST);
	}

	Upload.Transfer.Footprints.resize(numSubresources);
	Upload.Transfer.NumRows.resize(numSubresources);
	Upload.Transfer.RowSizeInBytes.resize(numSubresources);

	D3D12_RESOURCE_DESC desc = resource->GetDesc();
	desc.Width = width;
	desc.Height = height;

	// GetCopyableFootprints in Windows 10 needs the BaseOffset to be aligned already.
	Upload.Pos = (Upload.Base + Upload.Pos + (D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1)) / D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT * D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - Upload.Base;

	UINT64 totalSize = 0;
	Device->GetCopyableFootprints(&desc, firstSubresource, numSubresources, Upload.Base + Upload.Pos, Upload.Transfer.Footprints.data(), Upload.Transfer.NumRows.data(), Upload.Transfer.RowSizeInBytes.data(), &totalSize);

	if (Upload.Pos + totalSize > Upload.Size)
	{
		SubmitCommands(false);
		Upload.Pos = (Upload.Base + Upload.Pos + (D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1)) / D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT * D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - Upload.Base;
		Device->GetCopyableFootprints(&desc, firstSubresource, numSubresources, Upload.Base + Upload.Pos, Upload.Transfer.Footprints.data(), Upload.Transfer.NumRows.data(), Upload.Transfer.RowSizeInBytes.data(), &totalSize);
		if (Upload.Pos + totalSize > Upload.Size)
		{
			debugf(TEXT("Could not upload texture. Total memory requirements are bigger than the entire upload buffer!"));
			return;
		}
	}

	for (int i = 0; i < numSubresources; i++)
	{
		onWriteSubresource(Upload.Data + Upload.Transfer.Footprints[i].Offset, i, Upload.Transfer.Footprints[i].Footprint);

		D3D12_TEXTURE_COPY_LOCATION src = {};
		src.pResource = Upload.Buffer;
		src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		src.PlacedFootprint = Upload.Transfer.Footprints[i];

		D3D12_TEXTURE_COPY_LOCATION dest = {};
		dest.pResource = resource;
		dest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dest.SubresourceIndex = firstSubresource + i;

		D3D12_BOX srcBox = {};
		srcBox.right = Max(width >> i, 1);
		srcBox.bottom = Max(height >> i, 1);
		srcBox.back = 1;

		// Block compressed requires the box to be 4x4 aligned
		switch (desc.Format)
		{
		case DXGI_FORMAT_BC1_TYPELESS:
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC2_TYPELESS:
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC3_TYPELESS:
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_BC4_TYPELESS:
		case DXGI_FORMAT_BC4_UNORM:
		case DXGI_FORMAT_BC5_TYPELESS:
		case DXGI_FORMAT_BC5_UNORM:
			srcBox.right = (srcBox.right + 3) / 4 * 4;
			srcBox.bottom = (srcBox.bottom + 3) / 4 * 4;
			break;
		}

		Commands.Current->Transfer->CopyTextureRegion(&dest, x, y, 0, &src, &srcBox);
	}

	Upload.Pos += totalSize;

	if (stateAfter != D3D12_RESOURCE_STATE_COPY_DEST)
	{
		TransitionResourceBarrier(Commands.Current->Transfer, resource, D3D12_RESOURCE_STATE_COPY_DEST, stateAfter);
	}
}

#if defined(UNREALGOLD)

void UD3D12RenderDevice::Flush()
{
	guard(UD3D12RenderDevice::Flush);

	try
	{
		DrawBatches();
		ClearTextureCache();

		if (UsePrecache && !GIsEditor)
			PrecacheOnFlip = 1;
	}
	catch (const std::exception& e)
	{
		debugf(TEXT("Could not flush d3d12 renderer: %s"), to_utf16(e.what()).c_str());
		Exit();
	}

	unguard;
}

#else

void UD3D12RenderDevice::Flush(UBOOL AllowPrecache)
{
	guard(UD3D12RenderDevice::Flush);

	try
	{
		DrawBatches();
		ClearTextureCache();

		if (AllowPrecache && UsePrecache && !GIsEditor)
			PrecacheOnFlip = 1;
	}
	catch (const std::exception& e)
	{
		debugf(TEXT("Could not flush d3d12 renderer: %s"), to_utf16(e.what()).c_str());
		Exit();
	}

	unguard;
}

#endif

UBOOL UD3D12RenderDevice::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	guard(UD3D12RenderDevice::Exec);

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
		HRESULT result = SwapChain3->GetContainingOutput(&output);
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

void UD3D12RenderDevice::Lock(FPlane InFlashScale, FPlane InFlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* InHitData, INT* InHitSize)
{
	guard(UD3D12RenderDevice::Lock);

	// This crashes currently for some reason
#if 0
	int wantedBufferCount = GetWantedSwapChainBufferCount();
	if (BufferCount != wantedBufferCount)
	{
		SubmitCommands(false);
		WaitDeviceIdle();
		ReleaseSwapChainResources();
		BufferCount = wantedBufferCount;
		UpdateSwapChain();
	}
#endif

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

	try
	{
		UpdateScenePass();

		HitData = InHitData;
		HitSize = InHitSize;

		FlashScale = InFlashScale;
		FlashFog = InFlashFog;

		Commands.PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		FLOAT color[4] = { ScreenClear.X, ScreenClear.Y, ScreenClear.Z, ScreenClear.W };
		FLOAT zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		D3D12_CPU_DESCRIPTOR_HANDLE views[2] = { SceneBuffers.SceneRTVs.CPUHandle(0), SceneBuffers.SceneRTVs.CPUHandle(1) };
		D3D12_CPU_DESCRIPTOR_HANDLE depthview = SceneBuffers.SceneDSV.CPUHandle();
		Commands.Current->Draw->ClearRenderTargetView(views[0], color, 0, nullptr);
		Commands.Current->Draw->ClearRenderTargetView(views[1], zero, 0, nullptr);
		Commands.Current->Draw->ClearDepthStencilView(depthview, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		D3D12_RECT box = {};
		box.right = CurrentSizeX;
		box.bottom = CurrentSizeY;
		Commands.Current->Draw->RSSetScissorRects(1, &box);

		SceneConstants.HitIndex = 0;
		ForceHitIndex = -1;

		IsLocked = true;
	}
	catch (const std::exception& e)
	{
		debugf(TEXT("Could not lock d3d12 renderer: %s"), to_utf16(e.what()).c_str());
		Exit();
	}

	unguard;
}

void UD3D12RenderDevice::DrawStats(FSceneNode* Frame)
{
	Super::DrawStats(Frame);

#if defined(OLDUNREAL469SDK)
	GRender->ShowStat(CurrentFrame, TEXT("D3D12: Draw calls: %d, Complex surfaces: %d, Gouraud polygons: %d, Tiles: %d; Uploads: %d, Rect Uploads: %d, Buffers Used: %d, Textures Cached: %d, Descriptors Used: %d\r\n"), Stats.DrawCalls, Stats.ComplexSurfaces, Stats.GouraudPolygons, Stats.Tiles, Stats.Uploads, Stats.RectUploads, Stats.BuffersUsed, Textures->GetTexturesInCache(), Heaps.Common->GetUsedCount());
#endif

	Stats.DrawCalls = 0;
	Stats.ComplexSurfaces = 0;
	Stats.GouraudPolygons = 0;
	Stats.Tiles = 0;
	Stats.Uploads = 0;
	Stats.RectUploads = 0;
	Stats.BuffersUsed = 1;
}

PresentPushConstants UD3D12RenderDevice::GetPresentPushConstants()
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

void UD3D12RenderDevice::Unlock(UBOOL Blit)
{
	guard(UD3D12RenderDevice::Unlock);

	if (!IsLocked) // Don't trust the engine.
		return;

	try
	{
		if (Blit || HitData)
			DrawBatches();

		if (Blit)
		{
			if (SceneBuffers.Multisample > 1)
			{
				TransitionResourceBarrier(
					Commands.Current->Draw,
					SceneBuffers.PPImage[0], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RESOLVE_DEST,
					SceneBuffers.ColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);

				Commands.Current->Draw->ResolveSubresource(SceneBuffers.PPImage[0], 0, SceneBuffers.ColorBuffer, 0, DXGI_FORMAT_R16G16B16A16_FLOAT);

				TransitionResourceBarrier(
					Commands.Current->Draw,
					SceneBuffers.PPImage[0], D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					SceneBuffers.ColorBuffer, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
			}
			else
			{
				TransitionResourceBarrier(
					Commands.Current->Draw,
					SceneBuffers.PPImage[0], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST,
					SceneBuffers.ColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);

				Commands.Current->Draw->CopyResource(SceneBuffers.PPImage[0], SceneBuffers.ColorBuffer);

				TransitionResourceBarrier(
					Commands.Current->Draw,
					SceneBuffers.PPImage[0], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					SceneBuffers.ColorBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
			}

			if (Bloom)
			{
				RunBloomPass();
			}

			TransitionResourceBarrier(Commands.Current->Draw, FrameBuffers[BackBufferIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

			D3D12_CPU_DESCRIPTOR_HANDLE rtv = FrameBufferRTVs.CPUHandle(BackBufferIndex);
			Commands.Current->Draw->SetGraphicsRootSignature(PresentPass.RootSignature);
			Commands.Current->Draw->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

			RECT box = {};
			GetClientRect((HWND)Viewport->GetWindow(), &box);
			int width = box.right;
			int height = box.bottom;
			float scale = std::min(width / (float)CurrentSizeX, height / (float)CurrentSizeY);
			int letterboxWidth = (int)std::round(CurrentSizeX * scale);
			int letterboxHeight = (int)std::round(CurrentSizeY * scale);
			int letterboxX = (width - letterboxWidth) / 2;
			int letterboxY = (height - letterboxHeight) / 2;

			D3D12_VIEWPORT viewport = {};
			viewport.TopLeftX = letterboxX;
			viewport.TopLeftY = letterboxY;
			viewport.Width = letterboxWidth;
			viewport.Height = letterboxHeight;
			viewport.MaxDepth = 1.0f;
			Commands.Current->Draw->RSSetViewports(1, &viewport);

			D3D12_RECT scissorbox = {};
			scissorbox.left = box.left;
			scissorbox.top = box.top;
			scissorbox.right = box.right;
			scissorbox.bottom = box.bottom;
			Commands.Current->Draw->RSSetScissorRects(1, &scissorbox);

			PresentPushConstants pushconstants = GetPresentPushConstants();

			// Select present shader based on what the user is actually using
			int presentShader = 0;
			if (ActiveHdr) presentShader |= (1 | 16); // 1 = HDR in shader, 16 = output is rgba16f
			if (GammaMode == 1) presentShader |= 2;
			if (pushconstants.Brightness != 0.0f || pushconstants.Contrast != 1.0f || pushconstants.Saturation != 1.0f) presentShader |= (Clamp(GrayFormula, 0, 2) + 1) << 2;

			Commands.Current->Draw->SetPipelineState(PresentPass.Present[presentShader]);
			Commands.Current->Draw->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			Commands.Current->Draw->IASetVertexBuffers(0, 1, &PresentPass.PPStepVertexBufferView);
			Commands.Current->Draw->SetGraphicsRootDescriptorTable(0, SceneBuffers.PresentSRVs.GPUHandle());
			Commands.Current->Draw->SetGraphicsRoot32BitConstants(1, sizeof(PresentPushConstants) / sizeof(uint32_t), &pushconstants, 0);
			Commands.Current->Draw->DrawInstanced(6, 1, 0, 0);

			TransitionResourceBarrier(Commands.Current->Draw, FrameBuffers[BackBufferIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

			Batch.Pipeline = nullptr;
			Batch.PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			Batch.Tex = nullptr;
			Batch.Lightmap = nullptr;
			Batch.Detailtex = nullptr;
			Batch.Macrotex = nullptr;
			Batch.SceneIndexStart = 0;

			SubmitCommands(true);

			// Flush the descriptors if we are running out
			if (Heaps.Common->GetUsedCount() * 100 / Heaps.Common->GetHeapSize() > 75)
			{
				WaitDeviceIdle();
				for (auto& it : Descriptors.Tex)
					it.second.reset();
				Descriptors.Tex.clear();
			}
		}

		if (HitData)
		{
			D3D12_BOX box = {};
			box.left = Viewport->HitX;
			box.right = Viewport->HitX + Viewport->HitXL;
			box.top = SceneBuffers.Height - Viewport->HitY - Viewport->HitYL;
			box.bottom = SceneBuffers.Height - Viewport->HitY;
			box.front = 0;
			box.back = 1;

			// Resolve multisampling and place the result in PPHitBuffer
			if (SceneBuffers.Multisample > 1)
			{
				TransitionResourceBarrier(
					Commands.Current->Draw,
					SceneBuffers.HitBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					SceneBuffers.PPHitBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

				D3D12_CPU_DESCRIPTOR_HANDLE rtv = SceneBuffers.PPHitBufferRTV.CPUHandle();
				Commands.Current->Draw->SetGraphicsRootSignature(PresentPass.RootSignature);
				Commands.Current->Draw->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

				D3D12_VIEWPORT viewport = {};
				viewport.TopLeftX = box.left;
				viewport.TopLeftY = box.top;
				viewport.Width = box.right - box.left;
				viewport.Height = box.bottom - box.top;
				viewport.MaxDepth = 1.0f;
				Commands.Current->Draw->RSSetViewports(1, &viewport);

				D3D12_RECT scissorbox = {};
				scissorbox.left = box.left;
				scissorbox.top = box.top;
				scissorbox.right = box.right;
				scissorbox.bottom = box.bottom;
				Commands.Current->Draw->RSSetScissorRects(1, &scissorbox);

				Commands.Current->Draw->SetPipelineState(PresentPass.HitResolve);
				Commands.Current->Draw->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				Commands.Current->Draw->IASetVertexBuffers(0, 1, &PresentPass.PPStepVertexBufferView);
				Commands.Current->Draw->SetGraphicsRootDescriptorTable(0, SceneBuffers.HitBufferSRV.GPUHandle());
				Commands.Current->Draw->DrawInstanced(6, 1, 0, 0);

				TransitionResourceBarrier(
					Commands.Current->Draw,
					SceneBuffers.HitBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET,
					SceneBuffers.PPHitBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
			}
			else
			{
				TransitionResourceBarrier(
					Commands.Current->Draw,
					SceneBuffers.HitBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE,
					SceneBuffers.PPHitBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);

				D3D12_TEXTURE_COPY_LOCATION src = {}, dest = {};
				src.pResource = SceneBuffers.HitBuffer;
				src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				dest.pResource = SceneBuffers.PPHitBuffer;
				dest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				Commands.Current->Draw->CopyTextureRegion(&dest, box.left, box.top, 0, &src, &box);

				TransitionResourceBarrier(
					Commands.Current->Draw,
					SceneBuffers.HitBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET,
					SceneBuffers.PPHitBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
			}

			// Copy PPHitBuffer to StagingHitbuffer, but only the part we want to examine
			D3D12_RESOURCE_DESC desc = SceneBuffers.PPHitBuffer->GetDesc();
			desc.Width = box.right - box.left;
			desc.Height = box.bottom - box.top;
			UINT64 totalSize = 0, rowSizeInBytes = 0;
			D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
			UINT numRows = 0;
			Device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalSize);
			D3D12_TEXTURE_COPY_LOCATION src = {}, dest = {};
			src.pResource = SceneBuffers.PPHitBuffer;
			src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			dest.pResource = SceneBuffers.StagingHitBuffer;
			dest.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			dest.PlacedFootprint = footprint;
			Commands.Current->Draw->CopyTextureRegion(&dest, 0, 0, 0, &src, &box);

			SubmitCommands(false);
			WaitDeviceIdle();

			// Lock the buffer and look for the last hit
			int hit = 0;
			void* data = nullptr;
			D3D12_RANGE readRange = {};
			readRange.End = totalSize;
			HRESULT result = SceneBuffers.StagingHitBuffer->Map(0, &readRange, &data);
			if (SUCCEEDED(result))
			{
				int width = Viewport->HitXL;
				int height = Viewport->HitYL;
				for (int y = 0; y < height; y++)
				{
					const INT* line = (const INT*)(((const char*)data) + y * footprint.Footprint.RowPitch);
					for (int x = 0; x < width; x++)
					{
						hit = std::max(hit, line[x]);
					}
				}
				D3D12_RANGE writtenRange = {};
				SceneBuffers.StagingHitBuffer->Unmap(0, &writtenRange);
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

		HitQueryStack.clear();
		HitQueries.clear();
		HitBuffer.clear();
		HitData = nullptr;
		HitSize = nullptr;

		IsLocked = false;
	}
	catch (const std::exception& e)
	{
		debugf(TEXT("Could not unlock d3d12 renderer: %s"), to_utf16(e.what()).c_str());
		Exit();
	}

	unguard;
}

void UD3D12RenderDevice::PushHit(const BYTE* Data, INT Count)
{
	guard(UD3D12RenderDevice::PushHit);

	if (Count <= 0) return;
	HitQueryStack.insert(HitQueryStack.end(), Data, Data + Count);

	SetHitLocation();

	unguard;
}

void UD3D12RenderDevice::PopHit(INT Count, UBOOL bForce)
{
	guard(UD3D12RenderDevice::PopHit);

	if (bForce) // Force hit what we are popping
		ForceHitIndex = HitQueries.size() - 1;

	HitQueryStack.resize(HitQueryStack.size() - Count);

	SetHitLocation();

	unguard;
}

void UD3D12RenderDevice::SetHitLocation()
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
}

#if defined(OLDUNREAL469SDK)

UBOOL UD3D12RenderDevice::SupportsTextureFormat(ETextureFormat Format)
{
	guard(UD3D12RenderDevice::SupportsTextureFormat);

	return Uploads->SupportsTextureFormat(Format) ? TRUE : FALSE;

	unguard;
}

void UD3D12RenderDevice::UpdateTextureRect(FTextureInfo& Info, INT U, INT V, INT UL, INT VL)
{
	guardSlow(UD3D12RenderDevice::UpdateTextureRect);

	Textures->UpdateTextureRect(&Info, U, V, UL, VL);

	unguardSlow;
}

#endif

void UD3D12RenderDevice::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet)
{
	guardSlow(UD3D12RenderDevice::DrawComplexSurface);

	DWORD PolyFlags = ApplyPrecedenceRules(Surface.PolyFlags);

	CachedTexture* tex = Textures->GetTexture(Surface.Texture, !!(PolyFlags & PF_Masked));
	CachedTexture* lightmap = Textures->GetTexture(Surface.LightMap, false);
	CachedTexture* macrotex = Textures->GetTexture(Surface.MacroTexture, false);
	CachedTexture* detailtex = Textures->GetTexture(Surface.DetailTexture, false);
	CachedTexture* fogmap = (Surface.FogMap && Surface.FogMap->Mips[0] && Surface.FogMap->Mips[0]->DataPtr) ? Textures->GetTexture(Surface.FogMap, false) : nullptr;

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

	SetPipeline(PolyFlags);
	SetDescriptorSet(PolyFlags, tex, lightmap, macrotex, detailtex);

	vec4 color(1.0f);

	for (FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next)
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

			UseVertices(vcount, icount);
		}
	}

	Stats.ComplexSurfaces++;

	if (!GIsEditor || (PolyFlags & (PF_Selected | PF_FlatShaded)) == 0)
		return;

	// Editor highlight surface (so stupid this is delegated to the renderdev as the engine could just issue a second call):

	SetPipeline(PF_Highlighted);
	SetDescriptorSet(PF_Highlighted);

	if (PolyFlags & PF_FlatShaded)
	{
		color.x = Surface.FlatColor.R / 255.0f;
		color.y = Surface.FlatColor.G / 255.0f;
		color.z = Surface.FlatColor.B / 255.0f;
		color.w = 0.85f;
		if (PolyFlags & PF_Selected)
		{
			color.x *= 1.5f;
			color.y *= 1.5f;
			color.z *= 1.5f;
			color.w = 1.0f;
		}
	}
	else
	{
		color = vec4(0.0f, 0.0f, 0.05f, 0.20f);
	}

	for (FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next)
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

			UseVertices(vcount, icount);
		}
	}

	unguardSlow;
}

void UD3D12RenderDevice::DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, int NumPts, DWORD PolyFlags, FSpanBuffer* Span)
{
	guardSlow(UD3D12RenderDevice::DrawGouraudPolygon);

	if (NumPts < 3) return; // This can apparently happen!!

	PolyFlags = ApplyPrecedenceRules(PolyFlags);

	CachedTexture* tex = Textures->GetTexture(&Info, !!(PolyFlags & PF_Masked));

	SetPipeline(PolyFlags);
	SetDescriptorSet(PolyFlags, tex);

	float UMult = GetUMult(Info);
	float VMult = GetVMult(Info);
	int flags = (PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated)) == PF_RenderFog ? 16 : 0;

	if ((PolyFlags & (PF_Translucent | PF_Modulated)) == 0 && LightMode == 2) flags |= 32;

	auto alloc = ReserveVertices(NumPts, (NumPts - 2) * 3);
	if (alloc.vptr)
	{
		SceneVertex* vptr = alloc.vptr;
		uint32_t* iptr = alloc.iptr;
		uint32_t vpos = alloc.vpos;

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

	unguardSlow;
}

#if defined(OLDUNREAL469SDK)

static void EnviroMap(const FSceneNode* Frame, FTransTexture& P, FLOAT UScale, FLOAT VScale)
{
	FVector T = P.Point.UnsafeNormal().MirrorByVector(P.Normal).TransformVectorBy(Frame->Uncoords);
	P.U = (T.X + 1.0f) * 0.5f * 256.0f * UScale;
	P.V = (T.Y + 1.0f) * 0.5f * 256.0f * VScale;
}

void UD3D12RenderDevice::DrawGouraudTriangles(const FSceneNode* Frame, const FTextureInfo& Info, FTransTexture* const Pts, INT NumPts, DWORD PolyFlags, DWORD DataFlags, FSpanBuffer* Span)
{
	guardSlow(UD3D12RenderDevice::DrawGouraudTriangles);

	if (NumPts < 3) return; // This can apparently happen!!

	PolyFlags = ApplyPrecedenceRules(PolyFlags);

	CachedTexture* tex = Textures->GetTexture(const_cast<FTextureInfo*>(&Info), !!(PolyFlags & PF_Masked));

	SetPipeline(PolyFlags);
	SetDescriptorSet(PolyFlags, tex);

	if (!ScenePass.VertexData || !ScenePass.IndexData) return;

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

	auto alloc = ReserveVertices(NumPts, (NumPts - 2) * 3);
	if (alloc.vptr)
	{
		SceneVertex* vptr = alloc.vptr;
		uint32_t* iptr = alloc.iptr;
		uint32_t vpos = alloc.vpos;

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

	unguardSlow;
}

#endif

void UD3D12RenderDevice::DrawTile(FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags)
{
	guardSlow(UD3D12RenderDevice::DrawTile);

	// stijn: fix for invisible actor icons in ortho viewports
	if (GIsEditor && Frame->Viewport->Actor && (Frame->Viewport->IsOrtho() || Abs(Z) <= SMALL_NUMBER))
	{
		Z = 1.f;
	}

	PolyFlags = ApplyPrecedenceRules(PolyFlags);

	CachedTexture* tex = Textures->GetTexture(&Info, !!(PolyFlags & PF_Masked));

	float UMult = tex ? GetUMult(Info) : 0.0f;
	float VMult = tex ? GetVMult(Info) : 0.0f;
	float u0 = U * UMult;
	float v0 = V * VMult;
	float u1 = (U + UL) * UMult;
	float v1 = (V + VL) * VMult;
	bool clamp = (u0 >= 0.0f && u1 <= 1.00001f && v0 >= 0.0f && v1 <= 1.00001f);

	SetPipeline(PolyFlags);
	SetDescriptorSet(PolyFlags, tex, nullptr, nullptr, nullptr, clamp);

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

	auto alloc = ReserveVertices(4, 6);
	if (alloc.vptr)
	{
		SceneVertex* vptr = alloc.vptr;
		uint32_t* iptr = alloc.iptr;
		uint32_t vpos = alloc.vpos;

		vptr[0] = { 0, vec3(RFX2 * Z * (X - Frame->FX2),      RFY2 * Z * (Y - Frame->FY2),      Z), vec2(U * UMult,        V * VMult),        vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(r, g, b, a), };
		vptr[1] = { 0, vec3(RFX2 * Z * (X + XL - Frame->FX2), RFY2 * Z * (Y - Frame->FY2),      Z), vec2((U + UL) * UMult, V * VMult),        vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(r, g, b, a), };
		vptr[2] = { 0, vec3(RFX2 * Z * (X + XL - Frame->FX2), RFY2 * Z * (Y + YL - Frame->FY2), Z), vec2((U + UL) * UMult, (V + VL) * VMult), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(r, g, b, a), };
		vptr[3] = { 0, vec3(RFX2 * Z * (X - Frame->FX2),      RFY2 * Z * (Y + YL - Frame->FY2), Z), vec2(U * UMult,        (V + VL) * VMult), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(r, g, b, a), };

		iptr[0] = vpos;
		iptr[1] = vpos + 1;
		iptr[2] = vpos + 2;
		iptr[3] = vpos;
		iptr[4] = vpos + 2;
		iptr[5] = vpos + 3;

		UseVertices(4, 6);
	}

	Stats.Tiles++;

	unguardSlow;
}

vec4 UD3D12RenderDevice::ApplyInverseGamma(vec4 color)
{
	if (Viewport->IsOrtho())
		return color;
	float brightness = Clamp(Viewport->GetOuterUClient()->Brightness * 2.0, 0.05, 2.99);
	float gammaRed = Max(brightness + GammaOffset + GammaOffsetRed, 0.001f);
	float gammaGreen = Max(brightness + GammaOffset + GammaOffsetGreen, 0.001f);
	float gammaBlue = Max(brightness + GammaOffset + GammaOffsetBlue, 0.001f);
	return vec4(pow(color.r, gammaRed), pow(color.g, gammaGreen), pow(color.b, gammaBlue), color.a);
}

void UD3D12RenderDevice::Draw3DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2)
{
	guard(UD3D12RenderDevice::Draw3DLine);

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
		SetPipeline(&ScenePass.LinePipeline[occlude], D3D_PRIMITIVE_TOPOLOGY_LINELIST);
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

void UD3D12RenderDevice::Draw2DClippedLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2)
{
	guard(UD3D12RenderDevice::Draw2DClippedLine);
	URenderDevice::Draw2DClippedLine(Frame, Color, LineFlags, P1, P2);
	unguard;
}

void UD3D12RenderDevice::Draw2DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2)
{
	guard(UD3D12RenderDevice::Draw2DLine);

#if defined(OLDUNREAL469SDK)
	bool occlude = !!(LineFlags & LINE_DepthCued);
#else
	bool occlude = OccludeLines;
#endif
	SetPipeline(&ScenePass.LinePipeline[occlude], D3D_PRIMITIVE_TOPOLOGY_LINELIST);
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

void UD3D12RenderDevice::Draw2DPoint(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z)
{
	guard(UD3D12RenderDevice::Draw2DPoint);

	// Hack to fix UED selection problem with selection brush
	if (GIsEditor) Z = 1.0f;

#if defined(OLDUNREAL469SDK)
	bool occlude = !!(LineFlags & LINE_DepthCued);
#else
	bool occlude = OccludeLines;
#endif
	SetPipeline(&ScenePass.PointPipeline[occlude], D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
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

void UD3D12RenderDevice::ClearZ(FSceneNode* Frame)
{
	guard(UD3D12RenderDevice::ClearZ);

	DrawBatches();

	Commands.Current->Draw->ClearDepthStencilView(SceneBuffers.SceneDSV.CPUHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	unguard;
}

void UD3D12RenderDevice::GetStats(TCHAR* Result)
{
	guard(UD3D12RenderDevice::GetStats);
	Result[0] = 0;
	unguard;
}

void UD3D12RenderDevice::ReadPixels(FColor* Pixels)
{
	guard(UD3D12RenderDevice::GetStats);

	ID3D12Resource* imageResource = nullptr;

	if (GammaCorrectScreenshots)
	{
		TransitionResourceBarrier(Commands.Current->Draw, SceneBuffers.PPImage[1], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

		D3D12_CPU_DESCRIPTOR_HANDLE rtv = SceneBuffers.PPImageRTV[1].CPUHandle();
		Commands.Current->Draw->SetGraphicsRootSignature(PresentPass.RootSignature);
		Commands.Current->Draw->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

		D3D12_VIEWPORT viewport = {};
		viewport.Width = CurrentSizeX;
		viewport.Height = CurrentSizeY;
		viewport.MaxDepth = 1.0f;
		Commands.Current->Draw->RSSetViewports(1, &viewport);

		D3D12_RECT box = {};
		box.right = CurrentSizeX;
		box.bottom = CurrentSizeY;
		Commands.Current->Draw->RSSetScissorRects(1, &box);

		PresentPushConstants pushconstants = GetPresentPushConstants();

		// Select present shader based on what the user is actually using
		int presentShader = 16; // output is always for rgba16f for PPImage[1]
		if (ActiveHdr) presentShader |= (1 | 16); // 1 = HDR in shader, 16 = output is rgba16f
		if (GammaMode == 1) presentShader |= 2;
		if (pushconstants.Brightness != 0.0f || pushconstants.Contrast != 1.0f || pushconstants.Saturation != 1.0f) presentShader |= (Clamp(GrayFormula, 0, 2) + 1) << 2;

		Commands.Current->Draw->SetPipelineState(PresentPass.Present[presentShader]);
		Commands.Current->Draw->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Commands.Current->Draw->IASetVertexBuffers(0, 1, &PresentPass.PPStepVertexBufferView);
		Commands.Current->Draw->SetGraphicsRootDescriptorTable(0, SceneBuffers.PresentSRVs.GPUHandle());
		Commands.Current->Draw->SetGraphicsRoot32BitConstants(1, sizeof(PresentPushConstants) / sizeof(uint32_t), &pushconstants, 0);
		Commands.Current->Draw->DrawInstanced(6, 1, 0, 0);

		TransitionResourceBarrier(Commands.Current->Draw, SceneBuffers.PPImage[1], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
		imageResource = SceneBuffers.PPImage[1];
	}
	else
	{
		TransitionResourceBarrier(Commands.Current->Draw, SceneBuffers.PPImage[0], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
		imageResource = SceneBuffers.PPImage[0];
	}

	D3D12_RESOURCE_DESC desc = imageResource->GetDesc();
	UINT64 totalSize = 0, rowSizeInBytes = 0;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
	UINT numRows = 0;
	Device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalSize);

	D3D12_HEAP_PROPERTIES readbackHeapProps = { D3D12_HEAP_TYPE_READBACK };

	D3D12_RESOURCE_DESC bufDesc = {};
	bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufDesc.Width = totalSize;
	bufDesc.Height = 1;
	bufDesc.DepthOrArraySize = 1;
	bufDesc.MipLevels = 1;
	bufDesc.SampleDesc.Count = 1;
	bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ComPtr<ID3D12Resource> buffer;
	HRESULT result = Device->CreateCommittedResource(
		&readbackHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		buffer.GetIID(),
		buffer.InitPtr());
	ThrowIfFailed(result, "CreateCommittedResource(ReadPixelsBuffer) failed");
	buffer->SetName(TEXT("ReadPixelsBuffer"));

	D3D12_TEXTURE_COPY_LOCATION src = {};
	src.pResource = imageResource;
	src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

	D3D12_TEXTURE_COPY_LOCATION dest = {};
	dest.pResource = buffer;
	dest.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dest.PlacedFootprint = footprint;

	Commands.Current->Draw->CopyTextureRegion(&dest, 0, 0, 0, &src, nullptr);

	TransitionResourceBarrier(Commands.Current->Draw, imageResource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	SubmitCommands(false);
	WaitDeviceIdle();

	void* data = nullptr;
	result = buffer->Map(0, nullptr, &data);
	if (SUCCEEDED(result))
	{
		uint8_t* srcpixels = (uint8_t*)data;
		int w = CurrentSizeX;
		int h = CurrentSizeY;
		void* data = Pixels;

		for (int y = 0; y < h; y++)
		{
			int desty = GammaCorrectScreenshots ? y : (h - y - 1);
			uint8_t* dest = (uint8_t*)data + desty * w * 4;
			uint16_t* src = (uint16_t*)(srcpixels + y * footprint.Footprint.RowPitch);
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

		D3D12_RANGE writtenRange = {};
		buffer->Unmap(0, &writtenRange);
	}

	unguard;
}

void UD3D12RenderDevice::EndFlash()
{
	guard(UD3D12RenderDevice::EndFlash);
	if (FlashScale != FPlane(0.5f, 0.5f, 0.5f, 0.0f) || FlashFog != FPlane(0.0f, 0.0f, 0.0f, 0.0f))
	{
		DrawBatches();

		vec4 color(FlashFog.X, FlashFog.Y, FlashFog.Z, 1.0f - Min(FlashScale.X * 2.0f, 1.0f));
		vec2 zero2(0.0f);

		SceneConstants.ObjectToProjection = mat4::identity();
		SceneConstants.NearClip = vec4(0.0f, 0.0f, 0.0f, 1.0f);

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

void UD3D12RenderDevice::SetSceneNode(FSceneNode* Frame)
{
	guardSlow(UD3D12RenderDevice::SetSceneNode);

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
	Commands.Current->Draw->RSSetViewports(1, &SceneViewport);

	SceneConstants.ObjectToProjection = mat4::frustum(-RProjZ, RProjZ, -Aspect * RProjZ, Aspect * RProjZ, 1.0f, 32768.0f, handedness::left, clipzrange::zero_positive_w);
	SceneConstants.NearClip = vec4(Frame->NearClip.X, Frame->NearClip.Y, Frame->NearClip.Z, Frame->NearClip.W);

	unguardSlow;
}

void UD3D12RenderDevice::PrecacheTexture(FTextureInfo& Info, DWORD PolyFlags)
{
	guard(UD3D12RenderDevice::PrecacheTexture);
	PolyFlags = ApplyPrecedenceRules(PolyFlags);
	Textures->GetTexture(&Info, !!(PolyFlags & PF_Masked));
	unguard;
}

void UD3D12RenderDevice::ClearTextureCache()
{
	DrawBatches(true);
	WaitDeviceIdle();
	Textures->ClearCache();
	for (auto& it : Descriptors.Tex)
		it.second.reset();
	Descriptors.Tex.clear();
}

void UD3D12RenderDevice::AddDrawBatch()
{
	if (Batch.SceneIndexStart != ScenePass.IndexPos)
	{
		Batch.SceneIndexEnd = ScenePass.IndexPos;
		QueuedBatches.push_back(Batch);
		Batch.SceneIndexStart = ScenePass.IndexPos;
	}
}

void UD3D12RenderDevice::DrawBatches(bool nextBuffer)
{
	AddDrawBatch();

	if (!QueuedBatches.empty())
	{
		D3D12_CPU_DESCRIPTOR_HANDLE views[2] = { SceneBuffers.SceneRTVs.CPUHandle(0), SceneBuffers.SceneRTVs.CPUHandle(1) };
		D3D12_CPU_DESCRIPTOR_HANDLE depthview = SceneBuffers.SceneDSV.CPUHandle();

		Commands.Current->Draw->SetGraphicsRootSignature(ScenePass.RootSignature);
		Commands.Current->Draw->SetGraphicsRoot32BitConstants(2, sizeof(ScenePushConstants) / sizeof(uint32_t), &SceneConstants, 0);
		Commands.Current->Draw->OMSetRenderTargets(2, views, FALSE, &depthview);
		Commands.Current->Draw->IASetPrimitiveTopology(Commands.PrimitiveTopology);
		Commands.Current->Draw->IASetVertexBuffers(0, 1, &ScenePass.VertexBufferView);
		Commands.Current->Draw->IASetIndexBuffer(&ScenePass.IndexBufferView);

		for (const DrawBatchEntry& entry : QueuedBatches)
			DrawEntry(entry);
		QueuedBatches.clear();

		if (nextBuffer)
		{
			SubmitCommands(false);
		}
	}

	Batch.SceneIndexStart = ScenePass.IndexPos;
}

void UD3D12RenderDevice::DrawEntry(const DrawBatchEntry& entry)
{
	size_t icount = entry.SceneIndexEnd - entry.SceneIndexStart;

	if (SceneViewport.MinDepth != entry.Pipeline->MinDepth || SceneViewport.MaxDepth != entry.Pipeline->MaxDepth)
	{
		SceneViewport.MinDepth = entry.Pipeline->MinDepth;
		SceneViewport.MaxDepth = entry.Pipeline->MaxDepth;
		Commands.Current->Draw->RSSetViewports(1, &SceneViewport);
	}

	DescriptorSet& common = Descriptors.Tex[{ entry.Tex, entry.Lightmap, entry.Detailtex, entry.Macrotex }];
	if (!common)
	{
		common = Heaps.Common->Alloc(4);
		Device->CreateShaderResourceView(entry.Tex ? entry.Tex->Texture : Textures->GetNullTexture()->Texture, nullptr, common.CPUHandle(0));
		Device->CreateShaderResourceView(entry.Lightmap ? entry.Lightmap->Texture : Textures->GetNullTexture()->Texture, nullptr, common.CPUHandle(1));
		Device->CreateShaderResourceView(entry.Macrotex ? entry.Macrotex->Texture : Textures->GetNullTexture()->Texture, nullptr, common.CPUHandle(2));
		Device->CreateShaderResourceView(entry.Detailtex ? entry.Detailtex->Texture : Textures->GetNullTexture()->Texture, nullptr, common.CPUHandle(3));
	}

	DescriptorSet& sampler = Descriptors.Sampler[(entry.TexSamplerMode << 16) | (entry.DetailtexSamplerMode << 8) | entry.MacrotexSamplerMode];
	if (!sampler)
	{
		sampler = Heaps.Sampler->Alloc(4);
		Device->CreateSampler(&ScenePass.Samplers[entry.TexSamplerMode], sampler.CPUHandle(0));
		Device->CreateSampler(&ScenePass.Samplers[0], sampler.CPUHandle(1));
		Device->CreateSampler(&ScenePass.Samplers[entry.MacrotexSamplerMode], sampler.CPUHandle(2));
		Device->CreateSampler(&ScenePass.Samplers[entry.DetailtexSamplerMode], sampler.CPUHandle(3));
	}

	if (entry.PrimitiveTopology != Commands.PrimitiveTopology)
	{
		Commands.PrimitiveTopology = entry.PrimitiveTopology;
		Commands.Current->Draw->IASetPrimitiveTopology(Commands.PrimitiveTopology);
	}

	Commands.Current->Draw->SetPipelineState(entry.Pipeline->Pipeline);
	Commands.Current->Draw->SetGraphicsRootDescriptorTable(0, common.GPUHandle());
	Commands.Current->Draw->SetGraphicsRootDescriptorTable(1, sampler.GPUHandle());
	Commands.Current->Draw->DrawIndexedInstanced(icount, 1, ScenePass.IndexBase + entry.SceneIndexStart, 0, 0);

	Stats.DrawCalls++;
}

ComPtr<ID3D12RootSignature> UD3D12RenderDevice::CreateRootSignature(const char* name, const std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>>& descriptorTables, const D3D12_ROOT_CONSTANTS& pushConstants, const std::vector<D3D12_STATIC_SAMPLER_DESC>& staticSamplers)
{
	std::vector<D3D12_ROOT_PARAMETER> parameters;

	for (const std::vector<D3D12_DESCRIPTOR_RANGE>& table : descriptorTables)
	{
		D3D12_ROOT_PARAMETER param = {};
		param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		param.DescriptorTable.NumDescriptorRanges = table.size();
		param.DescriptorTable.pDescriptorRanges = table.data();
		parameters.push_back(param);
	}

	if (pushConstants.Num32BitValues > 0)
	{
		D3D12_ROOT_PARAMETER param = {};
		param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		param.Constants = pushConstants;
		parameters.push_back(param);
	}

	D3D12_ROOT_SIGNATURE_DESC desc = {};
	desc.NumParameters = parameters.size();
	desc.pParameters = parameters.data();
	desc.NumStaticSamplers = staticSamplers.size();
	desc.pStaticSamplers = staticSamplers.data();
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ComPtr<ID3DBlob> signatureblob, error;
	HRESULT result = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, signatureblob.TypedInitPtr(), error.TypedInitPtr());
	if (FAILED(result))
	{
		std::string text = "Could not serialize ";
		text += name;
		if (error)
		{
			text += ": ";
			text.append((const char*)error->GetBufferPointer(), error->GetBufferSize());
		}
		throw std::runtime_error(text);
	}

	ComPtr<ID3D12RootSignature> signature;
	result = Device->CreateRootSignature(0, signatureblob->GetBufferPointer(), signatureblob->GetBufferSize(), signature.GetIID(), signature.InitPtr());
	if (FAILED(result))
	{
		std::string text = "Could not create ";
		text += name;
		throw std::runtime_error(text);
	}

	return signature;
}

std::vector<uint8_t> UD3D12RenderDevice::CompileHlsl(const std::string& filename, const std::string& shadertype, const std::vector<std::string> defines)
{
	std::string code = FileResource::readAllText(filename);

	std::string target;
	switch (FeatureLevel)
	{
	default:
	// case D3D_FEATURE_LEVEL_12_2: target = shadertype + "_6_5"; break;
	case D3D_FEATURE_LEVEL_12_1: target = shadertype + "_5_0"; break;
	case D3D_FEATURE_LEVEL_12_0: target = shadertype + "_5_0"; break;
	case D3D_FEATURE_LEVEL_11_1: target = shadertype + "_5_0"; break;
	case D3D_FEATURE_LEVEL_11_0: target = shadertype + "_5_0"; break;
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

	ComPtr<ID3DBlob> blob, errors;
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

void UD3D12RenderDevice::OnDebugMessage(D3D12_MESSAGE_CATEGORY category, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID id, LPCSTR description, void* context)
{
	UD3D12RenderDevice* self = (UD3D12RenderDevice*)context;

	const char* severitystr = "unknown";
	switch (severity)
	{
	case D3D12_MESSAGE_SEVERITY_CORRUPTION: severitystr = "corruption"; break;
	case D3D12_MESSAGE_SEVERITY_ERROR: severitystr = "error"; break;
	case D3D12_MESSAGE_SEVERITY_WARNING: severitystr = "warning"; break;
	case D3D12_MESSAGE_SEVERITY_INFO: severitystr = "info"; break;
	case D3D12_MESSAGE_SEVERITY_MESSAGE: severitystr = "message"; break;
	}

	debugf(TEXT("[%s] %s"), to_utf16(severitystr).c_str(), to_utf16(description).c_str());
}

void ThrowError(HRESULT result, const char* msg)
{
	std::string message = msg;

	switch (result)
	{
	case D3D12_ERROR_ADAPTER_NOT_FOUND: message += " (D3D12_ERROR_ADAPTER_NOT_FOUND)"; break;
	case D3D12_ERROR_DRIVER_VERSION_MISMATCH: message += " (D3D12_ERROR_DRIVER_VERSION_MISMATCH)"; break;
	case DXGI_ERROR_INVALID_CALL: message += " (DXGI_ERROR_INVALID_CALL)"; break;
	case DXGI_ERROR_WAS_STILL_DRAWING: message += " (DXGI_ERROR_WAS_STILL_DRAWING)"; break;
	case DXGI_ERROR_ACCESS_DENIED: message += " (DXGI_ERROR_ACCESS_DENIED)"; break;
	case DXGI_ERROR_ACCESS_LOST: message += " (DXGI_ERROR_ACCESS_LOST)"; break;
	case DXGI_ERROR_ALREADY_EXISTS: message += " (DXGI_ERROR_ALREADY_EXISTS)"; break;
	case DXGI_ERROR_CANNOT_PROTECT_CONTENT: message += " (DXGI_ERROR_CANNOT_PROTECT_CONTENT)"; break;
	case DXGI_ERROR_DEVICE_HUNG: message += " (DXGI_ERROR_DEVICE_HUNG)"; break;
	case DXGI_ERROR_DEVICE_REMOVED: message += " (DXGI_ERROR_DEVICE_REMOVED)"; break;
	case DXGI_ERROR_DEVICE_RESET: message += " (DXGI_ERROR_DEVICE_RESET)"; break;
	case DXGI_ERROR_DRIVER_INTERNAL_ERROR: message += " (DXGI_ERROR_DRIVER_INTERNAL_ERROR)"; break;
	case DXGI_ERROR_FRAME_STATISTICS_DISJOINT: message += " (DXGI_ERROR_FRAME_STATISTICS_DISJOINT)"; break;
	case DXGI_ERROR_GRAPHICS_VIDPN_SOURCE_IN_USE: message += " (DXGI_ERROR_GRAPHICS_VIDPN_SOURCE_IN_USE)"; break;
	case DXGI_ERROR_MORE_DATA: message += " (DXGI_ERROR_MORE_DATA)"; break;
	case DXGI_ERROR_NAME_ALREADY_EXISTS: message += " (DXGI_ERROR_NAME_ALREADY_EXISTS)"; break;
	case DXGI_ERROR_NONEXCLUSIVE: message += " (DXGI_ERROR_NONEXCLUSIVE)"; break;
	case DXGI_ERROR_NOT_CURRENTLY_AVAILABLE: message += " (DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)"; break;
	case DXGI_ERROR_NOT_FOUND: message += " (DXGI_ERROR_NOT_FOUND)"; break;
	case DXGI_ERROR_RESTRICT_TO_OUTPUT_STALE: message += " (DXGI_ERROR_RESTRICT_TO_OUTPUT_STALE)"; break;
	case DXGI_ERROR_SDK_COMPONENT_MISSING: message += " (DXGI_ERROR_SDK_COMPONENT_MISSING)"; break;
	case DXGI_ERROR_SESSION_DISCONNECTED: message += " (DXGI_ERROR_SESSION_DISCONNECTED)"; break;
	case DXGI_ERROR_UNSUPPORTED: message += " (DXGI_ERROR_UNSUPPORTED)"; break;
	case DXGI_ERROR_WAIT_TIMEOUT: message += " (DXGI_ERROR_WAIT_TIMEOUT)"; break;
	case E_FAIL: message += " (E_FAIL)"; break;
	case E_INVALIDARG: message += " (E_INVALIDARG)"; break;
	case E_OUTOFMEMORY: message += " (E_OUTOFMEMORY)"; break;
	case E_NOTIMPL: message += " (E_NOTIMPL)"; break;
	default: message += " (HRESULT " + std::to_string(result) + ")"; break;
	}

	throw std::runtime_error(message);
}
