/*=============================================================================
	UnEngineWin.h: Unreal engine windows-specific code.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

/*-----------------------------------------------------------------------------
	Exec hook.
-----------------------------------------------------------------------------*/

// FExecHook.
class FExecHook : public FExec, public FNotifyHook
{
private:
	WConfigProperties* Preferences;
	void NotifyDestroy( void* Src )
	{
		if( Src==Preferences )
			Preferences = NULL;
	}
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar )
	{
		guard(FExecHook::Exec);
		if( ParseCommand(&Cmd,TEXT("ShowLog")) )
		{
			if( GLogWindow )
			{
				GLogWindow->Show(1);
				SetFocus( *GLogWindow );
				GLogWindow->Display.ScrollCaret();
			}
			return 1;
		}
		else if( ParseCommand(&Cmd,TEXT("TakeFocus")) )
		{
			TObjectIterator<UEngine> EngineIt;
			if
			(	EngineIt
			&&	EngineIt->Client
			&&	EngineIt->Client->Viewports.Num() )
				SetForegroundWindow( (HWND)EngineIt->Client->Viewports(0)->GetWindow() );
			return 1;
		}
		else if( ParseCommand(&Cmd,TEXT("EditActor")) )
		{
			UClass* Class;
			TObjectIterator<UEngine> EngineIt;
			if( EngineIt && ParseObject<UClass>( Cmd, TEXT("Class="), Class, ANY_PACKAGE ) )
			{
				AActor* Player  = EngineIt->Client ? EngineIt->Client->Viewports(0)->Actor : NULL;
				AActor* Found   = NULL;
				FLOAT   MinDist = 999999.0;
				for( TObjectIterator<AActor> It; It; ++It )
				{
					FLOAT Dist = Player ? FDist(It->Location,Player->Location) : 0.0;
					if
					(	(!Player || It->GetLevel()==Player->GetLevel())
					&&	(!It->bDeleteMe)
					&&	(It->IsA( Class) )
					&&	(Dist<MinDist) )
					{
						MinDist = Dist;
						Found   = *It;
					}
				}
				if( Found )
				{
					WObjectProperties* P = new WObjectProperties( TEXT("EditActor"), 0, TEXT(""), NULL, 1 );
					P->OpenWindow( (HWND)EngineIt->Client->Viewports(0)->GetWindow() );
					P->Root.SetObjects( (UObject**)&Found, 1 );
					P->Show(1);
				}
				else Ar.Logf( TEXT("Actor not found") );
			}
			else Ar.Logf( TEXT("Missing class") );
			return 1;
		}
		else if( ParseCommand(&Cmd,TEXT("HideLog")) )
		{
			if( GLogWindow )
				GLogWindow->Show(0);
			return 1;
		}
		else if( ParseCommand(&Cmd,TEXT("Preferences")) && !GIsClient )
		{
			if( !Preferences )
			{
				Preferences = new WConfigProperties( TEXT("Preferences"), LocalizeGeneral("AdvancedOptionsTitle",TEXT("Window")) );
				Preferences->SetNotifyHook( this );
				Preferences->OpenWindow( GLogWindow ? GLogWindow->hWnd : NULL );
				Preferences->ForceRefresh();
			}
			Preferences->Show(1);
			SetFocus( *Preferences );
			return 1;
		}
		else return 0;
		unguard;
	}
public:
	FExecHook()
	: Preferences( NULL )
	{}
};

#if 1 //NEW:UG
/*-----------------------------------------------------------------------------
	Config wizard.
-----------------------------------------------------------------------------*/

class WConfigWizard : public WWizardDialog
{
	DECLARE_WINDOWCLASS(WConfigWizard,WWizardDialog,Startup)
	WLabel LogoStatic;
	FWindowsBitmap LogoBitmap;
	UBOOL Cancel;
	FString Title;
	WConfigWizard()
	: LogoStatic(this,IDC_Logo)
	, Cancel(0)
	{}
	void OnInitDialog()
	{
		guard(WStartupWizard::OnInitDialog);
		WWizardDialog::OnInitDialog();
		SendMessageX( *this, WM_SETICON, ICON_BIG, (WPARAM)LoadIconIdX(hInstance,IDICON_Mainframe) );
		LogoBitmap.LoadFile( TEXT("ConfigLogo.bmp") );
		SendMessageX( LogoStatic, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)LogoBitmap.GetBitmapHandle() );
		SetText( *Title );
		SetForegroundWindow( hWnd );
		unguard;
	}
	void OnCancel()
	{
		GIsCriticalError = 1;
		ExitProcess( 0 );
	}
};

