/*=============================================================================
	Launch.cpp: Game launcher.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Brandon Reinhart.
=============================================================================*/

#ifdef __EMSCRIPTEN__
// Before the engine headers: html5.h pulls in <time.h>, and UnFile.h #defines clock(). The page's visibility,
// for the frame wait.
#include <emscripten/html5.h>
#endif
#include "LaunchPrivate.h"
#ifdef PLATFORM_SDL
#include <SDL2/SDL.h>
#endif
#if defined(__ANDROID__)
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>

// stdout and stderr on logcat (tag HP2-stdio): both are a pipe that a thread reads line by line. GLog's own
// lines go to logcat directly (FOutputDeviceFile); this catches printf output, appThrowf's among it.
static void* AndroidStdioReader( void* Arg )
{
	const int Fd = (int)(intptr_t)Arg;
	char Buf[1024];
	size_t Len = 0;
	for( ;; )
	{
		const ssize_t N = read( Fd, Buf + Len, sizeof(Buf) - 1 - Len );
		if( N <= 0 )
			break;
		Len += (size_t)N;
		Buf[Len] = 0;
		char* Start = Buf;
		while( char* End = strchr( Start, '\n' ) )
		{
			*End = 0;
			if( End > Start )
				__android_log_write( ANDROID_LOG_INFO, "HP2-stdio", Start );
			Start = End + 1;
		}
		Len = strlen( Start );
		memmove( Buf, Start, Len + 1 );
		if( Len == sizeof(Buf) - 1 )
		{
			__android_log_write( ANDROID_LOG_INFO, "HP2-stdio", Buf );
			Len = 0;
		}
	}
	return NULL;
}
static void AndroidRedirectStdio()
{
	int Fds[2];
	if( pipe( Fds ) != 0 )
		return;
	setvbuf( stdout, NULL, _IOLBF, 0 );
	setvbuf( stderr, NULL, _IONBF, 0 );
	dup2( Fds[1], STDOUT_FILENO );
	dup2( Fds[1], STDERR_FILENO );
	close( Fds[1] );
	pthread_t Thread;
	pthread_attr_t Attr;
	pthread_attr_init( &Attr );
	pthread_attr_setdetachstate( &Attr, PTHREAD_CREATE_DETACHED );
	if( pthread_create( &Thread, &Attr, AndroidStdioReader, (void*)(intptr_t)Fds[0] ) != 0 )
		close( Fds[0] );
	pthread_attr_destroy( &Attr );
}
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/heap.h> // emscripten_get_heap_size (the pacing ring's heap column)
#include <unistd.h> // chdir

// Whether the page is hidden, for the frame wait (the worker has no document of its own).
EM_JS(void, ufront_set_hidden, (int hidden), {
    globalThis.ufrontHidden = !!hidden;
});

// A hidden page has no display frames: its frames run from timers.
static EM_BOOL UfrontVisibilityChange( int EventType, const EmscriptenVisibilityChangeEvent* Event, void* UserData )
{
	ufront_set_hidden( Event->hidden );
	return EM_TRUE;
}

// The per-frame wait. The engine thread returns to the worker's event loop every frame: that is when the worker's
// OffscreenCanvas is composited (the frame's present point) and when input and console commands arrive. It resumes
// at the display's next frame (the worker's requestAnimationFrame), or at the first display frame that reaches
// waitMs from now: frames that start on the display's frames take the browser's mouse motion (one coalesced event a
// display frame) one batch a frame, and are shown at an even pace. Where the worker has no requestAnimationFrame,
// while the page is hidden and with -TIMERFRAMES, a timer resumes it instead (waitMs, or 0 ms); a display frame
// that does not come within 100 ms (the page hidden meanwhile) gives way to a timer too. EM_ASYNC_JS imports match
// Asyncify's default `__asyncjs__*` list, so no ASYNCIFY_IMPORTS entry is needed.
EM_ASYNC_JS(void, ufront_wait_frame, (int onDisplayFrame, double waitMs), {
    const end = performance.now() + Math.max(0, waitMs);
    for (;;) {
        if (!onDisplayFrame || typeof requestAnimationFrame !== 'function' || globalThis.ufrontHidden) {
            const left = end - performance.now();
            await new Promise((resolve) => { setTimeout(resolve, Math.max(0, left)); });
            return;
        }
        await new Promise((resolve) => {
            const t = setTimeout(resolve, 100);
            requestAnimationFrame(() => { clearTimeout(t); resolve(); });
        });
        if (performance.now() >= end - 1)
            return;
    }
});

// -TIMERFRAMES (e.g. a benchmark): frames resume from timers alone. The first call, on the engine's thread, also
// starts following the page's visibility.
static INT UfrontOnDisplayFrame()
{
	static INT OnDisplayFrame = -1;
	if( OnDisplayFrame < 0 )
	{
		OnDisplayFrame = !ParseParam( appCmdLine(), TEXT("TIMERFRAMES") );
		debugf( NAME_Init, TEXT("ufront: frames start %s"), OnDisplayFrame ? TEXT("on the display's frames") : TEXT("from timers") );
		EmscriptenVisibilityChangeEvent Status;
		if( emscripten_get_visibility_status( &Status ) == EMSCRIPTEN_RESULT_SUCCESS )
			ufront_set_hidden( Status.hidden );
		emscripten_set_visibilitychange_callback( NULL, EM_TRUE, UfrontVisibilityChange );
	}
	return OnDisplayFrame;
}

