/*=============================================================================
	EAXADecoder.cpp: EA-XA ADPCM (15-byte frames, 28 mono samples). See EAXADecoder.h.
=============================================================================*/

#include "EAXADecoder.h"

static const int32_t GEAXACoef1[4] = { 0, 240, 460, 392 };
static const int32_t GEAXACoef2[4] = { 0, 0, -208, -220 };

static inline int16_t EAXAClamp16( int32_t V )
{
	return (int16_t)( V < -32768 ? -32768 : V > 32767 ? 32767 : V );
}

static inline int32_t EAXAReadBE16( const uint8_t* P )
{
	return (int16_t)(uint16_t)( ( (uint32_t)P[0] << 8 ) | P[1] );
}

int32_t FEAXADecoder::DecodeFrame( const uint8_t* In, int32_t Avail, int32_t& H1, int32_t& H2, int16_t* Out )
{
	if( Avail < EAXA_FRAME_BYTES )
		return 0;
	const uint8_t Header = In[0];
	if( Header == 0xEE )
	{
		// Raw frame: new history, then 28 big-endian PCM samples.
		if( Avail < EAXA_RAW_FRAME_BYTES )
			return 0;
		H1 = EAXAReadBE16( In + 1 );
		H2 = EAXAReadBE16( In + 3 );
		for( int32_t i = 0; i < EAXA_FRAME_SAMPLES; i++ )
			Out[i] = (int16_t)EAXAReadBE16( In + 5 + 2 * i );
		return EAXA_RAW_FRAME_BYTES;
	}
	// The predictor index is two bits in every HP2 frame (high nibble 0..3); mask so a corrupt byte cannot
	// index past the tables.
	const int32_t C1    = GEAXACoef1[( Header >> 4 ) & 3];
	const int32_t C2    = GEAXACoef2[( Header >> 4 ) & 3];
	const int32_t Shift = ( Header & 15 ) + 8;
	int32_t P1 = H1, P2 = H2;
	for( int32_t i = 0; i < EAXA_FRAME_SAMPLES; i++ )
	{
		const uint8_t Byte = In[1 + ( i >> 1 )];
		const uint32_t Nib = ( i & 1 ) ? ( Byte & 15 ) : ( Byte >> 4 );   // high nibble first
		const int32_t Delta = ( (int32_t)( Nib << 28 ) ) >> Shift;         // sign-extend, scale
		const int16_t S = EAXAClamp16( ( Delta + C1 * P1 + C2 * P2 + 128 ) >> 8 );
		Out[i] = S;
		P2 = P1;
		P1 = S;
	}
	H1 = P1;
	H2 = P2;
	return EAXA_FRAME_BYTES;
}

void FEAXADecoder::Init( const uint8_t* InSrc, int32_t InSrcBytes, int32_t InTotalSamples )
{
	Src          = InSrc;
	SrcBytes     = InSrc ? InSrcBytes : 0;
	TotalSamples = InTotalSamples;
	Rewind();
}

void FEAXADecoder::Rewind()
{
	Pos        = 0;
	Emitted    = 0;
	H1         = 0;
	H2         = 0;
	FrameCount = 0;
	FrameRead  = 0;
}

bool FEAXADecoder::AtEnd() const
{
	if( FrameRead < FrameCount )
		return false;
	if( TotalSamples > 0 && Emitted >= TotalSamples )
		return true;
	return SrcBytes - Pos < EAXA_FRAME_BYTES;
}

bool FEAXADecoder::NextFrame()
{
	const int32_t Used = DecodeFrame( Src + Pos, SrcBytes - Pos, H1, H2, Frame );
	if( !Used )
	{
		// Truncated tail (or nothing left): the sound ends here.
		Pos        = SrcBytes;
		FrameCount = FrameRead = 0;
		return false;
	}
	Pos       += Used;
	FrameCount = EAXA_FRAME_SAMPLES;
	FrameRead  = 0;
	return true;
}

int32_t FEAXADecoder::Decode( int16_t* Out, int32_t MaxSamples )
{
	int32_t Written = 0;
	while( Written < MaxSamples )
	{
		if( TotalSamples > 0 && Emitted >= TotalSamples )
			break;
		if( FrameRead >= FrameCount && !NextFrame() )
			break;
		int32_t Take = FrameCount - FrameRead;
		if( Take > MaxSamples - Written )
			Take = MaxSamples - Written;
		if( TotalSamples > 0 && Take > TotalSamples - Emitted )
			Take = TotalSamples - Emitted;
		for( int32_t i = 0; i < Take; i++ )
			Out[Written + i] = Frame[FrameRead + i];
		FrameRead += Take;
		Written   += Take;
		Emitted   += Take;
	}
	return Written;
}
