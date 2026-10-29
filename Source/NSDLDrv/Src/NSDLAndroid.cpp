/*=============================================================================
	NSDLAndroid.cpp: the Android touch overlay's side of NSDLDrv.

	The on-screen overlay (Android/app/src/main/java/com/hp2/android/GameActivity.java, a View above SDL's
	surface) drives play in landscape: a move stick on the left half, a drag-look on the right half, and the
	CAST, JUMP and MENU buttons. While a menu is up (HP2's book, cut-scene console or vendor bar, the player's
	bShowMenu or the viewport's bShowWindowsMouse; published below) the overlay lets the touches through to SDL,
	which turns them into mouse events, and the viewport takes their absolute positions for the menu's cursor.

	The overlay runs on Android's UI thread, so it writes its intent here through JNI and
	UNSDLViewport::TickAndroidTouch (the engine thread) turns it into the CauseInputEvent calls a mouse and a
	pad would make, on keys DefUser.ini binds:
	  move stick    -> IK_JoyX / IK_JoyY       (aBaseX turn / aBaseY forward, the pad's left stick)
	  drag-look     -> IK_MouseX / IK_MouseY   (the mouse's own axes)
	  CAST          -> IK_LeftMouse            (AltFire: the spell, the vendor reply)
	  JUMP          -> IK_RightMouse           (jump, the broom boost, the duel spell cycle)
	  MENU          -> IK_Escape

	Gyro aiming (GyroAim.java, the launcher's switch and sensitivities) sends the view's turn in degrees; in play it
	becomes the mouse's axes in the camera's rotation units, apart from the mouse's sensitivity and inversion; while a
	menu is up, or over a load, it is dropped.
=============================================================================*/

#include <jni.h>
#include <atomic>
#include <mutex>
#include <vector>

#include "NSDLDrv.h"
#include "UnCon.h"

// The button ids GameActivity.Hp2TouchOverlayView sends.
enum EAndroidTouchButton
{
	ATB_Cast = 0,
	ATB_Jump = 1,
	ATB_Menu = 2,
};

// Menu or play, for the overlay: the engine thread writes it each tick, the UI thread reads it.
static std::atomic<bool> GAndroidMenuVisible( false );

// The move stick, held-absolute in [-1,1]; the UI thread writes, the engine thread reads.
static std::atomic<float> GAndroidMoveX( 0.f );
static std::atomic<float> GAndroidMoveY( 0.f );
static float GAndroidLastMoveX = 0.f;
static float GAndroidLastMoveY = 0.f;

// The drag-look, in pixels: the UI thread adds, the engine thread takes and clears each tick.
static std::atomic<float> GAndroidLookX( 0.f );
static std::atomic<float> GAndroidLookY( 0.f );

// Gyro aiming's turn in view degrees, to the right and up: the UI thread adds, the engine thread takes and clears it
// each tick.
static std::atomic<float> GAndroidGyroYaw( 0.f );
static std::atomic<float> GAndroidGyroPitch( 0.f );

// Button edges, a queue so a tap pressed and released between two ticks still fires.
struct FAndroidButtonEdge { INT Key; BYTE Action; };
static std::mutex GAndroidButtonMutex;
static std::vector<FAndroidButtonEdge> GAndroidButtonQueue;

static void AndroidQueueButton( INT Key, EInputAction Action )
{
	std::lock_guard<std::mutex> Lock( GAndroidButtonMutex );
	FAndroidButtonEdge Edge; Edge.Key = Key; Edge.Action = (BYTE)Action;
	GAndroidButtonQueue.push_back( Edge );
}

static inline void AndroidAtomicAdd( std::atomic<float>& Value, float Delta )
{
	float Old = Value.load( std::memory_order_relaxed );
	while( !Value.compare_exchange_weak( Old, Old + Delta, std::memory_order_relaxed ) ) {}
}

// The gyro's fractions of a rotation unit not applied yet (yaw, pitch), and with -UFRONTLOG its totals in play.
static FLOAT GAndroidGyroCarry[2] = { 0.f, 0.f };
static DOUBLE GAndroidGyroDegrees[2] = { 0.0, 0.0 };
static INT GAndroidGyroUnits[2] = { 0, 0 };
static FTime GAndroidGyroLastTime;
static UBOOL GAndroidGyroUnlogged = 0;
// The last tick's time: the turn sent while the game did not tick for half a second (a load) is not aim.
static FTime GAndroidGyroTickTime;

