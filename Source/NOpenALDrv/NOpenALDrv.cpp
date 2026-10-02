/*=============================================================================
	NOpenALDrv.cpp: HP2's OpenAL audio subsystem (NOpenALDrv.NOpenALAudioSubsystem).

	Source/ALAudio's UALAudioSubsystem as the retail ALAudio.dll runs it (voices, priorities, fades, ambient
	sounds, lip sync, the PlayMusic/StopMusic handle contract, the Exec commands; no occlusion) on openal-soft,
	with EFX resolved at run time and a stream table decoded synchronously from Update() (no streaming thread):
	EA-XA sounds through FEAXADecoder, Ogg Vorbis music through vorbisfile over GFileManager.
=============================================================================*/

#include <stdlib.h>

#include "AL/al.h"
#include "AL/alc.h"
#include "AL/alext.h"
// EFX is resolved at run time and never linked: the pointers live in `Efx` below, so a core OpenAL 1.1
// without EFX still links and plays everything but reverb.
#if defined(__has_include)
#  if __has_include("AL/efx.h")
#    define NOPENAL_HAVE_EFX_HEADERS 1
#  endif
#else
#  define NOPENAL_HAVE_EFX_HEADERS 1
#endif
#if NOPENAL_HAVE_EFX_HEADERS
#include "AL/efx.h"
#include "AL/efx-presets.h"
#else
#define NOPENAL_NO_EAX 1
typedef void (AL_APIENTRY *LPALGENEFFECTS)( ALsizei, ALuint* );
typedef void (AL_APIENTRY *LPALDELETEEFFECTS)( ALsizei, const ALuint* );
typedef void (AL_APIENTRY *LPALEFFECTI)( ALuint, ALenum, ALint );
typedef void (AL_APIENTRY *LPALEFFECTF)( ALuint, ALenum, ALfloat );
typedef void (AL_APIENTRY *LPALEFFECTFV)( ALuint, ALenum, const ALfloat* );
typedef void (AL_APIENTRY *LPALGENAUXILIARYEFFECTSLOTS)( ALsizei, ALuint* );
typedef void (AL_APIENTRY *LPALDELETEAUXILIARYEFFECTSLOTS)( ALsizei, const ALuint* );
typedef void (AL_APIENTRY *LPALAUXILIARYEFFECTSLOTI)( ALuint, ALenum, ALint );
#define AL_EFFECT_TYPE                           0x8001
#define AL_EFFECT_NULL                           0x0000
#define AL_EFFECTSLOT_EFFECT                     0x0001
#define AL_EFFECTSLOT_NULL                       0x0000
#define AL_FILTER_NULL                           0x0000
#define AL_AUXILIARY_SEND_FILTER                 0x20006
#endif
// openal-soft's output-mode / HRTF extensions, spelled out for older alext.h headers.
#ifndef ALC_HRTF_SOFT
#define ALC_HRTF_SOFT 0x1992
#endif
#ifndef ALC_OUTPUT_MODE_SOFT
#define ALC_OUTPUT_MODE_SOFT 0x19AC
#endif
#ifndef ALC_ANY_SOFT
#define ALC_ANY_SOFT 0x19AD
#endif
#ifndef ALC_STEREO_HRTF_SOFT
#define ALC_STEREO_HRTF_SOFT 0x19B2
#endif

#include "Engine.h"
#include "UnRender.h"

// libvorbisfile, built without the engine's -fpack-struct=4 (NOpenALDrv/CMakeLists.txt), so its structs must
// be seen here at natural alignment: OggVorbis_File has ogg_int64_t/double members, 8-aligned on wasm32.
// Init() cross-checks the size with the library's own (VorbisLayout.c).
#if SUPPORTS_PRAGMA_PACK
#pragma pack (push,8)
#endif
#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"
#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

#include "NOpenALDrvPrivate.h"

extern "C" int NOpenALDrv_VorbisFileSize( void );

/*-----------------------------------------------------------------------------
	Package / class.
-----------------------------------------------------------------------------*/

IMPLEMENT_PACKAGE(NOpenALDrv);
IMPLEMENT_CLASS(UNOpenALAudioSubsystem);

/*-----------------------------------------------------------------------------
	EFX entry points.
-----------------------------------------------------------------------------*/

enum { NAL_REVERB_EAX=0, NAL_REVERB_NONE=1 };

static struct FNOpenALEfx
{
	LPALGENEFFECTS                 GenEffects;
	LPALDELETEEFFECTS              DeleteEffects;
	LPALEFFECTI                    Effecti;
	LPALEFFECTF                    Effectf;
	LPALEFFECTFV                   Effectfv;
	LPALGENAUXILIARYEFFECTSLOTS    GenAuxiliaryEffectSlots;
	LPALDELETEAUXILIARYEFFECTSLOTS DeleteAuxiliaryEffectSlots;
	LPALAUXILIARYEFFECTSLOTI       AuxiliaryEffectSloti;
	UBOOL                          Present;
} Efx;

// Resolve them. Call with the context current: alGetProcAddress is context-dependent.
static UBOOL ResolveEfx( ALCdevice* InDevice )
{
	appMemzero( &Efx, sizeof(Efx) );
	if( !alcIsExtensionPresent( InDevice, "ALC_EXT_EFX" ) )
		return 0;
	Efx.GenEffects                 = (LPALGENEFFECTS)                 alGetProcAddress( "alGenEffects" );
	Efx.DeleteEffects              = (LPALDELETEEFFECTS)              alGetProcAddress( "alDeleteEffects" );
	Efx.Effecti                    = (LPALEFFECTI)                    alGetProcAddress( "alEffecti" );
	Efx.Effectf                    = (LPALEFFECTF)                    alGetProcAddress( "alEffectf" );
	Efx.Effectfv                   = (LPALEFFECTFV)                   alGetProcAddress( "alEffectfv" );
	Efx.GenAuxiliaryEffectSlots    = (LPALGENAUXILIARYEFFECTSLOTS)    alGetProcAddress( "alGenAuxiliaryEffectSlots" );
	Efx.DeleteAuxiliaryEffectSlots = (LPALDELETEAUXILIARYEFFECTSLOTS) alGetProcAddress( "alDeleteAuxiliaryEffectSlots" );
	Efx.AuxiliaryEffectSloti       = (LPALAUXILIARYEFFECTSLOTI)       alGetProcAddress( "alAuxiliaryEffectSloti" );
	// All or nothing: a partial table would fault on the first missing one, at a random later moment.
	Efx.Present = Efx.GenEffects && Efx.DeleteEffects && Efx.Effecti && Efx.Effectf && Efx.Effectfv
		&& Efx.GenAuxiliaryEffectSlots && Efx.DeleteAuxiliaryEffectSlots && Efx.AuxiliaryEffectSloti;
	return Efx.Present;
}

/*-----------------------------------------------------------------------------
	Helpers.
-----------------------------------------------------------------------------*/

// Unreal space (left-handed, Z up, units) -> OpenAL space (right-handed, meters): negate Z.
static inline FVector ToALSpace( const FVector& V )
{
	return FVector( V.X * DISTANCE_FACTOR, V.Y * DISTANCE_FACTOR, -V.Z * DISTANCE_FACTOR );
}

// vorbisfile I/O over an FArchive from GFileManager, so the music path goes through the engine's file manager
// (FFileManagerLinux folds '\' and resolves case per component) instead of a raw fopen.
static size_t NalOggRead( void* Ptr, size_t Size, size_t Count, void* Source )
{
	FArchive* Ar = (FArchive*)Source;
	if( !Size )
		return 0;
	INT Avail = Ar->TotalSize() - Ar->Tell();
	INT Want  = (INT)( Size * Count );
	if( Want > Avail )
		Want = Avail - Avail % (INT)Size;
	if( Want <= 0 )
		return 0;
	Ar->Serialize( Ptr, Want );
	return Ar->IsError() ? 0 : (size_t)Want / Size;
}
static int NalOggSeek( void* Source, ogg_int64_t Offset, int Whence )
{
	FArchive* Ar = (FArchive*)Source;
	ogg_int64_t Pos = Whence == SEEK_SET ? Offset : Whence == SEEK_CUR ? Ar->Tell() + Offset : Ar->TotalSize() + Offset;
	if( Pos < 0 || Pos > Ar->TotalSize() )
		return -1;
	Ar->Seek( (INT)Pos );
	return Ar->IsError() ? -1 : 0;
}
static int NalOggClose( void* Source )
{
	delete (FArchive*)Source;
	return 0;
}
static long NalOggTell( void* Source )
{
	return ((FArchive*)Source)->Tell();
}

/*------------------------------------------------------------------------------------
	UNOpenALAudioSubsystem.
------------------------------------------------------------------------------------*/

UNOpenALAudioSubsystem::UNOpenALAudioSubsystem()
{
	guard(UNOpenALAudioSubsystem::UNOpenALAudioSubsystem);
	// ALAudio's shipped values (Config/System/Default.ini [ALAudio.ALAudioSubsystem]) plus the driver's own
	// keys. The C++ constructor runs after the class defaults are copied in, so these are what an instance
	// starts with; Init() then LoadConfig()s the ini on top.
	DopplerFactor	= 1.f;
	MusicVolume		= 0.5f;
	SoundVolume		= 0.9f;
	RollOff			= 1.f;
	MaxChannels		= 32;
	OutputRate		= DEFAULT_OUTPUT_RATE;
	StreamBuffers	= DEFAULT_STREAM_BUFFERS;
	StreamChunkSize	= DEFAULT_STREAM_CHUNKSIZE;
	ReverseStereo	= 0;
	UsePrecache		= 0;
	Use3dHardware	= 0;
	UseHRTF			= 0;
	UseReverb		= 0;

	Viewport		= NULL;
	LastTime		= appSeconds();
	LastPosition	= FVector(0,0,0);
	Initialized		= 0;
	LastRealtime	= 0;
	LastPaused		= 0;
	AmbientTimer	= 0.f;
	RestartSounds	= 0;
	Muted			= 0;
	OggUsable		= 0;
	SoundDevice		= NULL;
	SoundContext	= NULL;
	ReverbKind		= NAL_REVERB_NONE;
	ReverbEffect	= 0;
	ReverbSlot		= 0;
	ReverbOn		= 0;
	ReverbZone		= NULL;
	StatXaStreams = StatOggStreams = StatWavBuffers = StatLipSync = StatUnderruns = 0;
	appMemzero( &RegionListener, sizeof(RegionListener) );
	unguard;
}

void UNOpenALAudioSubsystem::StaticConstructor()
{
	guard(UNOpenALAudioSubsystem::StaticConstructor);
	// ALAudio's names (the HGame options page reads/writes them through ini:Engine.Engine.AudioDevice), minus
	// UseEAX/CompatibilityMode (Win32 device switches). Use3dHardware: HGame's Harry.uc asks for it.
	new(GetClass(),TEXT("UsePrecache"),		RF_Public)UBoolProperty  (CPP_PROPERTY(UsePrecache     ), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("ReverseStereo"),	RF_Public)UBoolProperty  (CPP_PROPERTY(ReverseStereo   ), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("Channels"),		RF_Public)UIntProperty   (CPP_PROPERTY(MaxChannels     ), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("MusicVolume"),		RF_Public)UFloatProperty (CPP_PROPERTY(MusicVolume     ), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("SoundVolume"),		RF_Public)UFloatProperty (CPP_PROPERTY(SoundVolume     ), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("DopplerFactor"),	RF_Public)UFloatProperty (CPP_PROPERTY(DopplerFactor   ), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("RollOff"),			RF_Public)UFloatProperty (CPP_PROPERTY(RollOff         ), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("Use3dHardware"),	RF_Public)UBoolProperty  (CPP_PROPERTY(Use3dHardware   ), TEXT("Audio"), CPF_Config );
	// The driver's own: the frontend's HRTF toggle writes -ini:System:[NOpenALDrv.NOpenALAudioSubsystem]:UseHRTF=.
	new(GetClass(),TEXT("OutputRate"),		RF_Public)UIntProperty   (CPP_PROPERTY(OutputRate      ), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("UseHRTF"),			RF_Public)UBoolProperty  (CPP_PROPERTY(UseHRTF         ), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("UseReverb"),		RF_Public)UBoolProperty  (CPP_PROPERTY(UseReverb       ), TEXT("Audio"), CPF_Config );
	// Stream tuning (not in the shipped ini -- the defaults are the constructor's).
	new(GetClass(),TEXT("StreamBuffers"),	RF_Public)UIntProperty   (CPP_PROPERTY(StreamBuffers   ), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("StreamChunkSize"),	RF_Public)UIntProperty   (CPP_PROPERTY(StreamChunkSize ), TEXT("Audio"), CPF_Config );
	unguard;
}

