/*=============================================================================
	UnCon.h: UConsole game-specific definition
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Contains routines for: Messages, menus, status bar
=============================================================================*/

/*------------------------------------------------------------------------------
	UConsole definition.
------------------------------------------------------------------------------*/

//
// Viewport console.
//
class ENGINE_API UConsole : public UObject, public FOutputDevice
{
	DECLARE_CLASS(UConsole,UObject,CLASS_Transient)

	// Constructor.
	UConsole();

	// UConsole interface.
	virtual void _Init( UViewport* Viewport );
	virtual void PreRender( FSceneNode* Frame );
	virtual void PostRender( FSceneNode* Frame );
	virtual void Serialize( const TCHAR* Data, EName MsgType );
	virtual UBOOL GetDrawWorld();

	// Natives.
	DECLARE_FUNCTION(execConsoleCommand);

	// Script events.
    void eventMessage(class APlayerReplicationInfo* PRI, const FString& S, class AZoneInfo* PZone, FName Name)
    {
        struct {class APlayerReplicationInfo* PRI; FString S; class AZoneInfo* PZone; FName N;} Parms;
		Parms.PRI=PRI;
        Parms.S=S;
		Parms.PZone=PZone;
		Parms.N=Name;
        ProcessEvent(FindFunctionChecked(NAME_Message),&Parms);
    }
    void eventTick(FLOAT DeltaTime)
    {
        struct {FLOAT DeltaTime; } Parms;
        Parms.DeltaTime=DeltaTime;
        ProcessEvent(FindFunctionChecked(ENGINE_Tick),&Parms);
    }
    void eventPostRender(class UCanvas* C)
    {
        struct {class UCanvas* C; } Parms;
        Parms.C=C;
        ProcessEvent(FindFunctionChecked(ENGINE_PostRender),&Parms);
    }
    void eventPreRender(class UCanvas* C)
    {
        struct {class UCanvas* C; } Parms;
        Parms.C=C;
        ProcessEvent(FindFunctionChecked(ENGINE_PreRender),&Parms);
    }
    DWORD eventKeyType(BYTE Key)
    {
        struct {BYTE Key; DWORD ReturnValue; } Parms;
        Parms.Key=Key;
        Parms.ReturnValue=0;
        ProcessEvent(FindFunctionChecked(NAME_KeyType),&Parms);
        return Parms.ReturnValue;
    }
    DWORD eventKeyEvent(BYTE Key, BYTE Action, FLOAT Delta)
    {
        struct {BYTE Key; BYTE Action; FLOAT Delta; DWORD ReturnValue; } Parms;
        Parms.Key=Key;
        Parms.Action=Action;
        Parms.Delta=Delta;
        Parms.ReturnValue=0;
        ProcessEvent(FindFunctionChecked(NAME_KeyEvent),&Parms);
        return Parms.ReturnValue;
    }
    void eventNotifyLevelChange()
    {
        ProcessEvent(FindFunctionChecked(NAME_NotifyLevelChange),NULL);
    }
private:
	// Constants.
	enum {MAX_BORDER     = 6};
	enum {MAX_LINES		 = 64};
	enum {MAX_HISTORY	 = 16};

	// Variables.
	UViewport*		Viewport;
	INT				HistoryTop;
	INT				HistoryBot;
	INT				HistoryCur;
	FStringNoInit	TypedStr;
	FStringNoInit	History[MAX_HISTORY];
	INT				Scrollback;
	INT				NumLines;
	INT				TopLine;
	INT				TextLines;
	FLOAT			MsgTime;
	FStringNoInit	MsgText[MAX_LINES];
	FName			MsgType[MAX_LINES];
	class APlayerReplicationInfo* MsgPlayer[MAX_LINES];
	FLOAT			MsgTick[MAX_LINES];
	INT				BorderSize;
	INT				ConsoleLines;
	INT				BorderLines;
	INT				BorderPixels;
	FLOAT			ConsolePos;
	FLOAT			ConsoleDest;
	FLOAT			FrameX;
	FLOAT			FrameY;
	UTexture*		ConBackground;
	UTexture*		Border;
    BITFIELD		bNoStuff:1;
	BITFIELD		bTyping:1;
	BITFIELD		bTimeDemo:1;
	AInfo*			TimeDemo;
	BITFIELD		bNoDrawWorld:1;
    FStringNoInit	LoadingMessage;
    FStringNoInit	SavingMessage;
    FStringNoInit	ConnectingMessage;
    FStringNoInit	PausedMessage;
    FStringNoInit	PrecachingMessage;
};

/*------------------------------------------------------------------------------
	The End.
------------------------------------------------------------------------------*/
