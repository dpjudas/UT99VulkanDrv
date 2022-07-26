
#include "Precomp.h"
#include "UVulkanRenderDevice.h"
#include "VulkanTexture.h"
#include "VulkanBuilders.h"
#include "VulkanSwapChain.h"
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
	VkDeviceIndex = 0;
	VkDebug = 0;
	Multisample = 0;
	UsePrecache = 0;

	UseLightmapAtlas = 0;
	SupportsUpdateTextureRect = 1;
	MaxTextureSize = 4096;

	new(GetClass(), TEXT("UseLightmapAtlas"), RF_Public) UBoolProperty(CPP_PROPERTY(UseLightmapAtlas), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("UseVSync"), RF_Public) UBoolProperty(CPP_PROPERTY(UseVSync), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("UsePrecache"), RF_Public) UBoolProperty(CPP_PROPERTY(UsePrecache), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("Multisample"), RF_Public) UIntProperty(CPP_PROPERTY(Multisample), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("VkDeviceIndex"), RF_Public) UIntProperty(CPP_PROPERTY(VkDeviceIndex), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("VkDebug"), RF_Public) UBoolProperty(CPP_PROPERTY(VkDebug), TEXT("Display"), CPF_Config);

	unguard;
}

UBOOL UVulkanRenderDevice::Init(UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen)
{
	guard(UVulkanRenderDevice::Init);

	auto printLog = [](const char* typestr, const std::string& msg)
	{
		debugf(TEXT("[%s] %s"), to_utf16(typestr).c_str(), to_utf16(msg).c_str());
	};

	Viewport = InViewport;

	try
	{
		Device = new VulkanDevice((HWND)Viewport->GetWindow(), VkDeviceIndex, VkDebug, printLog);
		Commands.reset(new CommandBufferManager(this));
		Samplers.reset(new SamplerManager(this));
		Textures.reset(new TextureManager(this));
		Buffers.reset(new BufferManager(this));
		Shaders.reset(new ShaderManager(this));
		DescriptorSets.reset(new DescriptorSetManager(this));
		RenderPasses.reset(new RenderPassManager(this));
		Framebuffers.reset(new FramebufferManager(this));

		if (VkDebug)
		{
			const auto& props = Device->physicalDevice.properties;

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

			debugf(TEXT("Vulkan extensions:"));
			for (const VkExtensionProperties& p : Device->physicalDevice.extensions)
			{
				debugf(TEXT(" %s"), to_utf16(p.extensionName).c_str());
			}

			const auto& limits = props.limits;
			debugf(TEXT("Max. texture size: %d"), limits.maxImageDimension2D);
			debugf(TEXT("Max. uniform buffer range: %d"), limits.maxUniformBufferRange);
			debugf(TEXT("Min. uniform buffer offset alignment: %llu"), limits.minUniformBufferOffsetAlignment);
		}
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

	if (!Viewport->ResizeViewport(Fullscreen ? (BLIT_Fullscreen | BLIT_OpenGL) : (BLIT_HardwarePaint | BLIT_OpenGL), NewX, NewY, NewColorBytes))
		return 0;

	SaveConfig();

	Flush(1);

	return 1;
	unguard;
}

void UVulkanRenderDevice::Exit()
{
	guard(UVulkanRenderDevice::Exit);

	if (Device) vkDeviceWaitIdle(Device->device);

	Framebuffers.reset();
	RenderPasses.reset();
	DescriptorSets.reset();
	Shaders.reset();
	Buffers.reset();
	Textures.reset();
	Samplers.reset();
	Commands.reset();

	delete Device; Device = nullptr;

	unguard;
}

void UVulkanRenderDevice::Flush(UBOOL AllowPrecache)
{
	guard(UVulkanRenderDevice::Flush);

	if (IsLocked)
	{
		RenderPasses->EndScene(Commands->GetDrawCommands());
		Commands->SubmitCommands(false, 0, 0);
		ClearTextureCache();

		auto cmdbuffer = Commands->GetDrawCommands();
		RenderPasses->BeginScene(cmdbuffer);

		VkBuffer vertexBuffers[] = { Buffers->SceneVertexBuffer->buffer };
		VkDeviceSize offsets[] = { 0 };
		cmdbuffer->bindVertexBuffers(0, 1, vertexBuffers, offsets);
	}
	else
	{
		ClearTextureCache();
	}

	if (AllowPrecache && UsePrecache && !GIsEditor)
		PrecacheOnFlip = 1;

	unguard;
}

UBOOL UVulkanRenderDevice::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	guard(UVulkanRenderDevice::Exec);

	if (URenderDevice::Exec(Cmd, Ar))
	{
		return 1;
	}
	else if (ParseCommand(&Cmd, TEXT("DGL")))
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
		};

		std::vector<Resolution> resolutions;

