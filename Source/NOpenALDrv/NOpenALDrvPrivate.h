/*=============================================================================
	NOpenALDrvPrivate.h: HP2's OpenAL audio subsystem (NOpenALDrv.NOpenALAudioSubsystem).

	Source/ALAudio's UALAudioSubsystem on OpenAL 1.1 / openal-soft. The game logic -- voices, priorities,
	occlusion, fades, lip sync, ambient sounds, the per-Id stream bookkeeping on USound -- is ALAudio's; the
	device/EFX/HRTF setup and the streams (decoded in-tree and pumped from Update(), no thread) are the driver's.
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

// Streams: AL buffers per stream and bytes per buffer. One Update() refills every processed buffer, so four
// buffers are the cushion (32 KB = 0.74 s of 22.05 kHz mono XA, 0.19 s of 44.1 kHz stereo music). Both are
// config (StreamBuffers / StreamChunkSize, not in the shipped ini).
#define MAX_STREAM_BUFFERS         8
#define DEFAULT_STREAM_BUFFERS     4
#define DEFAULT_STREAM_CHUNKSIZE   32768

// Occlusion factor.
#define OCCLUSION_FACTOR 0.35f

// Priority modifier for playing sounds.
#define PLAYING_PRIORITY_MULTIPLIER 1.0f

// Conversion factor for Unreal units -> meter: ALAudio's (the game's radii are tuned against it).
#define DISTANCE_FACTOR ( 0.0254f / 2.f )

// Output rate when the ini has none.
#define DEFAULT_OUTPUT_RATE 44100

/*------------------------------------------------------------------------------------
	Lip-sync animation manager (ALAudioSubsystem.h, plus Involves/Detach).
------------------------------------------------------------------------------------*/

class FLipSyncAnimMgr
{
public:
	FLipSyncAnimMgr( AActor *ParentActor, FLipSyncData *LipSyncData );
	~FLipSyncAnimMgr( );

	UBOOL	Start( FLOAT fTime );
	void	Update( FLOAT fTime );
	void	EatPauseTime( FLOAT fDeltaTime );

	// NoteDestroy: the actor (or its mouth channel) is being destroyed -- forget both so the destructor does
	// not touch them.
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
	void*			Ogg;			// OggVorbis_File* (heap; opaque so only NOpenALDrv.cpp includes vorbisfile, under pack(8))
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
	// its UseEAX/CompatibilityMode device switches, plus OutputRate/UseHRTF/UseReverb and the stream sizes.
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
				Use3dHardware,		// read by HGame's Harry.uc only; no meaning here (ALAudio has no such key)
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
	UBOOL		OggUsable;			// libvorbisfile's struct layout matches ours (Init, VorbisLayout.c)
	FPointRegion RegionListener;

	// Channels.
	TArray<FNALSource>	Sources;
	TArray<FNALBuffer>	Buffers;
	TArray<FNALStream>	Streams;
	TArray<BYTE>		StreamScratch;	// one decoded chunk

	// AL specific.
	ALCdevice*	SoundDevice;
	ALCcontext* SoundContext;
	INT			ReverbKind;			// resolved at Init: 0 = EFX EAX reverb, 1 = none
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
