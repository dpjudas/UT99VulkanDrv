/*=============================================================================
	UnD3D.cpp: Unreal Direct3D6 support.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by independent contractor who wishes to remanin anonymous.
		* Taken over by Tim Sweeney.
=============================================================================*/

// Includes.
#include "D3DDrv.h"
#if 0

// Globals.
DWORD AlphaPalette[256];
HRESULT h;
DWORD Timing;
typedef HRESULT (WINAPI *DD_CREATE_FUNC)(GUID* lpGUID, LPDIRECTDRAW* lplpDD, IUnknown* pUnkOuter);
typedef HRESULT (WINAPI *DD_ENUM_FUNC  )(LPDDENUMCALLBACKA lpCallback,void* lpContext);
#define LoadLibraryX(a) TCHAR_CALL_OS(LoadLibraryW(a),LoadLibraryA(TCHAR_TO_ANSI(a)))
#define SAFETRY(cmd) {try{cmd;}catch(...){debugf(TEXT("Exception in ") TEXT(#cmd));}}

/*-----------------------------------------------------------------------------
	UD3DRenderDevice definition.
-----------------------------------------------------------------------------*/

//
// Direct3D rendering device.
//
class DLL_EXPORT UD3DRenderDevice : public URenderDevice
{
    DECLARE_CLASS(UD3DRenderDevice,URenderDevice,CLASS_Config)

	// Defines.
	#define PYR(n)         ((n)*((n+1))/2)
	#define UNREALVFMT     (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX2)
	#define MAX_TMU        8
	#define UNREAL_NOZBIAS -15 /* was 0  */
	#define UNREAL_ZBIAS   0   /* was 15 */
	#define DECLARE_INITED(typ,var) typ var; appMemzero(&var,sizeof(var)); var.dwSize=sizeof(var);
	struct FTexInfo;
	struct FPixFormat;

	// Texture information classes.
	struct FTexPool
	{
		IDirectDrawSurface4* SystemSurface;
		IDirect3DTexture2*   SystemTexture;
		FTexInfo*			 First;
		FPixFormat*			 PixelFormat;
		INT					 USize, VSize;
		INT					 TexCount;
		INT					 UsedCount;
		INT					 DesiredSwapCopies;
		FTexPool()
		: SystemSurface(NULL), SystemTexture(NULL), First(NULL), TexCount(0), UsedCount(0)
		{}
	};
	struct FPixFormat
	{
		// Pixel format info.
		DDPIXELFORMAT		pf;				// Pixel format from Direct3D.
		FPixFormat*			Next;			// Next in linked list of all compatible pixel formats.
		const TCHAR*		Desc;			// Stat: Human readable name for stats.
		INT					BitsPerPixel;	// Total bits per pixel.
		DWORD				MaxWidth;
		DWORD				MaxHeight;

		// Multi-frame stats.
		INT					Binned;			// Stat: How many textures of this format are available in bins.
		INT					BinnedRAM;		// Stat: How much RAM is used by total textures of this format in the cache.

		// Per-frame stats.
		INT					Active;			// Stat: How many textures of this format are active.
		INT					ActiveRAM;		// Stat: How much RAM is used by active textures of this format per frame.
		INT					Sets;			// Stat: Number of SetTexture was called this frame on textures of this format.
		INT					Uploads;		// Stat: Number of texture Blts this frame.
		INT					UploadCycles;	// Stat: Cycles spent Blting.
		TMap<INT,FTexPool*>	PoolList;		// List of all pools using this pixel format.
		void Init()
		{
			appMemzero( &pf, sizeof(pf) );
			Next = NULL;
			BitsPerPixel = Binned = BinnedRAM = 0;
		}
		void InitStats()
		{
			Sets = Uploads = UploadCycles = Active = ActiveRAM = 0;
		}
	};
	struct FTexFiller
	{
		FPixFormat* PixelFormat;
		FTexFiller( FPixFormat* InPixelFormat )
		: PixelFormat( InPixelFormat )
		{}
		virtual void BeginUpload( UD3DRenderDevice* Device, FTexInfo* Tex, const FTextureInfo& Info, DWORD PolyFlags ) {}
		virtual void UploadMipmap( UD3DRenderDevice* Device, FTexInfo* Tex, FRainbowPtr Dest, INT Stride, const FTextureInfo& Info, INT MipIndex, DWORD PolyFlags ) {}
		virtual void EndUpload( UD3DRenderDevice* Device, FTexInfo* Tex, const FTextureInfo& Info, DWORD PolyFlags ) {}
		void BuildAlphaPalette( FColor* Pal, DWORD FracA, DWORD MaskA, DWORD FracR, DWORD MaskR, DWORD FracG, DWORD MaskG, DWORD FracB, DWORD MaskB )
		{
			DWORD* Dest = AlphaPalette;
			for( FColor *End=Pal+NUM_PAL_COLORS; Pal<End; Pal++ )
				*Dest++ = (((FracA*Pal->A)&MaskA) | ((FracR*Pal->R)&MaskR) | ((FracG*Pal->G)&MaskG) | ((FracB*Pal->B)&MaskB))>>16;
		}
	};
	struct FTexInfo
	{
		// Info persistent for this cache slot.
		QWORD					CacheID;
		FTexInfo*				PoolNext;
		INT						FrameCounter;
		INT						SubCounter;
		INT						Unloaded;
		IDirectDrawSurface4*	Surface;
		IDirect3DTexture2*		Texture;
		FTexPool*				Pool;

		// Info swapped in and out with textures.
		FTexInfo*				HashNext;
		FTexInfo**				HashPrevLink;
		FTexFiller*				Filler;
		INT						FirstMip;
		INT						UIndex, VIndex;
		FLOAT					UScale, VScale;
		FColor					MaxColor;
	};
	struct FTexFillerDXT1 : public FTexFiller
	{
		FTexFillerDXT1( UD3DRenderDevice* InOuter ) : FTexFiller(&InOuter->FormatDXT1) {}
		void UploadMipmap( UD3DRenderDevice* Device, FTexInfo* ti, FRainbowPtr Dest, INT Stride, const FTextureInfo& Info, INT MipIndex, DWORD PolyFlags )
		{
			guard(FTexFillerDXT1::UploadMipmap);
			appMemcpy( Dest.PtrVOID, Info.Mips[MipIndex]->DataPtr, (Info.Mips[MipIndex]->USize * Info.Mips[MipIndex]->VSize)/2 );
			unguard;
		}
	};
	struct FTexFillerP8_P8 : public FTexFiller
	{
		FTexFillerP8_P8( UD3DRenderDevice* InOuter )
		: FTexFiller(&InOuter->FormatP8)
		{}
		void UploadMipmap( UD3DRenderDevice* Device, FTexInfo* Tex, FRainbowPtr Dest, INT Stride, const FTextureInfo& Info, INT MipIndex )
		{
			guard(FTexFillerP8_P8::UploadMipmap);
			FRainbowPtr Src  = Info.Mips[MipIndex]->DataPtr;
			for( INT j=Info.Mips[MipIndex]->VSize-1; j>=0; j-- )
			{
				for( INT k=Info.Mips[MipIndex]->USize-1; k>=0; k-- )
					*Dest.PtrBYTE++ = *Src.PtrBYTE++;
				Dest.PtrBYTE += Stride - Info.Mips[MipIndex]->USize;
			}
			unguard;
		}
	};
	struct FTexFiller8888_RGBA7 : public FTexFiller
	{
		FTexFiller8888_RGBA7( UD3DRenderDevice* InOuter ) : FTexFiller(&InOuter->Format8888) {}
		void UploadMipmap( UD3DRenderDevice* Device, FTexInfo* ti, FRainbowPtr Dest, INT Stride, const FTextureInfo& Info, INT MipIndex, DWORD PolyFlags )
		{
			guard(FTexFiller8888_RGBA7::UploadMipmap);
			DWORD* RPtr = Device->XScale + PYR(Info.MaxColor->R/2);
			DWORD* GPtr = Device->XScale + PYR(Info.MaxColor->G/2);
			DWORD* BPtr = Device->XScale + PYR(Info.MaxColor->B/2);
			FRainbowPtr Src  = Info.Mips[MipIndex]->DataPtr;
			for( INT i=Info.VClamp-1; i>=0; i-- )
			{
				for( INT j=Info.UClamp-1; j>=0; j--,Src.PtrDWORD++ )
					*Dest.PtrDWORD++ = RGBA_MAKE(RPtr[Src.PtrBYTE[2]],GPtr[Src.PtrBYTE[1]], BPtr[Src.PtrBYTE[0]], (DWORD)(Src.PtrBYTE[3]*2));
				Dest.PtrBYTE  += Stride     - Info.UClamp*4;
				Src .PtrDWORD += Info.USize - Info.UClamp;
			}
			unguard;
		}
	};
	struct FTexFiller1555_RGBA7 : public FTexFiller
	{
		FTexFiller1555_RGBA7( UD3DRenderDevice* InOuter ) : FTexFiller(&InOuter->Format1555) {}
		void UploadMipmap( UD3DRenderDevice* Device, FTexInfo* ti, FRainbowPtr Dest, INT Stride, const FTextureInfo& Info, INT MipIndex, DWORD PolyFlags )
		{
			guard(FTexFiller1555_RGBA7::UploadMipmap);
			//debugf(TEXT("%ix%i -> %i"),Info.Mips[MipIndex]->USize,Info.Mips[MipIndex]->VSize,Stride/2);
			_WORD*      RPtr     = Device->RScale + PYR(Info.MaxColor->R/2);
			_WORD*      GPtr     = Device->GScale + PYR(Info.MaxColor->G/2);
			_WORD*      BPtr     = Device->BScale + PYR(Info.MaxColor->B/2);
			FRainbowPtr Src      = Info.Mips[MipIndex]->DataPtr;
			if( ti->UIndex==0 )
				for( INT v=0; v<Info.VClamp; v++,Dest.PtrBYTE+=Stride-Info.UClamp*2,Src .PtrDWORD+=Info.USize-Info.UClamp )
					for( INT u=0; u<Info.UClamp; u++,Src.PtrDWORD++ )
						*Dest.PtrWORD++ = BPtr[Src.PtrBYTE[0]] + GPtr[Src.PtrBYTE[1]] + RPtr[Src.PtrBYTE[2]];
			else
				for( INT v=0; v<Info.VClamp; v++,Src.PtrDWORD+=Info.USize-Info.UClamp,Dest.PtrBYTE+=2-Stride*Info.UClamp )
					for( INT u=0; u<Info.UClamp; u++,Src.PtrDWORD++,Dest.PtrBYTE+=Stride )
						*Dest.PtrWORD = BPtr[Src.PtrBYTE[0]] + GPtr[Src.PtrBYTE[1]] + RPtr[Src.PtrBYTE[2]];
			unguard;
		}
	};
	struct FTexFiller1555_P8 : public FTexFiller
	{
		FTexFiller1555_P8( UD3DRenderDevice* InOuter ) : FTexFiller(&InOuter->Format1555) {}
		void UploadMipmap( UD3DRenderDevice* Device, FTexInfo* ti, FRainbowPtr Dest, INT Stride, const FTextureInfo& Info, INT MipIndex, DWORD PolyFlags )
		{
			guard(FTexFiller16_P8::UploadMipmap);
			INT  USize      = Info.Mips[MipIndex]->USize;
			INT  VSize      = Info.Mips[MipIndex]->VSize;
			FRainbowPtr Src = Info.Mips[MipIndex]->DataPtr;
			if( ti->UIndex==0 )
				for( INT j=VSize-1; j>=0; j--,Dest.PtrBYTE+=Stride-Info.Mips[MipIndex]->USize*2 )
					for( INT k=USize-1; k>=0; k-- )
						*Dest.PtrWORD++ = AlphaPalette[*Src.PtrBYTE++];
			else
				for( INT j=VSize-1; j>=0; j--,Dest.PtrBYTE+=2-Stride*USize )
					for( INT k=USize-1; k>=0; k--,Dest.PtrBYTE+=Stride )
						*Dest.PtrWORD = AlphaPalette[*Src.PtrBYTE++];
			unguard;
		}
		void BeginUpload( UD3DRenderDevice* Device, FTexInfo* ti, const FTextureInfo& Info, DWORD PolyFlags )
		{
			guard(FTexFiller1555_P8::BeginUpload);
			BuildAlphaPalette
			(
				Info.Palette,
				0x1000000, 0x80000000,
				(0x07fffffff/Max<INT>(ti->MaxColor.R,1)), 0x7C000000,
				(0x003ffffff/Max<INT>(ti->MaxColor.G,1)), 0x03E00000,
				(0x0001fffff/Max<INT>(ti->MaxColor.B,1)), 0x001F0000
			);
			if( PolyFlags & PF_Masked )
				AlphaPalette[0] = 0;
			unguard;
		}
	};

	// Structs.
	struct FUnrealVertex
	{
		D3DVALUE			dvSX;
		D3DVALUE			dvSY;
		D3DVALUE			dvSZ;
		D3DVALUE			dvRHW;
		D3DCOLOR			dcColor;
		D3DVALUE			dvTU[2];
		D3DVALUE			dvTU2[2];
	};
	struct FD3DVertex
	{
		D3DVALUE			dvSX;
		D3DVALUE			dvSY;
		D3DVALUE			dvSZ;
		D3DVALUE			dvRHW;
		D3DCOLOR			dcColor;
		D3DCOLOR			dcSpecular;
		D3DVALUE			dvTU[2];
	};
	struct FD3DStats
	{
		INT					SurfTime, PolyTime, TileTime, ThrashTime;
		INT					ThrashX, ThrashY;
		DWORD				Surfs, Polys, Tiles, Thrashes;
	};
	struct FDeviceInfo
	{
		GUID				Guid;
		FString				Description;
		FString				Name;
		FDeviceInfo( GUID InGuid, const TCHAR* InDescription, const TCHAR* InName )
		: Guid(InGuid), Description(InDescription), Name(InName)
		{}
	};
	struct FTexStageInfo
	{
		QWORD				TextureCacheID;
		FPlane				MaxColor;
		INT					UIndex, VIndex;
		FLOAT				UScale, VScale;
		UBOOL				UseMips;
		FTexStageInfo()
		{}
		void Flush()
		{
			TextureCacheID  = 0;
			MaxColor        = FPlane(1,1,1,1);
		}
	};

	// Saved viewport info.
    INT						ViewportX;
    INT						ViewportY;
    HWND					ViewporthWnd;
    DWORD					ViewportColorBits;
    UBOOL					ViewportFullscreen;

	// Pixel formats from D3D.
    FPixFormat				FormatDXT1;
    FPixFormat				FormatP8;
    FPixFormat				Format8888;
    FPixFormat				Format1555;
	FPixFormat*				FirstPixelFormat;

	// Fillers.
	FTexFillerDXT1			FillerDXT1;
	FTexFiller8888_RGBA7	Filler8888_RGBA7;
	FTexFiller8888_RGBA7	Filler8888_P8;
	FTexFiller1555_RGBA7	Filler1555_RGBA7;
	FTexFiller1555_P8		Filler1555_P8;

	// From D3D.
    D3DDEVICEDESC			DeviceDesc;
	DDCAPS					DirectDrawCaps;
	DDSCAPS2				NextMipmapCaps;
	D3DCOLOR				WhiteColor;
	DDDEVICEIDENTIFIER		DeviceIdentifier;
	_WORD					wProduct, wVersion, wSubVersion, wBuild;
	TArray<DDPIXELFORMAT>	PixelFormats;
	TArray<DDPIXELFORMAT>	ZFormats;
	TArray<DDSURFACEDESC2>	DisplayModes;
	TArray<FDeviceInfo>		DirectDraws;

	// Direct3D init sequence objects and variables.
	UBOOL					DidCoInitialize;
    IDirectDraw4*			DirectDraw;
    IDirect3D3*				Direct3D;
	UBOOL					DidSetCooperativeLevel;
	UBOOL					DidSetDisplayMode;
	IDirectDrawSurface4*	PrimarySurface;
	IDirectDrawSurface4*	BackSurface;
	IDirectDrawClipper*		Clipper;
	IDirectDrawSurface4*	ZSurface;
    IDirect3DDevice3*		Direct3DDevice;
    IDirect3DViewport3*		Direct3DViewport;
    IDirectDrawGammaControl*GammaControl;

	// Direct3D-specific render options.
    BITFIELD				UseMipmapping;
    BITFIELD				UseTrilinear;
    BITFIELD				UseMultitexture;
    BITFIELD				UsePalettes;
    BITFIELD				UseGammaCorrection;
	BITFIELD				Use3dfx;
	BITFIELD				UseD3DSoftwareRenderer;
	BITFIELD				UseTripleBuffering;
	BITFIELD				UseVSync;
	BITFIELD				UseVertexSpecular;
	BITFIELD				UseAlphaPalettes;
	BITFIELD				UsePrecache;
	INT						MaxResWidth, MaxResHeight;
	INT						dwVendorId, dwDeviceId;

