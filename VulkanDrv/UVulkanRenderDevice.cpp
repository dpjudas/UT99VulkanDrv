
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

	unguard;
}

void UVulkanRenderDevice::Flush(UBOOL AllowPrecache)
{
	guard(UVulkanRenderDevice::Flush);

	auto viewport = GetViewport();
	if (IsLocked)
	{
		viewport->SubmitCommands(false);
		viewport->ClearTextureCache();

		auto cmdbuffer = viewport->GetDrawCommands();
		viewport->SceneRenderPass->begin(cmdbuffer);

		VkBuffer vertexBuffers[] = { viewport->SceneVertexBuffer->buffer };
		VkDeviceSize offsets[] = { 0 };
		cmdbuffer->bindVertexBuffers(0, 1, vertexBuffers, offsets);
	}
	else
	{
		viewport->ClearTextureCache();
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

	auto viewport = GetViewport();
	if (!viewport->SceneBuffers || viewport->SceneBuffers->width != viewport->SizeX || viewport->SceneBuffers->height != viewport->SizeY)
	{
		delete viewport->SceneRenderPass; viewport->SceneRenderPass = nullptr;
		delete viewport->SceneBuffers; viewport->SceneBuffers = nullptr;
		viewport->SceneBuffers = new SceneBuffers(viewport->Device, viewport->SizeX, viewport->SizeY);
		viewport->SceneRenderPass = new SceneRenderPass(viewport);
	}

	auto cmdbuffer = viewport->GetDrawCommands();

	viewport->Postprocess->beginFrame();
	viewport->Postprocess->imageTransitionScene(true);
	viewport->SceneRenderPass->begin(cmdbuffer);

	VkBuffer vertexBuffers[] = { viewport->SceneVertexBuffer->buffer };
	VkDeviceSize offsets[] = { 0 };
	cmdbuffer->bindVertexBuffers(0, 1, vertexBuffers, offsets);

	IsLocked = true;

	unguard;
}

void UVulkanRenderDevice::Unlock(UBOOL Blit)
{
	guard(UVulkanRenderDevice::Unlock);

	auto viewport = GetViewport();
	viewport->SceneRenderPass->end(viewport->GetDrawCommands());

	viewport->Postprocess->blitSceneToPostprocess();

	int sceneWidth = viewport->SceneBuffers->colorBuffer->width;
	int sceneHeight = viewport->SceneBuffers->colorBuffer->height;

	auto pp = viewport->PostprocessModel;

	VulkanPPRenderState renderstate(viewport);
	pp->exposure.render(&renderstate, sceneWidth, sceneHeight);
	pp->bloom.renderBloom(&renderstate, sceneWidth, sceneHeight);
	pp->tonemap.render(&renderstate, sceneWidth, sceneHeight);
	//pp->bloom.renderBlur(&renderstate, sceneWidth, sceneHeight);

	GetViewport()->SubmitCommands(Blit ? true : false);
	viewport->SceneVertexPos = 0;

	IsLocked = false;

	unguard;
}

void UVulkanRenderDevice::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet)
{
	guard(UVulkanRenderDevice::DrawComplexSurface);

	// glColor4f( TexInfo[0].ColorRenorm.X*TexInfo[1].ColorRenorm.X, TexInfo[0].ColorRenorm.Y*TexInfo[1].ColorRenorm.Y, TexInfo[0].ColorRenorm.Z*TexInfo[1].ColorRenorm.Z, 1 );

	UVulkanViewport* viewport = GetViewport();
	VulkanCommandBuffer* cmdbuffer = viewport->GetDrawCommands();

	VulkanTexture* tex = viewport->GetTexture(Surface.Texture, Surface.PolyFlags);
	VulkanTexture* lightmap = viewport->GetTexture(Surface.LightMap, 0);

	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, viewport->SceneRenderPass->getPipeline(Surface.PolyFlags));
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, viewport->ScenePipelineLayout, 0, viewport->GetTextureDescriptorSet(tex, lightmap));

	FLOAT UDot = Facet.MapCoords.XAxis | Facet.MapCoords.Origin;
	FLOAT VDot = Facet.MapCoords.YAxis | Facet.MapCoords.Origin;

	float r = 1.0f; // TexInfo[0].ColorRenorm.X* TexInfo[1].ColorRenorm.X;
	float g = 1.0f; // TexInfo[0].ColorRenorm.Y * TexInfo[1].ColorRenorm.Y;
	float b = 1.0f; // TexInfo[0].ColorRenorm.Z * TexInfo[1].ColorRenorm.Z;
	float a = 1.0f;

	float UPan = Surface.Texture->Pan.X;
	float VPan = Surface.Texture->Pan.Y;
	float UMult = tex ? tex->UMult : 0.0f;
	float VMult = tex ? tex->VMult : 0.0f;
	float LMUPan = lightmap ? Surface.LightMap->Pan.X - 0.5f * Surface.LightMap->UScale : 0.0f;
	float LMVPan = lightmap ? Surface.LightMap->Pan.Y - 0.5f * Surface.LightMap->VScale : 0.0f;
	float LMUMult = lightmap ? lightmap->UMult : 0.0f;
	float LMVMult = lightmap ? lightmap->VMult : 0.0f;

	for (FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next)
	{
		SceneVertex* vertexdata = &viewport->SceneVertices[viewport->SceneVertexPos];

		float uu[] = { 0.0f, 1.0f, 1.0f, 0.0f };
		float vv[] = { 0.0f, 0.0f, 1.0f, 1.0f };

		for (INT i = 0; i < Poly->NumPts; i++)
		{
			SceneVertex& vtx = vertexdata[i];

			FLOAT U = Facet.MapCoords.XAxis | Poly->Pts[i]->Point;
			FLOAT V = Facet.MapCoords.YAxis | Poly->Pts[i]->Point;
			vtx.x = Poly->Pts[i]->Point.X;
			vtx.y = Poly->Pts[i]->Point.Y;
			vtx.z = Poly->Pts[i]->Point.Z;
			vtx.u = (U - UDot - UPan) * UMult;
			vtx.v = (V - VDot - VPan) * VMult;
			vtx.lu = (U - UDot - LMUPan) * LMUMult;
			vtx.lv = (V - VDot - LMVPan) * LMVMult;
			vtx.r = r;
			vtx.g = g;
			vtx.b = b;
			vtx.a = a;
		}

		size_t start = viewport->SceneVertexPos;
		size_t count = Poly->NumPts;
		viewport->SceneVertexPos += count;

		cmdbuffer->draw(count, 1, start, 0);
	}

	if (Surface.FogMap)
	{
	}

	if ((Surface.PolyFlags & PF_Selected) && GIsEditor)
	{
	}

	unguard;
}