/*------------------------------------------------------------------------------------
	UObject Interface.
------------------------------------------------------------------------------------*/

void UNOpenALAudioSubsystem::PostEditChange()
{
	guard(UNOpenALAudioSubsystem::PostEditChange);

	// Validate configurable variables.
	SoundVolume = Clamp(SoundVolume,0.f,1.f);
	MusicVolume = Clamp(MusicVolume,0.f,1.f);

	// Apply the volumes to the playing voices too, not only to the next sound.
	ApplyVolumes();

	unguard;
}

void UNOpenALAudioSubsystem::Destroy()
{
	guard(UNOpenALAudioSubsystem::Destroy);
	if( Initialized )
	{
		// Unhook.
		USound::Audio = NULL;

		for( INT i=0; i<Sources.Num(); i++ )
			StopSoundSource( i );

		for( TObjectIterator<USound> SoundIt; SoundIt; ++SoundIt )
			if( SoundIt->IsRegistered() || SoundIt->OpenStreams.Num() )
				UnregisterSound( *SoundIt );

		for( INT i=0; i<Streams.Num(); i++ )
			if( Streams(i).Kind != NALSTREAM_None )
				DestroyStream( i );
		Streams.Empty();

		for( INT i=0; i<Sources.Num(); i++ )
			alDeleteSources( 1, &Sources(i).Source );
		Sources.Empty();

		for( INT i=0; i<Buffers.Num(); i++ )
			if( Buffers(i).Buffer )
				alDeleteBuffers( 1, &Buffers(i).Buffer );
		Buffers.Empty();

		if( ReverbKind != NAL_REVERB_NONE && Efx.Present )
		{
			Efx.DeleteAuxiliaryEffectSlots( 1, &ReverbSlot );
			Efx.DeleteEffects( 1, &ReverbEffect );
		}

		Viewport = NULL;
		Initialized = 0;
		debugf( NAME_Exit, TEXT("NOpenALDrv: audio subsystem shut down.") );
	}
	if( SoundContext )
	{
		alcMakeContextCurrent( NULL );
		alcDestroyContext( SoundContext );
		SoundContext = NULL;
	}
	if( SoundDevice )
	{
		alcCloseDevice( SoundDevice );
		SoundDevice = NULL;
	}
	Super::Destroy();
	unguard;
}

void UNOpenALAudioSubsystem::ShutdownAfterError()
{
	guard(UNOpenALAudioSubsystem::ShutdownAfterError);
	if( Initialized )
	{
		// Unhook, and silence what is playing so a crash dialog is not accompanied by a stuck loop.
		USound::Audio = NULL;
		for( INT i=0; i<Sources.Num(); i++ )
			alSourceStop( Sources(i).Source );
		debugf( NAME_Exit, TEXT("UNOpenALAudioSubsystem::ShutdownAfterError") );
	}
	Super::ShutdownAfterError();
	unguard;
}

/*------------------------------------------------------------------------------------
	Init.
------------------------------------------------------------------------------------*/

UBOOL UNOpenALAudioSubsystem::Init()
{
	guard(UNOpenALAudioSubsystem::Init);

	if( Initialized )
		return 1;

	// The engine ConstructObject()s us and never LoadConfig()s the instance, and the C++ constructor above ran
	// after the class defaults were copied in -- so apply the [NOpenALDrv.NOpenALAudioSubsystem] ini here.
	LoadConfig();
	SoundVolume		= Clamp( SoundVolume, 0.f, 1.f );
	MusicVolume		= Clamp( MusicVolume, 0.f, 1.f );
	if( OutputRate <= 0 )
		OutputRate = DEFAULT_OUTPUT_RATE;
	if( DopplerFactor < 0.f )
		DopplerFactor = 0.f;
	if( RollOff <= 0.0f )
		RollOff = 1.0f;
	StreamBuffers	= Clamp( StreamBuffers, 2, MAX_STREAM_BUFFERS );
	StreamChunkSize	= Clamp( StreamChunkSize, 4096, 262144 ) & ~3;	// whole 16-bit stereo frames

	if( MaxChannels <= 0 )
	{
		debugf( NAME_Init, TEXT("NOpenALDrv: Channels=%i -- no audio"), MaxChannels );
		return 0;
	}

	SoundDevice = alcOpenDevice( NULL );
	if( !SoundDevice )
	{
		debugf( NAME_Init, TEXT("NOpenALDrv: no OpenAL device (alcOpenDevice: %04x)"), alcGetError( NULL ) );
		return 0;
	}

	// ALC_HRTF_SOFT is the attribute key (not the ALC_SOFT_HRTF presence macro), and HRTF is a stereo
	// rendering, so UseHRTF also asks for the stereo-HRTF output mode. Off by default: the player's choice
	// (the frontend's "Headphone audio"), no browser can tell headphones from speakers.
	const ALCint AttrList[] =
	{
		ALC_FREQUENCY, OutputRate,
		ALC_HRTF_SOFT, UseHRTF ? ALC_TRUE : ALC_FALSE,
		ALC_OUTPUT_MODE_SOFT, UseHRTF ? ALC_STEREO_HRTF_SOFT : ALC_ANY_SOFT,
		0
	};
	SoundContext = alcCreateContext( SoundDevice, AttrList );
	if( !SoundContext )
	{
		debugf( NAME_Init, TEXT("NOpenALDrv: context creation failed (%04x)"), alcGetError( SoundDevice ) );
		alcCloseDevice( SoundDevice );
		SoundDevice = NULL;
		return 0;
	}
	alcMakeContextCurrent( SoundContext );
	if( alError( TEXT("Init") ) )
	{
		debugf( NAME_Init, TEXT("NOpenALDrv: makecurrent failed.") );
		return 0;
	}

	// Reverb: EFX's EAX reverb when the OpenAL library has EFX, otherwise none. UseReverb defaults off:
	// ALAudio's EAX path is compiled out and no map carries I3DL2 data; with it on, the two bReverbZone maps
	// get a generic reverb.
	ResolveEfx( SoundDevice );
	ReverbKind = ( UseReverb && Efx.Present ) ? NAL_REVERB_EAX : NAL_REVERB_NONE;
#if NOPENAL_NO_EAX
	ReverbKind = NAL_REVERB_NONE;
#endif

	// Initialize channels.
	Sources.Empty();
	for( INT i=0; i<Min(MaxChannels, MAX_AUDIOCHANNELS); i++ )
	{
		ALuint sid = 0;
		alGenSources( 1, &sid );
		if( alError( TEXT("Init (creating sources)"), false ) )
			break;
		INT Index = Sources.AddZeroed( 1 );
		Sources(Index).Source = sid;
	}
	if( !Sources.Num() )
	{
		debugf( NAME_Init, TEXT("NOpenALDrv: couldn't allocate sources") );
		return 0;
	}

	// Initialize streams (all free: Kind == NALSTREAM_None).
	Streams.Empty( MAX_AUDIOSTREAMS );
	Streams.AddZeroed( MAX_AUDIOSTREAMS );
	StreamScratch.Empty();
	StreamScratch.Add( StreamChunkSize );

	// ALAudio's distance model: DS3D's, with the reference distance = the sound's radius (PlaySound).
	alDistanceModel( AL_INVERSE_DISTANCE_CLAMPED );
	alDopplerFactor( DopplerFactor );

	if( ReverbKind != NAL_REVERB_NONE )
	{
		Efx.GenEffects( 1, &ReverbEffect );
		InitReverbEffect();
		Efx.GenAuxiliaryEffectSlots( 1, &ReverbSlot );
		Efx.AuxiliaryEffectSloti( ReverbSlot, AL_EFFECTSLOT_EFFECT, AL_EFFECT_NULL );
	}
	ReverbOn	= 0;
	ReverbZone	= NULL;

	// libvorbisfile was compiled at natural alignment and we include its headers under pack(8); an
	// OggVorbis_File of another size would be overrun by ov_open. Music is disabled (SFX still play) rather
	// than corrupt the heap.
	OggUsable = NOpenALDrv_VorbisFileSize() == (int)sizeof(OggVorbis_File);
	if( !OggUsable )
		debugf( NAME_Warning, TEXT("NOpenALDrv: sizeof(OggVorbis_File) is %i here but %i in libvorbisfile -- struct packing mismatch, music disabled"),
			(INT)sizeof(OggVorbis_File), NOpenALDrv_VorbisFileSize() );

	Initialized		= 1;
	LastRealtime	= 0;
	LastPaused		= 0;
	LastTime		= appSeconds();

	// Initialized.
	USound::Audio = this;

	// Read back what the library did: a request is not a result.
	ALCint HRTFActive = 0, Frequency = 0;
	alcGetIntegerv( SoundDevice, ALC_HRTF_SOFT, 1, &HRTFActive );
	alcGetIntegerv( SoundDevice, ALC_FREQUENCY, 1, &Frequency );
	const char* DevName = alcGetString( SoundDevice, ALC_DEVICE_SPECIFIER );
	const char* Version = alGetString( AL_VERSION );
	debugf( NAME_Init, TEXT("NOpenALDrv: OpenAL %s on '%s', %i Hz, %i channels, streams %ix%i bytes, HRTF %s (UseHRTF=%s), reverb %s, vorbisfile %s"),
		ANSI_TO_TCHAR( Version ? Version : "?" ), ANSI_TO_TCHAR( DevName ? DevName : "?" ), Frequency, Sources.Num(),
		StreamBuffers, StreamChunkSize, HRTFActive ? TEXT("ACTIVE") : TEXT("inactive"), UseHRTF ? TEXT("True") : TEXT("False"),
		ReverbKind != NAL_REVERB_NONE ? TEXT("on (EFX EAX reverb)") : UseReverb ? TEXT("off (no EFX in this OpenAL)") : TEXT("off (UseReverb=False)"),
		OggUsable ? TEXT("ok") : TEXT("LAYOUT MISMATCH") );
	debugf( NAME_Init, TEXT("NOpenALDrv: SoundVolume=%1.2f MusicVolume=%1.2f DopplerFactor=%1.2f RollOff=%1.2f ReverseStereo=%s"),
		SoundVolume, MusicVolume, DopplerFactor, RollOff, ReverseStereo ? TEXT("True") : TEXT("False") );
	return 1;

	unguard;
}

/*------------------------------------------------------------------------------------
	Viewport.
------------------------------------------------------------------------------------*/

void UNOpenALAudioSubsystem::SetViewport( UViewport* InViewport )
{
	guard(UNOpenALAudioSubsystem::SetViewport);

	// Stop playing sounds -- the music too: retail dropped the prototype's "&& !(Flags & SF_Music)", so a map
	// change (UGameEngine::LoadMap calls SetViewport(GetViewport())) or a device switch ends the song.
	for( INT i=0; i<Sources.Num(); i++ )
		if( Sources(i).Started )
			StopSoundSource( i );

	if( Viewport != InViewport )
	{
		// Switch viewports.
		Viewport = InViewport;

		// Set listener region to 0.
		appMemzero( &RegionListener, sizeof(RegionListener) );

		// Nothing is registered ahead of play, as in the retail ALAudio; the UsePrecache config key stays, unread.
	}

	unguard;
}

UViewport* UNOpenALAudioSubsystem::GetViewport()
{
	guard(UNOpenALAudioSubsystem::GetViewport);
	return Viewport;
	unguard;
}

/*------------------------------------------------------------------------------------
	Streams (the GFileStream replacement).
------------------------------------------------------------------------------------*/