class WConfigPageRenderer : public WWizardPage
{
	DECLARE_WINDOWCLASS(WConfigPageRenderer,WWizardPage,Startup);
	WConfigWizard* Owner;
	WListBox RenderList;
	WLabel RenderNote;
	TArray<FRegistryObjectInfo> RenderDevices;
	WConfigPageRenderer( WConfigWizard* InOwner )
	: WWizardPage( TEXT("ConfigPageRenderer"), IDDIALOG_ConfigPageRenderer, InOwner )
	, Owner(InOwner)
	, RenderList(this,IDC_RenderList)
	, RenderNote(this,IDC_RenderNote)
	{}
	void OnInitDialog()
	{
		WWizardPage::OnInitDialog();
		RenderList.SelectionChangeDelegate = FDelegate(this,(TDelegate)CurrentChange);
		RenderList.DoubleClickDelegate = FDelegate(Owner,(TDelegate)WWizardDialog::OnFinish);
		RefreshList();
	}
	void RefreshList()
	{
		RenderList.Empty();
		RenderDevices.Empty();
		UObject::GetRegistryObjects( RenderDevices, UClass::StaticClass(), URenderDevice::StaticClass(), 0 );
		FString Default;
		for( TArray<FRegistryObjectInfo>::TIterator It(RenderDevices); It; ++It )
		{
			FString Path=It->Object, Left, Right, Temp;
			if( Path.Split(TEXT("."),&Left,&Right) )
			{
				RenderList.AddString( *(Temp=Localize(*Right,TEXT("ClassCaption"),*Left)) );
				if( Default==TEXT("") )
				{
					Default=Temp;
				}
			}
		}
		if( Default!=TEXT("") )
			RenderList.SetCurrent( RenderList.FindStringChecked(*Default), 1 );
		CurrentChange();
	}
	void CurrentChange()
	{
		if( CurrentDriver()!=TEXT("") )
		{
			GConfig->SetString(TEXT("Engine.Engine"),TEXT("GameRenderDevice"),*CurrentDriver());
		}
		RenderNote.SetText(Localize(TEXT("Descriptions"),*CurrentDriver(),TEXT("Startup"),NULL,1));
	}
	FString CurrentDriver()
	{
		if( RenderList.GetCurrent()>=0 )
		{
			FString Name = RenderList.GetString(RenderList.GetCurrent());
			for( TArray<FRegistryObjectInfo>::TIterator It(RenderDevices); It; ++It )
			{
				FString Path=It->Object, Left, Right, Temp;
				if( Path.Split(TEXT("."),&Left,&Right) )
					if( Name==Localize(*Right,TEXT("ClassCaption"),*Left) )
						return Path;
			}
		}
		return TEXT("");
	}
	const TCHAR* GetNextText()
	{
		return NULL;
	}
	const TCHAR* GetFinishText()
	{
		return LocalizeGeneral("FinishButton",TEXT("Window"));
	}
};
#endif

/*-----------------------------------------------------------------------------
	Startup and shutdown.
-----------------------------------------------------------------------------*/