void UVulkanRenderDevice::DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, int NumPts, DWORD PolyFlags, FSpanBuffer* Span)
{
	guard(UVulkanRenderDevice::DrawGouraudPolygon);

	if (NumPts < 3) return;

	// SetBlend(PF_Highlighted)

	UVulkanViewport* viewport = GetViewport();
	auto cmdbuffer = viewport->GetDrawCommands();

	VulkanTexture* tex = viewport->GetTexture(&Info, PolyFlags);

	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, viewport->SceneRenderPass->getPipeline(PolyFlags));
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, viewport->ScenePipelineLayout, 0, viewport->GetTextureDescriptorSet(tex));

	float UMult = tex ? tex->UMult : 0.0f;
	float VMult = tex ? tex->VMult : 0.0f;

	SceneVertex* vertexdata = &viewport->SceneVertices[viewport->SceneVertexPos];

	if (PolyFlags & PF_Modulated)
	{
		for (INT i = 0; i < NumPts; i++)
		{
			FTransTexture* P = Pts[i];
			SceneVertex& v = vertexdata[i];
			v.x = P->Point.X;
			v.y = P->Point.Y;
			v.z = P->Point.Z;
			v.u = P->U * UMult;
			v.v = P->V * VMult;
			v.lu = 0.0f;
			v.lv = 0.0f;
			v.r = 1.0f;
			v.g = 1.0f;
			v.b = 1.0f;
			v.a = 1.0f;
		}
	}
	else
	{
		for (INT i = 0; i < NumPts; i++)
		{
			FTransTexture* P = Pts[i];
			SceneVertex& v = vertexdata[i];
			v.x = P->Point.X;
			v.y = P->Point.Y;
			v.z = P->Point.Z;
			v.u = P->U * UMult;
			v.v = P->V * VMult;
			v.lu = 0.0f;
			v.lv = 0.0f;
			v.r = P->Light.X;
			v.g = P->Light.Y;
			v.b = P->Light.Z;
			v.a = 1.0f;
		}
	}

	size_t start = viewport->SceneVertexPos;
	size_t count = NumPts;
	viewport->SceneVertexPos += count;

	cmdbuffer->draw(count, 1, start, 0);

	unguard;
}

