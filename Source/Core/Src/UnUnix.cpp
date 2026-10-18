/*=============================================================================
	UnIX: Unix port of UnVcWin32.cpp.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Cloned by Mike Danylchuk
		* Severely amputated and mutilated by Brandon Reinhart
		* Surgically altered by Jack Porter
		* Mangled and rehabilitated by Brandon Reinhart
=============================================================================*/
#if __GNUG__


#if defined(PLATFORM_SDL)
#include <SDL2/SDL.h>
#elif !defined(PLATFORM_POSIX)
#error "Unsupported platform."
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>  // EM_ASM for the return-to-launcher reload in appRequestExit
#endif

// Standard includes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <float.h>
#include <time.h>
#include <signal.h>

// System includes.
#include <unistd.h>
#include <dirent.h>
#include <utime.h>
#include <sys/time.h>
#include <sys/stat.h>
#ifndef UNREAL_STATIC
#include <dlfcn.h>
#endif
#ifdef PLATFORM_WIN32
// MinGW: name resolution is WinSock; UnMinGW.cpp holds the calls with no POSIX equivalent.
#include <winsock2.h>
#include <windows.h>
#include <process.h>	// _spawnl / _spawnv
#else
#include <netdb.h>
#endif
#if !defined(PLATFORM_WIN32) && !defined(__EMSCRIPTEN__)
#include <sys/wait.h>	// waitpid, for appGetProcReturnCode
#endif

// Core includes.
#include "CorePrivate.h"
#include "UnUnix.h"

// A POSIX build without SDL takes its clock, CPU count and base directory from the system itself.
#if defined(PLATFORM_POSIX) && !defined(PLATFORM_SDL)
#define UE_POSIX_WITHOUT_SDL 1
static QWORD PosixMonotonicNs()
{
	timespec Now;
	clock_gettime( CLOCK_MONOTONIC, &Now );
	return (QWORD)Now.tv_sec * 1000000000ull + (QWORD)Now.tv_nsec;
}
#endif
/*-----------------------------------------------------------------------------
	Globals
-----------------------------------------------------------------------------*/

// Module
ANSICHAR GModule[32];

// Environment
extern char **environ;

/*-----------------------------------------------------------------------------
	USystem.
-----------------------------------------------------------------------------*/