// Wait until the next frame may start: the display's next frame (Period 0), or with a frame-rate cap a display
// frame, at most one a Period on average. Each frame's due time is the one before plus Period, so a cap the
// display's rate does not divide still averages the cap (some display frames are skipped); a frame later than its
// due time moves the schedule to itself. Returns the wait the cap asked for in ms, INDEX_NONE without a cap.
static INT UfrontWaitFrame( FLOAT Period )
{
	static FTime Due;
	if( Period <= 0.f )
	{
		ufront_wait_frame( UfrontOnDisplayFrame(), 0.0 );
		return INDEX_NONE;
	}
	const FLOAT WaitMs = Max( 0.f, Due - appSeconds() ) * 1000.f;
	ufront_wait_frame( UfrontOnDisplayFrame(), WaitMs );
	const FTime Then = appSeconds();
	Due = Due + Period;
	if( Then - Due > 0.f )
		Due = Then;
	return (INT)WaitMs;
}


// The running engine, for ue_exec_command and WebRTCDrv's listen-status push (set in InitEngine after
// Engine->Init(); WebRTCDrv.cpp declares it extern).
UEngine* GEngineForExec = NULL;

// The frame-pacing ring: the timings of the last UFRONT_PACE_MAX frames, read with `ufront_pacing` through
// ue_exec_command. A mean frame rate (CurrentTickRate) hides an uneven frame-interval distribution, so every
// frame is kept and the reader computes percentiles and judder.
//   TickMs     — Engine->Tick + GWindowManager->Tick: simulation and the whole render/submit path.
//   YieldMs    — the present point: the frame wait (UfrontWaitFrame).
//   SleepReqMs — what the cap asked for.
#define UFRONT_PACE_MAX 4096
struct FUfrontPaceSample
{
	FLOAT  Start;      // seconds since the main loop started
	FLOAT  TickMs;
	FLOAT  YieldMs;
	INT    SleepReqMs; // the wait the cap asked for; INDEX_NONE without a cap
	// wasm heap size: under -pthread a memory.grow rebuilds every thread's typed-array views, a stall suspect.
	INT    HeapMB;
};
static FUfrontPaceSample GUfrontPace[UFRONT_PACE_MAX];
static INT GUfrontPaceHead  = 0; // next write slot
static INT GUfrontPaceCount = 0; // total frames ever recorded (so the ring's fill level is known)

static void UfrontPaceRecord( FLOAT Start, FLOAT TickMs, FLOAT YieldMs, INT SleepReqMs )
{
	FUfrontPaceSample& S = GUfrontPace[GUfrontPaceHead];
	S.Start      = Start;
	S.TickMs     = TickMs;
	S.YieldMs    = YieldMs;
	S.SleepReqMs = SleepReqMs;
	S.HeapMB     = (INT)(emscripten_get_heap_size() / (1024*1024));
	GUfrontPaceHead = (GUfrontPaceHead+1) % UFRONT_PACE_MAX;
	GUfrontPaceCount++;
}

// Dump the ring as raw CSV, one header line and then one line per frame: the reader computes percentiles,
// judder and the sleep/yield split from the per-frame series.
static FString UfrontPaceDump()
{
	const INT N = Min(GUfrontPaceCount, (INT)UFRONT_PACE_MAX);
	const INT First = (GUfrontPaceHead - N + UFRONT_PACE_MAX) % UFRONT_PACE_MAX;
	FString Out = FString::Printf( TEXT("ufront_pacing v3 frames=%i total=%i now=%f\n"), N, GUfrontPaceCount, (DOUBLE)appSeconds().GetFloat() );
	Out += TEXT("start_s,tick_ms,yield_ms,sleep_req_ms,heap_mb\n");
	for( INT i=0; i<N; i++ )
	{
		const FUfrontPaceSample& S = GUfrontPace[(First+i) % UFRONT_PACE_MAX];
		Out += FString::Printf( TEXT("%f,%.3f,%.3f,%i,%i\n"), S.Start, S.TickMs, S.YieldMs, S.SleepReqMs, S.HeapMB );
	}
	return Out;
}

// Run a console command and return its captured output (the host calls this via ccall). The text is held in a
// static FString, so the returned pointer stays valid until the next call.
extern "C" char* EMSCRIPTEN_KEEPALIVE ue_exec_command( const char* Cmd )
{
	static FString Captured;
	Captured = TEXT("");
	// `ufront_pacing` is answered here, not by UEngine::Exec: the ring belongs to the launcher's main loop.
	// `ufront_pacing reset` empties it, e.g. after a map load.
	if( Cmd && appStrnicmp( Cmd, TEXT("ufront_pacing"), 13 )==0 )
	{
		if( appStrfind( (TCHAR*)Cmd, TEXT("reset") ) )
		{
			GUfrontPaceHead = GUfrontPaceCount = 0;
			Captured = TEXT("ufront_pacing reset\n");
		}
		else Captured = UfrontPaceDump();
		return (char*)(*Captured);
	}
	if( GEngineForExec && Cmd )
	{
		FStringOutputDevice Out;
		// What the engine itself does not take goes to the first viewport, as a line typed on the console would
		// (the render device's commands, the player's exec functions); a viewport runs commands once it has a player.
		UClient* Client = GEngineForExec->Client;
		if( !GEngineForExec->Exec( Cmd, Out ) && Client && Client->Viewports.Num() && Client->Viewports(0)->Actor )
			Client->Viewports(0)->Exec( Cmd, Out );
		Captured = Out;
	}
	return (char*)(*Captured);
}
#endif
/*-----------------------------------------------------------------------------
	Global variables.
-----------------------------------------------------------------------------*/