// Point a free FNALStream at a sound: XA over USound::Data, or an Ogg file by path (USound::Filename).
UBOOL UNOpenALAudioSubsystem::OpenStream( FNALStream& Stream, USound* Sound, INT Flags )
{
	guard(UNOpenALAudioSubsystem::OpenStream);

	appMemzero( &Stream, sizeof(Stream) );
	Stream.Sound	= Sound;
	Stream.Flags	= Flags;
	Stream.Looping	= (Flags & (SF_Looping | SF_Music)) != 0;

	if( appStricmp( *Sound->FileType, TEXT("ogg") ) == 0 )
	{
		if( !OggUsable )
			return 0;
		FArchive* Ar = GFileManager->CreateFileReader( *Sound->Filename );
		if( !Ar )
		{
			debugf( NAME_Warning, TEXT("NOpenALDrv: cannot open %s"), *Sound->Filename );
			return 0;
		}
		ov_callbacks Callbacks;
		Callbacks.read_func  = NalOggRead;
		Callbacks.seek_func  = NalOggSeek;
		Callbacks.close_func = NalOggClose;
		Callbacks.tell_func  = NalOggTell;
		OggVorbis_File* Ogg = new OggVorbis_File;
		appMemzero( Ogg, sizeof(OggVorbis_File) );
		const int Result = ov_open_callbacks( Ar, Ogg, NULL, 0, Callbacks );
		if( Result != 0 )
		{
			// On failure vorbisfile does not close the data source.
			debugf( NAME_Warning, TEXT("NOpenALDrv: %s is not Ogg Vorbis (ov_open %i)"), *Sound->Filename, Result );
			delete Ogg;
			delete Ar;
			return 0;
		}
		vorbis_info* Info = ov_info( Ogg, -1 );
		if( !Info || Info->channels < 1 || Info->channels > 2 || Info->rate <= 0 )
		{
			debugf( NAME_Warning, TEXT("NOpenALDrv: %s: unsupported Vorbis stream (%i channels)"), *Sound->Filename, Info ? Info->channels : 0 );
			ov_clear( Ogg );
			delete Ogg;
			return 0;
		}
		Stream.Kind		= NALSTREAM_Ogg;
		Stream.Ogg		= Ogg;
		Stream.Rate		= Info->rate;
		Stream.Channels	= Info->channels;
		Stream.Format	= Info->channels == 2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
		Sound->Duration	= ov_time_total( Ogg, -1 );
		if( StatOggStreams++ == 0 || (Flags & SF_Music) )
			debugf( TEXT("NOpenALDrv: Ogg Vorbis stream %s: %i Hz, %i ch, %1.1f s"), *Sound->Filename, Stream.Rate, Stream.Channels, Sound->Duration );
		return 1;
	}

	if( appStricmp( *Sound->FileType, TEXT("XA") ) == 0 )
	{
		// EA-XA: mono 16-bit at raw_SampleRate. The data must stay loaded while any stream reads it;
		// UnregisterSound unloads it again once the last one is gone.
		Sound->Data.Load();
		if( Sound->Data.Num() < EAXA_FRAME_BYTES || Sound->raw_NumChannels > 1 || ( Sound->raw_BitsPerSample && Sound->raw_BitsPerSample != 16 ) )
		{
			debugf( NAME_Warning, TEXT("NOpenALDrv: %s: unplayable XA data (%i bytes, %i ch, %i bit)"), Sound->GetPathName(),
				Sound->Data.Num(), Sound->raw_NumChannels, Sound->raw_BitsPerSample );
			return 0;
		}
		Stream.Kind		= NALSTREAM_XA;
		Stream.Rate		= Sound->raw_SampleRate > 0 ? Sound->raw_SampleRate : 22050;
		Stream.Channels	= 1;
		Stream.Format	= AL_FORMAT_MONO16;
		Stream.Xa.Init( (const uint8_t*)&Sound->Data(0), Sound->Data.Num(), Sound->raw_NumSamples );
		if( StatXaStreams++ == 0 && GUfrontLog )
			debugf( TEXT("NOpenALDrv: first EA-XA stream: %s (%i samples @ %i Hz, %i bytes, flags %i)"), Sound->GetPathName(),
				Sound->raw_NumSamples, Stream.Rate, Sound->Data.Num(), Sound->CoreFlags );
		return 1;
	}

	debugf( NAME_Warning, TEXT("NOpenALDrv: %s: streaming FileType '%s' is neither XA nor ogg"), Sound->GetPathName(), *Sound->FileType );
	return 0;
	unguard;
}

// Decode up to MaxBytes of 16-bit PCM. A looping stream rewinds at its end; a finished one sets Eof.
INT UNOpenALAudioSubsystem::FillStreamChunk( FNALStream& Stream, BYTE* Out, INT MaxBytes )
{
	guard(UNOpenALAudioSubsystem::FillStreamChunk);
	INT Got = 0;
	UBOOL JustRewound = 0;
	while( Got < MaxBytes && !Stream.Eof )
	{
		INT N = 0;
		if( Stream.Kind == NALSTREAM_XA )
		{
			N = 2 * Stream.Xa.Decode( (int16_t*)( Out + Got ), ( MaxBytes - Got ) / 2 );
		}
		else if( Stream.Kind == NALSTREAM_Ogg )
		{
			for( INT Tries=0; Tries<8; Tries++ )
			{
				int Section = 0;
				const long R = ov_read( (OggVorbis_File*)Stream.Ogg, (char*)( Out + Got ), MaxBytes - Got, 0, 2, 1, &Section );
				if( R == OV_HOLE )
					continue;	// a gap in the data: vorbisfile resyncs on the next call
				if( R < 0 )
				{
					debugf( NAME_Warning, TEXT("NOpenALDrv: Ogg decode error %i in %s"), (INT)R, Stream.Sound ? *Stream.Sound->Filename : TEXT("?") );
					Stream.Looping = 0;
				}
				N = R > 0 ? (INT)R : 0;
				break;
			}
		}
		if( N > 0 )
		{
			Got += N;
			JustRewound = 0;
			continue;
		}
		// End of the data. A rewind that yields nothing again means there is nothing to loop.
		if( !Stream.Looping || JustRewound )
		{
			Stream.Eof = 1;
			break;
		}
		if( Stream.Kind == NALSTREAM_XA )
			Stream.Xa.Rewind();
		else if( ov_pcm_seek( (OggVorbis_File*)Stream.Ogg, 0 ) != 0 )
		{
			Stream.Eof = 1;
			break;
		}
		JustRewound = 1;
	}
	return Got;
	unguard;
}

void UNOpenALAudioSubsystem::DestroyStream( INT i )
{
	guard(UNOpenALAudioSubsystem::DestroyStream);
	FNALStream& Stream = Streams(i);
	if( Stream.NumBuffers )
		alDeleteBuffers( Stream.NumBuffers, Stream.Buffer );
	alError( TEXT("DestroyStream"), false );
	if( Stream.Kind == NALSTREAM_Ogg && Stream.Ogg )
	{
		ov_clear( (OggVorbis_File*)Stream.Ogg );	// closes the FArchive through NalOggClose
		delete (OggVorbis_File*)Stream.Ogg;
	}
	appMemzero( &Stream, sizeof(FNALStream) );
	unguard;
}

INT UNOpenALAudioSubsystem::GetNewStreamStruct()
{
	guard(UNOpenALAudioSubsystem::GetNewStreamStruct);
	for( INT i=0; i<Streams.Num(); i++ )
		if( Streams(i).Kind == NALSTREAM_None )
			return i;
	// Full: take back a stream that was registered but never bound to a voice (PlaySound's early returns
	// leave those behind).
	const INT Reclaimed = ReclaimIdleStream();
	if( Reclaimed >= 0 )
		return Reclaimed;
	debugf( NAME_DevSound, TEXT("More than %i streams in use"), MAX_AUDIOSTREAMS );
	return -1;
	unguard;
}

INT UNOpenALAudioSubsystem::ReclaimIdleStream()
{
	guard(UNOpenALAudioSubsystem::ReclaimIdleStream);
	for( INT i=0; i<Streams.Num(); i++ )
	{
		USound* Sound = Streams(i).Sound;
		if( Streams(i).Kind == NALSTREAM_None || !Sound )
			continue;
		// OpenStreams holds (index+1)*2, with bit 0 set while bound to a PlaySound Id.
		INT Entry = INDEX_NONE;
		for( INT j=0; j<Sound->OpenStreams.Num() && Entry == INDEX_NONE; j++ )
			if( (Sound->OpenStreams(j) & ~1) == (i+1)*2 )
				Entry = j;
		if( Entry != INDEX_NONE && !(Sound->OpenStreams(Entry) & 1) )
		{
			Sound->DeleteStreamHandle( i+1 );
			Sound->RefCount--;
			DestroyStream( i );
			if( !Sound->OpenStreams.Num() )
				UnloadSoundData( Sound );
			return i;
		}
	}
	return -1;
	unguard;
}

FNALStream* UNOpenALAudioSubsystem::StreamForSource( INT i )
{
	if( !Sources(i).Sound )
		return NULL;
	const INT Handle = Sources(i).Sound->GetHandle( Sources(i).Id );
	if( Handle < 1 || Handle > Streams.Num() || Streams(Handle-1).Kind == NALSTREAM_None )
		return NULL;
	return &Streams(Handle-1);
}

// Refill every processed buffer of a streaming voice (one per Update underruns at a throttled frame rate)
// and restart it after an underrun. Returns false once the stream has played out.
UBOOL UNOpenALAudioSubsystem::PumpStream( INT i )
{
	guard(UNOpenALAudioSubsystem::PumpStream);
	FNALStream* Stream = StreamForSource( i );
	if( !Stream )
		return 0;
	const ALuint Sid = Sources(i).Source;

	ALint Processed = 0;
	alGetSourcei( Sid, AL_BUFFERS_PROCESSED, &Processed );
	while( Processed-- > 0 )
	{
		ALuint Buf = 0;
		alSourceUnqueueBuffers( Sid, 1, &Buf );
		if( alError( TEXT("unqueueing") ) )
			break;
		const INT Bytes = Stream->Eof ? 0 : FillStreamChunk( *Stream, &StreamScratch(0), StreamChunkSize );
		if( Bytes > 0 )
		{
			alBufferData( Buf, Stream->Format, &StreamScratch(0), Bytes, Stream->Rate );
			alSourceQueueBuffers( Sid, 1, &Buf );
			alError( TEXT("queueing") );
		}
	}

	ALint State = AL_STOPPED, Queued = 0;
	alGetSourcei( Sid, AL_SOURCE_STATE, &State );
	alGetSourcei( Sid, AL_BUFFERS_QUEUED, &Queued );
	if( State == AL_STOPPED || State == AL_INITIAL )
	{
		// A stopped source has every queued buffer processed; what is queued now was refilled just above.
		if( Queued > 0 )
		{
			// Ran dry (a load hitch, a throttled tab): play on from the fresh buffers.
			Stream->Underruns++;
			if( StatUnderruns++ < 16 )
				debugf( NAME_DevSound, TEXT("NOpenALDrv: stream underrun, restarting %s"), Sources(i).Sound->GetName() );
			alSourcePlay( Sid );
			return 1;
		}
		return 0;
	}
	return 1;
	unguard;
}

// TLazyArray<T>::Unload() is a no-op under GCC while GLazyLoad is 0 (Core/Inc/UnTemplate.h), and the client
// always runs with GLazyLoad 0. The engine's own idiom for a forced lazy (un)load is to raise GLazyLoad around
// the call (UnTex.cpp, UnFont.cpp). The data comes back from the package linker on the next Load(); a linker
// reset force-loads first (ResetLoaders(...,ForceLazyLoad=1)), so nothing is lost.
void UNOpenALAudioSubsystem::UnloadSoundData( USound* Sound )
{
	const UBOOL SavedLazyLoad = GLazyLoad;
	GLazyLoad = 1;
	Sound->Data.Unload();
	GLazyLoad = SavedLazyLoad;
}

/*------------------------------------------------------------------------------------
	Sound registration.
------------------------------------------------------------------------------------*/

