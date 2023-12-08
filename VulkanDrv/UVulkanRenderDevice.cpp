
#include "Precomp.h"
#include "UVulkanRenderDevice.h"
#include "CachedTexture.h"
#include "UTF16.h"

IMPLEMENT_CLASS(UVulkanRenderDevice);

UVulkanRenderDevice::UVulkanRenderDevice()
{
}

void UVulkanRenderDevice::StaticConstructor()
{
	guard(UVulkanRenderDevice::StaticConstructor);

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
	Bloom = 0;
	BloomAmount = 128;

	LODBias = -0.5f;
	LightMode = 0;

	VkDeviceIndex = 0;
	VkDebug = 0;
	VkExclusiveFullscreen = 0;

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

	new(GetClass(), TEXT("VkDeviceIndex"), RF_Public) UIntProperty(CPP_PROPERTY(VkDeviceIndex), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("VkDebug"), RF_Public) UBoolProperty(CPP_PROPERTY(VkDebug), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("VkExclusiveFullscreen"), RF_Public) UBoolProperty(CPP_PROPERTY(VkExclusiveFullscreen), TEXT("Display"), CPF_Config);

	unguard;
}

void VulkanPrintLog(const char* typestr, const std::string& msg)
{
	debugf(TEXT("[%s] %s"), to_utf16(typestr).c_str(), to_utf16(msg).c_str());
}

void VulkanError(const char* text)
{
	throw std::runtime_error(text);
}

