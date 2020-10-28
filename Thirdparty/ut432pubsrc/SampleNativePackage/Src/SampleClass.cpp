/*=============================================================================
	SampleClass.cpp
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	This is a minimal example of how to mix UnrealScript and C++ code
	within a class (defined by SampleClass.uc and SampleClass.cpp),
	for a package (defined by SamplePackage.u and SamplePackage.dll).
=============================================================================*/

// Includes.
#include "SampleNativePackage.h"

/*-----------------------------------------------------------------------------
	The following must be done once per package (.dll).
-----------------------------------------------------------------------------*/

// This is some necessary C++/UnrealScript glue logic.
// If you forget this, you get a VC++ linker errors like:
// SampleClass.obj : error LNK2001: unresolved external symbol "class FName  SAMPLENATIVEPACKAGE_SampleEvent" (?SAMPLENATIVEPACKAGE_SampleEvent@@3VFName@@A)
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) SAMPLENATIVEPACKAGE_API FName SAMPLENATIVEPACKAGE_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "SampleNativePackageClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY
void RegisterNames()
{
	static INT Registered=0;
	if(!Registered++)
	{
		#define NAMES_ONLY
		#define AUTOGENERATE_NAME(name) extern SAMPLENATIVEPACKAGE_API FName SAMPLENATIVEPACKAGE_##name; SAMPLENATIVEPACKAGE_##name=FName(TEXT(#name),FNAME_Intrinsic);
		#define AUTOGENERATE_FUNCTION(cls,idx,name)
		#include "SampleNativePackageClasses.h"
		#undef DECLARE_NAME
		#undef NAMES_ONLY
	}
}

// Package implementation.
IMPLEMENT_PACKAGE(SampleNativePackage);

/*-----------------------------------------------------------------------------
	ASampleClass class implementation.
-----------------------------------------------------------------------------*/

// C++ implementation of "SampleNativeFunction".
void ASampleClass::execSampleNativeFunction( FFrame& Stack, RESULT_DECL )
{
	// Init if not already done.
	RegisterNames();

	// Write a message to the log.
	debugf(TEXT("Entered C++ SampleNativeFunction"));

	// Parse the UnrealScript parameters.
	P_GET_INT(A);     // Parse an integer parameter.
	P_GET_STR(B);     // Parse a string parameter.
	P_GET_VECTOR(C);  // Parse a vector parameter.
	P_FINISH;         // You must call me when finished parsing all parameters.

	// Generate a message as an FString.
	// This accesses both UnrealScript function parameters (like "B"),
	// and UnrealScript member variables (like "MyBool").
	FString Msg = FString::Printf(TEXT("In C++ SampleNativeFunction (i=%i,s=%s,MyBool=%i)"),i,*B,MyBool);

	// Call some UnrealScript functions from C++.
    this->eventBroadcastMessage(Msg,0,NAME_None);
	this->eventSampleEvent(i);
}

// Register ASampleClass.
// If you forget this, you get a VC++ linker error like:
// SampleClass.obj : error LNK2001: unresolved external symbol "private: static class UClass  ASampleClass::PrivateStaticClass" (?PrivateStaticClass@ASampleClass@@0VUClass@@A)
IMPLEMENT_CLASS(ASampleClass);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
