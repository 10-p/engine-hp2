/*=============================================================================
	EAXAOracle.cpp: the EA-XA decoder's acceptance test.

	The oracle is in the game data: USound::CalcLipSyncData (Engine/Src/UnAudio.cpp) computes each dialog
	line's lip-sync table at import time from the original PCM: one byte every 20 ms = the peak |sample|/128
	over a 5 ms window. Decoding the shipped EA-XA bytes and recomputing that table must give the same curve,
	so a wrong coefficient table, shift, nibble order or history rule shows up as a collapsed correlation.

	Checks, per run:
	  1. Oracle  (every AllDialog.uax given): the first 50 sounds with FileType 'XA' and SF_HasLipSync, decoded
	     through the driver's streaming path (FEAXADecoder::Decode in 32 KB chunks). Pass: Pearson r > 0.99
	     and mean |delta| under 2% of the line's peak table value (relative, because lossy ADPCM errs in
	     proportion to loudness). The first five must reproduce the reference values to the printed precision
	     (r 0.9997/0.9959/0.9981/0.9994/0.9993, MAD 0.43/1.36/0.85/0.80/0.73).
	  2. Decode  (every XA sound in every package given): the sound decodes to exactly raw_NumSamples samples,
	     consumes no more than its bytes (ceil(n/28)*15 for plain frames), and a whole-sound decode equals the
	     chunked one (odd chunk sizes included).
	  3. Census  (when a whole Sounds/ directory is given): the known counts (3,289 sounds, 2,994 XA =
	     121,218,855 bytes, 626 x flags 260 + 2,368 x flags 388, 295 wav = 218 x 0 + 77 x 128).

	Not part of the normal build (EXCLUDE_FROM_ALL; never compiled for the web). Run it by hand:
	  cmake --build <native-tree> --target eaxa_oracle && <native-tree>/NOpenALDrv/eaxa_oracle Assets/Sounds
	or with no CMake tree at all (from the repo root):
	  g++ -O2 -o /tmp/eaxa_oracle Source/NOpenALDrv/Test/EAXAOracle.cpp Source/NOpenALDrv/EAXADecoder.cpp \
	      && /tmp/eaxa_oracle Assets/Sounds
	Exit status 0 = every check passed. Assets/ is only ever read.
=============================================================================*/

#include "../EAXADecoder.h"

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <string>
#include <vector>
#include <algorithm>

/*-----------------------------------------------------------------------------
	A minimal UE1 package reader (enough for USound exports of a version-79 .uax).
-----------------------------------------------------------------------------*/

struct FPkgReader
{
	const uint8_t* B;
	size_t         N;
	size_t         P;
	bool           Bad;

	FPkgReader( const std::vector<uint8_t>& Bytes, size_t At=0 ) : B( Bytes.data() ), N( Bytes.size() ), P( At ), Bad( false ) {}

	bool Need( size_t Count ) { if( P + Count > N ) { Bad = true; return false; } return true; }
	uint8_t  U8()  { if( !Need(1) ) return 0; return B[P++]; }
	uint16_t U16() { if( !Need(2) ) return 0; uint16_t V = (uint16_t)( B[P] | ( B[P+1] << 8 ) ); P += 2; return V; }
	int32_t  I32() { if( !Need(4) ) return 0; uint32_t V = (uint32_t)B[P] | ( (uint32_t)B[P+1] << 8 ) | ( (uint32_t)B[P+2] << 16 ) | ( (uint32_t)B[P+3] << 24 ); P += 4; return (int32_t)V; }
	float    F32() { int32_t I = I32(); float F; memcpy( &F, &I, 4 ); return F; }
	// UE1 compact index.
	int32_t  CI()
	{
		uint8_t V = U8();
		const bool Neg = ( V & 0x80 ) != 0;
		int32_t R = V & 0x3f;
		if( V & 0x40 )
		{
			int Shift = 6;
			do
			{
				V = U8();
				R |= ( V & 0x7f ) << Shift;
				Shift += 7;
			} while( ( V & 0x80 ) && Shift < 32 && !Bad );
		}
		return Neg ? -R : R;
	}
};

struct FSoundRec
{
	std::string Name;
	std::string FileType;
	int32_t     CoreFlags;
	float       Duration;
	int32_t     NumSamples, BitsPerSample, NumChannels, SampleRate;
	size_t      DataPos;
	int32_t     DataNum;
	size_t      LipPos;
	int32_t     LipNum;
};