UBOOL UVulkanRenderDevice::Init(UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen)
{
	guard(UVulkanRenderDevice::Init);

	Viewport = InViewport;

	try
	{
		auto instance = VulkanInstanceBuilder()
			.RequireSurfaceExtensions()
			.DebugLayer(VkDebug)
			.Create();

		auto surface = VulkanSurfaceBuilder()
			.Win32Window((HWND)Viewport->GetWindow())
			.Create(instance);

		Device = VulkanDeviceBuilder()
			.Surface(surface)
			.OptionalDescriptorIndexing()
			.RequireExtension(VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME)
			.SelectDevice(VkDeviceIndex)
			.Create(instance);

		SupportsBindless =
			Device->EnabledFeatures.DescriptorIndexing.descriptorBindingPartiallyBound &&
			Device->EnabledFeatures.DescriptorIndexing.runtimeDescriptorArray &&
			Device->EnabledFeatures.DescriptorIndexing.shaderSampledImageArrayNonUniformIndexing;

		Commands.reset(new CommandBufferManager(this));
		Samplers.reset(new SamplerManager(this));
		Textures.reset(new TextureManager(this));
		Buffers.reset(new BufferManager(this));
		Shaders.reset(new ShaderManager(this));
		Uploads.reset(new UploadManager(this));
		DescriptorSets.reset(new DescriptorSetManager(this));
		RenderPasses.reset(new RenderPassManager(this));
		Framebuffers.reset(new FramebufferManager(this));

		UsesBindless = SupportsBindless;

		const auto& props = Device->PhysicalDevice.Properties.Properties;

		FString deviceType;
		switch (props.deviceType)
		{
		case VK_PHYSICAL_DEVICE_TYPE_OTHER: deviceType = TEXT("other"); break;
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: deviceType = TEXT("integrated gpu"); break;
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: deviceType = TEXT("discrete gpu"); break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: deviceType = TEXT("virtual gpu"); break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU: deviceType = TEXT("cpu"); break;
		default: deviceType = FString::Printf(TEXT("%d"), (int)props.deviceType); break;
		}

		FString apiVersion, driverVersion;
		apiVersion = FString::Printf(TEXT("%d.%d.%d"), VK_VERSION_MAJOR(props.apiVersion), VK_VERSION_MINOR(props.apiVersion), VK_VERSION_PATCH(props.apiVersion));
		driverVersion = FString::Printf(TEXT("%d.%d.%d"), VK_VERSION_MAJOR(props.driverVersion), VK_VERSION_MINOR(props.driverVersion), VK_VERSION_PATCH(props.driverVersion));

		debugf(TEXT("Vulkan device: %s"), to_utf16(props.deviceName).c_str());
		debugf(TEXT("Vulkan device type: %s"), *deviceType);
		debugf(TEXT("Vulkan version: %s (api) %s (driver)"), *apiVersion, *driverVersion);

		if (VkDebug)
		{
			debugf(TEXT("Vulkan extensions:"));
			for (const VkExtensionProperties& p : Device->PhysicalDevice.Extensions)
			{
				debugf(TEXT(" %s"), to_utf16(p.extensionName).c_str());
			}
		}

		const auto& limits = props.limits;
		debugf(TEXT("Max. texture size: %d"), limits.maxImageDimension2D);
		debugf(TEXT("Max. uniform buffer range: %d"), limits.maxUniformBufferRange);
		debugf(TEXT("Min. uniform buffer offset alignment: %llu"), limits.minUniformBufferOffsetAlignment);

		if (SupportsBindless)
			debugf(TEXT("GPU supports bindless textures"));
		else
			debugf(TEXT("GPU does not support bindless textures"));

		if (UsesBindless)
			debugf(TEXT("Vulkan driver is using bindless textures"));
		else
			debugf(TEXT("Vulkan driver is not using bindless textures"));
	}
	catch (const std::exception& e)
	{
		debugf(TEXT("Could not create vulkan renderer: %s"), to_utf16(e.what()).c_str());
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

UBOOL UVulkanRenderDevice::SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen)
{
	guard(UVulkanRenderDevice::SetRes);

	if (NewX == 0 || NewY == 0)
		return 1;

	if (!Viewport->ResizeViewport(Fullscreen ? (BLIT_Fullscreen | BLIT_OpenGL) : (BLIT_HardwarePaint | BLIT_OpenGL), NewX, NewY, NewColorBytes))
		return 0;

#ifdef USE_HORRIBLE_WIN32_MODE_SWITCHES
	if (Fullscreen)
	{
		DEVMODE devmode = {};
		devmode.dmSize = sizeof(DEVMODE);
		devmode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT; // | DM_DISPLAYFREQUENCY
		devmode.dmPelsWidth = NewX;
		devmode.dmPelsHeight = NewY;
		// devmode.dmDisplayFrequency = 360;
		ChangeDisplaySettingsEx(nullptr, &devmode, 0, CDS_FULLSCREEN, 0);
	}
#endif

	SaveConfig();

#if defined(UNREALGOLD)
	Flush();
#else
	Flush(1);
#endif

	return 1;
	unguard;
}

void UVulkanRenderDevice::Exit()
{
	guard(UVulkanRenderDevice::Exit);

	if (Device) vkDeviceWaitIdle(Device->device);

#ifdef USE_HORRIBLE_WIN32_MODE_SWITCHES
	ChangeDisplaySettingsEx(nullptr, nullptr, 0, 0, 0);
#endif

	Framebuffers.reset();
	RenderPasses.reset();
	DescriptorSets.reset();
	Uploads.reset();
	Shaders.reset();
	Buffers.reset();
	Textures.reset();
	Samplers.reset();
	Commands.reset();

	Device.reset();

	unguard;
}

void UVulkanRenderDevice::SubmitAndWait(bool present, int presentWidth, int presentHeight, bool presentFullscreen)
{
	if (UsesBindless)
		DescriptorSets->UpdateBindlessSet();

	Commands->SubmitCommands(present, presentWidth, presentHeight, presentFullscreen);

	Batch.Pipeline = nullptr;
	Batch.DescriptorSet = nullptr;
	Batch.SceneIndexStart = 0;
	SceneVertexPos = 0;
	SceneIndexPos = 0;
}

#if defined(UNREALGOLD)

void UVulkanRenderDevice::Flush()
{
	guard(UVulkanRenderDevice::Flush);

	if (IsLocked)
	{
		DrawBatch(Commands->GetDrawCommands());
		RenderPasses->EndScene(Commands->GetDrawCommands());
		SubmitAndWait(false, 0, 0, false);

		ClearTextureCache();

		auto cmdbuffer = Commands->GetDrawCommands();
		RenderPasses->BeginScene(cmdbuffer, 0.0f, 0.0f, 0.0f, 1.0f);

		VkBuffer vertexBuffers[] = { Buffers->SceneVertexBuffer->buffer };
		VkDeviceSize offsets[] = { 0 };
		cmdbuffer->bindVertexBuffers(0, 1, vertexBuffers, offsets);
		cmdbuffer->bindIndexBuffer(Buffers->SceneIndexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
	}
	else
	{
		ClearTextureCache();
	}

	if (UsePrecache && !GIsEditor)
		PrecacheOnFlip = 1;

	unguard;
}

#else

void UVulkanRenderDevice::Flush(UBOOL AllowPrecache)
{
	guard(UVulkanRenderDevice::Flush);

	if (IsLocked)
	{
		DrawBatch(Commands->GetDrawCommands());
		RenderPasses->EndScene(Commands->GetDrawCommands());
		SubmitAndWait(false, 0, 0, false);

		ClearTextureCache();

		auto cmdbuffer = Commands->GetDrawCommands();
		RenderPasses->BeginScene(cmdbuffer, 0.0f, 0.0f, 0.0f, 1.0f);

		VkBuffer vertexBuffers[] = { Buffers->SceneVertexBuffer->buffer };
		VkDeviceSize offsets[] = { 0 };
		cmdbuffer->bindVertexBuffers(0, 1, vertexBuffers, offsets);
		cmdbuffer->bindIndexBuffer(Buffers->SceneIndexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
	}
	else
	{
		ClearTextureCache();
	}

	if (AllowPrecache && UsePrecache && !GIsEditor)
		PrecacheOnFlip = 1;

	unguard;
}

#endif

UBOOL UVulkanRenderDevice::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	guard(UVulkanRenderDevice::Exec);

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

#ifdef WIN32
		// Always include what the monitor is currently using
		HDC screenDC = GetDC(0);
		int screenWidth = GetDeviceCaps(screenDC, HORZRES);
		int screenHeight = GetDeviceCaps(screenDC, VERTRES);
		resolutions.insert({ screenWidth, screenHeight });
		ReleaseDC(0, screenDC);

#ifdef USE_HORRIBLE_WIN32_MODE_SWITCHES
		// Get what else is available according to Windows
		DEVMODE devmode = {};
		devmode.dmSize = sizeof(DEVMODE);
		int i = 0;
		while (EnumDisplaySettingsEx(nullptr, i++, &devmode, 0) != 0)
		{
			if ((devmode.dmFields & (DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT)) == (DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT) && devmode.dmBitsPerPel >= 24)
			{
				resolutions.insert({ (int)devmode.dmPelsWidth, (int)devmode.dmPelsHeight });
			}

			devmode = {};
			devmode.dmSize = sizeof(DEVMODE);
		}

		// Add a letterboxed 4:3 mode for widescreen monitors
		resolutions.insert({ (screenHeight * 4 + 2) / 3, screenHeight });

		// Include a few classics from the era
		resolutions.insert({ 800, 600 });
		resolutions.insert({ 1024, 768 });
		resolutions.insert({ 1600, 1200 });
#endif
#else
		resolutions.insert({ 800, 600 });
		resolutions.insert({ 1280, 720 });
		resolutions.insert({ 1024, 768 });
		resolutions.insert({ 1280, 768 });
		resolutions.insert({ 1152, 864 });
		resolutions.insert({ 1280, 900 });
		resolutions.insert({ 1280, 1024 });
		resolutions.insert({ 1400, 1024 });
		resolutions.insert({ 1920, 1080 });
		resolutions.insert({ 2560, 1440 });
		resolutions.insert({ 2560, 1600 });
		resolutions.insert({ 3840, 2160 });
#endif

		FString Str;
		for (const Resolution& resolution : resolutions)
		{
			Str += FString::Printf(TEXT("%ix%i "), (INT)resolution.X, (INT)resolution.Y);
		}
		Ar.Log(*Str.LeftChop(1));
		return 1;
	}
	else if (ParseCommand(&Cmd, TEXT("GetVkDevices")))
	{
		std::vector<VulkanCompatibleDevice> supportedDevices = VulkanDeviceBuilder()
			.Surface(Device->Surface)
			.OptionalDescriptorIndexing()
			.FindDevices(Device->Instance);

		for (size_t i = 0; i < supportedDevices.size(); i++)
		{
			Ar.Log(FString::Printf(TEXT("#%d - %s\r\n"), (int)i, to_utf16(supportedDevices[i].Device->Properties.Properties.deviceName)));
		}
		return 1;
	}
	else
	{
#if !defined(UNREALGOLD)
		return Super::Exec(Cmd, Ar);
#else
		return 0;
#endif
	}

	unguard;
}

void UVulkanRenderDevice::Lock(FPlane InFlashScale, FPlane InFlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* InHitData, INT* InHitSize)
{
	guard(UVulkanRenderDevice::Lock);

	HitData = InHitData;
	HitSize = InHitSize;

	FlashScale = InFlashScale;
	FlashFog = InFlashFog;

	pushconstants.hitIndex = 0;
	ForceHitIndex = -1;

	try
	{
		// If frame textures no longer match the window or user settings, recreate them along with the swap chain
		if (!Textures->Scene || Textures->Scene->Width != Viewport->SizeX || Textures->Scene->Height != Viewport->SizeY ||Textures->Scene->Multisample != GetSettingsMultisample())
		{
			Framebuffers->DestroySceneFramebuffer();
			Textures->Scene.reset();
			Textures->Scene.reset(new SceneTextures(this, Viewport->SizeX, Viewport->SizeY, GetSettingsMultisample()));
			RenderPasses->CreateRenderPass();
			RenderPasses->CreatePipelines();
			Framebuffers->CreateSceneFramebuffer();
			DescriptorSets->UpdateFrameDescriptors();
		}

		PipelineBarrier()
			.AddImage(
				Textures->Scene->ColorBuffer.get(),
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_ACCESS_SHADER_READ_BIT,
				VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT)
			.AddImage(
				Textures->Scene->HitBuffer.get(),
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_ACCESS_SHADER_READ_BIT,
				VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT)
			.Execute(Commands->GetDrawCommands(), VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		auto cmdbuffer = Commands->GetDrawCommands();
		RenderPasses->BeginScene(cmdbuffer, ScreenClear.X, ScreenClear.Y, ScreenClear.Z, ScreenClear.W);

		VkBuffer vertexBuffers[] = { Buffers->SceneVertexBuffer->buffer };
		VkDeviceSize offsets[] = { 0 };
		cmdbuffer->bindVertexBuffers(0, 1, vertexBuffers, offsets);
		cmdbuffer->bindIndexBuffer(Buffers->SceneIndexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);

		IsLocked = true;
	}
	catch (const std::exception& e)
	{
		// To do: can we report this back to unreal in a better way?
		MessageBoxA(0, e.what(), "Vulkan Error", MB_OK);
		exit(0);
	}

	unguard;
}

void UVulkanRenderDevice::DrawStats(FSceneNode* Frame)
{
	Super::DrawStats(Frame);

#if defined(OLDUNREAL469SDK)
	GRender->ShowStat(CurrentFrame, TEXT("Vulkan: Draw calls: %d, Complex surfaces: %d, Gouraud polygons: %d, Tiles: %d; Uploads: %d, Rect Uploads: %d\r\n"), Stats.DrawCalls, Stats.ComplexSurfaces, Stats.GouraudPolygons, Stats.Tiles, Stats.Uploads, Stats.RectUploads);
#endif

	Stats.DrawCalls = 0;
	Stats.ComplexSurfaces = 0;
	Stats.GouraudPolygons = 0;
	Stats.Tiles = 0;
	Stats.Uploads = 0;
	Stats.RectUploads = 0;
}

void UVulkanRenderDevice::Unlock(UBOOL Blit)
{
	guard(UVulkanRenderDevice::Unlock);

	try
	{
		DrawBatch(Commands->GetDrawCommands());
		RenderPasses->EndScene(Commands->GetDrawCommands());

		BlitSceneToPostprocess();
		if (Bloom)
		{
			RunBloomPass();
		}
		SubmitAndWait(Blit ? true : false, Viewport->SizeX, Viewport->SizeY, Viewport->IsFullscreen());

		if (HitData)
		{
			// Look for the last hit
			int width = Viewport->HitXL;
			int height = Viewport->HitYL;
			int hit = 0;
			const int32_t* data = (const int32_t*)Textures->Scene->StagingHitBuffer->Map(0, width * height * sizeof(int32_t));
			if (data)
			{
				for (int y = 0; y < height; y++)
				{
					const INT* line = data + y * width;
					for (int x = 0; x < width; x++)
					{
						hit = std::max(hit, line[x]);
					}
				}
				Textures->Scene->StagingHitBuffer->Unmap();
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

		HitQueryStack.clear();
		HitQueries.clear();
		HitBuffer.clear();
		HitData = nullptr;
		HitSize = nullptr;

		IsLocked = false;
	}
	catch (std::exception& e)
	{
		static std::wstring err;
		err = to_utf16(e.what());
		appUnwindf(TEXT("%s"), err.c_str());
	}

	unguard;
}

#if defined(OLDUNREAL469SDK)

UBOOL UVulkanRenderDevice::SupportsTextureFormat(ETextureFormat Format)
{
	guard(UVulkanRenderDevice::SupportsTextureFormat);

	return Uploads->SupportsTextureFormat(Format) ? TRUE : FALSE;

	unguard;
}

void UVulkanRenderDevice::UpdateTextureRect(FTextureInfo& Info, INT U, INT V, INT UL, INT VL)
{
	guardSlow(UVulkanRenderDevice::UpdateTextureRect);

	Textures->UpdateTextureRect(&Info, U, V, UL, VL);

	unguardSlow;
}

#endif

void UVulkanRenderDevice::DrawBatch(VulkanCommandBuffer* cmdbuffer)
{
	size_t icount = SceneIndexPos - Batch.SceneIndexStart;
	if (icount > 0)
	{
		auto layout = Batch.Bindless ? RenderPasses->Scene.BindlessPipelineLayout.get() : RenderPasses->Scene.PipelineLayout.get();
		cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, Batch.Pipeline);
		cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, Batch.DescriptorSet);
		cmdbuffer->pushConstants(layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ScenePushConstants), &pushconstants);
		cmdbuffer->drawIndexed(icount, 1, Batch.SceneIndexStart, 0, 0);
		Batch.SceneIndexStart = SceneIndexPos;
		Stats.DrawCalls++;
	}
}

void UVulkanRenderDevice::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet)
{
	guardSlow(UVulkanRenderDevice::DrawComplexSurface);

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
	float DetailUPan = UPan;
	float DetailVPan = VPan;
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

	SetPipeline(RenderPasses->GetPipeline(Surface.PolyFlags, UsesBindless));

	ivec4 textureBinds = SetDescriptorSet(Surface.PolyFlags, tex, lightmap, macrotex, detailtex);
	vec4 color(1.0f);

	// Draw the surface twice if the editor selected it. Second time highlighted without textures
	int drawcount = (Surface.PolyFlags & PF_Selected) && GIsEditor ? 2 : 1;
	while (drawcount-- > 0)
	{
		uint32_t vpos = SceneVertexPos;
		uint32_t ipos = SceneIndexPos;

		SceneVertex* vptr = Buffers->SceneVertices + vpos;
		uint32_t* iptr = Buffers->SceneIndexes + ipos;

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
				vptr->TextureBinds = textureBinds;
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
			SetPipeline(RenderPasses->GetPipeline(PF_Highlighted, UsesBindless));
			textureBinds = SetDescriptorSet(PF_Highlighted, nullptr);
			color = vec4(0.0f, 0.0f, 0.05f, 0.20f);
		}
	}

	Stats.ComplexSurfaces++;

	unguardSlow;
}

