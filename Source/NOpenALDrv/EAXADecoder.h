/*=============================================================================
	EAXADecoder.h: the EA-XA ADPCM decoder HP2's compressed sounds need.

	HP2's compressed sounds are EA's `sx -eaxa_blk -raw` output (Engine/Src/UnAudio.cpp, USound::CompressXA),
	which the shipped driver decodes with the closed `coda` library (CEAXABLKDec). This is an open decoder
	for that format — no container, no per-sound header, mono 16-bit, 2,994 of the 3,289 shipped sounds
	(FileType 'XA', CoreFlags & SF_Compressed).

	Format (reference decoder tools/dev/analysis/xadec.py, checked by Test/EAXAOracle.cpp):
	  a frame is 15 bytes -> 28 samples. Byte 0 = (predictor << 4) | shift, bytes 1..14 = 28 signed
	  nibbles, high nibble first. With h1/h2 the previous two output samples:
	      sample = ((nib << 28) >> (shift + 8)) + C1[p]*h1 + C2[p]*h2 + 128) >> 8,  clamped to 16 bits
	      C1 = { 0, 240, 460, 392 },  C2 = { 0, 0, -208, -220 }
	  A header byte of 0xEE is a raw frame of 61 bytes: new h1, h2 (big-endian 16-bit) then 28 raw BE16
	  samples (none occur in HP2's data; decoded for completeness, as ffmpeg's ADPCM_EA_R1 does).
	  The decoder state starts at h1 = h2 = 0, as coda's CEAXABLKDec::SetState({0,0}) sets it.

	Engine-independent on purpose (plain <stdint.h> C++): the driver streams through it in chunks and the
	native unit test links it on its own.
=============================================================================*/

#ifndef _INC_EAXADECODER
#define _INC_EAXADECODER

#include <stdint.h>

// Bytes and samples of one EA-XA block.
#define EAXA_FRAME_BYTES     15
#define EAXA_FRAME_SAMPLES   28
#define EAXA_RAW_FRAME_BYTES 61

// Streaming decoder over one in-memory EA-XA byte run. Holds no allocation; the caller keeps Src alive.
struct FEAXADecoder
{
	const uint8_t* Src;        // the encoded bytes (USound::Data)
	int32_t        SrcBytes;   // their count
	int32_t        Pos;        // next byte to decode
	int32_t        TotalSamples; // samples the sound declares (raw_NumSamples); <= 0 = every whole frame
	int32_t        Emitted;    // samples handed out so far
	int32_t        H1, H2;     // predictor history (last two output samples)
	int16_t        Frame[EAXA_FRAME_SAMPLES];
	int32_t        FrameCount; // samples in Frame
	int32_t        FrameRead;  // of which already handed out

	FEAXADecoder() { Init( 0, 0, 0 ); }

	// Point the decoder at a byte run and reset it to the start.
	void Init( const uint8_t* InSrc, int32_t InSrcBytes, int32_t InTotalSamples );

	// Back to the first sample (a looping sound re-feeds the same data).
	void Rewind();

	// Decode up to MaxSamples mono samples into Out. Returns the number written; 0 = end of the sound.
	int32_t Decode( int16_t* Out, int32_t MaxSamples );

	// True once every sample has been handed out.
	bool AtEnd() const;

	// Decode one frame at Src (Avail bytes available) into Out[28], updating H1/H2. Returns the bytes the
	// frame occupies (15 or 61), or 0 if Avail is too short for a whole frame.
	static int32_t DecodeFrame( const uint8_t* In, int32_t Avail, int32_t& H1, int32_t& H2, int16_t* Out );

	// Bytes a sound of NumSamples occupies when every frame is a plain 15-byte one.
	static int32_t EncodedBytes( int32_t NumSamples ) { return ( ( NumSamples + EAXA_FRAME_SAMPLES - 1 ) / EAXA_FRAME_SAMPLES ) * EAXA_FRAME_BYTES; }

private:
	bool NextFrame();
};

#endif
