/*=============================================================================
	XViewport.cpp: UXViewport code.
	Copyright 1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Brandon Reinhart
=============================================================================*/

#include "XDrv.h"

/*-----------------------------------------------------------------------------
	Class implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UXViewport);

/*-----------------------------------------------------------------------------
	UXViewport Init/Exit.
-----------------------------------------------------------------------------*/

//
// Constructor.
//
UXViewport::UXViewport()
:	UViewport()
,	ViewportStatus( X_ViewportOpening )
{
	guard(UXViewport::UXViewport);

	// Open the display.
	XDisplay = XOpenDisplay(0);
	if (!XDisplay)
		appErrorf( TEXT("Can't open X server display.  XViewport requires X windows.") );

	// Create the default window.
	XSetWindowAttributes swa;
	swa.colormap = DefaultColormap(XDisplay, DefaultScreen(XDisplay));
	swa.border_pixel = 0;
	swa.override_redirect = True;
//	swa.override_redirect = False;
	swa.event_mask = ExposureMask | StructureNotifyMask | 
		KeyPressMask | KeyReleaseMask | ButtonPressMask | 
		ButtonReleaseMask | ButtonMotionMask | ResizeRedirectMask |
		PointerMotionMask | FocusChangeMask;
	XWindow = XCreateWindow(
		XDisplay,
		DefaultRootWindow(XDisplay),
		0, 0,
		640, 480,
		0,
		DefaultDepth(XDisplay, DefaultScreen(XDisplay)),
		InputOutput, DefaultVisual(XDisplay, DefaultScreen(XDisplay)),
		CWBorderPixel | CWColormap | CWEventMask | CWOverrideRedirect, &swa
	);

	TCHAR WindowName[80];
	appSprintf( WindowName, TEXT("Unreal Tournament") );
	XStoreName( XDisplay, XWindow, WindowName );

	XMapWindow(XDisplay, XWindow);
	Mapped = True;

	// Set color bytes based on screen resolution.
	switch( DefaultDepth( XDisplay, 0 ) )
	{
		case 8:
			ColorBytes  = 2;
			break;
		case 16:
			ColorBytes  = 2;
			Caps       |= CC_RGB565;
			break;
		case 24:
			ColorBytes  = 4;
			break;
		case 32: 
			ColorBytes  = 4;
			break;
		default: 
			ColorBytes  = 2; 
			Caps       |= CC_RGB565;
			break;
	}

	// Zero out Keysym map.
	for (INT i=0; i<65536; i++)
		KeysymMap[i] = 0;
		
	// Remap important keys.

	// TTY Functions.
	KeysymMap[XK_BackSpace]	= IK_Backspace;
	KeysymMap[XK_Tab]		= IK_Tab;
	KeysymMap[XK_Return]	= IK_Enter;
	KeysymMap[XK_Pause]		= IK_Pause;
	KeysymMap[XK_Escape]	= IK_Escape;
	KeysymMap[XK_Delete]	= IK_Delete;

	// Modifiers.
	KeysymMap[XK_Shift_L]	= IK_LShift;
	KeysymMap[XK_Shift_R]	= IK_RShift;
	KeysymMap[XK_Control_L]	= IK_LControl;
	KeysymMap[XK_Control_R]	= IK_RControl;
	KeysymMap[XK_Meta_L]	= IK_Alt;
	KeysymMap[XK_Meta_R]	= IK_Alt;
	KeysymMap[XK_Alt_L]		= IK_Alt;
	KeysymMap[XK_Alt_R]		= IK_Alt;
	
	// Special remaps.
	KeysymMap[XK_grave]		= IK_Tilde;

	// Misc function keys.
	KeysymMap[XK_F1]		= IK_F1;
	KeysymMap[XK_F2]		= IK_F2;
	KeysymMap[XK_F3]		= IK_F3;
	KeysymMap[XK_F4]		= IK_F4;
	KeysymMap[XK_F5]		= IK_F5;
	KeysymMap[XK_F6]		= IK_F6;
	KeysymMap[XK_F7]		= IK_F7;
	KeysymMap[XK_F8]		= IK_F8;
	KeysymMap[XK_F9]		= IK_F9;
	KeysymMap[XK_F10]		= IK_F10;
	KeysymMap[XK_F11]		= IK_F11;
	KeysymMap[XK_F12]		= IK_F12;

	// Cursor control and motion.
	KeysymMap[XK_Home]		= IK_Home;
	KeysymMap[XK_Left]		= IK_Left;
	KeysymMap[XK_Up]		= IK_Up;
	KeysymMap[XK_Right]		= IK_Right;
	KeysymMap[XK_Down]		= IK_Down;
	KeysymMap[XK_Page_Up]	= IK_PageUp;
	KeysymMap[XK_Page_Down]	= IK_PageDown;
	KeysymMap[XK_End]		= IK_End;

	// Keypad functions and numbers.
	KeysymMap[XK_KP_Enter]		= IK_Enter;
	KeysymMap[XK_KP_0]			= IK_NumPad0;
	KeysymMap[XK_KP_1]			= IK_NumPad1;
	KeysymMap[XK_KP_2]			= IK_NumPad2;
	KeysymMap[XK_KP_3]			= IK_NumPad3;
	KeysymMap[XK_KP_4]			= IK_NumPad4;
	KeysymMap[XK_KP_5]			= IK_NumPad5;
	KeysymMap[XK_KP_6]			= IK_NumPad6;
	KeysymMap[XK_KP_7]			= IK_NumPad7;
	KeysymMap[XK_KP_8]			= IK_NumPad8;
	KeysymMap[XK_KP_9]			= IK_NumPad9;
	KeysymMap[XK_KP_Multiply]	= IK_GreyStar;
	KeysymMap[XK_KP_Add]		= IK_GreyPlus;
	KeysymMap[XK_KP_Separator]	= IK_Separator;
	KeysymMap[XK_KP_Subtract]	= IK_GreyMinus;
	KeysymMap[XK_KP_Decimal]	= IK_NumPadPeriod;
	KeysymMap[XK_KP_Divide]		= IK_GreySlash;

	// Other
	KeysymMap[XK_minus]			= IK_Minus;
	KeysymMap[XK_equal]			= IK_Equals;
	
	// Zero out ShiftMask map.
	for (i=0; i<256; i++)
		ShiftMaskMap[i] = 0;

	// ShiftMask map.
	ShiftMaskMap['1']			= '!';
	ShiftMaskMap['2']			= '@';
	ShiftMaskMap['3']			= '#';
	ShiftMaskMap['4']			= '$';
	ShiftMaskMap['5']			= '%';
	ShiftMaskMap['6']			= '^';
	ShiftMaskMap['7']			= '&';
	ShiftMaskMap['8']			= '*';
	ShiftMaskMap['9']			= '(';
	ShiftMaskMap['0']			= ')';
	ShiftMaskMap['-']			= '_';
	ShiftMaskMap['=']			= '+';
	ShiftMaskMap['[']			= '{';
	ShiftMaskMap[']']			= '}';
	ShiftMaskMap['\\']			= '|';
	ShiftMaskMap[';']			= ':';
	ShiftMaskMap['\'']			= '\"';
	ShiftMaskMap[',']			= '<';
	ShiftMaskMap['.']			= '>';
	ShiftMaskMap['/']			= '?';

	// WM_CHAR allowables.
	for (i=0; i<256; i++)
		WMCharMap[i] = 0;
	for (i='A'; i<='Z'; i++)
		WMCharMap[i] = 1;
	for (i='a'; i<='z'; i++)
		WMCharMap[i] = 1;
	WMCharMap[IK_Backspace]		= 1;
	WMCharMap[IK_Space]			= 1;
	WMCharMap[IK_Tab]			= 1;
	WMCharMap[IK_Enter]			= 1;
	WMCharMap['1']				= 1;
	WMCharMap['2']				= 1;
	WMCharMap['3']				= 1;
	WMCharMap['4']				= 1;
	WMCharMap['5']				= 1;
	WMCharMap['6']				= 1;
	WMCharMap['7']				= 1;
	WMCharMap['8']				= 1;
	WMCharMap['9']				= 1;
	WMCharMap['0']				= 1;
	WMCharMap['-']				= 1;
	WMCharMap['=']				= 1;
	WMCharMap['[']				= 1;
	WMCharMap[']']				= 1;
	WMCharMap['\\']				= 1;
	WMCharMap[';']				= 1;
	WMCharMap['\'']				= 1;
	WMCharMap[',']				= 1;
	WMCharMap['.']				= 1;
	WMCharMap['/']				= 1;
	WMCharMap['!']				= 1;
	WMCharMap['@']				= 1;
	WMCharMap['#']				= 1;
	WMCharMap['$']				= 1;
	WMCharMap['%']				= 1;
	WMCharMap['^']				= 1;
	WMCharMap['&']				= 1;
	WMCharMap['*']				= 1;
	WMCharMap['(']				= 1;
	WMCharMap[')']				= 1;
	WMCharMap['_']				= 1;
	WMCharMap['+']				= 1;
	WMCharMap['{']				= 1;
	WMCharMap['}']				= 1;
	WMCharMap['|']				= 1;
	WMCharMap[':']				= 1;
	WMCharMap['\"']				= 1;
	WMCharMap['<']				= 1;
	WMCharMap['>']				= 1;
	WMCharMap['?']				= 1;
	
	// Zero out KeyRepeat map.
	for (i=0; i<256; i++)
		KeyRepeatMap[i] = 0;

	// Remember pointer settings.
	XGetPointerControl(XDisplay, &MouseAccel_N, &MouseAccel_D, &MouseThreshold);

	debugf( TEXT("Created and initialized a new X viewport.") );

	unguard;
}

