/*=============================================================================
	XDrv.h: Unreal X viewport and platform driver.
	Copyright 1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Brandon Reinhart.
=============================================================================*/

#ifndef _INC_XDRV
#define _INC_XDRV

/*----------------------------------------------------------------------------
	Dependencies.
----------------------------------------------------------------------------*/

// System includes.
#include <stdlib.h>

// Unreal includes.
#include "Engine.h"
#include "UnRender.h"

// X includes
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#define XK_MISCELLANY 	1
#define XK_LATIN1 		1
#include <X11/keysymdef.h>
#include <X11/extensions/xf86dga.h>

/*-----------------------------------------------------------------------------
	Declarations.
-----------------------------------------------------------------------------*/

// Classes.
class UXViewport;
class UXClient;

/*-----------------------------------------------------------------------------
	UXClient.
-----------------------------------------------------------------------------*/

//
// X implementation of the client.
//
class UXClient : public UClient, public FNotifyHook
{
	DECLARE_CLASS(UXClient,UClient,CLASS_Transient|CLASS_Config,XDrv)

	// Configuration.
	BITFIELD			StartupFullscreen;
	BITFIELD			SlowVideoBuffering;
	BITFIELD			DGAMouseEnabled;

	// Variables.
	Display*			XDisplay;
	UBOOL				InMenuLoop;
	UBOOL				ConfigReturnFullscreen;
	INT					NormalMouseInfo[3];
	INT					CaptureMouseInfo[3];

	// Constructors.
	UXClient();
	void StaticConstructor();

	// FNotifyHook interface.
	void NotifyDestroy( void* Src );

	// UObject interface.
	void Destroy();
	void PostEditChange();
	void ShutdownAfterError();

	// UClient interface.
	void Init( UEngine* InEngine );
	void ShowViewportWindows( DWORD ShowFlags, INT DoShow );
	void EnableViewportWindows( DWORD ShowFlags, INT DoEnable );
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
	void Tick();
	void MakeCurrent( UViewport* InViewport );
	class UViewport* NewViewport( const FName Name );
};

/*-----------------------------------------------------------------------------
	UXViewport.
-----------------------------------------------------------------------------*/

//
// Viewport window status.
//
enum EXViewportStatus
{
	X_ViewportOpening	= 0, // Viewport is opening.
	X_ViewportNormal	= 1, // Viewport is operating normally.
	X_ViewportClosing	= 2, // Viewport is closing and CloseViewport has been called.
};

//
// A X viewport.
//
class UXViewport : public UViewport
{
	DECLARE_CLASS(UXViewport,UViewport,CLASS_Transient,XDrv)
	DECLARE_WITHIN(UXClient)

	// Variables.
	Display*			XDisplay;
	Window 				XWindow;
	EXViewportStatus	ViewportStatus;
	INT					HoldCount;
	DWORD				BlitFlags;
	UBOOL				Borderless;
	UBOOL				RestoreAutoRepeat;
	UBOOL				LastKey;
	FTime				RepeatTimer;
	UBOOL				UseDGA;
	UBOOL				Mapped;
	UBOOL				Iconified;

	// Mouse.
	INT					MouseAccel_N;
	INT					MouseAccel_D;
	INT					MouseThreshold;

	// Info saved during captures and fullscreen sessions.
	INT					SavedColorBytes;
	INT					SavedCaps;

	// X Keysym to EInputKey map.
	BYTE				KeysymMap[65536];

	// ShiftMask map.
	BYTE				ShiftMaskMap[256];

	// KeyRepeat map.
	BYTE				KeyRepeatMap[256];

	// Map of keys that can be WM_CHAR'd
	BYTE				WMCharMap[256];
	
	// Constructor.
	UXViewport();

	// UObject interface.
	void Destroy();
	void ShutdownAfterError();

	// UViewport interface.
	UBOOL Lock( FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData=NULL, INT* HitSize=0 );
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar );
	UBOOL ResizeViewport( DWORD BlitFlags, INT NewX=INDEX_NONE, INT NewY=INDEX_NONE, INT NewColorBytes=INDEX_NONE );
	UBOOL IsFullscreen();
	void Unlock( UBOOL Blit );
	void Repaint( UBOOL Blit );
	void SetModeCursor();
	void UpdateWindowFrame();
	void OpenWindow( DWORD ParentWindow, UBOOL Temporary, INT NewX, INT NewY, INT OpenX, INT OpenY );
	void CloseWindow();
	void UpdateInput( UBOOL Reset );
	void* GetWindow();
	void* GetServer();
	void SetMouseCapture( UBOOL Capture, UBOOL Clip, UBOOL FocusOnly );

	// UXViewport interface.
	void ToggleFullscreen();
	void EndFullscreen();
	void TryHardware3D( UXViewport* Viewport );
	UBOOL CauseInputEvent( INT iKey, EInputAction Action, FLOAT Delta=0.0 );
	void TryRenderDevice( const TCHAR* ClassName, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen );
	void Tick();
	void Iconify();
	Cursor GetNullCursor();
	void CaptureInputs();
	void ReleaseInputs();
};

#endif //_INC_XDRV
/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
