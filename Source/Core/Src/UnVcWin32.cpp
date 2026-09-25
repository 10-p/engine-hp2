/*=============================================================================
	UnVcWin32.cpp: Visual C++ Windows 32-bit core.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/
#if _MSC_VER

// Windows and runtime includes.
#pragma warning( disable : 4201 )
#include <windows.h>
#include <commctrl.h> //oldver: Remnant of old editor!!
#include <shlobj.h>
#include <stdio.h>
#include <float.h>
#include <time.h>
#include <io.h>
#include <direct.h>
#include <errno.h>
#include <sys/stat.h>

// Core includes.
#include "CorePrivate.h"

/*-----------------------------------------------------------------------------
	Unicode helpers.
-----------------------------------------------------------------------------*/

#if UNICODE
CORE_API ANSICHAR* winToANSI( ANSICHAR* ACh, const TCHAR* InUCh, INT Count )
{
	guardSlow(winToANSI);
	WideCharToMultiByte( CP_ACP, 0, InUCh, -1, ACh, Count, NULL, NULL );
	return ACh;
	unguardSlow;
}
CORE_API ANSICHAR* winToOEM( ANSICHAR* ACh, const TCHAR* InUCh, INT Count )
{
	guardSlow(winToOEM);
	WideCharToMultiByte( CP_OEMCP, 0, InUCh, -1, ACh, Count, NULL, NULL );
	return ACh;
	unguardSlow;
}
CORE_API INT winGetSizeANSI( const TCHAR* InUCh )
{
	guardSlow(winGetSizeANSI);
	return WideCharToMultiByte( CP_ACP, 0, InUCh, -1, NULL, 0, NULL, NULL );
	unguardSlow;
}
CORE_API TCHAR* winToUNICODE( TCHAR* UCh, const ANSICHAR* InACh, INT Count )
{
	MultiByteToWideChar( CP_ACP, 0, InACh, -1, UCh, Count );
	return UCh;
}
CORE_API INT winGetSizeUNICODE( const ANSICHAR* InACh )
{
	return MultiByteToWideChar( CP_ACP, 0, InACh, -1, NULL, 0 );
}
#endif

/*-----------------------------------------------------------------------------
	FOutputDeviceWindowsError.
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
		ExitProcess( 1 );
	}
	else
	{
		// Tell the platform specific code we want to exit cleanly from the main loop.
		PostQuitMessage( 0 );
		GIsRequestingExit = 1;
	}
	unguard;
}

//
// Get system error.
//
CORE_API const TCHAR* appGetSystemErrorMessage( INT Error )
{
	guard(appGetSystemErrorMessage);
	static TCHAR Msg[1024];
	*Msg = 0;
	if( Error==0 )
		Error = GetLastError();
#if UNICODE
	if( !GUnicodeOS )
	{
		ANSICHAR ACh[1024];
		FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM, NULL, Error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), ACh, 1024, NULL );
		appStrcpy( Msg, appFromAnsi(ACh) );
	}
	else
#endif
	{
		FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM, NULL, Error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), Msg, 1024, NULL );
	}
	if( appStrchr(Msg,'\r') )
		*appStrchr(Msg,'\r')=0;
	if( appStrchr(Msg,'\n') )
		*appStrchr(Msg,'\n')=0;
	return Msg;
	unguard;
}

/*-----------------------------------------------------------------------------
	USystem.
-----------------------------------------------------------------------------*/

//
// System manager.
//

