/*=============================================================================
	Null.cpp: A do-nothing audio subsystem (silent).

	ufront step 2.7a (engine-ut99). ut99dc's shipped inis select
	`SoundDrv.NullAudioSubsystem`, but the `SoundDrv` package was never included
	in the ut99dc tree (only the Dreamcast `AICADrv` and the OSS-`/dev/dsp`
	`Audio` subsystem, neither usable on native-Linux/WASM). This provides the
	null subsystem the config already references — silent audio that works on
	every platform. Mirrors engine-unreal1's `SoundDrv/Null.cpp`.

	HP2: rewritten for build 433's own `UAudioSubsystem` (flags/fade/priority
	PlaySound, Ogg PlayMusic by name, ModifySound, fade-out StopSound). The
	real driver is NOpenALDrv (PLAN Phase E); this one is the bring-up and
	`-NOSOUND`-equivalent default.
=============================================================================*/

#include "Engine.h"
#include "UnRender.h"

/*-----------------------------------------------------------------------------
	UNullAudioSubsystem.
-----------------------------------------------------------------------------*/

//
// The null (silent) audio subsystem. Every method is a no-op; Init() reports
// success so the engine proceeds with a valid-but-silent audio device.
//
class DLL_EXPORT UNullAudioSubsystem : public UAudioSubsystem
{
	DECLARE_CLASS(UNullAudioSubsystem, UAudioSubsystem, CLASS_Config, SoundDrv)

	// Class registration hook (the ut99dc driver convention). No config
	// properties — silent audio needs none.
	static void InternalClassInitializer( UClass* Class ) {}

	// UAudioSubsystem interface (HP2 build 433, Engine/Inc/UnAudio.h). Return values follow the retail
	// ALAudio driver's "nothing played" answers: PlaySound -1 (script ignores it), PlayMusic 0 (MusicTrigger
	// stores the handle and later passes it to StopMusic, which must accept 0). USound::Audio stays NULL --
	// only a real driver sets it.
	virtual UBOOL Init() { return 1; }
	virtual void SetViewport( UViewport* Viewport ) { AudioViewport = Viewport; }
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog ) { return 0; }
	virtual void Update( FSceneNode* SceneNode ) {}
	virtual UBOOL RegisterSound( USound* Sound ) { return 0; }
	virtual void UnregisterSound( USound* Sound, INT Id=-1 ) {}
	virtual INT PlaySound( AActor* Actor, INT Id, USound* Sound, FVector Location, FLOAT Volume, FLOAT Radius, FLOAT Pitch, INT Flags, FLOAT FadeDuration, FLOAT Priority=0.f ) { return -1; }
	virtual UBOOL StopSound( AActor* Actor, USound* Sound ) { return 0; }
	virtual INT PlayMusic( FString Song, FLOAT FadeInTime ) { return 0; }
	virtual UBOOL StopMusic( INT SongHandle, FLOAT FadeOutTime ) { return 0; }
	virtual INT StopAllMusic( FLOAT FadeOutTime ) { return 0; }
	virtual void NoteDestroy( AActor* Actor ) {}
	virtual UViewport* GetViewport() { return AudioViewport; }
	virtual void CleanUp() {}
	virtual void PostRender( FSceneNode* Frame ) {}
	virtual UBOOL ModifySound( AActor* Actor, INT Id, USound* Sound, BYTE Parameter, FLOAT Value ) { return 0; }
	virtual UBOOL StopSound( AActor* Actor, INT Id, USound* Sound, FLOAT FadeOutTime=0.f ) { return 0; }
	virtual void RenderAudioGeometry( FSceneNode* Frame ) {}

private:
	UViewport* AudioViewport = NULL;
};

IMPLEMENT_CLASS(UNullAudioSubsystem);

/*-----------------------------------------------------------------------------
	Package.
-----------------------------------------------------------------------------*/

IMPLEMENT_PACKAGE(SoundDrv);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