extern "C" {TCHAR THIS_PACKAGE[64]=TEXT("Launch");}

// Memory allocator.
#include "FMallocAnsi.h"
FMallocAnsi Malloc;

// Log file.
#include "FOutputDeviceFile.h"
FOutputDeviceFile Log;

// Error handler.
#include "FOutputDeviceAnsiError.h"
FOutputDeviceAnsiError Error;

// Feedback.
#include "FFeedbackContextAnsi.h"
FFeedbackContextAnsi Warn;

// File manager.
#include "FFileManagerLinux.h"
FFileManagerLinux FileManager;

// Config.
#include "FConfigCacheIni.h"



/*-----------------------------------------------------------------------------
	Initialization
-----------------------------------------------------------------------------*/

//
// Render-device selection, the same on native and web. Each -*RENDERER switch names a render device
// (NSDLViewport picks the GL context from the device name). SelectRenderDevice() takes a switch, else the
// persisted ini choice (a hardware default, or the user's earlier pick marked by ufrontUserRenderDevice),
// else GRenderPreference. A switch whose driver is not compiled in is ignored. PersistRenderChoiceIfSwitched()
// writes a switch that initialized back to the ini, so the pick sticks next boot.
//
struct FRenderSwitch { const TCHAR* Param; const TCHAR* Device; };
static const FRenderSwitch GRenderSwitches[] =
{
	{ TEXT("XOPENGLRENDERER"),   TEXT("XOpenGLDrv.XOpenGLRenderDevice") },
	{ TEXT("UTGLRRENDERER"),     TEXT("OpenGLDrv.OpenGLRenderDevice") },
	// NOpenGLESDrv: the GLES2 device. Opt-in only (not in GRenderPreference).
	{ TEXT("NOPENGLESRENDERER"), TEXT("NOpenGLESDrv.NOpenGLESRenderDevice") },
	// SoftDrv: the CPU rasterizer. Opt-in only (not in GRenderPreference).
	{ TEXT("SOFTWARERENDERER"),  TEXT("SoftDrv.SoftwareRenderDevice") },
	// NullDrv: a no-op render device with no GL context, for headless runs. Opt-in only.
	{ TEXT("NULLRENDERER"),      TEXT("NullDrv.NullRenderDevice") },
};
// Default when nothing is chosen: best HW first.
static const TCHAR* GRenderPreference[] =
{
	TEXT("XOpenGLDrv.XOpenGLRenderDevice"),
	TEXT("OpenGLDrv.OpenGLRenderDevice"),
};
// The class a -*RENDERER switch requested (empty if none) — persisted after a successful Init().
static FString GRequestedRenderDevice;

// True iff the named "Package.Class" is compiled into this build (render devices, viewport clients, audio
// subsystems).
static UBOOL IsPackageClassAvailable( const TCHAR* Device )
{
#ifdef __EMSCRIPTEN__
	// Web (one static binary, engine code built without exception catching): StaticFindObject looks only at
	// registered classes, and every compiled-in class registers at startup, so there is no file load and no
	// throw. The class' short name (after the last '.') is matched in ANY_PACKAGE.
	FString Path = Device;
	INT Dot = Path.InStr( TEXT("."), 1 );
	const TCHAR* ClassName = ( Dot != INDEX_NONE ) ? ((*Path) + Dot + 1) : (*Path);
	return UObject::StaticFindObject( UClass::StaticClass(), ANY_PACKAGE, ClassName ) != NULL;
#else
	// Native: a class is registered only once its package is loaded, and this runs before Engine->Init(), so
	// load the class by name (LOAD_NoWarn): a compiled-in one resolves, an absent one returns NULL. The base is
	// UObject because viewport clients and audio subsystems are asked too.
	return UObject::StaticLoadClass( UObject::StaticClass(), NULL, Device, NULL, LOAD_NoWarn, NULL ) != NULL;
#endif
}

