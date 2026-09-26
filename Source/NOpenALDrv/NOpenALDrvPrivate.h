/*=============================================================================
	NOpenALDrvPrivate.h: HP2's OpenAL audio subsystem (NOpenALDrv.NOpenALAudioSubsystem).

	HP2 433 port (PLAN Phase E): the retail driver Source/ALAudio (UALAudioSubsystem, OpenAL 1.0 + the closed
	`coda` EA-XA decoder + a Win32 streaming thread) ported to OpenAL 1.1 / openal-soft on the pattern of the
	template's NOpenALDrv (engine-ut99). The game logic -- voices, priorities, occlusion, fades, lip sync,
	ambient sounds, the per-Id stream bookkeeping on USound -- is ALAudio's; the device/EFX/HRTF setup and the
	"everything pumped from Update(), no thread" rule are the template's. The replacement table this follows
	line by line is docs/porting/analysis/template-drivers.md §11.3.
=============================================================================*/

#ifndef _INC_NOPENALDRVPRIVATE
#define _INC_NOPENALDRVPRIVATE

#include "EAXADecoder.h"

/*------------------------------------------------------------------------------------
	Constants (ALAudioSubsystem.h, unless noted).
------------------------------------------------------------------------------------*/

// Maximum number of audio channels (voices). The ini's Channels= is clamped to this.
#define MAX_AUDIOCHANNELS 32

// Maximum number of audio streams (every XA sound and every song plays as a stream).
#define MAX_AUDIOSTREAMS 32

// Maximum listener velocity in meters/ second.
#define MAX_LISTENER_VELOCITY 50

// Maximum source velocity in meters/ seconds.
#define MAX_SOURCE_VELOCITY 300

// Streams: AL buffers per stream and bytes per buffer. ALAudio had 3 x 32 KB (XA) / 3 x 64 KB (Ogg), filled
// by a 1 ms background thread; here one Update() refills EVERY processed buffer, so a fourth buffer is the
// cushion (4 x 32 KB = 0.74 s of 22.05 kHz mono XA, 0.19 s x 4 of 44.1 kHz stereo music). Both are config
// (StreamBuffers / StreamChunkSize, not in the shipped ini) for web tuning (§11.3).
#define MAX_STREAM_BUFFERS         8
#define DEFAULT_STREAM_BUFFERS     4
#define DEFAULT_STREAM_CHUNKSIZE   32768

// Occlusion factor.
#define OCCLUSION_FACTOR 0.35f

// Priority modifier for playing sounds.
#define PLAYING_PRIORITY_MULTIPLIER 1.0f

// Conversion factor for Unreal units -> meter. HP2's (the game's radii were tuned against it), not the
// template's DISTANCE_SCALE.
#define DISTANCE_FACTOR ( 0.0254f / 2.f )

// Output rate when the ini has none (template).
#define DEFAULT_OUTPUT_RATE 44100

/*------------------------------------------------------------------------------------
	Lip-sync animation manager (ALAudioSubsystem.h, verbatim + an initialised channel pointer).
------------------------------------------------------------------------------------*/

class FLipSyncAnimMgr
{
public:
	FLipSyncAnimMgr( AActor *ParentActor, FLipSyncData *LipSyncData );
	~FLipSyncAnimMgr( );

	UBOOL	Start( FLOAT fTime );
	void	Update( FLOAT fTime );
	void	EatPauseTime( FLOAT fDeltaTime );

	// HP2 port: the actor (or its mouth channel) is being destroyed -- forget both so the destructor does not
	// touch them (retail kept raw pointers past NoteDestroy).
	UBOOL	Involves( AActor* Actor ) const { return Actor && ( Actor == m_ParentActor || Actor == m_LipSyncAnimChan ); }
	void	Detach() { m_ParentActor = NULL; m_LipSyncAnimChan = NULL; }

protected:
	AActor*			m_ParentActor;
	FLipSyncData	*m_pLipSyncData;
	FLOAT			m_StartTime;
	FLOAT			m_LastElapsedTime;
	BYTE			m_LastAmp;
	AActor*			m_LipSyncAnimChan;