#ifdef WIN32
		HDC screenDC = GetDC(0);
		resolutions.push_back({ GetDeviceCaps(screenDC, HORZRES), GetDeviceCaps(screenDC, VERTRES) });
		ReleaseDC(0, screenDC);
#else
		resolutions.push_back({ 1920, 1080 });
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
		for (size_t i = 0; i < Device->supportedDevices.size(); i++)
		{
			Ar.Log(FString::Printf(TEXT("#%d - %s\r\n"), (int)i, to_utf16(Device->supportedDevices[i].device->properties.deviceName)));
		}
		return 1;
	}
	else if (ParseCommand(&Cmd, TEXT("VkMemStats")))
	{
		VmaStats stats = {};
		vmaCalculateStats(Device->allocator, &stats);
		Ar.Log(FString::Printf(TEXT("Allocated objects: %d, used bytes: %d MB\r\n"), (int)stats.total.allocationCount, (int)stats.total.usedBytes / (1024 * 1024)));
		Ar.Log(FString::Printf(TEXT("Unused range count: %d, unused bytes: %d MB\r\n"), (int)stats.total.unusedRangeCount, (int)stats.total.unusedBytes / (1024 * 1024)));
		return 1;
	}
	else
	{
		return 0;
	}

	unguard;
}

void UVulkanRenderDevice::Lock(FPlane InFlashScale, FPlane InFlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData, INT* HitSize)
{
	guard(UVulkanRenderDevice::Lock);

	FlashScale = InFlashScale;
	FlashFog = InFlashFog;

	try
	{
		if (!Textures->Scene || Textures->Scene->width != Viewport->SizeX || Textures->Scene->height != Viewport->SizeY)
		{
			Framebuffers->DestroySceneFramebuffer();
			Textures->Scene.reset();
			Textures->Scene.reset(new SceneTextures(this, Viewport->SizeX, Viewport->SizeY, Multisample));
			RenderPasses->CreateRenderPass();
			RenderPasses->CreatePipelines();
			Framebuffers->CreateSceneFramebuffer();

			auto descriptors = DescriptorSets->GetPresentDescriptorSet();
			WriteDescriptors write;
			write.addCombinedImageSampler(descriptors, 0, Textures->Scene->ppImageView.get(), Samplers->ppLinearClamp.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			write.addCombinedImageSampler(descriptors, 1, Textures->DitherImageView.get(), Samplers->ppNearestRepeat.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			write.updateSets(Device);
		}

		PipelineBarrier barrier;
		barrier.addImage(Textures->Scene->colorBuffer.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
		barrier.execute(Commands->GetDrawCommands(), VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		auto cmdbuffer = Commands->GetDrawCommands();
		RenderPasses->BeginScene(cmdbuffer);

		VkBuffer vertexBuffers[] = { Buffers->SceneVertexBuffer->buffer };
		VkDeviceSize offsets[] = { 0 };
		cmdbuffer->bindVertexBuffers(0, 1, vertexBuffers, offsets);

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

void UVulkanRenderDevice::Unlock(UBOOL Blit)
{
	guard(UVulkanRenderDevice::Unlock);

	RenderPasses->EndScene(Commands->GetDrawCommands());

	BlitSceneToPostprocess();

	Commands->SubmitCommands(Blit ? true : false, Viewport->SizeX, Viewport->SizeY);
	SceneVertexPos = 0;

	IsLocked = false;

	unguard;
}

UBOOL UVulkanRenderDevice::SupportsTextureFormat(ETextureFormat Format)
{
	guard(UVulkanRenderDevice::SupportsTextureFormat);

	static const ETextureFormat knownFormats[] =
	{
		TEXF_P8,
		TEXF_RGBA7,
		TEXF_R5G6B5,
		TEXF_DXT1,
		TEXF_RGB8,
		TEXF_BGRA8,
		TEXF_BC2,
		TEXF_BC3,
		TEXF_BC1_PA,
		TEXF_BC7,
		TEXF_BC6H_S,
		TEXF_BC6H
	};

	for (ETextureFormat known : knownFormats)
	{
		if (known == Format)
			return TRUE;
	}
	return FALSE;

	unguard;
}

void UVulkanRenderDevice::UpdateTextureRect(FTextureInfo& Info, INT U, INT V, INT UL, INT VL)
{
	guard(UVulkanRenderDevice::UpdateTextureRect);

	Textures->UpdateTextureRect(&Info, U, V, UL, VL);

	unguard;
}

static float GetUMult(const FTextureInfo& Info) { return 1.0f / (Info.UScale * Info.USize); }
static float GetVMult(const FTextureInfo& Info) { return 1.0f / (Info.VScale * Info.VSize); }

void UVulkanRenderDevice::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet)
{
	guard(UVulkanRenderDevice::DrawComplexSurface);

	VulkanCommandBuffer* cmdbuffer = Commands->GetDrawCommands();

	VulkanTexture* tex = Textures->GetTexture(Surface.Texture, !!(Surface.PolyFlags & PF_Masked));
	VulkanTexture* lightmap = Textures->GetTexture(Surface.LightMap, false);
	VulkanTexture* macrotex = Textures->GetTexture(Surface.MacroTexture, false);
	VulkanTexture* detailtex = Textures->GetTexture(Surface.DetailTexture, false);
	VulkanTexture* fogmap = Textures->GetTexture(Surface.FogMap, false);

	if ((Surface.DetailTexture && Surface.FogMap) || (!DetailTextures)) detailtex = nullptr;

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

	if (fogmap) // if Surface.FogMap exists, use instead of detail texture
	{
		detailtex = fogmap;
		DetailUPan = UDot + Surface.FogMap->Pan.X - 0.5f * Surface.FogMap->UScale;
		DetailVPan = VDot + Surface.FogMap->Pan.Y - 0.5f * Surface.FogMap->VScale;
		DetailUMult = GetUMult(*Surface.FogMap);
		DetailVMult = GetVMult(*Surface.FogMap);
	}

	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, RenderPasses->getPipeline(Surface.PolyFlags));
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, RenderPasses->ScenePipelineLayout.get(), 0, DescriptorSets->GetTextureDescriptorSet(Surface.PolyFlags, tex, lightmap, macrotex, detailtex));

	for (FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next)
	{
		SceneVertex* vertexdata = &Buffers->SceneVertices[SceneVertexPos];

		for (INT i = 0; i < Poly->NumPts; i++)
		{
			SceneVertex& vtx = vertexdata[i];

			FLOAT u = Facet.MapCoords.XAxis | Poly->Pts[i]->Point;
			FLOAT v = Facet.MapCoords.YAxis | Poly->Pts[i]->Point;

			vtx.flags = flags;
			vtx.x = Poly->Pts[i]->Point.X;
			vtx.y = Poly->Pts[i]->Point.Y;
			vtx.z = Poly->Pts[i]->Point.Z;
			vtx.u = (u - UPan) * UMult;
			vtx.v = (v - VPan) * VMult;
			vtx.u2 = (u - LMUPan) * LMUMult;
			vtx.v2 = (v - LMVPan) * LMVMult;
			vtx.u3 = (u - MacroUPan) * MacroUMult;
			vtx.v3 = (v - MacroVPan) * MacroVMult;
			vtx.u4 = (u - DetailUPan) * DetailUMult;
			vtx.v4 = (v - DetailVPan) * DetailVMult;
			vtx.r = 1.0f;
			vtx.g = 1.0f;
			vtx.b = 1.0f;
			vtx.a = 1.0f;
		}

		size_t start = SceneVertexPos;
		size_t count = Poly->NumPts;
		SceneVertexPos += count;

		cmdbuffer->draw(count, 1, start, 0);
	}

	unguard;
}

void UVulkanRenderDevice::DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, int NumPts, DWORD PolyFlags, FSpanBuffer* Span)
{
	guard(UVulkanRenderDevice::DrawGouraudPolygon);

	auto cmdbuffer = Commands->GetDrawCommands();

	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, RenderPasses->getPipeline(PolyFlags));

	VulkanTexture* tex = Textures->GetTexture(&Info, !!(PolyFlags & PF_Masked));
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, RenderPasses->ScenePipelineLayout.get(), 0, DescriptorSets->GetTextureDescriptorSet(PolyFlags, tex));

	float UMult = tex ? GetUMult(Info) : 0.0f;
	float VMult = tex ? GetVMult(Info) : 0.0f;

	SceneVertex* vertexdata = &Buffers->SceneVertices[SceneVertexPos];

	int flags = (PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated)) == PF_RenderFog ? 16 : 0;

	for (INT i = 0; i < NumPts; i++)
	{
		FTransTexture* P = Pts[i];
		SceneVertex& v = vertexdata[i];
		v.flags = flags;
		v.x = P->Point.X;
		v.y = P->Point.Y;
		v.z = P->Point.Z;
		v.u = P->U * UMult;
		v.v = P->V * VMult;
		v.u2 = P->Fog.X;
		v.v2 = P->Fog.Y;
		v.u3 = P->Fog.Z;
		v.v3 = P->Fog.W;
		v.u4 = 0.0f;
		v.v4 = 0.0f;
		if (PolyFlags & PF_Modulated)
		{
			v.r = 1.0f;
			v.g = 1.0f;
			v.b = 1.0f;
			v.a = 1.0f;
		}
		else
		{
			v.r = P->Light.X;
			v.g = P->Light.Y;
			v.b = P->Light.Z;
			v.a = 1.0f;
		}
	}

	size_t start = SceneVertexPos;
	size_t count = NumPts;
	SceneVertexPos += count;

	cmdbuffer->draw(count, 1, start, 0);

	unguard;
}

