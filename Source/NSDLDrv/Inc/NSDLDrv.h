#include "Engine.h"
#include "SDL2/SDL.h"

/*-----------------------------------------------------------------------------
	Defines.
-----------------------------------------------------------------------------*/

#ifdef NSDLDRV_EXPORTS
#define NSDLDRV_API DLL_EXPORT
#else
#define NSDLDRV_API DLL_IMPORT
#endif


/*-----------------------------------------------------------------------------
	UNSDLViewport.
-----------------------------------------------------------------------------*/

extern NSDLDRV_API UBOOL GTickDue;

class FNSDLInputTest;

//
// A SDL2 viewport.
//
class NSDLDRV_API UNSDLViewport : public UViewport
{
	DECLARE_CLASS( UNSDLViewport, UViewport, CLASS_Transient, NSDLDrv )
	NO_DEFAULT_CONSTRUCTOR( UNSDLViewport )

	// Constructors.
	UNSDLViewport( ULevel* InLevel, class UNSDLClient* InClient );

	// UObject interface.
	virtual void Destroy() override;

	// UViewport interface.
	virtual UBOOL Lock( FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData=NULL, INT* HitSize=0 );
	virtual void Unlock( UBOOL Blit );
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar );
	virtual void Repaint( UBOOL Blit );
	virtual void SetModeCursor();
	virtual void UpdateWindowFrame();
	virtual void OpenWindow( PTRINT ParentWindow, UBOOL Temporary, INT NewX, INT NewY, INT OpenX, INT OpenY );
	virtual void CloseWindow();
	virtual void UpdateInput( UBOOL Reset );
	virtual void MakeCurrent();
	virtual void MakeFullscreen( INT NewX, INT NewY, UBOOL UpdateProfile );
	virtual void* GetWindow();
	virtual void SetMouseCapture( UBOOL Capture, UBOOL Clip, UBOOL FocusOnly );
	virtual UBOOL IsFullscreen();
	virtual UBOOL ResizeViewport( DWORD BlitType, INT X=INDEX_NONE, INT Y=INDEX_NONE, INT ColorBytes=INDEX_NONE );

	// UPlayer interface.
	virtual void ReadInput( FLOAT DeltaSeconds );

	// UNSDLViewport interface.
	void SetClientSize( INT NewX, INT NewY, UBOOL UpdateProfile );
	void EndFullscreen();
	UBOOL TickInput(); // returns true if the viewport has requested death

private:
	// Static variables.
	static BYTE KeyMap[SDL_NUM_SCANCODES]; // SDL_Scancode -> EInputKey map
	static const BYTE MouseButtonMap[6]; // SDL_BUTTON_ -> EInputKey map
	static const BYTE JoyButtonMap[SDL_CONTROLLER_BUTTON_MAX]; // SDL_GameControllerButton -> EInputKey map
	static const BYTE JoyButtonMapUI[SDL_CONTROLLER_BUTTON_MAX];  // SDL_GameControllerButton -> EInputKey map for UI
	static const BYTE JoyAxisMap[SDL_CONTROLLER_AXIS_MAX]; // SDL_GameControllerAxis -> EInputKey map
	static const FLOAT JoyAxisDefaultScale[SDL_CONTROLLER_AXIS_MAX];
	static const SWORD JoyAxisPressThreshold = 8192;

	// Variables.
	class UNSDLClient* Client;
	SDL_Window* hWnd;
	SDL_Renderer* SDLRen; // for accelerated SoftDrv
	SDL_Texture* SDLTex; // for use with the above renderer
	DWORD SDLTexFormat;
	SDL_GLContext GLCtx; // for OpenGLDrv
	// The configured render device could not start (no GL window or context, or its Init failed): SoftDrv renders
	// for the rest of the run, in a window without GL. The ini keeps the configured device.
	UBOOL SoftwareFallback;
	UBOOL FallBackToSoftware();
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
	// No SDL renderer on the worker (SDL's is GLES-backed and its proxied eglCreateContext deadlocks the
	// engine thread): SoftDrv renders into this buffer and Unlock blits it to the OffscreenCanvas in 2D.
	BYTE* SoftFB;
	INT SoftFBBytes;