static bool ReadFile( const char* Path, std::vector<uint8_t>& Out )
{
	FILE* F = fopen( Path, "rb" );
	if( !F )
		return false;
	fseek( F, 0, SEEK_END );
	long Size = ftell( F );
	fseek( F, 0, SEEK_SET );
	Out.resize( Size > 0 ? (size_t)Size : 0 );
	const bool Ok = Size <= 0 || fread( Out.data(), (size_t)Size, 1, F ) == 1;
	fclose( F );
	return Ok;
}

// Every USound export of the package, in export order. Returns false on a malformed package.
static bool ReadSounds( const std::vector<uint8_t>& Bytes, std::vector<FSoundRec>& Sounds, int& Version, int& Unparsed )
{
	FPkgReader R( Bytes );
	const uint32_t Tag = (uint32_t)R.I32();
	Version = R.U16();
	R.U16();              // licensee
	R.I32();              // package flags
	const int32_t NameCount = R.I32(), NameOff = R.I32();
	const int32_t ExpCount  = R.I32(), ExpOff  = R.I32();
	const int32_t ImpCount  = R.I32(), ImpOff  = R.I32();
	if( Tag != 0x9E2A83C1u || R.Bad || Version < 64 )
		return false;

	std::vector<std::string> Names;
	R.P = (size_t)NameOff;
	for( int32_t i = 0; i < NameCount && !R.Bad; i++ )
	{
		const int32_t Len = R.CI();
		if( Len < 0 || !R.Need( (size_t)Len ) )
			return false;
		Names.push_back( Len > 0 ? std::string( (const char*)R.B + R.P, (size_t)Len - 1 ) : std::string() );
		R.P += (size_t)Len;
		R.I32();          // name flags
	}
	std::vector<std::string> ImportNames;
	R.P = (size_t)ImpOff;
	for( int32_t i = 0; i < ImpCount && !R.Bad; i++ )
	{
		R.CI(); R.CI(); R.I32();
		const int32_t ObjName = R.CI();
		ImportNames.push_back( ObjName >= 0 && ObjName < (int32_t)Names.size() ? Names[ObjName] : std::string() );
	}
	R.P = (size_t)ExpOff;
	for( int32_t i = 0; i < ExpCount && !R.Bad; i++ )
	{
		const int32_t Class = R.CI();
		R.CI(); R.I32();
		const int32_t ObjName = R.CI();
		R.I32();          // object flags
		const int32_t SerialSize = R.CI();
		int32_t SerialOff = 0;
		if( SerialSize > 0 )
			SerialOff = R.CI();
		if( Class >= 0 || -Class - 1 >= (int32_t)ImportNames.size() || ImportNames[-Class - 1] != "Sound" || SerialSize <= 0 )
			continue;

		// USound::Serialize: UObject tagged properties (HP2's sounds carry none: "None"), FileType, CoreFlags,
		// Duration, raw_NumSamples (ver>76), raw_BitsPerSample+raw_NumChannels (ver>77), raw_SampleRate (ver>78),
		// Data (lazy: INT skip position + compact count + bytes), LipSyncData the same when SF_HasLipSync.
		FPkgReader S( Bytes, (size_t)SerialOff );
		const int32_t FirstProp = S.CI();
		if( FirstProp < 0 || FirstProp >= (int32_t)Names.size() || Names[FirstProp] != "None" )
		{
			Unparsed++;
			continue;
		}
		FSoundRec Rec;
		Rec.Name = ObjName >= 0 && ObjName < (int32_t)Names.size() ? Names[ObjName] : std::string( "?" );
		const int32_t FileType = S.CI();
		Rec.FileType      = FileType >= 0 && FileType < (int32_t)Names.size() ? Names[FileType] : std::string( "?" );
		Rec.CoreFlags     = S.I32();
		Rec.Duration      = S.F32();
		Rec.NumSamples    = Version > 76 ? S.I32() : 0;
		Rec.BitsPerSample = Version > 77 ? S.I32() : 0;
		Rec.NumChannels   = Version > 77 ? S.I32() : 0;
		Rec.SampleRate    = Version > 78 ? S.I32() : 0;
		S.I32();          // lazy skip position
		Rec.DataNum = S.CI();
		Rec.DataPos = S.P;
		if( Rec.DataNum < 0 || !S.Need( (size_t)Rec.DataNum ) )
		{
			Unparsed++;
			continue;
		}
		S.P += (size_t)Rec.DataNum;
		Rec.LipPos = 0;
		Rec.LipNum = 0;
		if( Rec.CoreFlags & 128 )
		{
			S.I32();
			Rec.LipNum = S.CI();
			Rec.LipPos = S.P;
			if( Rec.LipNum < 0 || !S.Need( (size_t)Rec.LipNum ) )
			{
				Unparsed++;
				continue;
			}
		}
		if( S.Bad )
		{
			Unparsed++;
			continue;
		}
		Sounds.push_back( Rec );
	}
	return !R.Bad;
}