#ifndef __EMSCRIPTEN__
// If the ini names a subsystem this build does not contain, substitute the first available entry of
// Preference. The engine loads these LOAD_NoFail, so a missing package is an appError before the first frame,
// and on a BUILD_STATIC target no package file beside the binary can supply it. A configured driver that is
// present is left alone. Not compiled on the web, where InitEngine sets every subsystem.
static void NormalizeSubsystem( const TCHAR* Section, const TCHAR* Key, const TCHAR* const* Preference, INT Num )
{
	FString Configured;
	if( GConfig->GetString( Section, Key, Configured ) && Configured.Len() && IsPackageClassAvailable( *Configured ) )
		return;	// what the user asked for is here — do not touch it

	for( INT i=0; i<Num; i++ )
	{
		if( IsPackageClassAvailable( Preference[i] ) )
		{
			debugf( TEXT("[%s] %s=%s is not in this build -> %s"),
				Section, Key, Configured.Len() ? *Configured : TEXT("(unset)"), Preference[i] );
			GConfig->SetString( Section, Key, Preference[i] );
			return;
		}
	}
	debugf( NAME_Warning, TEXT("[%s] %s=%s is not in this build and no replacement is either"),
		Section, Key, Configured.Len() ? *Configured : TEXT("(unset)") );
}
#endif

static void SetRenderDeviceConfig( const TCHAR* Device )
{
	GConfig->SetString( TEXT("Engine.Engine"), TEXT("GameRenderDevice"),     Device );
	GConfig->SetString( TEXT("Engine.Engine"), TEXT("WindowedRenderDevice"), Device );
	GConfig->SetString( TEXT("Engine.Engine"), TEXT("RenderDevice"),         Device );
}

// True iff Device is in GRenderPreference, i.e. may be adopted as an implicit default. SoftDrv is not: it is
// opt-in (-SOFTWARERENDERER), so an ini that names it does not make the CPU rasterizer the default.
static UBOOL IsHardwareDefaultEligible( const TCHAR* Device )
{
	for( INT i=0; i<ARRAY_COUNT(GRenderPreference); i++ )
		if( appStricmp( Device, GRenderPreference[i] )==0 )
			return 1;
	return 0;
}

// Choose the render device before Engine->Init().
static void SelectRenderDevice()
{
	// 1) A command-line -*RENDERER switch (first present wins).
	for( INT i=0; i<ARRAY_COUNT(GRenderSwitches); i++ )
	{
		if( ParseParam( appCmdLine(), GRenderSwitches[i].Param ) )
		{
			const TCHAR* Device = GRenderSwitches[i].Device;
			if( IsPackageClassAvailable( Device ) )
			{
				debugf( TEXT("-%s selects render device %s"), GRenderSwitches[i].Param, Device );
				SetRenderDeviceConfig( Device );
				GRequestedRenderDevice = Device; // persist iff it initializes (PersistRenderChoiceIfSwitched)
				return;
			}
			debugf( NAME_Warning, TEXT("render device %s not available (not compiled in this build) -> keeping the default; -%s ignored"), Device, GRenderSwitches[i].Param );
			break; // fall through to the default selection
		}
	}

	// 2) The persisted GameRenderDevice, if its driver is compiled in and it is a hardware default or the
	//    user's own pick.
	FString Persisted, UserSel;
	if( GConfig->GetString( TEXT("Engine.Engine"), TEXT("GameRenderDevice"), Persisted ) && Persisted.Len()>0 )
	{
		// PersistRenderChoiceIfSwitched writes the ufrontUserRenderDevice marker only after a -*RENDERER switch
		// initialized, so it tells a user's SoftDrv pick from an ini that merely names SoftDrv.
		const UBOOL bUserSelected =
			GConfig->GetString( TEXT("Engine.Engine"), TEXT("ufrontUserRenderDevice"), UserSel )
			&& UserSel.Len()>0 && appStricmp( *Persisted, *UserSel )==0;
		if( IsPackageClassAvailable( *Persisted ) && ( IsHardwareDefaultEligible( *Persisted ) || bUserSelected ) )
		{
			debugf( TEXT("using the persisted %s render device %s"), bUserSelected ? TEXT("user-selected") : TEXT("hardware"), *Persisted );
			SetRenderDeviceConfig( *Persisted );
			return;
		}
		debugf( NAME_Warning, TEXT("persisted render device %s is not an eligible hardware default (unavailable or software opt-in) -> preference order"), *Persisted );
	}

	// 3) Preference order — first compiled-in device wins.
	for( INT i=0; i<ARRAY_COUNT(GRenderPreference); i++ )
	{
		if( IsPackageClassAvailable( GRenderPreference[i] ) )
		{
			debugf( TEXT("default render device %s (preference order)"), GRenderPreference[i] );
			SetRenderDeviceConfig( GRenderPreference[i] );
			return;
		}
	}
	// 4) No hardware device is compiled in: the software rasterizer, rather than the retail ini's
	//    D3DDrv.D3DRenderDevice, which the engine loads LOAD_NoFail and would abort on.
	if( IsPackageClassAvailable( TEXT("SoftDrv.SoftwareRenderDevice") ) )
	{
		debugf( NAME_Warning, TEXT("no hardware render device is compiled into this build -> SoftDrv.SoftwareRenderDevice") );
		SetRenderDeviceConfig( TEXT("SoftDrv.SoftwareRenderDevice") );
		return;
	}
	debugf( NAME_Warning, TEXT("no known render device is compiled into this build!") );
}

