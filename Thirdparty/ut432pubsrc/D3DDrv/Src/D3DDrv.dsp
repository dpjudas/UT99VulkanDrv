# Microsoft Developer Studio Project File - Name="D3DDrv" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=D3DDrv - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "D3DDrv.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "D3DDrv.mak" CFG="D3DDrv - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "D3DDrv - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "D3DDrv - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""$/UT420/D3DDrv/Src", WDHAAAAA"
# PROP Scc_LocalPath ".."
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "D3DDrv - Win32 Release"

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
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "D3DDRV_EXPORTS" /YX /FD /c
# ADD CPP /nologo /Zp4 /MD /W4 /vd0 /GX /Ox /Ot /Oa /Og /Oi /I "..\..\Core\Inc" /I "..\..\Engine\Inc" /I "..\..\DirectX7\Inc" /D "_WINDOWS" /D "NDEBUG" /D "UNICODE" /D "_UNICODE" /D "WIN32" /D ThisPackage=D3DDrv /Yu"D3DDrv.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib advapi32.lib ole32.lib ..\..\DirectX7\Lib\DxGuid.lib ..\..\DirectX7\Lib\d3dim.lib ..\..\Engine\Lib\Engine.lib ..\..\Core\Lib\Core.lib /nologo /dll /incremental:yes /machine:I386 /out:"..\..\System\D3DDrv.dll"

!ELSEIF  "$(CFG)" == "D3DDrv - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "D3DDRV_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /Zp4 /MTd /W4 /Gm /vd0 /GX /ZI /Od /I "..\..\Core\Inc" /I "..\..\Engine\Inc" /I "..\..\DirectX7\Inc" /D "_WINDOWS" /D "NDEBUG" /D "UNICODE" /D "_UNICODE" /D "WIN32" /D ThisPackage=D3DDrv /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib advapi32.lib ole32.lib ..\..\DirectX7\Lib\DxGuid.lib ..\..\DirectX7\Lib\d3dim.lib ..\..\Engine\Lib\Engine.lib ..\..\Core\Lib\Core.lib /nologo /dll /debug /machine:I386 /out:"..\..\System\D3DDrv.dll" /pdbtype:sept /libpath:"..\core\lib" /libpath:"..\engine\lib"

!ENDIF 

# Begin Target

# Name "D3DDrv - Win32 Release"
# Name "D3DDrv - Win32 Debug"
# Begin Source File

SOURCE=.\D3DDrv.cpp

!IF  "$(CFG)" == "D3DDrv - Win32 Release"

# ADD CPP /Yc"D3DDrv.h"

!ELSEIF  "$(CFG)" == "D3DDrv - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\D3DDrv.h
# End Source File
# Begin Source File

SOURCE=.\Direct3D7.cpp
# End Source File
# End Target
# End Project
