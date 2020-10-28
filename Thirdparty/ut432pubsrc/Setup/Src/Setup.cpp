/*=============================================================================
	Filer.cpp: Unreal installer/filer.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

// System includes.
#pragma warning( disable : 4201 )
#define STRICT
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include "Res\resource.h"

// Unreal includes.
#include "SetupPrivate.h"
#include "USetupDefinitionWindows.h"
#include "Window.h"

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

// Package implementation.
IMPLEMENT_PACKAGE(Setup)

// Functions.
class WWizardPage* NewAutoPlayPage( class WFilerWizard* InOwner, UBOOL ShowInstallOptions );
class FInstallPoll GNoPoll;

// Memory allocator.
#include "FMallocWindows.h"
FMallocWindows Malloc;

// Error handler.
#include "FOutputDeviceWindowsError.h"
FOutputDeviceWindowsError Error;

// Feedback.
#include "FFeedbackContextWindows.h"
FFeedbackContextWindows Warn;

// File manager.
#include "FFileManagerWindows.h"
FFileManagerWindows FileManager;

// Config.
#include "FConfigCacheIni.h"

/*-----------------------------------------------------------------------------
	Helpers.
-----------------------------------------------------------------------------*/

// HRESULT checking.
#define verifyHRESULT(fn) {HRESULT hRes=fn; if( hRes!=S_OK ) appErrorf( TEXT(#fn) TEXT(" failed (%08X)"), hRes );}
#define verifyHRESULTSlow(fn) if(fn){}

void regSet( HKEY Base, FString Dir, FString Key, FString Value )
{
	guard(regSet);
	HKEY hKey = NULL;
	check(RegCreateKeyX( Base, *Dir, &hKey )==ERROR_SUCCESS);
	check(RegSetValueExX( hKey, *Key, 0, REG_SZ, (BYTE*)*Value, (Value.Len()+1)*sizeof(TCHAR) )==ERROR_SUCCESS);
	check(RegCloseKey( hKey )==ERROR_SUCCESS);
	unguard;
}
UBOOL regGet( HKEY Base, FString Dir, FString Key, FString& Str )
{
	guard(regGetFString);
	HKEY hKey = NULL;
	if( RegOpenKeyX( Base, *Dir, &hKey )==ERROR_SUCCESS )
	{
		TCHAR Buffer[4096]=TEXT("");
		DWORD Type=REG_SZ, BufferSize=sizeof(Buffer);
		if
		(	RegQueryValueExX( hKey, *Key, 0, &Type, (BYTE*)Buffer, &BufferSize )==ERROR_SUCCESS
		&&	Type==REG_SZ )
		{
			Str = Buffer;
			return 1;
		}
	}
	Str = TEXT("");
	return 0;
	unguard;
}
SQWORD FreeSpace( const TCHAR* Folder )
{
	guard(FreeSpace);
	if( appStrlen(Folder) && appIsAlpha(Folder[0]) )
	{
		TCHAR Root[]=TEXT("C:") PATH_SEPARATOR;
		Root[0] = Folder[0];
		DWORD SectorsPerCluster=0, BytesPerSector=0, FreeClusters=0, TotalClusters=0;
		GetDiskFreeSpaceX( Root, &SectorsPerCluster, &BytesPerSector, &FreeClusters, &TotalClusters );
		return (QWORD)BytesPerSector * (QWORD)SectorsPerCluster * (QWORD)FreeClusters;
	}
	else return 0;
	unguard;
}

//
// Remove a directory if it's empty. Returns error.
//
static UBOOL IsDrive( const TCHAR* Path )
{
	if( appStricmp(Path,TEXT(""))==0 )
		return 1;
	else if( appToUpper(Path[0])!=appToLower(Path[0]) && Path[1]==':' && Path[2]==0 )
		return 1;
	else if( appStricmp(Path,TEXT("\\"))==0 )
		return 1;
	else if( appStricmp(Path,TEXT("\\\\"))==0 )
		return 1;
	else if( Path[0]=='\\' && Path[1]=='\\' && !appStrchr(Path+2,'\\') )
		return 1;
	else if( Path[0]=='\\' && Path[1]=='\\' && appStrchr(Path+2,'\\') && !appStrchr(appStrchr(Path+2,'\\')+1,'\\') )
		return 1;
	else
		return 0;
}
UBOOL RemoveEmptyDirectory( FString Dir )
{
	for( ; ; )
	{
		if( Dir.Right(1)==PATH_SEPARATOR )
			Dir = Dir.LeftChop(1);
		if( IsDrive(*Dir) )
			break;
		TArray<FString> List = GFileManager->FindFiles( *(Dir * TEXT("*")), 1, 1 );
		if( List.Num() )
			break;
		if( !GFileManager->DeleteDirectory( *Dir, 1, 0 ) )
			return 0;
		while( Dir.Len() && Dir.Right(1)!=PATH_SEPARATOR )
			Dir = Dir.LeftChop(1);
	}
	return 1;
}
void LocalizedFileError( const TCHAR* Key, const TCHAR* AdviceKey, const TCHAR* Filename )
{
	guard(LocalizedError);
	appErrorf( NAME_FriendlyError, *FString::Printf(TEXT("%s: %s (%s)\n\n%s"),LocalizeError(Key),Filename,appGetSystemErrorMessage(),LocalizeError(AdviceKey)) );
	unguard;
}

/*-----------------------------------------------------------------------------
	Install wizard.
-----------------------------------------------------------------------------*/

// Filer wizard.
class WFilerWizard : public WWizardDialog
{
	DECLARE_WINDOWCLASS(WFilerWizard,WWizardDialog,Setup)

	// Config info.
	WLabel LogoStatic;
	FWindowsBitmap LogoBitmap;
	USetupDefinitionWindows* Manager;

	// Constructor.
	WFilerWizard()
	:	LogoStatic		( this, IDC_Logo )
	,   Manager         ( new(UObject::CreatePackage(NULL,MANIFEST_FILE), TEXT("Setup"))USetupDefinitionWindows )
	{
		guard(WFilerWizard::WFilerWizard);
		Manager->Init();
		if( Manager->Uninstalling )
			Manager->CreateRootGroup();
		unguard;
	}

	// WWindow interface.
	void OnInitDialog()
	{
		guard(WFilerWizard::OnInitDialog);

		// Dialog init.
		WWizardDialog::OnInitDialog();
		Manager->hWndManager = hWnd;
		SendMessageX( *this, WM_SETICON, ICON_BIG, (WPARAM)LoadIconIdX(hInstance,IDICON_Setup1) );
		if( Manager->Logo==TEXT("") || !Manager->LocateSourceFile( Manager->Logo ) )
		{
			Manager->Logo = TEXT("..\\Help\\Logo.bmp");//!!for setup
			if( GFileManager->FileSize(*Manager->Logo)<=0 )
				Manager->Logo = TEXT("Logo.bmp");//!!for uninstaller
		}
		LogoBitmap.LoadFile( *Manager->Logo );
		SendMessageX( LogoStatic, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)LogoBitmap.GetBitmapHandle() );

		// Windows init.
		CoInitialize(NULL);