// After Engine->Init(): persist a -*RENDERER switch's device so it sticks next boot. The render device itself
// initializes later (at level bring-up), so this checks only that Init did not rewrite GameRenderDevice to a
// fallback.
static void PersistRenderChoiceIfSwitched()
{
	if( GRequestedRenderDevice.Len()==0 )
		return;
	FString Active;
	if( GConfig->GetString( TEXT("Engine.Engine"), TEXT("GameRenderDevice"), Active ) && appStricmp( *Active, *GRequestedRenderDevice )==0 )
	{
		// Mark it as the user's pick, so SelectRenderDevice honours it next boot even when it is not a
		// hardware default (SoftDrv).
		GConfig->SetString( TEXT("Engine.Engine"), TEXT("ufrontUserRenderDevice"), *GRequestedRenderDevice );
		GConfig->Flush( 0 );
		debugf( TEXT("persisted render device %s as a user selection (sticks next boot)"), *GRequestedRenderDevice );
	}
	else
	{
		debugf( NAME_Warning, TEXT("render device changed from %s during init -> not persisted"), *GRequestedRenderDevice );
	}
}

//
// Creates a UEngine object.
//
static UEngine* InitEngine()
{
	guard(InitEngine);
	FTime LoadTime = appSeconds();

	// Set exec hook.
	GExec = NULL;

#ifdef __EMSCRIPTEN__
	// The web driver set, forced before Engine->Init() loads the subsystems whatever the ini names (the retail
	// Default.ini names WinDrv/ALAudio/D3DDrv). Set in code, so the game's own inis stay untouched.
	GConfig->SetString( TEXT("Engine.Engine"), TEXT("ViewportManager"),      TEXT("NSDLDrv.NSDLClient") );
	// Audio: NOpenALDrv, HP2's ALAudio ported to openal-soft (its web backend here): EA-XA sounds decoded in the
	// driver, Ogg Vorbis music opened by path, streams pumped from Update(); the ini's [NOpenALDrv...] section
	// carries ALAudio's float volumes.
#if UE_HAS_NOPENALDRV
	GConfig->SetString( TEXT("Engine.Engine"), TEXT("AudioDevice"),          TEXT("NOpenALDrv.NOpenALAudioSubsystem") );
#else
	// A build without NOpenALDrv is silent. InitAudio loads the AudioDevice class LOAD_NoFail, so it must name a
	// driver that is compiled in.
	GConfig->SetString( TEXT("Engine.Engine"), TEXT("AudioDevice"),          TEXT("SoundDrv.NullAudioSubsystem") );
#endif
	// A browser has no BSD sockets: the net driver is WebRTCDrv, whose transport is the glue's ue_net_* imports.
	GConfig->SetString( TEXT("Engine.Engine"), TEXT("NetworkDevice"),        TEXT("WebRTCDrv.WebRTCNetDriver") );
	// UGameEngine::Tick captures the pointer while a level is up and no menu shows; CaptureMouse=True keeps
	// UGameEngine::Click from releasing it on every mouse-button release.
	GConfig->SetString( TEXT("NSDLDrv.NSDLClient"),  TEXT("CaptureMouse"),    TEXT("True") );
	// GameInfo's stat logs (bWorldLog/bLocalLog) end in AStatLog's appCreateProc(), a process launch, and a
	// browser has no processes.
	GConfig->SetString( TEXT("Engine.GameInfo"),           TEXT("bWorldLog"), TEXT("False") );
	GConfig->SetString( TEXT("Engine.GameInfo"),           TEXT("bLocalLog"), TEXT("False") );
#endif

#ifndef __EMSCRIPTEN__
	// Native: the ini can name subsystems this build does not contain (the retail Default.ini names
	// WinDrv.WindowsClient and ALAudio.ALAudioSubsystem); each is replaced by the first available entry of
	// its list. A build without NOpenALDrv lands on SoundDrv's silent subsystem, because InitAudio loads the
	// AudioDevice LOAD_NoFail.
	{
		static const TCHAR* const ViewportPreference[] = { TEXT("NSDLDrv.NSDLClient") };
		static const TCHAR* const AudioPreference[]    = { TEXT("NOpenALDrv.NOpenALAudioSubsystem"),
		                                                   TEXT("SoundDrv.NullAudioSubsystem") };
		static const TCHAR* const NetPreference[]      = { TEXT("IpDrv.TcpNetDriver") };
		NormalizeSubsystem( TEXT("Engine.Engine"), TEXT("ViewportManager"), ViewportPreference, ARRAY_COUNT(ViewportPreference) );
		NormalizeSubsystem( TEXT("Engine.Engine"), TEXT("AudioDevice"),     AudioPreference,    ARRAY_COUNT(AudioPreference) );
		NormalizeSubsystem( TEXT("Engine.Engine"), TEXT("NetworkDevice"),   NetPreference,      ARRAY_COUNT(NetPreference) );
	}
#endif
#if defined(__ANDROID__)
	// GameInfo's stat logs end in AStatLog's appCreateProc(), a fork/exec of a batcher an app does not carry.
	GConfig->SetString( TEXT("Engine.GameInfo"),           TEXT("bWorldLog"), TEXT("False") );
	GConfig->SetString( TEXT("Engine.GameInfo"),           TEXT("bLocalLog"), TEXT("False") );
	// XOpenGLDrv on the GLES 3 context NSDLViewport makes: an ES shader profile, whatever a Game.ini derived
	// from the retail Default.ini (which has no XOpenGLDrv section, so the device's own default, Core) says.
	// A player's ES stays.
	{
		FString Profile;
		GConfig->GetString( TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("OpenGLVersion"), Profile );
		if( appStricmp( *Profile, TEXT("ES") )!=0 && appStricmp( *Profile, TEXT("ES_WEBGL") )!=0 )
		{
			debugf( TEXT("[XOpenGLDrv.XOpenGLRenderDevice] OpenGLVersion=%s -> ES_WEBGL (GLES 3 context)"), *Profile );
			GConfig->SetString( TEXT("XOpenGLDrv.XOpenGLRenderDevice"), TEXT("OpenGLVersion"), TEXT("ES_WEBGL") );
		}
	}
#endif

	// The render device, before Engine->Init() loads the subsystems.
	SelectRenderDevice();

	// HP2: the game's debug mode (on-screen cut-scene/debug messages, the dev console, fast-forward, the level list
	// in the credits page) is the globalconfig bool [HGame.baseConsole] bDebugMode, which HGame reads when it loads.
	// Retail ships it True in Default.ini; here it is a command-line option, set on every boot (so a value written
	// to Game.ini by an earlier session never leaks into the next): -DEBUGMODE turns it on, otherwise it is off.
	{
		const UBOOL bDebugMode = ParseParam( appCmdLine(), TEXT("DEBUGMODE") );
		GConfig->SetBool( TEXT("HGame.baseConsole"), TEXT("bDebugMode"), bDebugMode );
		debugf( NAME_Init, TEXT("HP2 debug mode %s (-DEBUGMODE)"), bDebugMode ? TEXT("on") : TEXT("off") );
	}

	// Update first-run. HP2 433 reads the stamp first (UnEngineWin.h InitEngine); its [FirstRun] Reconfig /
	// ForceSoftware keys drive the Win32 D3D detection dialogs and are ignored here.
	INT FirstRun=0;
	GConfig->GetInt( TEXT("FirstRun"), TEXT("FirstRun"), FirstRun );
	if( FirstRun<ENGINE_VERSION )
	{
		debugf( NAME_Init, TEXT("First run of this build (FirstRun=%i -> %i)"), FirstRun, ENGINE_VERSION );
		FirstRun = ENGINE_VERSION;
	}
	GConfig->SetInt( TEXT("FirstRun"), TEXT("FirstRun"), FirstRun );

	// Create the global engine object.
	UClass* EngineClass;
	EngineClass = UObject::StaticLoadClass(
		UGameEngine::StaticClass(), NULL, 
		TEXT("ini:Engine.Engine.GameEngine"), 
		NULL, LOAD_NoFail, NULL 
	);
	UEngine* Engine = ConstructObject<UEngine>( EngineClass );
	// Empty ServerActors on every platform. HP2's retail ini lists IpDrv.UdpBeacon, IpServer.UdpServerQuery,
	// three IpServer.UdpServerUplink and UWeb.WebServer; IpServer and UWeb do not exist in HP2, and ULevel::Listen
	// loads each LOAD_NoFail, so any listen would abort. Done after ConstructObject (the array comes from the class
	// default object, which reads its config at package bind, so clearing GConfig would be too late) and before
	// Init(), whose first Browse() may listen.
	if( Engine->IsA(UGameEngine::StaticClass()) )
		((UGameEngine*)Engine)->ServerActors.Empty();
	Engine->Init();
	// Persist a -*RENDERER switch's device for the next boot.
	PersistRenderChoiceIfSwitched();
#ifdef __EMSCRIPTEN__
	// Expose the running engine to ue_exec_command and WebRTCDrv.
	GEngineForExec = Engine;
#endif
	debugf( TEXT("Startup time: %f seconds."), appSeconds()-LoadTime ); // FTime - FTime -> FLOAT

	return Engine;
	unguard;
}

