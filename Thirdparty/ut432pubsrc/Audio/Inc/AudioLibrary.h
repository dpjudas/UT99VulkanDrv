/*=============================================================================
	AudioLibrary.h: Unreal general audio management functions.
	Copyright 1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Brandon Reinhart.
=============================================================================*/

#include "AudioTypes.h"

/*------------------------------------------------------------------------------------
	Globals
------------------------------------------------------------------------------------*/

// Raw Buffer
extern void* 	AudioBuffer;
extern void*	MixBuffer;
extern INT   	BufferSize;
extern INT		AudioFormat;
extern INT		AudioRate;

// State
extern INT   	AudioInitialized;
extern INT		AudioPaused;

// Audio
extern INT		MusicVoices;
extern INT		SampleVoices;
extern Voice 	Voices[AUDIO_TOTALVOICES];

// Volume
extern INT		SampleVolume;

// Threads
extern AudioThread		MixingThread;
extern AudioMutex		Mutex;

/*------------------------------------------------------------------------------------
	Interface
------------------------------------------------------------------------------------*/

// Library control.
UBOOL AudioInit( DWORD Rate, INT OutputMode, INT Latency );
UBOOL AudioReinit( DWORD Rate, INT OutputMode, INT Latency );
UBOOL AudioShutdown();
UBOOL AllocateVoices( INT Channels );

// Master control.
UBOOL AudioStartOutput( DWORD Rate, INT OutputMode, INT Latency );
UBOOL AudioStopOutput();

// Sample control.
Sample* LoadSample( MemChunk* LoadChunk, const TCHAR* SampleName );
UBOOL UnloadSample( Sample* UnloadSample );
Voice* StartSample( INT Voice, Sample* Sample, INT Freq, INT Volume, INT Panning );
UBOOL StopSample( Voice* StopVoice );
UBOOL SampleFinished( Voice* Sample );
void UpdateSample( Voice* InVoice, INT Freq, INT Volume, INT Panning );

// Voice control.
Voice* AcquireVoice();
void GetVoiceStats( VoiceStats* InStats, Voice* InVoice );

// Volume control.
UBOOL SetSampleVolume( FLOAT Volume );
UBOOL SetMusicVolume( FLOAT Volume );
UBOOL SetCDAudioVolume( FLOAT Volume );

// CD Audio control.
UBOOL StartCDAudio( INT Track );
UBOOL StopCDAudio();