#pragma warning (push)
#pragma warning (disable : 4717)
static void Recurse()
{
	guard(Recurse);
	Recurse();
	unguard;
}
#pragma warning (pop)
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
		MEMORYSTATUS B; B.dwLength = sizeof(B);
		GlobalMemoryStatus(&B);
		Ar.Logf( TEXT("Memory available: Phys=%iK Pagef=%iK Virt=%iK"), B.dwAvailPhys/1024, B.dwAvailPageFile/1024, B.dwAvailVirtual/1024 );
		Ar.Logf( TEXT("Memory load = %i%%"), B.dwMemoryLoad );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("CONFIGHASH")) )
	{
		GConfig->Dump( Ar );
		return 1;
	}
	else if( ParseCommand( &Cmd, TEXT("RELAUNCH") ) )
	{
		debugf( TEXT("Relaunch: %s"), Cmd );
		GConfig->Flush( 0 );
#if UNICODE
		if( !GUnicodeOS )
		{
			ANSICHAR ThisFile[256];
			GetModuleFileNameA( NULL, ThisFile, ARRAY_COUNT(ThisFile) );
			debugf( TEXT("GetModuleFileName returned %s"), appFromAnsi(ThisFile) );
#if ENGINE_VERSION>=400
			ShellExecuteA( NULL, "open", "unrealtournament.exe", TCHAR_TO_ANSI(Cmd), TCHAR_TO_ANSI(appBaseDir()), SW_SHOWNORMAL );
#else
			ShellExecuteA( NULL, "open", ThisFile, TCHAR_TO_ANSI(Cmd), TCHAR_TO_ANSI(appBaseDir()), SW_SHOWNORMAL );
#endif
		}
		else
#endif
		{
			TCHAR ThisFile[256];
			GetModuleFileName( NULL, ThisFile, ARRAY_COUNT(ThisFile) );
			debugf( TEXT("GetModuleFileName returned %s"), ThisFile );
#if ENGINE_VERSION>=400
			ShellExecute( NULL, TEXT("open"), TEXT("unrealtournament.exe"), Cmd, appBaseDir(), SW_SHOWNORMAL );
#else
			ShellExecute( NULL, TEXT("open"), ThisFile, Cmd, appBaseDir(), SW_SHOWNORMAL );
#endif
		}
		appRequestExit( 0 );
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
	Clipboard.
-----------------------------------------------------------------------------*/

//
// Copy text to clipboard.
//
CORE_API void appClipboardCopy( const TCHAR* Str )
{
	guard(appClipboardCopy);
	if( OpenClipboard(GetActiveWindow()) )
	{
		verify(EmptyClipboard());
#if UNICODE
		if( GUnicode && !GUnicodeOS )
		{
			INT Count = WideCharToMultiByte(CP_ACP,0,Str,-1,NULL,0,NULL,NULL);
			ANSICHAR* Data = (ANSICHAR*)GlobalAlloc( GMEM_DDESHARE, Count+1 );
			WideCharToMultiByte(CP_ACP,0,Str,-1,Data,Count,NULL,NULL);
			Data[Count] = 0;
			verify(SetClipboardData( CF_TEXT, Data ));
		}
		else
#endif
		{
			TCHAR* Data = (TCHAR*)GlobalAlloc( GMEM_DDESHARE, sizeof(TCHAR)*(appStrlen(Str)+1) );
			appStrcpy( Data, Str );
			verify(SetClipboardData( GUnicode ? CF_UNICODETEXT : CF_TEXT, Data ));
		}
		verify(CloseClipboard());
	}
	unguard;
}

//
// Paste text from clipboard.
//
CORE_API FString appClipboardPaste()
{
	guard(appClipboardPaste);
	FString Result;
	if( OpenClipboard(GetActiveWindow()) )
	{
		void* V = NULL;
		if( GUnicode && GUnicodeOS )
		{
			V = GetClipboardData( CF_UNICODETEXT );
			if( V )
				Result = (TCHAR*)V;
		}
		if( !V )
		{
			V = GetClipboardData( CF_TEXT );
			if( V )
			{
				ANSICHAR* ACh = (ANSICHAR*)V;
				INT i;
				for( i=0; ACh[i]; i++ );
				TArray<TCHAR> Ch(i+1);
				for( i=0; i<Ch.Num(); i++ )
					Ch(i)=FromAnsi(ACh[i]);
				Result = &Ch(0);
			}
		}
		if( !V )
		{
			Result = TEXT("");
		}
		verify(CloseClipboard());
	}
	else Result=TEXT("");
	return Result;
	unguard;
}

/*-----------------------------------------------------------------------------
	DLLs.
-----------------------------------------------------------------------------*/

//
// Load a library.
//warning: Intended only to be called by UPackage::BindPackage. Don't call directly.
//
static void* GetDllHandleHelper( const TCHAR* Filename )
{
	return TCHAR_CALL_OS(LoadLibraryW(Filename),LoadLibraryA(TCHAR_TO_ANSI(Filename)));
}
CORE_API void* appGetDllHandle( const TCHAR* Filename )
{
	guard(appGetDllHandle);
	check(Filename);

	void* Result = GetDllHandleHelper( Filename );
	if( !Result )
		Result = GetDllHandleHelper( *(FString(Filename) + DLLEXT) );
	if( !Result )
		Result = GetDllHandleHelper( *(US + TEXT("_") + Filename + DLLEXT) );

	return Result;
	unguard;
}

//
// Free a DLL.
//
CORE_API void appFreeDllHandle( void* DllHandle )
{
	guard(appFreeDllHandle);
	check(DllHandle);

	FreeLibrary( (HMODULE)DllHandle );

	unguard;
}

//
// Lookup the address of a DLL function.
//
CORE_API void* appGetDllExport( void* DllHandle, const TCHAR* ProcName )
{
	guard(appGetDllExport);
	check(DllHandle);
	check(ProcName);

	return (void*)GetProcAddress( (HMODULE)DllHandle, TCHAR_TO_ANSI(ProcName) );

	unguard;
}

CORE_API const void appDebugMessagef( const TCHAR* Fmt, ... )
{
	TCHAR TempStr[4096]=TEXT("");
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), Fmt );
	guard(appDebugMessagef);
	MessageBox(NULL,TempStr,TEXT("appDebugMessagef"),MB_OK);
	unguard;
}

#if 1 //U2Ed
CORE_API const void appMsgf( const TCHAR* Fmt, ... )
{
	guard(appMsgf);
	TCHAR TempStr[4096]=TEXT("");
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), Fmt );
	MessageBox( NULL, TempStr, TEXT("Message"), MB_OK );
	unguard;
}

CORE_API const void appGetLastError( void )
{
	guard(appGetLastError);
	TCHAR TempStr[4096]=TEXT("");
	appSprintf( TempStr, TEXT("GetLastError : %d\n\n%s"),
		GetLastError(), appGetSystemErrorMessage() );
	MessageBox( NULL, TempStr, TEXT("System Error"), MB_OK );
	unguard;
}

// This is a special version of ANSI_TO_TCHAR that does things a little differently.  The code editing window seems
// to need this version because the original ANSI_TO_TCHAR makes it crash on large scripts.
CORE_API TCHAR* LEGEND_ANSI_TO_TCHAR(char* str)
{
	int iLength = winGetSizeUNICODE(str);
	TCHAR* pBuffer = new TCHAR[iLength];
	appStrcpy(pBuffer,TEXT(""));
	TCHAR* ret = winToUNICODE(pBuffer,str,iLength);
	//delete [] pBuffer;
	return ret;
}
#endif

//
// Break the debugger.
//
#ifndef DEFINED_appDebugBreak
void appDebugBreak()
{
	guard(appDebugBreak);
	::DebugBreak();
	unguard;
}
#endif

/*-----------------------------------------------------------------------------
	Timing.
-----------------------------------------------------------------------------*/