UBOOL UNOpenALAudioSubsystem::RegisterSound( USound* Sound )
{
	guard(UNOpenALAudioSubsystem::RegisterSound);

	if( !Sound || !Initialized )
		return 0;

	if( !Sound->IsRegistered() )
	{
		INT Flags = Sound->CoreFlags | Sound->WorkingFlags;

		if( Flags & SF_Streaming )
		{
			// One stream per playing instance (USound::AddStreamHandle / BindStream / GetHandle(Id)).
			const INT Index = GetNewStreamStruct();
			if( Index < 0 )
			{
				debugf( NAME_DevSound, TEXT("Failed to register sound: %s (no free stream)"), Sound->GetName() );
				return 0;
			}
			FNALStream& Stream = Streams(Index);
			if( !OpenStream( Stream, Sound, Flags ) )
			{
				appMemzero( &Stream, sizeof(FNALStream) );
				if( !Sound->OpenStreams.Num() && appStricmp( *Sound->FileType, TEXT("XA") ) == 0 )
					UnloadSoundData( Sound );
				return 0;
			}

			// Prime the buffers (a short sound needs fewer than StreamBuffers).
			Stream.NumBuffers = StreamBuffers;
			alGenBuffers( Stream.NumBuffers, Stream.Buffer );
			if( alError( TEXT("Creating streaming buffer.") ) )
			{
				Stream.NumBuffers = 0;
				DestroyStream( Index );
				return 0;
			}
			for( INT b=0; b<Stream.NumBuffers; b++ )
			{
				const INT Bytes = FillStreamChunk( Stream, &StreamScratch(0), StreamChunkSize );
				if( Bytes <= 0 )
					break;
				alBufferData( Stream.Buffer[b], Stream.Format, &StreamScratch(0), Bytes, Stream.Rate );
				Stream.NumPrimed++;
			}
			if( !Stream.NumPrimed || alError( TEXT("Priming streaming buffer.") ) )
			{
				debugf( NAME_DevSound, TEXT("Failed to register sound: %s (no audio data)"), Sound->GetName() );
				DestroyStream( Index );
				return 0;
			}
			Sound->AddStreamHandle( Index + 1 );
		}
		else
		{
			// Plain RIFF WAVE: one AL buffer, kept until the sound is destroyed.
			Sound->Data.Load();
			FWaveModInfo WaveInfo;
			if( Sound->Data.Num() <= 0 || !WaveInfo.ReadWaveInfo( Sound->Data ) || !WaveInfo.SampleDataSize )
			{
				debugf( NAME_DevSound, TEXT("Failed to register sound: %s (not a RIFF WAVE)"), Sound->GetPathName() );
				return 0;
			}
			const INT BufferFlags = WaveInfo.SampleLoopsNum ? SF_Looping : 0;
			ALenum Format;
			if( *WaveInfo.pBitsPerSample == 8 )
				Format = ( *WaveInfo.pChannels == 1 ) ? AL_FORMAT_MONO8 : AL_FORMAT_STEREO8;
			else
				Format = ( *WaveInfo.pChannels == 1 ) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

			ALuint bid = 0;
			alGenBuffers( 1, &bid );
			alError( TEXT("RegisterSound (generating buffer)") );
			// Whole sample frames only (a padded odd byte count is an AL_INVALID_VALUE).
			const INT FrameBytes = ( *WaveInfo.pBitsPerSample / 8 ) * Max<INT>( 1, *WaveInfo.pChannels );
			alBufferData( bid, Format, WaveInfo.SampleDataStart, WaveInfo.SampleDataSize - WaveInfo.SampleDataSize % Max(FrameBytes,1), *WaveInfo.pSamplesPerSec );
			const UBOOL Failed = alError( TEXT("RegisterSound (creating buffer)") );

			// Unload the data: the AL buffer has its own copy.
			UnloadSoundData( Sound );

			if( Failed )
			{
				alDeleteBuffers( 1, &bid );
				return 0;
			}
			INT Slot = INDEX_NONE;
			for( INT b=0; b<Buffers.Num() && Slot == INDEX_NONE; b++ )
				if( !Buffers(b).Buffer )
					Slot = b;
			if( Slot == INDEX_NONE )
				Slot = Buffers.AddZeroed( 1 );
			Buffers(Slot).Buffer = bid;
			Buffers(Slot).Flags  = BufferFlags;
			Sound->SetHandle( Slot + 1 );
			if( StatWavBuffers++ == 0 && GUfrontLog )
				debugf( TEXT("NOpenALDrv: first PCM sound: %s (%i Hz, %i bit, %i ch)"), Sound->GetPathName(),
					*WaveInfo.pSamplesPerSec, *WaveInfo.pBitsPerSample, *WaveInfo.pChannels );
		}
	}

	// Need to keep track of how many times a sound object is SUCCESSFULLY registered.
	Sound->RefCount++;
	return 1;

	unguard;
}

void UNOpenALAudioSubsystem::UnregisterSound( USound* Sound, INT Id )
{
	guard(UNOpenALAudioSubsystem::UnregisterSound);

	if( !Sound || !Initialized )
		return;

	// Stop the sound's voice(s).
	for( INT i=0; i<Sources.Num(); i++ )
	{
		if( Id == -1 )
		{
			if( Sources(i).Sound == Sound )
				OldStopSound( i );
		}
		else if( Sources(i).Sound && Sources(i).Id == Id )
			OldStopSound( i );
	}
	alError( TEXT(""), false );

	if( Sound->CoreFlags & SF_Streaming )
	{
		if( Id == -1 )
		{
			// The sound is going away (USound::Destroy) or the driver is: every stream, bound or not.
			for( INT j=0; j<Sound->OpenStreams.Num(); j++ )
			{
				const INT i = Sound->OpenStreams(j) / 2 - 1;
				if( i >= 0 && i < Streams.Num() && Streams(i).Sound == Sound )
					DestroyStream( i );
			}
			Sound->UnbindAllStreams();
			Sound->DeleteAllStreamHandles();
			Sound->RefCount = 0;
		}
		else
		{
			Sound->RefCount--;
			const INT Handle = Sound->GetHandle( Id );
			if( Handle > 0 )
			{
				if( Handle <= Streams.Num() && Streams(Handle-1).Sound == Sound )
					DestroyStream( Handle - 1 );
				Sound->UnbindStream( Id );
				Sound->DeleteStreamHandle( Handle );
			}
		}
		// The last stream of an XA sound is gone -- drop the encoded bytes (dialog would otherwise pile up
		// line after line; AllDialog.uax is 97 MB).
		if( !Sound->OpenStreams.Num() && appStricmp( *Sound->FileType, TEXT("XA") ) == 0 )
			UnloadSoundData( Sound );
	}
	else
	{
		// Plain buffers stay until the sound itself is destroyed (Id == -1): replaying a sound then costs
		// nothing, and the whole PCM set is 6 MB.
		if( Id == -1 )
		{
			const INT Handle = Sound->GetHandle();
			if( Handle > 0 && Handle <= Buffers.Num() && Buffers(Handle-1).Buffer )
			{
				alDeleteBuffers( 1, &Buffers(Handle-1).Buffer );
				Buffers(Handle-1).Buffer = 0;
				Buffers(Handle-1).Flags  = 0;
			}
			Sound->SetHandle( 0 );
			Sound->RefCount = 0;
		}
		else if( Sound->RefCount > 0 )
			Sound->RefCount--;
	}

	unguard;
}

/*------------------------------------------------------------------------------------
	Playing.
------------------------------------------------------------------------------------*/

FLOAT UNOpenALAudioSubsystem::SourceGain( INT i, FLOAT Volume )
{
	return Clamp( Volume * ( (Sources(i).Flags & SF_Music) ? MusicVolume : SoundVolume ), 0.f, 1.f );
}

void UNOpenALAudioSubsystem::ApplyVolumes()
{
	guard(UNOpenALAudioSubsystem::ApplyVolumes);
	if( !Initialized || Muted )
		return;
	for( INT i=0; i<Sources.Num(); i++ )
		if( Sources(i).Id && Sources(i).FadeMode == FADE_None )
			alSourcef( Sources(i).Source, AL_GAIN, SourceGain( i, Sources(i).Volume ) );
	unguard;
}

INT UNOpenALAudioSubsystem::PlaySound( AActor* Actor, INT Id, USound* Sound, FVector Location, FLOAT Volume, FLOAT Radius, FLOAT Pitch, INT Flags, FLOAT FadeDuration, FLOAT InPriority )
{
	guard(UNOpenALAudioSubsystem::PlaySound);

	static INT FreeSlot = 0;

	if( !Viewport || !Sound || !Initialized )
		return -1;
	if( Radius <= 0.f )
		Radius = GAudioDefaultRadius;

	debugf( NAME_DevSound, TEXT("Playing %ssound %s"), (Sound->CoreFlags & SF_Streaming) ? TEXT("streaming ") : TEXT(""), Sound->GetName() );

	// Add any resident flags from the sound -tg
	Sound->WorkingFlags = Sound->CoreFlags | Flags;

	// (Retail registers the sound only once a voice is chosen and freed -- see below. The prototype registered
	// first, and on failure stole a voice through FindLeastImportantSound and retried.)

	Volume = Clamp( Volume, 0.f, 1.f );

	// Compute priority.
	FLOAT Priority;
	if( (Id & 14) == (SLOT_Talk * 2) )
	{
		// Boost priority of sounds played in SLOT_Talk to make sure they get played.
		Priority = 100.f;
	}
	else if( InPriority && ((Id & 14) == SLOT_Ambient * 2) )
	{
		// Ambient sounds have their priority calculated in Update.
		Priority = InPriority;
	}
	else
	{
		// (Retail: no occlusion pass before the priority.)
		Priority = SoundPriority( Viewport, Location, Volume * SoundVolume, Radius, Flags );
	}

	FLOAT BestPriority = Priority;

	// Allocate a new slot if requested. (Id & 00001110)
	if( (Id & 14) == 2 * SLOT_None )
		Id = 16 * --FreeSlot;

	INT Index = -1;

	// Find a voice to play the sound in.
	for( INT i=0; i<Sources.Num(); i++ )
	{
		if( (Sources(i).Id & ~1) == (Id & ~1) )
		{
			// Skip if not interruptable.
			if( Id & 1 )
				return -1;

			// Stop the sound.
			Index = i;
			break;
		}
		else if( (Sources(i).Priority * PLAYING_PRIORITY_MULTIPLIER) < BestPriority )
		{
			Index = i;
			BestPriority = Sources(i).Priority;
		}
	}

	// If no room to play the sound, see if we can make some room by killing a less important sound (retail).
	if( Index == -1 )
	{
		debugf( NAME_DevSound, TEXT("No empty slots in Sources() - try killing a sound") );
		Index = FindLeastImportantSound( Sound, Id, Priority );
		if( Index == -1 )
		{
			debugf( NAME_DevSound, TEXT("Unable to kill a sound - returning (Index == -1)") );
			return -1;
		}
	}

	StopSoundSource( Index );

	// Retail registers here, after the voice (and the stream it held) is free: the early returns above no
	// longer leave a registered-but-unbound stream behind, and a full stream table has just got one back.
	if( !RegisterSound( Sound ) )
		return -1;

	// Here's where we bind the stream to the sound (if a streaming sound).
	FNALStream* Stream = NULL;
	if( Sound->CoreFlags & SF_Streaming )
	{
		Sound->BindStream( Id );
		const INT Handle = Sound->GetHandle( Id );
		if( Handle < 1 || Handle > Streams.Num() || Streams(Handle-1).Kind == NALSTREAM_None )
			return -1;
		Stream = &Streams(Handle-1);
		// A stream registered for an earlier, non-looping request can serve a looping one: it is still at its
		// start, so switching only changes what happens at its end.
		const UBOOL WantLoop = (Flags & (SF_Looping | SF_Music)) != 0;
		if( WantLoop && !Stream->Looping )
		{
			Stream->Looping = 1;
			Stream->Eof = 0;
			// A short sound primed fewer buffers than it has; fill the rest (rewinding) so the loop has its cushion.
			while( Stream->NumPrimed < Stream->NumBuffers )
			{
				const INT Bytes = FillStreamChunk( *Stream, &StreamScratch(0), StreamChunkSize );
				if( Bytes <= 0 )
					break;
				alBufferData( Stream->Buffer[Stream->NumPrimed], Stream->Format, &StreamScratch(0), Bytes, Stream->Rate );
				Stream->NumPrimed++;
			}
		}
	}
	else if( Sound->GetHandle() < 1 || Sound->GetHandle() > Buffers.Num() )
		return -1;

	Pitch  = Clamp<FLOAT>( Pitch, 0.1f, 2.0f );
	Volume = Clamp<FLOAT>( Volume, 0.01f, 1.0f );

	FNALSource& Src = Sources(Index);

	// Set default values. (Retail: no initial occlusion pass -- the radius is the radius.)
	Src.ZoneRadius		= Radius;
	Src.UsedRadius		= Radius;
	Src.WantedRadius	= Radius;

	// Setup the voice.
	Src.Sound			= Sound;
	Src.Id				= Id;
	Src.Actor			= Actor;
	Src.Priority		= Priority;
	Src.Location		= (Flags & SF_No3D) ? FVector(0,0,0) : Location;
	Src.Radius			= Radius;
	Src.LastChange		= appSeconds();
	Src.FadeDuration	= FadeDuration;
	Src.FadeTime		= 0.f;
	Src.FadeMode		= FadeDuration > 0.f ? FADE_In : FADE_None;
	Src.Volume			= Volume;
	Src.Started			= 1;
	Src.Paused			= 0;
	Src.Flags			= Flags | ( Stream ? Stream->Flags : Buffers(Sound->GetHandle() - 1).Flags );
	Src.LipSyncAnimMgr	= NULL;

	// If this fails, no big deal, as subsequent code will assume NULL means it has no lip-sync data.
	if( (Sound->WorkingFlags & SF_HasLipSync) && Actor )
		Src.LipSyncAnimMgr = new FLipSyncAnimMgr( Actor, &Sound->LipSyncData );

	const ALuint sid = Src.Source;
	alSourceStop( sid );
	alSourcei( sid, AL_BUFFER, 0 );
	alSourcef( sid, AL_REFERENCE_DISTANCE, Src.UsedRadius * DISTANCE_FACTOR );
	alSourcef( sid, AL_GAIN, ( Muted || Src.FadeMode == FADE_In ) ? 0.f : SourceGain( Index, Src.Volume ) );
	alSourcef( sid, AL_PITCH, Pitch );
	alSourcei( sid, AL_LOOPING, ( !Stream && (Src.Flags & SF_Looping) ) ? AL_TRUE : AL_FALSE );
	const FVector ALLocation = ToALSpace( Src.Location );
	alSourcefv( sid, AL_POSITION, (ALfloat*)&ALLocation );
	const FVector ALVelocity( 0.f, 0.f, 0.f );
	alSourcefv( sid, AL_VELOCITY, (ALfloat*)&ALVelocity );
	alSourcef( sid, AL_ROLLOFF_FACTOR, RollOff );
	alSourcei( sid, AL_SOURCE_RELATIVE, (Flags & SF_No3D) ? AL_TRUE : AL_FALSE );
	if( ReverbKind != NAL_REVERB_NONE )
		alSource3i( sid, AL_AUXILIARY_SEND_FILTER, (Flags & SF_No3D) ? (ALint)AL_EFFECTSLOT_NULL : (ALint)ReverbSlot, 0, AL_FILTER_NULL );

	if( Stream )
		alSourceQueueBuffers( sid, Stream->NumPrimed, Stream->Buffer );
	else
		alSourcei( sid, AL_BUFFER, Buffers( Sound->GetHandle() - 1 ).Buffer );
	alSourcePlay( sid );
	alError( TEXT("PlaySound") );

	// Lip sync also needs to know how long a sound has been playing so it can determine the appropriate lip
	// frame at any given time.
	if( Src.LipSyncAnimMgr )
	{
		const UBOOL HasChannel = Src.LipSyncAnimMgr->Start( appSeconds().GetFloat() );
		if( StatLipSync++ == 0 )
		{
			if( GUfrontLog )
				debugf( TEXT("NOpenALDrv: first lip-synced line: %s on %s (%i amplitude frames, mouth channel %s)"), Sound->GetPathName(),
					Actor->GetName(), Sound->LipSyncData.Num(), HasChannel ? TEXT("created") : TEXT("none") );
		}
		else
			debugf( NAME_DevSound, TEXT("NOpenALDrv: lip-synced line %s on %s (%i frames, channel %s)"), Sound->GetName(),
				Actor->GetName(), Sound->LipSyncData.Num(), HasChannel ? TEXT("created") : TEXT("none") );
	}

	return Index;

	unguard;
}