//
// System manager.
//
static void Recurse()
{
	guard(Recurse);
	Recurse();
	unguard;
}
USystem::USystem()
:	SavePath	( E_NoInit )
,	CachePath	( E_NoInit )
,	CacheExt	( E_NoInit )
,	Paths		( E_NoInit )
,	Suppress	( E_NoInit )
{}
void USystem::StaticConstructor()
{
	guard(USystem::StaticConstructor);

	new(GetClass(),TEXT("PurgeCacheDays"),      RF_Public)UIntProperty   (CPP_PROPERTY(PurgeCacheDays    ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("SavePath"),            RF_Public)UStrProperty   (CPP_PROPERTY(SavePath          ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("CachePath"),           RF_Public)UStrProperty   (CPP_PROPERTY(CachePath         ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("CacheExt"),            RF_Public)UStrProperty   (CPP_PROPERTY(CacheExt          ), TEXT("Options"), CPF_Config );

	UArrayProperty* A = new(GetClass(),TEXT("Paths"),RF_Public)UArrayProperty( CPP_PROPERTY(Paths), TEXT("Options"), CPF_Config );
	A->Inner = new(A,TEXT("StrProperty0"),RF_Public)UStrProperty;

	UArrayProperty* B = new(GetClass(),TEXT("Suppress"),RF_Public)UArrayProperty( CPP_PROPERTY(Suppress), TEXT("Options"), CPF_Config );
	B->Inner = new(B,TEXT("NameProperty0"),RF_Public)UNameProperty;

	unguard;
}
UBOOL USystem::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if( ParseCommand(&Cmd,TEXT("MEMSTAT")) )
	{
		//!UNIX No MEMSTAT command.
		Ar.Logf( TEXT("MEMSTAT command not available.") );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("EXIT")) )
	{
		Ar.Log( TEXT("Closing by request") );
		appRequestExit( 0 );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("APP")) )
	{
		//!UNIX No APP command.
		Ar.Logf( TEXT("APP command not available.") );
		return 1;
	}
	else if( ParseCommand( &Cmd, TEXT("RELAUNCH") ) )
	{
		debugf( TEXT("Relaunch: %s"), Cmd );
		GConfig->Flush( 0 );

		// Fork out a new process using Cmd as arguments.
		TCHAR* EndArg0 = appStrstr( Cmd, " " );
		INT CmdLen = appStrlen( Cmd );
		INT RestLen = appStrlen( EndArg0 );
		TCHAR Arg0[256];
		appStrncpy( Arg0, Cmd, (CmdLen - RestLen) + 1 );

#ifdef PLATFORM_WIN32
		// No fork() on Windows: _spawnl(_P_NOWAIT) starts the new process, then this one exits.
		if( _spawnl( _P_NOWAIT, "./ucc.exe", "ucc", "server", Arg0, (char*)NULL ) == -1 )
			appErrorf( TEXT("Failed to launch process.") );
		appRequestExit( 0 );
#else
		pid_t pid = fork();
		if (pid != 0)
		{
			appSleep(3);
			INT error = execl("./ucc", "ucc", "server", Arg0, (char*)NULL );
			if (error == -1)
				appErrorf( TEXT("Failed to launch process.") );
		} else {
			appRequestExit( 0 );
		}
#endif
		return 1;
	}
	else if( ParseCommand( &Cmd, TEXT("DEBUG") ) )
	{
		if( ParseCommand(&Cmd,TEXT("CRASH")) )
		{
			appErrorf( TEXT("%s"), TEXT("Unreal crashed at your request") );
			return 1;
		}
		else if( ParseCommand( &Cmd, TEXT("GPF") ) )
		{
			Ar.Log( TEXT("Unreal crashing with voluntary GPF") );
			*(int *)NULL = 123;
			return 1;
		}
		else if( ParseCommand( &Cmd, TEXT("RECURSE") ) )
		{
			Ar.Logf( TEXT("Recursing") );
			Recurse();
			return 1;
		}
		else if( ParseCommand( &Cmd, TEXT("EATMEM") ) )
		{
			Ar.Log( TEXT("Eating up all available memory") );
			while( 1 )
			{
				void* Eat = appMalloc(65536,TEXT("EatMem"));
				memset( Eat, 0, 65536 );
			}
			return 1;
		}
		else return 0;
	}
	else return 0;
}
IMPLEMENT_CLASS(USystem);

/*-----------------------------------------------------------------------------
	Exit.
-----------------------------------------------------------------------------*/

