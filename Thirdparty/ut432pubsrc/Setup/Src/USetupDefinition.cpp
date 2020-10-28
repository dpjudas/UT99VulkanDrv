/*=============================================================================
	USetupDefinition.cpp: Unreal installer/filer.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

#include "SetupPrivate.h"
#include "FFileManagerArc.h"
#include "FCodec.h"

/*-----------------------------------------------------------------------------
	Helper functions.
-----------------------------------------------------------------------------*/

// Text formatting.
inline const TCHAR* LineFormat( const TCHAR* In )
{
	guard(LineFormat);
	static TCHAR Result[4069];
	TCHAR* Ptr = Result;
	while( *In )
		*Ptr++ = *In++!='\\' ? In[-1] : *In++=='n' ? '\n' : In[-1];
	*Ptr++ = 0;
	return Result;
	unguard;
}

/*-----------------------------------------------------------------------------
	USetupObject.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(USetupObject);

/*-----------------------------------------------------------------------------
	USetupProduct.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(USetupProduct);

/*-----------------------------------------------------------------------------
	USetupGroup.
-----------------------------------------------------------------------------*/

USetupGroup::USetupGroup()
: File(E_NoInit), Copy(E_NoInit), Group(E_NoInit), Folder(E_NoInit), Backup(E_NoInit), Delete(E_NoInit), Ini(E_NoInit), Requires(E_NoInit)
{
	guard(USetupGroup::USetupGroup);
	check(Manager);
	if( Manager->Uninstalling )
	{
		Optional = 1;
		Visible  = 1;
		Selected = 0;
	}
	for( TArray<FString>::TIterator It(Group); It; ++It )
	{
		Subgroups.AddItem( ConstructObject<USetupGroup>(GetClass(),GetOuter(),**It) );
	}
	unguard;
}
SQWORD USetupGroup::SpaceRequired()
{
	guard(USetupGroup::SpaceRequired);
	SQWORD Count=0;
	if( Selected )
	{
		for( TArray<USetupGroup*>::TIterator ItG(Subgroups); ItG; ++ItG )
		{
			Count += (*ItG)->SpaceRequired();
		}
		for( TArray<FString>::TIterator ItF(File); ItF; ++ItF )
		{
			FFileInfo Info(*ItF);
			if( Info.Lang==TEXT("") || Info.Lang==UObject::GetLanguage() )
			{
				FString Filename = Manager->DestPath * (Info.Dest==TEXT("") ? Info.Src : Info.Dest);
				INT ExistingSize = GFileManager->FileSize(*Filename);
				Count += (ExistingSize<0) ? (Info.Size + PER_FILE_OVERHEAD) : Max<SQWORD>(0,Info.Size-ExistingSize);
			}
		}
	}
	return Count;
	unguard;
}
USetupDefinition* USetupGroup::Manager;
IMPLEMENT_CLASS(USetupGroup);

/*-----------------------------------------------------------------------------
	USetupDefinition.
-----------------------------------------------------------------------------*/

