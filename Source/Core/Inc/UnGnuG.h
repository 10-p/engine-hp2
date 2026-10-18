/*=============================================================================
	UnGnuG.h: Unreal definitions for Gnu G++. Unfinished. Unsupported.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*----------------------------------------------------------------------------
	Platform compiler definitions.
----------------------------------------------------------------------------*/


#ifdef PLATFORM_WIN32
#define __WIN32__	1
#endif

#ifndef PLATFORM_BIG_ENDIAN
#define __INTEL__	1
#define __INTEL_BYTE_ORDER__ 1
#endif

#include <stdarg.h>
#include <math.h>   // floorf/ceilf for the rounding primitives below
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#ifndef PLATFORM_WIN32
// Not on MinGW: alloca comes from <malloc.h> (below) and the <strings.h> names from <string.h>.
#include <alloca.h>
#endif
#include <string.h>
#ifndef PLATFORM_WIN32
#include <strings.h>
#endif
#include <ctype.h>
#include <malloc.h>
#include <dirent.h>
#include <limits.h>
// Emscripten's musl does not define PATH_MAX through these headers; UnUnix.cpp and the launcher use
// TCHAR[PATH_MAX] buffers.
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#ifdef PLATFORM_WIN32
#include <minwindef.h>
#endif

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
#ifndef PLATFORM_WIN32
// Not on Windows: VOID is <winnt.h>'s own `#define VOID void`, used by the Windows and GL headers included
// after this point (`VOID WINAPI ...`).
#undef VOID
#endif
#undef CDECL

#ifndef _WINDOWS_
	#define HANDLE uintptr_t
	#define HINSTANCE uintptr_t
#endif


// Sizes.
enum {DEFAULT_ALIGNMENT = 8 }; // Default boundary to align memory allocations on.
enum {CACHE_LINE_SIZE   = 32}; // Cache line size.
#define GCC_PACK(n) __attribute__((packed,aligned(n)))
#define GCC_ALIGN(n) __attribute__((aligned(n)))

// Optimization macros

#define DISABLE_OPTIMIZATION _Pragma("GCC push_options") \
	_Pragma("GCC optimize(\"O0\")")
#define ENABLE_OPTIMIZATION  _Pragma("GCC pop_options")


// Function type macros.
#define DLL_IMPORT
#define DLL_EXPORT
#define DLL_EXPORT_CLASS
#define VARARGS
#define CDECL
#ifdef PLATFORM_WIN32
// A real calling convention here: Win32 callback typedefs (LPTHREAD_START_ROUTINE) carry __stdcall. __cdecl
// is a MinGW builtin keyword and stays defined.
#define STDCALL __stdcall
#else
#define STDCALL
#define __cdecl
#endif
#define FORCEINLINE /* Force code to be inline */
#define ZEROARRAY 0 /* Zero-length arrays in structs */


#ifdef UNREAL_STATIC
#undef DLL_IMPORT
#define DLL_IMPORT
#endif

#ifdef PLATFORM_WIN32
#define GCC_HIDDEN
#elif !defined(UNREAL_STATIC)
#define GCC_HIDDEN __attribute__((visibility("hidden")))
#else
#define GCC_HIDDEN
#endif

#define GCC_USED __attribute__((used))

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
#ifdef _DEBUG
	#define COMPILER "Compiled with GCC (Debug)"
#else
	#define COMPILER "Compiled with GCC"
#endif

// Unsigned base types.
typedef uint8_t				BYTE;		// 8-bit  unsigned.
typedef uint16_t			_WORD;		// 16-bit unsigned.
#ifndef PLATFORM_WIN32
// Not on Windows: <minwindef.h> declares DWORD (unsigned long) and SIZE_T, and a second typedef conflicts.
typedef uint32_t 			DWORD;		// 32-bit unsigned.
#endif
typedef uint64_t 			QWORD;		// 64-bit unsigned.

// Signed base types.
typedef	int8_t				SBYTE;		// 8-bit  signed.
typedef int16_t  			SWORD;		// 16-bit signed.
typedef int32_t    			INT;		// 32-bit signed.
typedef int64_t  			SQWORD;		// 64-bit signed.

