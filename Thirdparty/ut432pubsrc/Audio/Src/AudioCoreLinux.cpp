/*=============================================================================
	AudioCoreLinux.h: Core audio implementation for Linux.
	Copyright 1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Brandon Reinhart.
=============================================================================*/

/*------------------------------------------------------------------------------------
	Audio includes.
------------------------------------------------------------------------------------*/

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#define __USE_UNIX98 1
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/soundcard.h>
#include "AudioPrivate.h"

/*------------------------------------------------------------------------------------
	AudioCore Globals.
------------------------------------------------------------------------------------*/

// Audio Device
INT AudioDevice = -1;

#define FRAGMENT_SIZE 0x0002000C

/*------------------------------------------------------------------------------------
	AudioCore Implementation.
------------------------------------------------------------------------------------*/

// Audio state control.
void* GetAudioBuffer()
{
	return AudioBuffer;
}

INT OpenAudio( DWORD Rate, INT OutputMode, INT Latency )
{
	// Open the audio device.
	AudioDevice = open("/dev/dsp", O_WRONLY | O_NONBLOCK, 0);

	if (AudioDevice == -1)
	{
		debugf( NAME_Init, TEXT("Failed to open audio device.") );
		return 0;
	}

	// Set the buffer size.
	INT Fragment = FRAGMENT_SIZE;
	if (ioctl(AudioDevice, SNDCTL_DSP_SETFRAGMENT, &Fragment) == -1)
	{
		debugf( NAME_Init, TEXT("Failed to set fragment format.") );
		close(AudioDevice);
		AudioDevice = -1;
		return 0;
	}
	
	// Set the output format.
	INT Format;
	if (OutputMode & AUDIO_16BIT)
	{
		Format = AFMT_S16_LE;
		AudioFormat |= AUDIO_16BIT;
	} else {
		Format = AFMT_U8;
		AudioFormat &= ~AUDIO_16BIT;
	}
	if (ioctl(AudioDevice, SNDCTL_DSP_SETFMT, &Format) == -1)
	{
		debugf( NAME_Init, TEXT("Failed to set audio format.") );
		close(AudioDevice);
		AudioDevice = -1;
		return 0;
	}

	// Set stereo.
	INT Stereo;
	if (OutputMode & AUDIO_STEREO)
	{
		Stereo = 1;
		AudioFormat |= AUDIO_STEREO;
	} else {
 		Stereo = 0;
		AudioFormat &= ~AUDIO_STEREO;
	}
	if (ioctl(AudioDevice, SNDCTL_DSP_STEREO, &Stereo) == -1)
	{
		debugf( NAME_Init, TEXT("Failed to enable stereo audio.") );
		close(AudioDevice);
		AudioDevice = -1;
		return 0;
	}

	// Get buffer size.
	if (ioctl(AudioDevice, SNDCTL_DSP_GETBLKSIZE, &BufferSize) == -1)
	{
		debugf( NAME_Init, TEXT("Failed to get audio buffer size.") );
		close(AudioDevice);
		AudioDevice = -1;
		return 0;
	}

	// Set the rate.
	if (ioctl(AudioDevice, SNDCTL_DSP_SPEED, &Rate) == -1)
	{
		debugf( NAME_Init, TEXT("Failed to set playback rate to %iHz"), Rate );
		close(AudioDevice);
		AudioDevice = -1;
		return 0;
	}
	AudioRate = Rate;

	// Initialize AudioBuffer.
	debugf( NAME_Init, TEXT("Allocating an audio buffer of %i bytes."), BufferSize );
	AudioBuffer = (BYTE*) appMalloc( BufferSize, TEXT("Audio Buffer") );

	return 1;
}

INT ReopenAudioDevice( DWORD Rate, INT OutputMode, INT Latency )
{
	ALock;
	debugf( NAME_Init, TEXT("Reopening audio device.") );
	CloseAudio();
	INT Result = OpenAudio( Rate, OutputMode, Latency );
	AUnlock;

	return Result;
}

void CloseAudio()
{
	ALock;
	if (AudioBuffer != NULL)
	{
		appFree(AudioBuffer);
		AudioBuffer = NULL;
	}
	if (AudioDevice > -1)
	{
		close(AudioDevice);
		AudioDevice = -1;
	}
	AUnlock;
}

// Audio flow control.
void PlayAudio()
{
	if (AudioDevice == -1)
		return;
		
	ssize_t WriteResult;
	WriteResult = write( AudioDevice, AudioBuffer, BufferSize );

	if (WriteResult == -1)
	{
		switch (errno)
		{
			case 0:				// No error condition, try again?
			case EAGAIN:		// No room, write again.
			case EINTR:			// Write blocked, try again.
				PlayAudio();
				break;			
			default:
				// Handle legit error.
				break;
		}
	}
}

/*------------------------------------------------------------------------------------
	Read helpers.
------------------------------------------------------------------------------------*/