void UVulkanRenderDevice::DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, int NumPts, DWORD PolyFlags, FSpanBuffer* Span)
{
	guardSlow(UVulkanRenderDevice::DrawGouraudPolygon);

	if (NumPts < 3) return; // This can apparently happen!!

	SetPipeline(RenderPasses->GetPipeline(PolyFlags, UsesBindless));

	CachedTexture* tex = Textures->GetTexture(&Info, !!(PolyFlags & PF_Masked));
	ivec4 textureBinds = SetDescriptorSet(PolyFlags, tex);

	float UMult = GetUMult(Info);
	float VMult = GetVMult(Info);
	int flags = (PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated)) == PF_RenderFog ? 16 : 0;

	if ((PolyFlags & (PF_Translucent | PF_Modulated)) == 0 && LightMode == 2) flags |= 32;

	if (PolyFlags & PF_Modulated)
	{
		SceneVertex* vertex = &Buffers->SceneVertices[SceneVertexPos];
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
			vertex->TextureBinds = textureBinds;
			vertex++;
		}
	}
	else
	{
		SceneVertex* vertex = &Buffers->SceneVertices[SceneVertexPos];
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
			vertex->TextureBinds = textureBinds;
			vertex++;
		}
	}

	size_t vstart = SceneVertexPos;
	size_t vcount = NumPts;
	size_t istart = SceneIndexPos;
	size_t icount = (vcount - 2) * 3;

	uint32_t* iptr = Buffers->SceneIndexes + istart;
	for (uint32_t i = vstart + 2; i < vstart + vcount; i++)
	{
		*(iptr++) = vstart;
		*(iptr++) = i - 1;
		*(iptr++) = i;
	}

	SceneVertexPos += vcount;
	SceneIndexPos += icount;

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