//
// Handle an error.
//
void HandleError( const char* Exception )
{
	GIsGuarded=0;
	GIsCriticalError=1;
	debugf( NAME_Exit, "Shutting down after catching exception" );
	debugf( NAME_Exit, "Exiting due to exception" );
	GErrorHist[ARRAY_COUNT(GErrorHist)-1]=0;
#ifdef PLATFORM_SDL
	// HP2: a modal box under Xvfb/CI (or SDL's dummy/offscreen video) blocks forever on an invisible dialog.
	const char* VideoDriver = getenv( "SDL_VIDEODRIVER" );
	const UBOOL Headless = ParseParam( appCmdLine(), TEXT("NOMSGBOX") )
		|| ( VideoDriver && ( !appStricmp( VideoDriver, "dummy" ) || !appStricmp( VideoDriver, "offscreen" ) ) );
	if( !Headless )
		SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, LocalizeError("Critical"), GErrorHist, SDL_GetKeyboardFocus() );
#endif
}

/*-----------------------------------------------------------------------------
	Main Loop
-----------------------------------------------------------------------------*/

//
// X game message loop.
//
static void MainLoop( UEngine* Engine )
{
	guard(MainLoop);
	check(Engine);

	// Loop while running.
	GIsRunning = 1;
	// HP2 433: appSeconds() returns FTime (32.32 fixed point). FTime - FTime yields FLOAT seconds and is exact
	// for frame-sized deltas; FTime has >, + and - but no <, so every comparison is written with >.
	FTime OldTime = appSeconds();
	FTime SecondStartTime = OldTime;
#ifdef __EMSCRIPTEN__
	const FTime PaceEpoch = OldTime;     // pacing-ring timestamps are FLOAT seconds since the loop started
#endif
	INT TickCount = 0;
	while( GIsRunning && !GIsRequestingExit )
	{
#ifdef __EMSCRIPTEN__
		// Declared outside the guard() blocks below: each opens its own scope, and EnforceTickRate (where the
		// yield happens) reads what UpdateWorld sets.
		FTime PaceFrameStart, PaceTickEnd;
#endif
		// Update the world.
		guard(UpdateWorld);
		FTime  NewTime   = appSeconds();
		FLOAT  DeltaTime = NewTime - OldTime;
		Engine->Tick( DeltaTime );
		if( GWindowManager )
			GWindowManager->Tick( DeltaTime );
#ifdef __EMSCRIPTEN__
		PaceFrameStart = NewTime;
		PaceTickEnd    = appSeconds(); // end of simulate+render, start of the yield
#endif
		OldTime = NewTime;
		TickCount++;
		if( OldTime > SecondStartTime + 1.f )
		{
			Engine->CurrentTickRate = (FLOAT)TickCount / (OldTime - SecondStartTime);
			SecondStartTime = OldTime;
			TickCount = 0;
		}
		unguard;

		// Enforce optional maximum tick rate.
		guard(EnforceTickRate);
		FLOAT MaxTickRate = Engine->GetMaxTickRate();

		// The client frame-rate cap, native and web. GetMaxTickRate() is 0 in standalone play, and ULevel::Tick
		// floors DeltaSeconds (UnLevTic.cpp), so above that floor's frame rate the game runs faster than real
		// time. A net/demo cap (GetMaxTickRate>0) wins; otherwise [Engine.Engine] FrameRateCap.
		FLOAT Cap = MaxTickRate;
		if( Cap<=0.0 )
		{
			static FLOAT ClientCap = -1.f;
			if( ClientCap < -0.5f )
			{
				FLOAT V=0.f;
				// Read from the ini only (-ini:System:[Engine.Engine]:FrameRateCap=<n> sets it per launch); default
				// 120, <=0 means uncapped.
				ClientCap = ( GConfig->GetFloat(TEXT("Engine.Engine"),TEXT("FrameRateCap"),V) ) ? V : 120.f;
				debugf( NAME_Init, TEXT("ufront: client frame rate cap %.0f fps%s"), ClientCap, ClientCap>0.f ? TEXT("") : TEXT(" (uncapped)") );
			}
			Cap = ClientCap;
		}
#ifdef __EMSCRIPTEN__
		// The browser event loop must run every frame, so the loop waits through Asyncify until the next frame may
		// start (UfrontWaitFrame). Not emscripten_set_main_loop: its simulate_infinite_loop unwind collides with
		// main()'s try/catch.
		{
			const INT SleepReqMs = UfrontWaitFrame( Cap>0.f ? (1.f/Cap) : 0.f );
			// One pacing sample per frame, recorded after the wait so YieldMs covers the present point.
			const FTime PaceEnd = appSeconds();
			UfrontPaceRecord( PaceFrameStart-PaceEpoch, (PaceTickEnd-PaceFrameStart)*1000.f, (PaceEnd-PaceTickEnd)*1000.f, SleepReqMs );
		}
#else
		if( Cap>0.0 )
		{
			FLOAT Delta = (1.f/Cap) - (appSeconds()-OldTime);
			appSleep( Max(0.f,Delta) );
		}
#endif
		unguard;
	}
	GIsRunning = 0;

	unguard;
}