void* ReadMem( void* DestData, INT NumChunks, INT ChunkSize, MemChunk* SrcData )
{
	void* Result = memcpy( DestData, SrcData->Data + SrcData->Position, ChunkSize*NumChunks );
	SrcData->Position += ChunkSize*NumChunks;

	return Result;
}

void* SeekMem( MemChunk* SrcData, INT Position, INT SeekMode )
{
	switch (SeekMode)
	{
		case MEM_SEEK_CUR:
			SrcData->Position += Position;
			break;
		case MEM_SEEK_ABS:
			SrcData->Position = Position;
			break;
	}
	return SrcData->Data;
}

BYTE Read8_Mem( void* Data )
{
	BYTE Result;

	memcpy( Data, &Result, (sizeof Result) );

	return Result;
}

_WORD Read16_Mem( void* Data )
{
	_WORD Result;

	memcpy( Data, &Result, (sizeof Result) );

	return Result;
}

DWORD Read32_Mem( void* Data )
{
	DWORD Result;

	memcpy( Data, &Result, (sizeof Result) );

	return Result;
}

QWORD Read64_Mem( void* Data )
{
	QWORD Result;

	memcpy( Data, &Result, (sizeof Result) );

	return Result;
}

/*------------------------------------------------------------------------------------
	Thread control.
------------------------------------------------------------------------------------*/

UBOOL CreateAudioThread(AudioThread* NewThread, void* (*ThreadRoutine)(void*) )
{
	// Allocate a new thread.
	pthread_t* NewPosixThread;
	NewPosixThread = (pthread_t*) appMalloc(sizeof(pthread_t), TEXT("POSIX Thread"));

	// Initialize parameters.
	pthread_attr_t NewThreadAttributes;
	pthread_attr_init(&NewThreadAttributes);
	pthread_attr_setdetachstate(&NewThreadAttributes, PTHREAD_CREATE_JOINABLE);

	// Try to create the thread.
	NewThread->Valid = 1;
	INT Error = pthread_create(NewPosixThread, &NewThreadAttributes, ThreadRoutine, NULL);
	if (Error != 0)
	{
		// Some error occured.
		NewThread->Valid = 0;
		appErrorf( TEXT("Failed to create a valid mixing thread.") );
		return 0;
	}
	NewThread->Thread = NewPosixThread;
	NewThread->Exited = 0;
	debugf( NAME_Init, TEXT("Created a new audio thread.") );

	return 1;
}

UBOOL DestroyAudioThread(AudioThread* OldThread)
{
	ALock;
	OldThread->Valid = 0;
	AUnlock;
	pthread_t* Thread = (pthread_t*) OldThread->Thread;
	pthread_join(*Thread, NULL);
	appFree(OldThread->Thread);
	return 1;
}

UBOOL ExitAudioThread(AudioThread* Thread)
{
	Thread->Exited = 1;
	pthread_exit(NULL);
	return 1;
}

UBOOL CreateAudioMutex(AudioMutex* Mutex)
{
	pthread_mutex_t* NewMutex;
	NewMutex = (pthread_mutex_t*) appMalloc(sizeof(pthread_mutex_t), TEXT("POSIX Mutex"));

	pthread_mutexattr_t MutexAttr;
	pthread_mutexattr_init(&MutexAttr);
	pthread_mutexattr_settype(&MutexAttr, PTHREAD_MUTEX_RECURSIVE_NP);
	
	pthread_mutex_init(NewMutex, &MutexAttr);
	Mutex->Mutex = NewMutex;
	return 1;
}

UBOOL DestroyAudioMutex(AudioMutex* Mutex)
{
	pthread_mutex_destroy((pthread_mutex_t*) Mutex->Mutex);
	appFree(Mutex->Mutex);
	return 1;
}

UBOOL AudioLock(AudioMutex* Mutex)
{
	pthread_mutex_lock((pthread_mutex_t*) Mutex->Mutex);
	return 1;
}

UBOOL AudioUnlock(AudioMutex* Mutex)
{
	pthread_mutex_unlock((pthread_mutex_t*) Mutex->Mutex);
	return 1;
}

/*------------------------------------------------------------------------------------
	Timing
------------------------------------------------------------------------------------*/

// Sleeps for ms milliseconds.
void AudioSleep(INT	ms)
{
	struct timeval tv;

	tv.tv_sec = ms/1000;
	tv.tv_usec = (ms%1000)*1000;
	select(0, NULL, NULL, NULL, &tv);
}

// Blocks until audio device is open.
INT AudioWait()
{
	if (AudioDevice && AudioInitialized)
	{
		fd_set fdset;
		FD_ZERO(&fdset);
		FD_SET(AudioDevice, &fdset);
		select(AudioDevice+1, NULL, &fdset, NULL, NULL);
		return 1;
	} else
		return 0;
}