//-----------------------------------------------------------------------------
//	StopSound( AActor* Actor, USound* Sound ) -- New Warfare StopSound()
//-----------------------------------------------------------------------------
UBOOL UNOpenALAudioSubsystem::StopSound( AActor* Actor, USound* Sound )
{
	guard(UNOpenALAudioSubsystem::StopSound);
	UBOOL Stopped = false;

	// Stop all sounds.
	if( !Actor && !Sound )
	{
		for( INT i=0; i<Sources.Num(); i++ )
			StopSoundSource( i );
		return true;
	}

	// Stop selected sound.
	for( INT i=0; i<Sources.Num(); i++ )
	{
		const UBOOL Match = Sound
			? ( Sound == Sources(i).Sound && ( !Actor || Actor == Sources(i).Actor ) )
			: ( Actor == Sources(i).Actor );
		if( Match )
		{
			StopSoundSource( i );
			Stopped = true;
		}
	}
	return Stopped;
	unguard;
}

//-----------------------------------------------------------------------------
//	StopSound( AActor* Actor, INT Id, USound * Sound ) -- the one script calls.
//-----------------------------------------------------------------------------
UBOOL UNOpenALAudioSubsystem::StopSound( AActor* Actor, INT Id, USound * Sound, FLOAT fFadeOutTime )
{
	guard(UNOpenALAudioSubsystem::StopSound);
	UBOOL Stopped = false;

	for( INT i=0; i<Sources.Num(); i++ )
	{
		UBOOL bSoundFound;
		bSoundFound  = Sources(i).Actor == Actor;										// actors match
		bSoundFound &= (((Sources(i).Id & ~1) == (Id & ~1)) ||							// same slot or SLOT_None and
					   (((Id & 14) == SLOT_None) && (Sources(i).Id < 0)));				//   sound was played in SLOT_None
		bSoundFound &= Sources(i).Sound == Sound || Sound == NULL;						// sounds match or NULL sent in

		if( bSoundFound && Sources(i).Id )
		{
			if( fFadeOutTime > 0.f )
			{
				Sources(i).FadeTime		= 0.f;
				Sources(i).FadeDuration	= fFadeOutTime;
				Sources(i).FadeMode		= FADE_Out;
			}
			else
				StopSoundSource( i );
			Stopped = true;
		}
	}
	return Stopped;
	unguard;
}

//-----------------------------------------------------------------------------
//	ModifySound( AActor* Actor, INT Id, USound * Sound, BYTE parameter, FLOAT Value )
//-----------------------------------------------------------------------------
UBOOL UNOpenALAudioSubsystem::ModifySound( AActor* Actor, INT Id, USound * Sound, BYTE parameter, FLOAT Value )
{
	guard(UNOpenALAudioSubsystem::ModifySound);
	UBOOL bRet = false;

	for( INT i=0; i<Sources.Num(); i++ )
	{
		UBOOL bSoundFound;
		bSoundFound  = Sources(i).Actor == Actor;
		bSoundFound &= (((Sources(i).Id & ~1) == (Id & ~1)) ||
					   (((Id & 14) == SLOT_None) && (Sources(i).Id < 0)));
		bSoundFound &= Sources(i).Sound == Sound || Sound == NULL;

		if( bSoundFound && Sources(i).Id )
		{
			const ALuint sid = Sources(i).Source;
			switch( parameter )
			{
			case SOUND_Volume:
				// (Retail scales by SoundVolume here; SourceGain does that live. A muted voice stays silent.)
				Sources(i).Volume = Clamp<FLOAT>( Value, 0.01f, 1.0f );
				if( !Muted )
					alSourcef( sid, AL_GAIN, SourceGain( i, Sources(i).Volume ) );
				break;

			case SOUND_Radius:
				// (Retail: no occlusion pass.)
				Sources(i).ZoneRadius	= Value;
				Sources(i).UsedRadius	= Value;
				Sources(i).WantedRadius	= Value;
				alSourcef( sid, AL_REFERENCE_DISTANCE, Sources(i).UsedRadius * DISTANCE_FACTOR );
				break;

			case SOUND_Pitch:
				alSourcef( sid, AL_PITCH, Clamp<FLOAT>( Value, 0.1f, 2.0f ) );
				break;
			}
			bRet = true;
		}
	}
	return bRet;
	unguard;
}

void UNOpenALAudioSubsystem::NoteDestroy( AActor* Actor )
{
	guard(UNOpenALAudioSubsystem::NoteDestroy);
	check(Actor);
	check(Actor->IsValid());

	// Stop the actor's sound, and dereference owned sounds.
	for( INT i=0; i<Sources.Num(); i++ )
	{
		// The actor, or the mouth channel a line animates, is going: the manager must not touch it again.
		if( Sources(i).LipSyncAnimMgr && Sources(i).LipSyncAnimMgr->Involves( Actor ) )
		{
			Sources(i).LipSyncAnimMgr->Detach();
			delete Sources(i).LipSyncAnimMgr;
			Sources(i).LipSyncAnimMgr = NULL;
		}
		if( Sources(i).Actor == Actor )
		{
			if( (Sources(i).Id & 14) == SLOT_Ambient * 2 )
				StopSoundSource( i );
			else
				Sources(i).Actor = NULL; // Not interruptable sound.
		}
	}
	unguard;
}

/*------------------------------------------------------------------------------------
	Music: Ogg Vorbis by path.
------------------------------------------------------------------------------------*/

// A song name that carries its own path is tried as given first; then ../Music/<Song>.ogg, then
// ../Music/<Song> as given (Music/Peeves_chase_v2 has no extension). All through GFileManager, which folds
// '\' to '/' and resolves the case of every component.
UBOOL UNOpenALAudioSubsystem::ResolveMusic( const FString& Song, FString& Path, void* )
{
	guard(UNOpenALAudioSubsystem::ResolveMusic);
	TArray<FString> Candidates;
	const UBOOL HasExt = appStricmp( appFExt( *Song ), TEXT("ogg") ) == 0;
	const UBOOL HasDir = Song.InStr( TEXT("\\") ) != INDEX_NONE || Song.InStr( TEXT("/") ) != INDEX_NONE;
	if( HasDir )
	{
		new(Candidates) FString( HasExt ? Song : Song + TEXT(".ogg") );
		new(Candidates) FString( Song );
	}
	if( !HasExt )
		new(Candidates) FString( FString(TEXT("../Music/")) + Song + TEXT(".ogg") );
	new(Candidates) FString( FString(TEXT("../Music/")) + Song );
	for( INT i=0; i<Candidates.Num(); i++ )
	{
		FArchive* Ar = GFileManager->CreateFileReader( *Candidates(i) );
		if( Ar )
		{
			const INT Size = Ar->TotalSize();
			delete Ar;
			if( Size > 0 )
			{
				Path = Candidates(i);
				return 1;
			}
		}
	}
	return 0;
	unguard;
}

INT UNOpenALAudioSubsystem::PlayMusic( FString Song, FLOAT FadeInTime )
{
	guard(UNOpenALAudioSubsystem::PlayMusic);

	if( !Initialized || Song == TEXT("") || appStricmp( *Song, TEXT("none") ) == 0 )
		return 0;

	FString Filename;
	if( !ResolveMusic( Song, Filename, NULL ) )
	{
		// Nothing to play (e.g. Ch1Rictusempra names a song that exists nowhere).
		debugf( TEXT("NOpenALDrv: PlayMusic '%s' -- no ../Music/%s.ogg or ../Music/%s"), *Song, *Song, *Song );
		return 0;
	}

	// A transient USound, as in ALAudio: garbage-collected at the next map change, which stops the song.
	USound* Music = new USound( *Filename, SF_Streaming | SF_Music );
	if( !Music )
		return 0;
	Music->FileType = FName( TEXT("ogg") );

	const INT Index = PlaySound( NULL, 2*SLOT_None | 1, Music, FVector(0,0,0), 1.f, 1000, 1.f, SF_Streaming | SF_No3D | SF_Music, FadeInTime );
	if( Index < 0 )
	{
		debugf( TEXT("NOpenALDrv: PlayMusic '%s' (%s) -- no voice or stream available"), *Song, *Filename );
		UnregisterSound( Music );
		return 0;
	}
	debugf( TEXT("NOpenALDrv: PlayMusic '%s' -> %s on channel %i (fade in %1.1f s, MusicVolume %1.2f)"), *Song, *Filename, Index, FadeInTime, MusicVolume );
	return Index + 1;
	unguard;
}

