/*=============================================================================
	FOutputDeviceAnsiError.h: Ansi stdout error output device.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#if UNICODE
#define appPrintf wprintf
#else
#define appPrintf printf
#endif

//
// ANSI stdout output device.
//
class FOutputDeviceAnsiError : public FOutputDeviceError
{
public:
	void Serialize( const TCHAR* Msg, enum EName Event )
	{
#ifdef _DEBUG
		// Just display info and break the debugger.
  		debugf( NAME_Critical, TEXT("appError called while debugging:") );
		debugf( NAME_Critical, Msg );
		UObject::StaticShutdownAfterError();
  		debugf( NAME_Critical, TEXT("Breaking debugger") );
		*(BYTE*)NULL=0;
#else
		if( !GIsCriticalError )
		{
			// First appError.
			GIsCriticalError = 1;
			debugf( NAME_Critical, TEXT("appError called:") );
			debugf( NAME_Critical, Msg );

			// Shut down.
			UObject::StaticShutdownAfterError();
			appStrncpy( GErrorHist, Msg, ARRAY_COUNT(GErrorHist) );
			appStrncat( GErrorHist, TEXT("\r\n\r\n"), ARRAY_COUNT(GErrorHist) );
			if( GIsGuarded )
			{
				appStrncat( GErrorHist, LocalizeError("History",TEXT("Core")), ARRAY_COUNT(GErrorHist) );
				appStrncat( GErrorHist, TEXT(": "), ARRAY_COUNT(GErrorHist) );
			}
			else
			{
				HandleError();
			}
		}
		else debugf( NAME_Critical, TEXT("Error reentered: %s"), Msg );

		// Propagate the error or exit.
		if( GIsGuarded )
			throw( 1 );
		else
			appRequestExit( 1 );
#endif
	}
	void HandleError()
	{
		try
		{
			GIsGuarded       = 0;
			GIsRunning       = 0;
			GIsCriticalError = 1;
			GLogHook         = NULL;
			UObject::StaticShutdownAfterError();
			GErrorHist[ARRAY_COUNT(GErrorHist)-1]=0;
			appPrintf( TEXT("%s\n\nExiting due to error\n"), GErrorHist );
		}
		catch( ... )
		{}
	}
};

#undef appPrintf

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