//
// Get time in seconds. Origin is arbitrary.
//
#if !DEFINED_appSeconds
CORE_API FTime appSeconds()
{
	static DWORD  InitialTime = timeGetTime();
	static double TimeCounter = 0.0;

	// Accumulate difference to prevent wraparound.
	DWORD NewTime = timeGetTime();
	TimeCounter += (NewTime - InitialTime) * (1./1000.);
	InitialTime = NewTime;

	return TimeCounter;
}
#endif

//
// Return number of CPU cycles passed. Origin is arbitrary.
//
#if !DEFINED_appCycles
CORE_API DWORD appCycles()
{
	return appSeconds();
}
#endif

//
// Sleep this thread for Seconds, 0.0 means release the current
// timeslice to let other threads get some attention.
//
CORE_API void appSleep( FLOAT Seconds )
{
	guard(appSleep);
	Sleep( (DWORD)(Seconds * 1000.0) );
	unguard;
}

//
// Return the system time.
//
CORE_API void appSystemTime( INT& Year, INT& Month, INT& DayOfWeek, INT& Day, INT& Hour, INT& Min, INT& Sec, INT& MSec )
{
	guard(appSystemTime);

	SYSTEMTIME st;
	GetLocalTime( &st );

	Year		= st.wYear;
	Month		= st.wMonth;
	DayOfWeek	= st.wDayOfWeek;
	Day			= st.wDay;
	Hour		= st.wHour;
	Min			= st.wMinute;
	Sec			= st.wSecond;
	MSec		= st.wMilliseconds;

	unguard;
}

//
// Return seconds counter.
//
CORE_API FTime appSecondsSlow()
{
	static INT LastTickCount=0;
	static double TickCount=0.0;
	INT ThisTickCount = GetTickCount();
	TickCount += (ThisTickCount-LastTickCount) / 1000.0;
	LastTickCount = ThisTickCount;
	return TickCount;
}

// Return elapsed time for process only.
CORE_API FTime appProcessSeconds()
{
	FILETIME Creation, Exit, Kernel, User;
	if( GetProcessTimes( GetCurrentProcess(), &Creation, &Exit, &Kernel, &User ) )
	{
		__int64 Total = reinterpret_cast<__int64&>(User) + reinterpret_cast<__int64&>(Kernel);
		return double(Total) * 1e-7;
	}

	// Unsupported.
	return appSeconds();
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
	debugf( NAME_Log, TEXT("LaunchURL %s %s"), URL, Parms?Parms:TEXT("") );
	INT Code = (INT)TCHAR_CALL_OS(ShellExecuteW(NULL,TEXT("open"),URL,Parms?Parms:TEXT(""),TEXT(""),SW_SHOWNORMAL),ShellExecuteA(NULL,"open",TCHAR_TO_ANSI(URL),Parms?TCHAR_TO_ANSI(Parms):"","",SW_SHOWNORMAL));
	if( Error )
		*Error = Code<=32 ? LocalizeError("UrlFailed") : TEXT("");
	unguard;
}

void *appCreateProc( const TCHAR* URL, const TCHAR* Parms , UBOOL bRealTime )
{
	guard(appCreateProc);
	debugf( NAME_Log, TEXT("CreateProc %s %s"), URL, Parms );

	TCHAR CommandLine[1024];
	appSprintf( CommandLine, TEXT("%s %s"), URL, Parms );

	PROCESS_INFORMATION ProcInfo;
	SECURITY_ATTRIBUTES Attr;
	Attr.nLength = sizeof(SECURITY_ATTRIBUTES);
	Attr.lpSecurityDescriptor = NULL;
	Attr.bInheritHandle = TRUE;

#if UNICODE
	if( GUnicode && !GUnicodeOS )
	{
		STARTUPINFOA StartupInfoA = { sizeof(STARTUPINFO), NULL, NULL, NULL,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			NULL, NULL, NULL, NULL, SW_HIDE, NULL, NULL,
			NULL, NULL, NULL };
		if( !CreateProcessA( NULL, TCHAR_TO_ANSI(CommandLine), &Attr, &Attr, TRUE, bRealTime ? (DETACHED_PROCESS | REALTIME_PRIORITY_CLASS) : DETACHED_PROCESS,
			NULL, NULL, &StartupInfoA, &ProcInfo ) )
			return NULL;
	}
	else
#endif
	{
		STARTUPINFO StartupInfo = { sizeof(STARTUPINFO), NULL, NULL, NULL,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			NULL, NULL, NULL, NULL, SW_HIDE, NULL, NULL,
			NULL, NULL, NULL };
		if( !CreateProcess( NULL, CommandLine, &Attr, &Attr, TRUE, bRealTime ? (DETACHED_PROCESS | REALTIME_PRIORITY_CLASS) : DETACHED_PROCESS,
			NULL, NULL, &StartupInfo, &ProcInfo ) )
			return NULL;
	}
	return (void*)ProcInfo.hProcess;
	unguard;
}

