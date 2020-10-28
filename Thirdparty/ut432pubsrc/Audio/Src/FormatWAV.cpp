/*=============================================================================
	FormatWAV.cpp: Unreal audio wav reading code.
	Copyright 1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Brandon Reinhart.
=============================================================================*/

/*------------------------------------------------------------------------------------
	Audio includes.
------------------------------------------------------------------------------------*/

#include "AudioPrivate.h"

/*------------------------------------------------------------------------------------
	WAVE management.
------------------------------------------------------------------------------------*/

Sample* LoadWAV( Sample* lSample, MemChunk* Chunk )
{
	WAVFileHeader FileHeader;
	ReadMem(&FileHeader, 1, sizeof(WAVFileHeader), Chunk);
	FileHeader.Size = ((FileHeader.Size+1)&~1);
	FileHeader.Size -= 4;

	WAVChunkHeader		ChunkHeader;
	WAVFormatHeader 	FormatHeader;
	WAVFormatExHeader 	FormatExHeader;
	WAVSampleHeader		SampleHeader;
	while (FileHeader.Size != 0)
	{
		if (ReadMem(&ChunkHeader, 1, sizeof(WAVChunkHeader), Chunk))
		{
			if (!appMemcmp(ChunkHeader.ID, "fmt ", 4))
			{
				ReadMem(&FormatHeader, 1, sizeof(WAVFormatHeader), Chunk);
				if (FormatHeader.Format != 0x0001)
				{
					ReadMem(&FormatExHeader, 1, sizeof(WAVFormatExHeader), Chunk);
					SeekMem(Chunk, ChunkHeader.Size - sizeof(WAVFormatHeader) - sizeof(WAVFormatExHeader), MEM_SEEK_CUR);
				} else
					SeekMem(Chunk, ChunkHeader.Size - sizeof(WAVFormatHeader), MEM_SEEK_CUR);
			}
			else if (!appMemcmp(ChunkHeader.ID, "data", 4))
			{
				if (FormatHeader.Format == 0x0001)
				{
					lSample->Size = sizeof(lSample);
					lSample->Panning = AUDIO_MIDPAN;
					lSample->Volume = AUDIO_MAXVOLUME;
					if (FormatHeader.Channels == 2)
						lSample->Type = SAMPLE_STEREO;
					else
						lSample->Type = SAMPLE_MONO;
					if (FormatHeader.BitsPerSample == 16)
						lSample->Type |= SAMPLE_16BIT;
					else
						lSample->Type |= SAMPLE_8BIT;
					lSample->Length = (ChunkHeader.Size / FormatHeader.BlockAlign);
					lSample->LoopStart = 0;
					lSample->LoopEnd = 0;
					lSample->SamplesPerSec = FormatHeader.SamplesPerSec;
					if ((lSample->SamplesPerSec != 11025) && (lSample->SamplesPerSec != 22050) && (lSample->SamplesPerSec != 44100))
						appErrorf( TEXT("Unsupported rate: %i"), lSample->SamplesPerSec );
					if (lSample->Data = appMalloc(ChunkHeader.Size, TEXT("Sample data.")))
						ReadMem(lSample->Data, FormatHeader.BlockAlign, lSample->Length, Chunk);
				} else {
					appErrorf( TEXT("Unsupported WAVE format!") );
					SeekMem(Chunk, ChunkHeader.Size, MEM_SEEK_CUR);
				}
			}
			else if (!appMemcmp(ChunkHeader.ID, "smpl", 4))
			{
				ReadMem(&SampleHeader, 1, sizeof(WAVSampleHeader), Chunk);
				SeekMem(Chunk, ChunkHeader.Size - sizeof(WAVSampleHeader), MEM_SEEK_CUR);
				if (SampleHeader.Loops != 0)
				{
					if (SampleHeader.Loop[0].Type & 1)
						lSample->Type |= SAMPLE_BIDILOOP | SAMPLE_LOOPED;
					else
						lSample->Type |= SAMPLE_LOOPED;
					lSample->LoopStart = SampleHeader.Loop[0].Start;
					lSample->LoopEnd = SampleHeader.Loop[0].End;
				}
			}
			else
				// Move past this chunk. 
				SeekMem(Chunk, ChunkHeader.Size, MEM_SEEK_CUR);
				
			SeekMem(Chunk, ChunkHeader.Size & 1, MEM_SEEK_CUR);
			FileHeader.Size -= (((ChunkHeader.Size+1)&~1)+8);
		}
	}
	return lSample;
}