//
// Immediate exit.
//
CORE_API void appRequestExit( UBOOL Force )
{
	guard(appForceExit);
	debugf( TEXT("appRequestExit(%i)"), Force );
	if( Force )
	{
		// Force immediate exit. Dangerous because config code isn't flushed, etc.
		exit( 1 );
	}
	else
	{
		// Tell the platform specific code we want to exit cleanly from the main loop.
		//!UNIX No quit message in UNIX.
		GIsRequestingExit = 1;
#ifdef __EMSCRIPTEN__
		// A normal exit returns the page to the launcher: navigate to location.pathname, which drops any
		// autostart query, after the Asyncify main loop unwinds (setTimeout 0). Only the browser main thread
		// can navigate, hence MAIN_THREAD_EM_ASM under pthreads.
#ifdef __EMSCRIPTEN_PTHREADS__
		MAIN_THREAD_EM_ASM({
#else
		EM_ASM({
#endif
			if( typeof window !== 'undefined' )
			{
				console.log('[SHELL] Engine exit requested, returning to launcher');
				setTimeout(function(){ window.location.href = window.location.pathname; }, 0);
			}
		});
#endif
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	Clipboard.
-----------------------------------------------------------------------------*/

CORE_API void ClipboardCopy( const TCHAR* Str )
{
	guard(ClipboardCopy);
	//!UNIX Not supported in UNIX.
	unguard;
}

CORE_API void ClipboardPaste( FString& Result )
{
	guard(ClipboardPasteString);
	//!UNIX Not supported in UNIX.
	unguard;
}

/*-----------------------------------------------------------------------------
	Shared libraries.
-----------------------------------------------------------------------------*/
#ifdef UNREAL_STATIC

//
// Load a library.
//
CORE_API void* appGetDllHandle( const char* Filename )
{
	guard(appGetDllHandle);

	char Test[1024];
	const char* PackageName = Filename;
	char* Cur;

	check(Filename);

	// Get GLoadedPackage symbol name from full path.
	while( ( Cur = appStrchr( PackageName, '/' ) ) != NULL )
		PackageName = Cur + 1;
	while( ( Cur = appStrchr( PackageName, '\\' ) ) != NULL )
		PackageName = Cur + 1;
	appSprintf( Test, "GLoaded%s", PackageName );
	if( (Cur = appStrchr( Test, '.' )) != NULL )
		*Cur = '\0';

	return appGetStaticExport( Test );

	unguard;
}

#else
void* appGetDllHandle( const TCHAR* Filename )
{
	guard(appGetDllHandle);
	check(Filename);
	const TCHAR* PackageName = Filename;
	TCHAR* Cur;
	TCHAR* Test = appStaticString1024();
	void* Result;
	TCHAR* Error;

	// Get GLoadedPackage symbol name from full path.
 	while( (Cur = appStrchr( PackageName, '/' )) != NULL )
		PackageName = Cur + 1;
 	while( (Cur = appStrchr( PackageName, '\\' )) != NULL )
		PackageName = Cur + 1;
	appSprintf( Test, "GLoaded%s", PackageName );
	if( (Cur = appStrchr( Test, '.' )) != NULL )
		*Cur = '\0';

	dlerror();	// Clear any error condition.

	// Check if the library was linked to the executable.
	Result = (void*)dlopen( NULL, RTLD_NOW );
	Error = dlerror();
	if( Error != NULL )
		debugf( "dlerror(): %s", Error );
	else
	{
		(void*)dlsym( Result, appToAnsi(Test) );
		Error = dlerror();
		if( Error == NULL )
			return Result;
	}

	// Load the new library.
	Result = (void*)dlopen( Filename, RTLD_NOW );
	if( Result == NULL )
	{
		TCHAR Temp[256];
		appStrcpy( Temp, Filename );
		appStrcat( Temp, DLLEXT );
		Result = (void*)dlopen( Temp, RTLD_NOW );
	}

	return Result;
	unguard;
}
#endif
//
// Free a library.
//
void appFreeDllHandle( void* DllHandle )
{
	guard(appFreeDllHandle);
	check(DllHandle);

#if defined(UNREAL_STATIC)
	// nothing
#elif defined(PLATFORM_WIN32)
	FreeLibrary( (HMODULE)DllHandle );
#else
	dlclose( DllHandle );
#endif
	unguard;
}

//
// Lookup the address of a shared library function.
//
void* appGetDllExport( void* DllHandle, const TCHAR* ProcName )
{
	guard(appGetDllExport);
	check(DllHandle);
	check(ProcName);

	void* Result;
	TCHAR* Error;
#if defined(UNREAL_STATIC)
	return appGetStaticExport( ProcName );
#else
	dlerror();	// Clear any error condition.
	Result = (void*)dlsym( DllHandle, appToAnsi(ProcName) );
	Error = dlerror();
	if( Error != NULL )
		debugf( "dlerror: %s", Error );
	return Result;
#endif
	unguard;
}

//
// Break the debugger.
//
#ifndef DEFINED_appDebugBreak
void appDebugBreak()
{
	guard(appDebugBreak);
	//!UNIX Not implemented in UNIX.
	unguard;
}
#endif

/*-----------------------------------------------------------------------------
	External processes.
-----------------------------------------------------------------------------*/

// Returns a handle the caller polls with appGetProcReturnCode (AStatLog, the editor-only XA encoder in
// UnAudio.cpp).
void* appCreateProc( const TCHAR* URL, const TCHAR* Parms, UBOOL bRealTime )
{
	guard(appCreateProc);

	debugf( TEXT("Create Proc: %s %s"), URL, Parms );

#if defined(__EMSCRIPTEN__)
	// No processes in the browser; appGetProcReturnCode(NULL) reports "finished, failed".
	return NULL;
#else
	TCHAR LocalParms[PATH_MAX] = TEXT("");
	appStrncpy( LocalParms, Parms, ARRAY_COUNT(LocalParms) );

	char* argv[128];
	argv[0] = (char*) URL;
	argv[1] = (char*) LocalParms;
	INT j = 2;
	INT k = appStrlen(LocalParms) - 1;
	for( INT i=0; i<k && j<ARRAY_COUNT(argv)-1; i++ )
	{
		if (LocalParms[i] == ' ')
		{
			LocalParms[i] = 0;
			argv[j++] = LocalParms + i + 1;
		}
	}
	argv[j] = 0;

#ifdef PLATFORM_WIN32
	// _spawnv(_P_NOWAIT) returns the process handle.
	intptr_t Handle = _spawnv( _P_NOWAIT, URL, argv );
	return Handle==-1 ? NULL : (void*)Handle;
#else
	pid_t pid = fork();
	if( pid == 0 )
	{
		execv( URL, argv );
		_exit( 127 );
	}
	return pid>0 ? (void*)(intptr_t)pid : NULL;
#endif
#endif
	unguard;
}

// Returns 1 once the process has finished (exit code in *ReturnCode), 0 while it still runs. A NULL handle
// (no process started) is reported as finished with -1 so polling loops terminate.
UBOOL appGetProcReturnCode( void* ProcHandle, INT* ReturnCode )
{
	guard(appGetProcReturnCode);
	if( !ProcHandle )
	{
		*ReturnCode = -1;
		return 1;
	}
#if defined(__EMSCRIPTEN__)
	*ReturnCode = -1;
	return 1;
#elif defined(PLATFORM_WIN32)
	DWORD Code = 0;
	if( !GetExitCodeProcess( (HANDLE)ProcHandle, &Code ) )
	{
		*ReturnCode = -1;
		return 1;
	}
	if( Code == STILL_ACTIVE )
		return 0;
	CloseHandle( (HANDLE)ProcHandle );
	*ReturnCode = (INT)Code;
	return 1;
#else
	int Status = 0;
	pid_t Result = waitpid( (pid_t)(intptr_t)ProcHandle, &Status, WNOHANG );
	if( Result == 0 )
		return 0;
	*ReturnCode = (Result>0 && WIFEXITED(Status)) ? WEXITSTATUS(Status) : -1;
	return 1;
#endif
	unguard;
}

/*-----------------------------------------------------------------------------
	Timing.
-----------------------------------------------------------------------------*/

//
// String timestamp.
//
CORE_API const TCHAR* appTimestamp()
{
	guard(appTimestamp);

	TCHAR* 		Result = appStaticString1024();
	time_t		CurTime;
	struct tm*	SysTime;

	CurTime = time( NULL );
	SysTime = localtime( &CurTime );
	appSprintf( Result, asctime( SysTime ) );
	// Strip newline.
	Result[appStrlen( Result )-1] = '\0';
	return Result;

	unguard;
}

//
// Get file time.
//
CORE_API DWORD appGetTime( const TCHAR* Filename )
{
	guard(appGetTime);

	struct utimbuf FileTime;
	if( utime(Filename,&FileTime)!=0 )
		return 0;
	//FIXME Is this the right "time" to return?
	return (DWORD)FileTime.modtime;

	unguard;
}

//
// Get time in seconds.
//
CORE_API FTime appSecondsSlow()
{
	static DOUBLE InitialTime = 0.0;
	static DOUBLE TimeCounter = 0.0;
	DOUBLE NewTime;
	struct timeval TimeOfDay;

	gettimeofday( &TimeOfDay, NULL );

	// Initialize.
	if( InitialTime == 0.0 )
		 InitialTime = TimeOfDay.tv_sec + TimeOfDay.tv_usec / 1000000.0;

	// Accumulate difference to prevent wraparound.
	NewTime = TimeOfDay.tv_sec + TimeOfDay.tv_usec / 1000000.0;
	TimeCounter += NewTime - InitialTime;
	InitialTime = NewTime;

	return TimeCounter;
}


//
// Return the system time.
//
CORE_API void appSystemTime( INT& Year, INT& Month, INT& DayOfWeek, INT& Day, INT& Hour, INT& Min, INT& Sec, INT& MSec )
{
	guard(appSystemTime);

	time_t			CurTime;
	struct tm		*St;		// System time.
	struct timeval	Tv;			// Use timeval to get milliseconds.

	gettimeofday( &Tv, NULL );
	CurTime = time( NULL );
	St = localtime( &CurTime );
	
	Year		= St->tm_year + 1900;
	Month		= St->tm_mon + 1;
	DayOfWeek	= St->tm_wday;
	Day			= St->tm_mday;
	Hour		= St->tm_hour;
	Min			= St->tm_min;
	Sec			= St->tm_sec;
	MSec		= (INT) (Tv.tv_usec / 1000);

	unguard;
}

CORE_API void appSleep( FLOAT Seconds )
{
	guard(appSleep);

	INT SleepTime = (INT) (Seconds * 1000000);
	usleep( SleepTime );

	unguard;
}

/*-----------------------------------------------------------------------------
	Link functions.
-----------------------------------------------------------------------------*/

//
// Launch a uniform resource locator (i.e. http://www.epicgames.com/unreal).
// This is expected to return immediately as the URL is launched by another
// task.
//
void appLaunchURL( const TCHAR* URL, const TCHAR* Parms, FString* Error )
{
	guard(appLaunchURL);
	//!UNIX Server doesn't need to launch URLs.
	unguard;
}

/*-----------------------------------------------------------------------------
	File finding.
-----------------------------------------------------------------------------*/

//
// Clean out the file cache.
//
static INT GetFileAgeDays( const TCHAR* Filename )
{
	guard(GetFileAgeDays);
	struct stat Buf;
	INT Result = 0;

	Result = stat(TCHAR_TO_ANSI(Filename),&Buf);
	if( Result==0 )
	{
		time_t CurrentTime, FileTime;
		FileTime = Buf.st_mtime;
		time( &CurrentTime );
		DOUBLE DiffSeconds = difftime( CurrentTime, FileTime );
		return (INT)(DiffSeconds / 60.0 / 60.0 / 24.0);
	}
	return 0;
	unguard;
}

CORE_API void appCleanFileCache()
{
	guard(appCleanFileCache);

	// Delete all temporary files.
	guard(DeleteTemps);
	FString Temp = FString::Printf( TEXT("%s") PATH_SEPARATOR TEXT("*.tmp"), *GSys->CachePath );
	TArray<FString> Found = GFileManager->FindFiles( *Temp, 1, 0 );
	for( INT i=0; i<Found.Num(); i++ )
	{
		Temp = FString::Printf( TEXT("%s") PATH_SEPARATOR TEXT("%s"), *GSys->CachePath, *Found(i) );
		debugf( TEXT("Deleting temporary file: %s"), *Temp );
		GFileManager->Delete( *Temp );
	}
	unguard;

	// Delete cache files that are no longer wanted.
	guard(DeleteExpired);
	TArray<FString> Found = GFileManager->FindFiles( *(GSys->CachePath * TEXT("*") + GSys->CacheExt), 1, 0 );
	if( GSys->PurgeCacheDays )
	{
		for( INT i=0; i<Found.Num(); i++ )
		{
			FString Temp = FString::Printf( TEXT("%s") PATH_SEPARATOR TEXT("%s"), *GSys->CachePath, *Found(i) );
			INT DiffDays = GetFileAgeDays( *Temp );
			if( DiffDays > GSys->PurgeCacheDays )
			{
				debugf( TEXT("Purging outdated file from cache: %s (%i days old)"), *Temp, DiffDays );
				GFileManager->Delete( *Temp );
			}
		}
	}
	unguard;

	unguard;
}

/*-----------------------------------------------------------------------------
	Guids.
-----------------------------------------------------------------------------*/

//
// Create a new globally unique identifier.
//
CORE_API FGuid appCreateGuid()
{
	guard(appCreateGuid);

	FGuid Result;
	appGetGUID( (void*)&Result );
	return Result;

	unguard;
}

/*-----------------------------------------------------------------------------
	Clipboard
-----------------------------------------------------------------------------*/
static FString ClipboardText;
CORE_API void appClipboardCopy( const TCHAR* Str )
{
	guard(appClipboardCopy);
	ClipboardText = FString( Str );
	unguard;
}

CORE_API FString appClipboardPaste()
{
	guard(appClipboardPaste);
	return ClipboardText;
	unguard;
}
/*-----------------------------------------------------------------------------
	Command line.
-----------------------------------------------------------------------------*/

// Get startup directory.
CORE_API const char* appBaseDir()
{
	static char BaseDir[1024]="";

	if( !BaseDir[0] )
	{
		// Get directory this executable was launched from.
#if defined(__EMSCRIPTEN__)
		// SDL_GetBasePath() returns "/" here; the launcher chdirs to /System, so use getcwd().
		if( getcwd( BaseDir, sizeof(BaseDir) ) )
			appStrcat( BaseDir, "/" );
#elif defined(PLATFORM_SDL)
		char* BasePath = SDL_GetBasePath();
		appStrncpy( BaseDir, BasePath, sizeof(BaseDir) );
		SDL_free( BasePath );
#elif defined(UE_POSIX_WITHOUT_SDL)
		// The executable's own directory, as SDL_GetBasePath() finds it on Linux.
		char Exe[1024];
		ssize_t Len = readlink( "/proc/self/exe", Exe, sizeof(Exe)-1 );
		if( Len > 0 )
		{
			Exe[Len] = 0;
			if( char* Slash = strrchr( Exe, '/' ) )
			{
				Slash[1] = 0;
				appStrncpy( BaseDir, Exe, sizeof(BaseDir) );
			}
		}
#endif
		// Fallback to CWD.
		if ( !BaseDir[0] )
			strcpy( BaseDir, "./" );
	}

	return BaseDir;
}
// Get computer name.
CORE_API const TCHAR* appComputerName()
{
	guard(appComputerName);
	static TCHAR Result[256]="";
	if( !Result[0] )
	{
#ifdef PLATFORM_WIN32
		// gethostname is a WinSock call, and this can run before IpDrv loads.
		appWinSockInit();
#endif
		gethostname( Result, sizeof(Result) );
	}
	return Result;
	unguard;
}

// Get user name.
CORE_API const TCHAR* appUserName()
{
	guard(appUserName);
	static TCHAR Result[256]="";
	if( !Result[0] )
#ifdef PLATFORM_WIN32
	{
		// No getlogin() on Windows. Callers expect a name, so fall back to a constant.
		DWORD Size = ARRAY_COUNT(Result);
		if( !GetUserNameA( Result, &Size ) )
			appStrcpy( Result, TEXT("Player") );
	}
#else
		appStrncpy( Result, getlogin(), sizeof(Result) );
#endif
	return Result;
	unguard;
}

// Get launch package base name.
CORE_API const TCHAR* appPackage()
{
	guard(appPackage);
	return GModule;
	unguard;
}

/*-----------------------------------------------------------------------------
	App init/exit.
-----------------------------------------------------------------------------*/

//
// Platform specific initialization.
//
void appPlatformPreInit()
{
}

/*-----------------------------------------------------------------------------
	Exit signals.
-----------------------------------------------------------------------------*/

// SIGTERM, SIGHUP and SIGINT ask the main loop to exit cleanly. Before the main loop runs there is nothing to
// leave cleanly, so the process ends at once; a second signal while an exit is pending is reported and ignored.
#ifndef __EMSCRIPTEN__
static volatile INT GExitSignals = 0;

static void appWriteSignalText( const char* A, const char* B )
{
#ifdef PLATFORM_WIN32
	fputs( A, stdout ); fputs( B, stdout ); fflush( stdout );
#else
	// Async-signal-safe: the signal can interrupt any thread inside malloc or the log.
	write( 1, A, strlen(A) );
	write( 1, B, strlen(B) );
#endif
}

static UBOOL appHandleExitSignal( const char* Name )
{
	if( !GIsRunning )
	{
		appWriteSignalText( Name, ": before the main loop, exiting now.\n" );
		_exit( 1 );
	}
	if( GExitSignals++ )
	{
		appWriteSignalText( Name, ": exit already requested; this one is ignored.\n" );
		return 1;
	}
	appWriteSignalText( Name, ": requesting exit.\n" );
	GIsRequestingExit = 1;
	return 1;
}

#ifdef PLATFORM_WIN32
// The console's Ctrl-C, Ctrl-Break and close events, on a thread of their own.
static BOOL WINAPI appConsoleCtrlHandler( DWORD CtrlType )
{
	switch( CtrlType )
	{
		case CTRL_C_EVENT:     return appHandleExitSignal( "Signal: Ctrl-C" );
		case CTRL_BREAK_EVENT: return appHandleExitSignal( "Signal: Ctrl-Break" );
		case CTRL_CLOSE_EVENT: return appHandleExitSignal( "Signal: console closed" );
		default:               return 0;
	}
}
#else
static void appExitSignalHandler( int Signal )
{
	appHandleExitSignal( Signal==SIGTERM ? "Signal: SIGTERM" : Signal==SIGHUP ? "Signal: SIGHUP" : "Signal: SIGINT" );
}
#endif
#endif

static void appInstallExitSignalHandlers()
{
#if defined(PLATFORM_WIN32)
	SetConsoleCtrlHandler( appConsoleCtrlHandler, 1 );
#elif !defined(__EMSCRIPTEN__)
	struct sigaction Action;
	memset( &Action, 0, sizeof(Action) );
	Action.sa_handler = appExitSignalHandler;
	sigemptyset( &Action.sa_mask );
	sigaction( SIGTERM, &Action, NULL );
	sigaction( SIGHUP,  &Action, NULL );
	sigaction( SIGINT,  &Action, NULL );
#endif
}

void appPlatformInit()
{
	guard(appPlatformInit);

	// System initialization.
	GSys = new USystem;
	GSys->AddToRoot();
	for( INT i=0; i<GSys->Suppress.Num(); i++ )
		GSys->Suppress(i).SetFlags( RF_Suppress );

	// Exit signals request a clean exit.
	appInstallExitSignalHandlers();

	// Randomize.
	srand( (unsigned)time( NULL ) );

	// CPU speed.
	GTimestamp = 1;
#if defined(PLATFORM_SDL)
	debugf( NAME_Init, "Detected: %s", SDL_GetPlatform() );

	// CPU speed.
	DOUBLE Frequency = SDL_GetPerformanceFrequency();
	check(Frequency!=0.0);
	GSecondsPerCycle = 1.0 / Frequency;
	debugf( NAME_Init, "CPU Timer Freq=%f Hz", (FLOAT)Frequency );

	// Get CPU info.
	GPageSize = 4096; // TODO: sysconf?
	GProcessorCount = SDL_GetCPUCount();
#elif defined(UE_POSIX_WITHOUT_SDL)
	debugf( NAME_Init, "Detected: POSIX" );

	// The monotonic clock counts nanoseconds.
	DOUBLE Frequency = 1000000000.0;
	GSecondsPerCycle = 1.0 / Frequency;
	debugf( NAME_Init, "CPU Timer Freq=%f Hz", (FLOAT)Frequency );

	// Get CPU info.
	GPageSize = sysconf( _SC_PAGESIZE );
	GProcessorCount = sysconf( _SC_NPROCESSORS_ONLN );
#endif // PLATFORM_SDL
	unguard;
}

void appPlatformPreExit()
{
}

void appPlatformExit()
{
}

void appEnableFastMath( UBOOL Enable )
{
	guard(appEnableFastMath);

	unguard;
}

/*-----------------------------------------------------------------------------
	Pathnames.
-----------------------------------------------------------------------------*/

// Convert pathname to Unix format.
char* appUnixPath( const TCHAR* Path )
{
	guard(appUnixPath);
	TCHAR* UnixPath = appStaticString1024();
	TCHAR* Cur = UnixPath;
	appStrncpy( UnixPath, Path, 1024 );
	while( Cur = strchr( Cur, '\\' ) )
		*Cur = '/';
	return UnixPath;
	unguard;
}

/*-----------------------------------------------------------------------------
	Networking.
-----------------------------------------------------------------------------*/

unsigned int appGetLocalIP( void )
{
	static unsigned int LocalIP = 0;

	if( LocalIP==0 )
	{
		struct hostent *Hostinfo;
		char Hostname[256];
#ifdef PLATFORM_WIN32
		appWinSockInit();	// gethostname is a WinSock call
#endif
		gethostname( Hostname, sizeof(Hostname) );
		Hostinfo = gethostbyname( Hostname );
		if( Hostinfo && Hostinfo->h_addr_list[0] && Hostinfo->h_length==sizeof(LocalIP) )
			memcpy( &LocalIP, Hostinfo->h_addr_list[0], sizeof(LocalIP) );	// a 4-byte IPv4 address
	}
	
	return LocalIP;
}

//
// Get time in seconds.
//
// Returns FTime (32.32 fixed point). The counter is rebased on its first reading so FTime::GetFloat()
// (a FLOAT) keeps sub-millisecond precision however long the machine has been up.
static QWORD GStartCounter = 0;
static QWORD appReadCounter()
{
#ifdef PLATFORM_MSVC
	LARGE_INTEGER ret;
	QueryPerformanceCounter(&ret);
	return ret.QuadPart;
#elif defined(PLATFORM_SDL)
	return SDL_GetPerformanceCounter();
#else
	return PosixMonotonicNs();
#endif
}
CORE_API FTime appSeconds()
{
	QWORD Now = appReadCounter();
	if( !GStartCounter )
		GStartCounter = Now - 1;
	// FTime(double) scales by 2^32 into a signed 64-bit value: ~2.1e9 s of range.
	return FTime( (DOUBLE)(Now - GStartCounter) * (DOUBLE)GSecondsPerCycle );
}

CORE_API DWORD appCycles()
{
	return (DWORD)appReadCounter();
}

/*-----------------------------------------------------------------------------
	Platform functions HP2's 433 headers declare that only the Win32 core
	(UnVcWin32.cpp, MSVC-only) defines.
-----------------------------------------------------------------------------*/

//
// Per-user directory prefix for inis, the log and saves. Empty: everything stays relative to the
// working directory (System/). Callers concatenate, so a non-empty value must end with a path separator.
//
CORE_API const TCHAR* appUserDir()
{
	return TEXT("");
}

//
// Process CPU time (UnGame.cpp load-time line, UnRender.cpp stats). Never call clock() in Core:
// UnFile.h #defines clock(Timer) as a stat macro.
//
CORE_API FTime appProcessSeconds()
{
#if defined(__EMSCRIPTEN__)
	// Wall time here.
	return appSeconds();
#elif defined(PLATFORM_WIN32)
	// Kernel plus user time, in 100 ns units.
	FILETIME Creation, Exit, Kernel, User;
	if( !GetProcessTimes( GetCurrentProcess(), &Creation, &Exit, &Kernel, &User ) )
		return appSeconds();
	ULARGE_INTEGER K, U;
	K.LowPart = Kernel.dwLowDateTime; K.HighPart = Kernel.dwHighDateTime;
	U.LowPart = User.dwLowDateTime;   U.HighPart = User.dwHighDateTime;
	return FTime( (DOUBLE)(K.QuadPart + U.QuadPart) * 1e-7 );
#else
	struct timespec ts;
	if( clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &ts ) != 0 )
		return appSeconds();
	return FTime( (DOUBLE)ts.tv_sec + (DOUBLE)ts.tv_nsec * 1e-9 );
#endif
}

//
// Streaming reader. The Win32 core runs vorbisfile/coda on a thread (UnVcWin32.cpp); off MSVC the audio
// driver owns its streaming, so Core only provides link stubs (UnMisc.cpp's DestroyStream calls these).
//
UBOOL FFileStream::Read( INT StreamId, INT Bytes )
{
	return 0;
}
UBOOL FFileStream::Create( INT StreamId, const TCHAR* Filename )
{
	return 0;
}
UBOOL FFileStream::Create( INT StreamId, TLazyArray<BYTE>* RawData, INT NumBytes )
{
	return 0;
}
UBOOL FFileStream::Destroy( INT StreamId )
{
	return 1;
}

//
// Windows version probe used by the 433 launcher/engine paths.
//
CORE_API bool IsOSVer2kOrXP()
{
	return 1;
}

#endif
/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
