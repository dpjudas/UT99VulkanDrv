/*=============================================================================
	XMesaGL.cpp: Unreal MesaGL support implementation for X Windows.
	Copyright 1999 Epic Games, Inc. All Rights Reserved.

	Other URenderDevice subclasses include:
	* USoftwareRenderDevice: Software renderer.
	* UGlideRenderDevice: 3dfx Glide renderer.
	* UDirect3DRenderDevice: Direct3D renderer.
	* UOpenGLRenderDevice: OpenGL renderer.

	Revision history:
	* Created by Brandon Reinhart
=============================================================================*/

#include "XMesaGLDrv.h"

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

#define STDGL 1						/* Use standard GL driver or minidriver by default */
#define PYR(n) ((n)*((n+1))/2)		/* Pyramid scaling function */
BYTE ScaleByteNormal  [PYR(256)];	/* Regular normalization table */
BYTE ScaleByteBrighten[PYR(256)];	/* 2X overbrightening normalization table */
FPlane One4(1,1,1,1);				/* 4 consecutive floats for OpenGL */
#define OVERBRIGHT 1.4f
	
/*-----------------------------------------------------------------------------
	UXMesaGLDrv.
-----------------------------------------------------------------------------*/

//
// A MesaGL rendering device attached to an X viewport.
//
class UXMesaGLRenderDevice : public URenderDevice
{
	DECLARE_CLASS(UXMesaGLRenderDevice,URenderDevice,CLASS_Config)

	// Information about a cached texture.
	struct FCachedTexture
	{
		GLuint Id;
		INT BaseMip;
		INT UBits, VBits;
		INT UCopyBits, VCopyBits;
		FPlane ColorNorm, ColorRenorm;
	};

	// Permanent variables.
	Display* XDisplay;
	Window* XWindow;
	GLXContext Context;
	UBOOL WasFullscreen;
	TMap<QWORD,FCachedTexture> LocalBindMap, *BindMap;
	TArray<FPlane> Modes;
	UViewport* Viewport;

	// Timing.
	DWORD BindCycles, ImageCycles, ComplexCycles, GouraudCycles, TileCycles;

	// Hardware constraints.
	INT MaxLogUOverV;
	INT MaxLogVOverU;
	INT MinLogTextureSize;
	INT MaxLogTextureSize;
	UBOOL UseZTrick;
	UBOOL UseMultiTexture;
	UBOOL UsePalette;
	UBOOL Multipass1X;
	UBOOL DoPrecache;
	UBOOL ShareLists;
	UBOOL AlwaysMipmap;
	BYTE* ScaleByte;

	// Hit info.
	BYTE* HitData;
	INT* HitSize;

	// Lock variables.
	UBOOL ZTrickToggle;
	INT ZTrickFunc;
	FPlane FlashScale, FlashFog;
	FLOAT RProjZ, Aspect;
	DWORD CurrentPolyFlags;
	TArray<INT> GLHitData;
	struct FTexInfo
	{
		QWORD CurrentCacheID;
		FLOAT UMult;
		FLOAT VMult;
		FLOAT UPan;
		FLOAT VPan;
		FPlane ColorNorm;
		FPlane ColorRenorm;
	} TexInfo[4];
	FLOAT RFX2, RFY2;

	// Static variables.
	static TMap<QWORD,FCachedTexture> SharedBindMap;
	static TArray<GLXContext> AllContexts;
	static INT			NumDevices;
	static INT			LockCount;
	static GLXContext	CurrentContext;
	static void*   		MesaLib;

	// GL functions.
	#define GL_EXT(name) static UBOOL SUPPORTS##name;
	#define GL_PROC(ext,ret,func,parms) static ret (STDCALL *func)parms;
	#include "XMesaGLFuncs.h"
	#undef GL_EXT
	#undef GL_PROC

	// Implementation.
	UBOOL FindExt( const TCHAR* Name )
	{
		guard(UXMesaGLRenderDevice::FindExt);
		UBOOL Result = appStrfind(appFromAnsi((char*)glGetString(GL_EXTENSIONS)),Name)!=NULL;
		if( Result )
			debugf( NAME_Init, TEXT("Device supports: %s"), Name );
		return Result;
		unguard;
	}
	
	void FindProc( void*& ProcAddress, char* Name, char* SupportName, UBOOL& Supports, UBOOL AllowExt )
	{
		guard(UXMesaGLRenderDevice::FindProc);

		if( !ProcAddress )
			ProcAddress = (void*)dlsym( MesaLib, Name );

		//if( !ProcAddress && Supports && AllowExt )
		//	ProcAddress = wglGetProcAddress( Name );

		if( !ProcAddress )
		{
			if( Supports )
				debugf( TEXT("   Missing function '%s' for '%s' support"), appFromAnsi(Name), appFromAnsi(SupportName) );
			Supports = 0;
		}
		unguard;
	}
	
	void FindProcs( UBOOL AllowExt )
	{
		guard(UXMesaGLRenderDevice::FindProcs);
		#define GL_EXT(name) if( AllowExt ) SUPPORTS##name = FindExt( TEXT(#name)+1 );
		#define GL_PROC(ext,ret,func,parms) FindProc( *(void**)&func, #func, #ext, SUPPORTS##ext, AllowExt );
		#include "XMesaGLFuncs.h"
		#undef GL_EXT
		#undef GL_PROC
		unguard;
	}
	