//
// Destroy.
//
void UXViewport::Destroy()
{
	guard(UXViewport::Destroy);

	// Release input.
	ReleaseInputs();

	if( XWindow && XDisplay )
	{
		XDestroyWindow( XDisplay, XWindow );
		XWindow = (Window) 0;
	}
	
	if ( XDisplay )
	{
		XCloseDisplay( XDisplay );
		XDisplay = 0;
	}

	Super::Destroy();
	unguard;
}

//
// Error shutdown.
//
void UXViewport::ShutdownAfterError()
{
	debugf( NAME_Exit, TEXT("Executing UXViewport::ShutdownAfterError") );

	// Release input.
	ReleaseInputs();
	
	if( XWindow )
	{
		XDestroyWindow( XDisplay, XWindow );
		XWindow = (Window) 0;
	}
	if( XDisplay )
	{
		XCloseDisplay( XDisplay );
		XDisplay = 0;
	}
	Super::ShutdownAfterError();
}

/*-----------------------------------------------------------------------------
	Command line.
-----------------------------------------------------------------------------*/

//
// Command line.
//
UBOOL UXViewport::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(UXViewport::Exec);
	if( UViewport::Exec( Cmd, Ar ) )
	{
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("EndFullscreen")) )
	{
		EndFullscreen();
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("ToggleFullscreen")) )
	{
		ToggleFullscreen();
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("Iconify")) )
	{
		Iconify();
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("GetCurrentRes")) )
	{
		Ar.Logf( TEXT("%ix%i"), SizeX, SizeY, (ColorBytes?ColorBytes:2)*8 );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("GetCurrentColorDepth")) )
	{
		Ar.Logf( TEXT("%i"), (ColorBytes?ColorBytes:2)*8 );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("GetColorDepths")) )
	{
		Ar.Log( TEXT("16 32") );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("GetCurrentRenderDevice")) )
	{
		Ar.Log( RenDev->GetClass()->GetPathName() );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("SetRenderDevice")) )
	{
		FString Saved = RenDev->GetClass()->GetPathName();
		INT SavedSizeX=SizeX, SavedSizeY=SizeY, SavedColorBytes=ColorBytes, SavedFullscreen=((BlitFlags & BLIT_Fullscreen)!=0);
		TryRenderDevice( Cmd, SizeX, SizeY, ColorBytes, SavedFullscreen );
		if( !RenDev )
		{
			TryRenderDevice( *Saved, SavedSizeX, SavedSizeY, SavedColorBytes, SavedFullscreen );
			check(RenDev);
			Ar.Log(TEXT("0"));
		}
		else Ar.Log(TEXT("1"));
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("GetRes")) )
	{
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("SetRes")) )
	{
		INT X=appAtoi(Cmd);
		TCHAR* CmdTemp = (TCHAR*) (appStrchr(Cmd,'x') ? appStrchr(Cmd,'x')+1 : appStrchr(Cmd,'X') ? appStrchr(Cmd,'X')+1 : TEXT(""));
		INT Y=appAtoi(CmdTemp);
		Cmd = CmdTemp;
		CmdTemp = (TCHAR*) (appStrchr(Cmd,'x') ? appStrchr(Cmd,'x')+1 : appStrchr(Cmd,'X') ? appStrchr(Cmd,'X')+1 : TEXT(""));
		INT C=appAtoi(CmdTemp);
		INT NewColorBytes = C ? C/8 : ColorBytes;
		if( X && Y )
		{
			HoldCount++;
			UBOOL Result = RenDev->SetRes( X, Y, NewColorBytes, IsFullscreen() );
			HoldCount--;
			if( !Result )
				EndFullscreen();
		}
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("Preferences")) )
	{
		// No preferences window.
	
		return 1;
	}
	else return 0;
	unguard;
}

