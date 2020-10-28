/*=============================================================================
	AudioMixer.cpp: Unreal sound mixer.
	Copyright 1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Brandon Reinhart.
=============================================================================*/

/*------------------------------------------------------------------------------------
	Audio includes.
------------------------------------------------------------------------------------*/

#include "AudioPrivate.h"

/*------------------------------------------------------------------------------------
	Mixing functions.
------------------------------------------------------------------------------------*/

#define SOUND_MIXING	0
#define	SOUND_PLAYING	1

#define ADJUST_VOLUME(s, v) (s = (s*v)/AUDIO_MAXVOLUME)

void* DoSound(void* Arguments)
{
	// Allocate the mixing buffer.
	ALock;
	MixBuffer = appMalloc(BufferSize, TEXT("Mixing Buffer"));
	AUnlock;

	INT Task = SOUND_MIXING, i;
	while (MixingThread.Valid)
	{
		switch (Task)
		{
			case SOUND_MIXING:
				ALock;
				// Empty the mixing buffer.
				appMemset(MixBuffer, 0, BufferSize);
				for (i=0; i<AUDIO_TOTALVOICES; i++)
				{
					// Get an enabled and active voice.
					if ((Voices[i].State&VOICE_ENABLED) && (Voices[i].State&VOICE_ACTIVE) && !AudioPaused)
					{
						// Mix a buffer's worth of sound.
						INT Format = Voices[i].pSample->Type & SAMPLE_16BIT 
							? SAMPLE_16BIT : SAMPLE_8BIT;
						switch (Format)
						{
							case SAMPLE_8BIT:
								if (AudioFormat & AUDIO_16BIT)
									MixVoice8to16( i );
								break;
							case SAMPLE_16BIT:
								if (AudioFormat & AUDIO_16BIT)
									MixVoice16to16( i );
								break;
						}
					}
				}
				AUnlock;
				Task = SOUND_PLAYING;
				break;
			case SOUND_PLAYING:
				// Block until the audio device is writable.
				if (!AudioPaused)
				{
					if (AudioWait() == 0)
						break;
				} else break;

				ALock;
				// Silence the audio buffer.
				appMemset(AudioBuffer, 0, BufferSize);

				// Ready the most recently mixed audio.
				appMemcpy(AudioBuffer, MixBuffer, BufferSize);

				// Play it.
				if (!AudioPaused)
				{
					PlayAudio();
				}
				AUnlock;

				Task = SOUND_MIXING;
				break;
		}
	}

	// Free the mixing buffer.
	ALock;
	if (MixBuffer != NULL)
		appFree(MixBuffer);
	AUnlock;
	
	ExitAudioThread(&MixingThread);
}

// Mixes an 8 bit unsigned voice into 16 bit signed output.
void MixVoice8to16(INT VoiceIndex)
{
	Voice* CurrentVoice = &Voices[VoiceIndex];

	if (CurrentVoice->State & VOICE_FINISHED)
		return;

	if (CurrentVoice->pSample->SamplesPerSec != AudioRate)
		ConvertVoice8( CurrentVoice );
	
	// How many samples are in this sound?
	INT SoundSamples = CurrentVoice->pSample->Length;

	// What is our sample size.
	INT SampleSize;
	if (AudioFormat & AUDIO_STEREO)
		SampleSize = 4;
	else
		SampleSize = 2;

	// Mix a buffer's worth of samples.
	INT SamplesToMix = BufferSize / SampleSize;

	// Get our start position...
	BYTE* Src8 = (BYTE*) CurrentVoice->pSample->Data;

	// ...fast forward to where we left off...
	Src8 += CurrentVoice->PlayPosition;

	// ...and get the initial target.
	SBYTE* Dst8 = (SBYTE*) MixBuffer;

	// Scale playing volume by the volume set by the player.
	INT VolumeAdjust = ((CurrentVoice->Volume*2) * SampleVolume) / AUDIO_MAXVOLUME;

	// Calculate panning offset.
	FLOAT PanningOffset = CurrentVoice->Panning - AUDIO_MIDPAN;
	FLOAT PanningFactor = PanningOffset / AUDIO_MIDPAN;
	UBOOL PanSample = AudioFormat & AUDIO_STEREO;

	// For each target sample, mix two input samples.
	INT MixedSample, SourceSample;
	for (INT i=0; i<SamplesToMix; i++)
	{
		// Perform this operation for each speaker.
		for (INT j=0; j<SampleSize; j += 2)
		{
			// Ignore low byte.
			Dst8++;
		
			// Scale the source by the volume.
			SourceSample =  *Src8 - 128;
			SourceSample = (SourceSample * VolumeAdjust) / AUDIO_MAXVOLUME;

			// Pan the source.
			if (PanSample)
			{
				// Even the sound.
				SourceSample /= (SampleSize/2);
				// Pan according to our factor.
				if (j == 0)
					SourceSample -= (INT) (SourceSample * PanningFactor);
				else
					SourceSample += (INT) (SourceSample * PanningFactor);
			}

			// Mix the result.
			MixedSample = SourceSample + *Dst8;

			// Bound the result.
			if (MixedSample > AUDIO_MAXSAMPLE8) {
				*Dst8 = AUDIO_MAXSAMPLE8;
			} else if (MixedSample < AUDIO_MINSAMPLE8) {
				*Dst8 = AUDIO_MINSAMPLE8;
			} else {
				*Dst8 = MixedSample;
			}

			// Destination is 16 bit, so increment another byte.
			Dst8++;
		}
		// Source is 8 bit, so increment 1 byte.
		Src8++;

		// Keep track of how much we've mixed.
		CurrentVoice->PlayPosition++;
		if (CurrentVoice->PlayPosition >= SoundSamples)
		{
			// We've run out of source.
			// Finish if this isn't a looping sound.
			CurrentVoice->PlayPosition = 0;
			if (CurrentVoice->pSample->Type & SAMPLE_LOOPED)
			{
				Src8 = (BYTE*) CurrentVoice->pSample->Data;
			} else {
				i = SamplesToMix;
				CurrentVoice->State |= VOICE_FINISHED;
				CurrentVoice->State &= ~VOICE_ACTIVE;
			}
		}
	}
}