void UVulkanRenderDevice::DrawTile(FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags)
{
	guard(UVulkanRenderDevice::DrawTile);

	if ((PolyFlags & (PF_Modulated)) == (PF_Modulated) && Info.Format == TEXF_P8)
		PolyFlags = PF_Modulated;

	auto cmdbuffer = Commands->GetDrawCommands();

	VulkanTexture* tex = Textures->GetTexture(&Info, !!(PolyFlags & PF_Masked));

	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, RenderPasses->getPipeline(PolyFlags));
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, RenderPasses->ScenePipelineLayout.get(), 0, DescriptorSets->GetTextureDescriptorSet(PolyFlags, tex, nullptr, nullptr, nullptr, true));

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

	v[0] = { 0, RFX2 * Z * (X - Frame->FX2),      RFY2 * Z * (Y - Frame->FY2),      Z, U * UMult,        V * VMult,        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, r, g, b, a };
	v[1] = { 0, RFX2 * Z * (X + XL - Frame->FX2), RFY2 * Z * (Y - Frame->FY2),      Z, (U + UL) * UMult, V * VMult,        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, r, g, b, a };
	v[2] = { 0, RFX2 * Z * (X + XL - Frame->FX2), RFY2 * Z * (Y + YL - Frame->FY2), Z, (U + UL) * UMult, (V + VL) * VMult, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, r, g, b, a };
	v[3] = { 0, RFX2 * Z * (X - Frame->FX2),      RFY2 * Z * (Y + YL - Frame->FY2), Z, U * UMult,        (V + VL) * VMult, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, r, g, b, a };

	size_t start = SceneVertexPos;
	size_t count = 4;
	SceneVertexPos += count;
	cmdbuffer->draw(count, 1, start, 0);

	unguard;
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
		/*SetNoTexture(0);
		SetBlend(PF_Highlighted);
		glColor3fv(&Color.X);
		glBegin(GL_LINES);
		glVertex3fv(&P1.X);
		glVertex3fv(&P2.X);
		glEnd();*/
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
	//SetBlend(PF_Highlighted | PF_Occlude);
	//glColor3fv( &Color.X );
	//glBegin(GL_LINES);
	//glVertex3f( RFX2*P1.Z*(P1.X-Frame->FX2), RFY2*P1.Z*(P1.Y-Frame->FY2), P1.Z );
	//glVertex3f( RFX2*P2.Z*(P2.X-Frame->FX2), RFY2*P2.Z*(P2.Y-Frame->FY2), P2.Z );
	//glEnd();
	unguard;
}