USetupDefinition::USetupDefinition()
:	ConfigFile		( MANIFEST_FILE )
, 	Requires        ( E_NoInit )
{}
void USetupDefinition::Init()
{
	guard(USetupDefinition::Init);

	// Figure out source folder.
	SrcPath = appBaseDir();
	if( SrcPath.Right(1)==PATH_SEPARATOR )
		SrcPath = SrcPath.LeftChop( 1 );
	while( SrcPath.Len() && SrcPath.Right(1)!=PATH_SEPARATOR )
		SrcPath = SrcPath.LeftChop( 1 );
	if( SrcPath.Len() )
		SrcPath = SrcPath.LeftChop( 1 );

	// See if installing a packed .umod file.
	FString Token;
	const TCHAR* Cmd = appCmdLine();
	ParseToken( Cmd, Token, 0 );
	if( Token==TEXT("install") || (Token==TEXT("") && MasterProduct==TEXT("")) )
	{
		// Installing.
		if( Token==TEXT("install") )
		{
			// Verify that the specified module exists.
			ParseToken( Cmd, Token, 0 );
			GFileManager = new FFileManagerArc(GFileManager,*Token,1);
			GFileManager->Init(0);
			Token = TEXT("");
			ParseToken( Cmd, Token, 0 );

			// Reload config from the new filer manager.
			GConfig->Flush(1,MANIFEST_FILE MANIFEST_EXT);
			LoadConfig();
			LoadLocalized();
		}

		// Detach configuration file so we can modify it locally.
		GConfig->Detach( *ConfigFile );
		Manifest = 1;
	}
	else if( Token==TEXT("reallyuninstall") )
	{
		// Running uninstaller from temp folder.
		ParseToken( Cmd, Product, 0 );
		Parse( appCmdLine(), TEXT("PATH="), DestPath );
		if( DestPath.Right(1)==PATH_SEPARATOR )
			DestPath = DestPath.LeftChop( 1 );
		while( DestPath.Len() && DestPath.Right(1)!=PATH_SEPARATOR )
			DestPath = DestPath.LeftChop( 1 );
		Uninstalling = 1;
	}
	else if( Token==TEXT("uninstall") || (Token==TEXT("") && MasterProduct!=TEXT("")) )
	{
		// Running uninstaller for the first time.
		NoRun = 1;
		if( Token==TEXT("uninstall") )
			ParseToken( Cmd, Product, 0 );
		else
			Product = MasterProduct;
		PerformUninstallCopy();
	}
	else appErrorf(TEXT("Setup: Unknown disposition"));

	// Validate everything.
	if( !appAtoi(*Version) && !Uninstalling && !NoRun )
		appErrorf( TEXT("Setup: Missing version number") );

	// Determine whether known to be installed.
	Exists
	=	GetRegisteredProductFolder( Product, RegistryFolder )
	&&	GFileManager->FileSize(*(RegistryFolder*TEXT("System")*SETUP_INI))>0;

	// If this is a mod, find required product's path.
	FolderExists = Exists;
	if( !Exists && !Uninstalling && !NoRun )
	{
		FString RequiredProduct, FindFolder;
		for( TArray<FString>::TIterator It(Requires); It; ++It )
			if(	GConfig->GetString(**It, TEXT("Product"), RequiredProduct, *ConfigFile) )
			{
				if( RequiredProduct==TEXT("Unreal Tournament") )
					RequiredProduct = TEXT("UnrealTournament");
				if( GetRegisteredProductFolder(RequiredProduct, FindFolder)
				&&	GFileManager->FileSize(*(FindFolder*TEXT("System")*SETUP_INI)) )
				{
					RegistryFolder = FindFolder;
					FolderExists   = 1;
					break;
				}
			}
	}

	// Figure language.
	FString Str;
	if( GConfig->GetString(TEXT("Engine.Engine"),TEXT("Language"),Str,*(RegistryFolder*TEXT("System\\Default.ini"))) )
		UObject::SetLanguage( *Str );
	LanguageChange();

	unguard;
}
void USetupDefinition::CreateRootGroup()
{
	guard(USetupDefinition::CreateRootGroup);
	USetupGroup::Manager = this;
	RootGroup = new(GetOuter(),TEXT("Setup"))USetupGroup;
	unguard;
}
void USetupDefinition::PerformUninstallCopy()
{
	guard(USetupDefinition::PerformUninstallCopy);
	unguard;
}
UBOOL USetupDefinition::GetRegisteredProductFolder( FString Product, FString& Folder )
{
	guard(USetupDefinition::GetRegisteredProductFolder);
	return 0;
	unguard;
}
UBOOL USetupDefinition::LocateSourceFile( FString& Src )
{
	guard(USetupDefinition::LocateSourceFile);
	Src = FString(TEXT("..")) * Src;
	return GFileManager->FileSize(*Src)>=0;
	unguard;
}
void USetupDefinition::Reformat( FString& Msg, TMultiMap<FString,FString>* Map )
{
	if( Map )
		Msg = appFormat( Msg, *Map );
}
FString USetupDefinition::Format( FString Msg, const TCHAR* Other )
{
	guard(USetupDefinition::Format);
	if( Manifest )
	{
		if( Other )
		{
			Reformat( Msg, GConfig->GetSectionPrivate(Other,0,1,*(FString(MANIFEST_FILE)+TEXT(".")+UObject::GetLanguage())) );
			if( FString(UObject::GetLanguage())!=TEXT("INT") )
				Reformat( Msg, GConfig->GetSectionPrivate(Other,0,1,*(FString(MANIFEST_FILE)+TEXT(".INT"))) );
		}
		Reformat( Msg, GConfig->GetSectionPrivate(TEXT("Setup"  ),0,1,*(FString(MANIFEST_FILE)+TEXT(".")+UObject::GetLanguage())) );
		Reformat( Msg, GConfig->GetSectionPrivate(TEXT("General"),0,1,*(FString(MANIFEST_FILE)+TEXT(".")+UObject::GetLanguage())) );
		if( FString(UObject::GetLanguage())!=TEXT("INT") )
		{
			Reformat( Msg, GConfig->GetSectionPrivate(TEXT("Setup"  ),0,1,*(FString(MANIFEST_FILE)+TEXT(".INT"))) );
			Reformat( Msg, GConfig->GetSectionPrivate(TEXT("General"),0,1,*(FString(MANIFEST_FILE)+TEXT(".INT"))) );
		}
		Reformat( Msg, GConfig->GetSectionPrivate(TEXT("Setup"),0,1,*ConfigFile) );
	}
	return Msg;
	unguard;
}
INT USetupDefinition::UpdateRefCount( const TCHAR* Key, const TCHAR* Value, INT Inc )
{
	guard(USetupDefinition::UpdateRefCount);

	// Increment reference count.
	FString  RefKey   = FString(Key) + TEXT(":") + Value;
	FString* CountPtr = RefCounts.Find( *RefKey );
	if( CountPtr || Inc>0 )
	{
		INT Count = (CountPtr ? appAtoi(**CountPtr) : 0) + Inc;
		if( Count>0 )
		{
			RefCounts.Set( *RefKey, *FString::Printf(TEXT("%i"),Count) );
		}
		else
		{
			RefCounts.Remove( *RefKey );
			Count = 0;
		}
		return Count;
	}
	return 0;
	unguard;
}
void USetupDefinition::UninstallLogAdd( const TCHAR* Key, const TCHAR* Value, UBOOL Unique, UBOOL RefLog )
{
	guard(USetupDefinition::UninstallLogAdd);

	// Find in uninstall log.
	if( !RootGroup->UninstallLog.FindPair(Key,Value) )
	{
		// Add to uninstall log.
		if( Unique )
			RootGroup->UninstallLog.Set( Key, Value );
		else
			RootGroup->UninstallLog.Add( Key, Value );
		if( RefLog )
			UpdateRefCount( Key, Value, 1 );
	}
	unguard;
}
void USetupDefinition::LanguageChange()
{
	guard(USetupDefinition::LanguageChange);
	Super::LanguageChange();

	if( !FolderExists )
		RegistryFolder = DefaultFolder;
	if( RegistryFolder.Right(1)==PATH_SEPARATOR )
		RegistryFolder = RegistryFolder.LeftChop(1);
	if( AutoplayWindowTitle==TEXT("") )
		AutoplayWindowTitle = SetupWindowTitle;

	unguard;
}
FString USetupDefinition::GetFullRef( const FString RefFile )
{
	if( RefFile.Left(7)==FString(TEXT("System") PATH_SEPARATOR) )//oldver for Unreal 3dfx version, general multi-reference-path solution?
	{
		FString Test = RefPath * FString(TEXT("System200") PATH_SEPARATOR) + RefFile.Mid(7);
		if( GFileManager->FileSize(*Test)>0 )
			return Test;
	}
	return RefPath * RefFile;
}
void USetupDefinition::DidCancel()
{
	guard(DidCancel);
	appErrorf( NAME_FriendlyError, LocalizeGeneral(TEXT("DidCancel"))), LocalizeGeneral(TEXT("InstallCancel") );
	unguard;
}
void USetupDefinition::InstallTree( const TCHAR* Action, FInstallPoll* Poll, void (USetupDefinition::*Process)( FString Key, FString Value, UBOOL Selected, FInstallPoll* Poll ), USetupGroup* SetupGroup, UBOOL Selected )
{
	guard(USetupDefinition::InstallTree);
	if( !SetupGroup )
		SetupGroup = RootGroup;

	// Update selection status.
	Selected = Selected && SetupGroup->Selected;

	// Process this item.
	(this->*Process)( TEXT("GroupSpecial"), SetupGroup->GetName(), Selected, Poll );
	TMultiMap<FString,FString>* Map = GConfig->GetSectionPrivate( SetupGroup->GetName(), 0, 1, *ConfigFile );
	check(Map);
	for( TMultiMap<FString,FString>::TIterator It(*Map); It; ++It )
	{
		FString V = Format(It.Value());
		(this->*Process)( It.Key(), V, Selected, Poll );
	}

	// Handle all subgroups here.
	for( TArray<USetupGroup*>::TIterator ItG(SetupGroup->Subgroups); ItG; ++ItG )
		InstallTree( Action, Poll, Process, *ItG, Selected );

	unguardf(( TEXT("(%s %s)"), Action, SetupGroup->GetFullName() ));
}
void USetupDefinition::UninstallTree( const TCHAR* Action, FInstallPoll* Poll, void (USetupDefinition::*Process)( FString Key, FString Value, FInstallPoll* Poll ) )
{
	guard(USetupDefinition::UninstallTree);
	for( TArray<USetupGroup*>::TIterator GroupIt(UninstallComponents); GroupIt; ++GroupIt )
	{
		for( TMultiMap<FString,FString>::TIterator ItemIt((*GroupIt)->UninstallLog); ItemIt; ++ItemIt )
			(this->*Process)( ItemIt.Key(), ItemIt.Value(), Poll );
	}
	unguard;
}
void USetupDefinition::SetupFormatStrings()
{
	guard(USetupDefinition::SetupFormatStrings);

	// Native wildcard strings.
	GConfig->SetString( TEXT("Setup"), TEXT("SrcPath"),  *SrcPath,  *ConfigFile );
	GConfig->SetString( TEXT("Setup"), TEXT("DestPath"), *DestPath, *ConfigFile );
	GConfig->SetString( TEXT("Setup"), TEXT("RefPath"),  *RefPath,  *ConfigFile );
	GConfig->SetString( TEXT("Setup"), TEXT("CdPath"), Patch ? *RefPath : *SrcPath, *ConfigFile );

	unguard;
}
void USetupDefinition::BeginSteps()
{
	guard(USetupDefinition::BeginSteps);

	// Setup the format strings.
	SetupFormatStrings();

	// Empty saved .ini log.
	SavedIni.Empty();

	// Make .ini file.
	SetupIniFile = DestPath * TEXT("System") * SETUP_INI;

	// Load refcounts, if any.
	TMultiMap<FString,FString>* Map = GConfig->GetSectionPrivate( TEXT("RefCounts"), 0, 1, *SetupIniFile );
	if( Map )
		RefCounts = *Map;

	// Load existing log, if any.
	if( Uninstalling )//!!can merge?
	{
		for( TArray<USetupGroup*>::TIterator GroupIt(UninstallComponents); GroupIt; ++GroupIt )
		{
			TMultiMap<FString,FString>* Map = GConfig->GetSectionPrivate( (*GroupIt)->GetName(), 0, 1, *SetupIniFile );
			if( Map )
				(*GroupIt)->UninstallLog = *Map;
		}
	}
	else
	{
		TMultiMap<FString,FString>* Map = GConfig->GetSectionPrivate( *Product, 0, 1, *SetupIniFile );
		if( Map )
			RootGroup->UninstallLog = *Map;
	}

	unguard;
}
void USetupDefinition::EndSteps()
{
	guard(USetupDefinition::EndSteps);

	// Update registry.
	if( Uninstalling )
		for( TArray<USetupGroup*>::TIterator GroupIt(UninstallComponents); GroupIt; ++GroupIt )
			*GConfig->GetSectionPrivate( (*GroupIt)->GetName(), 1, 0, *SetupIniFile ) = (*GroupIt)->UninstallLog;
	else//!!can merge?
		*GConfig->GetSectionPrivate( *Product, 1, 0, *SetupIniFile ) = RootGroup->UninstallLog;
	*GConfig->GetSectionPrivate( TEXT("RefCounts"), 1, 0, *SetupIniFile ) = RefCounts;
	GConfig->Flush( 0 );

	unguard;
}
void USetupDefinition::DoInstallSteps( FInstallPoll* Poll )
{
	guard(USetupDefinition::DoInstallSteps);

	// Handle all install steps.
	BeginSteps();
	TotalBytes = 0;
	InstallTree( TEXT("ProcessPreCopy"),  Poll, (INSTALL_STEP)ProcessPreCopy  );
	InstallTree( TEXT("ProcessCopy"),     Poll, (INSTALL_STEP)ProcessCopy     );
	InstallTree( TEXT("ProcessExtra"),    Poll, (INSTALL_STEP)ProcessExtra    );
	InstallTree( TEXT("ProcessPostCopy"), Poll, (INSTALL_STEP)ProcessPostCopy );
	Exists = FolderExists = 1;
	RegistryFolder = DestPath;
	if( IsMasterProduct )
		GConfig->SetString( TEXT("Setup"), TEXT("MasterProduct"), *Product, *(DestPath * TEXT("System") * SETUP_INI) );
	TMultiMap<FString,FString>* Map = GConfig->GetSectionPrivate( TEXT("Setup"), 1, 0, *(DestPath * TEXT("System") * SETUP_INI) );
		Map->AddUnique( TEXT("Group"), *Product );
	for( TArray<FSavedIni>::TIterator It(SavedIni); It; ++It )
		GConfig->SetString( *It->Section, *It->Key, *It->SavedValue, *It->File );
	UninstallLogAdd( TEXT("Caption"), *LocalProduct, 1, 0 );
	RootGroup->UninstallLog.Remove( TEXT("Version") );
	UninstallLogAdd( TEXT("Version"), *Version, 1, 0 );
	for( INT i=0; i<Requires.Num(); i++ )
	{
		USetupProduct* SetupProduct = new(GetOuter(),*Requires(i))USetupProduct;
		if( SetupProduct->Product!=Product )
			UninstallLogAdd( TEXT("Requires"), *SetupProduct->Product, 0, 0 );
	}
	EndSteps();

	unguard;
}
void USetupDefinition::DoUninstallSteps( FInstallPoll* Poll )
{
	guard(USetupDefinition::DoInstallSteps);

	// Handle all uninstall steps.
	BeginSteps();
	UninstallTotal=0, UninstallCount=0;
	UninstallTree( TEXT("ProcessUninstallCountTotal"), Poll, ProcessUninstallCountTotal );
	UninstallTree( TEXT("ProcessUninstallRemove"),     Poll, ProcessUninstallRemove );
	TMultiMap<FString,FString>* Map = GConfig->GetSectionPrivate( TEXT("Setup"), 0, 0, *(DestPath * TEXT("System") * SETUP_INI) );
	for( TArray<USetupGroup*>::TIterator GroupIt(UninstallComponents); GroupIt; ++GroupIt )
	{
		(*GroupIt)->UninstallLog = TMultiMap<FString,FString>();
		if( Map )
			Map->RemovePair( TEXT("Group"), (*GroupIt)->GetName() );
	}
	EndSteps();

	// If reference counts exausted, delete unnecessary setup file so full directory can be removed.
	INT Refs=0;
	for( TMap<FString,FString>::TIterator It(RefCounts); It; ++It )
		Refs += appAtoi( *It.Value() );
	if( Refs==0 )
	{
		GFileManager->Delete( *SetupIniFile );
		RemoveEmptyDirectory( *BasePath(SetupIniFile) );
	}

	unguard;
}
UBOOL USetupDefinition::CheckRequirement( FString Folder, USetupProduct* RequiredProduct, FString& FailMessage )
{
	guard(USetupDefinition::CheckRequirement);

	// Verify that requirements are met.
	FString ExistingVersion;
	FString R=RequiredProduct->Product;
	if( R==TEXT("Unreal Tournament") )
		R = TEXT("UnrealTournament");
	if( !GConfig->GetString( *R, TEXT("Version"), ExistingVersion, *(Folder + PATH_SEPARATOR TEXT("System") PATH_SEPARATOR SETUP_INI) ) )
	{
		// See if old version file is there.
		if
		(	RequiredProduct->OldVersionInstallCheck!=TEXT("")
		&&	GFileManager->FileSize(*(Folder * RequiredProduct->OldVersionInstallCheck))>0 )
		{
			// Old version found.
			ExistingVersion = RequiredProduct->OldVersionNumber;
		}
		else
		{
			// Can't install here.
			FailMessage = FString::Printf( LocalizeError(TEXT("MissingProduct")), Patch ? LocalizeError(TEXT("MissingProductThis")) : *LocalProduct, *RequiredProduct->Product, *RequiredProduct->Product );
			return 0;
		}
	}
	if( appAtoi(*ExistingVersion) < appAtoi(*RequiredProduct->Version) )
	{
		FailMessage = FString::Printf(LocalizeError(TEXT("OldVersion")), Patch ? LocalizeError(TEXT("MissingProductThis")) : *LocalProduct, *RequiredProduct->Product, appAtoi(*RequiredProduct->Version), appAtoi(*ExistingVersion), *RequiredProduct->Product, appAtoi(*RequiredProduct->Version) );
		return 0;
	}
	return 1;
	unguard;
}
UBOOL USetupDefinition::CheckAllRequirements( FString Folder, USetupProduct*& RequiredProduct, FString& FailMessage )
{
	guard(USetupDefinition::CheckAllRequirements);
	for( TArray<FString>::TIterator It(Requires); It; ++It )
	{
		RequiredProduct = new(GetOuter(),**It)USetupProduct;
		if( !CheckRequirement( Folder, RequiredProduct, FailMessage) )
			return 0;
	}
	return 1;
	unguard;
}