// Character types.
typedef char			    ANSICHAR;	// An ANSI character.
typedef uint16_t    		 UNICHAR;	// A unicode character.
typedef uint8_t				ANSICHARU;	// An ANSI character.
typedef uint16_t     		UNICHARU;	// A unicode character.

// Other base types.
typedef int32_t				UBOOL;		// Boolean 0 (false) or 1 (true).
typedef float				FLOAT;		// 32-bit IEEE floating point.
typedef double				DOUBLE;		// 64-bit IEEE double.
#ifndef PLATFORM_WIN32
typedef size_t        		SIZE_T;     // Corresponds to C SIZE_T.
#endif

// Bitfield type.
#ifdef PLATFORM_WIN32
// The same type as DWORD (unsigned long), not just the same width: BITFIELD members serialise through the
// DWORD overload of operator<<.
typedef DWORD				BITFIELD;	// For bitfields.
#else
typedef uint32_t			BITFIELD;	// For bitfields.
#endif


// Make sure characters are unsigned.
static_assert((char)-1 < 0, "char must be signed.");

// Strings.
#define LINE_TERMINATOR TEXT("\n")
#define PATH_SEPARATOR TEXT("/")
#define DLLEXT TEXT(".so")

// No VC++ asm.
#undef ASM
#define ASM 0
#undef ASMLINUX
#define ASMLINUX 0
#undef ASM3DNOW
#define ASM3DNOW 0


// NULL.
#undef NULL
#define NULL 0

// Package implementation.
#ifdef UNREAL_STATIC
	#define IMPLEMENT_PACKAGE_PLATFORM(pkgname) \
		extern "C" {BYTE GCC_USED DLL_EXPORT GLoaded##pkgname;} \
		STATIC_EXPORT( GLoaded##pkgname, GLoaded##pkgname )
#else
	#define IMPLEMENT_PACKAGE_PLATFORM(pkgname) \
		extern "C" {HINSTANCE hInstance;} \
		BYTE GLoaded##pkgname;
#endif
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
// GSecondsPerCycle is a FLOAT and the clock returns FTime (32.32 fixed point, defined in Core.h before
// this header is included).
extern CORE_API FLOAT GSecondsPerCycle;
CORE_API FTime appSecondsSlow();
//
// Round a floating point number to an integer.
// Note that (int+.5) is rounded to (int+1).
//
#define DEFINED_appRound 1
inline INT appRound( FLOAT F )
{
	// Not (INT)F, which truncates toward zero: the MSVC original (UnVcWin32.h, `fld [F]; fistp [I]`)
	// rounds. The software renderer's rasterisers depend on these two primitives being exact.
	return (INT)floorf( F + 0.5f );
}

//
// Converts to integer equal to or less than.
//
#define DEFINED_appFloor 1
inline INT appFloor( FLOAT F )
{
	// A true floor: the MSVC original (`fld [F]; fsub [Half]; fistp [I]`) relies on fistp rounding, and a
	// truncated F-0.5 is off by one. IntY/YFloor come from here, so the scanline boundary must sit at y.0:
	// the `+1.0001` degeneracy guard in FX15/FY15 (UnCamera.cpp) is calibrated for integers.
	return (INT)floorf( F );
}

//
// CPU cycles, related to GSecondsPerCycle.
//
#define DEFINED_appCycles 1
CORE_API DWORD appCycles();
//
// Seconds, arbitrarily based.
//
#define DEFINED_appSeconds 1
CORE_API FTime appSeconds();

//
// Memory copy.
//
#define DEFINED_appMemcpy 1
inline void appMemcpy( void* Dest, const void* Src, INT Count )
{
	memcpy( Dest, Src, Count);
}

//
// Memory zero.
//
#define DEFINED_appMemzero 1
inline void appMemzero( void* Dest, INT Count )
{
	memset( Dest, 0, Count );
}

//
// POSIX equivalents for non-standard functions.
//
#ifndef strupr
inline char* strupr( char* str )
{
	if( str )
	{
		for( char* p = str; *p; ++p )
			*p = (char)toupper( (unsigned char)*p );
	}
	return str;
}
#endif
#ifndef strnicmp
#define strnicmp strncasecmp
#endif
#ifndef stricmp
#define stricmp strcasecmp
#endif

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
}

// Module name
extern ANSICHAR GModule[32];

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/
