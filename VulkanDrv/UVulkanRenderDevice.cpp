
#include "Precomp.h"
#include "UVulkanRenderDevice.h"
#include "UVulkanClient.h"
#include "VulkanSwapChain.h"
#include "VulkanPostprocess.h"
#include "VulkanTexture.h"
#include "SceneBuffers.h"
#include "SceneRenderPass.h"
#include "SceneSamplers.h"

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
	VolumetricLighting = 1;
	ShinySurfaces = 1;
	Coronas = 1;
	HighDetailActors = 1;
	SupportsTC = 1;
	SupportsLazyTextures = 0;
	PrefersDeferredLoad = 0;
	DetailTextures = 1;
	VSync = 1;

	new(GetClass(), TEXT("VSync"), RF_Public) UBoolProperty(CPP_PROPERTY(VSync), TEXT("Display"), CPF_Config);
	new(GetClass(), TEXT("UsePrecache"), RF_Public) UBoolProperty(CPP_PROPERTY(UsePrecache), TEXT("Display"), CPF_Config);

	unguard;
}

UBOOL UVulkanRenderDevice::Init(UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen)
{
	guard(UVulkanRenderDevice::Init);

	Viewport = InViewport;
	renderer = new Renderer((HWND)Viewport->GetWindow());

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

	delete renderer;
	renderer = nullptr;

	unguard;
}