// Installation steps.
void USetupDefinition::ProcessCheckRef( FString Key, FString Value, UBOOL Selected, FInstallPoll* Poll )
{
	guard(USetupDefinition::ProcessCheckRef);
	if( Selected && Key==TEXT("File") )
	{
		// See whether there is a delta-compression reference.
		FFileInfo Info(*Value);
		AnyRef = AnyRef || Info.Ref!=TEXT("");
	}
	unguard;
}
void USetupDefinition::ProcessVerifyCd( FString Key, FString Value, UBOOL Selected, FInstallPoll* Poll )
{
	guard(USetupDefinition::ProcessVerifyCd);
	if( Selected && Key==TEXT("File") && CdOk==TEXT("") )
	{
		// Verify existence.
		FFileInfo Info(*Value);
		if( Info.Lang==TEXT("") || Info.Lang==UObject::GetLanguage() )
			if
			(	Info.Ref!=TEXT("")
			&&	GFileManager->FileSize(*GetFullRef(*Info.Ref))!=Info.RefSize )
				CdOk = Info.Ref;
	}
	unguard;
}
void USetupDefinition::ProcessPreCopy( FString Key, FString Value, UBOOL Selected, FInstallPoll* Poll )
{
	guard(USetupDefinition::ProcessPreCopy);
	if( Selected && Key==TEXT("File") )
	{
		// Verify that file exists and is copyable.
		FFileInfo Info(*Value);
		if( Info.Lang==TEXT("") || Info.Lang==UObject::GetLanguage() )
		{
			if( !LocateSourceFile(Info.Src) )
				LocalizedFileError( TEXT("MissingInstallerFile"), Patch ? TEXT("AdviseBadDownload") : TEXT("AdviseBadMedia"), *Info.Src );
			if( Info.Ref!=TEXT("") )
			{
				FString FullRef = GetFullRef(*Info.Ref);
				if( GFileManager->FileSize(*FullRef)<=0 )
					LocalizedFileError( TEXT("MissingReferenceFile"), TEXT("AdviseBadMedia"), *FullRef );
				TotalBytes += GFileManager->FileSize(*FullRef);
				TotalBytes += Info.Size;
			}
			else TotalBytes += Info.Size;
		}
	}
	else if( Selected && Key==TEXT("SaveIni") )
	{
		Value           = Format(Value,NULL);
		INT Pos         = Value.InStr(TEXT(","));
		check(Pos>=0);
		FString File    = DestPath * Value.Left(Pos);
		Value           = Value.Mid(Pos+1);
		Pos             = Value.InStr(TEXT("."),1);
		check(Pos>=0);
		FString Section = Value.Left(Pos);
		FString Key     = Value.Mid(Pos+1);
		TMultiMap<FString,FString>* Map = GConfig->GetSectionPrivate( *Section, 0, 0, *File );
		if( Map )
			Map->AddUnique( *Key, *Value );
	}
	unguard;
}
void USetupDefinition::ProcessCopy( FString Key, FString Value, UBOOL Selected, FInstallPoll* Poll )
{
	guard(USetupDefinition::ProcessCopy);
	BYTE Buffer[4096];
	if( Selected && Key==TEXT("File") )
	{
		// Get source and dest filenames.
		FFileInfo Info(*Value);
		if( Info.Lang==TEXT("") || Info.Lang==UObject::GetLanguage() )
		{
			if( Info.Dest==TEXT("") )
				Info.Dest = Info.Src;
			if( !LocateSourceFile(Info.Src) )
				LocalizedFileError( TEXT("MissingInstallerFile"), Patch ? TEXT("AdviseBadDownload") : TEXT("AdviseBadMedia"), *Info.Src );
			FString FullDest  = DestPath * Info.Dest;
			FString FullSrc   = Info.Ref==TEXT("") ? Info.Src : GetFullRef(*Info.Ref);
			FString FullPatch = FullDest + TEXT("_tmp");

			// Update uninstallation log.
			UninstallLogAdd( TEXT("File"), *Info.Dest, 0, 1 );

			// Make destination directory.
			if( !GFileManager->MakeDirectory( *BasePath(FullDest), 1 ) )
				LocalizedFileError( TEXT("FailedMakeDir"), TEXT("AdviseBadDest"), *FullDest );

			// Status display.
			if( !Poll->Poll(*FullDest,0,0,RunningBytes,TotalBytes) )
				DidCancel();

			// Copy SrcAr -> DestAr.
			INT CalcOldCRC = 0;
			guard(CopyFile);
			FString ThisDest = Info.Ref==TEXT("") ? FullDest : FullPatch;
			debugf( TEXT("Copying %s to %s"), *FullSrc, *ThisDest);
			FArchive* SrcAr = GFileManager->CreateFileReader( *FullSrc );
			if( !SrcAr )
				LocalizedFileError( TEXT("FailedOpenSource"), Patch ? TEXT("AdviseBadDownload") : TEXT("AdviseBadMedia"), *FullSrc );
			INT Size = SrcAr->TotalSize();
			FArchive* DestAr = GFileManager->CreateFileWriter( *ThisDest, FILEWRITE_EvenIfReadOnly );
			if( !DestAr )
				LocalizedFileError( TEXT("FailedOpenDest"), TEXT("AdviseBadDest"), *ThisDest );

			if( FullSrc.Right(3).Caps() == TEXT(".UZ") && ThisDest.Right(3).Caps() != TEXT(".UZ"))
			{
				INT Signature;
				FString OrigFilename;
				*SrcAr << Signature;
				if( Signature != 5678 )
					LocalizedFileError( TEXT("FailedOpenSource"), TEXT("AdviseBadMedia"), *FullSrc );
				else
				{
					*SrcAr << OrigFilename;
					FCodecFull Codec;
					Codec.AddCodec(new FCodecRLE);
					Codec.AddCodec(new FCodecBWT);
					Codec.AddCodec(new FCodecMTF);
					Codec.AddCodec(new FCodecRLE);
					Codec.AddCodec(new FCodecHuffman);
					Codec.Decode( *SrcAr, *DestAr );
					if( !Poll->Poll(*FullDest,Size,Size,RunningBytes+=Size,TotalBytes) )					{
						delete SrcAr;
						delete DestAr;
						DidCancel();
					}
				}
			}
			else
			{
				for( SQWORD Pos=0; Pos<Size; Pos+=sizeof(Buffer) )
				{
					INT Count = Min( Size-Pos, (SQWORD)sizeof(Buffer) );
					SrcAr->Serialize( Buffer, Count );
					if( SrcAr->IsError() )
					{
						delete SrcAr;
						delete DestAr;
						LocalizedFileError( TEXT("FailedReadingSource"), Patch ? TEXT("AdviseBadDownload") : TEXT("AdviseBadMedia"), *FullSrc );
					}
					if( Info.Ref!=TEXT("") )
					{
						CalcOldCRC = appMemCrc( Buffer, Count, CalcOldCRC );
					}
					DestAr->Serialize( Buffer, Count );
					if( DestAr->IsError() )
					{
						delete SrcAr;
						delete DestAr;
						LocalizedFileError( TEXT("FailedWritingDest"), TEXT("AdviseBadDest"), *ThisDest );
					}
					if( !Poll->Poll(*FullDest,Pos,Size,RunningBytes+=Count,TotalBytes) )
					{
						delete SrcAr;
						delete DestAr;
						DidCancel();
					}
				}
			}
			delete SrcAr;
			if( !DestAr->Close() )
				LocalizedFileError( TEXT("FailedClosingDest"), TEXT("AdviseBadDest"), *ThisDest );
			delete DestAr;
			unguard;

			// Patch SrcAr + DeltaFile -> DestAr.
			if( Info.Ref!=TEXT("") )
			{
				guard(PatchFile);
				BYTE Buffer[4096];

				// Open files.
				FString ThisSrc = FullPatch;
				FArchive* SrcAr = GFileManager->CreateFileReader( *ThisSrc );
				if( !SrcAr )
					LocalizedFileError( TEXT("FailedOpenSource"), Patch ? TEXT("AdviseBadDownload") : TEXT("AdviseBadMedia"), *ThisSrc );
				INT Size = SrcAr->TotalSize();
				FArchive* DestAr = GFileManager->CreateFileWriter(*FullDest,FILEWRITE_EvenIfReadOnly);
				if( !DestAr )
					LocalizedFileError( TEXT("FailedOpenDest"), TEXT("AdviseBadDest"), *FullDest );

				// Load delta file.
				TArray<BYTE> Delta;
				FString DeltaName = Info.Src;
				if( !appLoadFileToArray( Delta, *DeltaName ) )
					LocalizedFileError( TEXT("FailedLoadingUpdate"), TEXT("AdviseBadDownload"), *Info.Src );
				debugf( TEXT("Patching %s to %s with %s"), *ThisSrc, *FullDest, *DeltaName );

				// Decompress variables.
				INT PrevSpot=0, CountSize=0, CRC=0;
				INT Magic=0, OldSize=0, OldCRC=0, NewSize=0, NewCRC;
				FBufferReader Reader( Delta );
				Reader << Magic << OldSize << OldCRC << NewSize << NewCRC;

				// Validate.
				if( Magic!=0x92f92912 )
					appErrorf( LineFormat(LocalizeError("PatchCorrupt")), *DeltaName, LocalizeError("AdviseBadDownload") );
				if( OldSize!=Size || OldCRC!=CalcOldCRC )
					appErrorf( LocalizeError("CdFileMismatch"), *Info.Ref, *LocalProduct );

				// Delta decode it.
				INT OldCountSize=0;
				while( !Reader.AtEnd() )
				{
					INT Index;
					Reader << AR_INDEX(Index);
					if( Index<0 )
					{
						CRC = appMemCrc( &Delta(Reader.Tell()), -Index, CRC );
						DestAr->Serialize( &Delta(Reader.Tell()), -Index );
						if( DestAr->IsError() )
							LocalizedFileError( TEXT("FailedWritingDest"), TEXT("AdviseBadDest"), *FullDest );
						Reader.Seek( Reader.Tell() - Index );
						CountSize -= Index;
					}
					else
					{
						INT CopyPos;
						Reader << AR_INDEX(CopyPos);
						CopyPos += PrevSpot;
						check(CopyPos>=0);
						check(CopyPos+Index<=Size);
						SrcAr->Seek( CopyPos );
						for( INT Base=Index; Base>0; Base-=sizeof(Buffer) )
						{
							INT Move = Min(Base,(INT)sizeof(Buffer));
							SrcAr->Serialize( Buffer, Move );
							if( SrcAr->IsError() )
								LocalizedFileError( TEXT("FailedReadingSource"), Patch ? TEXT("AdviseBadDownload") : TEXT("AdviseBadDownload"), *ThisSrc );
							CRC = appMemCrc( Buffer, Move, CRC );
							DestAr->Serialize( Buffer, Move );
							if( DestAr->IsError() )
								LocalizedFileError( TEXT("FailedWritingDest"), TEXT("AdviseBadDest"), *FullDest );
						}
						CountSize += Index;
						PrevSpot = CopyPos + Index;
					}
					if( ((CountSize^OldCountSize)&~(sizeof(Buffer)-1)) || Reader.AtEnd() )
					{
						if( !Poll->Poll(*FullDest,CountSize,Info.Size,RunningBytes+=(CountSize-OldCountSize),TotalBytes) )
						{
							delete SrcAr;
							delete DestAr;
							DidCancel();
						}
						OldCountSize = CountSize;
					}
				}
				if( NewSize!=CountSize || NewCRC!=CRC )
					appErrorf( LineFormat(LocalizeError("PatchCorrupt")), *DeltaName, LocalizeError("AdviseBadDownload") );
				delete SrcAr;
				if( !DestAr->Close() )
					LocalizedFileError( TEXT("FailedClosingDest"), TEXT("AdviseBadDest"), *FullDest );
				delete DestAr;
				GFileManager->Delete( *ThisSrc );
				unguard;
			}
		}
	}
	unguard;
}
void USetupDefinition::ProcessExtra( FString Key, FString Value, UBOOL Selected, FInstallPoll* Poll )
{
	guard(USetupDefinition::ProcessExtra);
	if( Selected && Key==TEXT("Folder") )
	{
		// Create folders.
		FString Path = DestPath * Value;
		if( !GFileManager->MakeDirectory( *Path, 1 ) )
			LocalizedFileError( TEXT("FailedMakeDir"), TEXT("AdviseBadDest"), *Path );

		// Update uninstallation log.
		UninstallLogAdd( TEXT("Folder"), *Value, 0, 1 );
	}
	else if( Selected && (Key==TEXT("Backup") || Key==TEXT("Delete")) )
	{
		UninstallLogAdd( *Key, *Value, 0, 0 );
	}
	else if( Key==TEXT("Ini") )
	{
		if( Value.Left(1)==TEXT("!") )
		{
			Selected = !Selected;
			Value = Value.Mid( 1 );
		}
		if( Selected )
		{
			Value            = Format(Value,NULL);
			INT Pos          = Value.InStr(TEXT(","));
			check(Pos>=0);
			FString IniFile  = DestPath*Value.Left(Pos);
			Value            = Value.Mid(Pos+1);
			Pos              = Value.InStr(TEXT("="));
			check(Pos>=0);
			FString IniValue = Value.Mid(Pos+1);
			Value            = Value.Left(Pos);
			Pos              = Value.InStr(TEXT("."),1);
			check(Pos>=0);
			FString IniSec   = Value.Left(Pos);
			FString IniKey   = Value.Mid(Pos+1);
			GConfig->SetString( *IniSec, *IniKey, *IniValue, *IniFile );
		}
	}
	unguard;
}
void USetupDefinition::ProcessPostCopy( FString Key, FString Value, UBOOL Selected, FInstallPoll* Poll )
{
	guard(USetupDefinition::ProcessPostCopy);
	if( Key==TEXT("AddIni") && Selected )
	{
		Value            = Format(Value,NULL);
		INT Pos          = Value.InStr(TEXT(","));
		check(Pos>=0);

		FString IniFile  = Value.Left(Pos);
		//!! OLDVER, needed for UT BonusPack.
		if( IniFile.Caps() != TEXT("UNREALTOURNAMENT.INI") )
			IniFile = DestPath*IniFile;
		Value            = Value.Mid(Pos+1);
		Pos              = Value.InStr(TEXT("="));
		check(Pos>=0);
		FString IniValue = Value.Mid(Pos+1);
		Value            = Value.Left(Pos);
		Pos              = Value.InStr(TEXT("."),1);
		check(Pos>=0);
		FString IniSec   = Value.Left(Pos);
		FString IniKey   = Value.Mid(Pos+1);

		TMultiMap<FString,FString>* Sec=GConfig->GetSectionPrivate(*IniSec,1,0,*IniFile);

		Sec->RemovePair( *IniKey, *IniValue );
		Sec->Add(*IniKey,*IniValue);
	}
	unguard;
}
void USetupDefinition::PreExit()
{
	guard(USetupDefinition::PreExit);
	unguard;
}

// Uninstall steps.
void USetupDefinition::ProcessUninstallCountTotal( FString Key, FString Value, FInstallPoll* Poll )
{
	guard(USetupDefinition::ProcessUninstallCountTotal);
	UninstallTotal++;
	unguard;
}
void USetupDefinition::ProcessUninstallRemove( FString Key, FString Value, FInstallPoll* Poll )
{
	guard(USetupDefinition::ProcessUninstallRemove);
	Poll->Poll(*Value,0,1,UninstallCount++,UninstallTotal);
	if( (Key==TEXT("File") || Key==TEXT("Delete")) && UpdateRefCount(*Key,*Value,-1)==0 )
	{
		// Delete file; remove folder if empty.
		GFileManager->Delete( *(DestPath * Value) );
		RemoveEmptyDirectory( *BasePath(DestPath * Value) );
	}
	else if( Key==TEXT("Folder") && UpdateRefCount(*Key,*Value,-1)==0 )
	{
		// if folder is empty, remove it.
		RemoveEmptyDirectory( *(DestPath * Value) );
	}
	unguard;
}

IMPLEMENT_CLASS(USetupDefinition);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
