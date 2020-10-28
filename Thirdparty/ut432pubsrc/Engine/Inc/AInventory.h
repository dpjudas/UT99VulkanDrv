/*=============================================================================
	AInventory.h.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

	// Constructors.
	AInventory() {}

	// AActor interface.
	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, INT NumReps );
	virtual UBOOL ShouldDoScriptReplication();
	virtual UBOOL NoVariablesToReplicate(AActor *OldVer);
	virtual UBOOL CheckRecentChanges() {return 1;};
	virtual FLOAT UpdateFrequency(AActor *Viewer, FVector &ViewDir, FVector &ViewPos);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