void UVulkanRenderDevice::DrawTile(FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags)
{
	guard(UVulkanRenderDevice::DrawTile);

	UVulkanViewport* viewport = GetViewport();
	auto cmdbuffer = viewport->GetDrawCommands();

	VulkanTexture* tex = viewport->GetTexture(&Info, PolyFlags);

	cmdbuffer->bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, viewport->SceneRenderPass->getPipeline(PolyFlags));
	cmdbuffer->bindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, viewport->ScenePipelineLayout, 0, viewport->GetTextureDescriptorSet(tex));

	float UMult = tex ? tex->UMult : 0.0f;
	float VMult = tex ? tex->VMult : 0.0f;

	SceneVertex* v = &viewport->SceneVertices[viewport->SceneVertexPos];

	float r = Color.X;// *TexInfo[0].ColorNorm.X;
	float g = Color.Y;// *TexInfo[0].ColorNorm.Y;
	float b = Color.Z;// *TexInfo[0].ColorNorm.Z;
	float a = 1.0f;

	v[0] = { RFX2 * Z * (X - Frame->FX2),      RFY2 * Z * (Y - Frame->FY2),      Z, U * UMult,        V * VMult,        0.0f, 0.0f, r, g, b, a };
	v[1] = { RFX2 * Z * (X + XL - Frame->FX2), RFY2 * Z * (Y - Frame->FY2),      Z, (U + UL) * UMult, V * VMult,        0.0f, 0.0f, r, g, b, a };
	v[2] = { RFX2 * Z * (X + XL - Frame->FX2), RFY2 * Z * (Y + YL - Frame->FY2), Z, (U + UL) * UMult, (V + VL) * VMult, 0.0f, 0.0f, r, g, b, a };
	v[3] = { RFX2 * Z * (X - Frame->FX2),      RFY2 * Z * (Y + YL - Frame->FY2), Z, U * UMult,        (V + VL) * VMult, 0.0f, 0.0f, r, g, b, a };

	size_t start = viewport->SceneVertexPos;
	size_t count = 4;
	viewport->SceneVertexPos += count;
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
	auto viewport = GetViewport();
	VkClearAttachment attachment = {};
	VkClearRect rect = {};
	attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	attachment.clearValue.depthStencil.depth = 1.0f;
	rect.layerCount = 1;
	rect.rect.extent.width = viewport->SceneBuffers->width;
	rect.rect.extent.height = viewport->SceneBuffers->height;
	viewport->GetDrawCommands()->clearAttachments(1, &attachment, 1, &rect);
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
	// glReadPixels(0, 0, Viewport->SizeX, Viewport->SizeY, GL_RGBA, GL_UNSIGNED_BYTE, Pixels);
	unguard;
}

void UVulkanRenderDevice::EndFlash()
{
	guard(UVulkanRenderDevice::EndFlash);
	if ((FlashScale != FPlane(0.5f, 0.5f, 0.5f, 0.0f)) || (FlashFog != FPlane(0.0f, 0.0f, 0.0f, 0.0f)))
	{
		// SetBlend(PF_Highlighted);
		// SetScreenQuadColor4f(FlashFog.X, FlashFog.Y, FlashFog.Z, 1.0f - Min(FlashScale.X * 2.0f, 1.0f));
		// DrawScreenQuad();
	}
	unguard;
}

void UVulkanRenderDevice::SetSceneNode(FSceneNode* Frame)
{
	guard(UVulkanRenderDevice::SetSceneNode);

	Aspect = Frame->FY / Frame->FX;
	RProjZ = (float)appTan(radians(Viewport->Actor->FovAngle) * 0.5f);
	RFX2 = 2.0f * RProjZ / Frame->FX;
	RFY2 = 2.0f * RProjZ * Aspect / Frame->FY;

	auto viewport = GetViewport();
	auto commands = viewport->GetDrawCommands();

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
	commands->pushConstants(viewport->ScenePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ScenePushConstants), &pushconstants);

	/*if (HitData)
	{
		FVector N[4] =
		{
			(FVector((Viewport->HitX - Frame->FX2) * Frame->RProj.Z, 0, 1) ^ FVector(0, -1, 0)).SafeNormal(),
			(FVector((Viewport->HitX + Viewport->HitXL - Frame->FX2) * Frame->RProj.Z,0,1) ^ FVector(0,+1,0)).SafeNormal(),
			(FVector(0,(Viewport->HitY - Frame->FY2) * Frame->RProj.Z,1) ^ FVector(+1,0,0)).SafeNormal(),
			(FVector(0,(Viewport->HitY + Viewport->HitYL - Frame->FY2) * Frame->RProj.Z,1) ^ FVector(-1,0,0)).SafeNormal()
		};
		for (INT i = 0; i < 4; i++)
		{
			float clipplane[4] = { N[i].X, N[i].Y, N[i].Z, 0.0f };
			glClipPlane(GL_CLIP_PLANE0 + i, clipplane);
			glEnable(GL_CLIP_PLANE0 + i);
		}
	}*/

	unguard;
}

void UVulkanRenderDevice::PrecacheTexture(FTextureInfo& Info, DWORD PolyFlags)
{
	guard(UVulkanRenderDevice::PrecacheTexture);
	GetViewport()->GetTexture(&Info, PolyFlags);
	unguard;
}