UBOOL UNOpenALAudioSubsystem::StopMusic( INT SongHandle, FLOAT FadeOutTime )
{
	guard(UNOpenALAudioSubsystem::StopMusic);

	// Stop music.
	SongHandle--;
	if( SongHandle >= 0 && SongHandle < Sources.Num() )
	{
		// A stale handle must not cut off a sound effect that took the channel since.
		if( !(Sources(SongHandle).Flags & SF_Music) )
			return 0;
		debugf( NAME_DevSound, TEXT("NOpenALDrv: StopMusic channel %i (fade out %1.1f s)"), SongHandle, FadeOutTime );
		if( FadeOutTime > 0.f )
		{
			Sources(SongHandle).FadeTime		= 0.f;
			Sources(SongHandle).FadeDuration	= FadeOutTime;
			Sources(SongHandle).FadeMode		= FADE_Out;
		}
		else
			StopSoundSource( SongHandle );
		return 1;
	}
	return 0;
	unguard;
}

INT UNOpenALAudioSubsystem::StopAllMusic( FLOAT FadeOutTime )
{
	guard(UNOpenALAudioSubsystem::StopAllMusic);
	for( INT i=0; i<Sources.Num(); i++ )
		if( Sources(i).Flags & SF_Music )
			StopMusic( i+1, FadeOutTime );
	return 0;
	unguard;
}

void UNOpenALAudioSubsystem::PauseSounds()
{
	guard(UNOpenALAudioSubsystem::PauseSounds);
	for( INT i=0; i<Sources.Num(); i++ )
	{
		if( Sources(i).Id )
		{
			Sources(i).Paused = 1;
			alSourcePause( Sources(i).Source );
		}
	}
	unguard;
}

void UNOpenALAudioSubsystem::UnpauseSounds()
{
	guard(UNOpenALAudioSubsystem::UnpauseSounds);
	for( INT i=0; i<Sources.Num(); i++ )
	{
		if( Sources(i).Id )
		{
			Sources(i).Paused = 0;
			if( Sources(i).Started )
				alSourcePlay( Sources(i).Source );
		}
	}
	unguard;
}

/*------------------------------------------------------------------------------------
	Update: the per-frame loop (ALAudio's), plus the stream pump.
------------------------------------------------------------------------------------*/

// Retail sorted the candidates twice behind a global mode flag: by AmbientSound (so the entries of one sound form
// one run; retail ordered the runs by pointer value, here by object index -- deterministic, equally arbitrary),
// then each run by priority, highest first. One composite key does both.
static INT Compare( FNALAmbient& A, FNALAmbient& B )
{
	USound* SA = A.Actor->AmbientSound;
	USound* SB = B.Actor->AmbientSound;
	if( SA != SB )
		return SA->GetIndex() < SB->GetIndex() ? -1 : 1;
	return (B.Priority - A.Priority >= 0) ? 1 : -1;
}

void UNOpenALAudioSubsystem::Update( FSceneNode* SceneNode )
{
	guard(UNOpenALAudioSubsystem::Update);

	if( !Viewport || !Viewport->Actor || !SceneNode || !Initialized )
		return;

	// Projection/ Orientation.
	FVector ViewLocation = SceneNode->Coords.Origin;

	// Find out which zone the listener is in.
	AActor* ViewTarget = Viewport->Actor->ViewTarget ? Viewport->Actor->ViewTarget : Viewport->Actor;
	ULevel* Level = ViewTarget->GetLevel();
	if( !Level )
		return;
	RegionListener = Level->Model->PointRegion( ViewTarget->Level, ViewLocation );

	// Time passes...
	FTime CurrentTime	= appSeconds();
	FLOAT DeltaTime		= CurrentTime - LastTime;
	LastTime			= CurrentTime;
	DeltaTime			= Clamp( DeltaTime, 0.0001f, 1.0f );

	UBOOL Realtime	= Viewport->IsRealtime();
	UBOOL Paused	= Viewport->Actor->Level->Pauser != TEXT("");

	// Stop all sounds if transitioning out of realtime, pause or unpause sounds if pause transition.
	if( (!Realtime && LastRealtime) || (Paused && !LastPaused) )
		PauseSounds();
	else if( (Realtime && !LastRealtime) || (!Paused && LastPaused) )
		UnpauseSounds();
	else if( RestartSounds )
	{
		RestartSounds = false;
		UnpauseSounds();
	}
	LastRealtime	= Realtime;
	LastPaused		= Paused;

	// Streams first (every processed buffer refilled), then finished sounds. A stream that plays out frees
	// its voice, music included (a looping stream never does, and a song whose file breaks off does not hold
	// its channel); a plain buffer is checked unless it loops.
	guard(PumpAndCheckFinished);
	for( INT i=0; i<Sources.Num(); i++ )
	{
		if( !Sources(i).Id || !Sources(i).Sound || Sources(i).Paused )
			continue;
		if( Sources(i).Flags & SF_Streaming )
		{
			if( !PumpStream( i ) )
				UnregisterSound( Sources(i).Sound, Sources(i).Id );	// played out: frees the voice and its stream
		}
		else if( !(Sources(i).Flags & (SF_Looping | SF_Music)) )
		{
			ALint State = AL_PLAYING;
			alGetSourcei( Sources(i).Source, AL_SOURCE_STATE, &State );
			if( State == AL_STOPPED )
				StopSoundSource( i );
		}
	}
	unguard;

	// Ambient sounds -- retail: a pass every AMBIENT_UPDATE_INTERVAL seconds (the prototype ran it every frame),
	// both the starting and the out-of-range stopping below.
	AmbientTimer += DeltaTime;
	if( AmbientTimer > AMBIENT_UPDATE_INTERVAL )
	{
		AmbientTimer = 0.f;

		// See if any new ambient sounds need to be started.
		guard(HandleAmbience);
		if( Realtime && !Paused )
		{
			TArray<FNALAmbient> AmbientSounds;

			for( INT a=0; a<Level->Actors.Num(); a++ )
			{
				AActor* Actor = Level->Actors(a);
				if( !Actor || !Actor->AmbientSound )
					continue;
				const INT Id = Actor->GetIndex()*16 + SLOT_Ambient*2;
				UBOOL Playing = 0;
				for( INT j=0; j<Sources.Num() && !Playing; j++ )
					Playing = Sources(j).Id == Id;
				if( Playing )
					continue;
				// (Retail: the priority volume includes the global SoundVolume, as a playing voice's does; and
				// no occlusion -- a plain SoundRadius*GAudioMaxRadiusMultiplier range check.)
				if( Actor->IsOwnedBy( ViewTarget ) )
				{
					INT iAmbient = AmbientSounds.Add( 1 );
					AmbientSounds(iAmbient).Actor		= Actor;
					AmbientSounds(iAmbient).Priority	= SoundPriority( Viewport, Actor->Location, Actor->SoundVolume/255.f * SoundVolume, Actor->SoundRadius, SF_Looping | SF_No3D | SF_UpdatePitch );
					AmbientSounds(iAmbient).Flags		= SF_Looping | SF_No3D | SF_UpdatePitch;
					AmbientSounds(iAmbient).Id			= Id;
					AmbientSounds(iAmbient).SourceIndex	= -1;
				}
				else if( FDistSquared(ViewLocation,Actor->Location) <= Square(Actor->SoundRadius*GAudioMaxRadiusMultiplier) )
				{
					INT iAmbient = AmbientSounds.Add( 1 );
					AmbientSounds(iAmbient).Actor		= Actor;
					AmbientSounds(iAmbient).Priority	= SoundPriority( Viewport, Actor->Location, Actor->SoundVolume/255.f * SoundVolume, Actor->SoundRadius, SF_Looping | SF_UpdatePitch );
					AmbientSounds(iAmbient).Flags		= SF_Looping | SF_UpdatePitch;
					AmbientSounds(iAmbient).Id			= Id;
					AmbientSounds(iAmbient).SourceIndex	= -1;
				}
			}

			// Retail: the ambient voices already playing compete with the new candidates.
			for( INT i=0; i<Sources.Num(); i++ )
			{
				if( Sources(i).Id && (Sources(i).Id&14) == SLOT_Ambient*2 && Sources(i).Actor && Sources(i).Actor->AmbientSound )
				{
					INT iAmbient = AmbientSounds.Add( 1 );
					AmbientSounds(iAmbient).Actor		= Sources(i).Actor;
					AmbientSounds(iAmbient).Priority	= Sources(i).Priority;
					AmbientSounds(iAmbient).Flags		= Sources(i).Flags;
					AmbientSounds(iAmbient).Id			= Sources(i).Id;
					AmbientSounds(iAmbient).SourceIndex	= i;
				}
			}

			// Per AmbientSound, keep the MAX_AMBIENT_INSTANCES highest priorities: stop the playing voices past
			// them, start the candidates among them. A refusal from PlaySound ends that sound's run only.
			if( AmbientSounds.Num() )
			{
				Sort( &AmbientSounds(0), AmbientSounds.Num() );
				for( INT First=0; First<AmbientSounds.Num(); )
				{
					USound* RunSound = AmbientSounds(First).Actor->AmbientSound;
					INT End = First;
					while( End<AmbientSounds.Num() && AmbientSounds(End).Actor->AmbientSound == RunSound )
						End++;
					for( INT k=First+MAX_AMBIENT_INSTANCES; k<End; k++ )
					{
						// (Retail stopped the recorded index blindly; a voice an earlier run's PlaySound took over
						// meanwhile carries another Id and is left alone.)
						const INT Idx = AmbientSounds(k).SourceIndex;
						if( Idx != -1 && Sources(Idx).Id == AmbientSounds(k).Id )
							StopSoundSource( Idx );
					}
					for( INT k=First; k<Min(End, First+MAX_AMBIENT_INSTANCES); k++ )
					{
						if( AmbientSounds(k).SourceIndex != -1 )
							continue;
						AActor* Actor = AmbientSounds(k).Actor;
						if( PlaySound( Actor, AmbientSounds(k).Id, Actor->AmbientSound, Actor->Location, Actor->SoundVolume/255.f,
							Actor->SoundRadius, Actor->SoundPitch / 64.f, AmbientSounds(k).Flags, 0.f, AmbientSounds(k).Priority ) == -1 )
							break;
					}
					First = End;
				}
			}
		}
		unguard;

		// Update all playing ambient sounds.
		guard(UpdateAmbience);
		for( INT i=0; i<Sources.Num(); i++ )
		{
			if( (Sources(i).Id&14) == SLOT_Ambient*2 && Sources(i).Actor )
			{
				FLOAT NewDist = FDistSquared( ViewLocation, Sources(i).Actor->Location );
				if( (NewDist > Square(Sources(i).ZoneRadius*GAudioMaxRadiusMultiplier)) || (Sources(i).Actor->AmbientSound != Sources(i).Sound) )
					StopSoundSource( i );
			}
		}
		unguard;
	}

	// Update all active sounds.
	guard(UpdateSounds);
	for( INT i=0; i<Sources.Num(); i++ )
	{
		if( Sources(i).Actor && !Sources(i).Actor->IsValid() )
			Sources(i).Actor = NULL;
		if( !Sources(i).Id )
			continue;

		// If sounds are paused, we need to let the lip sync mgr adjust.
		if( Sources(i).Paused )
		{
			if( Sources(i).LipSyncAnimMgr )
				Sources(i).LipSyncAnimMgr->EatPauseTime( DeltaTime );
			continue;
		}

		// Update lip sync data if present.
		if( Sources(i).LipSyncAnimMgr )
			Sources(i).LipSyncAnimMgr->Update( CurrentTime.GetFloat() );

		// Update position, velocity, pitch and volume from actor (if wanted).
		FVector Velocity	= FVector(0,0,0);
		FLOAT Volume		= Sources(i).Volume;
		if( Sources(i).Actor && !(Sources(i).Flags & SF_NoUpdates) )
		{
			// Set location.
			Sources(i).Location = Sources(i).Actor->Location;
			if( !(Sources(i).Flags & SF_No3D) )
				Velocity = Sources(i).Actor->Velocity;

			// Set pitch.
			if( Sources(i).Flags & SF_UpdatePitch )
				alSourcef( Sources(i).Source, AL_PITCH, Clamp<FLOAT>( Sources(i).Actor->SoundPitch / 64.f, 0.1f, 2.0f ) );

			// Set Volume.
			if( (Sources(i).Id&14) == SLOT_Ambient*2 )
			{
				Volume = Sources(i).Actor->SoundVolume / 255.f;
				Sources(i).Volume = Volume;
			}
			if( Sources(i).Actor->LightType != LT_None )
				Volume *= Sources(i).Actor->LightBrightness / 255.f;
		}

		if( Sources(i).FadeMode == FADE_In )
		{
			// Disregard initial loading time.
			if( DeltaTime < 1.f )
				Sources(i).FadeTime += DeltaTime;
			if( Sources(i).FadeTime >= Sources(i).FadeDuration )
				Sources(i).FadeMode = FADE_None;
			else
				Volume *= Sources(i).FadeTime / Sources(i).FadeDuration;
		}
		if( Sources(i).FadeMode == FADE_Out )
		{
			// Disregard initial loading time.
			if( DeltaTime < 1.f )
				Sources(i).FadeTime += DeltaTime;
			if( Sources(i).FadeTime >= Sources(i).FadeDuration )
			{
				// Stop sound.
				StopSoundSource( i );
				continue;
			}
			Volume *= (1.f - Sources(i).FadeTime / Sources(i).FadeDuration);
		}

		// (Retail: a muted voice keeps gain 0 -- MUTESOUNDS.)
		if( !Muted )
			alSourcef( Sources(i).Source, AL_GAIN, SourceGain( i, Volume ) );

		if( !(Sources(i).Flags & SF_No3D) )
		{
			// (Retail: no occlusion -- the zone radius is the radius.)
			FLOAT Radius = Sources(i).Radius;
			Sources(i).ZoneRadius = Radius;

			// Smooth transition between radii.
			if( Sources(i).WantedRadius != Radius )
			{
				Sources(i).LastChange	= CurrentTime;
				Sources(i).WantedRadius	= Radius;
			}
			if( (CurrentTime - Sources(i).LastChange) < 1.f )
				Radius = Lerp( Sources(i).UsedRadius, Radius, CurrentTime - Sources(i).LastChange );
			else
				Sources(i).UsedRadius = Radius;

			// Set Radius.
			alSourcef( Sources(i).Source, AL_REFERENCE_DISTANCE, Radius * DISTANCE_FACTOR );
		}

		// Update the priority. (Retail's voice volume is the scaled one: SoundVolume for a sound, MusicVolume for
		// a song -- SourceGain.)
		Sources(i).Priority = SoundPriority( Viewport, Sources(i).Location, SourceGain( i, Sources(i).Volume ), Sources(i).ZoneRadius, Sources(i).Flags );

		if( !(Sources(i).Flags & SF_No3D) )
		{
			const FVector ALLocation = ToALSpace( Sources(i).Location );
			const FVector ALVelocity = ToALSpace( Velocity );
			alSourcefv( Sources(i).Source, AL_POSITION, (ALfloat*)&ALLocation );
			if( ALVelocity.Size() < MAX_SOURCE_VELOCITY )
				alSourcefv( Sources(i).Source, AL_VELOCITY, (ALfloat*)&ALVelocity );
		}
	}
	unguard;

	guard(UpdateListenerVelocityAndPos);
	// Listener: position and velocity in AL space, orientation from the view axes (Coords: X right, Y down,
	// Z forward) -- at = Z, up = -Y, both with Z negated into the right-handed frame. ReverseStereo flips
	// "up", which mirrors left and right.
	const FVector At = SceneNode->Coords.ZAxis;
	const FVector Up = ReverseStereo ? SceneNode->Coords.YAxis : -SceneNode->Coords.YAxis;
	const ALfloat Orientation[6] = { At.X, At.Y, -At.Z, Up.X, Up.Y, -Up.Z };
	const FVector ALVelocity = ToALSpace( (ViewLocation - LastPosition) / DeltaTime );
	LastPosition = ViewLocation;
	const FVector ALPosition = ToALSpace( ViewLocation );
	alListenerfv( AL_POSITION, (ALfloat*)&ALPosition );
	alListenerfv( AL_ORIENTATION, Orientation );
	if( ALVelocity.Size() < MAX_LISTENER_VELOCITY )
		alListenerfv( AL_VELOCITY, (ALfloat*)&ALVelocity );
	unguard;

	// Zone reverb (EFX, UseReverb only).
	if( ReverbKind != NAL_REVERB_NONE )
		UpdateReverb( RegionListener.Zone );

	unguard;
}

