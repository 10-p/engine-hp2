/*=============================================================================
	UnIX: Unix port of UnVcWin32.cpp.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Cloned by Mike Danylchuk
		* Severely amputated and mutilated by Brandon Reinhart
		* Surgically altered by Jack Porter
		* Mangled and rehabilitated by Brandon Reinhart
		* Obfuscated by Daniel Vogel
=============================================================================*/
#if __GNUG__

// Standard includes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <float.h>
#include <time.h>

// System includes.
#include <unistd.h>
#include <dirent.h>
#include <utime.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <netdb.h>

// Core includes.
#include "CorePrivate.h"

/*-----------------------------------------------------------------------------
	Globals
-----------------------------------------------------------------------------*/

// Module
ANSICHAR GModule[32];

// Environment
extern char **environ;

// Signal
static int SignalExit = 0;
static int SignalCritical = 0;
static bool AlreadyAborting = false;

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
		
		pid_t pid = fork();
		if (pid != 0)
		{
			appSleep(3);
			INT error = execl("./ucc", "ucc", "server", Arg0, NULL );
			if (error == -1)
				appErrorf( TEXT("Failed to launch process.") );			
		} else {
			appRequestExit( 0 );
		}

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

//
// Load a library.
//
void* appGetDllHandle( const TCHAR* Filename )
{
	guard(appGetDllHandle);

	#if __STATIC_LINK
		INT* FalseHandle;
		return (void*) FalseHandle;
	#endif
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

//
// Free a library.
//
void appFreeDllHandle( void* DllHandle )
{
	guard(appFreeDllHandle);
	check(DllHandle);

	dlclose( DllHandle );

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

	dlerror();	// Clear any error condition.

	Result = (void*)dlsym( DllHandle, appToAnsi(ProcName) );
	Error = dlerror();
	if( Error != NULL )
		debugf( "dlerror: %s", Error );
	return Result;

	unguard;
}

//
// Break the debugger.
//
#ifndef DEFINED_appDebugBreak
void appDebugBreak()
{
	guard(appDebugBreak);
#ifdef ASMLINUX
	asm("int $03");
#endif
	unguard;
}
#endif

/*-----------------------------------------------------------------------------
	External processes.
-----------------------------------------------------------------------------*/
static TMap<pid_t,int>* ExitCodeMap = NULL;

void* appCreateProc( const TCHAR* URL, const TCHAR* Parms , UBOOL bRealTime )
{
	guard(appCreateProc);

	debugf( TEXT("Create Proc: %s %s"), URL, Parms );

	TCHAR LocalParms[PATH_MAX] = TEXT("");
	appStrcpy(LocalParms, URL);
	appStrcat(LocalParms, " ");
	appStrcat(LocalParms, Parms);

	pid_t pid = fork(); 
	if (pid == 0)
		_exit( system( LocalParms ) );

	return (void*)pid;
	unguard;
}

UBOOL appGetProcReturnCode( void* ProcHandle, INT* ReturnCode )
{
	guard(appGetProcReturnCode);
	int* p = ExitCodeMap->Find( (pid_t)ProcHandle );
	if(p)
	{
		*ReturnCode = *p;
		ExitCodeMap->Remove( (pid_t)ProcHandle );
		return 1;
	}
	return 0;
	unguard;
}

void HandleChild(int Signal)
{
	int Status;
	pid_t pid;
	while( (pid=waitpid(-1, &Status, WNOHANG)) > 0 )
		ExitCodeMap->Set( pid, WEXITSTATUS(Status) );
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
	static double InitialTime = 0.0;
	static double TimeCounter = 0.0;
	double NewTime;
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

#if !DEFINED_appCycles
CORE_API DWORD appCycles()
{
	return (DWORD)appSeconds();
}
#endif

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

	INT SleepTime = appRound(Seconds * 1000000);
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
		FLOAT DiffSeconds = difftime( CurrentTime, FileTime );
		return appRound(DiffSeconds / 60.0f / 60.0f / 24.0f);
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
//static FString ClipboardText;
CORE_API void appClipboardCopy( const TCHAR* Str )
{
	guard(appClipboardCopy);
//	ClipboardText = FString( Str );
	unguard;
}

CORE_API FString appClipboardPaste()
{
	guard(appClipboardPaste);
//	return ClipboardText;
	FString Empty;
	return Empty;
	unguard;
}
/*-----------------------------------------------------------------------------
	Command line.
-----------------------------------------------------------------------------*/

// Get startup directory.
CORE_API const TCHAR* appBaseDir()
{
	guard(appBaseDir);
	static TCHAR BaseDir[PATH_MAX]=TEXT("");
	if( !BaseDir[0] )
	{
		// If the executable isn't launched from its own directory, then the
		// dynamic linker won't be able to find the shared libraries.
		getcwd( BaseDir, sizeof(BaseDir) );
		appStrcat( BaseDir, "/" );
	}
	return BaseDir;
	unguard;
}

// Get computer name.
CORE_API const TCHAR* appComputerName()
{
	guard(appComputerName);
	static TCHAR Result[256]="";
	if( !Result[0] )
		gethostname( Result, sizeof(Result) );
	return Result;
	unguard;
}

// Get user name.
CORE_API const TCHAR* appUserName()
{
	guard(appUserName);
	static TCHAR Result[256]="";
	if( !Result[0] )
		appStrncpy( Result, getlogin(), sizeof(Result) );
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

void appPlatformInit()
{
	guard(appPlatformInit);

	// System initialization.
	GSys = new USystem;
	GSys->AddToRoot();
	for( INT i=0; i<GSys->Suppress.Num(); i++ )
		GSys->Suppress(i).SetFlags( RF_Suppress );

	// Randomize.
	srand( (unsigned)time( NULL ) );

	// Exit code handling
	ExitCodeMap = new TMap<pid_t,int>;

	// CPU speed.
	GTimestamp = 1;
	DWORD a = appCycles();
	appSleep(1);
	DWORD b = appCycles();
	DWORD CyclesPerSecond = b - a;
	double ASecond = 1.0;
	GSecondsPerCycle = 1.0 / CyclesPerSecond;
	debugf( NAME_Init, TEXT("CPU Speed=%f MHz"), 0.000001 / GSecondsPerCycle );

	struct sigaction sa_child;
	sa_child.sa_handler = HandleChild;
	sigemptyset( &sa_child.sa_mask );
	sa_child.sa_flags = 0;
	sigaction(SIGCHLD, &sa_child, 0);
	
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

unsigned long appGetLocalIP( void )
{
	static unsigned long LocalIP = 0;
	struct hostent *Hostinfo;
	char Hostname[256];

	if( LocalIP==0 )
	{
		gethostname( Hostname, sizeof(Hostname) );
		Hostinfo = gethostbyname( Hostname );
		LocalIP = *(unsigned long*)Hostinfo->h_addr_list[0];
	}
	
	return LocalIP;
}

/*-----------------------------------------------------------------------------
	String functions.
-----------------------------------------------------------------------------*/

int stricmp( const char* s, const char* t )
{
	int	i;
	for( i = 0; tolower(s[i]) == tolower(t[i]); i++ )
		if( s[i] == '\0' )
			return 0;
	return s[i] - t[i];
}

int strnicmp( const char* s, const char* t, int n )
{
	int	i;
	if( n <= 0 )
		return 0;
	for( i = 0; tolower(s[i]) == tolower(t[i]); i++ )
		if( (s[i] == '\0') || (i == n - 1) )
			return 0;
	return s[i] - t[i];
}

char* strupr( char* s )
{
	int	i;
	for( i = 0; s[i] != '\0'; i++ )
		s[i] = toupper(s[i]);
	return s;
}

/*-----------------------------------------------------------------------------
	Signal Handling
-----------------------------------------------------------------------------*/
jmp_buf __Context::Env;
struct sigaction __Context::Act_SIGHUP;
struct sigaction __Context::Act_SIGQUIT;
struct sigaction __Context::Act_SIGILL;
struct sigaction __Context::Act_SIGTRAP;
struct sigaction __Context::Act_SIGIOT;
struct sigaction __Context::Act_SIGBUS;
struct sigaction __Context::Act_SIGFPE;
struct sigaction __Context::Act_SIGSEGV;
struct sigaction __Context::Act_SIGTERM;

void __Context::StaticInit()
{
	// Only try once.
	INT DefaultFlag = SA_ONESHOT;

	// Install a handler for all signals.
	Act_SIGHUP.sa_handler = HandleSignal;
	sigemptyset( &Act_SIGHUP.sa_mask );
	Act_SIGHUP.sa_flags = DefaultFlag;
	sigaction( SIGHUP, &Act_SIGHUP, 0 );

	Act_SIGQUIT.sa_handler = HandleSignal;
	sigemptyset( &Act_SIGQUIT.sa_mask );
	Act_SIGQUIT.sa_flags = DefaultFlag;
	sigaction( SIGQUIT, &Act_SIGQUIT, 0 );

	Act_SIGILL.sa_handler = HandleSignal;
	sigemptyset( &Act_SIGILL.sa_mask );
	Act_SIGILL.sa_flags = DefaultFlag;
	sigaction( SIGILL, &Act_SIGILL, 0 );

	Act_SIGTRAP.sa_handler = HandleSignal;
	sigemptyset( &Act_SIGTRAP.sa_mask );
	Act_SIGTRAP.sa_flags = DefaultFlag;
	sigaction( SIGTRAP, &Act_SIGTRAP, 0 );

	Act_SIGIOT.sa_handler = HandleSignal;
	sigemptyset( &Act_SIGIOT.sa_mask );
	Act_SIGIOT.sa_flags = DefaultFlag;
	sigaction( SIGIOT, &Act_SIGIOT, 0 );

	Act_SIGBUS.sa_handler = HandleSignal;
	sigemptyset( &Act_SIGBUS.sa_mask );
	Act_SIGBUS.sa_flags = DefaultFlag;
	sigaction( SIGBUS, &Act_SIGBUS, 0 );

	Act_SIGFPE.sa_handler = HandleSignal;
	sigemptyset( &Act_SIGFPE.sa_mask );
	Act_SIGFPE.sa_flags = DefaultFlag;
	sigaction( SIGFPE, &Act_SIGFPE, 0 );

	Act_SIGSEGV.sa_handler = HandleSignal;
	sigemptyset( &Act_SIGSEGV.sa_mask );
	Act_SIGSEGV.sa_flags = DefaultFlag;
	sigaction( SIGSEGV, &Act_SIGSEGV, 0 );

	Act_SIGTERM.sa_handler = HandleSignal;
	sigemptyset( &Act_SIGTERM.sa_mask );
	Act_SIGTERM.sa_flags = DefaultFlag;
	sigaction( SIGTERM, &Act_SIGTERM, 0 );

	signal( SIGPIPE, SIG_IGN );
}

void __Context::HandleSignal( int Sig )
{
	switch (Sig)
	{
		case SIGHUP:
			printf( "Signal: SIGHUP [hangup]\n" );
			SignalExit++;
			break;
		case SIGQUIT:
			printf( "Signal: SIGQUIT [quit]\n" );
			SignalExit++;
			break;
		case SIGILL:
			printf( "Signal: SIGILL [illegal instruction]\n" );
			SignalCritical++;
			break;
		case SIGTRAP:
			printf( "Signal: SIGTRAP [trap]\n" );
			SignalCritical++;
			break;
		case SIGIOT:
			printf( "Signal: SIGIOT [iot trap]\n" );
			SignalCritical++;
			break;
		case SIGBUS:
			printf( "Signal: SIGBUS [bus error]\n" );
			SignalCritical++;
			break;
		case SIGFPE:
			printf( "Signal: SIGFPE [floating point exception]\n" );
			SignalCritical++;
			break;
		case SIGSEGV:
			printf( "Signal: SIGSEGV [segmentation fault]\n" );
			SignalCritical++;
			break;
		case SIGTERM:
			printf( "Signal: SIGTERM [terminate]\n" );
			SignalExit++;
			break;
	}

	if ( (SignalCritical > 0) || (SignalExit > 1) )
	{
		if (AlreadyAborting)
		{
			// Avoid calling appExit again.
			AlreadyAborting = true;
			printf( "Aborting.\n" );
			appExit();
		}
		exit(1);
	}
	if ( SignalExit == 1 )
	{
		printf("Requesting Exit.\n");
		appRequestExit( 0 );
	}

	longjmp( Env, 1 );
}

#endif
/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