void UVulkanRenderDevice::DrawGouraudTriangles(const FSceneNode* Frame, const FTextureInfo& Info, FTransTexture* const Pts, INT NumPts, DWORD PolyFlags, DWORD DataFlags, FSpanBuffer* Span)
{
	guardSlow(UVulkanRenderDevice::DrawGouraudTriangles);

	if (NumPts < 3) return; // This can apparently happen!!

	SetPipeline(RenderPasses->GetPipeline(PolyFlags, UsesBindless));

	CachedTexture* tex = Textures->GetTexture(const_cast<FTextureInfo*>(&Info), !!(PolyFlags & PF_Masked));
	ivec4 textureBinds = SetDescriptorSet(PolyFlags, tex);

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
		SceneVertex* vertex = &Buffers->SceneVertices[SceneVertexPos];
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
			vertex->TextureBinds = textureBinds;
			vertex++;
		}
	}
	else
	{
		SceneVertex* vertex = &Buffers->SceneVertices[SceneVertexPos];
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
			vertex->TextureBinds = textureBinds;
			vertex++;
		}
	}

	bool mirror = (Frame->Mirror == -1.0);

	size_t vstart = SceneVertexPos;
	size_t vcount = NumPts;
	size_t istart = SceneIndexPos;
	size_t icount = 0;

	if (PolyFlags & PF_TwoSided)
	{
		uint32_t* iptr = Buffers->SceneIndexes + istart;
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
		uint32_t* iptr = Buffers->SceneIndexes + istart;
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

	SceneVertexPos += vcount;
	SceneIndexPos += icount;

	Stats.GouraudPolygons++;

	unguardSlow;
}

#endif

