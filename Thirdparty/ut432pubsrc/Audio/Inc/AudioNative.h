/*=============================================================================
	AudioNative.h: Native function lookup table for static libraries.
	Copyright 2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Brandon Reinhart
=============================================================================*/

#ifndef AUDIONATIVE_H
#define AUDIONATIVE_H

#if __STATIC_LINK

#include "AudioTypes.h"
#include "AudioSubsystem.h"

/* No native execs. */

#define AUTO_INITIALIZE_REGISTRANTS_AUDIO \
	UGenericAudioSubsystem::StaticClass();

#endif

#endif
