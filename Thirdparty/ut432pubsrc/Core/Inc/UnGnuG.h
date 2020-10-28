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
	#define ASMLINUX 1
	#define COMPILER "Compiled with GNU g++ ("__VERSION__")"
#elif __PSX2_EE__
	#define __UNIX__ 1
	#define __LINUX__ 1
	#define __INTEL__ 1
	#define __INTEL_BYTE_ORDER__ 1
	#undef ASM
	#undef ASM3DNOW
	#undef ASMKNI
	#undef ASMLINUX
	#define ASMPSX2 1
	#define COMPILER "Compiled with PSX2-EE g++ ("__VERSION__")"
#else
	#error Unsupported platform.
#endif

// Stack control.
#include <sys/wait.h>
#include <signal.h>
#include <setjmp.h>
class __Context
{
public:
	__Context() { Last = Env; }
	~__Context() { Env = Last; }
	static void StaticInit();
	static jmp_buf Env;

protected:
	static void HandleSignal( int Sig );
	static struct sigaction Act_SIGHUP;
	static struct sigaction Act_SIGQUIT;
	static struct sigaction Act_SIGILL;
	static struct sigaction Act_SIGTRAP;
	static struct sigaction Act_SIGIOT;
	static struct sigaction Act_SIGBUS;
	static struct sigaction Act_SIGFPE;
	static struct sigaction Act_SIGSEGV;
	static struct sigaction Act_SIGTERM;
	jmp_buf Last;
};

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
enum {DEFAULT_ALIGNMENT = 16}; // Default boundary to align memory allocations on.
enum {CACHE_LINE_SIZE   = 32}; // Cache line size.
#define GCC_PACK(n) __attribute__((packed,aligned(n)))
//#define GCC_MOVE_ALIGN(n) __attribute__((aligned(n))) __attribute__((section (".bss")))
#define GCC_ALIGN(n) __attribute__((aligned(n)))
#define GCC_MOVE_ALIGN(n)
//#define GCC_ALIGN(n)

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
#define __cdecl

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

// Unsigned base types.
typedef unsigned char		BYTE;		// 8-bit  unsigned.
typedef unsigned short		_WORD;		// 16-bit unsigned.
typedef unsigned int		DWORD;		// 32-bit unsigned.
typedef unsigned long long	QWORD;		// 64-bit unsigned.
#if __GNUC__
typedef unsigned int		OWORD __attribute__ ((mode (TI)));
#endif

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

// Memory
#define appAlloca(size) alloca((size+7)&~7)

extern CORE_API UBOOL GTimestamp;
extern CORE_API FLOAT GSecondsPerCycle;
CORE_API FTime appSecondsSlow();

//
// Round a floating point number to an integer.
// Note that (int+.5) is rounded to (int+1).
//
#define DEFINED_appRound 1
inline INT appRound( FLOAT f )
{
#if __PSX2_EE__
	register int r;
	__asm__ __volatile__(
	"
		cvt.w.s %1,%1
		mfc1 %0,%1
	"
	:"=r"(r)
	:"$f"(f)
	);
	return r;
#else
	return (INT)(F);
#endif
}

//
// Converts to integer equal to or less than.
//
#define DEFINED_appFloor 1
inline INT appFloor( FLOAT f )
{
#if __PSX2_EE__
	register int r;
	__asm__ __volatile__(
	"
		cvt.w.s %1,%1
		mfc1 %0,%1
	"
	:"=r"(r)
	:"$f"(f)
	);
	return r;
#else
	return (INT)F;
#endif
}

//
// CPU cycles, related to GSecondsPerCycle.
//
#if ASMLINUX
#define DEFINED_appCycles 1
inline DWORD appCycles()
{
	if( GTimestamp )
	{
		DWORD r;
		asm("rdtsc" : "=a" (r) : "d" (r));
		return r;
	}
}
#endif

//
// Seconds, arbitrarily based.
//
#if ASMLINUX
#define DEFINED_appSeconds 1
inline FTime appSeconds()
{
	if( GTimestamp )
	{
		DWORD L,H;
		asm("rdtsc" : "=a" (L), "=d" (H));
		return ((double)L +  4294967296.0 * (double)H) * GSecondsPerCycle;
	}
	else return appSecondsSlow();
}
#endif

//
// Memory copy.
//
#if ASMLINUX
#define DEFINED_appMemcpy 1
inline void appMemcpy( void* Dest, const void* Src, INT Count )
{
	asm volatile("
		pushl %%ebx;
		pushl %%ecx;
		pushl %%esi;
		pushl %%edi;
		mov %%ecx, %%ebx;
		shr $2, %%ecx;
		and $3, %%ebx;
		rep;
		movsl;
		mov %%ebx, %%ecx;
		rep;
		movsb;
		popl %%edi;
		popl %%esi;
		popl %%ecx;
		popl %%ebx;
	"
	:
	: "S" (Src),
	  "D" (Dest),
	  "c" (Count)
	);
}
#endif

//
// Memory zero.
//
#define DEFINED_appMemzero 1
inline void appMemzero( void* Dest, INT Count )
{
	memset( Dest, 0, Count );
}

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

// Module name
extern ANSICHAR GModule[32];

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/