	FLipSyncAnimMgr() {};	// don't allow default construction
};

/*------------------------------------------------------------------------------------
	Voices, buffers, streams.
------------------------------------------------------------------------------------*/

// One OpenAL source (ALAudio's ALSource).
struct FNALSource
{
	USound*			Sound;
	ALuint			Source;
	AActor*			Actor;
	FVector			Location;
	FLOAT			Priority;
	FLOAT			Radius;
	FLOAT			ZoneRadius;
	FLOAT			UsedRadius;
	FLOAT			WantedRadius;
	FTime			LastChange;
	FLOAT			Volume;			// the sound's own volume, 0..1; SoundVolume/MusicVolume apply on top (live)
	FLOAT			FadeDuration;
	FLOAT			FadeTime;
	EFadeMode		FadeMode;
	INT				Flags;
	INT				Id;
	UBOOL			Started;
	UBOOL			Paused;
	FLipSyncAnimMgr	*LipSyncAnimMgr;
};

// One non-streaming sound's AL buffer (ALAudio's ALBuffer; the name lives on the USound).
struct FNALBuffer
{
	ALuint		Buffer;
	INT			Flags;
};

enum ENALStreamKind
{
	NALSTREAM_None	= 0,
	NALSTREAM_XA	= 1,	// EA-XA over USound::Data, decoded by FEAXADecoder
	NALSTREAM_Ogg	= 2,	// Ogg Vorbis file opened by path (music)
};

// The driver-owned stream that replaces GFileStream + its Win32 thread (ALAudio's ALStream + FFileStream's
// per-stream state). Decoded synchronously from Update(). Plain data: lives in a TArray, zeroed = free.
struct FNALStream
{
	INT				Kind;			// ENALStreamKind; NALSTREAM_None = free slot
	USound*			Sound;
	ALuint			Buffer[MAX_STREAM_BUFFERS];
	INT				NumBuffers;		// AL buffers generated
	INT				NumPrimed;		// of which filled at registration (a short sound needs fewer)
	INT				Flags;
	INT				Rate;
	INT				Channels;
	ALenum			Format;
	UBOOL			Looping;		// rewind at the end (SF_Looping, and every song)
	UBOOL			Eof;			// nothing more to queue
	INT				Underruns;
	FEAXADecoder	Xa;
	void*			Ogg;			// OggVorbis_File* (heap; see NOpenALDrv.cpp for why it is opaque here)
};

// An ambient sound candidate (ALAudio's ALAmbient).
struct FNALAmbient
{
	AActor*		Actor;
	FLOAT		Priority;
	DWORD		Flags;
	INT			Id;
};

/*------------------------------------------------------------------------------------
	UNOpenALAudioSubsystem.
------------------------------------------------------------------------------------*/

class DLL_EXPORT UNOpenALAudioSubsystem : public UAudioSubsystem
{
	DECLARE_CLASS(UNOpenALAudioSubsystem,UAudioSubsystem,CLASS_Config,NOpenALDrv)

	// Configuration -- [NOpenALDrv.NOpenALAudioSubsystem]. ALAudio's names with FLOAT 0..1 volumes (the HGame
	// options page reads and writes `ini:Engine.Engine.AudioDevice MusicVolume|SoundVolume` as floats) minus
	// its UseEAX/CompatibilityMode device switches, plus the template's OutputRate/UseHRTF/UseReverb.
	FLOAT		DopplerFactor,
				MusicVolume,
				SoundVolume,
				RollOff;
	INT			MaxChannels;		// "Channels"
	INT			OutputRate;
	INT			StreamBuffers;
	INT			StreamChunkSize;
	UBOOL		ReverseStereo,
				UsePrecache,
				Use3dHardware,		// read by HGame's Harry.uc only; no meaning here (retail had no such key)
				UseHRTF,
				UseReverb;