#endif
	UBOOL Destroyed;
	INT DisplayIndex;
	SDL_Rect DisplaySize;
	SWORD JoyAxis[SDL_CONTROLLER_AXIS_MAX];
	SWORD MoveStick[2];	// the left stick's X and Y before any dead zone: the move stick (TickMoveStick)
	void UpdatePadAxis( INT Axis, INT NewValue );	// one stick or trigger value, read from the pad's state
	void TickMoveStick( FLOAT DeltaTime );
	void ReleasePad();	// every pad key up and the axes zeroed (the pad went, or UseJoystick went off)
	FNSDLInputTest* InputTest;	// the input test channel (-INPUTTEST=), NULL without it
	friend class FNSDLInputTest;
	UBOOL QuitRequested;
public:
	INT HoldCount;
private:

	// Info saved during captures and fullscreen sessions.
	INT SavedX, SavedY;

	// UNSDLViewport private methods.
	static void InitKeyMap();
	UBOOL CauseInputEvent( INT iKey, EInputAction Action, FLOAT Delta=0.0 );
#if defined(__ANDROID__)
	// The touch overlay's input, folded into the engine's once per tick (NSDLAndroid.cpp); its move stick once a frame,
	// for the frame's length (ReadInput).
	void TickAndroidTouch();
	void ReadAndroidStick( FLOAT DeltaSeconds );
#endif
};

/*-----------------------------------------------------------------------------
	UNSDLClient.
-----------------------------------------------------------------------------*/

//
// SDL2 implementation of the client.
//
class NSDLDRV_API UNSDLClient : public UClient, public FNotifyHook
{
	DECLARE_CLASS( UNSDLClient, UClient, CLASS_Transient|CLASS_Config, NSDLDrv )

	// Configuration.
	INT DefaultDisplay;
	UBOOL StartupFullscreen;
	UBOOL BorderlessFullscreen;	// fullscreen is the display's own mode at its size; off: a mode change to FullscreenViewportX/Y
	UBOOL UseJoystick;
	UBOOL InvertY;
	UBOOL InvertV;
	FLOAT ScaleXYZ;
	FLOAT ScaleRUV;
	FLOAT DeadZoneXYZ;
	FLOAT DeadZoneRUV;
	FLOAT PadWalkTilt;	// the move stick out of its dead zone but tilted less than this (0..1) walks; 0: never

	// Constructors.
	UNSDLClient();
	// Registers the config options and sets their compiled defaults.
	void StaticConstructor();

	// UObject interface.
	virtual void Destroy() override;
	virtual void PostEditChange() override;
	virtual void ShutdownAfterError() override;

	// UClient interface.
	virtual void Init( UEngine* InEngine );
	virtual void ShowViewportWindows( DWORD ShowFlags, INT DoShow );
	virtual void EnableViewportWindows( DWORD ShowFlags, INT DoEnable );
	virtual void Poll();
	virtual UViewport* CurrentViewport();
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
	virtual void Tick();
	virtual void MakeCurrent( UViewport* InViewport );
	virtual UViewport* NewViewport( const FName Name );
	virtual void EndFullscreen();

	// UNSDLClient interface.
	void TryRenderDevice( UViewport* Viewport, const char* ClassName, UBOOL Fullscreen );
	const TArray<SDL_Rect>& GetDisplayResolutions();
	inline SDL_GameController* GetController() { return Controller; }
	// One pad: OpenPad opens the one at SDL device index DeviceIndex when none is open and UseJoystick is on; Held
	// gets the buttons already down at the connect, which SDL never reports (W3C indices; read on the web only).
	// ClosePad closes the pad when InstanceId is its joystick instance.
	UBOOL OpenPad( INT DeviceIndex, DWORD* Held );
	UBOOL ClosePad( INT InstanceId );
	inline const SDL_DisplayMode& GetDefaultDisplayMode() const { return DefaultDisplayMode; }

private:
	// Variables.
	SDL_GameController* Controller;
	SDL_DisplayMode DefaultDisplayMode;
	TArray<SDL_Rect> DisplayResolutions;
};