/*-----------------------------------------------------------------------------
	Main.
-----------------------------------------------------------------------------*/

//
// Exit wound.
//
int CleanUpOnExit( int ErrorLevel, UEngine* Engine )
{
	// HP2 retail (Launch.cpp): detach audio from the viewport before the subsystems shut down. Skipped after
	// a crash (Engine==NULL) -- do not touch audio then.
	if( Engine && Engine->Audio )
		Engine->Audio->SetViewport( NULL );
	GFileManager->Delete(TEXT("Running.ini"),0,0);
	debugf( NAME_Title, LocalizeGeneral("Exit") );
	appPreExit();
	GIsGuarded = 0;

	// Shutdown.
	appExit();
	GIsStarted = 0;

	return ErrorLevel;
}

//
// Entry point. On Android SDL_main (below) runs this on a thread of its own.
//
#if defined(__ANDROID__)
static int HP2Main( int argc, char* argv[] )
#else
int main( int argc, char* argv[] )
#endif
{
	try
	{
	guard(main);
	
	INT ErrorLevel = 0;
	GIsStarted	   = 1;
	// Set module name. Always "Game" (Game.log), whatever argv[0] is: an absolute path natively, and
	// "./this.program" under Emscripten, which would give /System/this.program.log instead of /System/Game.log.
	appStrcpy( GModule, TEXT("Game") );

	// Set the package name.
	appStrcpy( THIS_PACKAGE, appPackage() );	
	// Get the command line: up to 4096 characters (Core's GCmdLine); a token that does not fit is dropped whole,
	// with a warning once the log is up.
	TCHAR CmdLine[4096], *CmdLinePtr=CmdLine;
	*CmdLinePtr = 0;
	UBOOL CmdLineCut = 0;
	for( INT i=1; i<argc; i++ )
	{
		if( appStrlen(CmdLine) + appStrlen(argv[i]) + 2 >= ARRAY_COUNT(CmdLine) )
		{
			CmdLineCut = 1;
			break;
		}
		if( i>1 )
			appStrcat( CmdLine, " " );
		appStrcat( CmdLine, argv[i] );
	}
	// Init core.
	GIsClient = 1;
	GIsGuarded = 0;
#if defined(UE_ANDROID_APP)
	// The app's bridge has made the game folder's System/ the working directory, and appBaseDir() reads it
	// from the bridge's HP2_ANDROID_BASEDIR (SDL_GetBasePath() is NULL on Android).
#elif !defined(__EMSCRIPTEN__)
	// HP2: natively, run from the executable's directory (System/). HP2's appInit derives MapPath from the
	// base dir's "System" component and every Paths= entry is ../-relative; Core's appBaseDir() is
	// SDL_GetBasePath() already, and the cwd has to agree for the relative package paths.
	{
		char* ExeDir = SDL_GetBasePath();
		if( ExeDir )
		{
			if( chdir( ExeDir ) != 0 )
				fprintf( stderr, "Game: cannot chdir to %s\n", ExeDir );
			SDL_free( ExeDir );
		}
	}
#else
	// Web: HP2 runs from its System/ dir like a native install. Default.ini is preloaded at /System and the
	// package Paths (../System, ../Maps, ...) resolve against appBaseDir(), which is getcwd() here; chdir
	// before appInit() caches it, so the base dir is "/System/".
	chdir( "/System" );
#endif
	appInit( TEXT("Game"), CmdLine, &Malloc, &Log, &Error, &Warn, &FileManager, FConfigCacheIni::Factory, 1 );
	if( CmdLineCut )
		debugf( NAME_Warning, TEXT("The command line is longer than %i characters: the arguments after '%s' were dropped"), ARRAY_COUNT(CmdLine), CmdLine + Max(0, appStrlen(CmdLine) - 40) );

	// Init mode.
	GIsServer		= 1;
	GIsClient		= !ParseParam(appCmdLine(), TEXT("SERVER"));
	GIsEditor		= 0;
	GIsScriptable	= 1;
	GLazyLoad		= !GIsClient || ParseParam(appCmdLine(), TEXT("LAZY"));
		// Init console log.
	if (ParseParam(CmdLine, TEXT("LOG")))
	{
			Warn.AuxOut = GLog;
			GLog		= &Warn;
	}

	// Init engine.
	UEngine* Engine = InitEngine();
	if( Engine )
	{
		debugf( NAME_Title, LocalizeGeneral("Run") );

		// Optionally Exec and exec file.
		FString Temp;
		if( Parse(CmdLine, TEXT("EXEC="), Temp) )
		{
			Temp = FString(TEXT("exec ")) + Temp;
			if( Engine->Client && Engine->Client->Viewports.Num() && Engine->Client->Viewports(0) )
				Engine->Client->Viewports(0)->Exec( *Temp, *GLog );
		}

		// Start main engine loop.
		debugf( TEXT("Entering main loop.") );
		if ( !GIsRequestingExit )
			MainLoop( Engine );
	}

	// Finish up.
	return CleanUpOnExit( ErrorLevel, Engine );

	unguard;
	}
	catch (...)
	{
		// Chained abort.  Do cleanup.
		HandleError(NULL);
		return CleanUpOnExit( 1, NULL );
	}
}