void UVulkanRenderDevice::DrawTile(FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags)
{
	guardSlow(UVulkanRenderDevice::DrawTile);

	// stijn: fix for invisible actor icons in ortho viewports
	if (GIsEditor && Frame->Viewport->Actor && (Frame->Viewport->IsOrtho() || Abs(Z) <= SMALL_NUMBER))
	{
		Z = 1.f;
	}

	if ((PolyFlags & (PF_Modulated)) == (PF_Modulated) && Info.Format == TEXF_P8)
		PolyFlags = PF_Modulated;

	CachedTexture* tex = Textures->GetTexture(&Info, !!(PolyFlags & PF_Masked));

	SetPipeline(RenderPasses->GetPipeline(PolyFlags, UsesBindless));

	ivec4 textureBinds = SetDescriptorSet(PolyFlags, tex, true);

	float UMult = tex ? GetUMult(Info) : 0.0f;
	float VMult = tex ? GetVMult(Info) : 0.0f;

	SceneVertex* v = &Buffers->SceneVertices[SceneVertexPos];

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

	if (Textures->Scene->Multisample > 1)
	{
		XL = std::floor(X + XL + 0.5f);
		YL = std::floor(Y + YL + 0.5f);
		X = std::floor(X + 0.5f);
		Y = std::floor(Y + 0.5f);
		XL = XL - X;
		YL = YL - Y;
	}

	v[0] = { 0, vec3(RFX2 * Z * (X - Frame->FX2),      RFY2 * Z * (Y - Frame->FY2),      Z), vec2(U * UMult,        V * VMult),        vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(r, g, b, a), textureBinds };
	v[1] = { 0, vec3(RFX2 * Z * (X + XL - Frame->FX2), RFY2 * Z * (Y - Frame->FY2),      Z), vec2((U + UL) * UMult, V * VMult),        vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(r, g, b, a), textureBinds };
	v[2] = { 0, vec3(RFX2 * Z * (X + XL - Frame->FX2), RFY2 * Z * (Y + YL - Frame->FY2), Z), vec2((U + UL) * UMult, (V + VL) * VMult), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(r, g, b, a), textureBinds };
	v[3] = { 0, vec3(RFX2 * Z * (X - Frame->FX2),      RFY2 * Z * (Y + YL - Frame->FY2), Z), vec2(U * UMult,        (V + VL) * VMult), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(r, g, b, a), textureBinds };

	size_t vstart = SceneVertexPos;
	size_t vcount = 4;
	size_t istart = SceneIndexPos;
	size_t icount = (vcount - 2) * 3;

	uint32_t* iptr = Buffers->SceneIndexes + istart;
	for (uint32_t i = vstart + 2; i < vstart + vcount; i++)
	{
		*(iptr++) = vstart;
		*(iptr++) = i - 1;
		*(iptr++) = i;
	}

	SceneVertexPos += vcount;
	SceneIndexPos += icount;

	Stats.Tiles++;

	unguardSlow;
}

static float square_f(float f) { return f * f; }
static float max3(vec3 v) { return Max(Max(v.x, v.y), v.z); }
static float max3(vec4 v) { return Max(Max(v.x, v.y), v.z); }

vec4 UVulkanRenderDevice::ApplyInverseGamma(vec4 color)
{
	float brightness = Clamp(Viewport->GetOuterUClient()->Brightness * 2.0, 0.05, 2.99);
	float gammaRed = Max(brightness + GammaOffset + GammaOffsetRed, 0.001f);
	float gammaGreen = Max(brightness + GammaOffset + GammaOffsetGreen, 0.001f);
	float gammaBlue = Max(brightness + GammaOffset + GammaOffsetBlue, 0.001f);
	return vec4(pow(color.r, gammaRed), pow(color.g, gammaGreen), pow(color.b, gammaBlue), color.a);
}

void UVulkanRenderDevice::Draw3DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2)
{
	guard(UVulkanRenderDevice::Draw3DLine);

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
		SetPipeline(RenderPasses->GetLinePipeline(OccludeLines, UsesBindless));

		ivec4 textureBinds = SetDescriptorSet(PF_Highlighted, nullptr);

		SceneVertex* v = &Buffers->SceneVertices[SceneVertexPos];
		uint32_t* iptr = Buffers->SceneIndexes + SceneIndexPos;

		vec4 color = ApplyInverseGamma(vec4(Color.X, Color.Y, Color.Z, 1.0f));

		v[0] = { 0, vec3(P1.X, P1.Y, P1.Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color, textureBinds };
		v[1] = { 0, vec3(P2.X, P2.Y, P2.Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color, textureBinds };

		iptr[0] = SceneVertexPos;
		iptr[1] = SceneVertexPos + 1;

		SceneVertexPos += 2;
		SceneIndexPos += 2;
	}

	unguard;
}

void UVulkanRenderDevice::Draw2DClippedLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2)
{
	guard(UVulkanRenderDevice::Draw2DClippedLine);
	URenderDevice::Draw2DClippedLine(Frame, Color, LineFlags, P1, P2);
	unguard;
}