// Degrees as whole rotation units (65536 a turn, toward zero), the fraction carried to the next tick.
static INT AndroidGyroUnits( FLOAT Degrees, FLOAT& Carry )
{
	const FLOAT Units = Degrees * ( 65536.f / 360.f ) + Carry;
	const INT Whole = (INT)Units;
	Carry = Units - (FLOAT)Whole;
	return Whole;
}

// The axis value that turns exactly Units when the script turns UnitsPerAxis times the axis, cut to whole units: half
// a unit past it, so the cut lands on it.
static FLOAT AndroidGyroAxis( INT Units, FLOAT UnitsPerAxis )
{
	if( Units == 0 )
		return 0.f;
	return ( (FLOAT)Units + ( Units > 0 ? 0.5f : -0.5f ) ) / UnitsPerAxis;
}

// With -UFRONTLOG, the totals applied in play once the gyro has rested for a quarter of a second.
static void AndroidGyroNote( FLOAT YawDegrees, FLOAT PitchDegrees, INT YawUnits, INT PitchUnits )
{
	const FTime Now = appSeconds();
	if( YawDegrees != 0.f || PitchDegrees != 0.f )
	{
		GAndroidGyroDegrees[0] += YawDegrees;
		GAndroidGyroDegrees[1] += PitchDegrees;
		GAndroidGyroUnits[0] += YawUnits;
		GAndroidGyroUnits[1] += PitchUnits;
		GAndroidGyroLastTime = Now;
		GAndroidGyroUnlogged = 1;
	}
	else if( GAndroidGyroUnlogged && Now - GAndroidGyroLastTime >= 0.25f )
	{
		GAndroidGyroUnlogged = 0;
		if( GUfrontLog )
			debugf( NAME_Log, TEXT("Gyro: %.3f degrees right and %.3f up in play in all, %i yaw and %i pitch units"),
				GAndroidGyroDegrees[0], GAndroidGyroDegrees[1], GAndroidGyroUnits[0], GAndroidGyroUnits[1] );
	}
}

//
// The camera's turn for its mouse input: HP2's camera (the player's view target) adds SmoothMouseX * DeltaTime
// * CurrentSet.fRotSpeed to its yaw a tick, and the same of SmoothMouseY to its pitch. The property is looked up once
// per class; 0 when the view target has none (that camera takes no mouse input).
//
static FLOAT AndroidCameraRotSpeed( AActor* Camera )
{
	static UClass* CameraClass = NULL;
	static UStructProperty* SetProp = NULL;
	static UFloatProperty* RotSpeedProp = NULL;
	if( !Camera )
		return 0.f;
	if( Camera->GetClass() != CameraClass )
	{
		CameraClass = Camera->GetClass();
		SetProp = FindField<UStructProperty>( CameraClass, TEXT("CurrentSet") );
		RotSpeedProp = SetProp ? FindField<UFloatProperty>( SetProp->Struct, TEXT("fRotSpeed") ) : NULL;
		debugf( NAME_Init, TEXT("NSDLAndroid: view target %s: CurrentSet.fRotSpeed %s"), CameraClass->GetPathName(),
			RotSpeedProp ? TEXT("found") : TEXT("missing") );
	}
	if( !RotSpeedProp )
		return 0.f;
	return *(const FLOAT*)( (const BYTE*)Camera + SetProp->Offset + RotSpeedProp->Offset );
}

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_hp2_android_GameActivity_nativeIsMenuVisible( JNIEnv*, jclass )
{
	return GAndroidMenuVisible.load( std::memory_order_relaxed ) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_hp2_android_GameActivity_nativeTouchAxis( JNIEnv*, jclass, jint Axis, jfloat Value )
{
	// Axis 0 is sideways (turn), 1 forward.
	if( Axis == 0 )
		GAndroidMoveX.store( Value, std::memory_order_relaxed );
	else if( Axis == 1 )
		GAndroidMoveY.store( Value, std::memory_order_relaxed );
}

JNIEXPORT void JNICALL
Java_com_hp2_android_GameActivity_nativeTouchLook( JNIEnv*, jclass, jfloat DX, jfloat DY )
{
	AndroidAtomicAdd( GAndroidLookX, DX );
	AndroidAtomicAdd( GAndroidLookY, DY );
}

JNIEXPORT void JNICALL
Java_com_hp2_android_GameActivity_nativeGyroLook( JNIEnv*, jclass, jfloat YawDegrees, jfloat PitchDegrees )
{
	AndroidAtomicAdd( GAndroidGyroYaw, YawDegrees );
	AndroidAtomicAdd( GAndroidGyroPitch, PitchDegrees );
}

JNIEXPORT void JNICALL
Java_com_hp2_android_GameActivity_nativeTouchButton( JNIEnv*, jclass, jint Button, jboolean Down )
{
	INT Key = IK_None;
	switch( Button )
	{
	case ATB_Cast: Key = IK_LeftMouse;  break;
	case ATB_Jump: Key = IK_RightMouse; break;
	case ATB_Menu: Key = IK_Escape;     break;
	default: return;
	}
	AndroidQueueButton( Key, Down ? IST_Press : IST_Release );
}

} // extern "C"

