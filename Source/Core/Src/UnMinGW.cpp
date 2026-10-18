/*=============================================================================
	UnMinGW.cpp: the few Win32 calls with no POSIX answer in UnUnix.cpp.
	Built for MinGW (Windows) targets only.
=============================================================================*/

#ifdef PLATFORM_WIN32

#include <windows.h>
#include <objbase.h>

#include "CorePrivate.h"

/*-----------------------------------------------------------------------------
	System errors.
-----------------------------------------------------------------------------*/

//
// Get system error, for FFileManagerWindows.h and FOutputDeviceWindowsError.h (UnVcWin32.cpp's version
// is MSVC-only).
//
CORE_API const TCHAR* appGetSystemErrorMessage( INT Error )
{
	guard(appGetSystemErrorMessage);
	static TCHAR Msg[1024];
	*Msg = 0;
	if( Error == 0 )
		Error = GetLastError();
	FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM, NULL, Error,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), Msg, ARRAY_COUNT(Msg), NULL );
	// FormatMessage terminates with CRLF; the callers log this inline.
	if( appStrchr(Msg,'\r') )
		*appStrchr(Msg,'\r') = 0;
	if( appStrchr(Msg,'\n') )
		*appStrchr(Msg,'\n') = 0;
	return Msg;
	unguard;
}

/*-----------------------------------------------------------------------------
	Guids.
-----------------------------------------------------------------------------*/

//
// appCreateGuid (UnUnix.cpp) calls this; on Windows the OS generator replaces UnGUID.cpp. FGuid is four
// DWORDs, the same shape and byte order as a Win32 GUID.
//
void appGetGUID( void* GUID )
{
	guard(appGetGUID);
	check(GUID);
	if( CoCreateGuid( (::GUID*)GUID ) != S_OK )
		appErrorf( TEXT("CoCreateGuid failed: %s"), appGetSystemErrorMessage() );
	unguard;
}

/*-----------------------------------------------------------------------------
	Sockets.
-----------------------------------------------------------------------------*/

//
// WinSock needs a process-wide startup, or every socket call fails with WSANOTINITIALISED. appComputerName
// and appGetLocalIP (UnUnix.cpp) call this, since they can run before IpDrv's own InitSockets.
//
CORE_API void appWinSockInit()
{
	guard(appWinSockInit);
	static UBOOL Initialized = 0;
	if( !Initialized )
	{
		WSADATA WsaData;
		INT Error = WSAStartup( MAKEWORD(2,2), &WsaData );
		if( Error != 0 )
			appErrorf( TEXT("WSAStartup failed: %s"), appGetSystemErrorMessage(Error) );
		Initialized = 1;
	}
	unguard;
}

#endif // PLATFORM_WIN32

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