// Mix a 16 bit signed sample into 16 bit signed output.
void MixVoice16to16(INT VoiceIndex)
{
	Voice* CurrentVoice = &Voices[VoiceIndex];

	if (CurrentVoice->State & VOICE_FINISHED)
		return;

	if (CurrentVoice->pSample->SamplesPerSec != AudioRate)
		ConvertVoice16( CurrentVoice );

	// How many samples are in this sound?
	INT SoundSamples = CurrentVoice->pSample->Length;

	// What is our sample size.
	INT SampleSize;
	if (AudioFormat & AUDIO_STEREO)
		SampleSize = 4;
	else
		SampleSize = 2;

	// Mix a buffer's worth of samples.
	INT SamplesToMix = BufferSize/SampleSize;

	// Get our start position...
	SWORD* Src16 = (SWORD*) CurrentVoice->pSample->Data;

	// ...fast forward to where we left off...
	Src16 += CurrentVoice->PlayPosition;

	// ...and get the initial target.
	SWORD* Dst16 = (SWORD*) MixBuffer;

	// Determine volume.
	INT VolumeAdjust = ((CurrentVoice->Volume*2) * SampleVolume) / AUDIO_MAXVOLUME;

	// Calculate panning offset.
	FLOAT PanningOffset = CurrentVoice->Panning - AUDIO_MIDPAN;
	FLOAT PanningFactor = PanningOffset / AUDIO_MIDPAN;
	UBOOL PanSample = AudioFormat & AUDIO_STEREO;

	// For each target sample, mix two input samples.
	INT MixedSample, SourceSample;
	for (INT i=0; i<SamplesToMix; i++)
	{
		// Perform this operation for each speaker.
		for (INT j=0; j<SampleSize; j+= 2)
		{
			// Adjust the volume.
			SourceSample = *Src16;
			SourceSample = (SourceSample * VolumeAdjust) / AUDIO_MAXVOLUME;

			// Pan the source.
			if (PanSample)
			{
				// Even the sound.
				SourceSample /= (SampleSize/2);
				// Pan according to our factor.
				if (j == 0)
					SourceSample -= (INT) (SourceSample * PanningFactor);
				else
					SourceSample += (INT) (SourceSample * PanningFactor);
			}

			// Mix the result.
			MixedSample = SourceSample + *Dst16;

			// Apply limits.
			if (MixedSample > AUDIO_MAXSAMPLE16) {
				*Dst16 = AUDIO_MAXSAMPLE16;
			} else if (MixedSample < AUDIO_MINSAMPLE16) {
				*Dst16 = AUDIO_MINSAMPLE16;
			} else {
				*Dst16 = MixedSample;
			}

			// Destination is 16 bit, so increment by an SWORD.
			Dst16++;
		}

		// Source is 16 bit, so increment by an SWORD.
		Src16++;

		// Keep track of how much we've mixed.
		CurrentVoice->PlayPosition++;
		if (CurrentVoice->PlayPosition >= SoundSamples)
		{
			// We've run out of source.
			// Finish if this isn't a looping sound.
			CurrentVoice->PlayPosition = 0;
			if (CurrentVoice->pSample->Type & SAMPLE_LOOPED)
			{
				Src16 = (SWORD*) CurrentVoice->pSample->Data;
			} else {
				i = SamplesToMix;
				CurrentVoice->State |= VOICE_FINISHED;
				CurrentVoice->State &= ~VOICE_ACTIVE;
			}
		}
	}
}