//
// Initialize.
//
static UEngine* InitEngine()
{
	guard(InitEngine);
	DOUBLE LoadTime = appSeconds();

	// Set exec hook.
	static FExecHook GLocalHook;
	GExec = &GLocalHook;

	// Create mutex so installer knows we're running.
	CreateMutexX( NULL, 0, TEXT("UnrealIsRunning") );

	// First-run menu.
	INT FirstRun=0;
	GConfig->GetInt( TEXT("FirstRun"), TEXT("FirstRun"), FirstRun );
	if( ParseParam(appCmdLine(),TEXT("FirstRun")) )
		FirstRun=0;

#if 1 //UG
	if( !GIsEditor && GIsClient )
	{
		if( FirstRun<ENGINE_VERSION || ParseParam(appCmdLine(),TEXT("changevideo")) )
		{
			WConfigWizard VideoDialog;
			VideoDialog.Title = LocalizeGeneral(TEXT("Video"),TEXT("Startup") );
			WWizardPage* Page = new WConfigPageRenderer( &VideoDialog );
			if( Page )
			{
				VideoDialog.Advance( Page );
				if( !VideoDialog.DoModal() )
					return NULL;
			}
		}
	}
#endif

	// First-run and version upgrading code.
	if( FirstRun<ENGINE_VERSION && !GIsEditor && GIsClient )
	{
		// Get system directory.
		TCHAR SysDir[256]=TEXT(""), WinDir[256]=TEXT("");
#if UNICODE
		if( !GUnicodeOS )
		{
			ANSICHAR ASysDir[256]="", AWinDir[256]="";
			GetSystemDirectoryA( ASysDir, ARRAY_COUNT(SysDir) );
			GetWindowsDirectoryA( AWinDir, ARRAY_COUNT(WinDir) );
			appStrcpy( SysDir, ANSI_TO_TCHAR(ASysDir) );
			appStrcpy( WinDir, ANSI_TO_TCHAR(AWinDir) );
		}
		else
#endif
		{
			GetSystemDirectory( SysDir, ARRAY_COUNT(SysDir) );
			GetWindowsDirectory( WinDir, ARRAY_COUNT(WinDir) );
		}
		if( FirstRun<220 )
		{
			// Migrate savegames.
			TArray<FString> Saves = GFileManager->FindFiles( TEXT("..\\Save\\*.usa"), 1, 0 );
			for( TArray<FString>::TIterator It(Saves); It; ++It )
			{
				INT Pos = appAtoi(**It+4);
				FString Section = TEXT("UnrealShare.UnrealSlotMenu");
				FString Key     = FString::Printf(TEXT("SlotNames[%i]"),Pos);
				if( appStricmp(GConfig->GetStr(*Section,*Key,TEXT("user")),TEXT(""))==0 )
					GConfig->SetString(*Section,*Key,TEXT("Saved game"),TEXT("user"));
			}
		}
		if( FirstRun<220 && !GIsMMX )
		{
			// Optimize for non-MMX.
			::MessageBox( NULL, Localize(TEXT("FirstRun"),TEXT("NonMMX"),TEXT("Window")), Localize(TEXT("FirstRun"),TEXT("Caption"),TEXT("Window")), MB_OK|MB_ICONINFORMATION|MB_TASKMODAL );
			GConfig->SetString( TEXT("Galaxy.GalaxyAudioSubsystem"), TEXT("OutputRate"),      TEXT("11025Hz") );
			GConfig->SetString( TEXT("Galaxy.GalaxyAudioSubsystem"), TEXT("UseReverb"),       TEXT("False") );
			GConfig->SetString( TEXT("Galaxy.GalaxyAudioSubsystem"), TEXT("UseSpatial"),      TEXT("False") );
			GConfig->SetString( TEXT("Galaxy.GalaxyAudioSubsystem"), TEXT("UseFilter"),       TEXT("False") );
			GConfig->SetString( TEXT("Galaxy.GalaxyAudioSubsystem"), TEXT("EffectsChannels"), TEXT("8") );
		}
		if( FirstRun<220 && GPhysicalMemory <= 32*1024*1024 )
		{
			// Low memory detection.
			TCHAR Temp[4096];
			appSprintf( Temp, Localize(TEXT("FirstRun"),TEXT("LowMemory"),TEXT("Window")), GPhysicalMemory/1024/1024 );
			::MessageBox( NULL, Temp, Localize(TEXT("FirstRun"),TEXT("Caption"),TEXT("Window")), MB_OK|MB_ICONINFORMATION|MB_TASKMODAL );
			GConfig->SetBool( TEXT("Galaxy.GalaxyAudioSubsystem"), TEXT("LowSoundQuality"), 1 );
			GConfig->SetBool( TEXT("WinDrv.WindowsClient"), TEXT("LowDetailTextures"), 1 );
		}
		if( FirstRun<220 && (!GIsMMX || !GIsPentiumPro) )
		{
			// Low res detection.
			GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("WindowedViewportX"), TEXT("320") );
			GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("WindowedViewportY"), TEXT("240") );
			GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("WindowedColorBits"), TEXT("16") );
			GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("FullscreenViewportX"), TEXT("320") );
			GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("FullscreenViewportY"), TEXT("240") );
			GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("FullscreenColorBits"), TEXT("16") );
		}