UBOOL appGetProcReturnCode( void* ProcHandle, INT* ReturnCode )
{
	guard(appGetProcReturnCode);
	return GetExitCodeProcess( (HANDLE)ProcHandle, (DWORD*)ReturnCode ) && *((DWORD*)ReturnCode) != STILL_ACTIVE;
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
	struct _stat Buf;
	INT Result = 0;
#if UNICODE
	if( GUnicodeOS )
	{
		Result = _wstat(Filename,&Buf);
	}
	else
#endif
	{
		Result = _stat(TCHAR_TO_ANSI(Filename),&Buf);
	}
	if( Result==0 )
	{
		time_t CurrentTime, FileTime;
		FileTime = Buf.st_mtime;
		time( &CurrentTime );
		return appRound(difftime(CurrentTime,FileTime) / 60 / 60 / 24);
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

// TG ALPHA begin
/*-----------------------------------------------------------------------------
	File Streaming.
-----------------------------------------------------------------------------*/
#ifndef _XBOX
#if SUPPORTS_PRAGMA_PACK
#pragma pack (push,8)
#endif
#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"
#include "coda.h"
#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif
#endif

static DWORD FileStreamingThreadId = 0;

static DWORD STDCALL FileStreamingThread( void* Arg )
{
	while (GFileStream->Destroyed == 0)
	{
		for (INT i=0; i<GFileStream->MaxStreams; i++)
		{	
			GFileStream->Enter(i);

			if (GFileStream->Streams[i].Used && GFileStream->Streams[i].ChunksRequested )
			{
				//debugf( NAME_DevSound, TEXT("FileStreamingThread  NumSamples = %d  # chunks = %d  time = %d"), GFileStream->Streams[i].NumSamples, GFileStream->Streams[i].ChunksRequested, (int)(appSeconds().GetFloat() * 10000));

				GFileStream->Read( i, GFileStream->Streams[i].ChunkSize );
				GFileStream->Streams[i].ChunksRequested--;
				GFileStream->Streams[i].ChunksRemaining++;

				//	This is general file streaming logic that breaks when used by the Warfare streaming code.
				//	They reset the data pointer each time they request a chunk, therefore the data at
				//	.Data + .ChunkSize is NEVER VALID.  This will cause a crash if more than 1 requested chunk is
				//	waiting.  -tg
				//
				//GFileStream->Streams[i].Data = ((BYTE*) GFileStream->Streams[i].Data) + GFileStream->Streams[i].ChunkSize;
			}
			GFileStream->Leave(i);
		}
		Sleep(1);
	}
	for (INT i=0; i<GFileStream->MaxStreams; i++)
	{
		GFileStream->Enter(i);
		if (GFileStream->Streams[i].Handle)
			GFileStream->Destroy(i);
		GFileStream->Leave(i);
	}
	GFileStream->Destroyed = 2;
	ExitThread( 0 );
	return 0;
}

UBOOL FFileStream::Read( INT StreamId, INT Bytes )
{
	if ( Streams[StreamId].Handle && Streams[StreamId].Data )
	{
		switch ( Streams[StreamId].Type )
		{
			case ST_Regular:	
			{
				INT Count=0;
				UBOOL RetVal = ReadFile( Streams[StreamId].Handle, Streams[StreamId].Data, Bytes, (DWORD*) &Count, NULL );
				if (RetVal)
					Streams[StreamId].Data = (BYTE*)(Streams[StreamId].Data) + Count;
				if (Count != Bytes)
					Streams[StreamId].EndOfFile = 1;
				return RetVal;
			}

			case ST_Ogg:
			case ST_OggLooping:
			{
#ifndef _XBOX
				long Count = 0;
				while ( Count < Bytes )
				{
					long Read = ov_read( 
						(OggVorbis_File*) Streams[StreamId].TDD, 
						((char*) Streams[StreamId].Data) + Count, 
						Bytes - Count, 
						0,	//!! vogel: TODO: should be 1 for big endian machines
						2,
						1, 
						&Streams[StreamId].FileSeek
					);
				//	debugf(NAME_DevSound, TEXT("FFileStream::Read()  Bytes = %d, Read (returned) = %d"), Bytes, Read );
					if ( Read == 0 )
					{
						if( Streams[StreamId].Type == ST_OggLooping )
						{
							//Rewind our stream
						//	debugf(TEXT("FFileStream::Read - doing ov_time_seek") );
							ov_time_seek( (OggVorbis_File*) Streams[StreamId].TDD, 0.0 );
						}
						else
						{
							// Filling the rest of the buffer with silence. Note that memset is used for reentrance reasons.
						//	debugf(TEXT("FFileStream::Read - Filling the rest of the buffer with silence") );
							memset( ((char*) Streams[StreamId].Data) + Count, 0 , Bytes - Count );
							Streams[StreamId].EndOfFile = 1;
							return false;
						}
					}
					else if ( Read < 0 )
					{
						return false;
					}
					else
					{
						Count += Read;
					}
				}
#endif
				return true;
			}

			case ST_XA:
			case ST_XALooping:
			{
				CEAXABLKDec	*pXADecoder = (CEAXABLKDec *)Streams[StreamId].TDD;

				INT	Count = 0;
				INT	Samples = Bytes / 2;

				while ( Count < Samples )
				{
					short	*buffer[1];
					buffer[0] = ((short *)Streams[StreamId].Data) + Count;

				//	FTime time = appSeconds();
					INT	Read = pXADecoder->Decode( &buffer[0], Samples - Count);

					//	Error - get out.
					//
					if (Read < 0)
						return false;

					if (Read < Samples - Count)
					{
						if (Streams[StreamId].Type == ST_XALooping)
						{
							//	We need to make sure the raw data is loaded - it may have been purged
							//
							TLazyArray<BYTE>	*pRawData = (TLazyArray<BYTE> *)Streams[StreamId].Handle;
							pRawData->Load();

							//	Re-feed the XA decoder from the beginning of the XA data stream
							//
							pXADecoder->Feed((void *)pRawData->GetData(), pRawData->Num(), Streams[StreamId].NumSamples);
						}
						else
						{
							int	iValidBytes = (Read + Count) * 2;

							memset( ((char*) Streams[StreamId].Data) + iValidBytes, 0 , Bytes - iValidBytes );
							Streams[StreamId].EndOfFile = 1;
	
							return true;
						}
					}

					Count += Read;
				}

				return true;
			}
			
			default:
				return false;
		}
	}
	else
	{
		return false;
	}
}

UBOOL FFileStream::Create( INT StreamId, const TCHAR* Filename )
{
	switch ( Streams[StreamId].Type )
	{
		case ST_Regular:
		{
			DWORD  Access    = GENERIC_READ;
			DWORD  WinFlags  = FILE_SHARE_READ;
			DWORD  Create    = OPEN_EXISTING;

#ifndef _XBOX
			HANDLE Handle    = TCHAR_CALL_OS(
				CreateFileW( Filename, Access, WinFlags, NULL, Create, FILE_ATTRIBUTE_NORMAL, NULL ), 
				CreateFileA( TCHAR_TO_ANSI(Filename), Access, WinFlags, NULL, Create, FILE_ATTRIBUTE_NORMAL, NULL ) 
			);
#else
			HANDLE Handle    = CreateFile( TCHAR_TO_ANSI(Filename), Access, WinFlags, NULL, Create, FILE_ATTRIBUTE_NORMAL, NULL );
#endif

			if( Handle == INVALID_HANDLE_VALUE )
			{
				Streams[StreamId].Handle = NULL;
				return false;
			}
			else
			{
				Streams[StreamId].Handle = Handle;
				Streams[StreamId].NumSamples = -1;
				return true;
			}
		}
		case ST_Ogg:
		case ST_OggLooping:
		{
#ifndef _XBOX
			void* Handle = TCHAR_CALL_OS( _wfopen(Filename,TEXT("rb")), fopen(TCHAR_TO_ANSI(Filename),"rb") );
			if ( Handle == NULL ) 
				return false;
			INT Error = ov_open( (FILE*) Handle, (OggVorbis_File*) Streams[StreamId].TDD, NULL, 0 );
			if (Error < 0)
				return false;
			else
			{
				Streams[StreamId].Handle = Handle;
				Streams[StreamId].NumSamples = -1;
				return true;
			}
#else
			return false;
#endif
		}

		default:
			return false;
	}
}


UBOOL FFileStream::Create( INT StreamId, TLazyArray<BYTE>* RawData, INT NumSamples )
{
	switch ( Streams[StreamId].Type )
	{
		case ST_XA:
		case ST_XALooping:
		{
			Streams[StreamId].Handle = (void *)RawData;
			Streams[StreamId].NumSamples = NumSamples;

			CEAXABLKDec* pXADecoder = new CEAXABLKDec();
			if (!pXADecoder)
				return false;

			Streams[StreamId].TDD = (void *)pXADecoder;

			XA16STATE	xav;;
			xav.sample1 = 0;
			xav.sample2 = 0;
			pXADecoder->SetState(&xav);

			//	Load the data and feed the decoder
			//
			RawData->Load();
			pXADecoder->Feed((void *)RawData->GetData(), RawData->Num(), NumSamples);

			return true;
		}

		default:
			return false;
	}
}

	
UBOOL FFileStream::Destroy( INT StreamId )
{
	switch ( Streams[StreamId].Type )
	{
		case ST_Regular:
		{	
			if ( Streams[StreamId].Handle )
				CloseHandle( Streams[StreamId].Handle );

			Streams[StreamId].TDD				= NULL;
			Streams[StreamId].Handle			= NULL;
			Streams[StreamId].Used				= 0;
			Streams[StreamId].ChunksRequested	= 0;
			Streams[StreamId].ChunksRemaining	= 0;

			return true;
		}

		case ST_Ogg:
		case ST_OggLooping:
		{
#ifndef _XBOX
			if ( Streams[StreamId].TDD )
			{
				ov_clear( (OggVorbis_File*) Streams[StreamId].TDD );
				delete (OggVorbis_File*) Streams[StreamId].TDD;		//tg
			}

			Streams[StreamId].TDD				= NULL;
			Streams[StreamId].Handle			= NULL;
			Streams[StreamId].Used				= 0;
			Streams[StreamId].ChunksRequested	= 0;
			Streams[StreamId].ChunksRemaining	= 0;

			return true;
#else	 
			return true;
#endif
		}

		case ST_XA:
		case ST_XALooping:
		{
			if ( Streams[StreamId].TDD )
			{
				delete (CEAXABLKDec *)Streams[StreamId].TDD;
			}

#if 0
			//	Unload the raw XA data
			//
			TLazyArray<BYTE>	*pRawData = (TLazyArray<BYTE> *)Streams[StreamId].Handle;
			// if ref count < 0 ?// TG ALPHA
			pRawData->Unload();
#endif

			Streams[StreamId].TDD				= NULL;
			Streams[StreamId].Handle			= NULL;
			Streams[StreamId].Used				= 0;
			Streams[StreamId].ChunksRequested	= 0;
			Streams[StreamId].ChunksRemaining	= 0;

			return true;
		}

		default:
			return false;
	}
}

// TG ALPHA end
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
	CoCreateGuid( (GUID*)&Result );
	return Result;

	unguard;
}

/*-----------------------------------------------------------------------------
	Command line.
-----------------------------------------------------------------------------*/

// Get startup directory.
CORE_API const TCHAR* appBaseDir()
{
	guard(appBaseDir);
	static TCHAR Result[256]=TEXT("");
	if( !Result[0] )
	{
		// Get directory this executable was launched from.
#if UNICODE
		if( GUnicode && !GUnicodeOS )
		{
			ANSICHAR ACh[256];
			GetModuleFileNameA( hInstance, ACh, ARRAY_COUNT(ACh) );
			MultiByteToWideChar( CP_ACP, 0, ACh, -1, Result, ARRAY_COUNT(Result) );
		}
		else
#endif
		{
			GetModuleFileName( hInstance, Result, ARRAY_COUNT(Result) );
		}
		for( INT i=appStrlen(Result)-1; i>0; i-- )
			if( Result[i-1]==PATH_SEPARATOR[0] || Result[i-1]=='/' )
				break;
		Result[i]=0;
	}
	return Result;
	unguard;
}

FARPROC (STDCALL *pfnSHGetFolderPath)(HWND hwnd, int csidl, HANDLE hToken, DWORD dwFlags, TCHAR pszPath[]);

CORE_API const TCHAR* appUserDir()
{
	guard(appUserDir);
	static TCHAR Result[1024]=TEXT("");
	if( !Result[0] )
	{
		HMODULE hModSHFolder = LoadLibrary(TEXT("shfolder.dll"));
		if( hModSHFolder != NULL )
		{
			(*(FARPROC*)&pfnSHGetFolderPath = GetProcAddress(hModSHFolder, "SHGetFolderPathW"));
			if (pfnSHGetFolderPath != NULL )
			{
				pfnSHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, 0, Result);
				if( *Result )
				{
					appStrcat( Result, TEXT("\\Harry Potter and the Chamber of Secrets\\") );
					CreateDirectory(Result, NULL);
				}
			}
		}
	}
	return Result;
	unguard;
}

