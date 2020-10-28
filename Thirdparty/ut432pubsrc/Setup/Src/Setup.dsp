# Microsoft Developer Studio Project File - Name="Setup" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=Setup - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Setup.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Setup.mak" CFG="Setup - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Setup - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "Setup - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""$/UT420/Setup/Src", RJGAAAAA"
# PROP Scc_LocalPath "."
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Setup - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\Lib"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /Zp4 /MD /W4 /WX /vd0 /GX /O1 /Ob2 /I "..\..\Core\Inc" /I "..\..\Window\Inc" /I "..\Inc" /I "." /D "NDEBUG" /D "_WINDOWS" /D "UNICODE" /D "_UNICODE" /D "WIN32" /D ThisPackage=Setup /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 ..\..\Window\Lib\Window.lib ..\..\Core\Lib\Core.lib user32.lib kernel32.lib gdi32.lib advapi32.lib shell32.lib ole32.lib /nologo /base:"0x10400000" /subsystem:windows /incremental:yes /machine:I386 /out:"..\..\System\Setup.exe"

!ELSEIF  "$(CFG)" == "Setup - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\Lib"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /Zp4 /MDd /W4 /WX /Gm /vd0 /GX /ZI /Od /I "..\..\Core\Inc" /I "..\..\Window\Inc" /I "..\Inc" /I "." /D "_DEBUG" /D "_WINDOWS" /D "UNICODE" /D "_UNICODE" /D "WIN32" /D ThisPackage=Setup /FD /c
# SUBTRACT CPP /YX /Yc /Yu
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 ..\..\Window\Lib\Window.lib ..\..\Core\Lib\Core.lib user32.lib kernel32.lib gdi32.lib advapi32.lib shell32.lib ole32.lib /nologo /base:"0x10400000" /subsystem:windows /debug /machine:I386 /out:"..\..\System\Setup.exe" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "Setup - Win32 Release"
# Name "Setup - Win32 Debug"
# Begin Group "Src"

# PROP Default_Filter "*.cpp"
# Begin Source File

SOURCE=.\Setup.cpp
# End Source File
# Begin Source File

SOURCE=.\SetupPrivate.h
# End Source File
# Begin Source File

SOURCE=.\USetupDefinition.cpp
# End Source File
# Begin Source File

SOURCE=.\USetupDefinitionWindows.cpp
# End Source File
# End Group
# Begin Group "Inc"

# PROP Default_Filter "*.h"
# Begin Source File

SOURCE=.\Res\resource.h
# End Source File
# Begin Source File

SOURCE=..\Inc\Setup.h
# End Source File
# Begin Source File

SOURCE=..\Inc\USetupDefinitionWindows.h
# End Source File
# End Group
# Begin Group "Res"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Res\idicon_s.ico
# End Source File
# Begin Source File

SOURCE=.\Res\idicon_t.ico
# End Source File
# Begin Source File

SOURCE=.\Res\SetupRes.rc
# End Source File
# Begin Source File

SOURCE=.\Res\Unreal.ico
# End Source File
# End Group
# Begin Group "Classes"

# PROP Default_Filter "*.uc"
# Begin Source File

SOURCE=..\Classes\SetupDefinition.uc
# End Source File
# Begin Source File

SOURCE=..\Classes\SetupGroup.uc
# End Source File
# Begin Source File

SOURCE=..\Classes\SetupLink.uc
# End Source File
# Begin Source File

SOURCE=..\Classes\SetupObject.uc
# End Source File
# Begin Source File

SOURCE=..\Classes\SetupProduct.uc
# End Source File
# End Group
# Begin Group "Int"

# PROP Default_Filter "*.int"
# Begin Source File

SOURCE=..\..\System\Setup.int
# End Source File
# End Group
# Begin Group "SetupPubSrc"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\System\SetupPubSrc.ini
# End Source File
# Begin Source File

SOURCE=..\..\System\SetupPubSrc.int
# End Source File
# End Group
# Begin Group "SetupUnreal"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\System\SetupUnreal.det
# End Source File
# Begin Source File

SOURCE=..\..\System\SetupUnreal.est
# End Source File
# Begin Source File

SOURCE=..\..\System\SetupUnreal.frt
# End Source File
# Begin Source File

SOURCE=..\..\System\SetupUnreal.ini
# End Source File
# Begin Source File

SOURCE=..\..\System\SetupUnreal.int
# End Source File
# Begin Source File

SOURCE=..\..\System\SetupUnreal.itt
# End Source File
# End Group
# Begin Group "SetupUnrealTournament"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\System\SetupUnrealTournament.est
# End Source File
# Begin Source File

SOURCE=..\..\System\SetupUnrealTournament.frt
# End Source File
# Begin Source File

SOURCE=..\..\System\SetupUnrealTournament.ini
# End Source File
# Begin Source File

SOURCE=..\..\System\SetupUnrealTournament.int
# End Source File
# Begin Source File

SOURCE=..\..\System\SetupUnrealTournament.itt
# End Source File
# End Group
# Begin Group "SetupUnrealPatch"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\System\SetupUnrealPatch.ini
# End Source File
# Begin Source File

SOURCE=..\..\System\SetupUnrealPatch.int
# End Source File
# End Group
# Begin Group "SetupFusion"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\System\SetupFusion.ini
# End Source File
# Begin Source File

SOURCE=..\..\System\SetupFusion.int
# End Source File
# End Group
# Begin Group "SetupUTCD2"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\System\SetupUTCD2.ini
# End Source File
# Begin Source File

SOURCE=..\..\System\SetupUTCD2.int
# End Source File
# End Group
# Begin Group "SetupUTPSX2"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\System\SetupUTPSX2.ini
# End Source File
# Begin Source File

SOURCE=..\..\System\SetupUTPSX2.int
# End Source File
# End Group
# End Target
# End Project
