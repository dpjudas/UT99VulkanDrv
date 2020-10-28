/*=============================================================================
	FFileManagerPSX2.h: Unreal PSX2 based file manager.
	Copyright 1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Brandon Reinhart
=============================================================================*/

#include <sifdev.h>
#include <libcdvd.h>
#include "FFileManagerGeneric.h"

/*-----------------------------------------------------------------------------
	File Manager.
-----------------------------------------------------------------------------*/
sceCdlFILE File;

// File manager.
class FArchiveFileReader : public FArchive
{
public:
	FArchiveFileReader( const TCHAR* InFilename, sceCdlFILE* InFile, FOutputDevice* InError )
	:	File			( InFile )
	,	Error			( InError )
	,	Pos				( 0 )
	,	BufferBase		( 0 )
	,	BufferCount		( 0 )
	,	MaxCacheEntries ( 24 )
	,	CacheSize		( 32768*8 )
	,	ReadBytes		( 0 )
	{
		guard(FArchiveFileReader::FArchiveFileReader);

		// Set filename.
		Filename = FString::Printf( TEXT("%s"), InFilename );

		// Set read mode.
		Mode.trycount = 3;
		Mode.spindlctrl = SCECdSpinNom;
		Mode.datapattern = SCECdSecS2048;

		// Seek to start of file.
		Seek( 0 );

		// Set status flags.
		ArIsLoading = ArIsPersistent = 1;

		unguard;
	}
	~FArchiveFileReader()
	{
		guard(FArchiveFileReader::~FArchiveFileReader);
		if( File )
			Close();
		unguard;
	}
	// Read approximately HintCount bytes of data.
	void Precache( INT HintCount )
	{
		guardSlow(FArchiveFileReader::Precache);
		unguardSlow;
	}
	// Seek to nearest sector in software.
	// Physical media seek isn't needed, hardware does it on first read.
	void Seek( INT InPos )
	{
		guard(FArchiveFileReader::Seek);
		check(InPos>=0);
		check(InPos<=File->size);
//		printf("\nSeek in %s [%i]\n", *Filename, InPos);

		// Software seek.
		INT NewSector = InPos / 2048;
		Pos			= InPos;
		Offset		= Pos - (NewSector * 2048);
		Sector		= NewSector;

		// Is this location in a cache?
		CacheKey = 0;
		INT CacheCount = 0;
		INT AlignedPos = InPos - (InPos % CacheSize);
//		printf("  Aligned Pos: %i\n", AlignedPos);
		for( TMap<INT, BYTE*>::TIterator It(ReadCache); It; ++It )
		{
			CacheCount++;

			if (AlignedPos == It.Key())
				CacheKey = It.Key();
		}

		if (CacheKey == 0)
		{
			// We did not find this position in the Cache.
			if (CacheCount < MaxCacheEntries)
			{
				// We are not at the max number of cache entries, add a new one.
				AddCacheEntry(-1, AlignedPos, CacheCount);
			} else {
				// We have too many cache entries.  Find and replace the oldest.
				FTime OldestCache = (FLOAT) MAXDWORD;
				INT DeadCache = -1;
				for( TMap<INT, FTime>::TIterator Ic(CacheTimestamp); Ic; ++Ic )
				{
					//printf("test: %i %f %f\n", Ic.Key(), Ic.Value().GetFloat(), OldestCache.GetFloat());
					if (Ic.Value().GetFloat() < OldestCache.GetFloat())
					{
						OldestCache = Ic.Value();
						DeadCache = Ic.Key();
					}
				}
				AddCacheEntry(DeadCache, AlignedPos, CacheCount);
			}
			CacheKey = AlignedPos;
		}

		unguard;
	}
	void AddCacheEntry(INT DeadCache, INT AlignedPos, INT CacheCount)
	{
		guard(FArchiveFileReader::AddCacheEntry);

		printf("AddCacheEntry %i [%i] %f %i\n", AlignedPos, AlignedPos/CacheSize, appSeconds().GetFloat(), ReadBytes);

		if (DeadCache != -1)
		{
			guard(KillCache);

			// Overwrite an existing entry.
			BYTE** Entry = ReadCache.Find( DeadCache );
			if (Entry != NULL)
			{
				if (*Entry != NULL)
				{
					//printf("Freeing\n");
					appFree( *Entry );
					ReadCache.Remove( DeadCache );
					CacheTimestamp.Remove( DeadCache );
				}
			}

			unguard;
		}

		// Add a new cache entry.
		CacheTimestamp.Set( AlignedPos, appSeconds() );
		BYTE* NewMem = (BYTE*) appMalloc( CacheSize, TEXT("Read Cache") );
		ReadCache.Set( AlignedPos, NewMem );
		CacheData( NewMem, AlignedPos );

		unguard;
	}
	void CacheData( BYTE* Mem, INT AlignedPos )
	{
		guard(FArchiveFileReader::CacheData);

		INT CacheSector = AlignedPos / 2048;
		INT ReadCount = CacheSize / 2048;
		if( sceCdRead( File->lsn+CacheSector, ReadCount, Mem, &Mode ) == 0 )
		{
			ArIsError = 1;
			Error->Logf( TEXT("Failed to issue CD read!") );
		}

		// Wait for command completion.
		if (sceCdSync(0) == 1)
			printf("Failed to sync CD!\n");

		unguard;
	}
	INT Tell()
	{
		return Pos;
	}
	INT TotalSize()
	{
		return File->size;
	}
	UBOOL Close()
	{
		guardSlow(FArchiveFileReader::Close);
		for( TMap<INT, BYTE*>::TIterator It(ReadCache); It; ++It )
		{
			// Empty allocated cache spaces.
			appFree( It.Value() );
		}
		ReadCache.Empty();
		CacheTimestamp.Empty();
		delete File;
		File = NULL;
		return !ArIsError;
		unguardSlow;
	}
	void Serialize( void* V, INT Length )
	{
		guardSlow(FArchiveFileReader::Serialize);

		INT AlignedPos = Pos - (Pos % CacheSize);
		if (AlignedPos == CacheKey)
		{
			// This position is inside this cache.
			INT MemOffset = Pos - CacheKey;
			BYTE** Mem = ReadCache.Find(CacheKey);
			if (Mem == NULL)
			{
				ArIsError = 1;
				Error->Logf( TEXT("Failed to find cached entry.") );
			}
			if (Pos+Length <= CacheKey+CacheSize)
			{
//				printf("  All is cached.\n");
				// In fact, all of the data we need is cached.
				appMemcpy( V, *Mem+MemOffset, Length );
			} else {
				// Only some of the data we need is cached.
				INT CachedLength = (Pos+Length) - (CacheKey+CacheSize);
				appMemcpy( V, *Mem+MemOffset, CachedLength );
				INT RemainingLength = Length - CachedLength;

//				printf("  Reading the remainder. %i %i %i\n", CachedLength, RemainingLength, Length);

				// Read the rest.
				BYTE* ReadBuffer;
				INT ReadCount = Length / 2048;
				ReadBuffer = (BYTE*) appMalloc( (ReadCount+1)*2048, TEXT("Temp Readdata") );

				// Issue non blocking CD read.
				if( sceCdRead( File->lsn+Sector, ReadCount+1, ReadBuffer, &Mode ) == 0 )
				{
					ArIsError = 1;
					Error->Logf( TEXT("Failed to issue CD read: Length=%i"), Length );
				}

				// Wait for command completion.
				if (sceCdSync(0) == 1)
					printf("Failed to sync CD!\n");

				appMemcpy( (BYTE*)V+CachedLength, ReadBuffer + Offset + CachedLength, RemainingLength );

				appFree(ReadBuffer);
			}
		} else {
//			printf("  None is cached.\n");
			// This position is outside of the cache.
			// Recache from this position.
			Seek( Pos );

			Serialize( V, Length );
			return;
		}

		// Update software position.
		ReadBytes += Length;
		Pos += Length;
		Sector = Pos / 2048;
		Offset = Pos - (Sector*2048);

		unguardSlow;
	}
	/*
	void Serialize( void* V, INT Length )
	{
		guardSlow(FArchiveFileReader::Serialize);
		BYTE* ReadBuffer;
		//printf("PS2 CD: Serialize %i from %i\n", Length, Pos);
		while( Length>0 )
		{
			INT Copy = Min( Length, BufferBase+BufferCount-Pos );
			//printf("Copy: %i/%i (%i) %i\n", Copy, BufferOffset, BufferBase, BufferCount);
			if( Copy==0 )
			{
				if( Length >= ARRAY_COUNT(Buffer) )
				{
					INT ReadCount = Length / 2048;
					ReadBuffer = (BYTE*) appMalloc( (ReadCount+1)*2048, TEXT("Temp Readdata") );

					// Issue non blocking CD read.
					//printf("PS2 CD: Reading %i sectors from %i (%i).\n", ReadCount+1, Sector+File->lsn, Offset);
					if( sceCdRead( File->lsn+Sector, ReadCount+1, ReadBuffer, &Mode ) == 0 )
					{
						ArIsError = 1;
						Error->Logf( TEXT("Failed to issue CD read: Length=%i"), Length );
					}

					// Wait for command completion.
					if (sceCdSync(0) == 1)
						printf("Failed to sync CD!\n");

//					printf("%x %x %x %x\n", ReadBuffer[0],ReadBuffer[1],ReadBuffer[2],ReadBuffer[3]);
//					printf("%x %x %x %x\n", ReadBuffer[0+Offset],ReadBuffer[1+Offset],ReadBuffer[2+Offset],ReadBuffer[3+Offset]);

					appMemcpy( V, ReadBuffer + Offset, Length );
					Pos += Length;
					Sector = Pos / 2048;
					Offset = Pos - (Sector*2048);
					BufferBase += Length;
					appFree(ReadBuffer);
					return;
				}
				Precache( MAXINT );
				Copy = Min( Length, BufferBase+BufferCount-Pos );
				//printf("PS2 CD: Copy is %i\n", Copy);
				if( Copy<=0 )
				{
					ArIsError = 1;
					Error->Logf( TEXT("ReadFile beyond EOF %i+%i/%i"), Pos, Length, File->size );
				}
				if( ArIsError )
					return;
			}
			//printf("PS2 CD: Buffer Memcopy %i Offset: %i\n", Copy, BufferOffset);
			//printf("Read offset: %i\n", BufferOffset+Pos-BufferBase);
//			printf("%x %x %x %x\n", Buffer[0],Buffer[1],Buffer[2],Buffer[3]);
//			printf("%x %x %x %x\n", Buffer[0+BufferOffset],Buffer[1+BufferOffset],Buffer[2+BufferOffset],Buffer[3+BufferOffset]);
			appMemcpy( V, Buffer+BufferOffset+Pos-BufferBase, Copy );
			Pos          += Copy;
			Length       -= Copy;
			Sector	     = Pos / 2048;
			Offset       = Pos - (Sector*2048);
			V            = (BYTE*)V + Copy;
		}
		unguardSlow;
	}
	*/
protected:
	sceCdlFILE*		File;
	FString			Filename;
	FOutputDevice*	Error;
	INT				Pos;					// Byte position in file.
	INT				Offset;					// Byte offset from sector position.  (Sector*2048 + Offset == Pos)
	INT				Sector;					// Sector position. (Sector + File->lsn) == Logical Sector on CD
	INT				BufferBase;
	INT				BufferCount;
	INT				BufferOffset;
	BYTE			Buffer[67584];
	TMap<INT, BYTE*> ReadCache;
	TMap<INT, FTime> CacheTimestamp;
	INT				MaxCacheEntries;
	INT				CacheSize;
	INT				CacheKey;
	sceCdRMode		Mode;
	INT				ReadBytes;
};