// Get computer name.
CORE_API const TCHAR* appComputerName()
{
	guard(appComputerName);
	static TCHAR Result[256]=TEXT("");
	if( !Result[0] )
	{
		DWORD Size=ARRAY_COUNT(Result);
#if UNICODE
		if( GUnicode && !GUnicodeOS )
		{
			ANSICHAR ACh[ARRAY_COUNT(Result)];
			GetComputerNameA( ACh, &Size );
			MultiByteToWideChar( CP_ACP, 0, ACh, -1, Result, ARRAY_COUNT(Result) );
		}
		else
#endif
		{
			GetComputerName( Result, &Size );
		}
		for( TCHAR *c=Result, *d=Result; *c!=0; c++ )
			if( appIsAlnum(*c) )
				*d++ = *c;
		*d++ = 0;
	}
	return Result;
	unguard;
}

// Get user name.
CORE_API const TCHAR* appUserName()
{
	guard(appUserName);
	static TCHAR Result[256]=TEXT("");
	if( !Result[0] )
	{
		DWORD Size=ARRAY_COUNT(Result);
#if UNICODE
		if( GUnicode && !GUnicodeOS )
		{
			ANSICHAR ACh[ARRAY_COUNT(Result)];
			GetUserNameA( ACh, &Size );
			MultiByteToWideChar( CP_ACP, 0, ACh, -1, Result, ARRAY_COUNT(Result) );
		}
		else
#endif
		{
			GetUserName( Result, &Size );
		}
		for( TCHAR *c=Result, *d=Result; *c!=0; c++ )
			if( appIsAlnum(*c) )
				*d++ = *c;
		*d++ = 0;
	}
	return Result;
	unguard;
}

