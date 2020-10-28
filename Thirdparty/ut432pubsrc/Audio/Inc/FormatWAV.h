/*=============================================================================
	FormatWAV.h: WAV file format.
	Copyright 1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Brandon Reinhart.
=============================================================================*/

typedef struct
{
	BYTE	ID[4];
	DWORD	Size;
	BYTE	Type[4];
} WAVFileHeader;

typedef struct
{
	_WORD	Format;
	_WORD	Channels;
	DWORD	SamplesPerSec;
	DWORD	BytesPerSec;
	_WORD	BlockAlign;
	_WORD	BitsPerSample;
} WAVFormatHeader;

typedef struct
{
	_WORD	Size;
	_WORD	SamplesPerBlock;
} WAVFormatExHeader;

typedef struct
{
	DWORD	Manufacturer;
	DWORD	Product;
	DWORD	SamplePeriod;
	DWORD	Note;
	DWORD	FineTune;
	DWORD	SMPTEFormat;
	DWORD	SMPTEOffset;
	DWORD	Loops;
	DWORD	SamplerData;
	struct
	{
		DWORD	Identifier;
		DWORD	Type;
		DWORD	Start;
		DWORD	End;
		DWORD	Fraction;
		DWORD	Count;
	} Loop[1];
} WAVSampleHeader;

typedef struct
{
	BYTE	ID[4];
	DWORD	Size;
} WAVChunkHeader;

/*------------------------------------------------------------------------------------
	WAVE management.
------------------------------------------------------------------------------------*/

Sample* LoadWAV( Sample* lSample, MemChunk* Chunk );