#if 0 //NEW:UG
		if( FirstRun<220 )
		{
			// Autodetect and ask about detected render devices.
			TArray<FRegistryObjectInfo> RenderDevices;
			UObject::GetRegistryObjects( RenderDevices, UClass::StaticClass(), URenderDevice::StaticClass(), 0 );
			for( INT i=0; i<RenderDevices.Num(); i++ )
			{
				TCHAR File1[256], File2[256];
				appSprintf( File1, TEXT("%s\\%s"), SysDir, *RenderDevices(i).Autodetect );
				appSprintf( File2, TEXT("%s\\%s"), WinDir, *RenderDevices(i).Autodetect );
				if( RenderDevices(i).Autodetect!=TEXT("") && (GFileManager->FileSize(File1)>=0 || GFileManager->FileSize(File2)>=0) )
				{
					TCHAR Path[256], *Str;
					appStrcpy( Path, *RenderDevices(i).Object );
					Str = appStrstr(Path,TEXT("."));
					if( Str )
					{
						*Str++ = 0;
						if( ::MessageBox( NULL, Localize(Str,TEXT("AskInstalled"),Path), Localize(TEXT("FirstRun"),TEXT("Caption"),TEXT("Window")), MB_YESNO|MB_ICONQUESTION|MB_TASKMODAL )==IDYES )
						{
							if( ::MessageBox( NULL, Localize(Str,TEXT("AskUse"),Path), Localize(TEXT("FirstRun"),TEXT("Caption"),TEXT("Window")), MB_YESNO|MB_ICONQUESTION|MB_TASKMODAL )==IDYES )
							{
								GConfig->SetString( TEXT("Engine.Engine"), TEXT("GameRenderDevice"), *RenderDevices(i).Object );
								GConfig->SetString( TEXT("SoftDrv.SoftwareRenderDevice"), TEXT("HighDetailActors"), TEXT("True") );
								break;
							}
						}
					}
				}
			}
		}
#endif
		if( FirstRun<ENGINE_VERSION )
		{
			// Initiate first run of this version.
			FirstRun = ENGINE_VERSION;
			GConfig->SetInt( TEXT("FirstRun"), TEXT("FirstRun"), FirstRun );
			::MessageBox( NULL, Localize(TEXT("FirstRun"),TEXT("Starting"),TEXT("Window")), Localize(TEXT("FirstRun"),TEXT("Caption"),TEXT("Window")), MB_OK|MB_ICONINFORMATION|MB_TASKMODAL );
		}
	}

	// Cd check.
	FString CdPath;
	GConfig->GetString( TEXT("Engine.Engine"), TEXT("CdPath"), CdPath );
	if
	(	CdPath!=TEXT("")
	&&	GFileManager->FileSize(TEXT("..\\Textures\\Palettes.utx"))<=0 )//oldver
	{
		FString Check = CdPath * TEXT("Textures\\Palettes.utx");
		while( !GIsEditor && GFileManager->FileSize(*Check)<=0 )
		{
			if( MessageBox
			(
				NULL,
				LocalizeGeneral("InsertCdText",TEXT("Window")),
				LocalizeGeneral("InsertCdTitle",TEXT("Window")),
				MB_TASKMODAL|MB_OKCANCEL
			)==IDCANCEL )
			{
				GIsCriticalError = 1;
				ExitProcess( 0 );
			}
		}
	}

	// Display the damn story to appease the German censors.
	UBOOL CanModifyGore=1;
	GConfig->GetBool( TEXT("UnrealI.UnrealGameOptionsMenu"), TEXT("bCanModifyGore"), CanModifyGore );
	if( !CanModifyGore && !GIsEditor )
	{
		FString S;
		if( appLoadFileToString( S, TEXT("Story.txt") ) )
		{
			WTextScrollerDialog Dlg( TEXT("The Story"), *S );
			Dlg.DoModal();
		}
	}

	// Create the global engine object.
	UClass* EngineClass;
	if( !GIsEditor )
	{
		// Create game engine.
		EngineClass = UObject::StaticLoadClass( UGameEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.GameEngine"), NULL, LOAD_NoFail, NULL );
	}
	else
	{
		// Editor.
		EngineClass = UObject::StaticLoadClass( UEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail, NULL );
	}
	UEngine* Engine = ConstructObject<UEngine>( EngineClass );
	Engine->Init();
	debugf( TEXT("Startup time: %f seconds"), appSeconds()-LoadTime );

	return Engine;
	unguard;
}

