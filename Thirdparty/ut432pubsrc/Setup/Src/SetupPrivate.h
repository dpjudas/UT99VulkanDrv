/*=============================================================================
	FilerPrivate.h: Unreal installer/filer.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

// Includes.
#include "Core.h"
#include "Setup.h"

// Globals.
UBOOL RemoveEmptyDirectory( FString Dir );
void LocalizedFileError( const TCHAR* Key, const TCHAR* AdviceKey, const TCHAR* Filename );

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