//
// HP2's UWindow menus are up while the console's book is open, the cut-scene console shows or the vendor bar
// shows (what HPConsole draws them on). The script properties are read through the console's and the book's
// class, looked up once per class. The title level is play: its Start is a spell-cursor target.
//
static UBOOL AndroidConsoleMenuUp( UConsole* Console )
{
	static UClass* ConsoleClass = NULL;
	static UObjectProperty* BookProp = NULL;
	static UBoolProperty* CutConsoleProp = NULL;
	static UBoolProperty* VendorBarProp = NULL;
	static UClass* BookClass = NULL;
	static UBoolProperty* IsOpenProp = NULL;
	if( !Console )
		return 0;
	if( Console->GetClass() != ConsoleClass )
	{
		ConsoleClass   = Console->GetClass();
		BookProp       = FindField<UObjectProperty>( ConsoleClass, TEXT("menuBook") );
		CutConsoleProp = FindField<UBoolProperty>( ConsoleClass, TEXT("bShowCutConsole") );
		VendorBarProp  = FindField<UBoolProperty>( ConsoleClass, TEXT("bVendorBar") );
		debugf( NAME_Init, TEXT("NSDLAndroid: console %s: menuBook %s, bShowCutConsole %s, bVendorBar %s"), ConsoleClass->GetPathName(),
			BookProp ? TEXT("found") : TEXT("missing"), CutConsoleProp ? TEXT("found") : TEXT("missing"), VendorBarProp ? TEXT("found") : TEXT("missing") );
	}
	const BYTE* Base = (const BYTE*)Console;
	if( CutConsoleProp && ( *(const BITFIELD*)(Base + CutConsoleProp->Offset) & CutConsoleProp->BitMask ) )
		return 1;
	if( VendorBarProp && ( *(const BITFIELD*)(Base + VendorBarProp->Offset) & VendorBarProp->BitMask ) )
		return 1;
	// An object property holds a pointer, on a 4-byte boundary.
	UObject* Book = NULL;
	if( BookProp )
		appMemcpy( &Book, Base + BookProp->Offset, sizeof(Book) );
	if( !Book )
		return 0;
	if( Book->GetClass() != BookClass )
	{
		BookClass  = Book->GetClass();
		IsOpenProp = FindField<UBoolProperty>( BookClass, TEXT("bIsOpen") );
		debugf( NAME_Init, TEXT("NSDLAndroid: menu book %s: bIsOpen %s"), BookClass->GetPathName(), IsOpenProp ? TEXT("found") : TEXT("missing") );
	}
	return IsOpenProp && ( *(const BITFIELD*)((const BYTE*)Book + IsOpenProp->Offset) & IsOpenProp->BitMask );
}