/*------------------------------------------------------------------------------------
	Reverb (EFX EAX reverb, default off).
------------------------------------------------------------------------------------*/

void UNOpenALAudioSubsystem::UpdateReverb( AZoneInfo* Zone )
{
	guard(UNOpenALAudioSubsystem::UpdateReverb);
	if( ReverbKind == NAL_REVERB_NONE )
		return;

	const UBOOL bNewReverb = Zone && Zone->bReverbZone;
	if( !bNewReverb && ReverbOn )
	{
		ReverbOn	= false;
		ReverbZone	= NULL;
		Efx.AuxiliaryEffectSloti( ReverbSlot, AL_EFFECTSLOT_EFFECT, AL_EFFECT_NULL );
		return;
	}
	ReverbOn = bNewReverb;
	if( ReverbOn && Zone != ReverbZone )
	{
		// Reverb zone changed: unbind the effect, change parameters, bind again.
		ReverbZone = Zone;
		Efx.AuxiliaryEffectSloti( ReverbSlot, AL_EFFECTSLOT_EFFECT, AL_EFFECT_NULL );
#if !NOPENAL_NO_EAX
		Efx.Effectf( ReverbEffect, AL_EAXREVERB_GAIN, ReverbZone->MasterGain / 255.f );
#endif
		Efx.AuxiliaryEffectSloti( ReverbSlot, AL_EFFECTSLOT_EFFECT, ReverbEffect );
		static UBOOL LoggedReverb = 0;
		if( !LoggedReverb )
		{
			if( GUfrontLog )
				debugf( TEXT("NOpenALDrv: EAX reverb applied for zone %s (MasterGain %i)"), ReverbZone->GetName(), ReverbZone->MasterGain );
			LoggedReverb = 1;
		}
	}
	unguard;
}

void UNOpenALAudioSubsystem::InitReverbEffect()
{
	guard(UNOpenALAudioSubsystem::InitReverbEffect);
	if( ReverbKind == NAL_REVERB_NONE )
		return;
#if !NOPENAL_NO_EAX
	EFXEAXREVERBPROPERTIES Reverb = EFX_REVERB_PRESET_GENERIC;
	Efx.Effecti( ReverbEffect, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB );
	Efx.Effectf( ReverbEffect, AL_EAXREVERB_DENSITY, Reverb.flDensity );
	Efx.Effectf( ReverbEffect, AL_EAXREVERB_DIFFUSION, Reverb.flDiffusion );
	Efx.Effectf( ReverbEffect, AL_EAXREVERB_GAIN, Reverb.flGain );
	Efx.Effectf( ReverbEffect, AL_EAXREVERB_GAINHF, Reverb.flGainHF );
	Efx.Effectf( ReverbEffect, AL_EAXREVERB_GAINLF, Reverb.flGainLF );
	Efx.Effectf( ReverbEffect, AL_EAXREVERB_DECAY_TIME, Reverb.flDecayTime );
	Efx.Effectf( ReverbEffect, AL_EAXREVERB_DECAY_HFRATIO, Reverb.flDecayHFRatio );
	Efx.Effectf( ReverbEffect, AL_EAXREVERB_DECAY_LFRATIO, Reverb.flDecayLFRatio );
	Efx.Effectf( ReverbEffect, AL_EAXREVERB_REFLECTIONS_GAIN, Reverb.flReflectionsGain );
	Efx.Effectf( ReverbEffect, AL_EAXREVERB_REFLECTIONS_DELAY, Reverb.flReflectionsDelay );
	Efx.Effectfv( ReverbEffect, AL_EAXREVERB_REFLECTIONS_PAN, Reverb.flReflectionsPan );
	Efx.Effectf( ReverbEffect, AL_EAXREVERB_LATE_REVERB_GAIN, Reverb.flLateReverbGain );
	Efx.Effectf( ReverbEffect, AL_EAXREVERB_LATE_REVERB_DELAY, Reverb.flLateReverbDelay );
	Efx.Effectfv( ReverbEffect, AL_EAXREVERB_LATE_REVERB_PAN, Reverb.flLateReverbPan );
	Efx.Effectf( ReverbEffect, AL_EAXREVERB_ECHO_TIME, Reverb.flEchoTime );
	Efx.Effectf( ReverbEffect, AL_EAXREVERB_ECHO_DEPTH, Reverb.flEchoDepth );
	Efx.Effectf( ReverbEffect, AL_EAXREVERB_MODULATION_TIME, Reverb.flModulationTime );
	Efx.Effectf( ReverbEffect, AL_EAXREVERB_MODULATION_DEPTH, Reverb.flModulationDepth );
	Efx.Effectf( ReverbEffect, AL_EAXREVERB_AIR_ABSORPTION_GAINHF, Reverb.flAirAbsorptionGainHF );
	Efx.Effectf( ReverbEffect, AL_EAXREVERB_HFREFERENCE, Reverb.flHFReference );
	Efx.Effectf( ReverbEffect, AL_EAXREVERB_LFREFERENCE, Reverb.flLFReference );
	Efx.Effectf( ReverbEffect, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, Reverb.flRoomRolloffFactor );
	Efx.Effecti( ReverbEffect, AL_EAXREVERB_DECAY_HFLIMIT, Reverb.iDecayHFLimit );
#endif
	unguard;
}

/*------------------------------------------------------------------------------------
	Exec (ALAudio's commands, plus NOPENALSTATS).
------------------------------------------------------------------------------------*/

