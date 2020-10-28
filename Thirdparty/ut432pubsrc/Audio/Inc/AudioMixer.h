/*=============================================================================
	AudioMixer.h: Unreal sound mixer.
	Copyright 1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Brandon Reinhart.
=============================================================================*/

/*------------------------------------------------------------------------------------
	Mixing functions.
------------------------------------------------------------------------------------*/

void* DoSound(void* Arguments);
void MixVoice8to16(INT VoiceIndex);
void MixVoice16to16(INT VoiceIndex);

void ConvertVoice8( Voice* InVoice );
void ConvertVoice16( Voice* InVoice );

/*------------------------------------------------------------------------------------
	End.
------------------------------------------------------------------------------------*/