// Get launch package base name.
CORE_API const TCHAR* appPackage()
{
	guard(appPackage);
	static TCHAR Result[256]=TEXT("");
	if( !Result[0] )
	{
		TCHAR Tmp[256], *End=Tmp;
#if UNICODE
		if( GUnicode && !GUnicodeOS )
		{
			ANSICHAR ACh[ARRAY_COUNT(Result)];
			GetModuleFileNameA( NULL, ACh, ARRAY_COUNT(ACh) );
			MultiByteToWideChar( CP_ACP, 0, ACh, -1, Tmp, ARRAY_COUNT(Tmp) );
		}
		else
#endif
		{
			GetModuleFileName( NULL, Tmp, ARRAY_COUNT(Tmp) );
		}
		while( appStrchr(End,PATH_SEPARATOR[0]) )
			End = appStrchr(End,PATH_SEPARATOR[0])+1;
		while( appStrchr(End,'/') )
			End = appStrchr(End,'/')+1;
		if( appStrchr(End,'.') )
			*appStrchr(End,'.') = 0;
		if( appStricmp(End,TEXT("UnrealEd"))==0 ) //WDM: remove?
			End = TEXT("Unreal");
		if( appStricmp(End,TEXT("NewEd"))==0 ) //WDM: remove?
			End = TEXT("Unreal");
#if 1 //U2Ed
		if( appStricmp(End,TEXT("U2Ed"))==0 )
			End = TEXT("Unreal");
#endif
		appStrcpy( Result, End );
	}
	return Result;
	unguard;
}

/*-----------------------------------------------------------------------------
	App init/exit.
-----------------------------------------------------------------------------*/

//
// Platform specific initialization.
//
static void DoCPUID( int i, DWORD *A, DWORD *B, DWORD *C, DWORD *D )
{
#if ASM
 	__asm
	{			
		mov eax,[i]
		_emit 0x0f
		_emit 0xa2

		mov edi,[A]
		mov [edi],eax

		mov edi,[B]
		mov [edi],ebx

		mov edi,[C]
		mov [edi],ecx

		mov edi,[D]
		mov [edi],edx

		mov eax,0
		mov ebx,0
		mov ecx,0
		mov edx,0
		mov esi,0
		mov edi,0
	}
#else
	*A=*B=*C=*D=0;
#endif
}