//
// Fold the overlay's input into the engine's, once per tick on the engine thread.
//
void UNSDLViewport::TickAndroidTouch()
{
	guard(UNSDLViewport::TickAndroidTouch);

	// HP2's menus run on the game's own cursor, moved by mouse deltas under capture. With touch the cursor has to
	// be where the finger is: while a menu is up the viewport shows the windows mouse (UGameEngine::Tick then
	// releases the capture, and HPConsole takes the absolute WindowsMouseX/Y), and clears it when the menu goes
	// unless script set it (WindowConsole.LaunchUWindow, the vendor bar).
	static UBOOL MenuMouseSet = 0;
	const UBOOL ConsoleMenu = AndroidConsoleMenuUp( Console );
	if( ConsoleMenu && !bShowWindowsMouse )
	{
		bShowWindowsMouse = 1;
		MenuMouseSet = 1;
	}
	else if( !ConsoleMenu && MenuMouseSet )
	{
		bShowWindowsMouse = 0;
		MenuMouseSet = 0;
	}
	const UBOOL MenuUp = ( Actor && Actor->bShowMenu ) || bShowWindowsMouse;
	GAndroidMenuVisible.store( MenuUp ? true : false, std::memory_order_relaxed );

	// The button edges, taken out under the lock and sent without it.
	{
		std::vector<FAndroidButtonEdge> Edges;
		{
			std::lock_guard<std::mutex> Lock( GAndroidButtonMutex );
			Edges.swap( GAndroidButtonQueue );
		}
		for( size_t i = 0; i < Edges.size(); ++i )
			CauseInputEvent( Edges[i].Key, (EInputAction)Edges[i].Action );
	}

	// The drag-look as the mouse's axes (the SDL_MOUSEMOTION handler's sign convention).
	{
		const FLOAT LookX = GAndroidLookX.exchange( 0.f, std::memory_order_relaxed );
		const FLOAT LookY = GAndroidLookY.exchange( 0.f, std::memory_order_relaxed );
		if( LookX != 0.f )
			CauseInputEvent( IK_MouseX, IST_Axis, +LookX );
		if( LookY != 0.f )
			CauseInputEvent( IK_MouseY, IST_Axis, -LookY );
	}

	// The move stick as the pad's left stick: the value every tick while it is held, and one 0 when it is
	// released. The scale and signs are the pad path's (TickInput: ScaleXYZ, JoyAxisDefaultScale, InvertY,
	// per second); the overlay's Y is SDL's, down positive, as a pad's.
	{
		const FTime CurTime = appSeconds();
		const FLOAT DeltaTime = Clamp( (FLOAT)(CurTime - InputUpdateTime), 0.f, 0.25f );
		const FLOAT MoveX = GAndroidMoveX.load( std::memory_order_relaxed );
		const FLOAT MoveY = GAndroidMoveY.load( std::memory_order_relaxed );
		const FLOAT ScaleX = Client->ScaleXYZ * JoyAxisDefaultScale[SDL_CONTROLLER_AXIS_LEFTX] * DeltaTime;
		FLOAT ScaleY = Client->ScaleXYZ * JoyAxisDefaultScale[SDL_CONTROLLER_AXIS_LEFTY] * DeltaTime;
		if( Client->InvertY )
			ScaleY = -ScaleY;
		if( MoveX != 0.f || GAndroidLastMoveX != 0.f )
			CauseInputEvent( IK_JoyX, IST_Axis, MoveX * ScaleX );
		if( MoveY != 0.f || GAndroidLastMoveY != 0.f )
			CauseInputEvent( IK_JoyY, IST_Axis, MoveY * ScaleY );
		GAndroidLastMoveX = MoveX;
		GAndroidLastMoveY = MoveY;
	}

	// Gyro aiming, in play (no menu, the game ticking): whole rotation units of the camera as the mouse's axes, past the
	// mouse's sensitivity and inversion. The camera turns SmoothMouseX * DeltaTime * fRotSpeed units a tick, and
	// SmoothMouseX is the mouse's axis after ReadInput's 20 / DeltaTime and PlayerInput's MouseSensitivity * DesiredFOV
	// * 0.01111, averaged with the tick before (which keeps the total): 20 * MouseSensitivity * FOV scale * fRotSpeed
	// units per unit of the axis.
	{
		const FLOAT Yaw = GAndroidGyroYaw.exchange( 0.f, std::memory_order_relaxed );
		const FLOAT Pitch = GAndroidGyroPitch.exchange( 0.f, std::memory_order_relaxed );
		const FLOAT RotSpeed = Actor ? AndroidCameraRotSpeed( Actor->ViewTarget ) : 0.f;
		const FTime Now = appSeconds();
		const UBOOL Ticking = Now - GAndroidGyroTickTime < 0.5f;
		GAndroidGyroTickTime = Now;
		const UBOOL InPlay = Ticking && !MenuUp && Actor && RotSpeed > 0.f && Actor->MouseSensitivity > 0.f && Actor->DesiredFOV > 0.f;
		INT YawUnits = 0, PitchUnits = 0;
		if( InPlay )
		{
			const FLOAT UnitsPerAxis = 20.f * Actor->MouseSensitivity * Actor->DesiredFOV * 0.01111f * RotSpeed;
			YawUnits = AndroidGyroUnits( Yaw, GAndroidGyroCarry[0] );
			PitchUnits = AndroidGyroUnits( Pitch, GAndroidGyroCarry[1] );
			Actor->aMouseX += AndroidGyroAxis( YawUnits, UnitsPerAxis );
			Actor->aMouseY += AndroidGyroAxis( Actor->bInvertMouse ? -PitchUnits : PitchUnits, UnitsPerAxis );
		}
		else
			GAndroidGyroCarry[0] = GAndroidGyroCarry[1] = 0.f;
		AndroidGyroNote( InPlay ? Yaw : 0.f, InPlay ? Pitch : 0.f, YawUnits, PitchUnits );
	}

	unguard;
}