// Convert an 8 bit unsigned voice to the current rate.
// Maintains unsignedness.
void ConvertVoice8( Voice* InVoice )
{
	// How fast is this sample?
	INT VoiceRate = InVoice->pSample->SamplesPerSec;
	if ((VoiceRate != 11025) && (VoiceRate != 22050) && (VoiceRate != 44100))
		appErrorf( TEXT("Unsupported playback rate: %i"), VoiceRate );
	if (VoiceRate > AudioRate)
	{
		// This voice is slower than our current rate.
		INT RateFactor = VoiceRate / AudioRate;
		INT NewSize;
		if (InVoice->pSample->Length % 2 == 1)
			NewSize = (InVoice->pSample->Length+1) / RateFactor;
		else
			NewSize = InVoice->pSample->Length / RateFactor;
		BYTE* Source = (BYTE*) InVoice->pSample->Data;
		BYTE* NewData = (BYTE*) appMalloc(NewSize, TEXT("Sample Data"));
		check(NewData);
		BYTE* Dest = NewData;

		appMemset( Dest, 0x80, NewSize );
		for (INT i=0; i<InVoice->pSample->Length; i += RateFactor)
		{
			*Dest = Source[i];
			Dest++;
		}

		InVoice->PlayPosition = 0;
		InVoice->pSample->SamplesPerSec = AudioRate;
		InVoice->pSample->Length = NewSize;
		void* OldData = InVoice->pSample->Data;
		InVoice->pSample->Data = NewData;
		appFree( OldData );
	} else {
		// This voice is faster than our current rate.
		INT RateFactor = AudioRate / VoiceRate;
		INT NewSize = InVoice->pSample->Length * RateFactor;

		BYTE* Source = (BYTE*) InVoice->pSample->Data;
		BYTE* NewData = (BYTE*) appMalloc(NewSize, TEXT("Sample Data"));
		check(NewData);
		BYTE* Dest = NewData;

		appMemset( Dest, 0x80, NewSize );
		for (INT i=0; i<NewSize; i++)
		{
			Dest[i] = *Source;
			if (i%RateFactor == 1)
				Source++;
		}

		InVoice->PlayPosition = 0;
		InVoice->pSample->SamplesPerSec = AudioRate;
		InVoice->pSample->Length = NewSize;
		void* OldData = InVoice->pSample->Data;
		InVoice->pSample->Data = (void*) NewData;
		appFree( OldData );
	}
}

// Convert an 16 bit signed voice to the current rate.
// Maintains signedness.
void ConvertVoice16( Voice* InVoice )
{
	// How fast is this sample?
	INT VoiceRate = InVoice->pSample->SamplesPerSec;
	if ((VoiceRate != 11025) && (VoiceRate != 22050) && (VoiceRate != 44100))
		appErrorf( TEXT("Unsupported playback rate: %i"), VoiceRate );
	if (VoiceRate > AudioRate)
	{
		// This voice is faster than our current rate.
		INT RateFactor = VoiceRate / AudioRate;
		INT NewSize;
		if (InVoice->pSample->Length % 2 == 1)
			NewSize = (InVoice->pSample->Length+1) / RateFactor;
		else
			NewSize = InVoice->pSample->Length / RateFactor;

		SWORD* Source = (SWORD*) InVoice->pSample->Data;
		SWORD* NewData = (SWORD*) appMalloc(NewSize*2, TEXT("Sample Data"));
		SWORD* Dest = NewData;

		appMemset(Dest, 0, NewSize*2);
		for (INT i=0; i<InVoice->pSample->Length; i += RateFactor)
		{
			*Dest = Source[i];
			Dest++;
		}

		InVoice->PlayPosition = 0;
		InVoice->pSample->SamplesPerSec = AudioRate;
		InVoice->pSample->Length = NewSize;
		void* OldData = InVoice->pSample->Data;
		InVoice->pSample->Data = (void*) NewData;
		appFree( OldData );
	} else {
		// This voice is slower than our current rate.
		INT RateFactor = AudioRate / VoiceRate;
		INT NewSize = InVoice->pSample->Length * RateFactor;

		SWORD* Source = (SWORD*) InVoice->pSample->Data;
		SWORD* NewData = (SWORD*) appMalloc(NewSize*2, TEXT("Sample Data"));
		SWORD* Dest = NewData;

		appMemset( Dest, 0, NewSize*2 );
		for (INT i=0; i<NewSize; i++)
		{
			Dest[i] = *Source;
			if (i%RateFactor == 1)
				Source++;
		}

		InVoice->PlayPosition = 0;
		InVoice->pSample->SamplesPerSec = AudioRate;
		InVoice->pSample->Length = NewSize;
		void* OldData = InVoice->pSample->Data;
		InVoice->pSample->Data = (void*) NewData;
		appFree( OldData );
	}
}