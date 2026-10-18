/*=============================================================================
	Null.cpp: A do-nothing audio subsystem (silent).

	Build 433's `UAudioSubsystem` with every call a no-op. The default audio
	driver is NOpenALDrv; this one is used when NOpenALDrv is not built, when
	it cannot be loaded natively, or when the ini names it.
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

	// UAudioSubsystem interface (HP2 build 433, Engine/Inc/UnAudio.h). Return values follow ALAudio's "nothing
	// played" answers: PlaySound -1 (script ignores it), PlayMusic 0 (MusicTrigger stores the handle and later
	// passes it to StopMusic, which must accept 0). USound::Audio stays NULL -- only a real driver sets it.
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