/*-----------------------------------------------------------------------------
	Decoding helpers.
-----------------------------------------------------------------------------*/

// Decode a whole sound through the streaming API, ChunkSamples at a time (the driver's path).
static std::vector<int16_t> DecodeChunked( const std::vector<uint8_t>& Bytes, const FSoundRec& S, int32_t ChunkSamples, int32_t* BytesUsed=NULL )
{
	FEAXADecoder Dec;
	Dec.Init( Bytes.data() + S.DataPos, S.DataNum, S.NumSamples );
	std::vector<int16_t> Pcm;
	std::vector<int16_t> Chunk( (size_t)ChunkSamples );
	for( ;; )
	{
		const int32_t Got = Dec.Decode( Chunk.data(), ChunkSamples );
		if( !Got )
			break;
		Pcm.insert( Pcm.end(), Chunk.begin(), Chunk.begin() + Got );
	}
	if( BytesUsed )
		*BytesUsed = Dec.Pos;
	return Pcm;
}

struct FOracleResult
{
	double R;
	double Mad;
	int    Frames;
	int    MaxLip, MaxPred;
};

// The lip-sync recomputation: N = len(lips)-1 windows of [t, t+5 ms), t = 20 ms * i, the
// window edges taken like FWaveModInfo::OffsetFromTime (byte offset rounded down to a 16-bit boundary).
static FOracleResult LipSyncOracle( const std::vector<int16_t>& Pcm, const uint8_t* Lips, int32_t LipNum, int32_t Rate )
{
	FOracleResult Res = { 0.0, 0.0, 0, 0, 0 };
	const int N = LipNum - 1;
	if( N <= 1 )
		return Res;
	std::vector<double> Pred( (size_t)N ), L( (size_t)N );
	for( int i = 0; i < N; i++ )
	{
		const double T = i * 0.020;
		int A = ( (int)( Rate * 2 * T ) & ~1 ) / 2;
		int E = ( (int)( Rate * 2 * ( T + 0.005 ) ) & ~1 ) / 2;
		int M = 0;
		for( int k = A; k < E && k < (int)Pcm.size(); k++ )
		{
			int Amp = abs( (int)Pcm[k] ) / 128;
			if( Amp > 255 )
				Amp = 255;
			if( Amp > M )
				M = Amp;
		}
		Pred[i] = M;
		L[i]    = Lips[i];
		Res.MaxLip  = std::max( Res.MaxLip, (int)Lips[i] );
		Res.MaxPred = std::max( Res.MaxPred, M );
	}
	double Mx = 0, My = 0;
	for( int i = 0; i < N; i++ ) { Mx += L[i]; My += Pred[i]; }
	Mx /= N; My /= N;
	double Num = 0, Dx = 0, Dy = 0, Mad = 0;
	for( int i = 0; i < N; i++ )
	{
		Num += ( L[i] - Mx ) * ( Pred[i] - My );
		Dx  += ( L[i] - Mx ) * ( L[i] - Mx );
		Dy  += ( Pred[i] - My ) * ( Pred[i] - My );
		Mad += fabs( L[i] - Pred[i] );
	}
	const double Den = sqrt( Dx * Dy );
	Res.R      = Den > 0 ? Num / Den : 0.0;
	Res.Mad    = Mad / N;
	Res.Frames = N;
	return Res;
}

static bool EndsWithNoCase( const std::string& S, const char* Suffix )
{
	const size_t L = strlen( Suffix );
	return S.size() >= L && strcasecmp( S.c_str() + S.size() - L, Suffix ) == 0;
}

static std::string BaseName( const std::string& Path )
{
	const size_t Slash = Path.find_last_of( '/' );
	return Slash == std::string::npos ? Path : Path.substr( Slash + 1 );
}

/*-----------------------------------------------------------------------------
	Synthetic frames (the paths the game data never takes).
-----------------------------------------------------------------------------*/