void UVulkanRenderDevice::Draw2DPoint(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z)
{
	guard(UVulkanRenderDevice::Draw2DPoint);
	//SetBlend(PF_Highlighted | PF_Occlude);
	//SetNoTexture(0);
	//glColor3fv(&Color.X);
	//glBegin(GL_TRIANGLE_FAN);
	//glVertex3f( RFX2*Z*(X1-Frame->FX2), RFY2*Z*(Y1-Frame->FY2), Z );
	//glVertex3f( RFX2*Z*(X2-Frame->FX2), RFY2*Z*(Y1-Frame->FY2), Z );
	//glVertex3f( RFX2*Z*(X2-Frame->FX2), RFY2*Z*(Y2-Frame->FY2), Z );
	//glVertex3f( RFX2*Z*(X1-Frame->FX2), RFY2*Z*(Y2-Frame->FY2), Z );
	//glEnd();
	unguard;
}

void UVulkanRenderDevice::ClearZ(FSceneNode* Frame)
{
	guard(UVulkanRenderDevice::ClearZ);
	VkClearAttachment attachment = {};
	VkClearRect rect = {};
	attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	attachment.clearValue.depthStencil.depth = 1.0f;
	rect.layerCount = 1;
	rect.rect.extent.width = Textures->Scene->width;
	rect.rect.extent.height = Textures->Scene->height;
	Commands->GetDrawCommands()->clearAttachments(1, &attachment, 1, &rect);
	unguard;
}

