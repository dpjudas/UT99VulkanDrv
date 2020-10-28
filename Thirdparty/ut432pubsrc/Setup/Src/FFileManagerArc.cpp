/*=============================================================================
	FFileManagerArc.cpp: Unreal archive-based file manager.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

#include "SetupPrivate.h"

/*-----------------------------------------------------------------------------
	File manager interceptor.
-----------------------------------------------------------------------------*/

// Wildcard matching test.
UBOOL WildcardMatch( const TCHAR* Pattern, const TCHAR* String )
{
	guard(WildcardMatch);
	for( ; *Pattern!='*'; String++,Pattern++ )
		if( appToUpper(*String)!=appToUpper(*Pattern) )
			return 0;
		else if( *Pattern==0 )
			return 1;
	do
		if( WildcardMatch(Pattern+1,String) )
			return 1;
	while( *String++ );
	return 0;
	unguard;
}

// Hack because there isn't a well defined "current working directory" idea with archives.
FString ToArcFilename( const TCHAR* Filename )
{
	guard(ToArcFilename);
	return 
		(Filename[0]=='.' && Filename[1]=='.' && Filename[2]=='\\')
	?	(Filename+3)
	:	(FString(TEXT("System")) * Filename);//!!
	unguard;
}
FString FromArcFilename( const TCHAR* Filename )
{
	guard(FromArcFilename);
	FString Found = Filename;
	return
		( Found.Left(7)==TEXT("System") PATH_SEPARATOR )
	?	Found.Mid(7)
	:	Found = FString(TEXT("..") PATH_SEPARATOR) + Found;
	unguard;
}

// File reader.
class FFileReaderArc : public FArchive
{
public:
	FFileReaderArc( FArchive* InAr, INT InBase, INT InSize )
	: Ar( InAr ), Base( InBase ), Size( InSize ), Pos( 0 )
	{
		Seek( 0 );
	}
	~FFileReaderArc()
	{
		delete Ar;
	}
	void Precache( INT HintCount )
	{
		Ar->Precache( HintCount );
	}
	void Seek( INT InPos )
	{
		Pos = InPos;
		Ar->Seek( InPos + Base );
	}
	INT Tell()
	{
		return Pos;
	}
	INT TotalSize()
	{
		return Size;
	}
	UBOOL Close()
	{
		return Ar->Close();
	}
	void Serialize( void* V, INT Length )
	{
		check(Pos+Length<=Size);
		Ar->Serialize( V, Length );
		Pos += Length;
	}
	FArchive* Ar;
	INT Base, Size, Pos;
};

// File manager.
class FFileManagerArc : public FFileManager
{
public:
	FFileManagerArc( FFileManager* InFM, const TCHAR* InMod, FArchiveHeader* InArc )
	: FM( InFM ), Mod( InMod ), Arc( InArc )
	{}
	FFileManager*   FM;
	FString         Mod;
	FArchiveHeader* Arc;

	// FFileManager interface.
	FArchive* CreateFileReader( const TCHAR* Filename, DWORD Flags=0, FOutputDevice* Error=GNull )
	{
		guard(FFileManagerArc::CreateFileReader);
		FString Find = ToArcFilename( Filename );
		for( INT i=0; i<Arc->_Items_.Num(); i++ )
		{
			FArchiveItem& Item = Arc->_Items_(i);
			if( Item._Filename_==Find )
			{
				FArchive* Ar = GFileManager->CreateFileReader( *Mod, FILEREAD_NoFail );
				return new FFileReaderArc( Ar, Item.Offset, Item.Size );
			}
		}
		return FM->CreateFileReader( Filename, Flags, Error );
		unguard;
	}
	FArchive* CreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error=GNull )
	{
		return FM->CreateFileWriter( Filename, Flags, Error );
	}
	INT FileSize( const TCHAR* Filename )
	{
		guard(FFileManagerArc::FileSize);
		FArchive* Ar = CreateFileReader( Filename );
		if( !Ar )
			return -1;
		INT Result = Ar->TotalSize();
		delete Ar;
		return Result;
		unguard;
	}
	UBOOL Delete( const TCHAR* Filename, UBOOL RequireExists=0, UBOOL EvenReadOnly=0 )
	{
		return FM->Delete( Filename, RequireExists, EvenReadOnly );
	}
	UBOOL Copy( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace=1, UBOOL EvenIfReadOnly=0, UBOOL Attributes=0, void (*Progress)(FLOAT Fraction)=NULL )
	{
		return FM->Copy( Dest, Src, Replace, EvenIfReadOnly, Attributes, Progress );
	}
	UBOOL Move( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace=1, UBOOL EvenIfReadOnly=0, UBOOL Attributes=0 )
	{
		return FM->Move( Dest, Src, Replace, EvenIfReadOnly, Attributes );
	}
	SQWORD GetGlobalTime( const TCHAR* Filename )
	{
		return FM->GetGlobalTime( Filename );
	}
	UBOOL SetGlobalTime( const TCHAR* Filename )
	{
		return FM->SetGlobalTime( Filename );
	}
	UBOOL MakeDirectory( const TCHAR* Path, UBOOL Tree=0 )
	{
		return FM->MakeDirectory( Path, Tree );
	}
	UBOOL DeleteDirectory( const TCHAR* Path, UBOOL RequireExists=0, UBOOL Tree=0 )
	{
		return FM->DeleteDirectory( Path, RequireExists, Tree );
	}
	TArray<FString> FindFiles( const TCHAR* Filename, UBOOL Files, UBOOL Directories )
	{
		TArray<FString> Result = FM->FindFiles( Filename, Files, Directories );
		FString Find = ToArcFilename( Filename );
		for( INT i=0; i<Arc->_Items_.Num(); i++ )
		{
			FArchiveItem& Item = Arc->_Items_(i);
			if( WildcardMatch( *Find, *Item._Filename_ ) )
			{
				FString Found = FromArcFilename(*Item._Filename_);
				if( Result.FindItemIndex(*Found)==INDEX_NONE )
					new(Result)FString(*Found);
			}
		}
		return Result;
	}
	UBOOL SetDefaultDirectory( const TCHAR* Filename )
	{
		return FM->SetDefaultDirectory( Filename );
	}
	FString GetDefaultDirectory()
	{
		return FM->GetDefaultDirectory();
	}
};
FFileManager* CreateFileManagerArc( FFileManager* InFM, const TCHAR* InMod, FArchiveHeader* InArc )
{
	guard(CreateFileManagerArc);
	return new FFileManagerArc( InFM, InMod, InArc );
	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