		unguard;
	}

	// WFilerWizard interface.
	void OnFinish()
	{
		guard(WFilerWizard::OnFinish);
		WWizardDialog::OnFinish();
		Manager->PreExit();
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	Product information.
-----------------------------------------------------------------------------*/

// Product information box.
class WProductInfo : public WDialog
{
	DECLARE_WINDOWCLASS(WProductInfo,WDialog,Setup)

	// Variables.
	USetupProduct* SetupProduct;
	WLabel ProductText;
	WLabel VersionText;
	WLabel DeveloperText;
	WButton ProductHolder;
	WUrlButton Product;
	WUrlButton Version;
	WUrlButton Developer;

	// Constructor.
	WProductInfo( WWindow* InOwner, USetupDefinition* Manager, USetupProduct* InSetupProduct )
	: WDialog		( TEXT("ProductInfo"), IDDIALOG_ProductInfo, InOwner )
	, SetupProduct  ( InSetupProduct )
	, ProductText   ( this, IDC_ProductText )
	, VersionText   ( this, IDC_VersionText )
	, DeveloperText ( this, IDC_DeveloperText )
	, ProductHolder ( this, IDC_ProductHolder )
	, Product       ( this, TEXT(""), IDC_Product   )
	, Version       ( this, TEXT(""), IDC_Version   )
	, Developer     ( this, TEXT(""), IDC_Developer )
	{}

	// WDialog interface.
	void LanguageChange()
	{
		guard(WProductInfo::LanguageChange);
		//!!super::languagechange

		// Product text.
		Product  .URL    =  SetupProduct->ProductURL;
		Version  .URL    =  SetupProduct->VersionURL;
		Developer.URL    =  SetupProduct->DeveloperURL;
		Product  .SetText( *SetupProduct->LocalProduct );
		Developer.SetText( *SetupProduct->Developer    );
		Version  .SetText( *SetupProduct->Version      );

		// General text.
		ProductText   .SetText(Localize("IDDIALOG_ProductInfo","IDC_ProductText"));
		VersionText   .SetText(Localize("IDDIALOG_ProductInfo","IDC_VersionText"));
		DeveloperText .SetText(Localize("IDDIALOG_ProductInfo","IDC_DeveloperText"));
		ProductHolder .SetText(Localize("IDDIALOG_ProductInfo","IDC_ProductHolder"));

		unguard;
	}
};

/*-----------------------------------------------------------------------------
	Failed requirement.
-----------------------------------------------------------------------------*/

// A password dialog box.
class WFailedRequirement : public WDialog
{
	DECLARE_WINDOWCLASS(WFailedRequirement,WDialog,Setup)

	// Controls.
	WCoolButton OkButton;
	WProductInfo ProductInfo;
	WLabel FailedText;
	FString Title;
	FString Message;

	// Constructor.
	WFailedRequirement( WWindow* InOwnerWindow, USetupDefinition* Manager, USetupProduct* InProduct, const TCHAR* InTitle, const TCHAR* InMessage )
	: WDialog	  ( TEXT("FailedRequirement"), IDDIALOG_FailedRequirement, InOwnerWindow )
	, FailedText  ( this, IDC_FailedMessage )
	, OkButton    ( this, IDOK, FDelegate(this,(TDelegate)EndDialogTrue) )
	, ProductInfo ( this, Manager, InProduct )
	, Title       ( InTitle )
	, Message     ( InMessage )
	{}
	void OnInitDialog()
	{
		guard(WFailedRequirement::OnInitDialog);
		WDialog::OnInitDialog();
		SetText( *Title );
		ProductInfo.OpenChildWindow( IDC_ProductInfoHolder, 1 );
		ProductInfo.LanguageChange();
		ProductInfo.ProductHolder.SetText( Localize("IDDIALOG_FailedRequirement","IDC_ProductHolder") );
		FailedText.SetText( LineFormat(*Message) );
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	Install components.
-----------------------------------------------------------------------------*/

// Base of components page.
class WFilerPageComponentsBase : public WWizardPage
{
	DECLARE_WINDOWCLASS(WFilerPageComponentsBase,WWizardPage,Setup)
	WFilerWizard* Owner;
	WFilerPageComponentsBase( const TCHAR* InText, INT InID, WFilerWizard* InOwner )
	: WWizardPage( InText, InID, InOwner )
	, Owner( InOwner )
	{}
	virtual void OnGroupChange( class FComponentItem* Group )=0;
};

// An component list item.
class FComponentItem : public FHeaderItem
{
public:
	// Variables.
	WFilerPageComponentsBase* OwnerComponents;
	USetupGroup* SetupGroup;
	UBOOL Forced;

	// Constructors.
	FComponentItem( WFilerPageComponentsBase* InOwnerComponents, USetupGroup* InSetupGroup, WPropertiesBase* InOwnerProperties, FTreeItem* InParent )
	: FHeaderItem		( InOwnerProperties, InParent, 1 )
	, OwnerComponents	( InOwnerComponents )
	, SetupGroup        ( InSetupGroup )
	, Forced            ( 0 )
	{
		guard(FComponentItem::FComponentItem);

		// Get subgroups.
		Sorted = SetupGroup->Manager->Uninstalling;
		for( TArray<USetupGroup*>::TIterator It(SetupGroup->Subgroups); It; ++It )
			if( (*It)->Visible )
				Children.AddItem( new(TEXT("FComponentItem"))FComponentItem(OwnerComponents,*It,OwnerProperties,this) );
		Expandable = Children.Num()>0;

		unguard;
	}
	FRect GetCheckboxRect()
	{
		guard(FComponentItem::GetCheckboxRect);
		return FRect(GetRect()-GetRect().Min).Right(16).Inner(FPoint(0,1))+FPoint(0,1);
		unguard;
	}

	// FTreeItem interface.
	UBOOL Greyed()
	{
		guard(FComponentItem::Greyed);
		if( SetupGroup->Manager->Uninstalling )
			return Forced;
		else
			for( FComponentItem* Item=(FComponentItem*)Parent; Item; Item=(FComponentItem*)Item->Parent )
				if( !Item->SetupGroup->Selected )
					return 1;
		return 0;
		unguard;
	}
	void Draw( HDC hDC )
	{
		guard(FComponentItem::Draw);
		FHeaderItem::Draw( hDC );
		DWORD GreyedFlags = SetupGroup->Manager->Uninstalling ? (DFCS_INACTIVE|DFCS_CHECKED) : (DFCS_INACTIVE);
		DrawFrameControl( hDC, GetCheckboxRect()+GetRect().Min, DFC_BUTTON, DFCS_BUTTONCHECK|(Greyed()?GreyedFlags:0)|(SetupGroup->Selected?DFCS_CHECKED:0) );
		unguard;
	}
	void ToggleSelection()
	{
		guard(FTreeItem::ToggleSelection);
		if( SetupGroup->Optional && !Greyed() )
		{
			SetupGroup->Selected = !SetupGroup->Selected;
			OwnerComponents->OnGroupChange( this );
			InvalidateRect( OwnerProperties->List, NULL, 0 );
			UpdateWindow( OwnerProperties->List );
		}
		unguard;
	}
	void OnItemDoubleClick()
	{
		guard(FTreeItem::OnItemDoubleClick);
		ToggleSelection();
		unguard;
	}
	void OnItemLeftMouseDown( FPoint P )
	{
		guard(FComponentItem::OnItemLeftMouseDown);
		if( GetCheckboxRect().Inner(FPoint(-1,-1)).Contains(P) )
			ToggleSelection();
		else
			FHeaderItem::OnItemLeftMouseDown( P );
		unguard;
	}
	void OnItemSetFocus()
	{
		guard(FComponentItem::OnItemSetFocus);
		FHeaderItem::OnItemSetFocus();
		OwnerComponents->OnGroupChange( this );
		unguard;
	}
	QWORD GetId() const
	{
		guard(FConfigItem::GetId);
		return (INT)this;
		unguard;
	}
	virtual FString GetCaption() const
	{
		guard(FConfigItem::GetText);
		return SetupGroup->Caption;
		unguard;
	}
};

// Group properties.
class WComponentProperties : public WProperties
{
	DECLARE_WINDOWCLASS(WComponentProperties,WProperties,Setup)
	FComponentItem Root;
	WComponentProperties( WFilerPageComponentsBase* InComponents )
	: WProperties( NAME_None, InComponents )
	, Root( InComponents, InComponents->Owner->Manager->RootGroup, this, NULL )
	{
		ShowTreeLines = 0;
	}
	FTreeItem* GetRoot()
	{
		return &Root;
	}
};

/*-----------------------------------------------------------------------------
	Install wizard pages.
-----------------------------------------------------------------------------*/

// Progess.
class WFilerPageProgress : public WWizardPage, public FInstallPoll
{
	DECLARE_WINDOWCLASS(WFilerPageProgress,WWizardPage,Setup)

	// Variables.
	WFilerWizard* Owner;
	USetupDefinition* Manager;
	WLabel InstallText;
	WLabel InstallingText;
	WLabel ProgressText;
	WLabel TotalText;
	WLabel Installing;
	WProgressBar Progress;
	WProgressBar Total;
	UBOOL Finished;
	UBOOL CancelFlag;

	// Constructor.
	WFilerPageProgress( WFilerWizard* InOwner, const TCHAR* Template )
	: WWizardPage   ( Template, IDDIALOG_FilerPageProgress, InOwner )
	, Owner         ( InOwner )
	, Manager       ( InOwner->Manager )
	, InstallText   ( this, IDC_InstallText )
	, InstallingText( this, IDC_InstallingText )
	, ProgressText  ( this, IDC_ProgressText )
	, TotalText     ( this, IDC_TotalText )
	, Installing    ( this, IDC_Installing )
	, Progress      ( this, IDC_Progress )
	, Total         ( this, IDC_Total )
	, Finished      ( 0 )
	, CancelFlag	( 0 )
	{}

	// FInstallPoll interface.
	UBOOL Poll( const TCHAR* Label, SQWORD LocalBytes, SQWORD LocalTotal, SQWORD RunningBytes, SQWORD TotalBytes )
	{
		guard(WWizardPageProgress::Poll);
		static TCHAR Saved[256]=TEXT("");
		if( appStricmp(Label,Saved)!=0 )
		{
			Installing.SetText( Label );
			appStrcpy( Saved, Label );
		}
		Progress.SetProgress( LocalBytes, LocalTotal );
		Total.SetProgress( RunningBytes, TotalBytes );
		MSG Msg;
		while( PeekMessageX(&Msg,NULL,0,0,1) )
			DispatchMessageX(&Msg);
		UpdateWindow( *this );
		if( CancelFlag )
			if( MessageBox( *OwnerWindow, *FString::Printf(LocalizeGeneral(TEXT("CancelPrompt")),*Manager->LocalProduct), LocalizeGeneral(TEXT("InstallCancel")), MB_YESNO )==IDYES )
				return 0;
		CancelFlag = 0;
		return 1;
		unguard;
	}
	const TCHAR* GetBackText()
	{
		return NULL;
	}
	void OnCancel()
	{
		CancelFlag = 1;
	}
	const TCHAR* GetNextText()
	{
		return Finished ? WWizardPage::GetNextText() : NULL;
	}
};

// Install progess.
class WFilerPageInstallProgress : public WFilerPageProgress
{
	DECLARE_WINDOWCLASS(WFilerPageInstallProgress,WFilerPageProgress,Setup)
	WFilerPageInstallProgress( WFilerWizard* InOwner )
	: WFilerPageProgress( InOwner, TEXT("FilerPageInstallProgress") )
	{}
	WWizardPage* GetNext()
	{
		guard(WFilerPageInstallProgress::GetNext);
		return NewAutoPlayPage( Owner, 0 );
		unguard;
	}
	void OnCurrent()
	{
		guard(WFilerPageInstallProgress::OnCurrent);
		UpdateWindow( *this );
		Manager->DoInstallSteps( this );
		Finished = 1;
		Owner->OnNext();
		unguard;
	}
};

// Done.
class WFilerPagePreInstall : public WWizardPage
{
	DECLARE_WINDOWCLASS(WFilerPagePreInstall,WWizardPage,Setup)

	// Variables.
	WFilerWizard* Owner;
	USetupDefinition* Manager;
	WLabel Message;

	// Constructor.
	WFilerPagePreInstall( WFilerWizard* InOwner )
	: WWizardPage   ( TEXT("FilerPagePreInstall"), IDDIALOG_FilerPagePreInstall, InOwner )
	, Owner         ( InOwner )
	, Manager       ( InOwner->Manager )
	, Message       ( this, IDC_Message )
	{}

	// WWizardPage interface.
	WWizardPage* GetNext()
	{
		return new WFilerPageInstallProgress( Owner );
	}
	const TCHAR* GetNextText()
	{
		return LocalizeGeneral("InstallButton",TEXT("Window"));
	}
	void OnInitDialog()
	{
		guard(WFilerPagePreInstall::OnInitDialog);
		WWizardPage::OnInitDialog();
		Message.SetText( *FString::Printf( LineFormat(LocalizeGeneral("Ready")), *Manager->LocalProduct, *Manager->DestPath, *Manager->LocalProduct ) );
		unguard;
	}
};

// Folder.
class WFilerPageCdFolder : public WWizardPage, public FControlSnoop
{
	DECLARE_WINDOWCLASS(WFilerPageCdFolder,WWizardPage,Setup)

	// Variables.
	WFilerWizard* Owner;
	USetupDefinition* Manager;
	WLabel FolderDescription;
	WLabel SpaceAvailable;
	WLabel SpaceAvailableMsg;
	WLabel SpaceRequired;
	WLabel SpaceRequiredMsg;
	WButton FolderHolder;
	WCoolButton DefaultButton;
	WEdit Folder;

	// Constructor.
	WFilerPageCdFolder( WFilerWizard* InOwner )
	: WWizardPage      ( TEXT("FilerPageFolder"), IDDIALOG_FilerPageFolder, InOwner )
	, Owner            ( InOwner )
	, Manager          ( InOwner->Manager )
	, FolderDescription( this, IDC_FolderDescription )
	, FolderHolder     ( this, IDC_FolderHolder )
	, DefaultButton    ( this, IDC_Default, FDelegate(this,(TDelegate)OnReset) )
	, Folder           ( this, IDC_Folder )
	, SpaceAvailable   ( this, IDC_SpaceAvailable )
	, SpaceAvailableMsg( this, IDC_SpaceAvailableMessage )
	, SpaceRequired    ( this, IDC_SpaceRequired )
	, SpaceRequiredMsg ( this, IDC_SpaceRequiredMessage )
	{
		guard(WFilerPageCdFolder::WFilerPageCdFolder);
		Manager->RefPath = TEXT("");
		unguard;
	}

	// WWizardPage interface.
	WWizardPage* GetNext()
	{
		guard(WFilerPageCdFolder::GetNext);
		FString Saved      = Manager->RefPath;
		Manager->CdOk      = TEXT("");
		Manager->RefPath   = Folder.GetText();
		Manager->InstallTree( TEXT("ProcessVerifyCd"), &GNoPoll, USetupDefinition::ProcessVerifyCd );
		if( Manager->CdOk!=TEXT("") )
		{
			MessageBox( *Owner, *FString::Printf(LineFormat(LocalizeError(TEXT("WrongCd"))),*Manager->Product,*Manager->CdOk), LocalizeError(TEXT("WrongCdTitle")), MB_OK );
			Manager->RefPath = Saved;
			return NULL;
		}
		return new WFilerPagePreInstall( Owner );
		unguard;
	}
	UBOOL GetShow()
	{
		guard(WFilerPageCdFolder::GetShow);

#if DEMOVERSION
		// This is a hack!!
		Manager->RefPath = Manager->DestPath;
		Manager->AnyRef = 0;
		Manager->InstallTree( TEXT("ProcessCheckRef"), &GNoPoll, USetupDefinition::ProcessCheckRef );
		return 0;
#endif

		// Only show if there are installable files stored as deltas relative to version on CD.
		Manager->AnyRef = 0;
		Manager->InstallTree( TEXT("ProcessCheckRef"), &GNoPoll, USetupDefinition::ProcessCheckRef );
		return Manager->AnyRef;

		unguard;
	}
	void OnInitDialog()
	{
		guard(WFilerPageCdFolder::OnInitDialog);
		WWizardPage::OnInitDialog();
		SpaceAvailable.Show(0);
		SpaceAvailableMsg.Show(0);
		SpaceRequired.Show(0);
		SpaceRequiredMsg.Show(0);
		FolderHolder.SetText( LocalizeGeneral(TEXT("CdDrive")) );
		FolderDescription.SetText( *FString::Printf(LineFormat(LocalizeGeneral(TEXT("CdDescription"))),*Manager->LocalProduct,*Manager->Product,*Manager->LocalProduct) );
		OnReset();
		unguard;
	}
	void OnReset()
	{
		guard(WFilerPageCdFolder::OnReset);
#if DEMOVERSION
		Folder.SetText( *Manager->DestPath );
#else
		Folder.SetText( TEXT("D:") PATH_SEPARATOR );
		TCHAR Str[4] = TEXT("A:") PATH_SEPARATOR;
		for( TCHAR Ch='A'; Ch<='Z'; Ch++ )
		{
			Str[0] = Ch;
			if( GetDriveTypeX(Str)==DRIVE_CDROM )
			{
				Folder.SetText( Str );
				break;
			}
		}
#endif
		unguard;
	}
};

// Components.
class WFilerPageComponents : public WFilerPageComponentsBase
{
	DECLARE_WINDOWCLASS(WFilerPageComponents,WFilerPageComponentsBase,Setup)

	// Variables.
	USetupDefinition* Manager;
	WButton DescriptionFrame;
	WButton DiskSpaceFrame;
	WLabel ComponentsDescription;
	WLabel ComponentsPrompt;
	WLabel DescriptionText;
	WLabel SpaceRequiredMessage;
	WLabel SpaceAvailableMessage;
	WLabel SpaceRequired;
	WLabel SpaceAvailable;
	WComponentProperties Components;

	// Constructor.
	WFilerPageComponents( WFilerWizard* InOwner )
	: WFilerPageComponentsBase( TEXT("FilerPageComponents"), IDDIALOG_FilerPageComponents, InOwner )
	, Manager              ( InOwner->Manager )
	, ComponentsDescription( this, IDC_ComponentsDescription )
	, ComponentsPrompt     ( this, IDC_ComponentsPrompt )
	, DiskSpaceFrame       ( this, IDC_DiskSpaceFrame )
	, DescriptionFrame     ( this, IDC_DescriptionFrame )
	, DescriptionText      ( this, IDC_DescriptionText )
	, SpaceRequiredMessage ( this, IDC_SpaceRequiredMessage )
	, SpaceAvailableMessage( this, IDC_SpaceAvailableMessage )
	, SpaceRequired        ( this, IDC_SpaceRequired )
	, SpaceAvailable       ( this, IDC_SpaceAvailable )
	, Components		   ( this )
	{
		Components.ShowTreeLines = 0;
	}

	// Functions.
	void OnGroupChange( class FComponentItem* Group )
	{
		guard(WFilerPageComponents::OnGroupChange);

		// Update space required.
		Manager->RequiredSpace = PER_INSTALL_OVERHEAD + Manager->RootGroup->SpaceRequired();
		SpaceAvailable.SetText( *FString::Printf(LocalizeGeneral("Space"), FreeSpace(*Manager->DestPath)/(1024*1024) ) );
		SpaceRequired .SetText( *FString::Printf(LocalizeGeneral("Space"), Manager->RequiredSpace/(1024*1024) ) );

		// Update description text.
		DescriptionText.SetText( Group ? *Group->SetupGroup->Description : TEXT("") );

		unguard;
	}

	// WWizardPage interface.
	WWizardPage* GetNext()
	{
		guard(WFilerPageComponents::GetNext);
		if( FreeSpace(*Manager->DestPath) < Manager->RequiredSpace )
		{
			TCHAR Root[]=TEXT("C:") PATH_SEPARATOR;
			Root[0] = (*Manager->DestPath)[0];
			MessageBox( *Owner, *FString::Printf(LineFormat(LocalizeGeneral("NotEnoughSpace")),Root,*Manager->LocalProduct), LocalizeGeneral("NotEnoughSpaceTitle"), MB_OK );
			return NULL;
		}
		return new WFilerPageCdFolder(Owner);
		unguard;
	}
	void OnInitDialog()
	{
		guard(WFilerPageComponents::OnInitDialog);
		WWizardPage::OnInitDialog();
		Components.OpenChildWindow( IDC_ComponentsHolder );

		OnGroupChange( NULL );
		Components.GetRoot()->Expand();
		Components.ResizeList();
		Components.List.SetCurrent( 0, 1 );
		Components.SetItemFocus( 1 );

		unguard;
	}
	UBOOL GetShow()
	{
		return Components.Root.SetupGroup->Visible;
	}
};

// Folder.
class WFilerPageFolder : public WWizardPage, public FControlSnoop
{
	DECLARE_WINDOWCLASS(WFilerPageFolder,WWizardPage,Setup)

	// Variables.
	WFilerWizard* Owner;
	USetupDefinition* Manager;
	WLabel FolderDescription;
	WLabel SpaceAvailable;
	WLabel SpaceRequired;
	WButton FolderHolder;
	WCoolButton DefaultButton;
	WEdit Folder;

	// Constructor.
	WFilerPageFolder( WFilerWizard* InOwner )
	: WWizardPage      ( TEXT("FilerPageFolder"), IDDIALOG_FilerPageFolder, InOwner )
	, Owner            ( InOwner )
	, Manager          ( InOwner->Manager )
	, FolderDescription( this, IDC_FolderDescription )
	, FolderHolder     ( this, IDC_FolderHolder )
	, DefaultButton    ( this, IDC_Default, FDelegate(this,(TDelegate)OnReset) )
	, Folder           ( this, IDC_Folder )
	, SpaceAvailable   ( this, IDC_SpaceAvailable )
	, SpaceRequired    ( this, IDC_SpaceRequired )
	{
		guard(WFilerPageFolder::WFilerPageFolder);
		Manager->DestPath = Manager->RegistryFolder;
		unguard;
	}

	// WWizardPage interface.
	void Update()
	{
		guard(WFilerPageFolder::Update);
		SpaceAvailable.SetText( *FString::Printf(LocalizeGeneral("Space"), FreeSpace(*Folder.GetText())/(1024*1024) ) );
		SpaceRequired .SetText( *FString::Printf(LocalizeGeneral("Space"), Manager->RequiredSpace/(1024*1024) ) );
		unguard;
	}
	WWizardPage* GetNext()
	{
		guard(WFilerPageFolder::GetNext);

		// Get folder name.
		FString NewFolder = Folder.GetText();
		if( NewFolder.Right(1)==PATH_SEPARATOR )
			NewFolder = NewFolder.LeftChop(1);

		// Make sure all requirements are met.
		USetupProduct* RequiredProduct=NULL;
		FString FailMessage;
		if( !Manager->CheckAllRequirements(Folder.GetText(),RequiredProduct,FailMessage) )
		{
			FString Title = FString::Printf( LocalizeError(TEXT("MissingProductTitle")), *Manager->LocalProduct, Manager->Patch ? LocalizeError(TEXT("MissingProductPatched")) : LocalizeError(TEXT("MissingProductInstalled")) );
			WFailedRequirement( OwnerWindow, Manager, RequiredProduct, *Title, *FailMessage ).DoModal();
			return NULL;
		}

		// Try to create folder.
		if
		(	NewFolder.Len()>=4
		&&	appIsAlpha((*NewFolder)[0])
		&&	(*NewFolder)[1]==':'
		&&	(*NewFolder)[2]==PATH_SEPARATOR[0] )
		{
			// Attempt to create the folder.
			if( NewFolder.Right(1)==PATH_SEPARATOR )
				NewFolder = NewFolder.LeftChop(1);
			if( GFileManager->MakeDirectory( *NewFolder, 1 ) )
			{
				Manager->DestPath = NewFolder;
				Manager->CreateRootGroup();
				return new WFilerPageComponents( Owner );
			}
		}
		FString Title = FString::Printf( LocalizeError("FolderTitle" ), *NewFolder );
		FString Msg   = FString::Printf( LocalizeError("FolderFormat"), *NewFolder );
		MessageBox( *Owner, *Msg, *Title, MB_OK );
		OnReset();
		return NULL;
		unguard;
	}
	void OnInitDialog()
	{
		guard(WFilerPageFolder::OnInitDialog);
		WWizardPage::OnInitDialog();
		FolderDescription.SetText( *FString::Printf(LineFormat(Localize("IDDIALOG_FilerPageFolder",Manager->Patch ? "IDC_FolderDescriptionPatch" : "IDC_FolderDescription")), Manager->LocalProduct ) );
		OnReset();
		Folder.ChangeDelegate=FDelegate(this,(TDelegate)OnChange);
		unguard;
	}
	void OnChange()
	{
		guard(WFilerPageFolder::OnChange);
		Update();
		unguard;
	}
	void OnReset()
	{
		guard(WFilerPageFolder::OnReset);
		Folder.SetText( *Manager->RegistryFolder );
		Update();
		unguard;
	}
};

// License.
class WFilerPageLicense : public WWizardPage
{
	DECLARE_WINDOWCLASS(WFilerPageLicense,WWizardPage,Setup)

	// Variables.
	WFilerWizard* Owner;
	USetupDefinition* Manager;
	WLabel LicenseText;
	WLabel LicenseQuestion;
	WEdit  LicenseEdit;

	// Constructor.
	WFilerPageLicense( WFilerWizard* InOwner )
	: WWizardPage    ( TEXT("FilerPageLicense"), IDDIALOG_FilerPageLicense, InOwner )
	, Owner          ( InOwner )
	, Manager        ( InOwner->Manager )
	, LicenseText    ( this, IDC_LicenseText     )
	, LicenseQuestion( this, IDC_LicenseQuestion )
	, LicenseEdit    ( this, IDC_LicenseEdit     )
	{}

	// WWizardPage interface.
	const TCHAR* GetNextText()
	{
		return LocalizeGeneral("AgreeButton",TEXT("Window"));
	}
	WWizardPage* GetNext()
	{
		return new WFilerPageFolder( Owner );
	}
	void OnInitDialog()
	{
		guard(WFilerPageLicense::OnInitDialog);
		WWizardPage::OnInitDialog();
		FString Str;
		if( **Manager->License && appLoadFileToString(Str,*Manager->License) )
			LicenseEdit.SetText( *Str );
		unguard;
	}
	UBOOL GetShow()
	{
		return LicenseEdit.GetText()!=TEXT("");
	}
};

// Progess.
class WFilerPageUninstallProgress : public WFilerPageProgress
{
	DECLARE_WINDOWCLASS(WFilerPageUninstallProgress,WFilerPageProgress,Setup)
	WFilerPageUninstallProgress ( WFilerWizard* InOwner )
	: WFilerPageProgress( InOwner, TEXT("FilerPageUninstallProgress") )
	{}
	WWizardPage* GetNext()
	{
		guard(WFilerPageUninstallProgress::GetNext);
		Owner->OnFinish();
		return NULL;
		unguard;
	}
	void OnCurrent()
	{
		guard(WFilerPageProgress::OnCurrent);
		UpdateWindow( *this );
		Progress.Show(0);
		ProgressText.Show(0);
		Manager->DoUninstallSteps( this );
		Finished = 1;
		Owner->OnNext();
		unguard;
	}
};

// Uninstall screen.
class WFilerPageUninstall : public WWizardPage
{
	DECLARE_WINDOWCLASS(WFilerPageUninstall,WWizardPage,Setup)
	WFilerWizard* Owner;
	USetupDefinition* Manager;
	TArray<USetupGroup*>& Dependencies;
	WLabel Prompt;
	WButton YesButton, NoButton;
	WEdit List;
	WFilerPageUninstall( WFilerWizard* InOwner )
	: WWizardPage   ( TEXT("FilerPageUninstall"), IDDIALOG_FilerPageUninstall, InOwner )
	, Owner         ( InOwner )
	, Manager       ( InOwner->Manager )
	, Dependencies  ( InOwner->Manager->UninstallComponents )
	, YesButton     ( this, IDC_Yes, FDelegate() )
	, NoButton      ( this, IDC_No,  FDelegate() )
	, Prompt        ( this, IDC_UninstallPrompt )
	, List			( this, IDC_UninstallListEdit )
	{}
	void OnInitDialog()
	{
		guard(WFilerPageUninstall::OnInitDialog);
		WWizardPage::OnInitDialog();
		SendMessageX( YesButton, BM_SETCHECK, 1, 0 );
		YesButton.SetText( LocalizeGeneral(TEXT("Yes"),TEXT("Core")) );
		NoButton .SetText( LocalizeGeneral(TEXT("No" ),TEXT("Core")) );
		Prompt   .SetText( *FString::Printf(Localize(TEXT("IDDIALOG_FilerPageUninstall"), TEXT("IDC_UninstallPrompt"), GetPackageName()), Manager->Product) );//!!LocalProduct isn't accessible
		SendMessageX( Prompt, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(0,0) );
		FString Str;
		for( INT i=0; i<Dependencies.Num(); i++ )
			Str += Dependencies(i)->Caption + TEXT("\r\n");
		List.SetText( *Str );
		unguard;
	}
	WWizardPage* GetNext()
	{
		guard(WFilerPageAutoPlay::GetNext);
		if( SendMessageX( YesButton, BM_GETCHECK, 0, 0 )!=BST_CHECKED )
		{
			Owner->OnFinish();
			return NULL;
		}
		else return new WFilerPageUninstallProgress( Owner );
		unguard;
	}
};

// Welcome.
class WFilerPageWelcome : public WWizardPage
{
	DECLARE_WINDOWCLASS(WFilerPageWelcome,WWizardPage,Setup)

	// Variables.
	WFilerWizard*		Owner;
	USetupDefinition*	Manager;
	WLabel				WelcomePrompt;
	WLabel				LanguagePrompt;
	WProductInfo		ProductInfo;
	WListBox			LanguageList;
	TArray<FString>		LanguageNames;
	TArray<FRegistryObjectInfo> Results;

	// Constructor.
	WFilerPageWelcome( WFilerWizard* InOwner )
	: WWizardPage   ( TEXT("FilerPageWelcome"), IDDIALOG_FilerPageWelcome, InOwner )
	, Owner         ( InOwner )
	, Manager       ( InOwner->Manager )
	, WelcomePrompt ( this, IDC_WelcomePrompt )
	, LanguagePrompt( this, IDC_LanguagePrompt )
	, ProductInfo	( this, InOwner->Manager, InOwner->Manager )
	, LanguageList  ( this, IDC_LanguageList )
	{
		guard(WFilerPageWelcome::WFilerPageWelcome);
		LanguageList.SelectionChangeDelegate = FDelegate(this,(TDelegate)OnUserChangeLanguage);
		unguard;
	}

	// WDialog interface.
	void OnCurrent()
	{
		guard(WFilerPageWelcome::OnSetFocus);
		Owner->SetText( *Manager->SetupWindowTitle );
		unguard;
	}
	void OnInitDialog()
	{
		guard(WFilerPageWelcome::OnInitDialog);
		WWizardPage::OnInitDialog();

		// Open product info window.
		ProductInfo.OpenChildWindow( IDC_ProductInfoHolder, 1 );

		// Get keyboard layout info.
		INT UserLangId = GetUserDefaultLangID() & ((1<<10)-1);
		INT UserSubLangId = GetUserDefaultLangID() >> 10;
		debugf( NAME_Init, TEXT("Language %i, Sublanguage %i"), UserLangId, UserSubLangId );

		// Get language list.
		INT Ideal=-1, Best=-1, Current=0;
		UObject::GetRegistryObjects( Results, UClass::StaticClass(), ULanguage::StaticClass(), 0 );
		if( Results.Num()==0 )
			appErrorf( TEXT("No Languages Found") );

		// Pick language matching keyboard layout if one exists, otherwise .int.
		for( INT i=0; i<Results.Num(); i++ )
		{
			TCHAR Name[256];
			INT LangId, SubLangId;
			FString Path = US + TEXT("Core.") + Results(i).Object;
			GConfig->GetString( TEXT("Language"), TEXT("Language"), Name, ARRAY_COUNT(Name), *Path );
			GConfig->GetInt( TEXT("Language"), TEXT("LangId"), LangId, *Path );
			GConfig->GetInt( TEXT("Language"), TEXT("SubLangId"), SubLangId, *Path );
			new(LanguageNames)FString( Name );
			LanguageList.AddString( Name );
			if( appStricmp(*Results(i).Object,TEXT("int"))==0 )
				Current = i;
			if( LangId==UserLangId )
				Best = i;
			if( LangId==UserLangId && SubLangId==UserSubLangId )
				Ideal = i;
		}
		if( Best>=0 )
			Current = Best;
		if( Ideal>=0 )
			Current = Ideal;
		LanguageList.SetCurrent( LanguageList.FindString(*LanguageNames(Current)), 1 );
		OnUserChangeLanguage();

		unguard;
	}

	// WWizardPage interface.
	WWizardPage* GetNext()
	{
		guard(WFilerPageWelcome::GetNext);
		return new WFilerPageLicense(Owner);
		unguard;
	}
	const TCHAR* GetBackText()
	{
		return NULL;
	}

	// WFilerPageWelcome interface.
	void OnUserChangeLanguage()
	{
		guard(WFilerPageWelcome::OnUserChangeLanguage);
		INT Index;
		if( LanguageNames.FindItem(*LanguageList.GetString(LanguageList.GetCurrent()),Index) )
		{
			FString Language = *Results(Index).Object;
			UObject::SetLanguage( *Language );
			GConfig->SetString( TEXT("Setup"), TEXT("Language"), *Language, *Manager->ConfigFile );
		}
		else
		{
			FString Language = TEXT("jpt"); //!!HACK
			UObject::SetLanguage( *Language );
			GConfig->SetString( TEXT("Setup"), TEXT("Language"), *Language, *Manager->ConfigFile );
		}
		LanguageChange();//!!
		unguard;
	}
	virtual void LanguageChange()
	{
		guard(WFilerPageWelcome::LanguageChange);

		// Welcome text.
		WelcomePrompt.SetText( *FString::Printf( LineFormat(Localize("IDDIALOG_FilerPageWelcome",Manager->Patch ? "IDC_WelcomePromptUpdate" : "IDC_WelcomePrompt" )), *Manager->LocalProduct, *Manager->Version ) );

		// Other text.
		Owner->SetText(LineFormat(Localize("IDDIALOG_WizardDialog", "IDC_WizardDialog" )));
		LanguagePrompt.SetText(LineFormat(Localize("IDDIALOG_FilerPageWelcome","IDC_LanguagePrompt")));
		ProductInfo.LanguageChange();//!!
		Owner->RefreshPage();//!!

		unguard;
	}
};

// Components.
class WFilerPageUninstallComponents : public WFilerPageComponentsBase
{
	DECLARE_WINDOWCLASS(WFilerPageUninstallComponents,WFilerPageComponentsBase,Setup)

	// Variables.
	USetupDefinition* Manager;
	TArray<USetupGroup*>& Dependencies;
	WComponentProperties Components;

	// Constructor.
	WFilerPageUninstallComponents( WFilerWizard* InOwner )
	: WFilerPageComponentsBase( TEXT("FilerPageUninstallComponents"), IDDIALOG_FilerPageUninstallComponents, InOwner )
	, Manager              ( InOwner->Manager )
	, Components		   ( this )
	, Dependencies		   ( InOwner->Manager->UninstallComponents )
	{}

	// Functions.
	void OnGroupChange( class FComponentItem* Group )
	{
		guard(WFilerPageComponents::OnSelectionChange);
		INT i, Added;

		// Unforce all.
		for( i=0; i<Components.Root.Children.Num(); i++ )
		{
			FComponentItem* Item = (FComponentItem*)Components.Root.Children(i);
			Item->Forced = 0;
		}

		// Build list of dependent components that must be uninstalled due to selected products.
		Dependencies.Empty();
		for( i=0; i<Components.Root.Children.Num(); i++ )
		{
			FComponentItem* Item = (FComponentItem*)Components.Root.Children(i);
			if( Item->SetupGroup->Selected )
				Dependencies.AddItem( Item->SetupGroup );
		}

		// All items that are dependent but not selected must be forced.
		do
		{
			Added = 0;
			for( i=0; i<Components.Root.Children.Num(); i++ )
			{
				FComponentItem* Item = (FComponentItem*)Components.Root.Children(i);
				if( !Item->Forced )
				{
					for( INT j=0; j<Item->SetupGroup->Requires.Num(); j++ )
					{
						for( INT k=0; k<Components.Root.Children.Num(); k++ )
						{
							FComponentItem* Other = (FComponentItem*)Components.Root.Children(k);
							if( Item->SetupGroup->Requires(j)==Other->SetupGroup->GetName() && (Other->SetupGroup->Selected || Other->Forced) )
							{
								Dependencies.AddUniqueItem( Item->SetupGroup );
								Item->Forced = 1;
								Added = 1;
							}
						}
					}
				}
			}
		} while( Added );

		// Refresh.
		Owner->RefreshPage();

		unguard;
	}

	// WWizardPage interface.
	const TCHAR* GetNextText()
	{
		guard(WFilerPageComponents::GetNextText);
		return Dependencies.Num() ? WWizardPage::GetNextText() : NULL;
		unguard;
	}
	WWizardPage* GetNext()
	{
		guard(WFilerPageComponents::GetNext);
		return Dependencies.Num() ? new WFilerPageUninstall(Owner) : NULL;
		unguard;
	}
	void OnInitDialog()
	{
		guard(WFilerPageComponents::OnInitDialog);
		WWizardPage::OnInitDialog();
		Components.OpenChildWindow( IDC_ComponentsHolder );

		OnGroupChange( NULL );
		Components.GetRoot()->Expand();
		Components.ResizeList();
		Components.List.SetCurrent( 0, 1 );
		Components.SetItemFocus( 1 );

		unguard;
	}
	UBOOL GetShow()
	{
		guard(WFilerPageComponents::GetShow);
		if( Components.Root.Children.Num()==1 )
		{
			FComponentItem* Item = (FComponentItem*)Components.Root.Children(0);
			if( Item->Children.Num()==0 )
			{
				Dependencies.AddItem( Item->SetupGroup );
				return 0;
			}
		}
		return 1;
		unguard;
	}
};

// WFilerPageAutoplay.
class WFilerPageAutoPlay : public WWizardPage
{
	DECLARE_WINDOWCLASS(WFilerPageAutoPlay,WWizardPage,Setup)

	// Variables.
	WFilerWizard* Owner;
	USetupDefinition* Manager;
	WLabel Options;
	WLabel CompleteLabel;
	WButton CompleteFrame;
	WCoolButton PlayButton;
	WCoolButton ReleaseNotesButton;
	WCoolButton ReinstallButton;
	WCoolButton UninstallButton;
	WCoolButton WebButton;
	UBOOL ShowInstallOptions;

	// Constructor.
	WFilerPageAutoPlay( WFilerWizard* InOwner, UBOOL InShowInstallOptions )
	: WWizardPage( TEXT("FilerPageAutoPlay"), IDDIALOG_FilerPageAutoPlay, InOwner )
	, Owner             ( InOwner )
	, Manager           ( InOwner->Manager )
	, Options           ( this, IDC_Options )
	, PlayButton        ( this, IDC_Play,         FDelegate(this,(TDelegate)OnPlay),         CBFF_ShowOver|CBFF_UrlStyle )
	, ReleaseNotesButton( this, IDC_ReleaseNotes, FDelegate(this,(TDelegate)OnReleaseNotes), CBFF_ShowOver|CBFF_UrlStyle )
	, WebButton         ( this, IDC_Web,          FDelegate(this,(TDelegate)OnWeb),          CBFF_ShowOver|CBFF_UrlStyle )
	, ReinstallButton   ( this, IDC_Reinstall,    FDelegate(this,(TDelegate)OnInstall),      CBFF_ShowOver|CBFF_UrlStyle )
	, UninstallButton   ( this, IDC_Uninstall,    FDelegate(this,(TDelegate)OnUninstall),    CBFF_ShowOver|CBFF_UrlStyle )
	, CompleteLabel     ( this, IDC_Complete )
	, CompleteFrame     ( this, IDC_Divider )
	, ShowInstallOptions( InShowInstallOptions )
	{}

	// Buttons.
	void OnPlay()
	{
		guard(WFilerPageAutoPlay::OnPlay);
		FString Exe = Manager->RegistryFolder * Manager->Exe;
		FString Folder = Exe;
		while( Folder.Len() && Folder.Right(1)!=PATH_SEPARATOR )
			Folder = Folder.LeftChop( 1 );
		if( Folder.Right(1)==PATH_SEPARATOR )
			Folder = Folder.LeftChop( 1 );
		ShellExecuteX( *this, TEXT("open"), *Exe, TEXT(""), *Folder, SW_SHOWNORMAL );
		Owner->OnFinish();
		unguard;
	}
	void OnInstall()
	{
		guard(WFilerPageAutoPlay::OnInstall);
		Owner->Advance( new WFilerPageWelcome(Owner) );
		unguard;
	}
	void OnUninstall()
	{
		guard(WFilerPageAutoPlay::OnUninstall);
		FString Path = Manager->RegistryFolder*TEXT("System");
		ShellExecuteX( NULL, TEXT("open"), *(Path*TEXT("Setup.exe")), *(US+TEXT("uninstall \"") + Manager->Product + TEXT("\"")), *Path, SW_SHOWNORMAL );
		Owner->OnFinish();
		unguard;
	}
	void OnReleaseNotes()
	{
		guard(WFilerPageAutoPlay::OnReleaseNotes);
		ShellExecuteX( *this, TEXT("open"), *(Manager->RegistryFolder * Manager->ReadMe), TEXT(""), NULL, SW_SHOWNORMAL );
		unguard;
	}
	void OnWeb()
	{
		guard(WFilerPageAutoPlay::OnWeb);
		ShellExecuteX( *this, TEXT("open"), *Manager->ProductURL, TEXT(""), appBaseDir(), SW_SHOWNORMAL );
		unguard;
	}

	// WWizardPage interface.
	void OnCurrent()
	{
		guard(WFilerPageAutoplay::OnCurrent);
		Owner->SetText( *Manager->AutoplayWindowTitle );
		unguard;
	}
	void OnInitDialog()
	{
		guard(WFilerPageAutoPlay::OnInitDialog);
		WWizardPage::OnInitDialog();
		Options.SetFont( hFontHeadline );
		if( ShowInstallOptions )
		{
			CompleteLabel.Show(0);
			CompleteFrame.Show(0);
		}
		else
		{
			ReinstallButton.Show(0);
			UninstallButton.Show(0);
		}
		if( !Manager->Exists || Manager->Exe==TEXT("") || Manager->MustReboot )
			PlayButton.Show(0);
		if( Manager->MustReboot )
			CompleteLabel.SetText( LineFormat(Localize("IDDIALOG_FilerPageAutoPlay","IDC_CompleteReboot")) );
		Options.SetText( *Manager->AutoplayWindowTitle );
		unguard;
	}
	WWizardPage* GetNext()
	{
		guard(WFilerPageAutoPlay::GetNext);
		Manager->MustReboot = 0;
		Owner->OnFinish();
		return NULL;
		unguard;
	}
	const TCHAR* GetBackText()
	{
		return NULL;
	}
	const TCHAR* GetFinishText()
	{
		return ShowInstallOptions ? NULL : Manager->MustReboot ? LocalizeGeneral("RebootButton") : LocalizeGeneral("FinishButton",TEXT("Window"));
	}
	const TCHAR* GetNextText()
	{
		return (Manager->MustReboot && !ShowInstallOptions) ? LocalizeGeneral("ExitButton") : NULL;
	}
	virtual const TCHAR* GetCancelText()
	{
		return ShowInstallOptions ? WWizardPage::GetCancelText() : NULL;
	}
};
WWizardPage* NewAutoPlayPage( WFilerWizard* InOwner, UBOOL ShowInstallOptions )
{
	return new WFilerPageAutoPlay( InOwner, ShowInstallOptions );
}

/*-----------------------------------------------------------------------------
	WinMain.
-----------------------------------------------------------------------------*/

//
// Main window entry point.
//
INT WINAPI WinMain( HINSTANCE hInInstance, HINSTANCE hPrevInstance, char* InCmdLine, INT nCmdShow )
{
	// Remember instance info.
	GIsStarted = 1;
	hInstance = hInInstance;
	appStrcpy( GPackage, appPackage() );

	// Begin.
#ifndef _DEBUG
	try
	{
#endif
	{
		// Init.
		HANDLE hMutex = NULL;
		GIsEditor = 0;
		GIsScriptable = GIsClient = GIsServer = GIsGuarded = 1;
		appInit( GPackage, GetCommandLine(), &Malloc, GNull, &Error, &Warn, &FileManager, FConfigCacheIni::Factory, 0 );
		GConfig->Detach( *(FString(GPackage)+TEXT(".ini")) );

		// Init windowing.
		InitWindowing();
		IMPLEMENT_WINDOWCLASS(WFilerPageWelcome,0);
		IMPLEMENT_WINDOWCLASS(WFilerPageLicense,0);
		IMPLEMENT_WINDOWCLASS(WFilerPageComponentsBase,0);
		IMPLEMENT_WINDOWCLASS(WFilerPageComponents,0);
		IMPLEMENT_WINDOWCLASS(WFilerPageFolder,0);
		IMPLEMENT_WINDOWCLASS(WFilerPageCdFolder,0);
		IMPLEMENT_WINDOWCLASS(WFilerPageProgress,0);
		IMPLEMENT_WINDOWCLASS(WFilerPageInstallProgress,0);
		IMPLEMENT_WINDOWCLASS(WFilerPageUninstallProgress,0);
		IMPLEMENT_WINDOWCLASS(WFilerPageUninstall,0);
		IMPLEMENT_WINDOWCLASS(WFilerWizard,0);
		IMPLEMENT_WINDOWCLASS(WFilerPageAutoPlay,0);
		IMPLEMENT_WINDOWCLASS(WFailedRequirement,0);
		IMPLEMENT_WINDOWCLASS(WProductInfo,0);
		IMPLEMENT_WINDOWCLASS(WComponentProperties,0);
		IMPLEMENT_WINDOWCLASS(WFilerPageUninstallComponents,0);

		//oldver: Detect Unreal version 200 installation and write new format manifest.
		guard(DetectOldUnrealVersion);
		FString Str, Path;
		if
		(	!regGet( HKEY_LOCAL_MACHINE, TEXT("Software\\Unreal Technology\\Installed Apps\\Unreal"), TEXT("Folder"), Str )
		&&	 regGet( HKEY_CLASSES_ROOT, TEXT("Unreal.Map\\Shell\\Open\\Command"), TEXT(""), Path ) )
		{
			FString OriginalPath=Path;

			FString Check1=TEXT("\\System\\Unreal.exe \"%1\"");
			if( Path.Right(Check1.Len())==Check1 )
				Path = Path.LeftChop(Check1.Len());

			FString Check2=TEXT("\\Unreal.exe \"%1\"");
			if( Path.Right(Check2.Len())==Check2 )
				Path = Path.LeftChop(Check2.Len());

			if( Path.Right(1)==PATH_SEPARATOR )
				Path = Path.LeftChop(1);

			if( Path!=OriginalPath )
			{
				regSet( HKEY_LOCAL_MACHINE, TEXT("Software\\Unreal Technology\\Installed Apps\\Unreal"), TEXT("Folder"), *Path );
				GConfig->SetString( TEXT("Setup"),  TEXT("Group"),  TEXT("Unreal"), *(Path * TEXT("System") * SETUP_INI) );
				GConfig->SetString( TEXT("Unreal"), TEXT("Version"), TEXT("200"),   *(Path * TEXT("System") * SETUP_INI) );
				GConfig->Flush( 0 );
			}
		}
		unguard;

		// See if Unreal or Filer is running.
		INT Count=0;
	RetryMutex:
		hMutex = CreateMutexX( NULL, 0, TEXT("UnrealIsRunning") );
		if( GetLastError()==ERROR_ALREADY_EXISTS )
		{
			CloseHandle( hMutex );
			Sleep(100);
			if( ++Count<20 )
				if( appStrfind(appCmdLine(),TEXT("reallyuninstall")) || appStrfind(appCmdLine(),TEXT("uninstall")) )
					goto RetryMutex;
			if( MessageBox(NULL,LocalizeError(TEXT("AlreadyRunning")),LocalizeError(TEXT("AlreadyRunningTitle")),MB_OKCANCEL)==IDOK )
				goto RetryMutex;
			goto Finished;
		}

		// Filer interface.
		guard(Setup);
		WFilerWizard D;
		if( !D.Manager->NoRun )
		{
			WWizardPage* Page;
			if( D.Manager->Uninstalling )
				Page = new WFilerPageUninstallComponents(&D);
			else if( D.Manager->Exists && D.Manager->CdAutoPlay )
				Page = new WFilerPageAutoPlay(&D,1);
			else
				Page = new WFilerPageWelcome(&D);
			D.Advance( Page );
			D.DoModal();
		}
		unguard;

		// Exit.
	Finished:
		appPreExit();
		GIsGuarded = 0;
	}
#ifndef _DEBUG
	}
	catch( ... )
	{
		// Crashed.
		try
		{
			Error.HandleError();
		}
		catch( ... )
		{}
	}
#endif

	// Shut down.
	appExit();
	GIsStarted = 0;

	return 0;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