void UVulkanRenderDevice::PushHit(const BYTE* Data, INT Count)
{
	guard(UVulkanRenderDevice::PushHit);
	unguard;
}

void UVulkanRenderDevice::PopHit(INT Count, UBOOL bForce)
{
	guard(UVulkanRenderDevice::PopHit);
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
	float gamma = 1.5f * Viewport->GetOuterUClient()->Brightness;

	ImageBuilder imgbuilder;
	imgbuilder.setFormat(VK_FORMAT_B8G8R8A8_UNORM);
	imgbuilder.setUsage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	imgbuilder.setSize(w, h);
	auto dstimage = imgbuilder.create(Device);

	// Convert from rgba16f to bgra8 using the GPU:
	auto srcimage = Textures->Scene->ppImage.get();
	auto cmdbuffer = Commands->GetDrawCommands();

	PipelineBarrier barrier0;
	barrier0.addImage(srcimage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT);
	barrier0.addImage(dstimage.get(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
	barrier0.execute(cmdbuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

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

	PipelineBarrier barrier1;
	barrier1.addImage(srcimage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT);
	barrier1.addImage(dstimage.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
	barrier1.execute(cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	// Staging buffer for download
	BufferBuilder bufbuilder;
	bufbuilder.setSize(w * h * 4);
	bufbuilder.setUsage(VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU);
	auto staging = bufbuilder.create(Device);

	// Copy from image to buffer
	VkBufferImageCopy region = {};
	region.imageExtent.width = w;
	region.imageExtent.height = h;
	region.imageExtent.depth = 1;
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	cmdbuffer->copyImageToBuffer(dstimage->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging->buffer, 1, &region);

	// Submit command buffers and wait for device to finish the work
	Commands->SubmitCommands(false, 0, 0);

	uint8_t* pixels = (uint8_t*)staging->Map(0, w * h * 4);
	if (gamma != 1.0f)
	{
		float invGamma = 1.0f / gamma;

		uint8_t gammatable[256];
		for (int i = 0; i < 256; i++)
			gammatable[i] = (int)clamp(std::round(std::pow(i / 255.0f, invGamma) * 255.0f), 0.0f, 255.0f);

		uint8_t* dest = (uint8_t*)data;
		for (int i = 0; i < w * h * 4; i += 4)
		{
			dest[i] = gammatable[pixels[i]];
			dest[i + 1] = gammatable[pixels[i + 1]];
			dest[i + 2] = gammatable[pixels[i + 2]];
			dest[i + 3] = pixels[i + 3];
		}
	}
	else
	{
		memcpy(data, pixels, w * h * 4);
	}
	staging->Unmap();

	unguard;
}

void UVulkanRenderDevice::EndFlash()
{
	guard(UVulkanRenderDevice::EndFlash);
	if (FlashScale != FPlane(0.5f, 0.5f, 0.5f, 0.0f) || FlashFog != FPlane(0.0f, 0.0f, 0.0f, 0.0f))
	{
		float r = FlashFog.X;
		float g = FlashFog.Y;
		float b = FlashFog.Z;
		float a = 1.0f - Min(FlashScale.X * 2.0f, 1.0f);

		auto cmdbuffer = Commands->GetDrawCommands();

		ScenePushConstants pushconstants;
		pushconstants.objectToProjection = mat4::identity();
		cmdbuffer->pushConstants(RenderPasses->ScenePipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ScenePushConstants), &pushconstants);

		cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, RenderPasses->getEndFlashPipeline());
		cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, RenderPasses->ScenePipelineLayout.get(), 0, DescriptorSets->GetTextureDescriptorSet(0, nullptr));

		SceneVertex* v = &Buffers->SceneVertices[SceneVertexPos];

		v[0] = { 0, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, r, g, b, a };
		v[1] = { 0,  1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, r, g, b, a };
		v[2] = { 0,  1.0f,  1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, r, g, b, a };
		v[3] = { 0, -1.0f,  1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, r, g, b, a };

		size_t start = SceneVertexPos;
		size_t count = 4;
		SceneVertexPos += count;
		cmdbuffer->draw(count, 1, start, 0);

		if (CurrentFrame)
			SetSceneNode(CurrentFrame);
	}
	unguard;
}

void UVulkanRenderDevice::SetSceneNode(FSceneNode* Frame)
{
	guard(UVulkanRenderDevice::SetSceneNode);

	CurrentFrame = Frame;
	Aspect = Frame->FY / Frame->FX;
	RProjZ = (float)appTan(radians(Viewport->Actor->FovAngle) * 0.5);
	RFX2 = 2.0f * RProjZ / Frame->FX;
	RFY2 = 2.0f * RProjZ * Aspect / Frame->FY;

	auto commands = Commands->GetDrawCommands();

	VkViewport viewportdesc = {};
	viewportdesc.x = Frame->XB;
	viewportdesc.y = Frame->YB;
	viewportdesc.width = Frame->X;
	viewportdesc.height = Frame->Y;
	viewportdesc.minDepth = 0.0f;
	viewportdesc.maxDepth = 1.0f;
	commands->setViewport(0, 1, &viewportdesc);

	mat4 projection = mat4::frustum(-RProjZ, RProjZ, -Aspect * RProjZ, Aspect * RProjZ, 1.0f, 32768.0f, handedness::left, clipzrange::zero_positive_w);

	ScenePushConstants pushconstants;
	pushconstants.objectToProjection = projection;
	commands->pushConstants(RenderPasses->ScenePipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ScenePushConstants), &pushconstants);

	unguard;
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

	PipelineBarrier barrier0;
	barrier0.addImage(
		buffers->colorBuffer.get(),
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_TRANSFER_READ_BIT);
	barrier0.addImage(
		buffers->ppImage.get(),
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_TRANSFER_WRITE_BIT);
	barrier0.execute(Commands->GetDrawCommands(), VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	if (buffers->sceneSamples != VK_SAMPLE_COUNT_1_BIT)
	{
		VkImageResolve resolve = {};
		resolve.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		resolve.srcSubresource.layerCount = 1;
		resolve.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		resolve.dstSubresource.layerCount = 1;
		resolve.extent = { (uint32_t)buffers->colorBuffer->width, (uint32_t)buffers->colorBuffer->height, 1 };
		cmdbuffer->resolveImage(
			buffers->colorBuffer->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			buffers->ppImage->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &resolve);
	}
	else
	{
		auto colorBuffer = buffers->colorBuffer.get();
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
			buffers->ppImage->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit, VK_FILTER_NEAREST);
	}

	PipelineBarrier barrier1;
	barrier1.addImage(
		buffers->ppImage.get(),
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT);
	barrier1.execute(Commands->GetDrawCommands(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

void UVulkanRenderDevice::DrawPresentTexture(int x, int y, int width, int height)
{
	if (Commands->SwapChain->newSwapChain)
	{
		Commands->SwapChain->newSwapChain = false;
		RenderPasses->CreatePresentRenderPass();
		RenderPasses->CreatePresentPipeline();
	}

	float gamma = (1.5f * Viewport->GetOuterUClient()->Brightness * 2.0f);

	PresentPushConstants pushconstants;
	pushconstants.InvGamma = 1.0f / gamma;

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
	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, RenderPasses->PresentPipeline.get());
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, RenderPasses->PresentPipelineLayout.get(), 0, DescriptorSets->GetPresentDescriptorSet());
	cmdbuffer->pushConstants(RenderPasses->PresentPipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PresentPushConstants), &pushconstants);
	cmdbuffer->draw(6, 1, 0, 0);
	RenderPasses->EndPresent(cmdbuffer);
}
