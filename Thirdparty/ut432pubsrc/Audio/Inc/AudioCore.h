/*=============================================================================
	AudioCore.h: Core audio function declarations.
	Copyright 1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Brandon Reinhart.
=============================================================================*/

/*------------------------------------------------------------------------------------
	Audio state control.
------------------------------------------------------------------------------------*/

void* GetAudioBuffer();
INT OpenAudio( DWORD Rate, INT OutputMode, INT Latency );
void CloseAudio();
INT ReopenAudioDevice( DWORD Rate, INT OutputMode, INT Latency );

/*------------------------------------------------------------------------------------
	Audio flow control.
------------------------------------------------------------------------------------*/

void PlayAudio();

/*------------------------------------------------------------------------------------
	Read helpers.
------------------------------------------------------------------------------------*/

void* ReadMem( void* DestData, INT NumChunks, INT ChunkSize, MemChunk* SrcData );
void* SeekMem( MemChunk* SrcData, INT Position, INT SeekMode );
BYTE Read8_Mem( void* Data );
_WORD Read16_Mem( void* Data );
DWORD Read32_Mem( void* Data );
QWORD Read64_Mem( void* Data );

/*------------------------------------------------------------------------------------
	Thread control.
------------------------------------------------------------------------------------*/

UBOOL CreateAudioThread( AudioThread* NewThread, void* (*ThreadRoutine)(void*) );
UBOOL DestroyAudioThread( AudioThread* OldThread );
UBOOL ExitAudioThread( AudioThread* Thread );
UBOOL CreateAudioMutex( AudioMutex* Mutex );
UBOOL DestroyAudioMutex( AudioMutex* Mutex );
UBOOL AudioLock( AudioMutex* Mutex );
UBOOL AudioUnlock( AudioMutex* Mutex );

/*------------------------------------------------------------------------------------
	Timing.
------------------------------------------------------------------------------------*/

void AudioSleep( INT ms );
INT AudioWait();

/*------------------------------------------------------------------------------------
	End.
------------------------------------------------------------------------------------*/