UBOOL UNOpenALAudioSubsystem::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(UNOpenALAudioSubsystem::Exec);
	const TCHAR* Str = Cmd;
	if( ParseCommand(&Str,TEXT("ASTAT")) )
	{
		if( ParseCommand(&Str,TEXT("AUDIO")) )
			return 1;
	}
	else if( ParseCommand(&Str,TEXT("PAUSESOUNDS")) )
	{
		PauseSounds();
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("UNPAUSESOUNDS")) )
	{
		UnpauseSounds();
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("RESTARTSOUNDS")) )
	{
		RestartSounds = true;
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("WEAPONRADIUS")) )
	{
		GAudioDefaultRadius = appAtof(Str);
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("ROLLOFF")) )
	{
		RollOff = appAtof(Str);
		if( RollOff <= 0.f )
			RollOff = 1.f;
		StopSound( NULL, NULL );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("SETVOLUMES")) )
	{
		// Retail: HGame's FESoundVideoPage sends "SETVOLUMES MUSIC=<f>" / "SETVOLUMES SOUND=<f>" (then writes the
		// ini with "set ini:Engine.Engine.AudioDevice ..."): new global volumes, applied to the playing voices.
		FLOAT Value;
		if( Parse( Str, TEXT("SOUND="), Value ) )
			SoundVolume = Clamp( Value, 0.f, 1.f );
		if( Parse( Str, TEXT("MUSIC="), Value ) )
			MusicVolume = Clamp( Value, 0.f, 1.f );
		ApplyVolumes();
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("SOUNDSOURCESTATES")) )
	{
		// Retail debug command.
		for( INT i=0; i<Sources.Num(); i++ )
		{
			if( Sources(i).Id )
			{
				ALint State = 0;
				alGetSourcei( Sources(i).Source, AL_SOURCE_STATE, &State );
				Ar.Logf( TEXT("Source %d, State = %d"), i, State );
			}
			else
				Ar.Logf( TEXT("Source %d not active"), i );
		}
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("MUTESOUNDS")) )
	{
		// Retail: HGame mutes everything while a cut-scene is fast-forwarded (SkipCS), UNMUTESOUNDS after.
		// Voices keep playing; PlaySound/Update/ApplyVolumes leave their gain at 0 until then.
		Muted = 1;
		for( INT i=0; i<Sources.Num(); i++ )
			if( Sources(i).Id )
				alSourcef( Sources(i).Source, AL_GAIN, 0.f );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("UNMUTESOUNDS")) )
	{
		Muted = 0;
		ApplyVolumes();		// fading voices get theirs from the next Update
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("NOPENALSTATS")) )
	{
		INT Playing = 0, OpenStreams = 0;
		for( INT i=0; i<Sources.Num(); i++ )
			Playing += Sources(i).Id != 0;
		for( INT i=0; i<Streams.Num(); i++ )
			OpenStreams += Streams(i).Kind != NALSTREAM_None;
		Ar.Logf( TEXT("NOpenALDrv: %i/%i voices, %i/%i streams, %i buffers; XA streams %i, Ogg streams %i, PCM buffers %i, lip-sync lines %i, underruns %i"),
			Playing, Sources.Num(), OpenStreams, Streams.Num(), Buffers.Num(), StatXaStreams, StatOggStreams, StatWavBuffers, StatLipSync, StatUnderruns );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("PLAYINGSOUNDSTATS")) )
	{
		INT iTotalMem = 0, iOverhead = 0;
		for( INT i=0; i<Sources.Num(); i++ )
		{
			USound* Sound = Sources(i).Sound;
			if( !Sound || !Sources(i).Id )
				continue;
			const INT iSoundDataSize = Sound->Data.Num();
			INT iALBufferSize = 0;
			FNALStream* Stream = StreamForSource( i );
			if( Stream )
			{
				for( INT j=0; j<Stream->NumBuffers; j++ )
				{
					ALint iTemp = 0;
					alGetBufferi( Stream->Buffer[j], AL_SIZE, &iTemp );
					iALBufferSize += iTemp;
				}
				iOverhead += StreamChunkSize;
				Ar.Logf( TEXT("Streaming Sound:  Name = %s  Pri =%d Sound data = %iK  ALAudio buffer = %iK  Streaming buffer = %iK"),
					Sound->GetName(), (INT)(Sources(i).Priority * 1000.f), iSoundDataSize / 1024, iALBufferSize / 1024, StreamChunkSize / 1024 );
			}
			else if( Sound->GetHandle() > 0 && Sound->GetHandle() <= Buffers.Num() )
			{
				ALint iTemp = 0;
				alGetBufferi( Buffers( Sound->GetHandle() - 1 ).Buffer, AL_SIZE, &iTemp );
				iALBufferSize = iTemp;
				Ar.Logf( TEXT("Sound:  Name = %s  Pri = %d Sound data = %iK  ALAudio buffer = %iK"), Sound->GetName(), (INT)(Sources(i).Priority * 1000.f), iSoundDataSize / 1024, iALBufferSize / 1024 );
			}
			iOverhead += iALBufferSize;
			iTotalMem += iALBufferSize + iSoundDataSize;
		}
		Ar.Logf( TEXT("Total memory = %iK  Overhead = %iK"), (iTotalMem + iOverhead) / 1024, iOverhead / 1024 );
		return 1;
	}
	return 0;
	unguard;
}

/*------------------------------------------------------------------------------------
	Internals.
------------------------------------------------------------------------------------*/

//-----------------------------------------------------------------------------
//	ALAudio's wrapper: UnregisterSound for streaming sounds (frees the stream) and the plain stop for others.
//-----------------------------------------------------------------------------
void UNOpenALAudioSubsystem::StopSoundSource( INT i )
{
	guard(UNOpenALAudioSubsystem::StopSoundSource);
	if( Sources(i).Sound )
		UnregisterSound( Sources(i).Sound, Sources(i).Id );
	else
		OldStopSound( i );
	unguard;
}

void UNOpenALAudioSubsystem::OldStopSound( INT i )
{
	guard(UNOpenALAudioSubsystem::OldStopSound);
	if( Sources(i).Id != 0 || Sources(i).Sound )
	{
		if( Sources(i).Source )
		{
			alSourceStop( Sources(i).Source );
			alSourcei( Sources(i).Source, AL_BUFFER, 0 );	// unqueues a stream's buffers too
		}
		if( Sources(i).LipSyncAnimMgr )
		{
			delete Sources(i).LipSyncAnimMgr;
			Sources(i).LipSyncAnimMgr = NULL;
		}
		Sources(i).Sound	= NULL;
		Sources(i).Actor	= NULL;
		Sources(i).Flags	= 0;
		Sources(i).Priority	= 0;
		Sources(i).Id		= 0;
		Sources(i).Started	= 0;
		Sources(i).Paused	= 0;
		Sources(i).FadeMode	= FADE_None;
	}
	unguard;
}

FLOAT UNOpenALAudioSubsystem::SoundPriority( UViewport* InViewport, FVector Location, FLOAT Volume, FLOAT Radius, INT Flags )
{
	guard(UNOpenALAudioSubsystem::SoundPriority);
	FLOAT RadiusFactor = 1.f;
	if( Radius && InViewport && InViewport->Actor )
	{
		AActor *ViewTarget = InViewport->Actor->ViewTarget ? InViewport->Actor->ViewTarget : InViewport->Actor;
		RadiusFactor = 1 - FDistSquared(Location, ViewTarget->Location) / Square(GAudioMaxRadiusMultiplier*Radius);
	}
	RadiusFactor = Clamp( RadiusFactor, 0.01f, 1.f );
	return Volume * RadiusFactor + ((Flags & SF_HasLipSync)? 4 : 0) + ((Flags & SF_Music)? 2 : 0) + ((Flags & SF_No3D)? 1 : 0);
	unguard;
}

UBOOL UNOpenALAudioSubsystem::alError( const TCHAR* Text, UBOOL Log )
{
	ALenum Error = alGetError();
	if( Error == AL_NO_ERROR )
		return false;
	if( Log )
	{
		switch( Error )
		{
		case AL_INVALID_NAME:		debugf( TEXT("NOpenALDrv: AL_INVALID_NAME in %s"), Text ); break;
		case AL_INVALID_ENUM:		debugf( TEXT("NOpenALDrv: AL_INVALID_ENUM in %s"), Text ); break;
		case AL_INVALID_VALUE:		debugf( TEXT("NOpenALDrv: AL_INVALID_VALUE in %s"), Text ); break;
		case AL_INVALID_OPERATION:	debugf( TEXT("NOpenALDrv: AL_INVALID_OPERATION in %s"), Text ); break;
		case AL_OUT_OF_MEMORY:		debugf( TEXT("NOpenALDrv: AL_OUT_OF_MEMORY in %s"), Text ); break;
		default:					debugf( TEXT("NOpenALDrv: Unknown error in %s"), Text );
		}
	}
	return true;
}

// Retail's version (FindLeastImportantSound(USound*,INT,FLOAT)): only a voice LESS important than the incoming
// sound may go. Among the AMBIENT voices, the AmbientSound with the most playing instances is culled first; with
// no ambient voice playing, an incoming ambient sound finds nothing and any other sound may take any (less
// important) voice. The incoming USound is no longer consulted, and SLOT_Talk is protected by its priority of
// 100 instead of by slot. PlaySound only calls this after its own scan found no voice of lower priority, so in
// practice it returns -1 -- retail too: the prototype's "steal the 4th instance of a sound" is gone.
INT UNOpenALAudioSubsystem::FindLeastImportantSound( USound *pIncomingSound, INT Id, FLOAT Priority )
{
	guard(UNOpenALAudioSubsystem::FindLeastImportantSound);
	INT					iRet = -1;
	TMap<USound*, INT>	SoundInstances;
	INT					iMaxCount = 0;
	USound*				pMaxCountSound = NULL;

	// Count the number of instances of each playing ambient sound.
	for( INT i=0; i<Sources.Num(); i++ )
	{
		if( Sources(i).Id != 0 && (Sources(i).Id & 14) == SLOT_Ambient*2 && Sources(i).Sound )
		{
			INT iNumInstances = SoundInstances.FindRef( Sources(i).Sound ) + 1;
			SoundInstances.Set( Sources(i).Sound, iNumInstances );
			if( iNumInstances > iMaxCount )
			{
				iMaxCount = iNumInstances;
				pMaxCountSound = Sources(i).Sound;
			}
		}
	}

	FLOAT fLowestPriority = 1000.f;
	for( INT i=0; i<Sources.Num(); i++ )
	{
		if( Sources(i).Id == 0 || !Sources(i).Sound )
			continue;
		UBOOL bCandidate;
		if( pMaxCountSound )
			bCandidate = Sources(i).Sound == pMaxCountSound;
		else if( (Id & 14) == SLOT_Ambient*2 )
			bCandidate = (Sources(i).Id & 14) == SLOT_Ambient*2;
		else
			bCandidate = 1;
		if( bCandidate && Sources(i).Priority < Priority )
		{
			FLOAT fPriority = SoundPriority( Viewport, Sources(i).Location, SourceGain( i, Sources(i).Volume ), Sources(i).Radius, Sources(i).Sound->WorkingFlags );
			if( fPriority < fLowestPriority )
			{
				fLowestPriority = fPriority;
				iRet = i;
			}
		}
	}
	debugf( NAME_DevSound, TEXT("FindLeastImportantSound() returning %d, pMaxCountSound = %d"), iRet, pMaxCountSound != NULL );
	return iRet;
	unguard;
}

/*-----------------------------------------------------------------------------
	FLipSyncAnimMgr (ALAudio's).
-----------------------------------------------------------------------------*/

FLipSyncAnimMgr::FLipSyncAnimMgr( AActor *ParentActor, FLipSyncData *pLipSyncData )
{
	m_ParentActor		= ParentActor;
	m_pLipSyncData		= pLipSyncData;
	m_StartTime			= 0.f;
	m_LastElapsedTime	= 0.f;
	m_LastAmp			= 0;
	m_LipSyncAnimChan	= NULL;		// the destructor reads it even if Start() never ran
}

FLipSyncAnimMgr::~FLipSyncAnimMgr( )
{
	if( m_LipSyncAnimChan && !m_LipSyncAnimChan->bDeleteMe )
		m_LipSyncAnimChan->PlayAnim( TEXT("mouth_rest"), false, 1.0, 1.0, 1.0 );
	m_ParentActor = NULL;
	m_pLipSyncData = NULL;
}

// Tells a lip sync animation manager object that the sound is starting.
UBOOL FLipSyncAnimMgr::Start( FLOAT fTime )
{
	m_StartTime			= fTime;
	m_LastElapsedTime	= 0.0;
	m_LastAmp			= 0;
	m_LipSyncAnimChan	= NULL;

	// AAnimChannel is a native Engine class, registered at boot: no LoadObject needed.
	if( m_ParentActor && !m_ParentActor->bDeleteMe )
		m_LipSyncAnimChan = m_ParentActor->CreateAnimChannel( AAnimChannel::StaticClass(), AT_Replace, TEXT("mouth_root"), false, true );
	if( m_LipSyncAnimChan )
		m_LipSyncAnimChan->PlayAnim( TEXT("mouth_anim"), false, 0.0 );
	return m_LipSyncAnimChan != NULL;
}

// Updates the actor's animation state based on time elapsed from beginning of sound.
#define LIP_SYNC_UPDATE_INTERVAL	(0.05f)

void FLipSyncAnimMgr::Update( FLOAT fTime )
{
	// Bail if we failed to get an anim channel earlier.
	if( !m_LipSyncAnimChan || !m_pLipSyncData )
		return;

	FLOAT ElapsedTime		= fTime - m_StartTime;
	FLOAT fFrameDeltaTime	= ElapsedTime - m_LastElapsedTime;
	if( fFrameDeltaTime >= LIP_SYNC_UPDATE_INTERVAL )
	{
		m_LastElapsedTime = ElapsedTime;
		m_LipSyncAnimChan->AnimFrame = m_pLipSyncData->GetMouthPos( ElapsedTime );
	}
}

// Called while the parent sound is paused.
void FLipSyncAnimMgr::EatPauseTime( FLOAT fDeltaTime )
{
	m_StartTime += fDeltaTime;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