	// Info used while rendering a frame.
    D3DMATRIX				ProjectionMatrix;
    FPlane					FlashScale;
    FPlane					FlashFog;
    DWORD					LockFlags;
	DWORD					CurrentPolyFlags;
    FD3DStats				Stats;
	FTexStageInfo			Stages[MAX_TMU];
    FD3DVertex				Verts[1024];//danger of overflow, bad design
    FUnrealVertex			MVerts[1024];//danger of overflow, bad design
    FLOAT					MasterU[512], MasterV[512];
    _WORD					RScale[PYR(128)];
    _WORD					GScale[PYR(128)];
    _WORD					BScale[PYR(128)];
	DWORD					XScale[PYR(128)];
	UBOOL					VideoMemoryFull;
	INT						FrameCounter;
	INT						SubCounter;
	INT						PreloadMemory;
	INT						TotalMemory;
	INT						PrecacheCycle;
	FTexInfo*				CachedTextures[4096];

    // UObject interface.
    void StaticConstructor()
	{
		guard(UD3DRenderDevice::StaticConstructor);

		new(GetClass(),TEXT("UseMipmapping"),       RF_Public)UBoolProperty ( CPP_PROPERTY(UseMipmapping       ), TEXT("Options"), CPF_Config );
		new(GetClass(),TEXT("UseTrilinear"),        RF_Public)UBoolProperty ( CPP_PROPERTY(UseTrilinear        ), TEXT("Options"), CPF_Config );
		new(GetClass(),TEXT("UseMultitexture"),     RF_Public)UBoolProperty ( CPP_PROPERTY(UseMultitexture     ), TEXT("Options"), CPF_Config );
		new(GetClass(),TEXT("UsePalettes"),         RF_Public)UBoolProperty ( CPP_PROPERTY(UsePalettes         ), TEXT("Options"), CPF_Config );
		new(GetClass(),TEXT("UseGammaCorrection"),  RF_Public)UBoolProperty ( CPP_PROPERTY(UseGammaCorrection  ), TEXT("Options"), CPF_Config );
		new(GetClass(),TEXT("Use3dfx"),             RF_Public)UBoolProperty ( CPP_PROPERTY(Use3dfx             ), TEXT("Options"), CPF_Config );
		new(GetClass(),TEXT("UseTripleBuffering"),  RF_Public)UBoolProperty ( CPP_PROPERTY(UseTripleBuffering  ), TEXT("Options"), CPF_Config );
		new(GetClass(),TEXT("UseVSync"),            RF_Public)UBoolProperty ( CPP_PROPERTY(UseVSync            ), TEXT("Options"), CPF_Config );
		new(GetClass(),TEXT("UsePrecache"),         RF_Public)UBoolProperty ( CPP_PROPERTY(UsePrecache         ), TEXT("Options"), CPF_Config );
		new(GetClass(),TEXT("dwVendorId"),          RF_Public)UIntProperty  ( CPP_PROPERTY(dwVendorId          ), TEXT("Options"), CPF_Config );
		new(GetClass(),TEXT("dwDeviceId"),          RF_Public)UIntProperty  ( CPP_PROPERTY(dwDeviceId          ), TEXT("Options"), CPF_Config );

		UseVertexSpecular		= 1;
		UseAlphaPalettes		= 1;
		SpanBased				= 0;
		SupportsFogMaps			= 1;
		SupportsDistanceFog		= 0;
		MaxResWidth				= MAXINT;
		MaxResHeight			= MAXINT;

		unguard;
	}

    // URenderDevice interface.
	UD3DRenderDevice()
	:	Filler1555_RGBA7(this)
	,	Filler8888_RGBA7(this)
	,	FillerDXT1		(this)
	,	Filler1555_P8	(this)
	,	Filler8888_P8	(this)
	{}
	UBOOL Init( UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen )
	{
		guard(UD3DRenderDevice::Init);
		Viewport = InViewport;

		// Static info.
		appMemzero( &NextMipmapCaps, sizeof(NextMipmapCaps) );
		NextMipmapCaps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_MIPMAP;
		{for( INT i=0; i<ARRAY_COUNT(Verts); i++ )
			Verts[i].dcColor=0xffffffff;}
		{for( INT i=0; i<ARRAY_COUNT(MVerts); i++ )
			MVerts[i].dcColor=0xffffffff;}
		WhiteColor = 0xffffffff;

		// Flags.
		SupportsLazyTextures	= 1;
		PrefersDeferredLoad		= UsePrecache;

		// Init pyramic-compressed scaling tables.
		for( INT A=0; A<128; A++ )
		{
			for( INT B=0; B<=A; B++ )
			{
				INT M=Max(A,1);
				RScale[PYR(A)+B] = Min(((B*0x08000)/M),0x7C00) & 0xf800;
				GScale[PYR(A)+B] = Min(((B*0x00400)/M),0x03e0) & 0x07e0;
				BScale[PYR(A)+B] = Min(((B*0x00020)/M),0x001f) & 0x001f;
				XScale[PYR(A)+B] = Min(((B*0x00100)/M),0x00ff) & 0x00ff;
			}
		}
		return SetRes( NewX, NewY, NewColorBytes, Fullscreen );
		unguard;
	}
	void Exit()
	{
		guard(UD3DRenderDevice::Exit);
		UnSetRes(NULL,0);   
		unguard;
	}
	void ShutdownAfterError()
	{
		guard(UD3DRenderDevice::ShutdownAfterError);
		debugf(NAME_Exit, TEXT("UD3DRenderDevice::ShutdownAfterError"));
		UnSetRes(NULL,0);
		unguard;
	}
	void Flush()
	{
		guard(UD3DRenderDevice::Flush);
		if( Direct3DDevice )
		{
			guard(UnsetTextures);
			for( INT i=0; i<DeviceDesc.wMaxSimultaneousTextures; i++ )
				Direct3DDevice->SetTexture( i, NULL );
			for( FPixFormat* Fmt=FirstPixelFormat; Fmt; Fmt=Fmt->Next )
			{
				for( TMap<INT,FTexPool*>::TIterator ItP(Fmt->PoolList); ItP; ++ItP )
				{
					ItP.Value()->UsedCount = 0;
					for( FTexInfo* Info=ItP.Value()->First; Info; Info=Info->PoolNext )
						Info->CacheID = Info->Unloaded = Info->FrameCounter = Info->SubCounter = 0;
				}
			}
			for( i=0; i<ARRAY_COUNT(CachedTextures); i++ )
				CachedTextures[i]=NULL;
			unguard;
		}
		for( INT i=0; i<ARRAY_COUNT(Stages); i++ )
			Stages[i].Flush();
		if( GammaControl )
		{
			FLOAT Gamma = 1.5f * Viewport->GetOuterUClient()->Brightness;
			DDGAMMARAMP gramp;
			for( INT x = 0; x < 256; x++ )
				gramp.red[x] = gramp.green[x] = gramp.blue[x] = appFloor(appPow(x / 255.f, 1.0f / Gamma) * 65535.f + .5f);
			GammaControl->SetGammaRamp(0, &gramp);
		}
		PrecacheOnFlip = UsePrecache;
		PreloadMemory = 0;
		unguard;
	}
	void Lock( FPlane InFlashScale, FPlane InFlashFog, FPlane ScreenClear, DWORD InLockFlags, BYTE* HitData, INT* HitSize )
	{
		guard(UD3DRenderDevice::Lock);
		INT FailCount=0;
		Timing = 0;
		FrameCounter++;

		// Remember parameters.
		LockFlags  = InLockFlags;
		FlashScale = InFlashScale;
		FlashFog   = InFlashFog;

		// Check cooperative level.
		HRESULT hr=NULL, hr2=NULL;
		guard(TestCooperativeLevel);
		hr=DirectDraw->TestCooperativeLevel();
		unguard;
		if( hr!=DD_OK )
		{
			debugf(TEXT("TestCooperativeLevel failed (%s)"),*D3DError(hr));
			Failed:
			guard(HandleBigChange);
			// Fullscreen apps get DDERR_NOEXCLUSIVEMODE if we lost exclusive device access, i.e. other app takes over or user alt-tabs.
			// Windowed apps receive DDERR_EXCLUSIVEMODEALREADYSET if someone else has taken exclusive.
			// Windowed apps receive DDERR_WRONGMODE if mode was changed.
			do
			{
				guard(ReTestCooperativeLevel);
				Sleep( 100 );
				hr2 = DirectDraw->TestCooperativeLevel();
				unguard;
			}
			while( hr2==DDERR_NOEXCLUSIVEMODE || hr2==DDERR_EXCLUSIVEMODEALREADYSET );
			debugf(TEXT("Resetting mode (%s)"),*D3DError(hr2));
			if( !SetRes(ViewportX, ViewportY, ViewportColorBits*8, ViewportFullscreen) )
				appErrorf(TEXT("Failed resetting mode"));
			unguard;
		}

		// Clear the Z-buffer.
		guard(ClearZ);
		DWORD dwFlags = D3DCLEAR_ZBUFFER; 
		if( LockFlags & LOCKR_ClearScreen )
			dwFlags |= D3DCLEAR_TARGET;
		D3DRECT Rect={0,0,ViewportX,ViewportY};
		Direct3DViewport->Clear2( 1, &Rect, dwFlags, (DWORD)FColor(ScreenClear).TrueColor(), 1.f, 0 );
		unguard;

		// Init stats.
		appMemzero( &Stats, sizeof(Stats) );
		for( FPixFormat* Fmt=FirstPixelFormat; Fmt; Fmt=Fmt->Next )
			Fmt->InitStats();

		// Begin scene.
		guard(BeginScene);
		if( FAILED(h=Direct3DDevice->BeginScene()) )
		{
			if( ++FailCount==1 )
				goto Failed;
			appErrorf(TEXT("BeginScene failed (%s)"),*D3DError(h));
		}
		unguard;

		unguard;
	}
	void PrecacheTexture( FTextureInfo& Info, DWORD PolyFlags )
	{
		guard(UD3DRenderDevice::PrecacheTexture);
		SetTexture( 0, Info, PolyFlags, 1 );
		PrecacheCycle = 1;
		unguard;
	}
	void Unlock( BOOL Blit )
	{
		guard(UD3DRenderDevice::Unlock);
		Direct3DDevice->EndScene();
		if( PrecacheCycle )
		{
			PrecacheCycle = 0;
			debugf(TEXT("D3D Driver: Preloaded %iK/%iK (%i)"),PreloadMemory/1024,TotalMemory/1024,VideoMemoryFull);
		}
		if( Blit )
		{
			if( ViewportFullscreen )
			{
				// DDFLIP_WAIT means "wait until the flip is QUEUED", not necessarily executed.
				PrimarySurface->Flip( NULL, DDFLIP_WAIT | (UseVSync?DDFLIP_NOVSYNC:0) );
			}
			else
			{
				POINT Point={0,0};
				ClientToScreen( ViewporthWnd, &Point );
				RECT ScreenRect={Point.x,Point.y,ViewportX+Point.x,ViewportY+Point.y}, ViewportRect={0,0,ViewportX,ViewportY};
				PrimarySurface->Blt( &ScreenRect, BackSurface, &ViewportRect, DDBLT_WAIT, NULL );
			}
		}
		//debugf(TEXT("D3DTime=%f"),Timing*1000.0*GSecondsPerCycle);
		unguard;
	}
	void DrawComplexSurface( FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet )
	{
		guard(UD3DRenderDevice::DrawComplexSurface);
		clock(Stats.SurfTime);
		Stats.Surfs++;

		// Mutually exclusive effects.
		if( Surface.DetailTexture && Surface.FogMap )
			Surface.DetailTexture = NULL;

		// Render texture and lightmap.
		SetBlending( Surface.PolyFlags );
		if( UseMultitexture && Surface.LightMap!=NULL && Surface.MacroTexture==NULL )
		{
			// Use multitexturing when rendering base + lightmap.
			SetTexture( 0, *Surface.Texture, Surface.PolyFlags, 0 );
			SetTexture( 1, *Surface.LightMap, 0, 0 );

			// Set up all poly vertices.
			INT n = 0;
			D3DCOLOR clr = FColor(Stages[0].MaxColor * Stages[1].MaxColor).TrueColor() | 0xff000000;
			for( FSavedPoly* Poly=Facet.Polys; Poly; Poly = Poly->Next )
			{
				// Set up vertices.
				for( INT i=0; i<Poly->NumPts; i++ )
				{
					MVerts[n].dvSX    = Poly->Pts[i]->ScreenX + Frame->XB - 0.5f;
					MVerts[n].dvSY    = Poly->Pts[i]->ScreenY + Frame->YB - 0.5f;
					MVerts[n].dvRHW   = Poly->Pts[i]->RZ * Frame->RProj.Z;
					MVerts[n].dvSZ    = ProjectionMatrix._33 + ProjectionMatrix._43 * MVerts[n].dvRHW;
					MVerts[n].dcColor = clr;
					FLOAT u           = Facet.MapCoords.XAxis | (*(FVector*)Poly->Pts[i] - Facet.MapCoords.Origin);
					FLOAT v           = Facet.MapCoords.YAxis | (*(FVector*)Poly->Pts[i] - Facet.MapCoords.Origin);
					if( Surface.FogMap || Surface.DetailTexture )
					{
						Verts[n].dvSX = MVerts[n].dvSX;    
						Verts[n].dvSY = MVerts[n].dvSY;    
						Verts[n].dvRHW= MVerts[n].dvRHW;   
						Verts[n].dvSZ = MVerts[n].dvSZ;
						MasterU[n]    = u;
						MasterV[n]    = v;
					}
					MVerts[n].dvTU [Stages[0].UIndex] = (u - Surface.Texture->Pan.X                                   ) * Stages[0].UScale;
					MVerts[n].dvTU [Stages[0].VIndex] = (v - Surface.Texture->Pan.Y                                   ) * Stages[0].VScale;
					MVerts[n].dvTU2[Stages[1].UIndex] = (u - Surface.LightMap->Pan.X + 0.5f * Surface.LightMap->UScale) * Stages[1].UScale;
					MVerts[n].dvTU2[Stages[1].VIndex] = (v - Surface.LightMap->Pan.Y + 0.5f * Surface.LightMap->VScale) * Stages[1].VScale;
					++n;
				}
			}

			// Draw base texture + lightmap.
			n = 0;
			Direct3DDevice->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_MODULATE2X );
			Direct3DDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2 );
			for( Poly=Facet.Polys; Poly; n+=Poly->NumPts,Poly=Poly->Next)
			{
				Direct3DDevice->DrawPrimitive( D3DPT_TRIANGLEFAN, UNREALVFMT, &MVerts[n], Poly->NumPts, D3DDP_DONOTCLIP | D3DDP_DONOTLIGHT | D3DDP_DONOTUPDATEEXTENTS );
				Stats.Polys++;
			}
			Direct3DDevice->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
			Direct3DDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

