/*=============================================================================
	UnGnuG.h: Unreal definitions for Gnu G++. Unfinished. Unsupported.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*----------------------------------------------------------------------------
	Platform compiler definitions.
----------------------------------------------------------------------------*/

#ifdef __LINUX_X86__
	#define __UNIX__  1
	#define __LINUX__ 1
	#define __INTEL__ 1
	#define __INTEL_BYTE_ORDER__ 1
	#undef ASM
	#undef ASM3DNOW
	#undef ASMKNI
#else
	#error Unsupported platform.
#endif

/*----------------------------------------------------------------------------
	Signal handling.
----------------------------------------------------------------------------*/

#include <signal.h>

/*----------------------------------------------------------------------------
	Platform specifics types and defines.
----------------------------------------------------------------------------*/

// Undo any Windows defines.
#undef BYTE
#undef WORD
#undef DWORD
#undef INT
#undef FLOAT
#undef MAXBYTE
#undef MAXWORD
#undef MAXDWORD
#undef MAXINT
#undef VOID
#undef CDECL

// Make sure HANDLE is defined.
#define HANDLE DWORD
#define HINSTANCE DWORD

// Sizes.
enum {DEFAULT_ALIGNMENT = 8 }; // Default boundary to align memory allocations on.
enum {CACHE_LINE_SIZE   = 32}; // Cache line size.
#define GCC_PACK(n) __attribute__((packed,aligned(n)))

// Optimization macros
#define DISABLE_OPTIMIZATION
#define ENABLE_OPTIMIZATION

// Function type macros.
#define DLL_IMPORT
#define DLL_EXPORT			extern "C"
#define DLL_EXPORT_CLASS
#define VARARGS
#define CDECL
#define STDCALL
#define FORCEINLINE /* Force code to be inline */
#define ZEROARRAY 0 /* Zero-length arrays in structs */

// Variable arguments.
#define GET_VARARGS(msg,len,fmt)	\
{	\
	va_list ArgPtr;	\
	va_start( ArgPtr, fmt );	\
	vsprintf( msg, fmt, ArgPtr );	\
	va_end( ArgPtr );	\
}

#define GET_VARARGS_RESULT(msg,len,fmt,result)	\
{	\
	va_list ArgPtr;	\
	va_start( ArgPtr, fmt );	\
	result = vsprintf( msg, fmt, ArgPtr );	\
	va_end( ArgPtr );	\
}

// Compiler name.
#define COMPILER "Compiled with GNU g++ ("__VERSION__")"

// Unsigned base types.
typedef unsigned char		BYTE;		// 8-bit  unsigned.
typedef unsigned short		_WORD;		// 16-bit unsigned.
typedef unsigned int		DWORD;		// 32-bit unsigned.
typedef unsigned long long	QWORD;		// 64-bit unsigned.

// Signed base types.
typedef	signed char			SBYTE;		// 8-bit  signed.
typedef signed short		SWORD;		// 16-bit signed.
typedef signed int  		INT;		// 32-bit signed.
typedef signed long long	SQWORD;		// 64-bit signed.

// Character types.
typedef char			    ANSICHAR;	// An ANSI character.
typedef unsigned short      UNICHAR;	// A unicode character.
typedef unsigned char		ANSICHARU;	// An ANSI character.
typedef unsigned short      UNICHARU;	// A unicode character.

// Other base types.
typedef signed int			UBOOL;		// Boolean 0 (false) or 1 (true).
typedef float				FLOAT;		// 32-bit IEEE floating point.
typedef double				DOUBLE;		// 64-bit IEEE double.
typedef unsigned int        SIZE_T;     // Corresponds to C SIZE_T.

// Bitfield type.
typedef unsigned int		BITFIELD;	// For bitfields.

typedef unsigned int size_t;

// Make sure characters are unsigned.
#ifdef __CHAR_UNSIGNED__
	#error "Bad compiler option: Characters must be signed"
#endif

// Strings.
#if __UNIX__
#define LINE_TERMINATOR TEXT("\n")
#define PATH_SEPARATOR TEXT("/")
#define DLLEXT TEXT(".so")
#else
#define LINE_TERMINATOR TEXT("\r\n")
#define PATH_SEPARATOR TEXT("\\")
#define DLLEXT TEXT(".dll")
#endif

// NULL.
#undef NULL
#define NULL 0

// Package implementation.
#define IMPLEMENT_PACKAGE_PLATFORM(pkgname) \
	extern "C" {HINSTANCE hInstance;} \
	BYTE GLoaded##pkgname;

// Platform support options.
#define PLATFORM_NEEDS_ARRAY_NEW 1
#define FORCE_ANSI_LOG           0

// OS unicode function calling.
#define TCHAR_CALL_OS(funcW,funcA) (funcA)
#define TCHAR_TO_ANSI(str) str
#define ANSI_TO_TCHAR(str) str

// !! Fixme: This is a workaround.
#define GCC_OPT_INLINE

/*----------------------------------------------------------------------------
	Globals.
----------------------------------------------------------------------------*/

// System identification.
extern "C"
{
	extern HINSTANCE      hInstance;
	extern CORE_API UBOOL GIsMMX;
	extern CORE_API UBOOL GIsPentiumPro;
	extern CORE_API UBOOL GIsKatmai;
	extern CORE_API UBOOL GIsK6;
	extern CORE_API UBOOL GIs3DNow;
	extern CORE_API UBOOL GTimestamp;
}

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/