/*
class FArchiveFileReaderConfig : public FArchive
{
public:
	FArchiveFileReaderConfig( sceCdlFILE* InFile, FOutputDevice* InError )
	:	File			( InFile )
	,	Error			( InError )
	,	Pos				( 0 )
	,	BufferBase		( 0 )
	,	BufferCount		( 0 )
	{
		guard(FArchiveFileReaderConfig::FArchiveFileReaderConfig);

		// Set read mode.
		Mode.trycount = 3;
		Mode.spindlctrl = SCECdSpinNom;
		Mode.datapattern = SCECdSecS2048;

		// Seek to start of file.
		Seek( 0 );

		// Read the file in its entirity.
		FileData = appMalloc( File->size, TEXT("Config File") );
		if( sceCdRead( File->lsn+Sector, ReadCount+1, ReadBuffer, &Mode ) == 0 )

		// Set status flags.
		ArIsLoading = ArIsPersistent = 1;

		unguard;
	}
	~FArchiveFileReaderConfig()
	{
		guard(FArchiveFileReaderConfig::~FArchiveFileReaderConfig);
		if( File )
			Close();
		unguard;
	}
	// Read approximately HintCount bytes of data.
	void Precache( INT HintCount )
	{
		guardSlow(FArchiveFileReaderConfig::Precache);
		BufferBase = Pos;
		BufferOffset = Offset;
		BufferCount = Min( HintCount, (INT)ARRAY_COUNT(Buffer)-2048 );
		BufferCount = Min( BufferCount, (INT)File->size-Pos );
//		BufferCount = Min( Min( HintCount, (INT)(ARRAY_COUNT(Buffer) - (Pos&(ARRAY_COUNT(Buffer)-1))) ), (INT)File->size-Pos );
		//printf("PS2 CD: Precache %i, Count %i, Offset: %i Pos: %i\n", HintCount, BufferCount, BufferOffset, Pos);

		INT ReadSectors = (Offset+BufferCount) / 2048;

		if (BufferCount > ARRAY_COUNT(Buffer))
			appFailAssert( "Precache", "FFileManagerPSX2CD.h", 64 );	

		// Issue non blocking CD read.
		if( sceCdRead( File->lsn+Sector, ReadSectors+1, Buffer, &Mode ) == 0 )
		{
			ArIsError = 1;
			Error->Logf( TEXT("fread failed: BufferCount=%i"), BufferCount );
			return;
		}

		// Wait for command completion.
		sceCdSync(0);

		unguardSlow;
	}
	// Seek to nearest sector.
	void Seek( INT InPos )
	{
		guard(FArchiveFileReaderConfig::Seek);
		check(InPos>=0);
		check(InPos<=File->size);
		//printf("PS2 CD: Seek %i/%i\n", InPos, File->size);
		INT NewSector = InPos / 2048;
		//printf("  Seeking to sector %i\n", NewSector);
		if( sceCdSeek( File->lsn + NewSector ) == 0 )
		{
			ArIsError = 1;
			Error->Logf( TEXT("Seek failed: %i/%i: %i"), InPos, File->size, Pos );
		}

		// Wait for command completion.
		sceCdSync(0);

		Pos			= InPos;
		Offset		= Pos - (NewSector * 2048);
		//printf("  Offset from sector: %i\n", Offset);
		Sector		= NewSector;
		BufferBase 	= Pos;
		BufferOffset= Offset;
		BufferCount = 0;
		unguard;
	}
	INT Tell()
	{
		return Pos;
	}
	INT TotalSize()
	{
		return File->size;
	}
	UBOOL Close()
	{
		guardSlow(FArchiveFileReaderConfig::Close);
		delete File;
		File = NULL;
		return !ArIsError;
		unguardSlow;
	}
	void Serialize( void* V, INT Length )
	{
		guardSlow(FArchiveFileReaderConfig::Serialize);
		BYTE* ReadBuffer;
		//printf("PS2 CD: Serialize %i from %i\n", Length, Pos);
		while( Length>0 )
		{
			INT Copy = Min( Length, BufferBase+BufferCount-Pos );
			//printf("Copy: %i/%i (%i) %i\n", Copy, BufferOffset, BufferBase, BufferCount);
			if( Copy==0 )
			{
				if( Length >= ARRAY_COUNT(Buffer) )
				{
					INT ReadCount = Length / 2048;
					ReadBuffer = (BYTE*) appMalloc( (ReadCount+1)*2048, TEXT("Temp Readdata") );

					// Issue non blocking CD read.
					//printf("PS2 CD: Reading %i sectors from %i (%i).\n", ReadCount+1, Sector+File->lsn, Offset);
					if( sceCdRead( File->lsn+Sector, ReadCount+1, ReadBuffer, &Mode ) == 0 )
					{
						ArIsError = 1;
						Error->Logf( TEXT("Failed to issue CD read: Length=%i"), Length );
					}

					// Wait for command completion.
					if (sceCdSync(0) == 1)
						printf("Failed to sync CD!\n");

//					printf("%x %x %x %x\n", ReadBuffer[0],ReadBuffer[1],ReadBuffer[2],ReadBuffer[3]);
//					printf("%x %x %x %x\n", ReadBuffer[0+Offset],ReadBuffer[1+Offset],ReadBuffer[2+Offset],ReadBuffer[3+Offset]);

					appMemcpy( V, ReadBuffer + Offset, Length );
					Pos += Length;
					Sector = Pos / 2048;
					Offset = Pos - (Sector*2048);
					BufferBase += Length;
					appFree(ReadBuffer);
					return;
				}
				Precache( MAXINT );
				Copy = Min( Length, BufferBase+BufferCount-Pos );
				//printf("PS2 CD: Copy is %i\n", Copy);
				if( Copy<=0 )
				{
					ArIsError = 1;
					Error->Logf( TEXT("ReadFile beyond EOF %i+%i/%i"), Pos, Length, File->size );
				}
				if( ArIsError )
					return;
			}
			//printf("PS2 CD: Buffer Memcopy %i Offset: %i\n", Copy, BufferOffset);
			//printf("Read offset: %i\n", BufferOffset+Pos-BufferBase);
//			printf("%x %x %x %x\n", Buffer[0],Buffer[1],Buffer[2],Buffer[3]);
//			printf("%x %x %x %x\n", Buffer[0+BufferOffset],Buffer[1+BufferOffset],Buffer[2+BufferOffset],Buffer[3+BufferOffset]);
			appMemcpy( V, Buffer+BufferOffset+Pos-BufferBase, Copy );
			Pos          += Copy;
			Length       -= Copy;
			Sector	     = Pos / 2048;
			Offset       = Pos - (Sector*2048);
			V            = (BYTE*)V + Copy;
		}
		unguardSlow;
	}
protected:
	sceCdlFILE*		File;
	BYTE*			FileData;
	FOutputDevice*	Error;
	INT				Pos;					// Byte position in file.
	INT				Offset;					// Byte offset from sector position.  (Sector*2048 + Offset == Pos)
	INT				Sector;					// Sector position. (Sector + File->lsn) == Logical Sector on CD
	INT				BufferBase;
	INT				BufferCount;
	INT				BufferOffset;
	BYTE			Buffer[6144];
	sceCdRMode		Mode;
};
*/
class FArchiveFileWriter : public FArchive
{
public:
	FArchiveFileWriter( INT InFile, FOutputDevice* InError )
	:	File		(InFile)
	,	Error		( InError )
	,	Pos			(0)
	,	BufferCount	(0)
	{
		ArIsSaving = ArIsPersistent = 1;
	}
	~FArchiveFileWriter()
	{
		guard(FArchiveFileWriter::~FArchiveFileWriter);
		if( File )
			Close();
		File = NULL;
		unguard;
	}
	void Seek( INT InPos )
	{
		Flush();
		if( sceLseek(File,InPos,SCE_SEEK_SET) )
		{
			ArIsError = 1;
			Error->Logf( LocalizeError("SeekFailed",TEXT("Core")) );
		}
		Pos = InPos;
	}
	INT Tell()
	{
		return Pos;
	}
	UBOOL Close()
	{
		guardSlow(FArchiveFileWriter::Close);
		Flush();
		if( File && sceClose( File ) )
		{
			ArIsError = 1;
			Error->Logf( LocalizeError("WriteFailed",TEXT("Core")) );
		}
		File = NULL;
		return !ArIsError;
		unguardSlow;
	}
	void Serialize( void* V, INT Length )
	{
		Pos += Length;
		INT Copy=Length;
		while( Length > (Copy=ARRAY_COUNT(Buffer)-BufferCount) )
		{
			appMemcpy( Buffer+BufferCount, V, Copy );
			BufferCount += Copy;
			Length      -= Copy;
			V            = (BYTE*)V + Copy;
			Flush();
		}
		if( Length )
		{
			appMemcpy( Buffer+BufferCount, V, Length );
			BufferCount += Length;
		}
	}
	void Flush()
	{
		if( BufferCount && sceWrite( File, Buffer, BufferCount )==-1 )
		{
			ArIsError = 1;
			Error->Logf( LocalizeError("WriteFailed",TEXT("Core")) );
		}
		BufferCount=0;
	}
protected:
	INT				File;
	FOutputDevice*	Error;
	INT				Pos;
	INT				BufferCount;
	BYTE			Buffer[4096];
};

