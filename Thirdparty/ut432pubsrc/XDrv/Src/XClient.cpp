/*=============================================================================
	XClient.cpp: UXClient code.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Brandon Reinhart
=============================================================================*/

#include "XDrv.h"

/*-----------------------------------------------------------------------------
	Class implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UXClient);

/*-----------------------------------------------------------------------------
	UWindowsClient implementation.
-----------------------------------------------------------------------------*/

//
// UXClient constructor.
//
UXClient::UXClient()
{
	guard(UXClient::UXClient);

	unguard;
}

//
// Static init.
//
void UXClient::StaticConstructor()
{
	guard(UXClient::StaticConstructor);

	new(GetClass(),TEXT("StartupFullscreen"), RF_Public)UBoolProperty(CPP_PROPERTY(StartupFullscreen), TEXT("Display"),  CPF_Config );
	new(GetClass(),TEXT("SlowVideoBuffering"),RF_Public)UBoolProperty(CPP_PROPERTY(SlowVideoBuffering),TEXT("Display"),  CPF_Config );
	new(GetClass(),TEXT("DGAMouseEnabled"),RF_Public)UBoolProperty(CPP_PROPERTY(DGAMouseEnabled),TEXT("Display"), CPF_Config );
	unguard;
}

//
// Initialize the platform-specific viewport manager subsystem.
// Must be called after the Unreal object manager has been initialized.
// Must be called before any viewports are created.
//
void UXClient::Init( UEngine* InEngine )
{
	guard(UXClient::UXClient);

	// Init base.
	UClient::Init( InEngine );

	// Fix up the environment variables for 3dfx.
	putenv( "MESA_GLX_FX=fullscreen" );
	putenv( "FX_GLIDE_NO_SPLASH=1" );

	// Note configuration.
	PostEditChange();

	// Default res option.
	if( ParseParam(appCmdLine(),TEXT("defaultres")) )
	{
		WindowedViewportX  = FullscreenViewportX  = 800;
		WindowedViewportY  = FullscreenViewportY  = 600;
	}

	// Success.
	debugf( NAME_Init, TEXT("XClient initialized.") );
	unguard;
}

//
// Shut down the platform-specific viewport manager subsystem.
//
void UXClient::Destroy()
{
	guard(UXClient::Destroy);

	// Stop mouse capture.

	// Clean up X resources.

	debugf( NAME_Exit, TEXT("X client shut down.") );
	Super::Destroy();
	unguard;
}

//
// Failsafe routine to shut down viewport manager subsystem
// after an error has occured. Not guarded.
//
void UXClient::ShutdownAfterError()
{
	debugf( NAME_Exit, TEXT("Executing UXClient::ShutdownAfterError") );

	// End mouse capture.

	// Kill the audio subsystem.
	if( Engine && Engine->Audio )
	{
		Engine->Audio->ConditionalShutdownAfterError();
	}

	// Release all viewports.
	for( INT i=Viewports.Num()-1; i>=0; i-- )
	{
		UXViewport* Viewport = (UXViewport*)Viewports( i );
		Viewport->ConditionalShutdownAfterError();
	}

	Super::ShutdownAfterError();
}

void UXClient::NotifyDestroy( void* Src )
{
	guard(UXClient::NotifyDestroy);

	unguard;
}

//
// Command line.
//
UBOOL UXClient::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(UXClient::Exec);
	if( UClient::Exec( Cmd, Ar ) )
	{
		return 1;
	}
	return 0;
	unguard;
}

//
// Perform timer-tick processing on all visible viewports.  This causes
// all realtime viewports, and all non-realtime viewports which have been
// updated, to be blitted.
//
void UXClient::Tick()
{
	guard(UXClient::Tick);

	// Tick the viewports.
  	for( INT i=0; i<Viewports.Num(); i++ )
	{
		UXViewport* Viewport = CastChecked<UXViewport>(Viewports(i));
		Viewport->Tick();
	}

	// Blit any viewports that need blitting.
	UXViewport* BestViewport = NULL;
  	for( i=0; i<Viewports.Num(); i++ )
	{
		UXViewport* Viewport = CastChecked<UXViewport>(Viewports(i));
		check(!Viewport->HoldCount);
		if( !(Viewport->XWindow) )
		{
			// Window was closed via close button.
			delete Viewport;
			return;
		}
  		else if
		(	Viewport->IsRealtime()
		&&	Viewport->SizeX
		&&	Viewport->SizeY
		&&	(!BestViewport || Viewport->LastUpdateTime<BestViewport->LastUpdateTime) )
		{
			BestViewport = Viewport;
		}
	}
	if( BestViewport )
		BestViewport->Repaint( 1 );

	unguard;
}

//
// Create a new viewport.
//
UViewport* UXClient::NewViewport( const FName Name )
{
	guard(UXClient::NewViewport);
	return new( this, Name )UXViewport();
	unguard;
}

//
// Configuration change.
//
void UXClient::PostEditChange()
{
	guard(UXClient::PostEditChange);

	Super::PostEditChange();

	unguard;
}

//
// Enable or disable all viewport windows that have ShowFlags set (or all if ShowFlags=0).
//
void UXClient::EnableViewportWindows( DWORD ShowFlags, int DoEnable )
{
	guard(UXClient::EnableViewportWindows);

	unguard;
}

//
// Show or hide all viewport windows that have ShowFlags set (or all if ShowFlags=0).
//
void UXClient::ShowViewportWindows( DWORD ShowFlags, int DoShow )
{
	guard(UXClient::ShowViewportWindows); 	
	for( int i=0; i<Viewports.Num(); i++ )
	{
		UXViewport* Viewport = (UXViewport*)Viewports(i);
		if( (Viewport->Actor->ShowFlags & ShowFlags)==ShowFlags )
			XMapWindow(XDisplay, Viewport->XWindow);
	}
	unguard;
}

//
// Make this viewport the current one.
// If Viewport=0, makes no viewport the current one.
//
void UXClient::MakeCurrent( UViewport* InViewport )
{
	guard(UXViewport::MakeCurrent);
	for( INT i=0; i<Viewports.Num(); i++ )
	{
		UViewport* OldViewport = Viewports(i);
		if( OldViewport->Current && OldViewport!=InViewport )
		{
			OldViewport->Current = 0;
			OldViewport->UpdateWindowFrame();
		}
	}
	if( InViewport )
	{
		InViewport->Current = 1;
		InViewport->UpdateWindowFrame();
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	End.
-----------------------------------------------------------------------------*/