void UVulkanRenderDevice::Flush(UBOOL AllowPrecache)
{
	guard(UVulkanRenderDevice::Flush);

	if (IsLocked)
	{
		renderer->SubmitCommands(false);
		renderer->ClearTextureCache();

		auto cmdbuffer = renderer->GetDrawCommands();
		renderer->SceneRenderPass->begin(cmdbuffer);

		VkBuffer vertexBuffers[] = { renderer->SceneVertexBuffer->buffer };
		VkDeviceSize offsets[] = { 0 };
		cmdbuffer->bindVertexBuffers(0, 1, vertexBuffers, offsets);
	}
	else
	{
		renderer->ClearTextureCache();
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
		if (!renderer->SceneBuffers || renderer->SceneBuffers->width != Viewport->SizeX || renderer->SceneBuffers->height != Viewport->SizeY)
		{
			delete renderer->SceneRenderPass; renderer->SceneRenderPass = nullptr;
			delete renderer->SceneBuffers; renderer->SceneBuffers = nullptr;
			renderer->SceneBuffers = new SceneBuffers(renderer->Device, Viewport->SizeX, Viewport->SizeY);
			renderer->SceneRenderPass = new SceneRenderPass(renderer);
		}

		auto cmdbuffer = renderer->GetDrawCommands();

		renderer->Postprocess->beginFrame();
		renderer->Postprocess->imageTransitionScene(true);
		renderer->SceneRenderPass->begin(cmdbuffer);

		VkBuffer vertexBuffers[] = { renderer->SceneVertexBuffer->buffer };
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

	renderer->SceneRenderPass->end(renderer->GetDrawCommands());

	renderer->Postprocess->blitSceneToPostprocess();

	int sceneWidth = renderer->SceneBuffers->colorBuffer->width;
	int sceneHeight = renderer->SceneBuffers->colorBuffer->height;

	auto pp = renderer->PostprocessModel;

	VulkanPPRenderState renderstate(renderer);
	//pp->exposure.render(&renderstate, sceneWidth, sceneHeight);
	//pp->bloom.renderBloom(&renderstate, sceneWidth, sceneHeight);
	pp->tonemap.render(&renderstate, sceneWidth, sceneHeight);
	//pp->bloom.renderBlur(&renderstate, sceneWidth, sceneHeight);

	renderer->SubmitCommands(Blit ? true : false);
	renderer->SceneVertexPos = 0;

	IsLocked = false;

	unguard;
}

void UVulkanRenderDevice::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet)
{
	guard(UVulkanRenderDevice::DrawComplexSurface);

	VulkanCommandBuffer* cmdbuffer = renderer->GetDrawCommands();

	VulkanTexture* tex = renderer->GetTexture(Surface.Texture, Surface.PolyFlags);
	VulkanTexture* lightmap = renderer->GetTexture(Surface.LightMap, 0);
	VulkanTexture* macrotex = renderer->GetTexture(Surface.MacroTexture, 0);
	VulkanTexture* detailtex = renderer->GetTexture(Surface.DetailTexture, 0);
	VulkanTexture* fogmap = renderer->GetTexture(Surface.FogMap, 0);

	if ((Surface.DetailTexture && Surface.FogMap) || (!DetailTextures)) detailtex = nullptr;

	float UDot = Facet.MapCoords.XAxis | Facet.MapCoords.Origin;
	float VDot = Facet.MapCoords.YAxis | Facet.MapCoords.Origin;

	float UPan = tex ? UDot + Surface.Texture->Pan.X : 0.0f;
	float VPan = tex ? VDot + Surface.Texture->Pan.Y : 0.0f;
	float UMult = tex ? tex->UMult : 0.0f;
	float VMult = tex ? tex->VMult : 0.0f;
	float LMUPan = lightmap ? UDot + Surface.LightMap->Pan.X - 0.5f * Surface.LightMap->UScale : 0.0f;
	float LMVPan = lightmap ? VDot + Surface.LightMap->Pan.Y - 0.5f * Surface.LightMap->VScale : 0.0f;
	float LMUMult = lightmap ? lightmap->UMult : 0.0f;
	float LMVMult = lightmap ? lightmap->VMult : 0.0f;
	float MacroUPan = macrotex ? UDot + Surface.MacroTexture->Pan.X : 0.0f;
	float MacroVPan = macrotex ? VDot + Surface.MacroTexture->Pan.Y : 0.0f;
	float MacroUMult = macrotex ? macrotex->UMult : 0.0f;
	float MacroVMult = macrotex ? macrotex->VMult : 0.0f;
	float DetailUPan = detailtex ? UDot + Surface.DetailTexture->Pan.X : 0.0f;
	float DetailVPan = detailtex ? VDot + Surface.DetailTexture->Pan.Y : 0.0f;
	float DetailUMult = detailtex ? detailtex->UMult : 0.0f;
	float DetailVMult = detailtex ? detailtex->VMult : 0.0f;

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
		DetailUMult = fogmap->UMult;
		DetailVMult = fogmap->VMult;
	}

	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->SceneRenderPass->getPipeline(Surface.PolyFlags));
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->ScenePipelineLayout, 0, renderer->GetTextureDescriptorSet(Surface.PolyFlags, tex, lightmap, macrotex, detailtex));

	float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;

	for (FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next)
	{
		SceneVertex* vertexdata = &renderer->SceneVertices[renderer->SceneVertexPos];

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
			vtx.r = r;
			vtx.g = g;
			vtx.b = b;
			vtx.a = a;
		}

		size_t start = renderer->SceneVertexPos;
		size_t count = Poly->NumPts;
		renderer->SceneVertexPos += count;

		cmdbuffer->draw(count, 1, start, 0);
	}

	unguard;
}

void UVulkanRenderDevice::DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, int NumPts, DWORD PolyFlags, FSpanBuffer* Span)
{
	guard(UVulkanRenderDevice::DrawGouraudPolygon);

	auto cmdbuffer = renderer->GetDrawCommands();

	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->SceneRenderPass->getPipeline(PolyFlags));

	VulkanTexture* tex = renderer->GetTexture(&Info, PolyFlags);
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->ScenePipelineLayout, 0, renderer->GetTextureDescriptorSet(PolyFlags, tex));

	float UMult = tex ? tex->UMult : 0.0f;
	float VMult = tex ? tex->VMult : 0.0f;

	SceneVertex* vertexdata = &renderer->SceneVertices[renderer->SceneVertexPos];

	for (INT i = 0; i < NumPts; i++)
	{
		FTransTexture* P = Pts[i];
		SceneVertex& v = vertexdata[i];
		v.flags = 0;
		v.x = P->Point.X;
		v.y = P->Point.Y;
		v.z = P->Point.Z;
		v.u = P->U * UMult;
		v.v = P->V * VMult;
		v.u2 = 0.0f;
		v.v2 = 0.0f;
		v.u3 = 0.0f;
		v.v3 = 0.0f;
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

	size_t start = renderer->SceneVertexPos;
	size_t count = NumPts;
	renderer->SceneVertexPos += count;

	cmdbuffer->draw(count, 1, start, 0);

	unguard;
}