//
// Unreal's main message loop.  All windows in Unreal receive messages
// somewhere below this function on the stack.
//
static void MainLoop( UEngine* Engine )
{
	guard(MainLoop);
	check(Engine);

	// Enter main loop.
	guard(EnterMainLoop);
	if( GLogWindow )
		GLogWindow->SetExec( Engine );
	unguard;

	// Loop while running.
	GIsRunning = 1;
	DWORD ThreadId = GetCurrentThreadId();
	HANDLE hThread = GetCurrentThread();
	DOUBLE OldTime = appSeconds();
	while( GIsRunning && !GIsRequestingExit )
	{
		// Update the world.
		guard(UpdateWorld);
		DOUBLE NewTime   = appSeconds();
		FLOAT  DeltaTime = NewTime - OldTime;
		Engine->Tick( DeltaTime );
		if( GWindowManager )
			GWindowManager->Tick( DeltaTime );
		OldTime = NewTime;
		unguard;

		// Enforce optional maximum tick rate.
		guard(EnforceTickRate);
		FLOAT MaxTickRate = Engine->GetMaxTickRate();
		if( MaxTickRate>0.0 )
		{
			FLOAT Delta = (1.0/MaxTickRate) - (appSeconds()-OldTime);
			appSleep( Max(0.f,Delta) );
		}
		unguard;

		// Handle all incoming messages.
		guard(MessagePump);
		MSG Msg;
		//GTempDouble=0;
		while( PeekMessageX(&Msg,NULL,0,0,PM_REMOVE) )
		{
			if( Msg.message == WM_QUIT )
				GIsRequestingExit = 1;

			guard(TranslateMessage);
			TranslateMessage( &Msg );
			unguardf(( TEXT("%08X %i"), (INT)Msg.hwnd, Msg.message ));

			guard(DispatchMessage);
			DispatchMessageX( &Msg );
			unguardf(( TEXT("%08X %i"), (INT)Msg.hwnd, Msg.message ));
		}
		unguard;

		// If editor thread doesn't have the focus, don't suck up too much CPU time.
		if( GIsEditor )
		{
			guard(ThrottleEditor);
			static UBOOL HadFocus=1;
			UBOOL HasFocus = (GetWindowThreadProcessId(GetForegroundWindow(),NULL) == ThreadId );
			if( HadFocus && !HasFocus )
			{
				// Drop our priority to speed up whatever is in the foreground.
				SetThreadPriority( hThread, THREAD_PRIORITY_BELOW_NORMAL );
			}
			else if( HasFocus && !HadFocus )
			{
				// Boost our priority back to normal.
				SetThreadPriority( hThread, THREAD_PRIORITY_NORMAL );
			}
			if( !HasFocus )
			{
				// Surrender the rest of this timeslice.
				Sleep(0);
			}
			HadFocus = HasFocus;
			unguard;
		}
	}
	GIsRunning = 0;

	// Exit main loop.
	guard(ExitMainLoop);
	if( GLogWindow )
		GLogWindow->SetExec( NULL );
	GExec = NULL;
	unguard;

	unguard;
}

/*-----------------------------------------------------------------------------
	Splash screen.
-----------------------------------------------------------------------------*/

//
// Splash screen, implemented with old-style Windows code so that it
// can be opened super-fast before initialization.
//
BOOL CALLBACK SplashDialogProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	return uMsg==WM_INITDIALOG;
}
HWND hWndSplash = NULL;
void InitSplash( HINSTANCE hInstance, UBOOL Show, const TCHAR* Filename )
{
	if( Show )
	{
		hWndSplash = TCHAR_CALL_OS(CreateDialogW(hInstance, MAKEINTRESOURCEW(IDDIALOG_Splash), NULL, SplashDialogProc),CreateDialogA(hInstance, MAKEINTRESOURCEA(IDDIALOG_Splash), NULL, SplashDialogProc) );
		check(hWndSplash);
		FWindowsBitmap Bitmap;
		verify(Bitmap.LoadFile( Filename ) );
		INT screenWidth  = GetSystemMetrics( SM_CXSCREEN );
		INT screenHeight = GetSystemMetrics( SM_CYSCREEN );
		HWND hWndLogo = GetDlgItem(hWndSplash,IDC_Logo);
		check(hWndLogo);
		ShowWindow( hWndSplash, SW_SHOW );
		SendMessageX( hWndLogo, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)Bitmap.GetBitmapHandle() );
		SetWindowPos( hWndSplash, NULL, (screenWidth - Bitmap.SizeX)/2, (screenHeight - Bitmap.SizeY)/2, Bitmap.SizeX, Bitmap.SizeY, 0 );
		UpdateWindow( hWndSplash );
	}
}
void ExitSplash()
{
	if( hWndSplash )
		DestroyWindow( hWndSplash );
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