	// State.
	UViewport*	Viewport;
	FTime		LastTime;
	FVector		LastPosition;
	UBOOL		Initialized;
	UBOOL		LastRealtime;
	UBOOL		LastPaused;
	UBOOL		RestartSounds;
	UBOOL		OggUsable;			// libvorbisfile's struct layout matches ours (the A5 guard)
	FPointRegion RegionListener;

	// Channels.
	TArray<FNALSource>	Sources;
	TArray<FNALBuffer>	Buffers;
	TArray<FNALStream>	Streams;
	TArray<BYTE>		StreamScratch;	// one decoded chunk

	// AL specific.
	ALCdevice*	SoundDevice;
	ALCcontext* SoundContext;
	INT			ReverbKind;			// resolved at Init (template): 0 = EFX EAX reverb, 1 = none
	ALuint		ReverbEffect;
	ALuint		ReverbSlot;
	UBOOL		ReverbOn;
	AZoneInfo*	ReverbZone;

	// Counters for the log (first-of-a-kind lines, Exec NOPENALSTATS).
	INT			StatXaStreams;
	INT			StatOggStreams;
	INT			StatWavBuffers;
	INT			StatLipSync;
	INT			StatUnderruns;

	// Constructor.
	UNOpenALAudioSubsystem();
	void StaticConstructor();

	// UObject interface.
	void Destroy();
	void PostEditChange();
	void ShutdownAfterError();

	// UAudioSubsystem interface (HP2 433, Engine/Inc/UnAudio.h).
	UBOOL Init();
	void SetViewport( UViewport* Viewport );
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
	void Update( FSceneNode* SceneNode );
	UBOOL RegisterSound( USound* Sound );
	void UnregisterSound( USound* Sound, INT Id = -1 );
	INT PlaySound( AActor* Actor, INT Id, USound* Sound, FVector Location, FLOAT Volume, FLOAT Radius, FLOAT Pitch, INT Flags, FLOAT FadeDuration, FLOAT Priority = 0.f );
	UBOOL StopSound( AActor* Actor, USound* Sound );
	INT PlayMusic( FString Song, FLOAT FadeInTime );
	UBOOL StopMusic( INT SongHandle, FLOAT FadeOutTime );
	INT StopAllMusic( FLOAT FadeOutTime );
	void NoteDestroy( AActor* Actor );
	UViewport* GetViewport();
	void PostRender( FSceneNode* Frame ) {}
	UBOOL ModifySound( AActor* Actor, INT Id, USound * Sound, BYTE parameter, FLOAT Value );
	UBOOL StopSound  ( AActor* Actor, INT Id, USound * Sound, FLOAT fFadeOutTime = 0.f );
	void RenderAudioGeometry( FSceneNode* Frame ) {}

	// Internals (ALAudio's).
	void PauseSounds();
	void UnpauseSounds();
	void StopSoundSource( INT Index );
	void OldStopSound( INT Index );
	FLOAT SoundPriority( UViewport* Viewport, FVector Location, FLOAT Volume, FLOAT Radius, INT Flags );
	INT GetNewStreamStruct();
	UBOOL alError( const TCHAR* Text, UBOOL Log = true );
	INT FindLeastImportantSound( USound *pIncomingSound );

	// Internals (the port's).
	FLOAT SourceGain( INT Index, FLOAT Volume );
	void ApplyVolumes();
	UBOOL ResolveMusic( const FString& Song, FString& Path, void* OggOut );
	UBOOL OpenStream( FNALStream& Stream, USound* Sound, INT Flags );
	void DestroyStream( INT StreamIndex );
	INT FillStreamChunk( FNALStream& Stream, BYTE* Out, INT MaxBytes );
	UBOOL PumpStream( INT SourceIndex );
	FNALStream* StreamForSource( INT SourceIndex );
	INT ReclaimIdleStream();
	void UnloadSoundData( USound* Sound );
	void InitReverbEffect();
	void UpdateReverb( AZoneInfo* Zone );
};

#endif