/*-----------------------------------------------------------------------------
	Window openining and closing.
-----------------------------------------------------------------------------*/

//
// Open this viewport's window.
//
void UXViewport::OpenWindow( DWORD InParentWindow, UBOOL IsTemporary, INT NewX, INT NewY, INT OpenX, INT OpenY )
{
	guard(UXViewport::OpenWindow);
	check(Actor);
	check(!HoldCount);
	UXClient* C = GetOuterUXClient();

	if (!XWindow)
		return;

	debugf( TEXT("Opening X viewport.") );

	// Create or update the window.
	SizeX = C->FullscreenViewportX;
	SizeY = C->FullscreenViewportY;
	if (!Mapped)
	{
		XMoveResizeWindow( XDisplay, XWindow, 0, 0, SizeX, SizeY );
		XMapWindow( XDisplay, XWindow );
	}

	// Create rendering device.
	if( !RenDev && !GIsEditor && !ParseParam(appCmdLine(),TEXT("nohard")) )
		TryRenderDevice( TEXT("ini:Engine.Engine.GameRenderDevice"), NewX, NewY, ColorBytes, C->StartupFullscreen );
	check(RenDev);
	UpdateWindowFrame();
	Repaint( 1 );

	unguard;
}

//
// Close a viewport window.  Assumes that the viewport has been openened with
// OpenViewportWindow.  Does not affect the viewport's object, only the
// platform-specific information associated with it.
//
void UXViewport::CloseWindow()
{
	guard(UXViewport::CloseWindow);
	if( XWindow && ViewportStatus==X_ViewportNormal )
	{
		ViewportStatus = X_ViewportClosing;
		XDestroyWindow( XDisplay, XWindow );
		XWindow = (Window) 0;
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	UXViewport operations.
-----------------------------------------------------------------------------*/

//
// Repaint the viewport.
//
void UXViewport::Repaint( UBOOL Blit )
{
	guard(UXViewport::Repaint);
	GetOuterUXClient()->Engine->Draw( this, Blit );
	unguard;
}

//
// Return whether fullscreen.
//
UBOOL UXViewport::IsFullscreen()
{
	guard(UXViewport::IsFullscreen);
	return (BlitFlags & BLIT_Fullscreen)!=0;
	unguard;
}

//
// Set the mouse cursor according to Unreal or UnrealEd's mode, or to
// an hourglass if a slow task is active.
//
void UXViewport::SetModeCursor()
{
	guard(UXViewport::SetModeCursor);

	// FIXME: Implement for X
	
	unguard;
}

//
// Update user viewport interface.
//
void UXViewport::UpdateWindowFrame()
{
	guard(UXViewport::UpdateWindowFrame);

	// If not a window, exit.
	if( HoldCount || !XWindow || (BlitFlags&BLIT_Fullscreen) || (BlitFlags&BLIT_Temporary) )
		return;

	unguard;
}

//
// Return the viewport's window.
//
void* UXViewport::GetWindow()
{
	guard(UXViewport::GetWindow);
	return &XWindow;
	unguard;
}

//
// Return the viewport's display.
//
void* UXViewport::GetServer()
{
	guard(UXViewport::GetServer);
	return XDisplay;
	unguard;
}

/*-----------------------------------------------------------------------------
	Input.
-----------------------------------------------------------------------------*/

//
// Input event router.
//
UBOOL UXViewport::CauseInputEvent( INT iKey, EInputAction Action, FLOAT Delta )
{
	guard(UXViewport::CauseInputEvent);

	// Route to engine if a valid key; some keyboards produce key
	// codes that go beyond IK_MAX.
	if( iKey>=0 && iKey<IK_MAX )
		return GetOuterUXClient()->Engine->InputEvent( this, (EInputKey)iKey, Action, Delta );
	else
		return 0;

	unguard;
}

//
// If the cursor is currently being captured, stop capturing, clipping, and 
// hiding it, and move its position back to where it was when it was initially
// captured.
//
void UXViewport::SetMouseCapture( UBOOL Capture, UBOOL Clip, UBOOL OnlyFocus )
{
	guard(UXViewport::SetMouseCapture);

	bWindowsMouseAvailable = !Capture;

	if( OnlyFocus )
		return;

	if( Capture )
	{
		CaptureInputs();
	} else {
		ReleaseInputs();
	}
	unguard;
}

//
// Update input for this viewport.
//
void UXViewport::UpdateInput( UBOOL Reset )
{
	guard(UXViewport::UpdateInput);

	unguard;
}

/*-----------------------------------------------------------------------------
	Lock and Unlock.
-----------------------------------------------------------------------------*/

//
// Lock the viewport window and set the approprite Screen and RealScreen fields
// of Viewport.  Returns 1 if locked successfully, 0 if failed.  Note that a
// lock failing is not a critical error; it's a sign that a DirectDraw mode
// has ended or the user has closed a viewport window.
//
UBOOL UXViewport::Lock( FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData, INT* HitSize )
{
	guard(UXViewport::LockWindow);
	UXClient* Client = GetOuterUXClient();
	clock(Client->DrawCycles);

	// Success here, so pass to superclass.
	unclock(Client->DrawCycles);
	return UViewport::Lock(FlashScale,FlashFog,ScreenClear,RenderLockFlags,HitData,HitSize);

	unguard;
}

//
// Unlock the viewport window.  If Blit=1, blits the viewport's frame buffer.
//
void UXViewport::Unlock( UBOOL Blit )
{
	guard(UXViewport::Unlock);

	UViewport::Unlock( Blit );
	
	unguard;
}

/*-----------------------------------------------------------------------------
	Viewport modes.
-----------------------------------------------------------------------------*/

//
// Try switching to a new rendering device.
//
void UXViewport::TryRenderDevice( const TCHAR* ClassName, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen )
{
	guard(UXViewport::TryRenderDevice);

	// Shut down current rendering device.
	if( RenDev )
	{
		RenDev->Exit();
		delete RenDev;
		RenDev = NULL;
	}

	// Use appropriate defaults.
	UXClient* C = GetOuterUXClient();
	if( NewX==INDEX_NONE )
		NewX = Fullscreen ? C->FullscreenViewportX : C->WindowedViewportX;
	if( NewY==INDEX_NONE )
		NewY = Fullscreen ? C->FullscreenViewportY : C->WindowedViewportY;

	// Find device driver.
	UClass* RenderClass = UObject::StaticLoadClass( URenderDevice::StaticClass(), NULL, ClassName, NULL, 0, NULL );
	if( RenderClass )
	{
		debugf( TEXT("Loaded render device class.") );
		HoldCount++;
		RenDev = ConstructObject<URenderDevice>( RenderClass, this );
		if( RenDev->Init( this, NewX, NewY, NewColorBytes, Fullscreen ) )
		{
			if( GIsRunning )
				Actor->GetLevel()->DetailChange( RenDev->HighDetailActors );
		}
		else
		{
			debugf( NAME_Log, LocalizeError("Failed3D") );
			delete RenDev;
			RenDev = NULL;
		}
		HoldCount--;
	}
	GRenderDevice = RenDev;
	unguard;
}

//
// If in fullscreen mode, end it and return to Windows.
//
void UXViewport::EndFullscreen()
{
	guard(UXViewport::EndFullscreen);
	UXClient* Client = GetOuterUXClient();
	debugf(NAME_Log, TEXT("Ending fullscreen mode by request."));
	if( RenDev && RenDev->FullscreenOnly )
	{
		// This device doesn't support fullscreen, so use a window-capable rendering device.
		TryRenderDevice( TEXT("ini:Engine.Engine.WindowedRenderDevice"), INDEX_NONE, INDEX_NONE, ColorBytes, 0 );
		check(RenDev);
	}
	else if( RenDev && (BlitFlags & BLIT_OpenGL) )
	{
		RenDev->SetRes( INDEX_NONE, INDEX_NONE, ColorBytes, 0 );
	}
	else
	{
		ResizeViewport( BLIT_DibSection );
	}
	UpdateWindowFrame();
	unguard;
}

//
// Toggle fullscreen.
//
void UXViewport::ToggleFullscreen()
{
	guard(UXViewport::ToggleFullscreen);
	if( BlitFlags & BLIT_Fullscreen )
	{
		EndFullscreen();
	}
	else if( !(Actor->ShowFlags & SHOW_ChildWindow) )
	{
		debugf(TEXT("AttemptFullscreen"));
		TryRenderDevice( TEXT("ini:Engine.Engine.GameRenderDevice"), INDEX_NONE, INDEX_NONE, ColorBytes, 1 );
		if( !RenDev )
			TryRenderDevice( TEXT("ini:Engine.Engine.WindowedRenderDevice"), INDEX_NONE, INDEX_NONE, ColorBytes, 1 );
		if( !RenDev )
			TryRenderDevice( TEXT("ini:Engine.Engine.WindowedRenderDevice"), INDEX_NONE, INDEX_NONE, ColorBytes, 0 );
	}
	unguard;
}

//
// Resize the viewport.
//
UBOOL UXViewport::ResizeViewport( DWORD NewBlitFlags, INT InNewX, INT InNewY, INT InNewColorBytes )
{
	guard(UXViewport::ResizeViewport);
	UXClient* Client = GetOuterUXClient();

	debugf( TEXT("Resizing X viewport. X: %i Y: %i"), InNewX, InNewY );

	// Remember viewport.
	UViewport* SavedViewport = NULL;
	if( Client->Engine->Audio && !GIsEditor && !(GetFlags() & RF_Destroyed) )
		SavedViewport = Client->Engine->Audio->GetViewport();

	// Accept default parameters.
	INT NewX          = InNewX         ==INDEX_NONE ? SizeX      : InNewX;
	INT NewY          = InNewY         ==INDEX_NONE ? SizeY      : InNewY;
	INT NewColorBytes = InNewColorBytes==INDEX_NONE ? ColorBytes : InNewColorBytes;

	// Default resolution handling.
	NewX = InNewX!=INDEX_NONE ? InNewX : (NewBlitFlags&BLIT_Fullscreen) ? Client->FullscreenViewportX : Client->WindowedViewportX;
	NewY = InNewX!=INDEX_NONE ? InNewY : (NewBlitFlags&BLIT_Fullscreen) ? Client->FullscreenViewportY : Client->WindowedViewportY;

	if( NewBlitFlags & BLIT_Fullscreen )
	{
		// Changing to fullscreen.
		XResizeWindow(XDisplay, XWindow, NewX, NewY);

		// Grab mouse and keyboard.
		SetMouseCapture( 1, 1, 0 );
	} else {
		// Changing to windowed mode.
		XResizeWindow(XDisplay, XWindow, NewX, NewY);

		// Set focus to us.
		XSetInputFocus(XDisplay, XWindow, RevertToNone, CurrentTime);
		
		// End mouse and keyboard grab.
		SetMouseCapture( 0, 0, 0 );
	}

	// Update audio.
	if( SavedViewport && SavedViewport!=Client->Engine->Audio->GetViewport() )
		Client->Engine->Audio->SetViewport( SavedViewport );

	// Update the window.
	UpdateWindowFrame();

	// Set new info.
	DWORD OldBlitFlags = BlitFlags;
	BlitFlags          = NewBlitFlags & ~BLIT_ParameterFlags;
	SizeX              = NewX;
	SizeY              = NewY;
	ColorBytes         = NewColorBytes ? NewColorBytes : ColorBytes;

	// Save info.
	if( RenDev && !GIsEditor )
	{
		if( NewBlitFlags & BLIT_Fullscreen )
		{
			if( NewX && NewY )
			{
				Client->FullscreenViewportX = NewX;
				Client->FullscreenViewportY = NewY;
			}
		}
		else
		{
			if( NewX && NewY )
			{
				Client->WindowedViewportX = NewX;
				Client->WindowedViewportY = NewY;
			}
		}
		Client->SaveConfig();
	}
	return 1;
	unguard;
}

void UXViewport::Tick()
{
	guard(UXViewport::Tick);
	UXClient* Client = GetOuterUXClient();

	if (!XWindow)
		return;

	// Keyboard.
	EInputKey Key;
	KeySym LowerCase, UpperCase;

	// Mouse movement management.
	UBOOL MouseMoved;
	INT BaseX = SizeX/2;
	INT BaseY = SizeY/2;
	INT DX = 0, DY = 0;

	XEvent Event;
	while( XPending(XDisplay) )
	{
		XNextEvent(XDisplay, &Event);
		switch( Event.type )
		{
			case CreateNotify:
				// Window has been created.
				ViewportStatus = X_ViewportNormal;

				// Make this viewport current and update its title bar.
				GetOuterUClient()->MakeCurrent( this );				
				break;
			case DestroyNotify:
				// Window has been destroyed.
				if( BlitFlags & BLIT_Fullscreen )
					EndFullscreen();

				if( ViewportStatus == X_ViewportNormal )
				{
					// Closed normally.
					ViewportStatus = X_ViewportClosing;
					delete this;
				}
				break;
			case Expose:
				// Redraw the window.
				break;
			case KeyPress:
				// Reset timer.
				RepeatTimer = appSeconds();
				LastKey = True;

				// Get key code.
				Key = (EInputKey) XKeycodeToKeysym( XDisplay, Event.xkey.keycode, 0 );
				XConvertCase( Key, &LowerCase, &UpperCase );
				Key = (EInputKey) UpperCase;

				// Check the Keysym map.
				if (KeysymMap[Key] != 0)
					Key = (EInputKey) KeysymMap[Key];

				// Send key to input system.
				CauseInputEvent( Key, IST_Press );

				// Emulate WM_CHAR.
				// Check for shift modifier.
				if (Event.xkey.state & ShiftMask)
				{
					Key = (EInputKey) UpperCase;
					if (ShiftMaskMap[Key] != 0)
						Key = (EInputKey) ShiftMaskMap[Key];
				} else
					Key = (EInputKey) LowerCase;

				if (Key == XK_BackSpace)
					Key = IK_Backspace;
				if (Key == XK_Tab)
					Key = IK_Tab;
				if (Key == XK_Return)
					Key = IK_Enter;

				if (WMCharMap[Key] == 1)
				{
					KeyRepeatMap[Key] = 1;

					Client->Engine->Key( this, Key );
				}
				break;
			case KeyRelease:
				// Get key code.
				Key = (EInputKey) XKeycodeToKeysym( XDisplay, Event.xkey.keycode, 0 );
				XConvertCase( Key, &LowerCase, &UpperCase );
				Key = (EInputKey) UpperCase;

				// Check the Keysym map.
				if (KeysymMap[Key] != 0)
					Key = (EInputKey) KeysymMap[Key];

				// Send key to input system.
				CauseInputEvent( Key, IST_Release );

				// Release all types of this key.
				if (Key == XK_BackSpace)
					Key = IK_Backspace;
				if (Key == XK_Tab)
					Key = IK_Tab;
				if (Key == XK_Return)
					Key = IK_Enter;

				KeyRepeatMap[Key] = 0;
				KeyRepeatMap[ShiftMaskMap[Key]] = 0;
				KeyRepeatMap[ShiftMaskMap[(EInputKey) LowerCase]] = 0;
				KeyRepeatMap[(EInputKey) LowerCase] = 0;
				break;
			case ButtonPress:
				switch (Event.xbutton.button)
				{
					case 1:
						Key = IK_LeftMouse;
						break;
					case 2:
						Key = IK_MiddleMouse;
						break;
					case 3:
						Key = IK_RightMouse;
						break;
					case 4:
						Key = IK_MouseWheelUp;
						break;
					case 5:
						Key = IK_MouseWheelDown;
						break;
				}

				// Send to input system.
				CauseInputEvent( Key, IST_Press );
				break;
			case ButtonRelease:
				switch (Event.xbutton.button)
				{
					case 1:
						Key = IK_LeftMouse;
						break;
					case 2:
						Key = IK_MiddleMouse;
						break;
					case 3:
						Key = IK_RightMouse;
						break;
					case 4:
						Key = IK_MouseWheelUp;
						break;
					case 5:
						Key = IK_MouseWheelDown;
						break;
				}

				// Send to input system.
				CauseInputEvent( Key, IST_Release );
				break;
			case MotionNotify:
				MouseMoved = True;

				if (UseDGA)
				{
					if (abs(Event.xmotion.x_root) > 1)
						DX += Event.xmotion.x_root * 2;
					else
						DX += Event.xmotion.x_root;
					if (abs(Event.xmotion.y_root) > 1)
						DY += Event.xmotion.y_root * 2;
					else
						DY += Event.xmotion.y_root;
				} else {
					DX += Event.xmotion.x - BaseX;
					DY += Event.xmotion.y - BaseY;
					BaseX = Event.xmotion.x;
					BaseY = Event.xmotion.y;
				}
				break;
			case ResizeRequest:
				// Eventually resize and setres.
				break;
			case MapNotify:
				if (Iconified)
				{
					guard(Uniconify);
					Iconified = false;

					// Unpause the game if applicable.
					Exec( TEXT("SETPAUSE 0"), *this );

					// Reset the input buffer.
					Input->ResetInput();

					// Snag the mouse again.
					SetMouseCapture( 1, 1, 0 );

					// Make this viewport current.
					GetOuterUClient()->MakeCurrent( this );

					// Turn off that damn auto repeat.
					XAutoRepeatOff( XDisplay );

					// Return to fullscreen.
					if( BlitFlags & BLIT_Fullscreen )
						TryRenderDevice( TEXT("ini:Engine.Engine.GameRenderDevice"), INDEX_NONE, INDEX_NONE, ColorBytes, 1 );
					unguard;
				}
				break;
			case UnmapNotify:
				if (!Iconified)
					Iconify();
				break;
			case FocusIn:
				break;
			case FocusOut:
				Iconify();
				break;
		}
	}

	if (Iconified)
		return;

	// Deliver mouse behavior to the engine.
	if (MouseMoved)
	{
		if (!UseDGA)
		{
			XWarpPointer(XDisplay, None, XWindow,
				0, 0, 0, 0, SizeX/2, SizeY/2);

			// Clear out the warp.
			XEvent MouseEvent;
			while( XCheckWindowEvent(XDisplay, XWindow, ButtonMotionMask | PointerMotionMask, &MouseEvent) )
			{
				// Do Nothing.
			}
		}
		
		// Send to input subsystem.
		if( DX )
			CauseInputEvent( IK_MouseX, IST_Axis, +DX );
		if( DY )
			CauseInputEvent( IK_MouseY, IST_Axis, -DY );
	}
	
	// Send WM_CHAR for down keys.
	if ( LastKey && (appSeconds() - RepeatTimer < 0.5) )
		return;
	LastKey = False;
	if ( appSeconds() - RepeatTimer < 0.1 )
		return;

	RepeatTimer = appSeconds();
	for (INT i=0; i<256; i++)
		if (KeyRepeatMap[i] != 0)
		{
			if (i == IK_Backspace)
			{
				CauseInputEvent( i, IST_Press );
				CauseInputEvent( i, IST_Release );				
			}
			else
				Client->Engine->Key( this, (EInputKey) i );
		}
	
	unguard;
}

void UXViewport::Iconify()
{
	guard(UXViewport::Iconify);

	if (Iconified)
		return;
		
	Iconified = true;

	// Pause the game if applicable.
	if( GIsRunning )
		Exec( TEXT("SETPAUSE 1"), *this );

	// Release the mouse.
	SetMouseCapture( 0, 0, 0 );
	SetDrag( 0 );

	// Reset the input buffer.
	Input->ResetInput();

	// End fullscreen.
	if( BlitFlags & BLIT_Fullscreen )
		EndFullscreen();
	GetOuterUClient()->MakeCurrent( NULL );

	// Turn auto repeat back on.
	XAutoRepeatOn( XDisplay );

	// Iconify the window.
	XIconifyWindow( XDisplay, XWindow, DefaultScreen(XDisplay) );	
	unguard;
}

Cursor UXViewport::GetNullCursor()
{
	guard(UXViewport::GetNullCursor);

	Pixmap NullCursor;
	XGCValues _XGC;
	GC _Context;
	XColor Nothing;
	Cursor Result;

	NullCursor = XCreatePixmap(XDisplay, XWindow, 1, 1, 1);
	_XGC.function = GXclear;
	_Context = XCreateGC(XDisplay, NullCursor, GCFunction, &_XGC);
	XFillRectangle(XDisplay, NullCursor, _Context, 0, 0, 1, 1);
	Nothing.pixel = 0;
	Nothing.red = 0;
	Nothing.flags = 04;
	Result = XCreatePixmapCursor(XDisplay, NullCursor, NullCursor, 
		&Nothing, &Nothing, 0, 0);
	XFreePixmap(XDisplay, NullCursor);
	XFreeGC(XDisplay, _Context);

	return Result;
	
	unguard;
}

void UXViewport::CaptureInputs()
{
	guard(UXViewport::CaptureInputs);
	
	// Check keyboard state.
	XKeyboardState KeyState;
	XGetKeyboardControl(XDisplay, &KeyState);
	if( KeyState.global_auto_repeat == AutoRepeatModeOn )
	{
		RestoreAutoRepeat = true;
		XAutoRepeatOff( XDisplay );
	}

	// Examine root window.
	Window RootRoot;
	int r_x, r_y;
	unsigned int r_width, r_height, r_border, r_depth;
	XGetGeometry(
		XDisplay, DefaultRootWindow(XDisplay), &RootRoot,
		&r_x, &r_y, &r_width, &r_height, &r_border, &r_depth
	);

	XWarpPointer(XDisplay, None, XWindow,
		0, 0, 0, 0, r_width/2, r_height/2);

	XSync(XDisplay, False);

	//XDefineCursor(XDisplay, XWindow, GetNullCursor());

	// Capture the pointer.
	XGrabPointer(XDisplay, XWindow, False,
		ButtonPressMask | ButtonReleaseMask | 
		PointerMotionMask | ButtonMotionMask,
		GrabModeAsync, GrabModeAsync, XWindow, None, CurrentTime );

	// Control acceleration.
	XChangePointerControl(XDisplay, True, True, 2, 1, 0);

	XSync(XDisplay, False);
		
	// Query DGA capabilities.
	UXClient* C = GetOuterUXClient();
	if (C->DGAMouseEnabled)
	{
		INT VersionMajor, VersionMinor;
		if (!XF86DGAQueryVersion(XDisplay, &VersionMajor, &VersionMinor))
		{
			debugf( TEXT("XF86DGA disabled.") );
			UseDGA = false;
		} else {
			debugf( TEXT("XF86DGA enabled.") );
			UseDGA = true;
			XF86DGADirectVideo(XDisplay, DefaultScreen(XDisplay), XF86DGADirectMouse);
			XWarpPointer(XDisplay, None, XWindow, 0, 0, 0, 0, 0, 0);
		}
	} else
		UseDGA = false;

	XGrabKeyboard(XDisplay, XWindow, False, GrabModeAsync, GrabModeAsync, CurrentTime);

	XSync(XDisplay, False);

	unguard;
}

void UXViewport::ReleaseInputs()
{
	guard(UXViewport::ReleaseInputs);

	// Restore DGA mode.
	if (UseDGA)
	{
		UseDGA = false;
		XF86DGADirectVideo(XDisplay, DefaultScreen(XDisplay), 0);
	}

	// Restore acceleration.
	XChangePointerControl(XDisplay, True, True, MouseAccel_N, MouseAccel_D, MouseThreshold);
	
	// Restore pointer and keyboard.
	XUngrabPointer( XDisplay, CurrentTime );
	XUngrabKeyboard( XDisplay, CurrentTime );

	// Restore cursor.
	XUndefineCursor( XDisplay, XWindow );

	// Restore Keyboard AutoRepeat
	if(	RestoreAutoRepeat )
	{
		RestoreAutoRepeat = false;
		XAutoRepeatOn( XDisplay );
	}

	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