	UBOOL FailedInitf( const TCHAR* Fmt, ... )
	{
		TCHAR TempStr[4096];
		GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), Fmt );
		debugf( NAME_Init, TempStr );
		Exit();
		return 0;
	}
	
	void MakeCurrent()
	{
		guard(UXMesaGLRenderDevice::MakeCurrent);
		check(XDisplay);
		check(XWindow);
		check(Context);

		if( CurrentContext != Context)
		{
			verify(glXMakeCurrent(XDisplay, *XWindow, Context));
			CurrentContext = Context;
		}

		unguard;
	}
	
	void Check( const char* Tag )
	{
		GLenum Error = glGetError();
		if( Error!=GL_NO_ERROR )
		{
			const TCHAR* Msg=TEXT("Unknown");
			switch( Error )
			{
				case GL_NO_ERROR:
					Msg = TEXT("GL_NO_ERROR");
					break;
				case GL_INVALID_ENUM:
					Msg = TEXT("GL_INVALID_ENUM");
					break;
				case GL_INVALID_VALUE:
					Msg = TEXT("GL_INVALID_VALUE");
					break;
				case GL_INVALID_OPERATION:
					Msg = TEXT("GL_INVALID_OPERATION");
					break;
				case GL_STACK_OVERFLOW:
					Msg = TEXT("GL_STACK_OVERFLOW");
					break;
				case GL_STACK_UNDERFLOW:
					Msg = TEXT("GL_STACK_UNDERFLOW");
					break;
				case GL_OUT_OF_MEMORY:
					Msg = TEXT("GL_OUT_OF_MEMORY");
					break;
			};
			appErrorf( TEXT("OpenGL Error: %s (%s)"), Msg, Tag );
		}
	}
	
	void SetNoTexture( INT Multi )
	{
		guard(UXMesaGLRenderDevice::SetNoTexture);
		if( TexInfo[Multi].CurrentCacheID != 0 )
		{
			// Set small white texture.
			clock(BindCycles);
			glBindTexture( GL_TEXTURE_2D, 0 );
			TexInfo[Multi].CurrentCacheID = 0;
			unclock(BindCycles);
		}
		unguard;
	}
	
	void SetTexture( INT Multi, FTextureInfo& Info, DWORD PolyFlags, FLOAT PanBias )
	{
		guard(UXMesaGLRenderDevice::SetTexture);

		// Set panning.
		FTexInfo& Tex = TexInfo[Multi];
		Tex.UPan      = Info.Pan.X + PanBias*Info.UScale;
		Tex.VPan      = Info.Pan.Y + PanBias*Info.VScale;

		// Find in cache.
		if( Info.CacheID==Tex.CurrentCacheID && !Info.bRealtimeChanged )
			return;

		// Make current.
		clock(BindCycles);
		Tex.CurrentCacheID = Info.CacheID;
		FCachedTexture *Bind=BindMap->Find(Info.CacheID), *ExistingBind=Bind;
		if( !Bind )
		{
			// Figure out OpenGL-related scaling for the texture.
			Bind            = &BindMap->Set( Info.CacheID, FCachedTexture() );
			glGenTextures( 1, &Bind->Id );
			Bind->BaseMip   = Min(0,Info.NumMips-1);
			Bind->UCopyBits = 0;
			Bind->VCopyBits = 0;
			Bind->UBits     = Info.Mips[Bind->BaseMip]->UBits;
			Bind->VBits     = Info.Mips[Bind->BaseMip]->VBits;
			if( Bind->UBits-Bind->VBits > MaxLogUOverV )
			{
				Bind->VCopyBits += (Bind->UBits-Bind->VBits)-MaxLogUOverV;
				Bind->VBits      = Bind->UBits-MaxLogUOverV;
			}
			if( Bind->VBits-Bind->UBits > MaxLogVOverU )
			{
				Bind->UCopyBits += (Bind->VBits-Bind->UBits)-MaxLogVOverU;
				Bind->UBits      = Bind->VBits-MaxLogVOverU;
			}
			if( Bind->UBits < MinLogTextureSize )
			{
				Bind->UCopyBits += MinLogTextureSize - Bind->UBits;
				Bind->UBits     += MinLogTextureSize - Bind->UBits;
			}
			if( Bind->VBits < MinLogTextureSize )
			{
				Bind->VCopyBits += MinLogTextureSize - Bind->VBits;
				Bind->VBits     += MinLogTextureSize - Bind->VBits;
			}
			if( Bind->UBits > MaxLogTextureSize )
			{
				Bind->BaseMip += Bind->UBits-MaxLogTextureSize;
				Bind->VBits   -= Bind->UBits-MaxLogTextureSize;
				Bind->UBits    = MaxLogTextureSize;
				if( Bind->VBits < 0 )
				{
					Bind->VCopyBits = -Bind->VBits;
					Bind->VBits     = 0;
				}
			}
			if( Bind->VBits > MaxLogTextureSize )
			{
				Bind->BaseMip += Bind->VBits-MaxLogTextureSize;
				Bind->UBits   -= Bind->VBits-MaxLogTextureSize;
				Bind->VBits    = MaxLogTextureSize;
				if( Bind->UBits < 0 )
				{
					Bind->UCopyBits = -Bind->UBits;
					Bind->UBits     = 0;
				}
			}
		}
		glBindTexture( GL_TEXTURE_2D, Bind->Id );
		unclock(BindCycles);

		// Account for all the impact on scale normalization.
		Tex.UMult = 1.0 / (Info.UScale * (Info.USize << Bind->UCopyBits));
		Tex.VMult = 1.0 / (Info.VScale * (Info.VSize << Bind->VCopyBits));

		// Upload if needed.
		if( !ExistingBind || Info.bRealtimeChanged )
		{
			// Cleanup texture flags.
			Info.Load();
			Info.bRealtimeChanged = 0;

			// Set maximum color.
//			Info.CacheMaxColor();
			*Info.MaxColor = FColor(255,255,255,255); // Brandon's color hack.
			Bind->ColorNorm = Info.MaxColor->Plane();
			Bind->ColorNorm.W = 1;
			if( Multipass1X )
			{
				Bind->ColorRenorm.X = Min( Bind->ColorNorm.X * OVERBRIGHT, 1.0f );
				Bind->ColorRenorm.Y = Min( Bind->ColorNorm.Y * OVERBRIGHT, 1.0f );
				Bind->ColorRenorm.Z = Min( Bind->ColorNorm.Z * OVERBRIGHT, 1.0f );
			}
			else Bind->ColorRenorm = Bind->ColorNorm;

			// Setup scaling.
			BYTE* ScaleR = &ScaleByte[PYR(Info.MaxColor->R)];
			BYTE* ScaleG = &ScaleByte[PYR(Info.MaxColor->G)];
			BYTE* ScaleB = &ScaleByte[PYR(Info.MaxColor->B)];

			// Generate the palette.
			FColor LocalPal[256], *NewPal=Info.Palette, TempColor(0,0,0,0);
			UBOOL Paletted = UsePalette && Info.Palette && !(PolyFlags & PF_Masked) && Info.Palette[0].A==255;//!!hw might support alpha palettes?
			if( Info.Palette )
			{
				TempColor = Info.Palette[0];
				if( PolyFlags & PF_Masked )
					Info.Palette[0] = FColor(0,0,0,0);
				NewPal = LocalPal;
				for( INT i=0; i<256; i++ )
				{
					FColor& Src = Info.Palette[i];
					NewPal[i].R = ScaleR[Src.R];
					NewPal[i].G = ScaleG[Src.G];
					NewPal[i].B = ScaleB[Src.B];
					NewPal[i].A = Src.A;
				}
				if( Paletted )
					glColorTableEXT( GL_TEXTURE_2D, GL_RGBA, 256, GL_RGBA, GL_UNSIGNED_BYTE, NewPal );
			}

			// Download the texture.
			clock(ImageCycles);
			FMemMark Mark(GMem);
			BYTE* Compose  = New<BYTE>( GMem, (1<<(Bind->UBits+Bind->VBits))*4 );
			UBOOL SkipMipmaps = Info.NumMips==1 && !AlwaysMipmap;
			for( INT Level=0; Level<=Max(Bind->UBits,Bind->VBits); Level++ )
			{
				// Convert the mipmap.
				INT MipIndex=Bind->BaseMip+Level, StepBits=0;
				if( MipIndex>=Info.NumMips )
				{
					StepBits = MipIndex - (Info.NumMips - 1);
					MipIndex = Info.NumMips - 1;
				}
				FMipmapBase* Mip      = Info.Mips[MipIndex];
				BYTE*        Src      = (BYTE*)Compose;
				DWORD        Mask     = Mip->USize-1;
				GLuint       SourceFormat, InternalFormat;
				if( Mip->DataPtr )
				{
					if( Paletted )
					{
						guard(ConvertP8_P8);
						SourceFormat   = GL_COLOR_INDEX;
						InternalFormat = GL_COLOR_INDEX8_EXT;
						BYTE* Ptr      = (BYTE*)Compose;
						for( INT i=0; i<(1<<Max(0,Bind->VBits-Level)); i++ )
						{
							BYTE* Base = (BYTE*)Mip->DataPtr + ((i<<StepBits)&(Mip->VSize-1))*Mip->USize;
							for( INT j=0; j<(1<<Max(0,Bind->UBits-Level+StepBits)); j+=(1<<StepBits) )
								*Ptr++ = Base[j&Mask];
						}
						unguard;
					}
					else if( Info.Palette )
					{
						guard(ConvertP8_RGBA8888);
						SourceFormat   = GL_RGBA;
						InternalFormat = GL_RGBA8; // GL_RGBA4!!
						FColor* Ptr    = (FColor*)Compose;
						for( INT i=0; i<(1<<Max(0,Bind->VBits-Level)); i++ )
						{
							BYTE* Base = (BYTE*)Mip->DataPtr + ((i<<StepBits)&(Mip->VSize-1))*Mip->USize;
							for( INT j=0; j<(1<<Max(0,Bind->UBits-Level+StepBits)); j+=(1<<StepBits) )
								*Ptr++ = NewPal[Base[j&Mask]];
						}
						unguard;
					}
					else
					{
						guard(ConvertBGRA7777_RGBA8888);
						SourceFormat   = GL_RGBA;
						InternalFormat = GL_RGBA8;
						FColor* Ptr    = (FColor*)Compose;
						for( INT i=0; i<(1<<Max(0,Bind->VBits-Level)); i++ )
						{
							FColor* Base = (FColor*)Mip->DataPtr + Min<DWORD>((i<<StepBits)&(Mip->VSize-1),Info.VClamp-1)*Mip->USize;
							for( INT j=0; j<(1<<Max(0,Bind->UBits-Level+StepBits)); j+=(1<<StepBits) )
							{
								FColor& Src = Base[Min<DWORD>(j&Mask,Info.UClamp-1)]; 
								Ptr->R      = ScaleB[Src.B];
								Ptr->G      = ScaleG[Src.G];
								Ptr->B      = ScaleR[Src.R];
								Ptr->A      = Src.A*2;
								Ptr++;
							}
						}
						unguard;
					}
				}
				if( ExistingBind )//crashes frequently on TNT for Apache. My bug or theirs?
				{
					guard(glTexSubImage2D);
					glTexSubImage2D( (GLenum) GL_TEXTURE_2D, Level, 0, 0, 1<<Max(0,Bind->UBits-Level), 1<<Max(0,Bind->VBits-Level), (GLenum) SourceFormat, (GLenum) GL_UNSIGNED_BYTE, Src );
					unguard;
				}
				else
				{
					guard(glTexImage2D);
					glTexImage2D( (GLenum) GL_TEXTURE_2D, Level, InternalFormat, 1<<Max(0,Bind->UBits-Level), 1<<Max(0,Bind->VBits-Level), 0, (GLenum) SourceFormat, (GLenum) GL_UNSIGNED_BYTE, Src );
					unguard;
				}
				if( SkipMipmaps )
					break;
			}
			Mark.Pop();
			unclock(ImageCycles);

			// Set texture state.
			if( !(PolyFlags & PF_NoSmooth) )
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, SkipMipmaps ? GL_LINEAR : GL_LINEAR_MIPMAP_NEAREST );
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			}
			else
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, SkipMipmaps ? GL_NEAREST : GL_NEAREST_MIPMAP_NEAREST );
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			}

			// Cleanup.
			if( Info.Palette )
				Info.Palette[0] = TempColor;
			if( SupportsLazyTextures )
				Info.Unload();
		}

		// Copy color norm.
		Tex.ColorNorm   = Bind->ColorNorm;
		Tex.ColorRenorm = Bind->ColorRenorm;

		unguard;
	}
	
	void SetBlend( DWORD PolyFlags )
	{
		guardSlow(UXMesaGLRenderDevice::SetBlend);

		// Adjust PolyFlags according to Unreal's precedence rules.
		if( !(PolyFlags & (PF_Translucent|PF_Modulated)) )
			PolyFlags |= PF_Occlude;
		else if( PolyFlags & PF_Translucent )
			PolyFlags &= ~PF_Masked;

		// Detect changes in the blending modes.
		DWORD Xor = CurrentPolyFlags^PolyFlags;
		if( Xor & (PF_Translucent|PF_Modulated|PF_Invisible|PF_Occlude|PF_Masked|PF_Highlighted) )
		{
			if( Xor&(PF_Translucent|PF_Modulated|PF_Highlighted) )
			{
				if( PolyFlags & PF_Translucent )
				{
					glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_COLOR );
				}
				else if( PolyFlags & PF_Modulated )
				{
					glBlendFunc( GL_DST_COLOR, GL_SRC_COLOR );
				}
				else if( PolyFlags & PF_Highlighted )
				{
					glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
				}
				else
				{
					glBlendFunc( GL_ONE, GL_ZERO );
				}
			}
			if( Xor & PF_Invisible )
			{
				UBOOL Show = ((PolyFlags&PF_Invisible)==0);
				glColorMask( Show, Show, Show, Show );
			}
			if( Xor & PF_Occlude )
			{
				glDepthMask( (PolyFlags&PF_Occlude)!=0 );
			}
			if( Xor & PF_Masked )
			{
				if( PolyFlags & PF_Masked )
					glEnable( GL_ALPHA_TEST );
				else
					glDisable( GL_ALPHA_TEST );
			}
		}
		CurrentPolyFlags = PolyFlags;
		unguardSlow;
	}

	// URenderDevice interface.
	UBOOL Init( UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen )
	{
		guard(UXMesaGLRenderDevice::Init);

		debugf( TEXT("Initializing XMesaGLDrv...") );

		// X windows.
		XDisplay			= 0;
		XWindow				= 0;
		
		// Driver flags.
		SpanBased			= 0;
		FullscreenOnly		= 0;	//Driver dependent.
		SupportsFogMaps		= 1;
		SupportsDistanceFog	= 0;

		// Init global GL.
		if( NumDevices==0 )
		{
			// Clear dynamic load error conditions.
			dlerror();

			// Bind the library.
			MesaLib = (void*)dlopen( "/usr/lib/libMesaGL.so.3.0", RTLD_NOW );
			if (MesaLib == NULL)
				appErrorf( TEXT("Unable to bind libMesaGL.so!") );

			// Check available functions to confirm compatibility.
			SUPPORTS_GL = 1;
			FindProcs( 0 );
			if( !SUPPORTS_GL )
				return 0;

			// Init pyramid compression table.
			for( INT A=0; A<256; A++ )
			{
				for( INT B=0; B<=A; B++ )
				{
					FLOAT F                     = (FLOAT)B/Max(A,1);
					ScaleByteNormal  [PYR(A)+B] = (BYTE) (255.f*F);
					ScaleByteBrighten[PYR(A)+B] = (BYTE) (255.f*Min(1.f,(OVERBRIGHT)*F-(OVERBRIGHT-1.0f)*F*F*F*F*F*F));//!!always saturates, should be better
				}
			}
		}
		NumDevices++;

		BindMap = ShareLists ? &SharedBindMap : &LocalBindMap;
		Viewport = InViewport;

		// Get the viewport's display.
		XDisplay = (Display*) (InViewport->GetServer());
		check(XDisplay);
		
		// Get the viewport window.
		XWindow = (Window*) (InViewport->GetWindow());
		check(XWindow);

		// Try to change resolution to desired.
		if( !SetRes( NewX, NewY, NewColorBytes, Fullscreen ) )
			return FailedInitf( LocalizeError("ResFailed") );

		return 1;
		unguard;
	}
	
	void UnsetRes()
	{
		guard(UOpenGLRenderDevice::UnsetRes);
		check(Context)

		CurrentContext = NULL;
		glXMakeCurrent( XDisplay, *XWindow, NULL );
		if (Context)
			glXDestroyContext( XDisplay, Context );
		verify(AllContexts.RemoveItem(Context));
		Context = NULL;

		unguard;
	}
	
	UBOOL SetRes( INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen )
	{
		guard(UXMesaGlRenderDevice::SetRes);
		check(XDisplay);
		check(XWindow);

		// Flush textures.
//		Flush( 1 );

		// Exit res.
		if( Context )
			UnsetRes();

		Fullscreen = 1;

		// Change window size.
		Viewport->ResizeViewport( Fullscreen ? (BLIT_Fullscreen|BLIT_OpenGL) : (BLIT_HardwarePaint|BLIT_OpenGL), NewX, NewY, NewColorBytes );
		
		// Find a visual that supports us.
		XVisualInfo* VisualInfo = NULL;
		INT DesiredColorBits   = NewColorBytes<=2 ? 16 : 24;
//		INT DesiredColorBits   = 24;
		INT DesiredStencilBits = NewColorBytes<=2 ? 0  : 8;
		INT DesiredDepthBits   = NewColorBytes<=2 ? 16 : 32;
		INT AttributeList[] = {
			GLX_USE_GL,
			GLX_RGBA, 
			GLX_DOUBLEBUFFER, 
			GLX_BUFFER_SIZE, DesiredColorBits,
			GLX_DEPTH_SIZE, DesiredDepthBits, 
			GLX_STENCIL_SIZE, DesiredStencilBits,
			None };
		VisualInfo = glXChooseVisual( XDisplay, DefaultScreen(XDisplay), AttributeList );
		if (!VisualInfo)
			appErrorf( TEXT("XMesaGLDrv: No suitable visual found.") );

		// Report results.
		INT StencilSize = 0;
		glXGetConfig( XDisplay, VisualInfo, GLX_STENCIL_SIZE, &StencilSize );
		INT BufferSize = 0;
		glXGetConfig( XDisplay, VisualInfo, GLX_BUFFER_SIZE, &BufferSize );
		INT DepthSize = 0;
		glXGetConfig( XDisplay, VisualInfo, GLX_DEPTH_SIZE, &DepthSize );

		debugf( TEXT("Found suitable visual. ID: %i"), VisualInfo->visualid );
		debugf( TEXT("Depth: %i Colormap: %i BitsPerRGB: %i"), VisualInfo->depth, VisualInfo->colormap_size, VisualInfo->bits_per_rgb );
		debugf( TEXT("GLX_STENCIL_SIZE: %i"), StencilSize );
		debugf( TEXT("GLX_BUFFER_SIZE:  %i"), BufferSize );
		debugf( TEXT("GLX_DEPTH_SIZE:   %i"), DepthSize );

		// Create the context.	
		if( ShareLists && AllContexts.Num() )
			Context = glXCreateContext(XDisplay, VisualInfo, AllContexts(0), True );
		else
			Context = glXCreateContext(XDisplay, VisualInfo, NULL, True );
		check(Context);
		MakeCurrent();
		AllContexts.AddItem(Context);

		// Clean up.
		XFree(VisualInfo);
		
		// Get info and extensions.
		debugf( NAME_Init, TEXT("glGetString(GL_VENDOR): %s"),     appFromAnsi((ANSICHAR*)glGetString(GL_VENDOR)) );
		debugf( NAME_Init, TEXT("glGetString(GL_RENDERER): %s"),   appFromAnsi((ANSICHAR*)glGetString(GL_RENDERER)) );
		debugf( NAME_Init, TEXT("glGetString(GL_VERSION): %s"),    appFromAnsi((ANSICHAR*)glGetString(GL_VERSION)) );
		debugf( NAME_Init, TEXT("glGetString(GL_EXTENSIONS): %s"), appFromAnsi((ANSICHAR*)glGetString(GL_EXTENSIONS)) );
		FindProcs( 1 );

		// Set modelview.
		glMatrixMode( GL_MODELVIEW );
		glLoadIdentity();
		FLOAT Matrix[16] =
		{
			+1, +0, +0, +0,
			+0, -1, +0, +0,
			+0, +0, -1, +0,
			+0, +0, +0, +1,
		};
		glMultMatrixf( Matrix );

		// Find section.
		FString Section = US + appFromAnsi((ANSICHAR*)glGetString(GL_VENDOR)) + TEXT("/") + appFromAnsi((ANSICHAR*)glGetString(GL_RENDERER));
		if( !GConfig->GetSectionPrivate( *Section, 0, 1, GPackage ) )
			Section = TEXT("Default");
		debugf( NAME_Init, TEXT("Using OpenGL settings [%s]"), *Section );

		// Get hardware defaults.
		GConfig->GetInt( *Section, TEXT("MinLogTextureSize"), MinLogTextureSize, GPackage );
		GConfig->GetInt( *Section, TEXT("MaxLogTextureSize"), MaxLogTextureSize, GPackage );
		GConfig->GetInt( *Section, TEXT("MaxLogUOverV"     ), MaxLogUOverV,      GPackage );
		GConfig->GetInt( *Section, TEXT("MaxLogVOverU"     ), MaxLogVOverU,      GPackage );
		GConfig->GetInt( *Section, TEXT("UseZTrick"        ), UseZTrick,         GPackage );
		GConfig->GetInt( *Section, TEXT("UseMultiTexture"  ), UseMultiTexture,   GPackage );
		GConfig->GetInt( *Section, TEXT("UsePalette"       ), UsePalette,        GPackage );
		GConfig->GetInt( *Section, TEXT("ShareLists"       ), ShareLists,        GPackage );
		GConfig->GetInt( *Section, TEXT("AlwaysMipmap"     ), AlwaysMipmap,      GPackage );
		GConfig->GetInt( *Section, TEXT("DoPrecache"       ), DoPrecache,        GPackage );
		SupportsLazyTextures = 0; // DoPrecache

		// Verify hardware defaults.
		check(MinLogTextureSize>=0);
		check(MaxLogTextureSize>=0);
		check(MinLogTextureSize<MaxLogTextureSize);
		check(MinLogTextureSize<=MaxLogTextureSize);

		// Validate flags.
		if( !SUPPORTS_GL_ARB_multitexture )
			UseMultiTexture = 0;
		if( !SUPPORTS_GL_EXT_paletted_texture )
			UsePalette = 0;

		// Misc.
		Multipass1X = UseMultiTexture; //!! and 2X blending isn't supported
		ScaleByte   = Multipass1X ? ScaleByteBrighten : ScaleByteNormal;

		// Bind little white RGBA texture to ID 0.
		FColor Data[8*8];
		for( INT i=0; i<ARRAY_COUNT(Data); i++ )
			Data[i] = FColor(255,255,255,255);
		SetNoTexture( 0 );
		for( INT Level=0; 8>>Level; Level++ )
			glTexImage2D( GL_TEXTURE_2D, Level, 4, 8>>Level, 8>>Level, 0, GL_RGBA, GL_UNSIGNED_BYTE, Data );

		// Set permanent state.
		glEnable( GL_DEPTH_TEST );
		glShadeModel( GL_SMOOTH );
		glEnable( GL_TEXTURE_2D );
		glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		glAlphaFunc( GL_GREATER, 0.5 );
		glDisable( GL_ALPHA_TEST );
		glDepthMask( 1 );
		glBlendFunc( GL_ONE, GL_ZERO );
		glEnable( GL_BLEND );
		CurrentPolyFlags = PF_Occlude;

		// Remember fullscreenness.
		WasFullscreen = Fullscreen;
		return 1;

		unguard;
	}
	
	void Exit()
	{
		guard(UXMesaGLRenderDevice::Exit);
		check(NumDevices>0);

		// Shut down RC.
		Flush( 1 );
		if( Context )
			UnsetRes();

		// Shut down this GL context. May fail if window was already destroyed.
		if( Context )
			glXDestroyContext(XDisplay, Context);

		// Shut down global GL.
		if( --NumDevices==0 )
		{
			dlclose( MesaLib );

			// FIXME: Weird parse error.
			//SharedBindMap.~TMap<QWORD,FCachedTexture>();
			//AllContexts.~TArray<GLXContext>();
		}

		unguard;
	}
	
	void ShutdownAfterError()
	{
		guard(UXMesaGLRenderDevice::ShutdownAfterError);
		debugf( NAME_Exit, TEXT("UXMesaGLRenderDevice::ShutdownAfterError") );
		//ChangeDisplaySettings( NULL, 0 );
		unguard;
	}
	
	void Flush( UBOOL AllowPrecache )
	{
		guard(UXMesaGLRenderDevice::Flush);
		TArray<GLuint> Binds;
		for( TMap<QWORD,FCachedTexture>::TIterator It(*BindMap); It; ++It )
			Binds.AddItem( It.Value().Id );
		BindMap->Empty();
		glDeleteTextures( Binds.Num(), (GLuint*)&Binds(0) );
		if( AllowPrecache && DoPrecache && !GIsEditor )
			PrecacheOnFlip = 1;
		unguard;
	}
	
	static QSORT_RETURN CDECL CompareRes( const FPlane* A, const FPlane* B )
	{
		return (QSORT_RETURN) ( (A->X-B->X)!=0.0 ? (A->X-B->X) : (A->Y-B->Y) );
	}
	
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar )
	{
		guard(UXMesaGLRenderDevice::Exec);
		if( ParseCommand(&Cmd,TEXT("GetRes")) )
		{
			TArray<FPlane> Relevant;
			INT i;
			for( i=0; i<Modes.Num(); i++ )
				if( Modes(i).Z==Viewport->ColorBytes*8 )
					if
					(	(Modes(i).X!=320 || Modes(i).Y!=200)
					&&	(Modes(i).X!=640 || Modes(i).Y!=400) )
					Relevant.AddUniqueItem(FPlane(Modes(i).X,Modes(i).Y,0,0));
			appQsort( &Relevant(0), Relevant.Num(), sizeof(FPlane), (QSORT_COMPARE)CompareRes );
			FString Str;
			for( i=0; i<Relevant.Num(); i++ )
				Str += FString::Printf( TEXT("%ix%i "), (INT)Relevant(i).X, (INT)Relevant(i).Y );
			Ar.Log( *Str.LeftChop(1) );
			return 1;
		}
		return 0;
		unguard;
	}
	
	void Lock( FPlane InFlashScale, FPlane InFlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* InHitData, INT* InHitSize )
	{
		guard(UXMesaGLRenderDevice::Lock);
		check(LockCount==0);

		BindCycles = ImageCycles = ComplexCycles = GouraudCycles = TileCycles = 0;
		++LockCount;

		// Make this context current.
		MakeCurrent();

		// Clear the Z buffer if needed.
		if( !UseZTrick || GIsEditor || (RenderLockFlags & LOCKR_ClearScreen) )
		{
			glClearColor( ScreenClear.X, ScreenClear.Y, ScreenClear.Z, ScreenClear.W );
			glClearDepth( 1.0 );
			glDepthRange( 0.0, 1.0 );
			ZTrickFunc = GL_LEQUAL;
			SetBlend( PF_Occlude );
			glClear( GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|((RenderLockFlags&LOCKR_ClearScreen)?GL_COLOR_BUFFER_BIT:0) );
		}
		else if( ZTrickToggle )
		{
			ZTrickToggle = 0;
			glClearDepth( 0.5 );
			glDepthRange( 0.0, 0.5 );
			ZTrickFunc = GL_LEQUAL;
		}
		else
		{
			ZTrickToggle = 1;
			glClearDepth( 0.5 );
			glDepthRange( 1.0, 0.5 );
			ZTrickFunc = GL_GEQUAL;
		}
		glDepthFunc( (GLenum) ZTrickFunc );

		// Remember stuff.
		FlashScale = InFlashScale;
		FlashFog   = InFlashFog;
		HitData    = InHitData;
		HitSize    = InHitSize;
		if( HitData )
		{
			*HitSize = 0;
			if( !GLHitData.Num() )
				GLHitData.Add( 16384 );
			glSelectBuffer( GLHitData.Num(), (GLuint*)&GLHitData(0) );
			glRenderMode( GL_SELECT );
			glInitNames();
		}

		unguard;
	}
	
	void SetSceneNode( FSceneNode* Frame )
	{
		guard(UXMesaGLRenderDevice::SetSceneNode);

		// Precompute stuff.
		Aspect      = Frame->FY/Frame->FX;
		RProjZ      = appTan( Viewport->Actor->FovAngle * PI/360.0 );
		RFX2        = 2.0*RProjZ/Frame->FX;
		RFY2        = 2.0*RProjZ*Aspect/Frame->FY;

		// Set viewport and projection.
		glViewport( Frame->XB, Viewport->SizeY-Frame->Y-Frame->YB, Frame->X, Frame->Y );
		glMatrixMode( GL_PROJECTION );
		glLoadIdentity();
		glFrustum( -RProjZ, +RProjZ, -Aspect*RProjZ, +Aspect*RProjZ, 1.0, 32768.0 );

		// Set clip planes.
		if( HitData )
		{
			FVector N[4];
			N[0] = (FVector((Viewport->HitX-Frame->FX2)*Frame->RProj.Z,0,1)^FVector(0,-1,0)).SafeNormal();
			N[1] = (FVector((Viewport->HitX+Viewport->HitXL-Frame->FX2)*Frame->RProj.Z,0,1)^FVector(0,+1,0)).SafeNormal();
			N[2] = (FVector(0,(Viewport->HitY-Frame->FY2)*Frame->RProj.Z,1)^FVector(+1,0,0)).SafeNormal();
			N[3] = (FVector(0,(Viewport->HitY+Viewport->HitYL-Frame->FY2)*Frame->RProj.Z,1)^FVector(-1,0,0)).SafeNormal();
			for( INT i=0; i<4; i++ )
			{
				DOUBLE D0[4]={N[i].X,N[i].Y,N[i].Z,0};
				glClipPlane( (GLenum) (GL_CLIP_PLANE0+i), D0 );
				glEnable( (GLenum) (GL_CLIP_PLANE0+i) );
			}
		}

		unguard;
	}
	
	void Unlock( UBOOL Blit )
	{
		guard(UXMesaGLRenderDevice::Unlock);

		// Unlock and render.
		check(LockCount==1);
		glFlush();
		if( Blit )
			glXSwapBuffers(XDisplay, *XWindow);
		--LockCount;

		// Hits.
		if( HitData )
		{
			INT Records = glRenderMode( GL_RENDER );
			INT* Ptr = &GLHitData(0);
			DWORD BestDepth = MAXDWORD;
			for( INT i=0; i<Records; i++ )
			{
				INT   NameCount = *Ptr++;
				DWORD MinDepth  = *Ptr++;
				DWORD MaxDepth  = *Ptr++;
				if( MinDepth<=BestDepth )
				{
					BestDepth = MinDepth;
					*HitSize = 0;
					for( INT i=0; i<NameCount; )
					{
						INT Count = Ptr[i++];
						for( INT j=0; j<Count; j+=4 )
							*(INT*)(HitData+*HitSize+j) = Ptr[i++];
						*HitSize += Count;
					}
					check(i==NameCount);
				}
				Ptr += NameCount;
				(void)MaxDepth;
			}
			for( i=0; i<4; i++ )
				glDisable( (GLenum) (GL_CLIP_PLANE0+i) );
		}
		unguard;
	}
	
	void DrawComplexSurface( FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet )
	{
		guard(UXMesaGLRenderDevice::DrawComplexSurface);
		check(Surface.Texture);
		clock(ComplexCycles);
		FLOAT UDot = Facet.MapCoords.XAxis | Facet.MapCoords.Origin;
		FLOAT VDot = Facet.MapCoords.YAxis | Facet.MapCoords.Origin;

		// Modulation chain.
		if( Surface.LightMap && UseMultiTexture )
		{
			// Draw with multitexture.
			SetBlend( Surface.PolyFlags );
			SetTexture( 0, *Surface.LightMap, PF_Modulated, -0.5 );
			glActiveTextureARB( (GLenum) GL_TEXTURE1_ARB );
			SetTexture( 1, *Surface.Texture, Surface.PolyFlags, 0.0 );
			glEnable( GL_TEXTURE_2D );
			glColor4f( TexInfo[0].ColorRenorm.X*TexInfo[1].ColorRenorm.X, TexInfo[0].ColorRenorm.Y*TexInfo[1].ColorRenorm.Y, TexInfo[0].ColorRenorm.Z*TexInfo[1].ColorRenorm.Z, 1 );
			for( FSavedPoly* Poly=Facet.Polys; Poly; Poly=Poly->Next )
			{
				glBegin( GL_TRIANGLE_FAN );
				for( INT i=0; i<Poly->NumPts; i++ )
				{
					FLOAT U = Facet.MapCoords.XAxis | Poly->Pts[i]->Point;
					FLOAT V = Facet.MapCoords.YAxis | Poly->Pts[i]->Point;
					glMultiTexCoord2fARB( (GLenum) GL_TEXTURE0_ARB, (U-UDot-TexInfo[0].UPan)*TexInfo[0].UMult, (V-VDot-TexInfo[0].VPan)*TexInfo[0].VMult );
					glMultiTexCoord2fARB( (GLenum) GL_TEXTURE1_ARB, (U-UDot-TexInfo[1].UPan)*TexInfo[1].UMult, (V-VDot-TexInfo[1].VPan)*TexInfo[1].VMult );
					glVertex3fv( &Poly->Pts[i]->Point.X );
				}
				glEnd();
			}
			glDisable( GL_TEXTURE_2D );
			glActiveTextureARB( (GLenum) GL_TEXTURE0_ARB );
		}
		else
		{
			// Draw texture.
			SetBlend( Surface.PolyFlags );
			SetTexture( 0, *Surface.Texture, Surface.PolyFlags, 0.0 );
			FPlane Modulate = TexInfo[0].ColorNorm;
			glColor4fv( Surface.LightMap ? &One4.X : &Modulate.X );
			for( FSavedPoly* Poly=Facet.Polys; Poly; Poly=Poly->Next )
			{
				glBegin( GL_TRIANGLE_FAN );
				for( INT i=0; i<Poly->NumPts; i++ )
				{
					FLOAT U = Facet.MapCoords.XAxis | Poly->Pts[i]->Point;
					FLOAT V = Facet.MapCoords.YAxis | Poly->Pts[i]->Point;
					glTexCoord2f( (U-UDot-TexInfo[0].UPan)*TexInfo[0].UMult, (V-VDot-TexInfo[0].VPan)*TexInfo[0].VMult );
					glVertex3fv( &Poly->Pts[i]->Point.X );
				}
				glEnd();
			}

			// Draw lightmap.
			if( Surface.LightMap )
			{
				SetBlend( PF_Modulated );
				if( Surface.PolyFlags & PF_Masked )
					glDepthFunc( GL_EQUAL );
				SetTexture( 0, *Surface.LightMap, 0, -0.5 );
				Modulate.X *= TexInfo[0].ColorNorm.X;
				Modulate.Y *= TexInfo[0].ColorNorm.Y;
				Modulate.Z *= TexInfo[0].ColorNorm.Z;
				glColor4fv( &Modulate.X );
				for( FSavedPoly* Poly=Facet.Polys; Poly; Poly=Poly->Next )
				{
					glBegin( GL_TRIANGLE_FAN );
					for( INT i=0; i<Poly->NumPts; i++ )
					{
						FLOAT U = Facet.MapCoords.XAxis | Poly->Pts[i]->Point;
						FLOAT V = Facet.MapCoords.YAxis | Poly->Pts[i]->Point;
						glTexCoord2f( (U-UDot-TexInfo[0].UPan)*TexInfo[0].UMult, (V-VDot-TexInfo[0].VPan)*TexInfo[0].VMult );
						glVertex3fv ( &Poly->Pts[i]->Point.X );
					}
					glEnd();
				}
				if( Surface.PolyFlags & PF_Masked )
					glDepthFunc( (GLenum) ZTrickFunc );
			}
		}

		// Draw fog.
		if( Surface.FogMap )
		{
			SetBlend( PF_Highlighted );
			if( Surface.PolyFlags & PF_Masked )
				glDepthFunc( GL_EQUAL );
			SetTexture( 0, *Surface.FogMap, 0, -0.5 );
			glColor4fv( &TexInfo[0].ColorNorm.X );
			for( FSavedPoly* Poly=Facet.Polys; Poly; Poly=Poly->Next )
			{
				glBegin( GL_TRIANGLE_FAN );
				for( INT i=0; i<Poly->NumPts; i++ )
				{
					FLOAT U = Facet.MapCoords.XAxis | Poly->Pts[i]->Point;
					FLOAT V = Facet.MapCoords.YAxis | Poly->Pts[i]->Point;
					glTexCoord2f( (U-UDot-TexInfo[0].UPan)*TexInfo[0].UMult, (V-VDot-TexInfo[0].VPan)*TexInfo[0].VMult );
					glVertex3fv ( &Poly->Pts[i]->Point.X );
				}
				glEnd();
			}
			if( Surface.PolyFlags & PF_Masked )
				glDepthFunc( (GLenum) ZTrickFunc );
		}

		// UnrealEd selection.
		if( (Surface.PolyFlags & PF_Selected) && GIsEditor )
		{
			SetNoTexture( 0 );
			SetBlend( PF_Highlighted );
			glBegin( GL_TRIANGLE_FAN );
			glColor4f( 0.0, 0.0, 0.5, 0.5 );
			for( FSavedPoly* Poly=Facet.Polys; Poly; Poly=Poly->Next )
			{
				glBegin( GL_TRIANGLE_FAN );
				for( INT i=0; i<Poly->NumPts; i++ )
					glVertex3fv ( &Poly->Pts[i]->Point.X );
				glEnd();
			}
			glEnd();
		}
		unclock(ComplexCycles);
		unguard;
	}
	
	void DrawGouraudPolygon( FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span )
	{
		guard(UXMesaGLRenderDevice::DrawGouraudPolygon);
		clock(GouraudCycles);

		SetBlend( PolyFlags );
		SetTexture( 0, Info, PolyFlags, 0 );
		glBegin( GL_TRIANGLE_FAN  );
		if( PolyFlags & PF_Modulated )
			glColor4f( TexInfo[0].ColorNorm.X, TexInfo[0].ColorNorm.Y, TexInfo[0].ColorNorm.Z, 1 );
		for( INT i=0; i<NumPts; i++ )
		{
			FTransTexture* P = Pts[i];
			if( !(PolyFlags & PF_Modulated) )
				glColor4f( P->Light.X*TexInfo[0].ColorNorm.X, P->Light.Y*TexInfo[0].ColorNorm.Y, P->Light.Z*TexInfo[0].ColorNorm.Z, 1 );
			glTexCoord2f( P->U*TexInfo[0].UMult, P->V*TexInfo[0].VMult );
			glVertex3fv( &P->Point.X );
		}
		glEnd();
		if( (PolyFlags & (PF_RenderFog|PF_Translucent|PF_Modulated))==PF_RenderFog )
		{
			SetNoTexture( 0 );
			SetBlend( PF_Highlighted );
			glBegin( GL_TRIANGLE_FAN );
			for( INT i=0; i<NumPts; i++ )
			{
				FTransTexture* P = Pts[i];
				glColor4f( P->Fog.X, P->Fog.Y, P->Fog.Z, P->Fog.W );
				glVertex3fv( &P->Point.X );
			}
			glEnd();
		}
		unclock(GouraudCycles);
		unguard;
	}
	
	void DrawTile( FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags )
	{
		guard(UXMesaGLRenderDevice::DrawTile);
		clock(TileCycles);
		SetBlend( PolyFlags );
		SetTexture( 0, Info, PolyFlags, 0 );
		Color.X *= TexInfo[0].ColorNorm.X;
		Color.Y *= TexInfo[0].ColorNorm.Y;
		Color.Z *= TexInfo[0].ColorNorm.Z;
		Color.W  = 1;
		glColor4fv( &Color.X );
		glBegin( GL_TRIANGLE_FAN );
		glTexCoord2f( (U   )*TexInfo[0].UMult, (V   )*TexInfo[0].VMult ); glVertex3f( RFX2*Z*(X   -Frame->FX2), RFY2*Z*(Y   -Frame->FY2), Z );
		glTexCoord2f( (U+UL)*TexInfo[0].UMult, (V   )*TexInfo[0].VMult ); glVertex3f( RFX2*Z*(X+XL-Frame->FX2), RFY2*Z*(Y   -Frame->FY2), Z );
		glTexCoord2f( (U+UL)*TexInfo[0].UMult, (V+VL)*TexInfo[0].VMult ); glVertex3f( RFX2*Z*(X+XL-Frame->FX2), RFY2*Z*(Y+YL-Frame->FY2), Z );
		glTexCoord2f( (U   )*TexInfo[0].UMult, (V+VL)*TexInfo[0].VMult ); glVertex3f( RFX2*Z*(X   -Frame->FX2), RFY2*Z*(Y+YL-Frame->FY2), Z );
		glEnd();
		unclock(TileCycles);
		unguard;
	}
	
	void Draw2DLine( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2 )
	{
		guard(UXMesaGLRenderDevice::Draw2DLine);
		SetNoTexture( 0 );
		SetBlend( PF_Highlighted );
		glColor3fv( &Color.X );
		glBegin( GL_LINES );
		glVertex3f( RFX2*P1.Z*(P1.X-Frame->FX2), RFY2*P1.Z*(P1.Y-Frame->FY2), P1.Z );
		glVertex3f( RFX2*P2.Z*(P2.X-Frame->FX2), RFY2*P2.Z*(P2.Y-Frame->FY2), P2.Z );
		glEnd();
		unguard;
	}
	
	void Draw3DLine( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2 )
	{
		guard(UXMesaGLRenderDevice::Draw3DLine);
		P1 = P1.TransformPointBy( Frame->Coords );
		P2 = P2.TransformPointBy( Frame->Coords );
		if( Frame->Viewport->IsOrtho() )
		{
			// Zoom.
			P1.X = (P1.X) / Frame->Zoom + Frame->FX2;
			P1.Y = (P1.Y) / Frame->Zoom + Frame->FY2;
			P2.X = (P2.X) / Frame->Zoom + Frame->FX2;
			P2.Y = (P2.Y) / Frame->Zoom + Frame->FY2;
			P1.Z = P2.Z = 1;

			// See if points form a line parallel to our line of sight (i.e. line appears as a dot).
			if( Abs(P2.X-P1.X)+Abs(P2.Y-P1.Y)>=0.2 )
				Draw2DLine( Frame, Color, LineFlags, P1, P2 );
			else if( Frame->Viewport->Actor->OrthoZoom < ORTHO_LOW_DETAIL )
				Draw2DPoint( Frame, Color, LINE_None, P1.X-1, P1.Y-1, P1.X+1, P1.Y+1, P1.Z );
		}
		else
		{
			SetNoTexture( 0 );
			SetBlend( PF_Highlighted );
			glColor3fv( &Color.X );
			glBegin( GL_LINES );
			glVertex3fv( &P1.X );
			glVertex3fv( &P2.X );
			glEnd();
		}
		unguard;
	}
	
	void Draw2DPoint( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z )
	{
		guard(UXMesaGLRenderDevice::Draw2DPoint);
		SetBlend( PF_Highlighted );
		SetNoTexture( 0 );
		glColor4fv( &Color.X );
		glBegin( GL_TRIANGLE_FAN );
		glVertex3f( RFX2*Z*(X1-Frame->FX2), RFY2*Z*(Y1-Frame->FY2), Z );
		glVertex3f( RFX2*Z*(X2-Frame->FX2), RFY2*Z*(Y1-Frame->FY2), Z );
		glVertex3f( RFX2*Z*(X2-Frame->FX2), RFY2*Z*(Y2-Frame->FY2), Z );
		glVertex3f( RFX2*Z*(X1-Frame->FX2), RFY2*Z*(Y2-Frame->FY2), Z );
		glEnd();
		unguard;
	}
	
	void ClearZ( FSceneNode* Frame )
	{
		guard(UXMesaGLRenderDevice::ClearZ);
		SetBlend( PF_Occlude );
		glClear( GL_DEPTH_BUFFER_BIT );
		unguard;
	}
	
	void PushHit( const BYTE* Data, INT Count )
	{
		guard(UXMesaGLRenderDevice::PushHit);
		glPushName( Count );
		for( INT i=0; i<Count; i+=4 )
			glPushName( *(INT*)(Data+i) );
		unguard;
	}
	
	void PopHit( INT Count, UBOOL bForce )
	{
		guard(UXMesaGLRenderDevice::PopHit);
		glPopName();
		for( INT i=0; i<Count; i+=4 )
			glPopName();
		//!!implement bforce
		unguard;
	}
	
	void GetStats( TCHAR* Result )
	{
		guard(UXMesaGLRenderDevice::GetStats);
		appSprintf
		(
			Result,
			TEXT("OpenGL stats: Bind=%04.1f Image=%04.1f Complex=%04.1f Gouraud=%04.1f Tile=%04.1f"),
			GSecondsPerCycle*1000 * BindCycles,
			GSecondsPerCycle*1000 * ImageCycles,
			GSecondsPerCycle*1000 * ComplexCycles,
			GSecondsPerCycle*1000 * GouraudCycles,
			GSecondsPerCycle*1000 * TileCycles
		);
		unguard;
	}
	
	void ReadPixels( FColor* Pixels )
	{
		guard(UXMesaGLRenderDevice::ReadPixels);
		glReadPixels( 0, 0, Viewport->SizeX, Viewport->SizeY, GL_RGBA, GL_UNSIGNED_BYTE, Pixels );
		for( INT i=0; i<Viewport->SizeY/2; i++ )
		{
			for( INT j=0; j<Viewport->SizeX; j++ )
			{
				Exchange( Pixels[j+i*Viewport->SizeX].R, Pixels[j+(Viewport->SizeY-1-i)*Viewport->SizeX].B );
				Exchange( Pixels[j+i*Viewport->SizeX].G, Pixels[j+(Viewport->SizeY-1-i)*Viewport->SizeX].G );
				Exchange( Pixels[j+i*Viewport->SizeX].B, Pixels[j+(Viewport->SizeY-1-i)*Viewport->SizeX].R );
			}
		}
  		unguard;
	}
	
	void EndFlash()
	{
		if( FlashScale!=FPlane(.5,.5,.5,0) || FlashFog!=FPlane(0,0,0,0) )
		{
			SetBlend( PF_Highlighted );
			SetNoTexture( 0 );
			glColor4f( FlashFog.X, FlashFog.Y, FlashFog.Z, 1.0-Min(FlashScale.X*2.f,1.f) );
			FLOAT RFX2 = 2.0*RProjZ       /Viewport->SizeX;
			FLOAT RFY2 = 2.0*RProjZ*Aspect/Viewport->SizeY;
			glBegin( GL_TRIANGLE_FAN );
				glVertex3f( RFX2*(-Viewport->SizeX/2.0), RFY2*(-Viewport->SizeY/2.0), 1.0 );
				glVertex3f( RFX2*(+Viewport->SizeX/2.0), RFY2*(-Viewport->SizeY/2.0), 1.0 );
				glVertex3f( RFX2*(+Viewport->SizeX/2.0), RFY2*(+Viewport->SizeY/2.0), 1.0 );
				glVertex3f( RFX2*(-Viewport->SizeX/2.0), RFY2*(+Viewport->SizeY/2.0), 1.0 );
			glEnd();
		}
	}
	
	void PrecacheTexture( FTextureInfo& Info, DWORD PolyFlags )
	{
		guard(UXMesaGLRenderDevice::PrecacheTexture);
		SetTexture( 0, Info, PolyFlags, 0.0 );
		unguard;
	}
};
IMPLEMENT_CLASS(UXMesaGLRenderDevice);

// Static variables.
INT					UXMesaGLRenderDevice::NumDevices 		= 0;
INT					UXMesaGLRenderDevice::LockCount     	= 0;
GLXContext			UXMesaGLRenderDevice::CurrentContext	= NULL;
void*				UXMesaGLRenderDevice::MesaLib			= NULL;
TArray<GLXContext>	UXMesaGLRenderDevice::AllContexts;
TMap<QWORD, UXMesaGLRenderDevice::FCachedTexture> UXMesaGLRenderDevice::SharedBindMap;

// OpenGL function pointers.
#define GL_EXT(name) UBOOL UXMesaGLRenderDevice::SUPPORTS##name=0;
#define GL_PROC(ext,ret,func,parms) ret (STDCALL *UXMesaGLRenderDevice::func)parms;
#include "XMesaGLFuncs.h"
#undef GL_EXT
#undef GL_PROC

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