class FFileManagerPSX2CD : public FFileManagerGeneric
{
public:
	FArchive* CreateFileReader( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
	{
		guard(FFileManagerPSX2CD::CreateFileReader);

		sceCdDiskReady(0);

		sceCdlFILE* File = new sceCdlFILE;

		FString OpenName = FString::Printf(TEXT("\\%s;1"), Filename);
		OpenName = OpenName.Caps();
		INT FoundFile = sceCdSearchFile( File, *OpenName );
		if (FoundFile == 0)
		{
			delete File;
			return NULL;
		}
		printf("Opened file: %s\n", Filename);
		return new(TEXT("PSX2CDFileReader"))FArchiveFileReader(Filename, File, Error);
		unguard;
	}
	FArchive* CreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
	{
		guard(FFileManagerPSX2CD::CreateFileWriter);
		return NULL; // No CD files.
		unguard;
	}
	UBOOL Delete( const TCHAR* Filename, UBOOL RequireExists=0, UBOOL EvenReadOnly=0 )
	{
		guard(FFileManagerPSX2CD::Delete);
		return true;
		unguard;
	}
	SQWORD GetGlobalTime( const TCHAR* Filename )
	{
		guard(FFileManagerPSX2CD::GetGlobalTime);
		return 0;
		unguard;
	}
	UBOOL SetGlobalTime( const TCHAR* Filename )
	{
		guard(FFileManagerPSX2CD::SetGlobalTime);
		return 0;		
		unguard;
	}
	UBOOL MakeDirectory( const TCHAR* Path, UBOOL Tree=0 )
	{
		guard(FFileManagerPSX2CD::MakeDirectory);
		return 0;
		unguard;
	}
	UBOOL DeleteDirectory( const TCHAR* Path, UBOOL RequireExists=0, UBOOL Tree=0 )
	{
		guard(FFileManagerPSX2CD::DeleteDirectory);
		return 0;
		unguard;
	}
	TArray<FString> FindFiles( const TCHAR* Filename, UBOOL Files, UBOOL Directories )
	{
		guard(FFileManagerPSX2CD::FindFiles);
		TArray<FString> Result;	
		return Result;
		unguard;
	}
	UBOOL SetDefaultDirectory( const TCHAR* Filename )
	{
		guard(FFileManagerPSX2CD::SetDefaultDirectory);
		return 0;
		unguard;
	}
	FString GetDefaultDirectory()
	{
		FString blah;
		return blah;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