// 0xEE raw frame (no HP2 sound has one; the census counts them), then a plain frame predicting from its
// history, then a truncated tail that must end the sound instead of reading past the buffer.
static int SelfTest()
{
	uint8_t Buf[EAXA_RAW_FRAME_BYTES + EAXA_FRAME_BYTES + 7];
	memset( Buf, 0, sizeof(Buf) );
	Buf[0] = 0xEE;
	Buf[1] = 0x01; Buf[2] = 0x00;     // h1 = 256
	Buf[3] = 0xFF; Buf[4] = 0x80;     // h2 = -128
	for( int i = 0; i < EAXA_FRAME_SAMPLES; i++ )
	{
		const int16_t V = (int16_t)( i * 1000 - 14000 );
		Buf[5 + 2 * i]     = (uint8_t)( (uint16_t)V >> 8 );
		Buf[5 + 2 * i + 1] = (uint8_t)( (uint16_t)V & 0xff );
	}
	uint8_t* Plain = Buf + EAXA_RAW_FRAME_BYTES;
	Plain[0] = 0x2C;                  // predictor 2 (460, -208), shift 12+8
	Plain[1] = 0x7F;                  // first nibble +7, second -1
	FEAXADecoder Dec;
	Dec.Init( Buf, (int32_t)sizeof(Buf), 0 );
	int16_t Out[100];
	const int32_t Got = Dec.Decode( Out, 100 );
	int Fail = 0;
	if( Got != 2 * EAXA_FRAME_SAMPLES ) Fail++;
	for( int i = 0; i < EAXA_FRAME_SAMPLES && !Fail; i++ )
		Fail += Out[i] != (int16_t)( i * 1000 - 14000 );
	// First plain sample from the raw frame's header history (h1=256, h2=-128), as ffmpeg's ADPCM_EA_R1 does.
	const int32_t S0 = ( ( ( 7 << 28 ) >> 20 ) + 460 * 256 - 208 * -128 + 128 ) >> 8;
	const int32_t S1 = ( ( (int32_t)( 15u << 28 ) >> 20 ) + 460 * S0 - 208 * 256 + 128 ) >> 8;
	Fail += Out[EAXA_FRAME_SAMPLES] != S0 || Out[EAXA_FRAME_SAMPLES + 1] != S1;
	Fail += !Dec.AtEnd() || Dec.Decode( Out, 100 ) != 0;
	printf( "self-test (0xEE raw frame, history, truncated tail): %s\n", Fail ? "FAIL" : "ok" );
	return Fail ? 1 : 0;
}

/*-----------------------------------------------------------------------------
	Main.
-----------------------------------------------------------------------------*/