void UVulkanRenderDevice::DrawTile(FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags)
{
	guard(UVulkanRenderDevice::DrawTile);

	if ((PolyFlags & (PF_Modulated)) == (PF_Modulated) && Info.Format == TEXF_P8)
		PolyFlags = PF_Modulated;

	auto cmdbuffer = renderer->GetDrawCommands();

	VulkanTexture* tex = renderer->GetTexture(&Info, PolyFlags);

	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->SceneRenderPass->getPipeline(PolyFlags));
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->ScenePipelineLayout, 0, renderer->GetTextureDescriptorSet(PolyFlags, tex, nullptr, nullptr, nullptr, true));

	float UMult = tex ? tex->UMult : 0.0f;
	float VMult = tex ? tex->VMult : 0.0f;

	SceneVertex* v = &renderer->SceneVertices[renderer->SceneVertexPos];

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

	size_t start = renderer->SceneVertexPos;
	size_t count = 4;
	renderer->SceneVertexPos += count;
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
	rect.rect.extent.width = renderer->SceneBuffers->width;
	rect.rect.extent.height = renderer->SceneBuffers->height;
	renderer->GetDrawCommands()->clearAttachments(1, &attachment, 1, &rect);
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
	renderer->CopyScreenToBuffer(Viewport->SizeX, Viewport->SizeY, Pixels);
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
		r *= a;
		g *= a;
		b *= a;

		auto cmdbuffer = renderer->GetDrawCommands();

		ScenePushConstants pushconstants;
		pushconstants.objectToProjection = mat4::identity();
		cmdbuffer->pushConstants(renderer->ScenePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ScenePushConstants), &pushconstants);

		cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->SceneRenderPass->getEndFlashPipeline());
		cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->ScenePipelineLayout, 0, renderer->GetTextureDescriptorSet(0, nullptr));

		SceneVertex* v = &renderer->SceneVertices[renderer->SceneVertexPos];

		v[0] = { 0, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, r, g, b, a };
		v[1] = { 0,  1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, r, g, b, a };
		v[2] = { 0,  1.0f,  1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, r, g, b, a };
		v[3] = { 0, -1.0f,  1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, r, g, b, a };

		size_t start = renderer->SceneVertexPos;
		size_t count = 4;
		renderer->SceneVertexPos += count;
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
	RProjZ = (float)appTan(radians(Viewport->Actor->FovAngle) * 0.5f);
	RFX2 = 2.0f * RProjZ / Frame->FX;
	RFY2 = 2.0f * RProjZ * Aspect / Frame->FY;

	auto commands = renderer->GetDrawCommands();

	VkViewport viewportdesc = {};
	viewportdesc.x = Frame->XB;
	viewportdesc.y = Frame->YB;
	viewportdesc.width = Frame->X;
	viewportdesc.height = Frame->Y;
	viewportdesc.minDepth = 0.0f;
	viewportdesc.maxDepth = 1.0f;
	commands->setViewport(0, 1, &viewportdesc);

	mat4 projection = mat4::frustum(-RProjZ, RProjZ, -Aspect * RProjZ, Aspect * RProjZ, 1.0f, 32768.0f, handedness::left, clipzrange::zero_positive_w);
	mat4 modelview = mat4::scale(1.0f, 1.0f, 1.0f);

	ScenePushConstants pushconstants;
	pushconstants.objectToProjection = projection * modelview;
	commands->pushConstants(renderer->ScenePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ScenePushConstants), &pushconstants);

	unguard;
}

void UVulkanRenderDevice::PrecacheTexture(FTextureInfo& Info, DWORD PolyFlags)
{
	guard(UVulkanRenderDevice::PrecacheTexture);
	renderer->GetTexture(&Info, PolyFlags);
	unguard;
}