CORE_API bool IsOSVer2kOrXP ()
{
	guard(IsOSVer2kOrXP);

	DWORD dwPlatformId, dwMajorVersion, dwMinorVersion, dwBuildNumber;
#if UNICODE
	if( GUnicode && !GUnicodeOS )
	{
		OSVERSIONINFOA Version;
		Version.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
		GetVersionExA(&Version);
		dwPlatformId   = Version.dwPlatformId;
		dwMajorVersion = Version.dwMajorVersion;
		dwMinorVersion = Version.dwMinorVersion;
		dwBuildNumber  = Version.dwBuildNumber;
	}
	else
#endif
	{
		OSVERSIONINFO Version;
		Version.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		GetVersionEx(&Version);
		dwPlatformId   = Version.dwPlatformId;
		dwMajorVersion = Version.dwMajorVersion;
		dwMinorVersion = Version.dwMinorVersion;
		dwBuildNumber  = Version.dwBuildNumber;
	}

	if( dwPlatformId==VER_PLATFORM_WIN32_NT )
	{
		if (dwMajorVersion >= 5)
			return true;
	}

	return false;
	unguard;
}


void appPlatformPreInit()
{
	guard(appPlatformPreInit);

	// Check Windows version.
	guard(GetWindowsVersion);
	DWORD dwPlatformId, dwMajorVersion, dwMinorVersion, dwBuildNumber;
#if UNICODE
	if( GUnicode && !GUnicodeOS )
	{
		OSVERSIONINFOA Version;
		Version.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
		GetVersionExA(&Version);
		dwPlatformId   = Version.dwPlatformId;
		dwMajorVersion = Version.dwMajorVersion;
		dwMinorVersion = Version.dwMinorVersion;
		dwBuildNumber  = Version.dwBuildNumber;
	}
	else
#endif
	{
		OSVERSIONINFO Version;
		Version.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		GetVersionEx(&Version);
		dwPlatformId   = Version.dwPlatformId;
		dwMajorVersion = Version.dwMajorVersion;
		dwMinorVersion = Version.dwMinorVersion;
		dwBuildNumber  = Version.dwBuildNumber;
	}
	if( dwPlatformId==VER_PLATFORM_WIN32_NT )
	{
		debugf( NAME_Init, TEXT("Detected: Microsoft Windows NT %u.%u (Build: %u)"), dwMajorVersion, dwMinorVersion, dwBuildNumber );
		GUnicodeOS = 1;
	}
	else if( dwPlatformId==VER_PLATFORM_WIN32_WINDOWS && (dwMajorVersion>4 || dwMinorVersion>=10) )
	{
		debugf( NAME_Init, TEXT("Detected: Microsoft Windows 98 %u.%u (Build: %u)"), dwMajorVersion, dwMinorVersion, dwBuildNumber );
	}
	else if( dwPlatformId==VER_PLATFORM_WIN32_WINDOWS )
	{
		debugf( NAME_Init, TEXT("Detected: Microsoft Windows 95 %u.%u (Build: %u)"), dwMajorVersion, dwMinorVersion, dwBuildNumber );
	}
	else
	{
		debugf( NAME_Init, TEXT("Detected: Windows %u.%u (Build: %u)"), dwMajorVersion, dwMinorVersion, dwBuildNumber );
	}
	unguard;

	unguard;
}
BOOL WINAPI CtrlCHandler( DWORD dwCtrlType )
{
	if( !GIsRequestingExit )
	{
		PostQuitMessage( 0 );
		GIsRequestingExit = 1;
	}
	else
	{
		appErrorf( TEXT("Aborted!") );
	}
	return 1;
}

void *tgmalloc( size_t size, const char *tag )
{
	return malloc(size);
}