int main( int argc, char** argv )
{
	if( argc < 2 )
	{
		fprintf( stderr, "usage: %s <Sounds-dir | package.uax>...   (AllDialog.uax runs the lip-sync oracle)\n"
		                 "       %s --dump <package.uax> <SoundName> <out.s16>   (raw mono s16le PCM, for listening/comparing)\n", argv[0], argv[0] );
		return 2;
	}
	if( strcmp( argv[1], "--dump" ) == 0 )
	{
		std::vector<uint8_t> Bytes;
		std::vector<FSoundRec> Sounds;
		int Version = 0, Unparsed = 0;
		if( argc < 5 || !ReadFile( argv[2], Bytes ) || !ReadSounds( Bytes, Sounds, Version, Unparsed ) )
			return 2;
		for( size_t s = 0; s < Sounds.size(); s++ )
			if( strcasecmp( Sounds[s].Name.c_str(), argv[3] ) == 0 && Sounds[s].FileType == "XA" )
			{
				const std::vector<int16_t> Pcm = DecodeChunked( Bytes, Sounds[s], 16384 );
				FILE* F = fopen( argv[4], "wb" );
				if( !F )
					return 2;
				fwrite( Pcm.data(), 2, Pcm.size(), F );
				fclose( F );
				printf( "%s: %d samples @ %d Hz -> %s\n", Sounds[s].Name.c_str(), (int)Pcm.size(), Sounds[s].SampleRate, argv[4] );
				return 0;
			}
		fprintf( stderr, "no XA sound %s in %s\n", argv[3], argv[2] );
		return 1;
	}

	// Expand directories to their *.uax (sorted, so the output is stable).
	std::vector<std::string> Files;
	bool WholeDirectory = false;
	for( int a = 1; a < argc; a++ )
	{
		std::string Arg = argv[a];
		DIR* D = opendir( Arg.c_str() );
		if( D )
		{
			std::vector<std::string> Here;
			for( struct dirent* E = readdir( D ); E; E = readdir( D ) )
				if( EndsWithNoCase( E->d_name, ".uax" ) )
					Here.push_back( Arg + "/" + E->d_name );
			closedir( D );
			std::sort( Here.begin(), Here.end() );
			Files.insert( Files.end(), Here.begin(), Here.end() );
			WholeDirectory = true;
		}
		else
			Files.push_back( Arg );
	}

	int Failures = SelfTest();
	long TotalSounds = 0, XaSounds = 0, Xa260 = 0, Xa388 = 0, WavSounds = 0, Wav0 = 0, Wav128 = 0, RawFrames = 0;
	long long XaBytes = 0;
	long RailHits = 0, XaSamples = 0;
	bool RanOracle = false;

	for( size_t f = 0; f < Files.size(); f++ )
	{
		std::vector<uint8_t> Bytes;
		if( !ReadFile( Files[f].c_str(), Bytes ) )
		{
			fprintf( stderr, "FAIL cannot read %s\n", Files[f].c_str() );
			Failures++;
			continue;
		}
		std::vector<FSoundRec> Sounds;
		int Version = 0, Unparsed = 0;
		if( !ReadSounds( Bytes, Sounds, Version, Unparsed ) )
		{
			fprintf( stderr, "FAIL %s: not a readable UE1 package\n", Files[f].c_str() );
			Failures++;
			continue;
		}
		if( Unparsed )
		{
			fprintf( stderr, "FAIL %s: %d Sound exports could not be parsed\n", Files[f].c_str(), Unparsed );
			Failures++;
		}
		const std::string Base = BaseName( Files[f] );

		// 2. Decode + 3. Census
		int FileXa = 0, FileBad = 0;
		for( size_t s = 0; s < Sounds.size(); s++ )
		{
			const FSoundRec& S = Sounds[s];
			TotalSounds++;
			if( S.FileType == "XA" )
			{
				XaSounds++;
				FileXa++;
				XaBytes += S.DataNum;
				if( S.CoreFlags == 260 ) Xa260++;
				if( S.CoreFlags == 388 ) Xa388++;
				for( int32_t p = 0; p + EAXA_FRAME_BYTES <= S.DataNum; )
				{
					const bool Raw = Bytes[S.DataPos + p] == 0xEE;
					RawFrames += Raw;
					p += Raw ? EAXA_RAW_FRAME_BYTES : EAXA_FRAME_BYTES;
				}
				int32_t Used = 0;
				const std::vector<int16_t> Pcm = DecodeChunked( Bytes, S, 16384, &Used );   // 32 KB chunks
				bool Ok = (int32_t)Pcm.size() == S.NumSamples && Used <= S.DataNum
					&& Used == std::min( S.DataNum, FEAXADecoder::EncodedBytes( S.NumSamples ) );
				// Chunk boundaries must not matter: an odd chunk size gives the same samples.
				if( Ok && ( s % 7 ) == 0 )
					Ok = DecodeChunked( Bytes, S, 1000 ) == Pcm && DecodeChunked( Bytes, S, 1 << 22 ) == Pcm;
				for( size_t k = 0; k < Pcm.size(); k++ )
					RailHits += ( Pcm[k] == 32767 || Pcm[k] == -32768 );
				XaSamples += (long)Pcm.size();
				if( !Ok )
				{
					if( FileBad < 5 )
						fprintf( stderr, "FAIL %s.%s: decoded %d of %d samples, used %d of %d bytes\n", Base.c_str(), S.Name.c_str(),
							(int)Pcm.size(), S.NumSamples, Used, S.DataNum );
					FileBad++;
				}
			}
			else if( EndsWithNoCase( S.FileType, "wav" ) )
			{
				WavSounds++;
				if( S.CoreFlags == 0 )   Wav0++;
				if( S.CoreFlags == 128 ) Wav128++;
			}
		}
		Failures += FileBad;
		printf( "%-24s ver %d  %4d sounds  %4d XA  %s\n", Base.c_str(), Version, (int)Sounds.size(), FileXa, FileBad ? "DECODE FAILURES" : "decode ok" );

		// 1. Oracle
		if( strcasecmp( Base.c_str(), "AllDialog.uax" ) == 0 )
		{
			RanOracle = true;
			static const char* Named[5]   = { "pc_hry_gotoquid_01", "PC_Ara_Adv9aForest_10", "pc_ara_adv9blair_05", "pc_ara_adv9blair_07", "pc_ara_adv9blair_09" };
			static const double RefR[5]   = { 0.9997, 0.9959, 0.9981, 0.9994, 0.9993 };
			static const double RefMad[5] = { 0.43, 1.36, 0.85, 0.80, 0.73 };
			int Checked = 0, Bad = 0;
			double MinR = 2.0, SumR = 0.0, MaxMad = 0.0, MaxRelMad = 0.0;
			for( size_t s = 0; s < Sounds.size() && Checked < 50; s++ )
			{
				const FSoundRec& S = Sounds[s];
				if( S.FileType != "XA" || !( S.CoreFlags & 128 ) || S.LipNum < 3 )
					continue;
				const std::vector<int16_t> Pcm = DecodeChunked( Bytes, S, 16384 );
				const FOracleResult O = LipSyncOracle( Pcm, Bytes.data() + S.LipPos, S.LipNum, S.SampleRate );
				const bool Pass = O.R > 0.99 && O.Mad < 0.02 * O.MaxLip;
				if( Checked < 5 )
				{
					const bool SameName = strcasecmp( S.Name.c_str(), Named[Checked] ) == 0;
					printf( "  oracle %-24s n=%-6d frames=%-4d r=%.4f (ref %.4f)  mad=%.2f (ref %.2f)  max lip/pred %d/%d%s\n",
						S.Name.c_str(), S.NumSamples, O.Frames, O.R, RefR[Checked], O.Mad, RefMad[Checked], O.MaxLip, O.MaxPred,
						SameName ? "" : "  [NOT the reference sound]" );
					if( !SameName || fabs( O.R - RefR[Checked] ) > 0.00015 || fabs( O.Mad - RefMad[Checked] ) > 0.0051 )
					{
						fprintf( stderr, "FAIL oracle %s does not reproduce the reference numbers\n", S.Name.c_str() );
						Bad++;
					}
				}
				if( !Pass )
				{
					fprintf( stderr, "FAIL oracle %s: r=%.4f mad=%.2f\n", S.Name.c_str(), O.R, O.Mad );
					Bad++;
				}
				MinR    = std::min( MinR, O.R );
				MaxMad  = std::max( MaxMad, O.Mad );
				MaxRelMad = std::max( MaxRelMad, O.MaxLip ? O.Mad / O.MaxLip : 1.0 );
				SumR   += O.R;
				Checked++;
			}
			if( Checked < 50 )
			{
				fprintf( stderr, "FAIL oracle: only %d lip-synced XA sounds found\n", Checked );
				Bad++;
			}
			printf( "  oracle: %d lip-synced XA sounds, r min %.4f mean %.4f, mad max %.2f (max %.2f%% of the line's peak) -> %s\n",
				Checked, MinR, Checked ? SumR / Checked : 0.0, MaxMad, 100.0 * MaxRelMad, Bad ? "FAIL" : "PASS" );
			Failures += Bad;
		}
	}

	printf( "census: %ld sounds, %ld XA (%lld bytes; %ld flags 260, %ld flags 388; %ld raw 0xEE frames), %ld wav (%ld flags 0, %ld flags 128)\n",
		TotalSounds, XaSounds, XaBytes, Xa260, Xa388, RawFrames, WavSounds, Wav0, Wav128 );
	printf( "        %ld XA samples decoded, %ld at the 16-bit rails (%.4f%%)\n", XaSamples, RailHits, XaSamples ? 100.0 * RailHits / XaSamples : 0.0 );
	if( WholeDirectory )
	{
		// The known census -- only meaningful for the whole Sounds/ directory.
		const bool Census = TotalSounds == 3289 && XaSounds == 2994 && XaBytes == 121218855LL && Xa260 == 626 && Xa388 == 2368
			&& WavSounds == 295 && Wav0 == 218 && Wav128 == 77;
		printf( "census vs §11.4 (3289 / 2994 XA = 121218855 bytes / 626+2368 / 295 wav = 218+77): %s\n", Census ? "MATCH" : "MISMATCH" );
		if( !Census )
			Failures++;
	}
	if( !RanOracle )
		printf( "note: no AllDialog.uax given -- the lip-sync oracle did not run\n" );
	printf( "%s (%d failure%s)\n", Failures ? "FAILED" : "ALL PASSED", Failures, Failures == 1 ? "" : "s" );
	return Failures ? 1 : 0;
}