			// Handle depth buffering the appropriate areas of masked textures.
			if( Surface.PolyFlags & PF_Masked )
				Direct3DDevice->SetRenderState( D3DRENDERSTATE_ZFUNC, D3DCMP_EQUAL );
		}
		else
		{
			// Set up all poly vertices.
			INT n = 0;
			for( FSavedPoly* Poly=Facet.Polys; Poly; Poly=Poly->Next )
			{
				// Set up vertices.
				for( INT i=0; i<Poly->NumPts; i++ )
				{
					Verts[n].dvSX  = Poly->Pts[i]->ScreenX + Frame->XB - 0.5f;
					Verts[n].dvSY  = Poly->Pts[i]->ScreenY + Frame->YB - 0.5f;
					Verts[n].dvRHW = Poly->Pts[i]->RZ * Frame->RProj.Z;
					Verts[n].dvSZ  = ProjectionMatrix._33 + ProjectionMatrix._43 * Verts[n].dvRHW;
					MasterU[n]     = Facet.MapCoords.XAxis | (*(FVector*)Poly->Pts[i] - Facet.MapCoords.Origin);
					MasterV[n]     = Facet.MapCoords.YAxis | (*(FVector*)Poly->Pts[i] - Facet.MapCoords.Origin);
					++n;
				}
			}

			// Count things to draw.
			INT ModulateThings = (Surface.Texture!=NULL) + (Surface.LightMap!=NULL) + (Surface.MacroTexture!=NULL);
			FPlane FinalColor(1,1,1,1);

			// Setup texture.
			SetTexture( 0, *Surface.Texture, Surface.PolyFlags, 0 );
			D3DCOLOR Clr = UpdateModulation( ModulateThings, FinalColor, Stages[0].MaxColor );
			n = 0;
			for( Poly=Facet.Polys; Poly; Poly=Poly->Next )
			{
				for( INT i=0; i<Poly->NumPts; i++ )
				{
					Verts[n].dcColor = Clr;
					Verts[n].dvTU[Stages[0].UIndex] = (MasterU[n] - Surface.Texture->Pan.X) * Stages[0].UScale;
					Verts[n].dvTU[Stages[0].VIndex] = (MasterV[n] - Surface.Texture->Pan.Y) * Stages[0].VScale;
					++n;
				}
				Direct3DDevice->DrawPrimitive( D3DPT_TRIANGLEFAN, D3DFVF_TLVERTEX, &Verts[n - Poly->NumPts], Poly->NumPts, D3DDP_DONOTCLIP | D3DDP_DONOTLIGHT | D3DDP_DONOTUPDATEEXTENTS );
				Stats.Polys++;
			}

			// Handle depth buffering the appropriate areas of masked textures.
			if( Surface.PolyFlags & PF_Masked )
				Direct3DDevice->SetRenderState( D3DRENDERSTATE_ZFUNC, D3DCMP_EQUAL );

			// Macrotexture.
			if( Surface.MacroTexture )
			{
				// Set the macrotexture.
				SetBlending( PF_Modulated );
				SetTexture( 0, *Surface.MacroTexture, 0, 0 );
				D3DCOLOR Clr = UpdateModulation( ModulateThings, FinalColor, Stages[0].MaxColor );
				INT n=0;
				for( Poly=Facet.Polys; Poly; Poly=Poly->Next )
				{
					for( INT i=0; i<Poly->NumPts; i++,n++ )
					{
						Verts[n].dcColor = Clr;
						Verts[n].dvTU[Stages[0].UIndex] = (MasterU[n] - Surface.MacroTexture->Pan.X) * Stages[0].UScale;
						Verts[n].dvTU[Stages[0].VIndex] = (MasterV[n] - Surface.MacroTexture->Pan.Y) * Stages[0].VScale;
					}
					Direct3DDevice->DrawPrimitive( D3DPT_TRIANGLEFAN, D3DFVF_TLVERTEX, &Verts[n - Poly->NumPts], Poly->NumPts, D3DDP_DONOTCLIP | D3DDP_DONOTLIGHT | D3DDP_DONOTUPDATEEXTENTS );
				}
			}

			// Light map.
			if( Surface.LightMap )
			{
				// Set the light map.
				SetBlending( PF_Modulated );
				SetTexture( 0, *Surface.LightMap, 0, 0 );
				D3DCOLOR Clr = UpdateModulation( ModulateThings, FinalColor, Stages[0].MaxColor );
				INT n=0;
				for( Poly=Facet.Polys; Poly; Poly=Poly->Next )
				{
					for( INT i=0; i<Poly->NumPts; i++,n++ )
					{
						Verts[n].dcColor = Clr;
						Verts[n].dvTU[Stages[0].UIndex] = (MasterU[n] - Surface.LightMap->Pan.X + 0.5f * Surface.LightMap->UScale) * Stages[0].UScale;
						Verts[n].dvTU[Stages[0].VIndex] = (MasterV[n] - Surface.LightMap->Pan.Y + 0.5f * Surface.LightMap->VScale) * Stages[0].VScale;
					}
					Direct3DDevice->DrawPrimitive( D3DPT_TRIANGLEFAN, D3DFVF_TLVERTEX, &Verts[n - Poly->NumPts], Poly->NumPts, D3DDP_DONOTCLIP | D3DDP_DONOTLIGHT | D3DDP_DONOTUPDATEEXTENTS );
				}
			}
		}

		// Draw detail texture overlaid.
		if( Surface.DetailTexture && DetailTextures )
		{
			INT DetailMax = 4;
			FLOAT DetailScale=1.0, NearZ=380.0;
			if( !GIsEditor )
				*Surface.DetailTexture->MaxColor = FColor(255,255,255,255);
			while( DetailMax-- > 0 )
			{
				INT n=0;
				for( FSavedPoly* Poly=Facet.Polys; Poly; Poly=Poly->Next )
				{
					UBOOL IsNear[32], CountNear = 0;
					for( INT i=0; i<Poly->NumPts; i++ )
					{
						IsNear[i] = Poly->Pts[i]->Point.Z < NearZ;
						CountNear += IsNear[i];
					}
					if( CountNear )
					{
						INT NumNear = 0;
						SetTexture( 0, *Surface.DetailTexture, 0, 0 );
						FD3DVertex Near[32];
						for( INT i=0, j=Poly->NumPts-1, m=n+Poly->NumPts-1; i<Poly->NumPts; j=i++, m=n++ )
						{
							if( IsNear[i] ^ IsNear[j] )
							{
								FLOAT G               = (Poly->Pts[i]->Point.Z - NearZ) / (Poly->Pts[i]->Point.Z - Poly->Pts[j]->Point.Z);
								FLOAT F               = 1.f - G;
								Near[NumNear].dvRHW   = 1.f / NearZ;
								Near[NumNear].dvSZ    = ProjectionMatrix._33 + ProjectionMatrix._43 * Near[NumNear].dvRHW;
								Near[NumNear].dvSX    = (F * Poly->Pts[i]->ScreenX * Poly->Pts[i]->Point.Z + G * Poly->Pts[j]->ScreenX * Poly->Pts[j]->Point.Z) * Near[NumNear].dvRHW + Frame->XB - 0.5f;
								Near[NumNear].dvSY    = (F * Poly->Pts[i]->ScreenY * Poly->Pts[i]->Point.Z + G * Poly->Pts[j]->ScreenY * Poly->Pts[j]->Point.Z) * Near[NumNear].dvRHW + Frame->YB - 0.5f;
								Near[NumNear].dcColor = RGBA_MAKE(0x7F, 0x7F, 0x7F, 0);
								Near[NumNear].dvTU[Stages[0].UIndex] = (F * MasterU[n] + G * MasterU[m] - Surface.DetailTexture->Pan.X) * Stages[0].UScale;
								Near[NumNear].dvTU[Stages[0].VIndex] = (F * MasterV[n] + G * MasterV[m] - Surface.DetailTexture->Pan.Y) * Stages[0].VScale;
								NumNear++;
							}
							if( IsNear[i] )
							{
								Near[NumNear].dvRHW   = Verts[n].dvRHW;
								Near[NumNear].dvSZ    = Verts[n].dvSZ;
								Near[NumNear].dvSX    = Verts[n].dvSX;
								Near[NumNear].dvSY    = Verts[n].dvSY;
								Near[NumNear].dvTU[Stages[0].UIndex] = (MasterU[n] - Surface.DetailTexture->Pan.X) * Stages[0].UScale * DetailScale;
								Near[NumNear].dvTU[Stages[0].VIndex] = (MasterV[n] - Surface.DetailTexture->Pan.Y) * Stages[0].VScale * DetailScale;
								DWORD A               = Min<DWORD>( appRound(100.f * (NearZ / Poly->Pts[i]->Point.Z - 1.f)), 255 );
								Near[NumNear].dcColor = RGBA_MAKE( 0x7F, 0x7F, 0x7F, A );
								NumNear++;
							}
						}
						SetBlending( PF_Modulated );
						Direct3DDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_BLENDDIFFUSEALPHA );
						Direct3DDevice->SetRenderState( D3DRENDERSTATE_ZBIAS, UNREAL_NOZBIAS );
						Direct3DDevice->DrawPrimitive( D3DPT_TRIANGLEFAN, D3DFVF_TLVERTEX, Near, NumNear, D3DDP_DONOTCLIP | D3DDP_DONOTLIGHT | D3DDP_DONOTUPDATEEXTENTS );
						Direct3DDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
						Direct3DDevice->SetRenderState( D3DRENDERSTATE_ZBIAS, UNREAL_ZBIAS );
						n -= Poly->NumPts;
					}
					n += Poly->NumPts;
				}
				DetailScale *= 4.0;
				NearZ /= 4.0;
			}
		}

		// Fog map.
		if( Surface.FogMap )
		{
			SetBlending( PF_Highlighted );
			SetTexture( 0, *Surface.FogMap, 0, 0 );
			INT n = 0;
			D3DCOLOR Clr = FColor(Stages[0].MaxColor).TrueColor() | 0xff000000;
			if( !Format8888.pf.dwSize ) // Texture has no alpha.
				Clr&=~0xff000000;
			for( FSavedPoly* Poly=Facet.Polys; Poly; Poly=Poly->Next )
			{
				for( INT i=0; i<Poly->NumPts; i++ )
				{
					Verts[n].dcColor = Clr;
					Verts[n].dvTU[Stages[0].UIndex] = (MasterU[n] - Surface.FogMap->Pan.X + 0.5f * Surface.FogMap->UScale) * Stages[0].UScale;
					Verts[n].dvTU[Stages[0].VIndex] = (MasterV[n] - Surface.FogMap->Pan.Y + 0.5f * Surface.FogMap->VScale) * Stages[0].VScale;
					++n;
				}
				Direct3DDevice->DrawPrimitive( D3DPT_TRIANGLEFAN, D3DFVF_TLVERTEX, &Verts[n - Poly->NumPts], Poly->NumPts, D3DDP_DONOTCLIP | D3DDP_DONOTLIGHT | D3DDP_DONOTUPDATEEXTENTS );
			}
		}

		// Finish mask handling.
		if( Surface.PolyFlags & PF_Masked )
			Direct3DDevice->SetRenderState( D3DRENDERSTATE_ZFUNC, D3DCMP_LESSEQUAL );

		unclock(Stats.SurfTime);
		unguard;
	}
	void DrawGouraudPolygon( FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span )
	{
		guard(UD3DRenderDevice::DrawGouraudPolygon);
		clock(Stats.PolyTime);
		Stats.Polys++;

		// Set up verts.
		UBOOL DoFog = (PolyFlags & (PF_RenderFog|PF_Translucent|PF_Modulated))==PF_RenderFog;
		if( UseVertexSpecular && DoFog )
			Direct3DDevice->SetRenderState( D3DRENDERSTATE_SPECULARENABLE, TRUE );
		SetTexture( 0, Info, PolyFlags, 0 );
		for( INT i=0; i<NumPts; i++ )
		{
			FLOAT W             = DoFog ? (1.f-Pts[i]->Fog.W)*255.f : 255.f;
			Verts[i].dcColor    = RGBA_MAKE( appRound(Pts[i]->Light.X*Stages[0].MaxColor.X*W), appRound(Pts[i]->Light.Y*Stages[0].MaxColor.Y*W), appRound(Pts[i]->Light.Z*Stages[0].MaxColor.Z*W), 255 );
			Verts[i].dvSX       = Pts[i]->ScreenX + Frame->XB - 0.5f;
			Verts[i].dvSY       = Pts[i]->ScreenY + Frame->YB - 0.5f;
			Verts[i].dvRHW      = Pts[i]->RZ * Frame->RProj.Z;
			if( (GUglyHackFlags&1) && ViewportColorBits==16 )
					Verts[i].dvRHW *= 0.25;
			Verts[i].dvSZ       = ProjectionMatrix._33 + ProjectionMatrix._43 * Verts[i].dvRHW;
			Verts[i].dcSpecular = DoFog ? RGBA_MAKE( appRound(Pts[i]->Fog.X*(255.f-W)), appRound(Pts[i]->Fog.Y*(255.f-W)), appRound(Pts[i]->Fog.Z*(255.f-W)), 255 ) : 0;
			Verts[i].dvTU[Stages[0].UIndex] = Pts[i]->U * Stages[0].UScale;
			Verts[i].dvTU[Stages[0].VIndex] = Pts[i]->V * Stages[0].VScale;
		}
		if( PolyFlags & PF_Modulated )
			for( INT i=0; i<NumPts; i++ )
				Verts[i].dcColor = RGBA_MAKE( appRound(255.f*Stages[0].MaxColor.X), appRound(255.f*Stages[0].MaxColor.Y), appRound(255.f*Stages[0].MaxColor.Z), 255 );

		// Draw it.
		SetBlending( PolyFlags );
		Direct3DDevice->DrawPrimitive( D3DPT_TRIANGLEFAN, D3DFVF_TLVERTEX, Verts, NumPts, D3DDP_DONOTCLIP | D3DDP_DONOTLIGHT | D3DDP_DONOTUPDATEEXTENTS );
		if( UseVertexSpecular && DoFog )
			Direct3DDevice->SetRenderState( D3DRENDERSTATE_SPECULARENABLE, FALSE );

		// Fog.
		if( DoFog && !UseVertexSpecular )
		{
			SetBlending( PF_Highlighted );
			for( INT i=0; i<NumPts; i++ )
				Verts[i].dcColor = Verts[i].dcSpecular;
			Direct3DDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG2 );
			Direct3DDevice->DrawPrimitive( D3DPT_TRIANGLEFAN, D3DFVF_TLVERTEX, Verts, NumPts, D3DDP_DONOTCLIP | D3DDP_DONOTLIGHT | D3DDP_DONOTUPDATEEXTENTS );
			Direct3DDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
		}
		unclock(Stats.PolyTime);
		unguard;
	}
	void DrawTile( FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags )
	{
		guard(UD3DRenderDevice::DrawTile);
		clock(Stats.TileTime);
		Stats.Tiles++;
		if( Info.Palette && Info.Palette[128].A!=255 && !(PolyFlags&PF_Translucent) )
			PolyFlags |= PF_Highlighted;

		SetBlending( PolyFlags );
		SetTexture( 0, Info, PolyFlags, 0 );
		FLOAT RZ = 1.0 / Z;
		X       += Frame->XB - 0.5;
		Y       += Frame->YB - 0.5;
		INT u    = Stages[0].UIndex;
		INT v    = Stages[0].VIndex;
		D3DCOLOR Clr = FColor( (PolyFlags & PF_Modulated) ? Stages[0].MaxColor : Stages[0].MaxColor*Color).TrueColor() | 0xff000000;
		Verts[0].dvSX = X;      Verts[0].dvSY = Y;      Verts[0].dvRHW = RZ; Verts[0].dvSZ = ProjectionMatrix._33 + ProjectionMatrix._43 * Verts[0].dvRHW; Verts[0].dvTU[u] = (U     ) * Stages[0].UScale; Verts[0].dvTU[v] = (V     ) * Stages[0].VScale; Verts[0].dcColor = Clr;
		Verts[1].dvSX = X;      Verts[1].dvSY = Y + YL; Verts[1].dvRHW = RZ; Verts[1].dvSZ = ProjectionMatrix._33 + ProjectionMatrix._43 * Verts[1].dvRHW; Verts[1].dvTU[u] = (U     ) * Stages[0].UScale; Verts[1].dvTU[v] = (V + VL) * Stages[0].VScale; Verts[1].dcColor = Clr;
		Verts[2].dvSX = X + XL; Verts[2].dvSY = Y + YL; Verts[2].dvRHW = RZ; Verts[2].dvSZ = ProjectionMatrix._33 + ProjectionMatrix._43 * Verts[2].dvRHW; Verts[2].dvTU[u] = (U + UL) * Stages[0].UScale; Verts[2].dvTU[v] = (V + VL) * Stages[0].VScale; Verts[2].dcColor = Clr;
		Verts[3].dvSX = X + XL; Verts[3].dvSY = Y;      Verts[3].dvRHW = RZ; Verts[3].dvSZ = ProjectionMatrix._33 + ProjectionMatrix._43 * Verts[3].dvRHW; Verts[3].dvTU[u] = (U + UL) * Stages[0].UScale; Verts[3].dvTU[v] = (V     ) * Stages[0].VScale; Verts[3].dcColor = Clr;
		Direct3DDevice->DrawPrimitive( D3DPT_TRIANGLEFAN, D3DFVF_TLVERTEX, Verts, 4, D3DDP_DONOTCLIP | D3DDP_DONOTLIGHT | D3DDP_DONOTUPDATEEXTENTS );

		unclock(Stats.TileTime);
		unguard;
	}
	static HRESULT CALLBACK EnumModesCallback( DDSURFACEDESC2* Desc, void* Context )
	{
		((TArray<DDSURFACEDESC2>*)Context)->AddItem( *Desc );
		return DDENUMRET_OK;
	}
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar )
	{
		guard(UD3DRenderDevice::Exec);
		if( URenderDevice::Exec( Cmd, Ar ) )
		{
			return 1;
		}
		else if( DirectDraw && ParseCommand(&Cmd,TEXT("GetRes")) )
		{
			TArray<FVector> Res;
			for( TArray<DDSURFACEDESC2>::TIterator It(DisplayModes); It; ++It )
				if( It->ddpfPixelFormat.dwRGBBitCount==16 )
					Res.AddUniqueItem( FVector(It->dwWidth, It->dwHeight, 0) );
			for( INT i=0; i<Res.Num() && i<16/*script limitation*/; i++ )
				if( Res(i).X<=MaxResWidth && Res(i).Y<=MaxResHeight )
					Ar.Logf( i ? TEXT(" %ix%i") : TEXT("%ix%i"), (INT)Res(i).X, (INT)Res(i).Y );
			return 1;
		}
		else if( ParseCommand(&Cmd,TEXT("LodBias")) )
		{
			FLOAT LodBias=appAtof(Cmd);
			Ar.Logf(TEXT("Texture LodBias %f"),LodBias);
			Direct3DDevice->SetRenderState( D3DRENDERSTATE_MIPMAPLODBIAS, *(DWORD*)&LodBias );
			return 1;
		}
		else return 0;
		unguard;
	}
	void Draw2DLine( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2 )
	{
		guard(UD3DRenderDevice::Draw2DLine);

		//!!should optimize to avoid changing shade mode, color op, alpha op.
		Direct3DDevice->SetRenderState( D3DRENDERSTATE_SHADEMODE, D3DSHADE_FLAT );
		Direct3DDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE  );
		Direct3DDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

		Verts[0].dvSX    = P1.X - 0.5f;
		Verts[0].dvSY    = P1.Y - 0.5f;
		Verts[0].dvRHW   = 1.f; 
		Verts[0].dvSZ    = ProjectionMatrix._33 + ProjectionMatrix._43 * Verts[0].dvRHW;
		Verts[0].dcColor = FColor(Color).TrueColor() | 0xff000000;

		Verts[1].dvSX    = P2.X - 0.5f;
		Verts[1].dvSY    = P2.Y - 0.5f;
		Verts[1].dvRHW   = 1.f; 
		Verts[1].dvSZ    = ProjectionMatrix._33 + ProjectionMatrix._43 * Verts[1].dvRHW;
		Verts[1].dcColor = FColor(Color).TrueColor() | 0xff000000;

		Direct3DDevice->DrawPrimitive( D3DPT_LINESTRIP, D3DFVF_TLVERTEX, Verts, 2, D3DDP_DONOTCLIP | D3DDP_DONOTLIGHT | D3DDP_DONOTUPDATEEXTENTS );
		Direct3DDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
		Direct3DDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
		Direct3DDevice->SetRenderState( D3DRENDERSTATE_SHADEMODE, D3DSHADE_GOURAUD );

		unguard;
	}
	void UD3DRenderDevice::Draw2DPoint( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z )
	{
		guard(UD3DRenderDevice::Draw2DPoint);

		//!!should optimize to avoid changing shade mode, color op, alpha op.
		Direct3DDevice->SetRenderState( D3DRENDERSTATE_SHADEMODE, D3DSHADE_FLAT );
		Direct3DDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE );
		Direct3DDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

		Verts[0].dvSX    = X1 - 0.5f;
		Verts[0].dvSY    = Y1 - 0.5f;
		Verts[0].dvRHW   = 1.f; 
		Verts[0].dvSZ    = ProjectionMatrix._33 + ProjectionMatrix._43 * Verts[0].dvRHW;
		Verts[0].dcColor = FColor(Color).TrueColor() | 0xff000000;

		Verts[1].dvSX    = X2 - 1.5f;
		Verts[1].dvSY    = Y1 - 1.f;
		Verts[1].dvRHW   = 1.f; 
		Verts[1].dvSZ    = ProjectionMatrix._33 + ProjectionMatrix._43 * Verts[1].dvRHW;
		Verts[1].dcColor = FColor(Color).TrueColor() | 0xff000000;

		Verts[2].dvSX    = X2 - 1.5f;
		Verts[2].dvSY    = Y2 - 1.5f;
		Verts[2].dvRHW   = 1.f; 
		Verts[2].dvSZ    = ProjectionMatrix._33 + ProjectionMatrix._43 * Verts[2].dvRHW;
		Verts[2].dcColor = FColor(Color).TrueColor() | 0xff000000;

		Verts[3].dvSX    = X1 - 0.5f;
		Verts[3].dvSY    = Y2 - 1.5f;
		Verts[3].dvRHW   = 1.f; 
		Verts[3].dvSZ    = ProjectionMatrix._33 + ProjectionMatrix._43 * Verts[3].dvRHW;
		Verts[3].dcColor = FColor(Color).TrueColor() | 0xff000000;

		Verts[4].dvSX    = X1 - 0.5f;
		Verts[4].dvSY    = Y1 - 0.5f;
		Verts[4].dvRHW   = 1.f; 
		Verts[4].dvSZ    = ProjectionMatrix._33 + ProjectionMatrix._43 * Verts[4].dvRHW;
		Verts[4].dcColor = FColor(Color).TrueColor() | 0xff000000;

		Direct3DDevice->DrawPrimitive( D3DPT_LINESTRIP, D3DFVF_TLVERTEX, Verts, 5, D3DDP_DONOTCLIP | D3DDP_DONOTLIGHT | D3DDP_DONOTUPDATEEXTENTS );
		Direct3DDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
		Direct3DDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
		Direct3DDevice->SetRenderState( D3DRENDERSTATE_SHADEMODE, D3DSHADE_GOURAUD );

		unguard;
	}
	void UD3DRenderDevice::PushHit( const BYTE* Data, INT Count )
	{
		guard(UD3DRenderDevice::PushHit);
		unguard;
	}
	void UD3DRenderDevice::PopHit( INT Count, UBOOL bForce )
	{
		guard(UD3DRenderDevice::PopHit);
		unguard;
	}
	void GetStats( TCHAR* Result )
	{
		guard(UD3DRenderDevice::GetStats);
		appSprintf
		(
			Result,
			TEXT("surf=%03i %04.1f poly=%03i %04.1f tile=%03i %04.1f"),
			Stats.Surfs,
			GSecondsPerCycle * 1000 * Stats.SurfTime,
			Stats.Polys,
			GSecondsPerCycle * 1000 * Stats.PolyTime,
			Stats.Tiles,
			GSecondsPerCycle * 1000 * Stats.TileTime
		);
		for( FPixFormat* Fmt=FirstPixelFormat; Fmt; Fmt=Fmt->Next )
			appSprintf
			(
				Result + appStrlen(Result),
				TEXT(" %s=%iK/%iK %i/%i %i %i %04.1f"),
				Fmt->Desc,
				Fmt->ActiveRAM/1024,
				Fmt->BinnedRAM/1024,
				Fmt->Active,
				Fmt->Binned,
				Fmt->Sets,
				Fmt->Uploads,
				Fmt->UploadCycles * GSecondsPerCycle * 1000.f
			);
		if( Stats.Thrashes )
			appSprintf
			(
				Result + appStrlen(Result),
				TEXT(" thrash=%i %04.1f (%ix%i)"),
				Stats.Thrashes,
				GSecondsPerCycle * 1000 * Stats.ThrashTime,
				Stats.ThrashX,
				Stats.ThrashY
			);
		unguard;
	}
	void ClearZ( FSceneNode* Frame )
	{
		guard(UD3DRenderDevice::ClearZ);

		// Clear only the Z-buffer.
		D3DRECT Rect={0,0,ViewportX,ViewportY};
		Direct3DViewport->Clear2( 1, &Rect, D3DCLEAR_ZBUFFER, 0, 1.f, 0 );
    
		unguard;
	}
	void ReadPixels( FColor* Pixels )
	{
		guard(UD3DRenderDevice::ReadPixels);

		// Lock primary surface.
		DECLARE_INITED(DDSURFACEDESC2,ddsd);
		RECT r, *RectPtr=NULL;
		if( !ViewportFullscreen )
		{
			POINT p1={0,0}, p2={ViewportX,ViewportY};
			ClientToScreen( ViewporthWnd, &p1 );
			ClientToScreen( ViewporthWnd, &p2 );
			r.left = p1.x; r.right  = p2.x;
			r.top  = p1.y; r.bottom = p2.y;
			RectPtr = &r;
		}
		HRESULT ddrval = PrimarySurface->Lock( RectPtr, &ddsd, DDLOCK_READONLY | DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, NULL );
		if( ddrval!=DD_OK )
		{
			if( ddrval!=DDERR_SURFACELOST )
				debugf( NAME_Log, TEXT("D3D Driver: Lock on primary failed (%s)"), *D3DError(ddrval) );
			return;
		}

		// Compute needed bit shifts.
		DWORD rl, rr, gl, gr, bl, br, mask;
		for( rr=0, mask=ddsd.ddpfPixelFormat.dwRBitMask; !(mask&1); mask>>=1, ++rr );
		for( rl=8; mask&1; mask>>=1, --rl );
		for( gr=0, mask=ddsd.ddpfPixelFormat.dwGBitMask; !(mask&1); mask>>=1, ++gr );
		for( gl=8; mask&1; mask>>=1, --gl );
		for( br=0, mask=ddsd.ddpfPixelFormat.dwBBitMask; !(mask&1); mask>>=1, ++br );
		for( bl=8; mask&1; mask>>=1, --bl );

		// Compute gamma correction if needed.
		DWORD gamtbl[256];
		if( GammaControl )
		{
			FLOAT Gamma = 1.5f * Viewport->GetOuterUClient()->Brightness;
			for( INT i = 0; i < 256; ++i )
				gamtbl[i] = appFloor( appPow(i / 255.f, 1.0f / Gamma) * 255.f + .5f );
		}
		FColor* dst = Pixels;
		switch( ddsd.ddpfPixelFormat.dwRGBBitCount )
		{
			case 16:
			{
				_WORD* src = (_WORD*)ddsd.lpSurface;
				for( INT i=0; i<ViewportY; ++i )
				{
					for( INT j=0; j<ViewportX; ++j )
					{
						DWORD R = ((src[j] & ddsd.ddpfPixelFormat.dwRBitMask) >> rr) << rl;
						DWORD G = ((src[j] & ddsd.ddpfPixelFormat.dwGBitMask) >> gr) << gl;
						DWORD B = ((src[j] & ddsd.ddpfPixelFormat.dwBBitMask) >> br) << bl;
						if( GammaControl )
						{
							R = gamtbl[R];
							G = gamtbl[G];
							B = gamtbl[B];
						}
						GET_COLOR_DWORD(*dst++) = (R << 16) | (G << 8) | B;
					}
					src = (_WORD*)((BYTE*)src + ddsd.lPitch);
				}
				break;
			}
			case 24:
			{
				BYTE* src = (BYTE*)ddsd.lpSurface;
				for( INT i=0; i<ViewportY; ++i )
				{
					for( INT j=0; j<ViewportX; ++j )
					{
						DWORD R = ((*((DWORD*)&src[j * 3]) & ddsd.ddpfPixelFormat.dwRBitMask) >> rr) << rl;
						DWORD G = ((*((DWORD*)&src[j * 3]) & ddsd.ddpfPixelFormat.dwGBitMask) >> gr) << gl;
						DWORD B = ((*((DWORD*)&src[j * 3]) & ddsd.ddpfPixelFormat.dwBBitMask) >> br) << bl;
						if( GammaControl )
						{
							R = gamtbl[R];
							G = gamtbl[G];
							B = gamtbl[B];
						}
						GET_COLOR_DWORD(*dst++) = (R << 16) | (G << 8) | B;
					}
					src += ddsd.lPitch;
				}
				break;
			}
			case 32:
			{
				DWORD* src = (DWORD*)ddsd.lpSurface;
				for( INT i=0; i<ViewportY; ++i )
				{
					for( INT j=0; j<ViewportX; ++j )
					{
						DWORD R = ((src[j] & ddsd.ddpfPixelFormat.dwRBitMask) >> rr) << rl;
						DWORD G = ((src[j] & ddsd.ddpfPixelFormat.dwGBitMask) >> gr) << gl;
						DWORD B = ((src[j] & ddsd.ddpfPixelFormat.dwBBitMask) >> br) << bl;
						if( GammaControl )
						{
							R = gamtbl[R];
							G = gamtbl[G];
							B = gamtbl[B];
						}
						GET_COLOR_DWORD(*dst++) = (R << 16) | (G << 8) | B;
					}
					src = (DWORD*)((BYTE*)src + ddsd.lPitch);
				}
				break;
			}
		}
		PrimarySurface->Unlock( NULL );
		unguard;
	}
	void UD3DRenderDevice::EndFlash()
	{
		guard(UD3DRenderDevice::EndFlash);
		if( FlashScale!=FVector(.5,.5,.5) || FlashFog!=FVector(0,0,0) )
		{
			// Setup color.
			FColor D3DColor = FColor(FPlane(FlashFog.X,FlashFog.Y,FlashFog.Z,Min(FlashScale.X*2.f,1.f)));

			// Set up verts.
			Verts[0].dvSX = 0;               Verts[0].dvSY = 0;               Verts[0].dvRHW = 0.5f; Verts[0].dvSZ = ProjectionMatrix._33 + ProjectionMatrix._43 * Verts[0].dvRHW; Verts[0].dcColor = RGBA_MAKE(D3DColor.R, D3DColor.G, D3DColor.B, D3DColor.A);
			Verts[1].dvSX = 0;               Verts[1].dvSY = Viewport->SizeY; Verts[1].dvRHW = 0.5f; Verts[1].dvSZ = ProjectionMatrix._33 + ProjectionMatrix._43 * Verts[1].dvRHW; Verts[1].dcColor = RGBA_MAKE(D3DColor.R, D3DColor.G, D3DColor.B, D3DColor.A);
			Verts[2].dvSX = Viewport->SizeX; Verts[2].dvSY = Viewport->SizeY; Verts[2].dvRHW = 0.5f; Verts[2].dvSZ = ProjectionMatrix._33 + ProjectionMatrix._43 * Verts[2].dvRHW; Verts[2].dcColor = RGBA_MAKE(D3DColor.R, D3DColor.G, D3DColor.B, D3DColor.A);
			Verts[3].dvSX = Viewport->SizeX; Verts[3].dvSY = 0;               Verts[3].dvRHW = 0.5f; Verts[3].dvSZ = ProjectionMatrix._33 + ProjectionMatrix._43 * Verts[3].dvRHW; Verts[3].dcColor = RGBA_MAKE(D3DColor.R, D3DColor.G, D3DColor.B, D3DColor.A);

			// Draw it.
			SetBlending( PF_Translucent|PF_NoOcclude );
			Direct3DDevice->SetRenderState( D3DRENDERSTATE_ALPHABLENDENABLE, TRUE );
			Direct3DDevice->SetRenderState( D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE );
			Direct3DDevice->SetRenderState( D3DRENDERSTATE_DESTBLEND, D3DBLEND_SRCALPHA );
			Direct3DDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG2 );
			Direct3DDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2 );
			Direct3DDevice->SetRenderState( D3DRENDERSTATE_ZFUNC, D3DCMP_ALWAYS );
			Direct3DDevice->DrawPrimitive( D3DPT_TRIANGLEFAN, D3DFVF_TLVERTEX, Verts, 4, D3DDP_DONOTCLIP | D3DDP_DONOTLIGHT | D3DDP_DONOTUPDATEEXTENTS );
			Direct3DDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
			Direct3DDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
			Direct3DDevice->SetRenderState( D3DRENDERSTATE_ZFUNC, D3DCMP_LESSEQUAL );
			SetBlending( 0 );
		}
		unguard;
	}

    // Private functions.
	static HRESULT CALLBACK EnumPixelFormatsCallback( DDPIXELFORMAT* PixelFormat, void* Context )
	{
		((TArray<DDPIXELFORMAT>*)Context)->AddItem( *PixelFormat );
		return D3DENUMRET_OK;
	}
	void SetBlending( DWORD PolyFlags )
	{
		// Adjust PolyFlags according to Unreal's precedence rules.
		if( !(PolyFlags & (PF_Translucent|PF_Modulated)) )
			PolyFlags |= PF_Occlude;
		else if( PolyFlags & PF_Translucent )
			PolyFlags &= ~PF_Masked;

		// Detect changes in the blending modes.
		DWORD Xor = CurrentPolyFlags^PolyFlags;
		if( Xor & (PF_Translucent|PF_Modulated|PF_Invisible|PF_Occlude|PF_Masked|PF_Highlighted|PF_NoSmooth) )
		{
			guard(UD3DRenderDevice::SetBlending);
			if( Xor&(PF_Invisible|PF_Translucent|PF_Modulated|PF_Highlighted) )
			{
				if( !(PolyFlags & (PF_Invisible|PF_Translucent|PF_Modulated|PF_Highlighted)) )
				{
					Direct3DDevice->SetRenderState( D3DRENDERSTATE_ALPHABLENDENABLE, FALSE );
				}
				else if( PolyFlags & PF_Invisible )
				{
					Direct3DDevice->SetRenderState( D3DRENDERSTATE_ALPHABLENDENABLE, TRUE );
					Direct3DDevice->SetRenderState( D3DRENDERSTATE_SRCBLEND, D3DBLEND_ZERO );
					Direct3DDevice->SetRenderState( D3DRENDERSTATE_DESTBLEND, D3DBLEND_ONE );
				}
				else if( PolyFlags & PF_Translucent )
				{
					Direct3DDevice->SetRenderState( D3DRENDERSTATE_ALPHABLENDENABLE, TRUE );
					Direct3DDevice->SetRenderState( D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE );
					Direct3DDevice->SetRenderState( D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCCOLOR );
				}
				else if( PolyFlags & PF_Modulated )
				{
					Direct3DDevice->SetRenderState( D3DRENDERSTATE_ALPHABLENDENABLE, TRUE );
					Direct3DDevice->SetRenderState( D3DRENDERSTATE_SRCBLEND, D3DBLEND_DESTCOLOR );
					Direct3DDevice->SetRenderState( D3DRENDERSTATE_DESTBLEND, D3DBLEND_SRCCOLOR );
				}
				else if( PolyFlags & PF_Highlighted )
				{
					Direct3DDevice->SetRenderState( D3DRENDERSTATE_ALPHABLENDENABLE, TRUE );
					Direct3DDevice->SetRenderState( D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE );
					Direct3DDevice->SetRenderState( D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA );
				}
			}
			if( Xor & PF_Invisible )
			{
				UBOOL Invisible = ((PolyFlags&PF_Invisible)!=0);
				Direct3DDevice->SetRenderState( D3DRENDERSTATE_ALPHABLENDENABLE, Invisible );
				Direct3DDevice->SetRenderState( D3DRENDERSTATE_SRCBLEND, D3DBLEND_ZERO );
				Direct3DDevice->SetRenderState( D3DRENDERSTATE_DESTBLEND, D3DBLEND_ONE );
			}
			if( Xor & PF_Occlude )
			{
				Direct3DDevice->SetRenderState( D3DRENDERSTATE_ZWRITEENABLE, (PolyFlags&PF_Occlude)!=0 );
			}
			if( Xor & PF_Masked )
			{
				Direct3DDevice->SetRenderState( D3DRENDERSTATE_ALPHATESTENABLE, (PolyFlags & PF_Masked)!=0 );
			}
			if( Xor & PF_NoSmooth )
			{
				Direct3DDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, (PolyFlags & PF_NoSmooth) ? D3DTFG_POINT : D3DTFG_LINEAR );
				Direct3DDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, (PolyFlags & PF_NoSmooth) ? D3DTFN_POINT : D3DTFN_LINEAR );
			}
			CurrentPolyFlags = PolyFlags;
			unguard;
		}
	}
	FTexInfo* CreateVideoTexture( FTexPool* Pool )
	{
		guard(UD3DRenderDevice::CreateVideoTexture);
		//static INT c=0,d=0;c+=Pool->USize*Pool->VSize*2,d++;
		//debugf(TEXT("%i %ix%i %iK"),d,Pool->USize,Pool->VSize,c/1024);

		// Build surface description.
		DECLARE_INITED(DDSURFACEDESC2,SurfaceDesc);
		SurfaceDesc.dwFlags         = DDSD_CAPS | DDSD_PIXELFORMAT | DDSD_WIDTH | DDSD_HEIGHT;
		SurfaceDesc.ddsCaps.dwCaps  = DDSCAPS_TEXTURE | DDSCAPS_MIPMAP | DDSCAPS_COMPLEX | DDSCAPS_VIDEOMEMORY | DDSCAPS_LOCALVIDMEM;
		SurfaceDesc.ddsCaps.dwCaps2 = 0;
		SurfaceDesc.ddpfPixelFormat = Pool->PixelFormat->pf;
		SurfaceDesc.dwWidth         = Pool->USize;
		SurfaceDesc.dwHeight        = Pool->VSize;
		check(Pool->USize<=256);
		check(Pool->VSize<=256);

		// Create surface.
		IDirectDrawSurface4* Surface=NULL;
		h=DirectDraw->CreateSurface(&SurfaceDesc,&Surface,NULL);
		if( !Surface )
		{
			VideoMemoryFull = 1;
			return NULL;
		}

		// Query for texture.
		IDirect3DTexture2* Texture=NULL;
		Surface->QueryInterface( IID_IDirect3DTexture2, (void**)&Texture );
		check(Texture);

		// Create new FTexInfo.
		FTexInfo* ti		= new FTexInfo;
		ti->FrameCounter	= 0;
		ti->SubCounter		= 0;
		ti->Unloaded		= 0;
		ti->Surface			= Surface;
		ti->Texture			= Texture;
		ti->Pool			= Pool;
		ti->CacheID			= 0;

		// Add to beginning of linked list.
		INT Size = Pool->USize * Pool->VSize * Pool->PixelFormat->BitsPerPixel/8;
		ti->PoolNext = Pool->First;
		Pool->First = ti;
		Pool->TexCount++;
		Pool->PixelFormat->BinnedRAM += Size;
		TotalMemory += Size;
		Pool->PixelFormat->Binned++;

		return ti;
		unguard;
	}
	void SetTexture( DWORD dwStage, FTextureInfo& Info, DWORD PolyFlags, UBOOL Precache )
	{
		if( Stages[dwStage].TextureCacheID==Info.CacheID )
			return;

		// Make texture current.
		guard(UD3DRenderDevice::SetTexture);
		UBOOL Thrash=0;
		INT HashIndex = (7*(DWORD)Info.CacheID) & (ARRAY_COUNT(CachedTextures)-1);
		for( FTexInfo* ti=CachedTextures[HashIndex]; ti && ti->CacheID!=Info.CacheID; ti=ti->HashNext );
		if( !ti )
		{
			// Get filler object.
			FTexFiller* fi=NULL;
			if( Info.Format==TEXF_DXT1 )
				fi = &FillerDXT1;
			else if( Info.Format==TEXF_RGBA7 )
				fi = Format8888.pf.dwSize ? (FTexFiller*)&Filler8888_RGBA7 : (FTexFiller*)&Filler1555_RGBA7;
			else if( Info.Format==TEXF_P8 )
				fi = Format8888.pf.dwSize ? (FTexFiller*)&Filler8888_P8 : (FTexFiller*)&Filler1555_P8;
			else appErrorf(TEXT("Unknown Unreal Texture Format"));

			// If too large, shrink by going down mipmaps.
			Info.bRealtimeChanged=1;
			DWORD FirstMip=0, USize=0, VSize=0;
			while
			(	(USize=Info.Mips[FirstMip]->USize)>fi->PixelFormat->MaxWidth
			||	(VSize=Info.Mips[FirstMip]->VSize)>fi->PixelFormat->MaxHeight )
				if( ++FirstMip >= (DWORD)Info.NumMips )
					appErrorf( TEXT("D3D Driver: Encountered oversize texture without sufficient mipmaps") );

			// If too small or bad aspect ratio, enlarge by tiling.
			for( ; ; )
			{
				if
				(	USize<DeviceDesc.dwMinTextureWidth
				||	USize*DeviceDesc.dwMaxTextureAspectRatio<VSize )
					USize*=2;
				else if
				(	VSize<(INT)DeviceDesc.dwMinTextureHeight
				||	VSize*(INT)DeviceDesc.dwMaxTextureAspectRatio<USize )
					VSize*=2;
				else break;
			}

			// If video memory isn't full, and texture cache is full, try creating new cache entry here.
			FTexPool* Pool = fi->PixelFormat->PoolList.FindRef(Max(USize,VSize)+Min(USize,VSize)*65543);
			check(Pool);
			if( Precache && Pool->UsedCount==Pool->TexCount )
				{Info.Load(); return;}
			if( !VideoMemoryFull && Pool->TexCount-Pool->UsedCount<=Pool->DesiredSwapCopies )
				ti=CreateVideoTexture( Pool );
			UBOOL ShouldUnload = Pool->TexCount-Pool->UsedCount>Pool->DesiredSwapCopies;
			if( ShouldUnload )
				PreloadMemory += USize*VSize*Pool->PixelFormat->BitsPerPixel/8;

			// Find best item to replace in the cache.
			if( !ti )
			{
				ti = Pool->First;
				INT BestOldness=-1, BestUnloaded=1;
				for( FTexInfo* Link=Pool->First; Link; Link=Link->PoolNext )
				{
					INT Oldness = FrameCounter - Link->FrameCounter;
					if( Link->Unloaded<BestUnloaded || (Link->Unloaded==BestUnloaded && Oldness>BestOldness) )
						{ti=Link; BestUnloaded=Link->Unloaded; BestOldness=Oldness;}
				}
				if( BestOldness<=1 )
				{
					// We are thrashing, so flip among 4 most recent textures to avoid both complete thrashing and async problems.
					Thrash = 1;
					INT BestSub=-1, Sub=0, Count=0;
					for( FTexInfo* Link=Pool->First; Link; Link=Link->PoolNext )
					{
						if( !Link->Unloaded && FrameCounter-Link->FrameCounter<=1 )
						{
							if( (Sub=SubCounter-Link->SubCounter)>BestSub )
								{ti=Link; BestSub=Sub;}
							if( ++Count > 4 )
								break;
						}
					}
				}
			}

			// Remove replaced item from cache.
			check(ti);
			if( ti->CacheID!=0 )
			{
				if( ti->HashNext )
					ti->HashNext->HashPrevLink = ti->HashPrevLink;
				*ti->HashPrevLink = ti->HashNext;
			}
			else Pool->UsedCount++;

			// Update the texture.
			ti->Unloaded		= ShouldUnload;
			ti->CacheID         = Info.CacheID;
			ti->Filler			= fi;
			ti->FirstMip		= FirstMip;
			ti->UIndex			= USize<VSize;
			ti->VIndex			= USize>=VSize;
			ti->UScale			= 1.f / (USize * (1<<FirstMip) * Info.UScale);
			ti->VScale			= 1.f / (VSize * (1<<FirstMip) * Info.VScale);

			// Add to double-linked cache hash list.
			ti->HashNext              = CachedTextures[HashIndex];
			CachedTextures[HashIndex] = ti;
			ti->HashPrevLink          = &CachedTextures[HashIndex];
			if( ti->HashNext )
				ti->HashNext->HashPrevLink = &ti->HashNext;

			/*if( ti->PixelFormat==&FormatP8 )
			{
				PALETTEENTRY pe[256];
				for( INT i=0; i<NUM_PAL_COLORS; ++i )
				{
					pe[i].peFlags = Info.Palette[i].A;
					pe[i].peRed   = (Info.Palette[i].R * ScaleR)>>8;
					pe[i].peGreen = (Info.Palette[i].G * ScaleG)>>8;
					pe[i].peBlue  = (Info.Palette[i].B * ScaleB)>>8;
				}
				if( PolyFlags & PF_Masked )
					pe[0].peRed = pe[0].peGreen = pe[0].peBlue = pe[0].peFlags = 0;
				UBOOL UseAlpha = (Info.Palette && Info.Palette[128].A!=255) || (PolyFlags & PF_Masked);
				if( FAILED(h=DirectDraw->CreatePalette(DDPCAPS_8BIT | DDPCAPS_ALLOW256 | DDPCAPS_INITIALIZE | (UseAlpha?DDPCAPS_ALPHA:0), pe, &ti->Palette, NULL)) )
					appErrorf(TEXT("CreatePalette failed (%s)"),*D3DError(h));
			}*/
		}

		// Transfer texture data.
		if( Info.bRealtimeChanged || (Info.Format==TEXF_RGBA7 && GET_COLOR_DWORD(*Info.MaxColor)==0xFFFFFFFF) )
		{
			// Get ready for blt.
			if( SupportsLazyTextures )
				Info.Load();
			Info.CacheMaxColor();
			ti->MaxColor = *Info.MaxColor;

			// Update texture data.
			DWORD Cycles=0;
			clock(Cycles);
			IDirectDrawSurface4* LockSurface = ti->Pool->SystemSurface ? ti->Pool->SystemSurface : ti->Surface;
			IDirectDrawSurface4* BltSurface  = ti->Surface;//Info.NumMips==1 ? ti->Surface : NULL;
			ti->Filler->PixelFormat->Uploads++;
			ti->Filler->BeginUpload( this, ti, Info, PolyFlags );
			for( INT MipIndex=ti->FirstMip; ; )
			{
				DECLARE_INITED(DDSURFACEDESC2,ddsd);
				if( FAILED(h=LockSurface->Lock(NULL,&ddsd,DDLOCK_NOSYSLOCK|DDLOCK_SURFACEMEMORYPTR|DDLOCK_WAIT|DDLOCK_WRITEONLY,NULL)) )
					appErrorf( TEXT("D3D Driver: Lock failed while loading system surface (%s)"), *D3DError(h) );
				if( Info.Format==TEXF_RGBA7 )
					ti->Filler->UploadMipmap( this, ti, (BYTE*)ddsd.lpSurface, ddsd.lPitch, Info, MipIndex, PolyFlags );
				else if( ti->UIndex==0 )
					for( DWORD u=0; u<ddsd.dwWidth; u+=Info.Mips[MipIndex]->USize )
						for( DWORD v=0; v<ddsd.dwHeight; v+=Info.Mips[MipIndex]->VSize )
							ti->Filler->UploadMipmap( this, ti, (BYTE*)ddsd.lpSurface + u*ti->Filler->PixelFormat->pf.dwRGBBitCount/8 + v*ddsd.lPitch, ddsd.lPitch, Info, MipIndex, PolyFlags );
				else
					for( DWORD u=0; u<ddsd.dwWidth; u+=Info.Mips[MipIndex]->VSize )
						for( DWORD v=0; v<ddsd.dwHeight; v+=Info.Mips[MipIndex]->USize )
							ti->Filler->UploadMipmap( this, ti, (BYTE*)ddsd.lpSurface + u*ti->Filler->PixelFormat->pf.dwRGBBitCount/8 + v*ddsd.lPitch, ddsd.lPitch, Info, MipIndex, PolyFlags );
				LockSurface->Unlock( NULL );
				if( BltSurface )
					if( FAILED(BltSurface->Blt( NULL, LockSurface, NULL, DDBLT_WAIT, NULL )) )
						appErrorf(TEXT("Failed blt: %s"),*D3DError(h));
				if( ++MipIndex>=Info.NumMips || LockSurface->GetAttachedSurface(&NextMipmapCaps,&LockSurface)!=DD_OK )
					break;
				LockSurface->Release();
				if( BltSurface && BltSurface->GetAttachedSurface(&NextMipmapCaps,&BltSurface)!=DD_OK )
					break;
				if( BltSurface )
					BltSurface->Release();
			}
			if( ti->Pool->SystemSurface && !BltSurface && FAILED(h=ti->Texture->Load(ti->Pool->SystemTexture)) )
				appErrorf(TEXT("Failed Load: %s"),*D3DError(h));
			ti->Filler->EndUpload( this, ti, Info, PolyFlags );
			unclock(Cycles);
			ti->Filler->PixelFormat->UploadCycles += Cycles;
			if( Thrash )
			{
				Stats.ThrashTime+=Cycles;
				Stats.Thrashes++;
				if( Info.USize>Stats.ThrashX )
					Stats.ThrashX=Info.USize,Stats.ThrashY=Info.VSize;
			}

			// Unload texture.
			Info.bRealtimeChanged = 0;
			if( ti->Unloaded )
				Info.Unload();
		}
		if( Precache )
			return;

		// Set texture stage precomputes.
		Stages[dwStage].UScale			= ti->UScale;
		Stages[dwStage].VScale			= ti->VScale;
		Stages[dwStage].UIndex			= ti->UIndex;
		Stages[dwStage].VIndex			= ti->VIndex;
		Stages[dwStage].MaxColor		= ti->MaxColor.Plane();
		Stages[dwStage].TextureCacheID	= Info.CacheID;

		// Update texture info.
		if( ti->FrameCounter!=FrameCounter )
		{
			ti->Filler->PixelFormat->Active++;
			ti->Filler->PixelFormat->ActiveRAM += Info.USize * Info.VSize * ti->Filler->PixelFormat->BitsPerPixel / 8;
		}
		ti->FrameCounter = FrameCounter;
		ti->SubCounter   = SubCounter++;
		ti->Filler->PixelFormat->Sets++;

		// Set Direct3D state.
		Direct3DDevice->SetTexture( dwStage, ti->Texture );
		UBOOL UseMips = UseMipmapping && Info.NumMips>1;
		if( UseMips!=Stages[dwStage].UseMips )
		{
			Stages[dwStage].UseMips = UseMips;
			Direct3DDevice->SetTextureStageState( dwStage, D3DTSS_MIPFILTER, UseMips==0 ? D3DTFP_NONE : UseTrilinear ? D3DTFP_LINEAR : D3DTFP_POINT  );
		}
		unguard;
	}
	static BOOL CALLBACK EnumDirectDrawsCallback( GUID* Guid, char* Description, char* Name, void* Context )
	{
		FGuid NoGuid(0,0,0,0);
		new(*(TArray<FDeviceInfo>*)Context)FDeviceInfo( Guid?*Guid:*(GUID*)&NoGuid, Description?appFromAnsi(Description):TEXT(""), Name?appFromAnsi(Name):TEXT("") );
		debugf(NAME_Init,TEXT("   %s (%s)"), Name?appFromAnsi(Name):TEXT("None"), Description?appFromAnsi(Description):TEXT("None") );
		return 1;
	}
	static HRESULT CALLBACK EnumZBufferCallback( DDPIXELFORMAT* PixelFormat, void* Context )
	{
		if( PixelFormat->dwFlags & DDPF_ZBUFFER )
			((TArray<DDPIXELFORMAT>*)Context)->AddItem( *PixelFormat );
		return D3DENUMRET_OK;
	}
	static HRESULT CALLBACK EnumDevicesCallback( GUID* Guid, char* lpDeviceDescription, char* lpDeviceName, D3DDEVICEDESC* D3DHWDeviceDesc, D3DDEVICEDESC* lpD3DHELDeviceDesc, void* Context )
	{
		if( D3DHWDeviceDesc )
			new(*(TArray<D3DDEVICEDESC>*)Context)D3DDEVICEDESC(*D3DHWDeviceDesc);
		return D3DENUMRET_OK;
	}
	void RecognizePixelFormat( FPixFormat& Dest, const DDPIXELFORMAT& FromD3D, INT InBitsPerPixel, const TCHAR* InDesc,DWORD MaxWidth, DWORD MaxHeight )
	{
		guard(UDirect3DRenderDevice::RecognizePixelFormat);
		Dest.Init();
		Dest.pf              = FromD3D;
		Dest.MaxWidth		 = Min(DeviceDesc.dwMaxTextureWidth,MaxWidth);
		Dest.MaxHeight		 = Min(DeviceDesc.dwMaxTextureHeight,MaxHeight);
		Dest.Desc		     = InDesc;
		Dest.BitsPerPixel	 = InBitsPerPixel;
		Dest.Next            = FirstPixelFormat;
		FirstPixelFormat     = &Dest;
		unguard;
	}
	UBOOL SetRes( INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen )
	{
		guard(UD3DRenderDevice::SetRes);
		UBOOL LocalUse3dfx = Use3dfx;
		DescFlags=0;
		Description=TEXT("");
	Retry:

		// If D3D already inited, uninit it now.
		UnSetRes(NULL,0);

		// Initialize COM.
		if( FAILED(h=CoInitialize(NULL)) )
			return UnSetRes(TEXT("CoInitialize failed"),h);
		DidCoInitialize = 1;

		// Dynamically load the DLL.
		DD_CREATE_FUNC ddCreateFunc;
		DD_ENUM_FUNC ddEnumFunc;
		guard(DynamicallyLoadDLL);
		HINSTANCE Instance = LoadLibraryX(TEXT("ddraw.dll"));
		if( Instance == NULL )
			return UnSetRes(TEXT("DirectDraw not installed"),0);
		ddCreateFunc = (DD_CREATE_FUNC)GetProcAddress( Instance, "DirectDrawCreate"     );
		ddEnumFunc   = (DD_ENUM_FUNC  )GetProcAddress( Instance, "DirectDrawEnumerateA" );
		if( !ddCreateFunc || !ddEnumFunc )
			return UnSetRes(TEXT("DirectDraw GetProcAddress failed"),0);
		unguard;

		// Enumerate DirectDraw devices.
		guard(EnumDirectDraws);
		DirectDraws.Empty();
		debugf(NAME_Init,TEXT("DirectDraw drivers detected:"));
		ddEnumFunc( EnumDirectDrawsCallback, &DirectDraws );//Locks up on Dave Ewing's machine!!
		if( !DirectDraws.Num() )
			return UnSetRes(TEXT("No DirectDraw drivers found"),0);
		unguard;

		// Find best DirectDraw driver.
		FDeviceInfo Best = DirectDraws(0);
		guard(FindBestDriver);
		for( TArray<FDeviceInfo>::TIterator It(DirectDraws); It; ++It )
			if( It->Description.InStr(TEXT("Primary"))!=-1 )
				Best = *It;
		if( LocalUse3dfx )
			for( TArray<FDeviceInfo>::TIterator It(DirectDraws); It; ++It )
				if( It->Description.InStr(TEXT("Primary"))==-1 )
					Best = *It;
		unguard;

		// Create DirectDraw.
		guard(CreateDirectDraw);
		IDirectDraw* DirectDraw1=NULL;
		if( FAILED(h=ddCreateFunc(&Best.Guid,&DirectDraw1,NULL)) )
			return UnSetRes(TEXT("DirectDrawCreate failed"),h);
		h=DirectDraw1->QueryInterface(IID_IDirectDraw4,(void**)&DirectDraw);
		DirectDraw1->Release();
		if( FAILED(h) )
			return UnSetRes(TEXT("IDirectDraw failed query for IDirectDraw4"),h);
		unguard;

		// Check windowed rendering.
		guard(CheckWindowedCaps);
		DECLARE_INITED(DDCAPS,DDDriverCaps);
		DECLARE_INITED(DDCAPS,DDHelCaps);
		if( FAILED(h=DirectDraw->GetCaps(&DDDriverCaps,&DDHelCaps)) )
			return UnSetRes(TEXT("IDirectDraw failed GetCaps"),h);
		if( !(DDDriverCaps.dwCaps2 & DDCAPS2_CANRENDERWINDOWED) )
		{
			debugf(NAME_Init,TEXT("D3D Device: Fullscreen only"));
			FullscreenOnly = 1;
			if( Viewport && !Fullscreen )
				return UnSetRes(TEXT("Can't render to window"),h);
		}
		if( DDDriverCaps.dwCaps & DDCAPS_CANBLTSYSMEM )
			debugf(NAME_Init,TEXT("D3D Device: Supports system memory DMA blts"));
		if( DDDriverCaps.dwCaps2 & DDCAPS2_NOPAGELOCKREQUIRED )
			debugf(NAME_Init,TEXT("D3D Device: No page lock required"));
		debugf( TEXT("D3D Device %iK vram, %iK free"), DDDriverCaps.dwVidMemTotal/1024, DDDriverCaps.dwVidMemFree/1024 );
		unguard;

		// Get device identifier.
		// szDriver, szDescription aren't guaranteed consistent (might change by mfgr, distrubutor, language, etc). Don't do any compares on these.
		// liDriverVersion is safe to do QWORD comparisons on.
		// User has changed drivers/cards iff guidDeviceIdentifier changes.
		guard(GetDeviceIdentifier);
		appMemzero(&DeviceIdentifier,sizeof(DeviceIdentifier));
		if( FAILED(h=DirectDraw->GetDeviceIdentifier(&DeviceIdentifier,0)) )
			return UnSetRes(TEXT("GetDeviceIdentifier failed"),h);
		debugf(NAME_Init,TEXT("D3D Device: szDriver=%s"),      appFromAnsi(DeviceIdentifier.szDriver));
		debugf(NAME_Init,TEXT("D3D Device: szDescription=%s"), appFromAnsi(DeviceIdentifier.szDescription));
		debugf(NAME_Init,TEXT("D3D Device: wProduct=%i"),      wProduct=HIWORD(DeviceIdentifier.liDriverVersion.HighPart));
		debugf(NAME_Init,TEXT("D3D Device: wVersion=%i"),      wVersion=LOWORD(DeviceIdentifier.liDriverVersion.HighPart));
		debugf(NAME_Init,TEXT("D3D Device: wSubVersion=%i"),   wSubVersion=HIWORD(DeviceIdentifier.liDriverVersion.LowPart));
		debugf(NAME_Init,TEXT("D3D Device: wBuild=%i"),        wBuild=LOWORD(DeviceIdentifier.liDriverVersion.LowPart));
		debugf(NAME_Init,TEXT("D3D Device: dwVendorId=%i"),    DeviceIdentifier.dwVendorId);
		debugf(NAME_Init,TEXT("D3D Device: dwDeviceId=%i"),    DeviceIdentifier.dwDeviceId);
		debugf(NAME_Init,TEXT("D3D Device: dwSubSysId=%i"),    DeviceIdentifier.dwSubSysId);
		debugf(NAME_Init,TEXT("D3D Device: dwRevision=%i"),    DeviceIdentifier.dwRevision);
		Description = appFromAnsi(DeviceIdentifier.szDescription);
		unguard;

		// Get list of display modes.
		guard(EnumDisplayModes);
		DECLARE_INITED(DDSURFACEDESC2,SurfaceDesc);
		SurfaceDesc.dwFlags        = DDSD_CAPS;
		SurfaceDesc.ddsCaps.dwCaps = DDSCAPS_3DDEVICE;
		DisplayModes.Empty();
		DirectDraw->EnumDisplayModes( 0, &SurfaceDesc, &DisplayModes, EnumModesCallback );
		unguard;

		// Get IDirect3D3.
		guard(GetDirect3D);
		if( FAILED(h=DirectDraw->QueryInterface(IID_IDirect3D3,(void**)&Direct3D)) )
			return UnSetRes(TEXT("IDirectDraw4 failed query for IDirect3D3"),h);
		unguard;

		// Exit if just testing.
		if( Viewport==NULL )
		{
			dwVendorId=DeviceIdentifier.dwVendorId;
			dwDeviceId=DeviceIdentifier.dwDeviceId;
			TArray<D3DDEVICEDESC> HWDevices;
			Direct3D->EnumDevices( EnumDevicesCallback, &HWDevices );
			UBOOL A=0, C=0;
			{for( TArray<DDSURFACEDESC2>::TIterator It(DisplayModes); It; ++It )
			{
				A = A || (It->ddpfPixelFormat.dwRGBBitCount>=24);
			}}
			{for( TArray<D3DDEVICEDESC>::TIterator It(HWDevices); It; ++It )
				C+=(It->wMaxSimultaneousTextures>=2);}
			if( A && C && DeviceIdentifier.dwVendorId!=4638 )
			{
				DescFlags |= RDDESCF_Certified;
			}
			if
			(	(DeviceIdentifier.dwVendorId==4318 && DeviceIdentifier.dwDeviceId<=32)  // NVidia Riva TNT1
			||	(DeviceIdentifier.dwVendorId==4634 && DeviceIdentifier.dwDeviceId<=1 )) // 3dfx Voodoo1
				DescFlags |= RDDESCF_LowDetailWorld|RDDESCF_LowDetailSkins;
			SaveConfig();
			if( DeviceIdentifier.dwVendorId==4634 ) // Propagate 3dfx settings to Glide tab.
				GConfig->SetString(TEXT("GlideDrv.GlideRenderDevice"),TEXT("DescFlags"),*FString::Printf(TEXT("%i"),DescFlags|RDDESCF_Certified));
			UnSetRes(TEXT("Successfully tested Direct3D presence"),0);
			if( !LocalUse3dfx )
			{
				LocalUse3dfx = 1;
				goto Retry;
			}
			return 1;
		}

		// Remember parameters.
		ViewporthWnd       = (HWND)Viewport->GetWindow();
		ViewportX          = Min( NewX, MaxResWidth  );
		ViewportY          = Min( NewY, MaxResHeight );
		ViewportFullscreen = Fullscreen;
		ViewportColorBits  = NewColorBytes * 8;

		// Set cooperative level.
		if( FAILED(h=DirectDraw->SetCooperativeLevel(ViewportFullscreen?ViewporthWnd:NULL, DDSCL_FPUSETUP | (ViewportFullscreen ? (DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN|DDSCL_ALLOWREBOOT) : DDSCL_NORMAL))) )
			return UnSetRes(TEXT("SetCooperativeLevel failed"),h);
		DidSetCooperativeLevel = 1;

		// See if the window is full screen.
		if( ViewportFullscreen )
		{
			if( DisplayModes.Num()==0 )
				return UnSetRes(TEXT("No fullscreen display modes found"),0);

			// Find matching display mode.
			guard(FindBestMatchingMode);
			INT BestError = MAXINT;
			DECLARE_INITED(DDSURFACEDESC2,Best);
			for( TArray<DDSURFACEDESC2>::TIterator It(DisplayModes); It; ++It )
			{
				INT ThisError
				=	Abs((INT)It->dwWidth -(INT)ViewportX)
				+	Abs((INT)It->dwHeight-(INT)ViewportY)
				+	Abs((INT)It->ddpfPixelFormat.dwRGBBitCount-(INT)ViewportColorBits);
				if
				(	(ThisError<BestError)
				&&	(It->ddpfPixelFormat.dwRGBBitCount==16 || It->ddpfPixelFormat.dwRGBBitCount==24 || It->ddpfPixelFormat.dwRGBBitCount==32)
				&&	(It->ddpfPixelFormat.dwFlags & DDPF_RGB) )
				{
					Best = *It;
					BestError = ThisError;
				}
			}
			if( BestError==MAXINT )
				return UnSetRes(TEXT("No acceptable display modes found"),0);
			ViewportColorBits = Best.ddpfPixelFormat.dwRGBBitCount;
			ViewportX         = Best.dwWidth;
			ViewportY         = Best.dwHeight;
			debugf(NAME_Init,TEXT("Best-match display mode: %ix%ix%i (Error=%i)"),Best.dwWidth,Best.dwHeight,Best.ddpfPixelFormat.dwRGBBitCount,BestError);
			unguard;

			// Set display mode.
			guard(SetDisplayMode);
			if( FAILED(h=DirectDraw->SetDisplayMode(ViewportX,ViewportY,ViewportColorBits,0,0)) )
				return UnSetRes(TEXT("IDirectDraw4 failed query for IDirect3D3"),h);
			DidSetDisplayMode = 1;
			unguard;
		}

		// Decide here which IDirect3DDevice we will later create.
		//or IID_IDirect3DMMXDevice or IID_IDirect3DRGBDevice depending on options.
		GUID Direct3DDeviceGuid = IID_IDirect3DHALDevice;

		// Create primary and back IDirectDrawSurface4.
		DECLARE_INITED(DDSURFACEDESC2,FrontSurfaceDesc);
		FrontSurfaceDesc.dwFlags                 = DDSD_CAPS | (ViewportFullscreen?DDSD_BACKBUFFERCOUNT:0);
		FrontSurfaceDesc.dwBackBufferCount		 = (!ViewportFullscreen) ? 0 : UseTripleBuffering ? 2 : 1;
		FrontSurfaceDesc.ddsCaps.dwCaps          = DDSCAPS_PRIMARYSURFACE | DDSCAPS_3DDEVICE | (ViewportFullscreen?(DDSCAPS_COMPLEX|DDSCAPS_FLIP):0);
		RetryCreate:
		if( FAILED(h=DirectDraw->CreateSurface(&FrontSurfaceDesc,&PrimarySurface,NULL)) )
		{
			if( FrontSurfaceDesc.dwBackBufferCount==2 )
			{
				debugf(NAME_Init,TEXT("CreateSurface failed for primary triple-buffer, trying double-buffer"));
				FrontSurfaceDesc.dwBackBufferCount--;
				goto RetryCreate;
			}
			return UnSetRes(TEXT("CreateSurface failed for primary"),h);
		}

		// Create the back buffer.
		if( ViewportFullscreen )
		{
			// Get the surface directly attached to the primary (the back buffer).
			DDSCAPS2 Caps2={DDSCAPS_BACKBUFFER,0,0,0};
			if( FAILED(h=PrimarySurface->GetAttachedSurface(&Caps2,&BackSurface)) )
				return UnSetRes(TEXT("GetAttachedSurface failed for back"),h);
		}
		else
		{
			// Create back surface.
			DECLARE_INITED(DDSURFACEDESC2,BackSurfaceDesc);
			BackSurfaceDesc.dwFlags        = DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS;
			BackSurfaceDesc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_3DDEVICE;
			BackSurfaceDesc.dwWidth        = Max(32,ViewportX);
			BackSurfaceDesc.dwHeight	   = Max(32,ViewportY);
 			if( FAILED(h=DirectDraw->CreateSurface(&BackSurfaceDesc,&BackSurface,NULL)) )
				return UnSetRes(TEXT("CreateSurface failed for back"),h);

			// Create clipper.
			if( FAILED(h=DirectDraw->CreateClipper(0,&Clipper,NULL)) )
				return UnSetRes(TEXT("CreateClipper failed"),h);
			Clipper->SetHWnd( 0, ViewporthWnd );
			PrimarySurface->SetClipper( Clipper );
			Clipper->Release();
		}

		// Always use 32-bit z-buffering?
		UBOOL AlwaysUseGoodZ=0;
		if( DeviceIdentifier.dwVendorId==4139 && DeviceIdentifier.dwDeviceId>=1317 ) // Matrox G400 or better.
			AlwaysUseGoodZ=1;

		// Enumerate z-buffers. Must do AFTER creating back bufer and BEFORE creating IDirect3DDevice.
		guard(GetZBuffer);
		ZFormats.Empty();
		Direct3D->EnumZBufferFormats( Direct3DDeviceGuid, EnumZBufferCallback, &ZFormats );
		if( ZFormats.Num()==0 )
			return UnSetRes(TEXT("No z-buffer formats found"),0);
		DDPIXELFORMAT BestZFormat = ZFormats(0);
		for( TArray<DDPIXELFORMAT>::TIterator It(ZFormats); It; ++It )
			if( It->dwZBufferBitDepth==16 )
				BestZFormat = *It;
		if( ViewportColorBits>16 || AlwaysUseGoodZ )
			for( TArray<DDPIXELFORMAT>::TIterator It(ZFormats); It; ++It )
				if( It->dwZBufferBitDepth==24 )
					BestZFormat = *It;
		if( ViewportColorBits>16 || AlwaysUseGoodZ )
			for( TArray<DDPIXELFORMAT>::TIterator It(ZFormats); It; ++It )
				if( It->dwZBufferBitDepth==32 )
					BestZFormat = *It;
 		DECLARE_INITED(DDSURFACEDESC2,ZSurfaceDesc);
		ZSurfaceDesc.dwFlags         = DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;
		ZSurfaceDesc.ddsCaps.dwCaps  = DDSCAPS_ZBUFFER;
		ZSurfaceDesc.dwWidth         = Max(32,ViewportX);
		ZSurfaceDesc.dwHeight        = Max(32,ViewportY);
		ZSurfaceDesc.ddpfPixelFormat = BestZFormat;
		ZSurfaceDesc.ddsCaps.dwCaps |= IsEqualIID(Direct3DDeviceGuid,IID_IDirect3DHALDevice) ? DDSCAPS_VIDEOMEMORY : DDSCAPS_SYSTEMMEMORY; // Software devices require system-memory depth buffers.
		if( FAILED(h=DirectDraw->CreateSurface(&ZSurfaceDesc,&ZSurface,NULL)) )
			return UnSetRes(TEXT("CreateSurface failed for Z"),h);
		if( FAILED(h=BackSurface->AddAttachedSurface(ZSurface)) )
			return UnSetRes(TEXT("AddAttachedSurface failed for Z"),h);
		unguard;
 
		// Create IDirect3DDevice.
		guard(CreateIDirect3DDevice);
		if( FAILED(h=Direct3D->CreateDevice( IID_IDirect3DHALDevice, BackSurface, &Direct3DDevice, NULL )) )
			return UnSetRes(TEXT("CreateDevice failed"),h);
		unguard;

		// Create IDirect3DViewport3.
		guard(CreateIDirect3DViewport3);
		DECLARE_INITED(D3DVIEWPORT2,ViewportInfo);
		ViewportInfo.dwWidth      = ViewportX;
		ViewportInfo.dwHeight     = ViewportY;
		ViewportInfo.dvClipX      = -1.0f;
		ViewportInfo.dvClipWidth  = 2.0f;
		ViewportInfo.dvClipY      = 1.0f;
		ViewportInfo.dvClipHeight = 2.0f;
		ViewportInfo.dvMinZ       = 1.0f;
		ViewportInfo.dvMaxZ       = 32767.0f;
 		if( FAILED(h=Direct3D->CreateViewport(&Direct3DViewport,NULL)) )
			return UnSetRes(TEXT("CreateViewport failed"),h);
		verify(!FAILED(Direct3DDevice->AddViewport(Direct3DViewport)));
		verify(!FAILED(Direct3DViewport->SetViewport2(&ViewportInfo)));
		verify(!FAILED(Direct3DDevice->SetCurrentViewport(Direct3DViewport)));
		unguard;

		// Keep a copy of the device capabilities.
		DECLARE_INITED(D3DDEVICEDESC,ddHELDesc);
		DECLARE_INITED(D3DDEVICEDESC,ddHALDesc);
		guard(GetDirect3DDeviceCaps);
		Direct3DDevice->GetCaps( &ddHALDesc, &ddHELDesc );
		DeviceDesc = ddHALDesc.dwFlags ? ddHALDesc : ddHELDesc;
		unguard;

		// Get DirectDraw caps.
		guard(GetDirectDrawCaps);
		appMemzero( &DirectDrawCaps, sizeof(DirectDrawCaps) );
		DECLARE_INITED(DDCAPS,ddHALCaps);
		DECLARE_INITED(DDCAPS,ddHELCaps);
		if( !FAILED(DirectDraw->GetCaps(&ddHALCaps, &ddHELCaps)) )
			DirectDrawCaps = ddHALDesc.dwFlags ? ddHALCaps : ddHELCaps;
		unguard;

		// Check multitexture caps.
		debugf(NAME_Init,TEXT("D3D Driver: wMaxTextureBlendStages=%i"),DeviceDesc.wMaxTextureBlendStages);
		debugf(NAME_Init,TEXT("D3D Driver: wMaxSimultaneousTextures=%i"),DeviceDesc.wMaxSimultaneousTextures);
		if( UseMultitexture && (DeviceDesc.wMaxSimultaneousTextures<2 || !(DeviceDesc.dwTextureOpCaps & D3DTEXOPCAPS_MODULATE2X)) )
		{
			UseMultitexture = 0;
			debugf(NAME_Init, TEXT("D3D Driver: Multitexturing not available with this driver"));
		}

		// Handle the texture formats we need.
		guard(InitTextureFormats);
		{
			// Zero them all.
			Format8888.Init();
			Format1555.Init();
			FormatDXT1.Init();
			FormatP8  .Init();

			// Enumerate all texture formats.
			guard(EnumTextureFormats);
			PixelFormats.Empty();
			Direct3DDevice->EnumTextureFormats( EnumPixelFormatsCallback, (void*)&PixelFormats );
			unguard;

			// Find the needed texture formats.
			guard(FindPixelFormats);
			FirstPixelFormat = NULL;
			for( TArray<DDPIXELFORMAT>::TIterator It(PixelFormats); It; ++It )
			{
				if
				(   (It->dwFlags & DDPF_ALPHAPIXELS    )
				&&	(It->dwFlags & DDPF_RGB            )
				&&	(It->dwRGBBitCount==16             )
				&&	(It->dwRGBAlphaBitMask==0x8000     )
				&&	(It->dwRBitMask       ==0x7C00     )
				&&	(It->dwGBitMask       ==0x03E0     )
				&&	(It->dwBBitMask       ==0x001f     ) )
					RecognizePixelFormat( Format1555, *It, 16, TEXT("1555"), 256, 256 );
				/*if
				(	(It->dwFlags & DDPF_RGB            )
				&&	(It->dwFlags & DDPF_ALPHAPIXELS    )
				&&	(It->dwRGBBitCount    ==32         )
				&&	(It->dwRGBAlphaBitMask==0xff000000 )
				&&	(It->dwRBitMask       ==0x00ff0000 )
				&&	(It->dwGBitMask       ==0x0000ff00 )
				&&	(It->dwBBitMask       ==0x000000ff ) )
					RecognizePixelFormat( Format8888, *It, 32, TEXT("8888") );*/
				/*if
				(	(It->dwFlags & DDPF_FOURCC)
				&&	(It->dwFourCC == FOURCC_DXT1       ) )
					RecognizePixelFormat( FormatDXT1, *It, 8, TEXT("DXT1") );*/
				/*if( (It->dwFlags & DDPF_PALETTEINDEXED8) )
					RecognizePixelFormat( FormatP8, *It, 8, TEXT("P8") );*/
				/*if( It->dwFlags & DDPF_FOURCC )
				{
					TCHAR Code[5]=TEXT("XXXX");
					Code[0] = (ANSICHAR)(It->dwFourCC/0x00000001);
					Code[1] = (ANSICHAR)(It->dwFourCC/0x00000100);
					Code[2] = (ANSICHAR)(It->dwFourCC/0x00010000);
					Code[3] = (ANSICHAR)(It->dwFourCC/0x01000000);
					debugf(TEXT("D3D Driver: Supports FOURCC %s"),Code);
				}*/
			}
			unguard;

			// Make sure all formats were found.
			guard(VerifyTextureFormats);
			if( FormatDXT1.pf.dwSize )
				debugf( NAME_Init, TEXT("D3D Driver: Format DXT1 available") );
			//if( FormatP8.dwSize )
			//	debugf( NAME_Init, TEXT("D3D Driver: Supports paletted textures") );
			else UsePalettes = 0;
			if( !Format8888.pf.dwSize )
				debugf( NAME_Init, TEXT("D3D Driver: Format8888 not available") );
			if( !Format1555.pf.dwSize )
				return UnSetRes(TEXT("D3D Driver: Format1555 not found"),0);
			unguard;
		}
		unguard;

		// Hardware-specific initialization and workarounds for driver bugs.
		UBOOL ConstrainAspect = 1;
		if( ParseParam(appCmdLine(),TEXT("nodeviceid")) )
		{
			debugf(NAME_Init,TEXT("D3D Detected: -nodeviceid specified, 3D device identification skipped"));
		}
		else if( DeviceIdentifier.dwVendorId==4098 )
		{
			debugf(NAME_Init,TEXT("D3D Detected: ATI video card"));
			if( DeviceIdentifier.dwDeviceId==18242 )
			{
				debugf(NAME_Init,TEXT("D3D Detected: ATI Rage Pro"));
				UsePalettes = 0; // Both stages must use same texture format for multitexturing to function.
			}
			else if( DeviceIdentifier.dwDeviceId==21062 )
			{
				debugf(NAME_Init,TEXT("D3D Detected: ATI Rage 128"));
			}
		}
		else if( DeviceIdentifier.dwVendorId==4634 )
		{
			debugf(TEXT("D3D Detected: 3dfx video card"));
			if( DeviceIdentifier.dwDeviceId==1 )
			{
				debugf(NAME_Init,TEXT("D3D Detected: 3dfx Voodoo"));
				MaxResWidth  = 640;
				MaxResHeight = 480;
			}
			else if( DeviceIdentifier.dwDeviceId==2 )
			{
				debugf(NAME_Init,TEXT("D3D Detected: 3dfx Voodoo2"));
				MaxResWidth  = 800;
				MaxResHeight = 600;
			}
			else if( DeviceIdentifier.dwDeviceId==3 )
			{
				debugf(NAME_Init,TEXT("D3D Detected: 3dfx Voodoo Banshee"));
			}
			else if( DeviceIdentifier.dwDeviceId==5 )
			{
				debugf(NAME_Init,TEXT("D3D Detected: 3dfx Voodoo3"));
			}
			UseVertexSpecular = 0; // Driver lies about support for this.
			DeviceDesc.dwMaxTextureAspectRatio = 8; // Driver lies in its aspect-ratio caps.
		}
		else if( DeviceIdentifier.dwVendorId==32902 )
		{
			debugf(NAME_Init,TEXT("D3D Detected: Intel video card"));
			if( DeviceIdentifier.dwDeviceId==30720 )
			{
				debugf(TEXT("D3D Detected: Intel i740"));
			}
		}
		else if( DeviceIdentifier.dwVendorId==4318 )
		{
			debugf(NAME_Init,TEXT("D3D Detected: NVidia video card"));
			if( DeviceIdentifier.dwDeviceId==32 )
			{
				debugf(NAME_Init,TEXT("D3D Detected: Riva TNT"));
				ConstrainAspect = 0;
			}
			if( DeviceIdentifier.dwDeviceId==40 )
			{
				debugf(NAME_Init,TEXT("D3D Detected: Riva TNT2"));
				ConstrainAspect = 0;
			}
			ConstrainAspect = 0;
		}
		else if( DeviceIdentifier.dwVendorId==4139 )
		{
			debugf(NAME_Init,TEXT("D3D Detected: Matrox video card"));
			if( DeviceIdentifier.dwDeviceId==1313 )
				debugf(NAME_Init,TEXT("D3D Detected: Matrox G200"));
			else if( DeviceIdentifier.dwDeviceId==1317 )
				debugf(NAME_Init,TEXT("D3D Detected: Matrox G400"));
			//G400 lies about texture stages, last one is for bump only
		}
		else
		{
			debugf(NAME_Init,TEXT("D3D Detected: Generic 3D accelerator"));
		}

		// Setup projection matrix.
		FLOAT wNear=1.f, wFar=32767.f;
		ProjectionMatrix._33 = wFar / (wFar - wNear);
		ProjectionMatrix._34 = 1.f;
		ProjectionMatrix._43 = -ProjectionMatrix._33 * wNear;
		ProjectionMatrix._44 = 0.f;
		Direct3DDevice->SetTransform( D3DTRANSFORMSTATE_PROJECTION, &ProjectionMatrix );

		// Verify mipmapping supported.
		if
		(	UseMipmapping
		&&	!(DeviceDesc.dpcTriCaps.dwTextureFilterCaps & D3DPTFILTERCAPS_LINEARMIPLINEAR)
		&&	!(DeviceDesc.dpcTriCaps.dwTextureFilterCaps & D3DPTFILTERCAPS_LINEARMIPNEAREST)
		&&	!(DeviceDesc.dpcTriCaps.dwTextureFilterCaps & D3DPTFILTERCAPS_MIPLINEAR)
		&&	!(DeviceDesc.dpcTriCaps.dwTextureFilterCaps & D3DPTFILTERCAPS_MIPNEAREST) )
		{
			UseMipmapping = 0;
			debugf(NAME_Init, TEXT("D3D Driver: Mipmapping not available with this driver"));
		}
		else
		{
			if( DeviceDesc.dpcTriCaps.dwTextureFilterCaps & D3DPTFILTERCAPS_LINEARMIPLINEAR )
				debugf( NAME_Init, TEXT("D3D Driver: Supports trilinear"));
			else
				UseTrilinear = 0;
		}

		// Check caps.
		if( DeviceDesc.dpcTriCaps.dwShadeCaps & D3DPSHADECAPS_SPECULARGOURAUDRGB )
 			debugf( NAME_Init, TEXT("D3D Driver: Supports specular gouraud") );
		else UseVertexSpecular = 0;
		if( DeviceDesc.dwTextureOpCaps & D3DTEXOPCAPS_BLENDDIFFUSEALPHA )
			debugf( NAME_Init, TEXT("D3D Driver: Supports BLENDDIFFUSEALPHA") );
		else DetailTextures = 0;
		if( DeviceDesc.dpcTriCaps.dwTextureCaps  & D3DPTEXTURECAPS_ALPHAPALETTE )
 			debugf( NAME_Init, TEXT("D3D Driver: Supports alpha palettes") );
		else
			UseAlphaPalettes = 0;
		if( DeviceDesc.dpcTriCaps.dwTextureCaps & D3DPTEXTURECAPS_SQUAREONLY )
 			DeviceDesc.dwMaxTextureAspectRatio = 1, debugf( NAME_Init, TEXT("D3D Driver: Requires square textures") );
		else if( !DeviceDesc.dwMaxTextureAspectRatio )
			DeviceDesc.dwMaxTextureAspectRatio = Max<INT>(1,Max<INT>(DeviceDesc.dwMaxTextureWidth,DeviceDesc.dwMaxTextureHeight));
		if( !(DeviceDesc.dpcTriCaps.dwRasterCaps & D3DPTEXTURECAPS_POW2) )
 			debugf( NAME_Init, TEXT("D3D Driver: Supports non-power-of-2 textures") );
		if( DeviceDesc.dpcTriCaps.dwRasterCaps & D3DPRASTERCAPS_MIPMAPLODBIAS )
 			debugf( NAME_Init, TEXT("D3D Driver: Supports LOD biasing") );
		if( DeviceDesc.dpcTriCaps.dwRasterCaps & D3DPRASTERCAPS_ZBIAS )
 			debugf( NAME_Init, TEXT("D3D Driver: Supports Z biasing") );
		if( DeviceDesc.dwTextureOpCaps & D3DTEXOPCAPS_ADDSIGNED2X )
 			debugf( NAME_Init, TEXT("D3D Driver: Supports D3DTOP_ADDSIGNED2X") );
		if( DeviceDesc.dwTextureOpCaps & D3DTEXOPCAPS_BUMPENVMAP )
 			debugf( NAME_Init, TEXT("D3D Driver: Supports D3DTOP_BUMPENVMAP") );
		if( DeviceDesc.dwTextureOpCaps & D3DTEXOPCAPS_BUMPENVMAPLUMINANCE )
 			debugf( NAME_Init, TEXT("D3D Driver: Supports D3DTOP_BUMPENVMAPLUMINANCE") );
		if( DeviceDesc.dwTextureOpCaps & D3DTEXOPCAPS_DOTPRODUCT3 )
 			debugf( NAME_Init, TEXT("D3D Driver: Supports D3DTOP_DOTPRODUCT3") );
		if( DeviceDesc.dwTextureOpCaps & D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR )
 			debugf( NAME_Init, TEXT("D3D Driver: Supports D3DTOP_MODULATEALPHA_ADDCOLOR") );
		if( DeviceDesc.dwTextureOpCaps & D3DTEXOPCAPS_MODULATECOLOR_ADDALPHA )
 			debugf( NAME_Init, TEXT("D3D Driver: Supports D3DTOP_MODULATECOLOR_ADDALPHA") ); 
		debugf( NAME_Init, TEXT("D3D Driver: Textures (%ix%i)-(%ix%i), Max aspect %i"), DeviceDesc.dwMinTextureWidth, DeviceDesc.dwMinTextureHeight, DeviceDesc.dwMaxTextureWidth, DeviceDesc.dwMaxTextureHeight, DeviceDesc.dwMaxTextureAspectRatio );

		// Depth buffering.
		Direct3DDevice->SetRenderState( D3DRENDERSTATE_ZENABLE, TRUE );
		if( DeviceDesc.dpcTriCaps.dwRasterCaps & D3DPRASTERCAPS_WBUFFER )
		{
			Direct3DDevice->SetRenderState( D3DRENDERSTATE_ZENABLE, D3DZB_USEW );
			debugf( NAME_Init, TEXT("D3D Driver: Supports w-buffering") );
		}

		// See if device is able to support Gamma control.
		guard(InitGammaControl);
		GammaControl = 0;
		if( DirectDrawCaps.dwCaps2 & DDCAPS2_PRIMARYGAMMA )
			PrimarySurface->QueryInterface(IID_IDirectDrawGammaControl, (void**)&GammaControl);
		if( !GammaControl )
			debugf( NAME_Init, TEXT("D3D Driver: Gamma control not available. Brightness adjustment won't work.") );
		unguard;

		// Init render states.
		guard(InitRenderState);
		{
			FLOAT LodBias=-0.5;
			Direct3DDevice->SetRenderState( D3DRENDERSTATE_MIPMAPLODBIAS, *(DWORD*)&LodBias );
			Direct3DDevice->SetRenderState( D3DRENDERSTATE_SHADEMODE, D3DSHADE_GOURAUD );
			Direct3DDevice->SetRenderState( D3DRENDERSTATE_TEXTUREPERSPECTIVE, TRUE );
			Direct3DDevice->SetRenderState( D3DRENDERSTATE_SPECULARENABLE, FALSE );
			Direct3DDevice->SetRenderState( D3DRENDERSTATE_DITHERENABLE, TRUE );
			Direct3DDevice->SetRenderState( D3DRENDERSTATE_CULLMODE, D3DCULL_NONE );
			Direct3DDevice->SetRenderState( D3DRENDERSTATE_ALPHAREF, 127 );
			Direct3DDevice->SetRenderState( D3DRENDERSTATE_ALPHAFUNC, D3DCMP_GREATER );
			Direct3DDevice->SetRenderState( D3DRENDERSTATE_ZBIAS, UNREAL_ZBIAS );
			Direct3DDevice->SetRenderState( D3DRENDERSTATE_ZFUNC, D3DCMP_LESSEQUAL );
			Direct3DDevice->SetRenderState( D3DRENDERSTATE_FOGCOLOR, 0 );        
			Direct3DDevice->SetRenderState( D3DRENDERSTATE_FOGTABLEMODE, D3DFOG_LINEAR );
			FLOAT FogStart=0.f, FogEnd = 65535.f;
			Direct3DDevice->SetRenderState( D3DRENDERSTATE_FOGTABLESTART, *(DWORD*)&FogStart );
			Direct3DDevice->SetRenderState( D3DRENDERSTATE_FOGTABLEEND, *(DWORD*)&FogEnd );
		}
		unguard;

		// Init texture stage state.
		guard(InitTextureStageState);
		{
			// Workaround Direct3D bug: D3D sets (texturemapblend, modulate) after setting the tss.
			Direct3DDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE );
			Direct3DDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

			// Set stage 0 state.
			Direct3DDevice->SetTextureStageState( 0, D3DTSS_ADDRESS,   D3DTADDRESS_WRAP );
			Direct3DDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			Direct3DDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			Direct3DDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
			Direct3DDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			Direct3DDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
			Direct3DDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );
			Direct3DDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTFG_LINEAR );
			Direct3DDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTFN_LINEAR );
			Direct3DDevice->SetTextureStageState( 0, D3DTSS_MIPFILTER, UseMipmapping ? (UseTrilinear ? D3DTFP_LINEAR : D3DTFP_POINT) : D3DTFP_NONE );
			Direct3DDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
			if( UseMultitexture )
			{
				// Set stage 1 state.
				Direct3DDevice->SetTextureStageState( 1, D3DTSS_ADDRESS,   D3DTADDRESS_WRAP );
				Direct3DDevice->SetTextureStageState( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE );
				Direct3DDevice->SetTextureStageState( 1, D3DTSS_COLORARG2, D3DTA_CURRENT );
				Direct3DDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
				Direct3DDevice->SetTextureStageState( 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
				Direct3DDevice->SetTextureStageState( 1, D3DTSS_ALPHAARG2, D3DTA_CURRENT );
				Direct3DDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
				Direct3DDevice->SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTFG_LINEAR );
				Direct3DDevice->SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTFN_LINEAR );
				Direct3DDevice->SetTextureStageState( 1, D3DTSS_MIPFILTER, UseMipmapping==0 ? D3DTFP_NONE : UseTrilinear ? D3DTFP_LINEAR : D3DTFP_POINT  );
				Direct3DDevice->SetTextureStageState( 1, D3DTSS_TEXCOORDINDEX, 1 );
			}
			for( INT i=0; i<DeviceDesc.wMaxSimultaneousTextures; i++ )
				Stages[i].UseMips = 1;
		}
		unguard;

		// For safety, lots of drivers don't handle tiny texture sizes too tell.
		DeviceDesc.dwMinTextureWidth       = Max<INT>( DeviceDesc.dwMinTextureWidth,       4 );
		DeviceDesc.dwMinTextureHeight      = Max<INT>( DeviceDesc.dwMinTextureHeight,      4 );
		DeviceDesc.dwMaxTextureAspectRatio = Min<INT>( DeviceDesc.dwMaxTextureAspectRatio, 2 );//8 is safe, this is just more efficient

		// Update the viewport.
		verify(Viewport->ResizeViewport( (ViewportFullscreen ? BLIT_Fullscreen : 0) | BLIT_Direct3D, NewX, ViewportY, ViewportColorBits / 8 ));
		Lock( FPlane(0,0,0,0), FPlane(0,0,0,0), FPlane(0,0,0,0), LOCKR_ClearScreen, NULL, NULL );
		Unlock( 1 );

		// Allocate pools and system-memory blt'ing surfaces.
		guard(AllocPools);
		VideoMemoryFull = 0;
		TotalMemory = 0;
		for( FPixFormat* Fmt=FirstPixelFormat; Fmt; Fmt=Fmt->Next )
		{
			for( DWORD USize=DeviceDesc.dwMinTextureWidth; USize<=Fmt->MaxWidth; USize*=2 )
			{
				for( DWORD VSize=DeviceDesc.dwMinTextureHeight; VSize<=Fmt->MaxHeight; VSize*=2 )
				{
					if( USize>256 || VSize>256 || USize<VSize )//!!cleanup
						 continue;
					FTexPool* Pool    = new FTexPool;
					Pool->PixelFormat = Fmt;
					Pool->USize       = USize;
					Pool->VSize       = VSize;
					Fmt->PoolList.Set( USize+VSize*65543, Pool );
					DECLARE_INITED(DDSURFACEDESC2,SysSurfaceDesc);
					SysSurfaceDesc.dwFlags         = DDSD_CAPS | DDSD_PIXELFORMAT | DDSD_WIDTH | DDSD_HEIGHT;
					SysSurfaceDesc.ddsCaps.dwCaps  = DDSCAPS_TEXTURE | DDSCAPS_MIPMAP | DDSCAPS_COMPLEX | DDSCAPS_SYSTEMMEMORY;
					SysSurfaceDesc.ddpfPixelFormat = Fmt->pf;
					SysSurfaceDesc.dwWidth         = USize;
					SysSurfaceDesc.dwHeight        = VSize;
					if( FAILED(h=DirectDraw->CreateSurface(&SysSurfaceDesc, &Pool->SystemSurface, NULL)) )
						appErrorf(TEXT("D3D Driver: CreateSurface for system failed: %s"),*D3DError(h));
					Pool->SystemSurface->QueryInterface( IID_IDirect3DTexture2, (void**)&Pool->SystemTexture );
					check(Pool->SystemTexture);
					Pool->SystemSurface->PageLock(0);
				}
			}
		}
		unguard;

		// Preallocate textures.
		guard(AllocTextures);
		INT MaxCount=16;
		for( INT Count=0; Count<MaxCount; Count++ )
		{
			for( FPixFormat* Fmt=FirstPixelFormat; Fmt; Fmt=Fmt->Next )
			{
				for( DWORD USize=DeviceDesc.dwMinTextureWidth; USize<=Fmt->MaxWidth; USize*=2 )
				{
					for( DWORD VSize=DeviceDesc.dwMinTextureHeight; VSize<=Fmt->MaxHeight; VSize*=2 )
					{
						INT SizeKey = USize+VSize*65543;
						FTexPool* Pool = Fmt->PoolList.FindRef(SizeKey);
						if( Pool )
						{
							INT Copies = (USize<=32 && VSize<=32) ? 3 : (USize==VSize) ? 2 : 1;
							Pool->DesiredSwapCopies = MaxCount*Copies;
							for( INT c=0; c<Copies && Pool->TexCount<Pool->DesiredSwapCopies; c++ )
								if( VideoMemoryFull || !CreateVideoTexture(Pool) )
									if( Count==0 )
										appErrorf(TEXT("Failed to preallocate initial textures, %ix%i: %s"),USize,VSize,*D3DError(h));
						}
					}
				}
			}
		}
		unguard;

		Flush();
		return 1;
		unguard;
	}
	FString D3DError( HRESULT h )
	{
		#define D3DERR(x) case x: return TEXT(#x);
		switch( h )
		{
			D3DERR(DD_OK)
			D3DERR(DDERR_ALREADYINITIALIZED)
			D3DERR(DDERR_BLTFASTCANTCLIP)
			D3DERR(DDERR_CANNOTATTACHSURFACE)
			D3DERR(DDERR_CANNOTDETACHSURFACE)
			D3DERR(DDERR_CANTCREATEDC)
			D3DERR(DDERR_CANTDUPLICATE)
			D3DERR(DDERR_CANTLOCKSURFACE)
			D3DERR(DDERR_CANTPAGELOCK)
			D3DERR(DDERR_CANTPAGEUNLOCK)
			D3DERR(DDERR_CLIPPERISUSINGHWND)
			D3DERR(DDERR_COLORKEYNOTSET)
			D3DERR(DDERR_CURRENTLYNOTAVAIL)
			D3DERR(DDERR_DCALREADYCREATED)
			D3DERR(DDERR_DEVICEDOESNTOWNSURFACE)
			D3DERR(DDERR_DIRECTDRAWALREADYCREATED)
			D3DERR(DDERR_EXCEPTION)
			D3DERR(DDERR_EXCLUSIVEMODEALREADYSET)
			D3DERR(DDERR_EXPIRED)
			D3DERR(DDERR_GENERIC)
			D3DERR(DDERR_HEIGHTALIGN)
			D3DERR(DDERR_HWNDALREADYSET)
			D3DERR(DDERR_HWNDSUBCLASSED)
			D3DERR(DDERR_IMPLICITLYCREATED)
			D3DERR(DDERR_INCOMPATIBLEPRIMARY)
			D3DERR(DDERR_INVALIDCAPS)
			D3DERR(DDERR_INVALIDCLIPLIST)
			D3DERR(DDERR_INVALIDDIRECTDRAWGUID)
			D3DERR(DDERR_INVALIDMODE)
			D3DERR(DDERR_INVALIDOBJECT)
			D3DERR(DDERR_INVALIDPARAMS)
			D3DERR(DDERR_INVALIDPIXELFORMAT)
			D3DERR(DDERR_INVALIDPOSITION)
			D3DERR(DDERR_INVALIDRECT)
			D3DERR(DDERR_INVALIDSTREAM)
			D3DERR(DDERR_INVALIDSURFACETYPE)
			D3DERR(DDERR_LOCKEDSURFACES)
			D3DERR(DDERR_MOREDATA)
			D3DERR(DDERR_NO3D)
			D3DERR(DDERR_NOALPHAHW)
			D3DERR(DDERR_NOBLTHW)
			D3DERR(DDERR_NOCLIPLIST)
			D3DERR(DDERR_NOCLIPPERATTACHED)
			D3DERR(DDERR_NOCOLORCONVHW)
			D3DERR(DDERR_NOCOLORKEY)
			D3DERR(DDERR_NOCOLORKEYHW)
			D3DERR(DDERR_NOCOOPERATIVELEVELSET)
			D3DERR(DDERR_NODC)
			D3DERR(DDERR_NODDROPSHW)
			D3DERR(DDERR_NODIRECTDRAWHW)
			D3DERR(DDERR_NODIRECTDRAWSUPPORT)
			D3DERR(DDERR_NOEMULATION)
			D3DERR(DDERR_NOEXCLUSIVEMODE)
			D3DERR(DDERR_NOFLIPHW)
			D3DERR(DDERR_NOFOCUSWINDOW)
			D3DERR(DDERR_NOGDI)
			D3DERR(DDERR_NOHWND)
			D3DERR(DDERR_NOMIPMAPHW)
			D3DERR(DDERR_NOMIRRORHW)
			D3DERR(DDERR_NONONLOCALVIDMEM)
			D3DERR(DDERR_NOOPTIMIZEHW)
			D3DERR(DDERR_NOOVERLAYDEST)
			D3DERR(DDERR_NOOVERLAYHW)
			D3DERR(DDERR_NOPALETTEATTACHED)
			D3DERR(DDERR_NOPALETTEHW)
			D3DERR(DDERR_NORASTEROPHW)
			D3DERR(DDERR_NOROTATIONHW)
			D3DERR(DDERR_NOSTRETCHHW)
			D3DERR(DDERR_NOT4BITCOLOR)
			D3DERR(DDERR_NOT4BITCOLORINDEX)
			D3DERR(DDERR_NOT8BITCOLOR)
			D3DERR(DDERR_NOTAOVERLAYSURFACE)
			D3DERR(DDERR_NOTEXTUREHW)
			D3DERR(DDERR_NOTFLIPPABLE)
			D3DERR(DDERR_NOTFOUND)
			D3DERR(DDERR_NOTINITIALIZED)
			D3DERR(DDERR_NOTLOADED)
			D3DERR(DDERR_NOTLOCKED)
			D3DERR(DDERR_NOTPAGELOCKED)
			D3DERR(DDERR_NOTPALETTIZED)
			D3DERR(DDERR_NOVSYNCHW)
			D3DERR(DDERR_NOZBUFFERHW)
			D3DERR(DDERR_NOZOVERLAYHW)
			D3DERR(DDERR_OUTOFCAPS)
			D3DERR(DDERR_OUTOFMEMORY)
			D3DERR(DDERR_OUTOFVIDEOMEMORY)
			D3DERR(DDERR_OVERLAPPINGRECTS)
			D3DERR(DDERR_OVERLAYCANTCLIP)
			D3DERR(DDERR_OVERLAYCOLORKEYONLYONEACTIVE)
			D3DERR(DDERR_OVERLAYNOTVISIBLE)
			D3DERR(DDERR_PALETTEBUSY)
			D3DERR(DDERR_PRIMARYSURFACEALREADYEXISTS)
			D3DERR(DDERR_REGIONTOOSMALL)
			D3DERR(DDERR_SURFACEALREADYATTACHED)
			D3DERR(DDERR_SURFACEALREADYDEPENDENT)
			D3DERR(DDERR_SURFACEBUSY)
			D3DERR(DDERR_SURFACEISOBSCURED)
			D3DERR(DDERR_SURFACELOST)
			D3DERR(DDERR_SURFACENOTATTACHED)
			D3DERR(DDERR_TOOBIGHEIGHT)
			D3DERR(DDERR_TOOBIGSIZE)
			D3DERR(DDERR_TOOBIGWIDTH)
			D3DERR(DDERR_UNSUPPORTED)
			D3DERR(DDERR_UNSUPPORTEDFORMAT)
			D3DERR(DDERR_UNSUPPORTEDMASK)
			D3DERR(DDERR_UNSUPPORTEDMODE)
			D3DERR(DDERR_VERTICALBLANKINPROGRESS)
			D3DERR(DDERR_VIDEONOTACTIVE)
			D3DERR(DDERR_WASSTILLDRAWING)
			D3DERR(DDERR_WRONGMODE)
			D3DERR(DDERR_XALIGN)
			default: return FString::Printf(TEXT("%08X"),(INT)h);
		}
		#undef D3DERR
	}
	UBOOL UnSetRes( const TCHAR* Msg, HRESULT h )
	{
		guard(UD3DRenderDevice::UnSetRes);
		if( Msg )
			debugf(NAME_Init,TEXT("%s (%s)"),Msg,*D3DError(h));
		Flush();
		guard(ReleaseTextures);
		for( FPixFormat* Fmt=FirstPixelFormat; Fmt; Fmt=Fmt->Next )
		{
			for( TMap<INT,FTexPool*>::TIterator It(Fmt->PoolList); It; ++It )
			{
				if( It.Value()->SystemSurface )
				{
					SAFETRY(It.Value()->SystemTexture->Release());
					SAFETRY(It.Value()->SystemSurface->PageUnlock(0));
					SAFETRY(It.Value()->SystemSurface->Release());
				}
				while( It.Value()->First )
				{
					FTexInfo* Info = It.Value()->First;
					It.Value()->First = Info->PoolNext;
					SAFETRY(Info->Texture->Release());
					//if( ItP->Palette )
					//	SAFETRY(ItP->Palette->Release());
					SAFETRY(Info->Surface->Release());
					delete Info;
				}
			}
			Fmt->PoolList.Empty();
		}
		unguard;
		if( GammaControl )
		{
			SAFETRY(GammaControl->Release());
			GammaControl = NULL;
		}
		if( Direct3DViewport )
		{
			SAFETRY(Direct3DViewport->Release());
			Direct3DViewport = NULL;
		}
		if( Direct3DDevice )
		{
			SAFETRY(Direct3DDevice->Release());
			Direct3DDevice = NULL;
		}
		if( ZSurface )
		{
			SAFETRY(ZSurface->Release());
			ZSurface = NULL;
		}
		if( Clipper )
		{
			SAFETRY(Clipper->Release());
			Clipper = NULL;
		}
		if( BackSurface )
		{
			SAFETRY(BackSurface->Release());
			BackSurface = NULL;
		}
		if( PrimarySurface )
		{
			SAFETRY(PrimarySurface->Release());
			PrimarySurface = NULL;
		}
		if( DidSetDisplayMode )
		{
			SAFETRY(DirectDraw->RestoreDisplayMode());
			SAFETRY(DirectDraw->FlipToGDISurface());
		}
		if( DidSetCooperativeLevel )
		{
			SAFETRY(DirectDraw->SetCooperativeLevel(ViewporthWnd,DDSCL_NORMAL));
			DidSetCooperativeLevel = 0;
		}
		if( Direct3D )
		{
			SAFETRY(Direct3D->Release());
			Direct3D = NULL;
		}
		if( DirectDraw )
		{
			SAFETRY(DirectDraw->Release());
			DirectDraw = NULL;
		}
		if( DidCoInitialize )
		{
			SAFETRY(CoUninitialize());
			DidCoInitialize = 0;
		}
		return 0;
		unguard;
	}
	D3DCOLOR UpdateModulation( INT& ModulateThings, FPlane& FinalColor, const FPlane& MaxColor )
	{
		FinalColor *= MaxColor;
		return --ModulateThings ? 0xffffffff : (FColor(FinalColor).TrueColor() | 0xff000000);
	}
};
IMPLEMENT_CLASS(UD3DRenderDevice);
#endif

/*-----------------------------------------------------------------------------
	End.
-----------------------------------------------------------------------------*/