#if defined(__ANDROID__)
//
// Android: SDLActivity loads libHP2.so and calls SDL_main on its SDL thread, whose stack is Android's default
// 1 MB. The engine runs on a thread of its own with the 8 MB a Linux main thread has (the script VM and the
// level code recurse deeply); SDL_main waits for it. SDL's Android calls attach whichever thread makes them.
//
struct FHP2MainArgs { int argc; char** argv; int Result; };
static void* HP2MainThread( void* Arg )
{
	FHP2MainArgs* Args = (FHP2MainArgs*)Arg;
	Args->Result = HP2Main( Args->argc, Args->argv );
	return NULL;
}
extern "C" __attribute__((visibility("default"))) int SDL_main( int argc, char** argv )
{
	AndroidRedirectStdio();
	FHP2MainArgs Args = { argc, argv, 1 };
	pthread_attr_t Attr;
	pthread_attr_init( &Attr );
	pthread_attr_setstacksize( &Attr, 8 * 1024 * 1024 );
	pthread_t Thread;
	if( pthread_create( &Thread, &Attr, HP2MainThread, &Args ) != 0 )
	{
		__android_log_write( ANDROID_LOG_ERROR, "HP2", "could not start the engine thread: running it on the SDL thread" );
		Args.Result = HP2Main( argc, argv );
	}
	else
		pthread_join( Thread, NULL );
	pthread_attr_destroy( &Attr );
	return Args.Result;
}
#endif