void appPlatformInit()
{
	guard(appPlatformInit);

	// System initialization.
	GSys = new USystem;
	GSys->AddToRoot();
	for( INT i=0; i<GSys->Suppress.Num(); i++ )
		GSys->Suppress(i).SetFlags( RF_Suppress );

	// Ctrl+C handling.
	SetConsoleCtrlHandler( CtrlCHandler, 1 );

	// Randomize.
	srand( (unsigned)time( NULL ) );

	// Identity.
	debugf( NAME_Init, TEXT("Computer: %s"), appComputerName() );
	debugf( NAME_Init, TEXT("User: %s"), appUserName() );

	// Get memory.
	guard(GetMemory);
	MEMORYSTATUS M;
	GlobalMemoryStatus(&M);
	GPhysicalMemory=M.dwTotalPhys;
	debugf( NAME_Init, TEXT("Memory total: Phys=%iK Avail=%iK Pagef=%iK Virt=%iK"), 
		M.dwTotalPhys/1024, M.dwAvailPhys/1024, M.dwTotalPageFile/1024, M.dwTotalVirtual/1024 );
	unguard;

	// Working set.
	guard(GetWorkingSet);
	DWORD WsMin=0, WsMax=0;
	GetProcessWorkingSetSize( GetCurrentProcess(), &WsMin, &WsMax );
	debugf( NAME_Init, TEXT("Working set: %X / %X"), WsMin, WsMax );
	unguard;
 
	// CPU speed.
	guard(CheckCpuSpeed);
	FLOAT CpuSpeed;
	try
	{
		GTimestamp = 1;
		LARGE_INTEGER lFreq;
		QueryPerformanceFrequency(&lFreq);
		double Frequency = (double)(SQWORD)(((QWORD)lFreq.LowPart) + ((QWORD)lFreq.HighPart<<32));
		check(Frequency!=0);
		double S[3];
		for( INT i=0; i<ARRAY_COUNT(S); i++ )
		{
			INT Cycles=0;
			LARGE_INTEGER I1, I2;
			QueryPerformanceCounter(&I1);
			clock(Cycles);
			DWORD StartMsec = timeGetTime();
			while( timeGetTime()-StartMsec < 10 );
			QueryPerformanceCounter(&I2);
			unclock(Cycles);
			double T1 = (double)(SQWORD)((QWORD)I1.LowPart + (((QWORD)I1.HighPart)<<32)) / Frequency;
			double T2 = (double)(SQWORD)((QWORD)I2.LowPart + (((QWORD)I2.HighPart)<<32)) / Frequency;
			S[i]      = (T2-T1) / (double)Cycles;
			for( INT j=0; j<i-1; j++ )
				if( S[j]>S[i] )
					Exchange( S[j], S[i] );
		}
		GSecondsPerCycle = S[1];
		debugf( NAME_Init, TEXT("CPU Speed=%f MHz"), 0.000001 / GSecondsPerCycle );
	}
	catch( ... )
	{
		GTimestamp = 0;
		debugf( NAME_Init, TEXT("Timestamp not supported (Possibly 486 or Cyrix processor)") );
		GSecondsPerCycle = 1;
	}
	if( Parse(appCmdLine(),TEXT("CPUSPEED="),CpuSpeed) )
	{
		GSecondsPerCycle = 0.000001/CpuSpeed;
		debugf( NAME_Init, TEXT("CPU Speed Overridden=%f MHz"), 0.000001 / GSecondsPerCycle );
	}
	unguard;

	// Get CPU info.
	guard(GetCpuInfo);
	SYSTEM_INFO SI;
	GetSystemInfo(&SI);
	GPageSize = SI.dwPageSize;
	check(!(GPageSize&(GPageSize-1)));
	GProcessorCount=SI.dwNumberOfProcessors;
	debugf( NAME_Init, TEXT("CPU Page size=%i, Processors=%i"), SI.dwPageSize, SI.dwNumberOfProcessors );
	unguard;

	// Check processor version with CPUID.
	try
	{
		DWORD A=0, B=0, C=0, D=0;
		DoCPUID(0,&A,&B,&C,&D);
		TCHAR Brand[13], *Model, FeatStr[256]=TEXT("");
		Brand[ 0] = (ANSICHAR)(B);
		Brand[ 1] = (ANSICHAR)(B>>8);
		Brand[ 2] = (ANSICHAR)(B>>16);
		Brand[ 3] = (ANSICHAR)(B>>24);
		Brand[ 4] = (ANSICHAR)(D);
		Brand[ 5] = (ANSICHAR)(D>>8);
		Brand[ 6] = (ANSICHAR)(D>>16);
		Brand[ 7] = (ANSICHAR)(D>>24);
		Brand[ 8] = (ANSICHAR)(C);
		Brand[ 9] = (ANSICHAR)(C>>8);
		Brand[10] = (ANSICHAR)(C>>16);
		Brand[11] = (ANSICHAR)(C>>24);
		Brand[12] = (ANSICHAR)(0);
		DoCPUID( 1, &A, &B, &C, &D );
		switch( (A>>8) & 0x000f )
		{
			case 4:  Model=TEXT("486-class processor");        break;
			case 5:  Model=TEXT("Pentium-class processor");    break;
			case 6:  Model=TEXT("PentiumPro-class processor"); break;
			case 7:  Model=TEXT("P7-class processor");         break;
			default: Model=TEXT("Unknown processor");          break;
		}
		if( (D & 0x00008000) ) {appStrcat( FeatStr, TEXT(" CMov") ); GIsPentiumPro=1; }
		if( (D & 0x00000001) ) {appStrcat( FeatStr, TEXT(" FPU") );}
		if( (D & 0x00000010) ) {appStrcat( FeatStr, TEXT(" RDTSC") );}
		if( (D & 0x00000040) ) {appStrcat( FeatStr, TEXT(" PAE") );}
		if( (D & 0x00800000) && !ParseParam(appCmdLine(),TEXT("NOMMX")) ) {appStrcat( FeatStr, TEXT(" MMX") ); GIsMMX=1;}

		// Check for KNI instructions.
		if( (D & 0x02000000) && !ParseParam(appCmdLine(),TEXT("NOKNI")) )
		{
			appStrcat( FeatStr, TEXT(" KNI") );
			GIsKatmai=1;
		}

		// Check for K6 3D instructions.
		if( !ParseParam(appCmdLine(),TEXT("NOK6")) )
		{
			DoCPUID( 0x80000000, &A, &B, &C, &D );
			if( A >= 0x80000001 )
			{
				DoCPUID( 0x80000001, &A, &B, &C, &D );
				if( D & 0x80000000 )
				{
					appStrcat( FeatStr, TEXT(" 3DNow!") );
					GIs3DNow=1;
				}
			}
		}

		// Print features.
		debugf( NAME_Init, TEXT("CPU Detected: %s (%s)"), Model, Brand );
		debugf( NAME_Init, TEXT("CPU Features:%s"), FeatStr );
	}
	catch( ... )
	{
		debugf( NAME_Init, TEXT("Couldn't detect CPU: Probably 486 or non-Intel processor") );
	}

	// FPU.
	appEnableFastMath( 0 );

	//	TG ALPHA
	//
	//	Initialize CODA library
	//
#if DEBUG
	CODASetNew(tgmalloc);
#else
	CODASetNew(malloc);
#endif
	CODASetDelete(free);	

	// Create file streaming thread.
	HANDLE hThread = CreateThread( NULL, 0, FileStreamingThread, 0, 0, &FileStreamingThreadId );

	check(hThread);
	CloseHandle( hThread );

	unguard;
}
void appPlatformPreExit()
{
	guard(appPlatformPreExit);
	unguard;
}
void appPlatformExit()
{
}


//
// Set low precision mode.
//
void appEnableFastMath( UBOOL Enable )
{
	guard(appEnableFastMath);
	_controlfp( Enable ? (_PC_24) : (_PC_64), _MCW_PC );
	unguard;
}

#endif
/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