void UVulkanRenderDevice::Draw2DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2)
{
	guard(UVulkanRenderDevice::Draw2DLine);

	SetPipeline(RenderPasses->GetLinePipeline(OccludeLines, UsesBindless));

	ivec4 textureBinds = SetDescriptorSet(PF_Highlighted, nullptr);

	SceneVertex* v = &Buffers->SceneVertices[SceneVertexPos];
	uint32_t* iptr = Buffers->SceneIndexes + SceneIndexPos;

	vec4 color = ApplyInverseGamma(vec4(Color.X, Color.Y, Color.Z, 1.0f));

	v[0] = { 0, vec3(RFX2 * P1.Z * (P1.X - Frame->FX2), RFY2 * P1.Z * (P1.Y - Frame->FY2), P1.Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color, textureBinds };
	v[1] = { 0, vec3(RFX2 * P2.Z * (P2.X - Frame->FX2), RFY2 * P2.Z * (P2.Y - Frame->FY2), P2.Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color, textureBinds };

	iptr[0] = SceneVertexPos;
	iptr[1] = SceneVertexPos + 1;

	SceneVertexPos += 2;
	SceneIndexPos += 2;

	unguard;
}

void UVulkanRenderDevice::Draw2DPoint(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z)
{
	guard(UVulkanRenderDevice::Draw2DPoint);

	// Hack to fix UED selection problem with selection brush
	if (GIsEditor) Z = 1.0f;

	SetPipeline(RenderPasses->GetPointPipeline(OccludeLines, UsesBindless));

	ivec4 textureBinds = SetDescriptorSet(PF_Highlighted, nullptr);

	SceneVertex* v = &Buffers->SceneVertices[SceneVertexPos];

	vec4 color = ApplyInverseGamma(vec4(Color.X, Color.Y, Color.Z, 1.0f));

	v[0] = { 0, vec3(RFX2 * Z * (X1 - Frame->FX2 - 0.5f), RFY2 * Z * (Y1 - Frame->FY2 - 0.5f), Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color, textureBinds };
	v[1] = { 0, vec3(RFX2 * Z * (X2 - Frame->FX2 + 0.5f), RFY2 * Z * (Y1 - Frame->FY2 - 0.5f), Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color, textureBinds };
	v[2] = { 0, vec3(RFX2 * Z * (X2 - Frame->FX2 + 0.5f), RFY2 * Z * (Y2 - Frame->FY2 + 0.5f), Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color, textureBinds };
	v[3] = { 0, vec3(RFX2 * Z * (X1 - Frame->FX2 - 0.5f), RFY2 * Z * (Y2 - Frame->FY2 + 0.5f), Z), vec2(0.0f), vec2(0.0f), vec2(0.0f), vec2(0.0f), color, textureBinds };

	size_t vstart = SceneVertexPos;
	size_t vcount = 4;
	size_t istart = SceneIndexPos;
	size_t icount = (vcount - 2) * 3;

	uint32_t* iptr = Buffers->SceneIndexes + istart;
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

void UVulkanRenderDevice::ClearZ(FSceneNode* Frame)
{
	guard(UVulkanRenderDevice::ClearZ);

	DrawBatch(Commands->GetDrawCommands());

	VkClearAttachment attachment = {};
	VkClearRect rect = {};
	attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	attachment.clearValue.depthStencil.depth = 1.0f;
	rect.layerCount = 1;
	rect.rect.extent.width = Textures->Scene->Width;
	rect.rect.extent.height = Textures->Scene->Height;
	Commands->GetDrawCommands()->clearAttachments(1, &attachment, 1, &rect);
	unguard;
}

void UVulkanRenderDevice::PushHit(const BYTE* Data, INT Count)
{
	guard(UVulkanRenderDevice::PushHit);

	if (Count <= 0) return;
	HitQueryStack.insert(HitQueryStack.end(), Data, Data + Count);

	DrawBatch(Commands->GetDrawCommands());

	INT index = HitQueries.size();

	HitQuery query;
	query.Start = HitBuffer.size();
	query.Count = HitQueryStack.size();
	HitQueries.push_back(query);

	HitBuffer.insert(HitBuffer.end(), HitQueryStack.begin(), HitQueryStack.end());

	pushconstants.hitIndex = index + 1;

	unguard;
}

void UVulkanRenderDevice::PopHit(INT Count, UBOOL bForce)
{
	guard(UVulkanRenderDevice::PopHit);

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

void UVulkanRenderDevice::GetStats(TCHAR* Result)
{
	guard(UVulkanRenderDevice::GetStats);
	Result[0] = 0;
	unguard;
}

void UVulkanRenderDevice::ReadPixels(FColor* Pixels)
{
	guard(UVulkanRenderDevice::GetStats);

	int w = Viewport->SizeX;
	int h = Viewport->SizeY;
	void* data = Pixels;

	auto dstimage = ImageBuilder()
		.Format(VK_FORMAT_B8G8R8A8_UNORM)
		.Usage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.Size(w, h)
		.DebugName("ReadPixelsDstImage")
		.Create(Device.get());

	// Convert from rgba16f to bgra8 using the GPU:
	auto srcimage = Textures->Scene->PPImage.get();
	auto cmdbuffer = Commands->GetDrawCommands();

	DrawBatch(cmdbuffer);

	PipelineBarrier()
		.AddImage(srcimage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT)
		.AddImage(dstimage.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT)
		.Execute(cmdbuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	VkImageBlit blit = {};
	blit.srcOffsets[0] = { 0, 0, 0 };
	blit.srcOffsets[1] = { srcimage->width, srcimage->height, 1 };
	blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit.srcSubresource.mipLevel = 0;
	blit.srcSubresource.baseArrayLayer = 0;
	blit.srcSubresource.layerCount = 1;
	blit.dstOffsets[0] = { 0, 0, 0 };
	blit.dstOffsets[1] = { dstimage->width, dstimage->height, 1 };
	blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit.dstSubresource.mipLevel = 0;
	blit.dstSubresource.baseArrayLayer = 0;
	blit.dstSubresource.layerCount = 1;
	cmdbuffer->blitImage(
		srcimage->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		dstimage->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &blit, VK_FILTER_NEAREST);

	PipelineBarrier()
		.AddImage(srcimage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT)
		.AddImage(dstimage.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT)
		.Execute(cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	// Staging buffer for download
	auto staging = BufferBuilder()
		.Size(w * h * 4)
		.Usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU)
		.DebugName("ReadPixelsStaging")
		.Create(Device.get());

	// Copy from image to buffer
	VkBufferImageCopy region = {};
	region.imageExtent.width = w;
	region.imageExtent.height = h;
	region.imageExtent.depth = 1;
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	cmdbuffer->copyImageToBuffer(dstimage->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging->buffer, 1, &region);

	// Submit command buffers and wait for device to finish the work
	SubmitAndWait(false, 0, 0, false);

	uint8_t* pixels = (uint8_t*)staging->Map(0, w * h * 4);
	memcpy(data, pixels, w * h * 4);
	staging->Unmap();

	unguard;
}

void UVulkanRenderDevice::EndFlash()
{
	guard(UVulkanRenderDevice::EndFlash);
	if (FlashScale != FPlane(0.5f, 0.5f, 0.5f, 0.0f) || FlashFog != FPlane(0.0f, 0.0f, 0.0f, 0.0f))
	{
		vec4 color(FlashFog.X, FlashFog.Y, FlashFog.Z, 1.0f - Min(FlashScale.X * 2.0f, 1.0f));
		vec2 zero2(0.0f);
		ivec4 zero4(0);

		DrawBatch(Commands->GetDrawCommands());
		pushconstants.objectToProjection = mat4::identity();
		pushconstants.nearClip = vec4(0.0f, 0.0f, 0.0f, 1.0f);

		SetPipeline(RenderPasses->GetEndFlashPipeline());
		SetDescriptorSet(DescriptorSets->GetTextureSet(0, nullptr), false);

		SceneVertex* v = &Buffers->SceneVertices[SceneVertexPos];

		v[0] = { 0, vec3(-1.0f, -1.0f, 0.0f), zero2, zero2, zero2, zero2, color, zero4 };
		v[1] = { 0, vec3( 1.0f, -1.0f, 0.0f), zero2, zero2, zero2, zero2, color, zero4 };
		v[2] = { 0, vec3( 1.0f,  1.0f, 0.0f), zero2, zero2, zero2, zero2, color, zero4 };
		v[3] = { 0, vec3(-1.0f,  1.0f, 0.0f), zero2, zero2, zero2, zero2, color, zero4 };

		size_t vstart = SceneVertexPos;
		size_t vcount = 4;
		size_t istart = SceneIndexPos;
		size_t icount = (vcount - 2) * 3;

		uint32_t* iptr = Buffers->SceneIndexes + istart;
		for (uint32_t i = vstart + 2; i < vstart + vcount; i++)
		{
			*(iptr++) = vstart;
			*(iptr++) = i - 1;
			*(iptr++) = i;
		}

		SceneVertexPos += vcount;
		SceneIndexPos += icount;

		if (CurrentFrame)
			SetSceneNode(CurrentFrame);
	}
	unguard;
}

void UVulkanRenderDevice::SetSceneNode(FSceneNode* Frame)
{
	guardSlow(UVulkanRenderDevice::SetSceneNode);

	auto commands = Commands->GetDrawCommands();
	DrawBatch(commands);

	CurrentFrame = Frame;
	Aspect = Frame->FY / Frame->FX;
	RProjZ = (float)appTan(radians(Viewport->Actor->FovAngle) * 0.5);
	RFX2 = 2.0f * RProjZ / Frame->FX;
	RFY2 = 2.0f * RProjZ * Aspect / Frame->FY;

	VkViewport viewportdesc = {};
	viewportdesc.x = Frame->XB;
	viewportdesc.y = Frame->YB;
	viewportdesc.width = Frame->X;
	viewportdesc.height = Frame->Y;
	viewportdesc.minDepth = 0.0f;
	viewportdesc.maxDepth = 1.0f;
	commands->setViewport(0, 1, &viewportdesc);

	pushconstants.objectToProjection = mat4::frustum(-RProjZ, RProjZ, -Aspect * RProjZ, Aspect * RProjZ, 1.0f, 32768.0f, handedness::left, clipzrange::zero_positive_w);
	pushconstants.nearClip = vec4(Frame->NearClip.X, Frame->NearClip.Y, Frame->NearClip.Z, Frame->NearClip.W);

	unguardSlow;
}

void UVulkanRenderDevice::PrecacheTexture(FTextureInfo& Info, DWORD PolyFlags)
{
	guard(UVulkanRenderDevice::PrecacheTexture);
	Textures->GetTexture(&Info, !!(PolyFlags & PF_Masked));
	unguard;
}

void UVulkanRenderDevice::ClearTextureCache()
{
	DescriptorSets->ClearCache();
	Textures->ClearCache();
}

void UVulkanRenderDevice::BlitSceneToPostprocess()
{
	auto buffers = Textures->Scene.get();
	auto cmdbuffer = Commands->GetDrawCommands();

	PipelineBarrier barrer0;
	barrer0.AddImage(
		buffers->ColorBuffer.get(),
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_TRANSFER_READ_BIT);
	if (HitData)
	{
		barrer0.AddImage(
			buffers->HitBuffer.get(),
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_TRANSFER_READ_BIT);
		barrer0.AddImage(
			buffers->PPHitBuffer.get(),
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_ACCESS_TRANSFER_READ_BIT,
			VK_ACCESS_TRANSFER_WRITE_BIT);
	}
	barrer0.AddImage(
		buffers->PPImage.get(),
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_TRANSFER_WRITE_BIT);
	barrer0.Execute(
		Commands->GetDrawCommands(),
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT);

	if (buffers->SceneSamples != VK_SAMPLE_COUNT_1_BIT)
	{
		VkImageResolve resolve = {};
		resolve.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		resolve.srcSubresource.layerCount = 1;
		resolve.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		resolve.dstSubresource.layerCount = 1;
		resolve.extent = { (uint32_t)buffers->ColorBuffer->width, (uint32_t)buffers->ColorBuffer->height, 1 };
		cmdbuffer->resolveImage(
			buffers->ColorBuffer->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			buffers->PPImage->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &resolve);
		if (HitData)
		{
			cmdbuffer->resolveImage(
				buffers->HitBuffer->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				buffers->PPHitBuffer->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &resolve);
		}
	}
	else
	{
		auto colorBuffer = buffers->ColorBuffer.get();
		VkImageBlit blit = {};
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { colorBuffer->width, colorBuffer->height, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { colorBuffer->width, colorBuffer->height, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.layerCount = 1;
		cmdbuffer->blitImage(
			colorBuffer->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			buffers->PPImage->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit, VK_FILTER_NEAREST);
		if (HitData)
		{
			VkImageCopy copy = {};
			copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copy.srcSubresource.layerCount = 1;
			copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copy.dstSubresource.layerCount = 1;
			copy.extent = { (uint32_t)colorBuffer->width, (uint32_t)colorBuffer->height, (uint32_t)1 };
			cmdbuffer->copyImage(
				buffers->HitBuffer->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				buffers->PPHitBuffer->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &copy);
		}
	}

	PipelineBarrier barrier1;
	barrier1.AddImage(
		buffers->PPImage.get(),
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT);
	if (HitData)
	{
		barrier1.AddImage(
			buffers->PPHitBuffer.get(),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_TRANSFER_READ_BIT);
	}
	barrier1.Execute(
		Commands->GetDrawCommands(),
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		HitData ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	if (HitData)
	{
		VkBufferImageCopy copy = {};
		copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy.imageSubresource.layerCount = 1;
		copy.imageOffset = { (int32_t)Viewport->HitX, (int32_t)Viewport->HitY, (int32_t)0 };
		copy.imageExtent = { (uint32_t)Viewport->HitXL, (uint32_t)Viewport->HitYL, (uint32_t)1 };
		cmdbuffer->copyImageToBuffer(buffers->PPHitBuffer->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffers->StagingHitBuffer->buffer, 1, &copy);

		PipelineBarrier()
			.AddBuffer(buffers->StagingHitBuffer.get(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT)
			.Execute(cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);
	}
}

void UVulkanRenderDevice::RunBloomPass()
{
	float blurAmount = 0.6f + BloomAmount * (1.9f / 255.0f);
	BloomPushConstants pushconstants;
	ComputeBlurSamples(7, blurAmount, pushconstants.SampleWeights);

	auto cmdbuffer = Commands->GetDrawCommands();

	// Extract overbright pixels that we want to bloom:
	BloomStep(cmdbuffer,
		RenderPasses->Bloom.Extract.get(),
		DescriptorSets->GetBloomPPImageSet(),
		Framebuffers->BloomBlurLevels[0].VTextureFB.get(),
		Textures->Scene->BloomBlurLevels[0].Width,
		Textures->Scene->BloomBlurLevels[0].Height,
		pushconstants);

	// Blur and downscale:
	for (int i = 0; i < NumBloomLevels - 1; i++)
	{
		// Gaussian blur
		BloomStep(cmdbuffer,
			RenderPasses->Bloom.BlurVertical.get(),
			DescriptorSets->GetBloomVTextureSet(i),
			Framebuffers->BloomBlurLevels[i].HTextureFB.get(),
			Textures->Scene->BloomBlurLevels[i].Width,
			Textures->Scene->BloomBlurLevels[i].Height,
			pushconstants);
		BloomStep(cmdbuffer,
			RenderPasses->Bloom.BlurHorizontal.get(),
			DescriptorSets->GetBloomHTextureSet(i),
			Framebuffers->BloomBlurLevels[i].VTextureFB.get(),
			Textures->Scene->BloomBlurLevels[i].Width,
			Textures->Scene->BloomBlurLevels[i].Height,
			pushconstants);

		// Linear downscale
		BloomStep(cmdbuffer,
			RenderPasses->Bloom.Scale.get(),
			DescriptorSets->GetBloomVTextureSet(i),
			Framebuffers->BloomBlurLevels[i + 1].VTextureFB.get(),
			Textures->Scene->BloomBlurLevels[i + 1].Width,
			Textures->Scene->BloomBlurLevels[i + 1].Height,
			pushconstants);
	}

	// Blur and upscale:
	for (int i = NumBloomLevels - 1; i >= 0; i--)
	{
		// Gaussian blur
		BloomStep(cmdbuffer,
			RenderPasses->Bloom.BlurVertical.get(),
			DescriptorSets->GetBloomVTextureSet(i),
			Framebuffers->BloomBlurLevels[i].HTextureFB.get(),
			Textures->Scene->BloomBlurLevels[i].Width,
			Textures->Scene->BloomBlurLevels[i].Height,
			pushconstants);
		BloomStep(cmdbuffer,
			RenderPasses->Bloom.BlurHorizontal.get(),
			DescriptorSets->GetBloomHTextureSet(i),
			Framebuffers->BloomBlurLevels[i].VTextureFB.get(),
			Textures->Scene->BloomBlurLevels[i].Width,
			Textures->Scene->BloomBlurLevels[i].Height,
			pushconstants);

		// Linear upscale
		if (i > 0)
		{
			BloomStep(cmdbuffer,
				RenderPasses->Bloom.Scale.get(),
				DescriptorSets->GetBloomVTextureSet(i),
				Framebuffers->BloomBlurLevels[i - 1].VTextureFB.get(),
				Textures->Scene->BloomBlurLevels[i - 1].Width,
				Textures->Scene->BloomBlurLevels[i - 1].Height,
				pushconstants);
		}
	}

	// Add bloom back to frame post process texture:
	BloomStep(cmdbuffer,
		RenderPasses->Bloom.Combine.get(),
		DescriptorSets->GetBloomVTextureSet(0),
		Framebuffers->BloomPPImageFB.get(),
		Textures->Scene->Width,
		Textures->Scene->Height,
		pushconstants);
}

void UVulkanRenderDevice::BloomStep(VulkanCommandBuffer* cmdbuffer, VulkanPipeline* pipeline, VulkanDescriptorSet* input, VulkanFramebuffer* output, int width, int height, const BloomPushConstants& pushconstants)
{
	RenderPassBegin()
		.RenderPass(pipeline != RenderPasses->Bloom.Combine.get() ? RenderPasses->Bloom.RenderPass.get() : RenderPasses->Bloom.RenderPassCombine.get())
		.Framebuffer(output)
		.RenderArea(0, 0, width, height)
		.AddClearColor(0.0f, 0.0f, 0.0f, 1.0f)
		.Execute(cmdbuffer);

	VkViewport viewport = {};
	viewport.width = (float)width;
	viewport.height = (float)height;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.extent.width = width;
	scissor.extent.height = height;

	cmdbuffer->setViewport(0, 1, &viewport);
	cmdbuffer->setScissor(0, 1, &scissor);
	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, RenderPasses->Bloom.PipelineLayout.get(), 0, input);
	cmdbuffer->pushConstants(RenderPasses->Bloom.PipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(BloomPushConstants), &pushconstants);
	cmdbuffer->draw(6, 1, 0, 0);

	cmdbuffer->endRenderPass();
}

float UVulkanRenderDevice::ComputeBlurGaussian(float n, float theta) // theta = Blur Amount
{
	return (float)((1.0f / std::sqrtf(2 * 3.14159265359f * theta)) * std::expf(-(n * n) / (2.0f * theta * theta)));
}

void UVulkanRenderDevice::ComputeBlurSamples(int sampleCount, float blurAmount, float* sampleWeights)
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

void UVulkanRenderDevice::DrawPresentTexture(int x, int y, int width, int height)
{
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

	bool ActiveHdr = (Commands->SwapChain->Format().colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT) ? 1 : 0;

	// Select present shader based on what the user is actually using
	int presentShader = 0;
	if (ActiveHdr) presentShader |= 1;
	if (GammaMode == 1) presentShader |= 2;
	if (pushconstants.Brightness != 0.0f || pushconstants.Contrast != 1.0f || pushconstants.Saturation != 1.0f) presentShader |= (Clamp(GrayFormula, 0, 2) + 1) << 2;

	VkViewport viewport = {};
	viewport.x = x;
	viewport.y = y;
	viewport.width = width;
	viewport.height = height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.extent.width = width;
	scissor.extent.height = height;

	auto cmdbuffer = Commands->GetDrawCommands();

	RenderPasses->BeginPresent(cmdbuffer);
	cmdbuffer->setViewport(0, 1, &viewport);
	cmdbuffer->setScissor(0, 1, &scissor);
	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, RenderPasses->Present.Pipeline[presentShader].get());
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, RenderPasses->Present.PipelineLayout.get(), 0, DescriptorSets->GetPresentSet());
	cmdbuffer->pushConstants(RenderPasses->Present.PipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PresentPushConstants), &pushconstants);
	cmdbuffer->draw(6, 1, 0, 0);
	RenderPasses->EndPresent(cmdbuffer);
}
