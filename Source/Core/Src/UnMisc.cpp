/*=============================================================================
	UnMisc.cpp: Various core platform-independent functions.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

// Core includes.
#include "CorePrivate.h"

/*-----------------------------------------------------------------------------
	FOutputDevice implementation.
-----------------------------------------------------------------------------*/

void FOutputDevice::Log( EName Event, const TCHAR* Str )
{
	if( !FName::SafeSuppressed(Event) )
		Serialize( Str, Event );
}
void FOutputDevice::Log( const TCHAR* Str )
{
	if( !FName::SafeSuppressed(NAME_Log) )
		Serialize( Str, NAME_Log );
}
void FOutputDevice::Log( const FString& S )
{
	if( !FName::SafeSuppressed(NAME_Log) )
		Serialize( *S, NAME_Log );
}
void FOutputDevice::Log( enum EName Type, const FString& S )
{
	if( !FName::SafeSuppressed(Type) )
		Serialize( *S, Type );
}
void FOutputDevice::Logf( EName Event, const TCHAR* Fmt, ... )
{
	if( !FName::SafeSuppressed(Event) )
	{
		TCHAR TempStr[4096];
		GET_VARARGS(TempStr,ARRAY_COUNT(TempStr),Fmt);
		Serialize( TempStr, Event );
	}
}
void FOutputDevice::Logf( const TCHAR* Fmt, ... )
{
	if( !FName::SafeSuppressed(NAME_Log) )
	{
		TCHAR TempStr[4096];
		GET_VARARGS(TempStr,ARRAY_COUNT(TempStr),Fmt);
		Serialize( TempStr, NAME_Log );
	}
}

/*-----------------------------------------------------------------------------
	FString implementation.
-----------------------------------------------------------------------------*/

FString FString::Chr( TCHAR Ch )
{
	guardSlow(FString::Chr);
	TCHAR Temp[2]={Ch,0};
	return FString(Temp);
	unguardSlow;
}

FString FString::LeftPad( INT ChCount )
{
	guardSlow(FString::LeftPad);
	INT Pad = ChCount - Len();
	if( Pad > 0 )
	{
		TCHAR* Ch = (TCHAR*)appAlloca((Pad+1)*sizeof(TCHAR));
		INT i;
		for( i=0; i<Pad; i++ )
			Ch[i] = ' ';
		Ch[i] = 0;
		return FString(Ch) + *this;
	}
	else return *this;
	unguardSlow;
}
FString FString::RightPad( INT ChCount )
{
	guardSlow(FString::RightPad);
	INT Pad = ChCount - Len();
	if( Pad > 0 )
	{
		TCHAR* Ch = (TCHAR*)appAlloca((Pad+1)*sizeof(TCHAR));
		INT i;
		for( i=0; i<Pad; i++ )
			Ch[i] = ' ';
		Ch[i] = 0;
		return *this + (const TCHAR*)Ch;
	}
	else return *this;
	unguardSlow;
}
FString::FString( BYTE Arg, INT Digits )
: TArray<TCHAR>()
{
}
FString::FString( SBYTE Arg, INT Digits )
: TArray<TCHAR>()
{
}
FString::FString( _WORD Arg, INT Digits )
: TArray<TCHAR>()
{
}
FString::FString( SWORD Arg, INT Digits )
: TArray<TCHAR>()
{
}
FString::FString( INT Arg, INT Digits )
: TArray<TCHAR>()
{
}
FString::FString( DWORD Arg, INT Digits )
: TArray<TCHAR>()
{
}
FString::FString( FLOAT Arg, INT Digits, INT RightDigits, UBOOL LeadZero )
: TArray<TCHAR>()
{
}
FString FString::Printf( const TCHAR* Fmt, ... )
{
	TCHAR TempStr[4096];
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), Fmt );
	return FString(TempStr);
}
CORE_API FArchive& operator<<( FArchive& Ar, FString& A )
{
	guard(FString<<);
	A.CountBytes( Ar );
	INT SaveNum = appIsPureAnsi(*A) ? A.Num() : -A.Num();
	Ar << AR_INDEX(SaveNum);
	if( Ar.IsLoading() )
	{
		A.ArrayMax = A.ArrayNum = Abs(SaveNum);
		A.Realloc( sizeof(TCHAR) );
		if( SaveNum>=0 )
			for( INT i=0; i<A.Num(); i++ )
				{ANSICHAR ACh; Ar << *(BYTE*)&ACh; A(i)=FromAnsi(ACh);}
		else
			for( INT i=0; i<A.Num(); i++ )
				{UNICHAR UCh; Ar << UCh; A(i)=FromUnicode(UCh);}
		if( Ar.IsLoading() && A.Num()==1 )
			A.Empty();
	}
	else
	{
		if( SaveNum>=0 )
			for( INT i=0; i<A.Num(); i++ )
				{ANSICHAR ACh=ToAnsi(A(i)); Ar << *(BYTE*)&ACh;}
		else
			for( INT i=0; i<A.Num(); i++ )
				{UNICHAR UCh=ToUnicode(A(i)); Ar << UCh;}
	}
	return Ar;
	unguard;
}
CORE_API FArchive& operator<<( FArchive& Ar, FTime& T )
{
	union {struct FDoubleIEEE {QWORD man:52; QWORD exp:11; QWORD sgn:1;} s; QWORD m;} D;
	union {struct FFloatIEEE  {QWORD man:23; QWORD exp:8;  QWORD sgn:1;} s; FLOAT m;} F;
	F.m=T.GetFloat();
	D.s.man=F.s.man; D.s.exp=F.s.exp; D.s.sgn=F.s.sgn;
	Ar.ByteOrderSerialize( &D.m, sizeof(D.m) );
	F.s.man=D.s.man; F.s.exp=D.s.exp; F.s.sgn=D.s.sgn;
	T=FTime(F.m);
	return Ar;
}

/*-----------------------------------------------------------------------------
	String functions.
-----------------------------------------------------------------------------*/

//
// Returns whether the string is pure ANSI.
//
CORE_API UBOOL appIsPureAnsi( const TCHAR* Str )
{
#if UNICODE
	for( ; *Str; Str++ )
		if( *Str<0 || *Str>0xff )
			return 0;
#endif
	return 1;
}

//
// Failed assertion handler.
//warning: May be called at library startup time.
//
CORE_API void VARARGS appFailAssert( const ANSICHAR* Expr, const ANSICHAR* File, INT Line )
{
	appErrorf( TEXT("Assertion failed: %s [File:%s] [Line: %i]"), appFromAnsi(Expr), appFromAnsi(File), Line );
}

//
// Gets the extension of a file, such as "PCX".  Returns NULL if none.
// string if there's no extension.
//
CORE_API const TCHAR* appFExt( const TCHAR* fname )
{
	guard(appFExt);

	if( appStrstr(fname,TEXT(":")) )
		fname = appStrstr(fname,TEXT(":"))+1;

	while( appStrstr(fname,TEXT("/")) )
		fname = appStrstr(fname,TEXT("/"))+1;

	while( appStrstr(fname,TEXT(".")) )
		fname = appStrstr(fname,TEXT("."))+1;

	return fname;
	unguard;
}

//
// Get a static result string.
//
CORE_API TCHAR* appStaticString1024()
{
	guard(appStaticString1024);
	static TCHAR Results[128][1024];
	static INT Count=0;
	TCHAR* Result = Results[Count++ & 127];
	*Result = 0;
	return Result;
	unguard;
}

//
// Get an ANSI static string.
//
CORE_API ANSICHAR* appAnsiStaticString1024()
{
	return (ANSICHAR*)appStaticString1024();
}

//
// Find string in string, case insensitive, requires non-alphanumeric lead-in.
//
const TCHAR* appStrfind( const TCHAR* Str, const TCHAR* Find )
{
	guard(appStrfind);	
	UBOOL Alnum  = 0;
	TCHAR f      = (*Find<'a' || *Find>'z') ? (*Find) : (*Find+'A'-'a');
	INT   Length = appStrlen(Find++)-1;
	TCHAR c      = *Str++;
	while( c )
	{
		if( c>='a' && c<='z' )
			c += 'A'-'a';
		if( !Alnum && c==f && !appStrnicmp(Str,Find,Length) )
			return Str-1;
		Alnum = (c>='A' && c<='Z') || (c>='0' && c<='9');
		c = *Str++;
	}
	return NULL;
	unguard;
}

//
// Returns a certain number of spaces.
// Only one return value is valid at a time.
//
const TCHAR* appSpc( INT Num )
{
	guard(spc);
	static TCHAR Spacing[256];
	static INT OldNum=-1;
	if( Num != OldNum )
	{
		for( OldNum=0; OldNum<Num; OldNum++ )
			Spacing[OldNum] = ' ';
		Spacing[Num] = 0;
	}
	return Spacing;
	unguard;
}

// 
// Trim spaces from an asciiz string by zeroing them.
//
void appTrimSpaces( ANSICHAR* String )
{		
	guard(appTrimSpaces);
	// Find 0 terminator.
	INT t=0;
	while( (String[t]!=0 ) && (t< 1024) ) t++;
	if (t>0) t--;
	// Zero trailing spaces.
	while( (String[t]==32) && (t>0) )
	{
		String[t]=0;
		t--;
	}
	unguard;
}



/*-----------------------------------------------------------------------------
	Memory functions.
-----------------------------------------------------------------------------*/

//
// Memory functions.
//
CORE_API void appMemswap( void* Ptr1, void* Ptr2, DWORD Size )
{
	void* Temp = appAlloca(Size);
	appMemcpy( Temp, Ptr1, Size );
	appMemcpy( Ptr1, Ptr2, Size );
	appMemcpy( Ptr2, Temp, Size );
}

/*-----------------------------------------------------------------------------
	CRC functions.
-----------------------------------------------------------------------------*/

// CRC 32 polynomial.
#define CRC32_POLY 0x04c11db7
CORE_API DWORD GCRCTable[256];

//
// CRC32 computer based on CRC32_POLY.
//
DWORD appMemCrc( const void* InData, INT Length, DWORD CRC )
{
	guardSlow(appMemCrc);
	BYTE* Data = (BYTE*)InData;
	CRC = ~CRC;
	for( INT i=0; i<Length; i++ )
		CRC = (CRC << 8) ^ GCRCTable[(CRC >> 24) ^ Data[i]];
	return ~CRC;
	unguardSlow;
}

//
// String CRC.
//
DWORD appStrCrc( const TCHAR* Data )
{
	guardSlow(appStrCrc);
	INT Length = appStrlen( Data );
	DWORD CRC = 0xFFFFFFFF;
	for( INT i=0; i<Length; i++ )
	{
		TCHAR C   = Data[i];
		INT   CL  = (C&255);
		CRC       = (CRC << 8) ^ GCRCTable[(CRC >> 24) ^ CL];;
#if UNICODE
		INT   CH  = (C>>8)&255;
		CRC       = (CRC << 8) ^ GCRCTable[(CRC >> 24) ^ CH];;
#endif
	}
	return ~CRC;
	unguardSlow;
}

//
// String CRC, case insensitive.
//
DWORD appStrCrcCaps( const TCHAR* Data )
{
	guardSlow(appStrCrcCaps);
	INT Length = appStrlen( Data );
	DWORD CRC = 0xFFFFFFFF;
	for( INT i=0; i<Length; i++ )
	{
		TCHAR C   = appToUpper(Data[i]);
		INT   CL  = (C&255);
		CRC       = (CRC << 8) ^ GCRCTable[(CRC >> 24) ^ CL];
#if UNICODE
		INT   CH  = (C>>8)&255;
		CRC       = (CRC << 8) ^ GCRCTable[(CRC >> 24) ^ CH];
#endif
	}
	return ~CRC;
	unguardSlow;
}

//
// Returns smallest N such that (1<<N)>=Arg.
// Note: appCeilLogTwo(0)=0 because (1<<0)=1 >= 0.
//
static BYTE GLogs[257];
CORE_API BYTE appCeilLogTwo( DWORD Arg )
{
	if( --Arg == MAXDWORD )
		return 0;
	BYTE Shift = Arg<=0x10000 ? (Arg<=0x100?0:8) : (Arg<=0x1000000?16:24);
	return Shift + GLogs[Arg>>Shift];
}

/*-----------------------------------------------------------------------------
	MD5 functions, adapted from MD5 RFC by Brandon Reinhart
-----------------------------------------------------------------------------*/

//
// Constants for MD5 Transform.
//

enum {S11=7};
enum {S12=12};
enum {S13=17};
enum {S14=22};
enum {S21=5};
enum {S22=9};
enum {S23=14};
enum {S24=20};
enum {S31=4};
enum {S32=11};
enum {S33=16};
enum {S34=23};
enum {S41=6};
enum {S42=10};
enum {S43=15};
enum {S44=21};

static BYTE PADDING[64] = {
	0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0
};

//
// Basic MD5 transformations.
//
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

//
// Rotates X left N bits.
//
#define ROTLEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

//
// Rounds 1, 2, 3, and 4 MD5 transformations.
// Rotation is seperate from addition to prevent recomputation.
//
#define FF(a, b, c, d, x, s, ac) { \
	(a) += F ((b), (c), (d)) + (x) + (DWORD)(ac); \
	(a) = ROTLEFT ((a), (s)); \
	(a) += (b); \
}

#define GG(a, b, c, d, x, s, ac) { \
	(a) += G ((b), (c), (d)) + (x) + (DWORD)(ac); \
	(a) = ROTLEFT ((a), (s)); \
	(a) += (b); \
}

#define HH(a, b, c, d, x, s, ac) { \
	(a) += H ((b), (c), (d)) + (x) + (DWORD)(ac); \
	(a) = ROTLEFT ((a), (s)); \
	(a) += (b); \
}

#define II(a, b, c, d, x, s, ac) { \
	(a) += I ((b), (c), (d)) + (x) + (DWORD)(ac); \
	(a) = ROTLEFT ((a), (s)); \
	(a) += (b); \
}

//
// MD5 initialization.  Begins an MD5 operation, writing a new context.
//
CORE_API void appMD5Init( FMD5Context* context )
{
	context->count[0] = context->count[1] = 0;
	// Load magic initialization constants.
	context->state[0] = 0x67452301;
	context->state[1] = 0xefcdab89;
	context->state[2] = 0x98badcfe;
	context->state[3] = 0x10325476;
}

//
// MD5 block update operation.  Continues an MD5 message-digest operation,
// processing another message block, and updating the context.
//
CORE_API void appMD5Update( FMD5Context* context, BYTE* input, INT inputLen )
{
	INT i, index, partLen;

	// Compute number of bytes mod 64.
	index = (INT)((context->count[0] >> 3) & 0x3F);

	// Update number of bits.
	if ((context->count[0] += ((DWORD)inputLen << 3)) < ((DWORD)inputLen << 3))
		context->count[1]++;
	context->count[1] += ((DWORD)inputLen >> 29);

	partLen = 64 - index;

	// Transform as many times as possible.
	if (inputLen >= partLen) {
		appMemcpy( &context->buffer[index], input, partLen );
		appMD5Transform( context->state, context->buffer );
		for (i = partLen; i + 63 < inputLen; i += 64)
			appMD5Transform( context->state, &input[i] );
		index = 0;
	} else
		i = 0;

	// Buffer remaining input.
	appMemcpy( &context->buffer[index], &input[i], inputLen-i );
}

//
// MD5 finalization. Ends an MD5 message-digest operation, writing the
// the message digest and zeroizing the context.
// Digest is 16 BYTEs.
//
CORE_API void appMD5Final ( BYTE* digest, FMD5Context* context )
{
	BYTE bits[8];
	INT index, padLen;

	// Save number of bits.
	appMD5Encode( bits, context->count, 8 );

	// Pad out to 56 mod 64.
	index = (INT)((context->count[0] >> 3) & 0x3f);
	padLen = (index < 56) ? (56 - index) : (120 - index);
	appMD5Update( context, PADDING, padLen );

	// Append length (before padding).
	appMD5Update( context, bits, 8 );

	// Store state in digest
	appMD5Encode( digest, context->state, 16 );

	// Zeroize sensitive information.
	appMemset( context, 0, sizeof(*context) );
}

//
// MD5 basic transformation. Transforms state based on block.
//
CORE_API void appMD5Transform( DWORD* state, BYTE* block )
{
	DWORD a = state[0], b = state[1], c = state[2], d = state[3], x[16];

	appMD5Decode( x, block, 64 );

	// Round 1
	FF (a, b, c, d, x[ 0], S11, 0xd76aa478); /* 1 */
	FF (d, a, b, c, x[ 1], S12, 0xe8c7b756); /* 2 */
	FF (c, d, a, b, x[ 2], S13, 0x242070db); /* 3 */
	FF (b, c, d, a, x[ 3], S14, 0xc1bdceee); /* 4 */
	FF (a, b, c, d, x[ 4], S11, 0xf57c0faf); /* 5 */
	FF (d, a, b, c, x[ 5], S12, 0x4787c62a); /* 6 */
	FF (c, d, a, b, x[ 6], S13, 0xa8304613); /* 7 */
	FF (b, c, d, a, x[ 7], S14, 0xfd469501); /* 8 */
	FF (a, b, c, d, x[ 8], S11, 0x698098d8); /* 9 */
	FF (d, a, b, c, x[ 9], S12, 0x8b44f7af); /* 10 */
	FF (c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
	FF (b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
	FF (a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
	FF (d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
	FF (c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
	FF (b, c, d, a, x[15], S14, 0x49b40821); /* 16 */

	// Round 2
	GG (a, b, c, d, x[ 1], S21, 0xf61e2562); /* 17 */
	GG (d, a, b, c, x[ 6], S22, 0xc040b340); /* 18 */
	GG (c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
	GG (b, c, d, a, x[ 0], S24, 0xe9b6c7aa); /* 20 */
	GG (a, b, c, d, x[ 5], S21, 0xd62f105d); /* 21 */
	GG (d, a, b, c, x[10], S22,  0x2441453); /* 22 */
	GG (c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
	GG (b, c, d, a, x[ 4], S24, 0xe7d3fbc8); /* 24 */
	GG (a, b, c, d, x[ 9], S21, 0x21e1cde6); /* 25 */
	GG (d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
	GG (c, d, a, b, x[ 3], S23, 0xf4d50d87); /* 27 */
	GG (b, c, d, a, x[ 8], S24, 0x455a14ed); /* 28 */
	GG (a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
	GG (d, a, b, c, x[ 2], S22, 0xfcefa3f8); /* 30 */
	GG (c, d, a, b, x[ 7], S23, 0x676f02d9); /* 31 */
	GG (b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */

	// Round 3
	HH (a, b, c, d, x[ 5], S31, 0xfffa3942); /* 33 */
	HH (d, a, b, c, x[ 8], S32, 0x8771f681); /* 34 */
	HH (c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
	HH (b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
	HH (a, b, c, d, x[ 1], S31, 0xa4beea44); /* 37 */
	HH (d, a, b, c, x[ 4], S32, 0x4bdecfa9); /* 38 */
	HH (c, d, a, b, x[ 7], S33, 0xf6bb4b60); /* 39 */
	HH (b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
	HH (a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
	HH (d, a, b, c, x[ 0], S32, 0xeaa127fa); /* 42 */
	HH (c, d, a, b, x[ 3], S33, 0xd4ef3085); /* 43 */
	HH (b, c, d, a, x[ 6], S34,  0x4881d05); /* 44 */
	HH (a, b, c, d, x[ 9], S31, 0xd9d4d039); /* 45 */
	HH (d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
	HH (c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
	HH (b, c, d, a, x[ 2], S34, 0xc4ac5665); /* 48 */

	// Round 4
	II (a, b, c, d, x[ 0], S41, 0xf4292244); /* 49 */
	II (d, a, b, c, x[ 7], S42, 0x432aff97); /* 50 */
	II (c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
	II (b, c, d, a, x[ 5], S44, 0xfc93a039); /* 52 */
	II (a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
	II (d, a, b, c, x[ 3], S42, 0x8f0ccc92); /* 54 */
	II (c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
	II (b, c, d, a, x[ 1], S44, 0x85845dd1); /* 56 */
	II (a, b, c, d, x[ 8], S41, 0x6fa87e4f); /* 57 */
	II (d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
	II (c, d, a, b, x[ 6], S43, 0xa3014314); /* 59 */
	II (b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
	II (a, b, c, d, x[ 4], S41, 0xf7537e82); /* 61 */
	II (d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
	II (c, d, a, b, x[ 2], S43, 0x2ad7d2bb); /* 63 */
	II (b, c, d, a, x[ 9], S44, 0xeb86d391); /* 64 */

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;

	// Zeroize sensitive information.
	appMemset( x, 0, sizeof(x) );
}

//
// Encodes input (DWORD) into output (BYTE).
// Assumes len is a multiple of 4.
//
CORE_API void appMD5Encode( BYTE* output, DWORD* input, INT len )
{
	INT i, j;

	for (i = 0, j = 0; j < len; i++, j += 4) {
		output[j] = (BYTE)(input[i] & 0xff);
		output[j+1] = (BYTE)((input[i] >> 8) & 0xff);
		output[j+2] = (BYTE)((input[i] >> 16) & 0xff);
		output[j+3] = (BYTE)((input[i] >> 24) & 0xff);
	}
}

//
// Decodes input (BYTE) into output (DWORD).
// Assumes len is a multiple of 4.
//
CORE_API void appMD5Decode( DWORD* output, BYTE* input, INT len )
{
	INT i, j;

	for (i = 0, j = 0; j < len; i++, j += 4)
		output[i] = ((DWORD)input[j]) | (((DWORD)input[j+1]) << 8) |
		(((DWORD)input[j+2]) << 16) | (((DWORD)input[j+3]) << 24);
}

/*-----------------------------------------------------------------------------
	Exceptions.
-----------------------------------------------------------------------------*/

//
// Throw a string exception with a message.
//
CORE_API void VARARGS appThrowf( const TCHAR* Fmt, ... )
{
	static TCHAR TempStr[4096];
	GET_VARARGS(TempStr,ARRAY_COUNT(TempStr),Fmt);
	// Echo the reason first: a throw that nothing catches then still names its cause.
	printf("[appThrowf] %s\n", TempStr);
	fflush(stdout);
	throw( TempStr );
}

/*-----------------------------------------------------------------------------
	Parameter parsing.
-----------------------------------------------------------------------------*/

//
// Get a string from a text string.
//
CORE_API UBOOL Parse
(
	const TCHAR* Stream, 
	const TCHAR* Match,
	TCHAR*		 Value,
	INT			 MaxLen
)
{
	guard(ParseString);

	const TCHAR* Found = appStrfind(Stream,Match);
	const TCHAR* Start;

	if( Found )
	{
		Start = Found + appStrlen(Match);
		if( *Start == '\x22' )
		{
			// Quoted string with spaces.
			appStrncpy( Value, Start+1, MaxLen );
			Value[MaxLen-1]=0;
			TCHAR* Temp = appStrstr( Value, TEXT("\x22") );
			if( Temp != NULL )
				*Temp=0;
		}
		else
		{
			// Non-quoted string without spaces.
			appStrncpy( Value, Start, MaxLen );
			Value[MaxLen-1]=0;
			TCHAR* Temp;
			Temp = appStrstr( Value, TEXT(" ")  ); if( Temp ) *Temp=0;
			Temp = appStrstr( Value, TEXT("\r") ); if( Temp ) *Temp=0;
			Temp = appStrstr( Value, TEXT("\n") ); if( Temp ) *Temp=0;
			Temp = appStrstr( Value, TEXT("\t") ); if( Temp ) *Temp=0;
			Temp = appStrstr( Value, TEXT(",")  ); if( Temp ) *Temp=0;
		}
		return 1;
	}
	else return 0;
	unguard;
}

//
// See if a command-line parameter exists in the stream.
//
UBOOL CORE_API ParseParam( const TCHAR* Stream, const TCHAR* Param )
{
	guard(GetParam);
	const TCHAR* Start = Stream;
	if( *Stream )
		while( (Start=appStrfind(Start+1,Param)) != NULL )
			if( Start>Stream && (Start[-1]=='-' || Start[-1]=='/') )
				return 1;
	return 0;
	unguard;
}

// 
// Parse a string.
//
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, FString& Value )
{
	guard(FString::Parse);
	TCHAR Temp[4096]=TEXT("");
	if( ::Parse( Stream, Match, Temp, ARRAY_COUNT(Temp) ) )
	{
		Value = Temp;
		return 1;
	}
	else return 0;
	unguard;
}

//
// Parse a quadword.
//
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, QWORD& Value )
{
	guard(ParseQWORD);
	return Parse( Stream, Match, *(SQWORD*)&Value );
	unguard;
}

//
// Parse a signed quadword.
//
UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, SQWORD& Value )
{
	guard(ParseSQWORD);
	TCHAR Temp[4096]=TEXT(""), *Ptr=Temp;
	if( ::Parse( Stream, Match, Temp, ARRAY_COUNT(Temp) ) )
	{
		Value = 0;
		UBOOL Negative = (*Ptr=='-');
		Ptr += Negative;
		while( *Ptr>='0' && *Ptr<='9' )
			Value = Value*10 + *Ptr++ - '0';
		if( Negative )
			Value = -Value;
		return 1;
	}
	else return 0;
	unguard;
}

//
// Get an object from a text stream.
//
CORE_API UBOOL ParseObject( const TCHAR* Stream, const TCHAR* Match, UClass* Class, UObject*& DestRes, UObject* InParent )
{
	guard(ParseUObject);
	TCHAR TempStr[256];
	if( !Parse( Stream, Match, TempStr, NAME_SIZE ) )
	{
		return 0;
	}
	else if( appStricmp(TempStr,TEXT("NONE"))==0 )
	{
		DestRes = NULL;
		return 1;
	}
	else
	{
		// Look this object up.
		UObject* Res;
		Res = UObject::StaticFindObject( Class, InParent, TempStr );
		if( !Res )
			return 0;
		DestRes = Res;
		return 1;
	}
	unguard;
}

//
// Get a name.
//
CORE_API UBOOL Parse
(
	const TCHAR* Stream, 
	const TCHAR* Match, 
	FName& Name
)
{
	guard(ParseFName);
	TCHAR TempStr[NAME_SIZE];

	if( !Parse(Stream,Match,TempStr,NAME_SIZE) )
		return 0;
	Name = FName( TempStr );

	return 1;
	unguard;
}

//
// Get a DWORD.
//
CORE_API UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, DWORD& Value )
{
	guard(ParseDWORD);

	const TCHAR* Temp = appStrfind(Stream,Match);
	TCHAR* End;
	if( Temp==NULL )
		return 0;
	Value = appStrtoi( Temp + appStrlen(Match), &End, 10 );

	return 1;
	unguard;
}

//
// Get a byte.
//
UBOOL CORE_API Parse( const TCHAR* Stream, const TCHAR* Match, BYTE& Value )
{
	guard(ParseBYTE);

	const TCHAR* Temp = appStrfind(Stream,Match);
	if( Temp==NULL )
		return 0;
	Temp += appStrlen( Match );
	Value = (BYTE)appAtoi( Temp );
	return Value!=0 || appIsDigit(Temp[0]);

	unguard;
}

//
// Get a signed byte.
//
UBOOL CORE_API Parse( const TCHAR* Stream, const TCHAR* Match, SBYTE& Value )
{
	guard(ParseCHAR);
	const TCHAR* Temp = appStrfind(Stream,Match);
	if( Temp==NULL )
		return 0;
	Temp += appStrlen( Match );
	Value = appAtoi( Temp );
	return Value!=0 || appIsDigit(Temp[0]);
	unguard;
}

//
// Get a word.
//
UBOOL CORE_API Parse( const TCHAR* Stream, const TCHAR* Match, _WORD& Value )
{
	guard(ParseWORD);
	const TCHAR* Temp = appStrfind( Stream, Match );
	if( Temp==NULL )
		return 0;
	Temp += appStrlen( Match );
	Value = (_WORD)appAtoi( Temp );
	return Value!=0 || appIsDigit(Temp[0]);
	unguard;
}

//
// Get a signed word.
//
UBOOL CORE_API Parse( const TCHAR* Stream, const TCHAR* Match, SWORD& Value )
{
	guard(ParseSWORD);
	const TCHAR* Temp = appStrfind( Stream, Match );
	if( Temp==NULL )
		return 0;
	Temp += appStrlen( Match );
	Value = (SWORD)appAtoi( Temp );
	return Value!=0 || appIsDigit(Temp[0]);
	unguard;  
}

//
// Get a floating-point number.
//
UBOOL CORE_API Parse( const TCHAR* Stream, const TCHAR* Match, FLOAT& Value )
{
	guard(ParseFLOAT);
	const TCHAR* Temp = appStrfind( Stream, Match );
	if( Temp==NULL )
		return 0;
	Value = appAtof( Temp+appStrlen(Match) );
	return 1;
	unguard;
}

//
// Get a signed double word.
//
UBOOL CORE_API Parse( const TCHAR* Stream, const TCHAR* Match, INT& Value )
{
	guard(ParseINT);
	const TCHAR* Temp = appStrfind( Stream, Match );
	if( Temp==NULL )
		return 0;
	Value = appAtoi( Temp + appStrlen(Match) );
	return 1;
	unguard;
}

//
// Get a boolean value.
//
UBOOL CORE_API ParseUBOOL( const TCHAR* Stream, const TCHAR* Match, UBOOL& OnOff )
{
	guard(ParseUBOOL);
	TCHAR TempStr[16];
	if( Parse( Stream, Match, TempStr, 16 ) )
	{
		OnOff
		=	!appStricmp(TempStr,TEXT("On"))
		||	!appStricmp(TempStr,TEXT("True"))
		||	!appStricmp(TempStr,GTrue)
		||	!appStricmp(TempStr,TEXT("1"));
		return 1;
	}
	else return 0;
	unguard;
}

//
// Get a globally unique identifier.
//
CORE_API UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, class FGuid& Guid )
{
	guard(ParseGUID);

	TCHAR Temp[256];
	if( !Parse( Stream, Match, Temp, ARRAY_COUNT(Temp) ) )
		return 0;

	Guid.A = Guid.B = Guid.C = Guid.D = 0;
	if( appStrlen(Temp)==32 )
	{
		TCHAR* End;
		Guid.D = appStrtoi( Temp+24, &End, 16 ); Temp[24]=0;
		Guid.C = appStrtoi( Temp+16, &End, 16 ); Temp[16]=0;
		Guid.B = appStrtoi( Temp+8,  &End, 16 ); Temp[8 ]=0;
		Guid.A = appStrtoi( Temp+0,  &End, 16 ); Temp[0 ]=0;
	}
	return 1;

	unguard;
}

//
// Sees if Stream starts with the named command.  If it does,
// skips through the command and blanks past it.  Returns 1 of match,
// 0 if not.
//
CORE_API UBOOL ParseCommand
(
	const TCHAR** Stream, 
	const TCHAR*  Match
)
{
	guard(ParseCommand);

	while( (**Stream==' ')||(**Stream==9) )
		(*Stream)++;

	if( appStrnicmp(*Stream,Match,appStrlen(Match))==0 )
	{
		*Stream += appStrlen(Match);
		if( !appIsAlnum(**Stream) )
		{
			while ((**Stream==' ')||(**Stream==9)) (*Stream)++;
			return 1; // Success.
		}
		else
		{
			*Stream -= appStrlen(Match);
			return 0; // Only found partial match.
		}
	}
	else return 0; // No match.
	unguard;
}

//
// Get next command.  Skips past comments and cr's.
//
CORE_API void ParseNext( const TCHAR** Stream )
{
	guard(ParseNext);

	// Skip over spaces, tabs, cr's, and linefeeds.
	SkipJunk:
	while( **Stream==' ' || **Stream==9 || **Stream==13 || **Stream==10 )
		++*Stream;

	if( **Stream==';' )
	{
		// Skip past comments.
		while( **Stream!=0 && **Stream!=10 && **Stream!=13 )
			++*Stream;
		goto SkipJunk;
	}

	// Upon exit, *Stream either points to valid Stream or a nul.
	unguard;
}

//
// Is this command-line token a global option (Key=Value) rather than a URL?
//
CORE_API UBOOL appIsGlobalOptionToken( const TCHAR* Token )
{
	guard(appIsGlobalOptionToken);
	if( !Token || !*Token )
		return 0;
	for( const TCHAR* P=Token; *P; P++ )
	{
		if( *P == '?' )
			return 0;   // a URL's own options start here — the token is a URL
		if( *P == '=' )
			return P != Token;   // Key=Value, and Key is not empty
	}
	return 0;
	unguard;
}

//
// Apply every `-ini:<File>:[<Section>]:<Key>=<Value>` token on the command line to GConfig.
// `User` and `System` are the config cache's generic file names (FConfigCacheIni::Find translates them),
// so no path is hardcoded. Applied straight after GConfig->Init, before any class loads its config.
//
CORE_API void appApplyIniOverrides()
{
	guard(appApplyIniOverrides);
	if( !GConfig )
		return;
	const TCHAR* Stream = appCmdLine();
	TCHAR Token[1024];
	while( ParseToken( Stream, Token, ARRAY_COUNT(Token), 0 ) )
	{
		if( appStrnicmp( Token, TEXT("-ini:"), 5 )!=0 )
			continue;
		// -ini:<File>:[<Section>]:<Key>=<Value>
		TCHAR* File = Token + 5;
		TCHAR* Open = appStrstr( File, TEXT(":[") );
		if( !Open )
			continue;
		TCHAR* Section = Open + 2;
		TCHAR* Close = appStrstr( Section, TEXT("]:") );
		if( !Close )
			continue;
		TCHAR* Key = Close + 2;
		TCHAR* Eq = appStrstr( Key, TEXT("=") );
		if( !Eq || Eq==Key )
			continue;
		TCHAR* Value = Eq + 1;
		*Open = 0; *Close = 0; *Eq = 0;
		if( !*File || !*Section )
			continue;
		GConfig->SetString( Section, Key, Value, File );
		debugf( NAME_Init, TEXT("Ini override: [%s] %s=%s (%s)"), Section, Key, Value, File );
	}
	unguard;
}

//
// Grab the next space-delimited string from the input stream.
// If quoted, gets entire quoted string.
//
CORE_API UBOOL ParseToken( const TCHAR*& Str, TCHAR* Result, INT MaxLen, UBOOL UseEscape )
{
	guard(ParseToken);
	INT Len=0;

	// Skip spaces and tabs.
	while( *Str==' ' || *Str==9 )
		Str++;
	if( *Str == 34 )
	{
		// Get quoted string.
		Str++;
		while( *Str && *Str!=34 && (Len+1)<MaxLen )
		{
			TCHAR c = *Str++;
			if( c=='\\' && UseEscape )
			{
				// Get escape.
				c = *Str++;
				if( !c )
					break;
			}
			if( (Len+1)<MaxLen )
				Result[Len++] = c;
		}
		if( *Str==34 )
			Str++;
	}
	else
	{
		// Get unquoted string.
		for( ; *Str && *Str!=' ' && *Str!=9; Str++ )
			if( (Len+1)<MaxLen )
				Result[Len++] = *Str;
	}
	Result[Len]=0;
	return Len!=0;
	unguard;
}
CORE_API UBOOL ParseToken( const TCHAR*& Str, FString& Arg, UBOOL UseEscape )
{
	TCHAR Buffer[1024];
	if( ParseToken( Str, Buffer, ARRAY_COUNT(Buffer), UseEscape ) )
	{
		Arg = Buffer;
		return 1;
	}
	return 0;
}
CORE_API FString ParseToken( const TCHAR*& Str, UBOOL UseEscape )
{
	TCHAR Buffer[1024];
	if( ParseToken( Str, Buffer, ARRAY_COUNT(Buffer), UseEscape ) )
		return Buffer;
	else
		return TEXT("");
}

//
// Get a line of Stream (everything up to, but not including, CR/LF.
// Returns 0 if ok, nonzero if at end of stream and returned 0-length string.
//
CORE_API UBOOL ParseLine
(
	const TCHAR**	Stream,
	TCHAR*			Result,
	INT				MaxLen,
	UBOOL			Exact
)
{
	guard(ParseLine);
	UBOOL GotStream=0;
	UBOOL IsQuoted=0;
	UBOOL Ignore=0;

	*Result=0;
	while( **Stream!=0 && **Stream!=10 && **Stream!=13 && --MaxLen>0 )
	{
		// Start of comments.
		if( !IsQuoted && !Exact && (*Stream)[0]=='/' && (*Stream)[1]=='/' )
			Ignore = 1;
		
		// Command chaining.
		if( !IsQuoted && !Exact && **Stream=='|' )
			break;

		// Check quoting.
		IsQuoted = IsQuoted ^ (**Stream==34);
		GotStream=1;

		// Got stuff.
		if( !Ignore )
			*(Result++) = *((*Stream)++);
		else
			(*Stream)++;
	}
	if( Exact )
	{
		// Eat up exactly one CR/LF.
		if( **Stream == 13 )
			(*Stream)++;
		if( **Stream == 10 )
			(*Stream)++;
	}
	else
	{
		// Eat up all CR/LF's.
		while( **Stream==10 || **Stream==13 || **Stream=='|' )
			(*Stream)++;
	}
	*Result=0;
	return **Stream!=0 || GotStream;
	unguard;
}
CORE_API UBOOL ParseLine
(
	const TCHAR**	Stream,
	FString&		Result,
	UBOOL			Exact
)
{
	guard(ParseLine);
	TCHAR Temp[4096]=TEXT("");
	UBOOL Success = ParseLine( Stream, Temp, ARRAY_COUNT(Temp), Exact );
	Result = Temp;
	return Success;
	unguard;
}

/*----------------------------------------------------------------------------
	String substitution.
----------------------------------------------------------------------------*/

CORE_API FString appFormat( FString Src, const TMultiMap<FString,FString>& Map )
{
	guard(appFormat);
	FString Result;
	for( INT Toggle=0; ; Toggle^=1 )
	{
		INT Pos=Src.InStr(TEXT("%")), NewPos=Pos>=0 ? Pos : Src.Len();
		FString Str = Src.Left( NewPos );
		if( Toggle )
		{
			const FString* Ptr = Map.Find( Str );
			if( Ptr )
				Result += *Ptr;
			else if( NewPos!=Src.Len() )
				Result += US + TEXT("%") + Str + TEXT("%");
			else
				Result += US + TEXT("%") + Str;
		}
		else Result += Str;
		Src = Src.Mid( NewPos+1 );
		if( Pos<0 )
			break;
	}
	return Result;
	unguard;
}

/*----------------------------------------------------------------------------
	Localization.
----------------------------------------------------------------------------*/

CORE_API const TCHAR* Localize( const TCHAR* Section, const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt, UBOOL Optional )
{
	guard(Localize);
	TCHAR* Result = appStaticString1024();
	if( !GIsStarted || !GConfig )
	{
		appStrcpy( Result, Key );
		return Result;
	}
	TCHAR Filename[256];
	LangExt = LangExt ? LangExt : UObject::GetLanguage();
TryAgain:
	appSprintf( Filename, TEXT("%s.%s"), Package, LangExt );
	if( !GConfig->GetString( Section, Key, Result, 1024, Filename ) )
	{
		if( appStricmp(LangExt,TEXT("int"))!=0 )
		{
			LangExt = TEXT("int");
			goto TryAgain;
		}
		if( !Optional )
		{
			debugf( NAME_Localization, TEXT("No localization: %s.%s.%s (%s)"), Package, Section, Key, LangExt );
			appSprintf( Result, TEXT("<?%s?%s.%s.%s?>"), LangExt, Package, Section, Key );
		}
	}
	return Result;
	unguard;
}
CORE_API const TCHAR* LocalizeError( const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt )
{
	return Localize( TEXT("Errors"), Key, Package, LangExt );
}
CORE_API const TCHAR* LocalizeProgress( const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt )
{
	return Localize( TEXT("Progress"), Key, Package, LangExt );
}
CORE_API const TCHAR* LocalizeQuery( const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt )
{
	return Localize( TEXT("Query"), Key, Package, LangExt );
}
CORE_API const TCHAR* LocalizeGeneral( const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt )
{
	return Localize( TEXT("General"), Key, Package, LangExt );
}

#if UNICODE
CORE_API const TCHAR* Localize( const ANSICHAR* Section, const ANSICHAR* Key, const TCHAR* Package, const TCHAR* LangExt, UBOOL Optional )
{
	return Localize( appFromAnsi(Section), appFromAnsi(Key), Package, LangExt, Optional );
}
CORE_API const TCHAR* LocalizeError( const ANSICHAR* Key, const TCHAR* Package, const TCHAR* LangExt )
{
	return LocalizeError( appFromAnsi(Key), Package, LangExt );
}
CORE_API const TCHAR* LocalizeProgress( const ANSICHAR* Key, const TCHAR* Package, const TCHAR* LangExt )
{
	return LocalizeProgress( appFromAnsi(Key), Package, LangExt );
}
CORE_API const TCHAR* LocalizeQuery( const ANSICHAR* Key, const TCHAR* Package, const TCHAR* LangExt )
{
	return LocalizeQuery( appFromAnsi(Key), Package, LangExt );
}
CORE_API const TCHAR* LocalizeGeneral( const ANSICHAR* Key, const TCHAR* Package, const TCHAR* LangExt )
{
	return LocalizeGeneral( appFromAnsi(Key), Package, LangExt );
}
#endif

/*-----------------------------------------------------------------------------
	High level file functions.
-----------------------------------------------------------------------------*/

//
// Update file modification time.
//
CORE_API UBOOL appUpdateFileModTime( TCHAR* Filename )
{
	guard(appUpdateFileModTime);
	FArchive* Ar = GFileManager->CreateFileWriter(Filename,FILEWRITE_Append,GNull);
	if( Ar )
	{
		delete Ar;
		return 1;
	}
	return 0;
	unguard;
}

//
// Load a binary file to a dynamic array.
//
CORE_API UBOOL appLoadFileToArray( TArray<BYTE>& Result, const TCHAR* Filename, FFileManager* FileManager )
{
	guard(appLoadFileToArray);
	FArchive* Reader = FileManager->CreateFileReader( Filename );
	if( !Reader )
		return 0;
	Result.Set( Reader->TotalSize() );
	Reader->Serialize( &Result(0), Result.Num() );
	UBOOL Success = Reader->Close();
	delete Reader;
	return Success;
	unguard;
}

//
// Load a text file to an FString.
// Supports all combination of ANSI/Unicode files and platforms.
//
CORE_API UBOOL appLoadFileToString( FString& Result, const TCHAR* Filename, FFileManager* FileManager )
{
	guard(appLoadFileToString);
	FArchive* Reader = FileManager->CreateFileReader( Filename );
	if( !Reader )
		return 0;
	INT Size = Reader->TotalSize();
	TArray<ANSICHAR> Ch( Size+2 );
	Reader->Serialize( &Ch(0), Size );
	UBOOL Success = Reader->Close();
	delete Reader;
	Ch( Size+0 )=0;
	Ch( Size+1 )=0;
	TArray<TCHAR>& ResultArray = Result.GetCharArray();
	ResultArray.Empty();
	if( Size>=2 && !(Size&1) && (BYTE)Ch(0)==0xff && (BYTE)Ch(1)==0xfe )
	{
		// Unicode Intel byte order.
		ResultArray.Add( Size/sizeof(TCHAR) );
		for( INT i=0; i<ResultArray.Num()-1; i++ )
			ResultArray( i ) = FromUnicode( (_WORD)(ANSICHARU)Ch(i*2+2) + (_WORD)(ANSICHARU)Ch(i*2+3)*256 );
	}
	else if( Size>=2 && !(Size&1) && (BYTE)Ch(0)==0xfe && (BYTE)Ch(1)==0xff )
	{
		// Unicode non-Intel byte order.
		ResultArray.Add( Size/sizeof(TCHAR) );
		for( INT i=0; i<ResultArray.Num()-1; i++ )
			ResultArray( i ) = FromUnicode( (_WORD)(ANSICHARU)Ch(i*2+3) + (_WORD)(ANSICHARU)Ch(i*2+2)*256 );
	}
	else
	{
		// ANSI.
		ResultArray.Add( Size+1 );
		for( INT i=0; i<ResultArray.Num()-1; i++ )
			ResultArray( i ) = FromAnsi( Ch(i) );
	}
	ResultArray.Last() = 0;
	return Success;
	unguard;
}

//
// Save a binary array to a file.
//
CORE_API UBOOL appSaveArrayToFile( const TArray<BYTE>& Array, const TCHAR* Filename, FFileManager* FileManager )
{
	guard(appSaveArrayToFile);
	FArchive* Ar = FileManager->CreateFileWriter( Filename );
	if( !Ar )
		return 0;
	Ar->Serialize( const_cast<BYTE*>(&Array(0)), Array.Num() );
	delete Ar;
	return 1;
	unguard;
}

//
// Write the FString to a file.
// Supports all combination of ANSI/Unicode files and platforms.
//
CORE_API UBOOL appSaveStringToFile( const FString& String, const TCHAR* Filename, FFileManager* FileManager )
{
	guard(appSaveStringToFile);
	if( !String.Len() )
		return 0;
	FArchive* Ar = FileManager->CreateFileWriter( Filename );
	if( !Ar )
		return 0;
	UBOOL SaveAsUnicode=0, Success=1;
#if UNICODE
	for( INT i=0; i<String.Len(); i++ )
	{
		if( (*String)[i] != (TCHAR)(ANSICHARU)ToAnsi((*String)[i]) )
		{
			UNICHAR BOM = UNICODE_BOM;
			Ar->Serialize( &BOM, sizeof(BOM) );
			SaveAsUnicode = 1;
			break;
		}
	}
#endif
	if( SaveAsUnicode || sizeof(TCHAR)==1 )
	{
		Ar->Serialize( const_cast<TCHAR*>(*String), String.Len()*sizeof(TCHAR) );
	}
	else
	{
		TArray<ANSICHAR> AnsiBuffer(String.Len());
		for( INT i=0; i<String.Len(); i++ )
			AnsiBuffer(i) = ToAnsi((*String)[i]);
		Ar->Serialize( const_cast<ANSICHAR*>(&AnsiBuffer(0)), String.Len() );
	}
	delete Ar;
	if( !Success )
		GFileManager->Delete( Filename );
	return Success;
	unguard;
}

/*-----------------------------------------------------------------------------
	Files.
-----------------------------------------------------------------------------*/

//
// Case-insensitive match for one search path: does any file in `Dir` have the name `In` (or
// `In` + `Ext`) ignoring case? Fills `Out` with the first match's real, correctly-cased path and returns 1.
//
// `Dir` is a Paths= entry with its wildcard stripped ("../Textures/"); `Ext` is what followed the
// wildcard (".utx"), or NULL for a path that carries none.
//
static UBOOL appFindFileNoCase( const TCHAR* Dir, const TCHAR* In, const TCHAR* Ext, TCHAR* Out )
{
	guard(appFindFileNoCase);
	TCHAR Spec[256];
	appStrcpy( Spec, Dir );
	appStrcat( Spec, TEXT("*") );
	if( Ext )
		appStrcat( Spec, Ext );
	TArray<FString> Files = GFileManager->FindFiles( Spec, 1, 0 );

	TCHAR InExt[256];
	*InExt = 0;
	if( Ext )
	{
		appStrcpy( InExt, In );
		appStrcat( InExt, Ext );
	}
	for( INT i=0; Files.IsValidIndex(i); i++ )
	{
		if( appStricmp( *(Files(i)), In )==0 || (Ext && appStricmp( *(Files(i)), InExt )==0) )
		{
			appStrcpy( Out, Dir );
			appStrcat( Out, *(Files(i)) );
			if( GFileManager->FileSize( Out ) >= 0 )
			{
				debugf( TEXT("Case-insensitive search: %s -> %s"), In, Out );
				return 1;
			}
		}
	}
	return 0;
	unguard;
}

//
// Find a file.
//
UBOOL appFindPackageFile( const TCHAR* In, const FGuid* Guid, TCHAR* Out )
{
	guard(appFindPackageFile);
	TCHAR Temp[256];

	// Don't return it if it's a library.
	FPushMemTag Push(TEXT("appFindPackageFile"));
	if( appStrlen(In)>appStrlen(DLLEXT) && appStricmp( In + appStrlen(In)-appStrlen(DLLEXT), DLLEXT )==0 )
		return 0;

	// If using non-default language, search for internationalized version.
	UBOOL International = (appStricmp(UObject::GetLanguage(),TEXT("int"))!=0);

	// Try file as specified.
	appStrcpy( Out, In );
	if( GFileManager->FileSize( Out ) >= 0 )
		return 1;

	// Try all of the predefined paths.
	//
	// Each path is tried exactly and then ignoring case before the next one, so an earlier path's case-variant
	// hit beats a later path's exact hit, as on a case-insensitive filesystem.
	INT DoCd;
	for( DoCd=0; DoCd<(1+(GCdPath[0]!=0)); DoCd++ )
	{
		for( INT i=DoCd; i<GSys->Paths.Num()+(Guid!=NULL); i++ )
		{
			// Get directory only, once per path: the case-insensitive retry at the bottom needs it too.
			const TCHAR* Ext;
			*Temp = 0;
			if( DoCd )
			{
				appStrcat( Temp, GCdPath );
				appStrcat( Temp, TEXT("System") PATH_SEPARATOR );
			}
			if( i<GSys->Paths.Num() )
			{
				appStrcat( Temp, *GSys->Paths(i) );
				TCHAR* Ext2 = appStrstr(Temp,TEXT("*"));
				if( Ext2 )
					*Ext2++ = 0;
				Ext = Ext2;
			}
			else
			{
				appStrcat( Temp, *GSys->CachePath );
				appStrcat( Temp, PATH_SEPARATOR );
				Ext = *GSys->CacheExt;
			}
			for( INT j=0; j<International+1; j++ )
			{
				appStrcpy( Out, Temp );
				if( i<GSys->Paths.Num() )
					appStrcat( Out, In );
				else
					appStrcat( Out, Guid->String() );

				// Check for file.
				UBOOL Found = 0;
				Found = (GFileManager->FileSize(Out)>=0);
				if( !Found && Ext )
				{
					appStrcat( Out, TEXT(".") );
					if( International-j )
					{
						appStrcat( Out, UObject::GetLanguage() );
						appStrcat( Out, TEXT("_") );
					}
					appStrcat( Out, Ext+1 );
					Found = (GFileManager->FileSize( Out )>=0);
				}
				if( Found )
				{
					if( i==GSys->Paths.Num() )
						appUpdateFileModTime( Out );
					return 1;
				}
			}
			// Nothing matched exactly in this directory: try it ignoring case before moving on. Not for the
			// cache directory (i == Paths.Num()), whose files are named after a GUID, not the package.
			if( i<GSys->Paths.Num() && appFindFileNoCase( Temp, In, Ext, Out ) )
				return 1;
		}
	}

	// Not found.
	return 0;
	unguard;
}

//
// Create a temporary file.
//
CORE_API void appCreateTempFilename( const TCHAR* Path, TCHAR* Result256 )
{
	guard(appCreateTempFilename);
	static INT i=0;
	do
		appSprintf( Result256, TEXT("%s%04X.tmp"), Path, i++ );
	while( GFileManager->FileSize(Result256)>0 );
	unguard;
}


// TG ALPHA begin

//
// File Stream implementation.
//

FFileStream* FFileStream::Init( INT InMaxStreams )
{
	if (Instance == NULL)
	{
		Instance	= new FFileStream;
		Streams		= new FStream[InMaxStreams];
		MaxStreams	= InMaxStreams;
		appMemzero( Streams, sizeof(FStream)*InMaxStreams );
	}
	return Instance;
}

void FFileStream::Destroy()
{
	if (Instance != NULL )
	{
		Destroyed = 1;
		debugf(TEXT("Waiting for file streaming thread to finish..."));
#if _MSC_VER // only the Win32 core runs the streaming thread that ends this wait
//!!TODO: vogel: implement
		while (Destroyed == 1)
			appSleep(1);
#endif
		if ( Streams )
		{
			delete [] Streams;
			Streams = NULL;
		}
		delete Instance;
		Instance = NULL;
	}
}

void FFileStream::Enter( INT StreamId ) // has to be reentrant
{
	while (Streams[StreamId].Locked)
		appSleep(0); //!! appSleep has to be reentrant!
	Streams[StreamId].Locked = 1;
}

void FFileStream::Leave( INT StreamId ) // has to be reentrant
{
	Streams[StreamId].Locked = 0;
}

INT FFileStream::CreateStream( TLazyArray<BYTE>* RawData, INT NumSamples, INT ChunkSize, INT InitialChunks, void* Data, EFileStreamType Type, void* TDD )
{
	//	Find an available stream - fixed from Warfare code.  -tg
	//
	int	iStreamIndex = -1;

	for (int i = 0; i < MaxStreams && iStreamIndex == -1; i++)
	{
		Enter(i);

		if (!Streams[i].Used)
			iStreamIndex = i;

		Leave(i);
	}

	if (iStreamIndex == -1)
		return -1;

	Enter(iStreamIndex);

	//	debugf(TEXT("FFileStream::CreateStream: StreamID = %d"), iStreamIndex);

	//	Create() uses Type and TDD - must set up before calling.
	//
	Streams[iStreamIndex].Type				= Type;
	//Streams[iStreamIndex].TDD				= TDD;	// we now create the TDD object in Create()

	if (!Create( iStreamIndex, RawData, NumSamples ) )
	{
		appMemzero( &Streams[iStreamIndex], sizeof(Streams[iStreamIndex]) );
		Leave(iStreamIndex);
		return -1;
	}

	Streams[iStreamIndex].Data				= Data;
	Streams[iStreamIndex].ChunkSize			= ChunkSize;
	Streams[iStreamIndex].ChunksRequested	= 0;
	Streams[iStreamIndex].ChunksRemaining	= 0;
	Streams[iStreamIndex].EndOfFile			= 0;
	Streams[iStreamIndex].Used				= 1; 

	Read( iStreamIndex, ChunkSize * InitialChunks );

	Leave(iStreamIndex);

	return iStreamIndex;
}

INT FFileStream::CreateStream( const TCHAR* Filename, INT ChunkSize, INT InitialChunks, void* Data, EFileStreamType Type, void* TDD )
{
	//	Find an available stream - fixed from Warfare code.  -tg
	//
	int	iStreamIndex = -1;

	for (int i = 0; i < MaxStreams && iStreamIndex == -1; i++)
	{
		Enter(i);

		if (!Streams[i].Used)
			iStreamIndex = i;

		Leave(i);
	}

	if (iStreamIndex == -1)
		return -1;

	Enter(iStreamIndex);

	//	debugf(TEXT("FFileStream::CreateStream: StreamID = %d"), StreamIndex);

	//	Create() uses Type and TDD - must set up before calling.
	//
	Streams[iStreamIndex].Type				= Type;
	Streams[iStreamIndex].TDD				= TDD;

	if (!Create( iStreamIndex, Filename ) )
	{
		appMemzero( &Streams[iStreamIndex], sizeof(Streams[iStreamIndex]) );
		Leave(iStreamIndex);
		return -1;
	}

	Streams[iStreamIndex].Data				= Data;
	Streams[iStreamIndex].ChunkSize			= ChunkSize;
	Streams[iStreamIndex].ChunksRequested	= 0;
	Streams[iStreamIndex].ChunksRemaining	= 0;
	Streams[iStreamIndex].EndOfFile			= 0;
	Streams[iStreamIndex].Used				= 1; 

	Read( iStreamIndex, ChunkSize * InitialChunks );

	Leave(iStreamIndex);

	return iStreamIndex;
}

void FFileStream::DestroyStream( INT StreamId, UBOOL ReadQueuedChunks )
{
	Enter(StreamId);

//	debugf(NAME_DevSound, TEXT("FFileStream::DestroyStream: StreamID = %d"), StreamId);

	if (Streams[StreamId].Used)
	{
		if (ReadQueuedChunks)
		{
			Read( StreamId, Streams[StreamId].ChunkSize * Streams[StreamId].ChunksRequested ); 
			DecrementChunkCount(StreamId, Streams[StreamId].ChunksRequested);
		}
		Destroy(StreamId);
	}
	Leave(StreamId);
}

void FFileStream::RequestChunks( INT StreamId, INT Chunks, void* Data )
{
	Enter(StreamId);
	Streams[StreamId].ChunksRequested += Chunks;
//	Streams[StreamId].ChunksRemaining += Chunks;
	Streams[StreamId].Data = Data;
	Leave(StreamId);
}

void FFileStream::DecrementChunkCount( INT StreamId, INT NumChunks )
{
	Enter(StreamId);
	Streams[StreamId].ChunksRemaining -= NumChunks;
	Leave(StreamId);

	return;
}

INT FFileStream::ChunksRemaining( INT StreamId )
{
	Enter(StreamId);
	INT ChunksRemaining = Streams[StreamId].ChunksRemaining;
	Leave(StreamId);
	return ChunksRemaining;
}

UBOOL FFileStream::IsStreamAlive( INT StreamId )
{
	Enter(StreamId);
	UBOOL Alive	 = !Streams[StreamId].EndOfFile && Streams[StreamId].Used;
	Leave(StreamId);

	return Alive;
}

FFileStream* FFileStream::Instance	= 0;
FStream* FFileStream::Streams		= 0;
INT FFileStream::MaxStreams			= 0;
INT FFileStream::Destroyed			= 0;

// TG ALPHA end


/*-----------------------------------------------------------------------------
	Init and Exit.
-----------------------------------------------------------------------------*/

//
// General initialization.
//
static TCHAR GCmdLine[4096]=TEXT("");	// 4096, as the launchers' command-line buffer
CORE_API const TCHAR* appCmdLine()
{
	return GCmdLine;
}


CORE_API const TCHAR* appSaveSlotDir()
{
	return *GSys->SaveSlotPath;
}

CORE_API const TCHAR* appSetSaveSlotDir( int iSlot )
{
	// --- Set our SaveSlotPath
	TCHAR itoa[32];
	appSprintf( itoa, TEXT("%d"), iSlot );
	GSys->SaveSlotPath = GSys->SavePath * TEXT("Slot") + (const TCHAR*)itoa;
	return *GSys->SaveSlotPath;
}



CORE_API void appInit( const TCHAR* InPackage, const TCHAR* InCmdLine, FMalloc* InMalloc, FOutputDevice* InLog, FOutputDeviceError* InError, FFeedbackContext* InWarn, FFileManager* InFileManager, FConfigCache*(*ConfigFactory)(), UBOOL RequireConfig )
{
	guard(appInit);
	GFileManager = InFileManager;

	// Init CRC table.
    for( DWORD iCRC=0; iCRC<256; iCRC++ )
		for( DWORD c=iCRC<<24, j=8; j!=0; j-- )
			GCRCTable[iCRC] = c = c & 0x80000000 ? (c << 1) ^ CRC32_POLY : (c << 1);

	// Init log table.
	{for( INT i=0,e=-1,c=0; i<=256; i++ )
	{
		GLogs[i] = e+1;
		if( !i || ++c>=(1<<e) )
			c=0, e++;
	}}

	// Command line.
	#if _MSC_VER
		if( *InCmdLine=='\"' )
		{
			InCmdLine++;
			while( *InCmdLine && *InCmdLine!='\"' )
				InCmdLine++;
			if( *InCmdLine )
				InCmdLine++;
		}
		while( *InCmdLine && *InCmdLine!=' ' )
			InCmdLine++;
		if( *InCmdLine )
			InCmdLine++;
	#endif
	appStrncpy( GCmdLine, InCmdLine, ARRAY_COUNT(GCmdLine) );	// bounded

	// Error history.
	appStrcpy( GErrorHist, TEXT("General protection fault!\r\n\r\nHistory: ") );

	// Subsystems.
	GLog         = InLog;
	GError       = InError;
	GWarn        = InWarn;

	// Memory allocator.
	GMalloc = InMalloc;
	GMalloc->Init();

	// Init names.
	FName::StaticInit();

	// Platform specific pre-init.
	appPlatformPreInit();

	// Switch into executable's directory.
	GFileManager->Init(1);
	GFileManager->SetDefaultDirectory( appBaseDir() );

	// Command line.
	debugf( NAME_Init, TEXT("Version: %i"), ENGINE_VERSION );
	debugf( NAME_Init, TEXT("Compiled: %s %s"), appFromAnsi(__DATE__), appFromAnsi(__TIME__) );
	debugf( NAME_Init, TEXT("Command line: %s"), appCmdLine() );
	debugf( NAME_Init, TEXT("Base directory: %s"), appBaseDir() );
	debugf( NAME_Init, TEXT("Character set: %s"), sizeof(TCHAR)==1 ? TEXT("ANSI") : TEXT("Unicode") );

	// Parameters.
	GIsStrict = ParseParam( appCmdLine(), TEXT("STRICT") );

	// Ini.
	TCHAR GIni[512]=TEXT("");
	if( !Parse( appCmdLine(), TEXT("INI="), GIni, 256 ) )
		appSprintf( GIni, TEXT("%s%s.ini"), appUserDir(), InPackage );
	bool bDefaultIniChanged = false;
	if(   RequireConfig
	   && (   GFileManager->FileSize(GIni) < 0
	       || GFileManager->GetGlobalTime(TEXT("Default.ini"))  >  GFileManager->GetGlobalTime(GIni) //ft
	      )
	  )
	{
		// Create Package.ini from default.ini.
		FString S;
		if( !appLoadFileToString( S, TEXT("Default.ini"), GFileManager ) )
			appErrorf( LocalizeError("MissingIni Default.ini"), "Default.ini" );
		appSaveStringToFile( S, GIni );
		bDefaultIniChanged = true;
	}


	// User Ini.
	TCHAR GUserIni[512]=TEXT("");
	if( !Parse( appCmdLine(), TEXT("USERINI="), GUserIni, 256 ) )
		appSprintf( GUserIni, TEXT("%sUser.ini"), appUserDir() );
	if(   RequireConfig
	   && (   GFileManager->FileSize(GUserIni) < 0
	       || GFileManager->GetGlobalTime(TEXT("DefUser.ini"))  >  GFileManager->GetGlobalTime(GUserIni) //ft
	       || bDefaultIniChanged
	      )
	  )
	{
		// Create User.ini from DefUser.ini.
		FString S;
		if( !appLoadFileToString( S, TEXT("DefUser.ini"), GFileManager ) )
			appErrorf( LocalizeError("MissingIni DefUser.ini"), "DefUser.ini" );
		appSaveStringToFile( S, GUserIni );
	}

 	// Init config.
	GConfig = ConfigFactory();
	GConfig->Init( GIni, GUserIni, RequireConfig );

		// gk 10/15/01
	// "Default.ini" in its on-disk case (a case-sensitive filesystem misses "default.ini"), and Language is
	// only overwritten when Default.ini provides one: an empty Language breaks every Localize().
	{
		TCHAR DTemp[256]=TEXT("");
		FConfigCache* DConfig=ConfigFactory();
		DConfig->Init(TEXT("Default.ini"),GUserIni,RequireConfig);
		if( DConfig->GetString( TEXT("Engine.Engine"), TEXT("Language"), DTemp, ARRAY_COUNT(DTemp)) && *DTemp )
			GConfig->SetString( TEXT("Engine.Engine"), TEXT("Language"), DTemp );
	}

	// -ini:<File>:[<Section>]:<Key>=<Value> overrides, applied before any class loads config and after the
	// gk block, which would otherwise clobber a Language override.
	appApplyIniOverrides();



	// Language.
	TCHAR Temp[256];
	if( GConfig->GetString( TEXT("Engine.Engine"), TEXT("Language"), Temp, ARRAY_COUNT(Temp) ) )
		UObject::SetLanguage( Temp );
	// Say which language this run uses.
	debugf( NAME_Init, TEXT("Language: %s"), UObject::GetLanguage() );

	// Object initialization.
	UObject::StaticInit();

	// Memory initalization.
	GMem.Init( 65536, TEXT("GMem") );

	// Cd path.
	if( !Parse( appCmdLine(), TEXT("CDPATH="), GCdPath, 256 ) )
	{
		GConfig->GetString( TEXT("Engine.Engine"), TEXT("CdPath"), GCdPath, 256 );
		if( GFileManager->FileSize(TEXT("..") PATH_SEPARATOR TEXT("Textures") PATH_SEPARATOR TEXT("Palettes.utx"))>=0 )//oldver
			appStrcpy( GCdPath, TEXT("") );
	}
	if( *GCdPath && GCdPath[appStrlen(GCdPath)-1]!=PATH_SEPARATOR[0] )
		appStrcat( GCdPath, PATH_SEPARATOR );
	if( *GCdPath ) 
		debugf( TEXT("Cd Path: %s"), GCdPath );

	// Init the file streaming singleton.
	//
	INT		iMaxChannels;
	UBOOL	bSuccess;

	bSuccess = GConfig->GetInt( TEXT("ALAudio.ALAudioSubsystem"), TEXT("Channels"), iMaxChannels );
	GFileStream = FFileStream::Init( bSuccess ? iMaxChannels : 32 );

	// Platform specific init.
	appPlatformInit();
	
	if( *appUserDir() )
	{
		// Subdirectory of user folder.
		GSys->SavePath = FString(appUserDir()) * TEXT("Save");
		GSys->Paths[5]=*(GSys->SavePath * TEXT("*.usa"));
	}
	else
	{
		// Subdirectory of app folder.
		GSys->SavePath = TEXT("..") PATH_SEPARATOR TEXT("Save");
	}
	// Make our SavePath
	GFileManager->MakeDirectory( *GSys->SavePath, 0 );
	

	// --- Set our SaveSlotPath
	int iSlotNum;
	if( Parse( appCmdLine(), TEXT("SAVESLOT="), iSlotNum ) )
	{
		appSetSaveSlotDir( iSlotNum );
	}
	else // The SaveSlot was not set in the command line so set to Default
	{
		GSys->SaveSlotPath = GSys->SavePath;
	}
	
	if( *GSys->SaveSlotPath )
	{
		debugf( TEXT("Save Slot Path: %s"), *GSys->SaveSlotPath ); // FString through varargs traps under clang
		
		// Make our SaveSlotPath
		GFileManager->MakeDirectory( *GSys->SaveSlotPath, 0 );

		// Make our SaveSlotCachePath
		GFileManager->MakeDirectory( *(GSys->SaveSlotPath * TEXT("cache")), 0 );
	}
	
	
	// --- Set our maps path	
	// Get the destination path from a Paths array entry such as ../Maps/*.unr
	FString FileExt = TEXT(".unr");
	for(int i=0; i<GSys->Paths.Num(); i++ )
	{
		if( GSys->Paths(i).Right( FileExt.Len() ) == FileExt )
		{
			const TCHAR* p; // MSVC6 for-scope: used after the loop.
			for( p = *GSys->Paths(i) + GSys->Paths(i).Len() - 1; p >= *GSys->Paths(i); p-- )
			{
				if( *p=='/' || *p=='\\' || *p==':' )
					break;
			}
			GSys->MapPath = appBaseDir();
			GSys->MapPath = GSys->MapPath.Left( GSys->MapPath.InStr(TEXT("System")));
			GSys->MapPath += GSys->Paths(i).Left( p - *GSys->Paths(i)).Right(4);			
			break;
		}
	}
	
	// Clean our our unverified persistent actor files
	LoadPersistentActorCache();

	GConfig->SetString( TEXT("Core.System"), TEXT("SavePath"), *GSys->SaveSlotPath );
	GConfig->SetString( TEXT("Core.System"), TEXT("paths"), *(GSys->SaveSlotPath * TEXT("*.usa")) );
	
	GConfig->Flush( 0 );
	

	unguard;
}


CORE_API void SavePersistentActorCache( void )
{
	// *** Save our SavePersistent Actor Cache by copying all cached pa files into our imediate SaveSlotPath.
	if( !*GSys->SaveSlotPath )
		return;

	FString			Spec  = GSys->SaveSlotPath * TEXT("*_pa.*");
	TArray<FString> Files = GFileManager->FindFiles( *Spec, true, false );
	FString			PAFileName;
	
	// Delete all _pa files that are in the imediate SaveSlotPath
	INT j; // MSVC6 for-scope: reused below.
	for( j=0; Files.IsValidIndex(j); j++ )
	{
		PAFileName = GSys->SaveSlotPath * Files(j);
		if( GFileManager->FileSize( *PAFileName ) )
		{
			GFileManager->Delete( *PAFileName, false, true );
		}
	}
	Files.Empty();
	
	// Copy all the _pa files from the cache to the imediate SaveSlotPath
	Spec  = GSys->SaveSlotPath * TEXT("cache") * TEXT("*_pa.*");
	Files = GFileManager->FindFiles( *Spec, true, false );
	
	for( j=0; Files.IsValidIndex(j); j++ )
	{
		PAFileName = GSys->SaveSlotPath * TEXT("cache") * Files(j);
		if( GFileManager->FileSize( *PAFileName ) )
		{
			GFileManager->Copy( *(GSys->SaveSlotPath * Files(j)), *PAFileName, true, true );
		}
	}
}


CORE_API void LoadPersistentActorCache( void )
{
	// *** Load our SavePersistent Actor Cache by copying all pa files in our imediate SaveSlotPath into our cache dir
	if( !*GSys->SaveSlotPath )
		return;
	
	// Delete all cache _pa files
	FString			Spec  = GSys->SaveSlotPath * TEXT("cache") * TEXT("*.*");
	TArray<FString> Files = GFileManager->FindFiles( *Spec, true, false );
	FString			PAFileName;
	
	INT j; // MSVC6 for-scope: reused below.
	for( j=0; Files.IsValidIndex(j); j++ )
	{
		PAFileName = GSys->SaveSlotPath * TEXT("cache") * Files(j);
		if( GFileManager->FileSize( *PAFileName ) )
		{
			GFileManager->Delete( *PAFileName, false, true );
		}
	}
	Files.Empty();
	
	// Copy pa files from our saveGameSlot to our cache
	Spec  = GSys->SaveSlotPath * TEXT("*_pa.*");
	Files = GFileManager->FindFiles( *Spec, true, false );
		
	// Copy all _pa files from our SaveGame dir to our cache dir
	for( j=0; Files.IsValidIndex(j); j++ )
	{
		PAFileName = GSys->SaveSlotPath * Files(j);
		if( GFileManager->FileSize( *PAFileName ) )
		{
			GFileManager->Copy( *(GSys->SaveSlotPath * TEXT("cache") * Files(j)), *PAFileName, true, true );
		}
	}
}


//
// Pre-shutdown.
// Called from within guarded exit code, only during non-error exits.
//
CORE_API void appPreExit()
{
	guard(appPreExit);

	debugf( NAME_Exit, TEXT("Preparing to exit.") );
	appPlatformPreExit();
	GMem.Exit();
	UObject::StaticExit();

	unguard;
}

//
// Shutdown.
// Called outside guarded exit code, during all exits (including error exits).
//
void appExit()
{
	guard(appExit);
	debugf( NAME_Exit, TEXT("Exiting.") );
	appPlatformExit();
	if( GConfig )
	{
		GConfig->Exit();
		delete GConfig;
		GConfig = NULL;
	}
	FName::StaticExit();
	if( !GIsCriticalError )
		GMalloc->DumpAllocs();
	unguard;
}

/*-----------------------------------------------------------------------------
	String conversion.
-----------------------------------------------------------------------------*/

// TCHAR to ANSICHAR.
CORE_API const ANSICHAR* appToAnsi( const TCHAR* Str )
{
#if UNICODE
	guard(appToAnsi);
	if( !Str )
		return NULL;
	ANSICHAR* ACh = appAnsiStaticString1024();
	INT Count;
	for( Count=0; Count<1024-1 && Str[Count]; Count++ )
		ACh[Count] = ToAnsi( Str[Count] );
	ACh[Count] = 0;
	return ACh;
	unguard;
#else
	return Str;
#endif
}

// TCHAR to UNICHAR.
CORE_API const UNICHAR* appToUnicode( const TCHAR* Str )
{
#if UNICODE
	return Str;
#else
	guard(appToUnicode);
	if( !Str )
		return NULL;
	static UNICHAR UCh[1024];
	INT Count;
	for( Count=0; Count<ARRAY_COUNT(UCh)-1 && Str[Count]; Count++ )
		UCh[Count] = ToUnicode( Str[Count] );
	UCh[Count] = 0;
	return UCh;
	unguard;
#endif
}

// ANSICHAR to TCHAR.
CORE_API const TCHAR* appFromAnsi( const ANSICHAR* ACh )
{
#if UNICODE
	guard(appFromAnsi);
	if( !ACh )
		return NULL;
	TCHAR* Ch = appStaticString1024();
	INT Count;
	for( Count=0; Count<1024-1 && ACh[Count]; Count++ )
		Ch[Count] = FromAnsi( ACh[Count] );
	Ch[Count] = 0;
	return Ch;
	unguard;
#else
	return ACh;
#endif
}

CORE_API const TCHAR* appFromAnsi( TArray<TCHAR> & Ch, const ANSICHAR* ACh )
{
#if UNICODE
	guard(appFromAnsi);
	if( !ACh )
		return NULL;

	for (int len = 0; ACh[len]; ++len);

	Ch.Set(len);

	for (; len >=0; --len)
		Ch(len) = FromAnsi( ACh[len] );

	return Ch.GetData();
	unguard;
#else
	return ACh;
#endif
}

// UNICHAR to TCHAR.
CORE_API const TCHAR* appFromUnicode( const UNICHAR* UCh )
{
#if UNICODE
	return UCh;
#else
	guard(appFromUnicode);
	if( !UCh )
		return NULL;
	TCHAR* Ch = appStaticString1024();
	INT Count;
	for( Count=0; Count<1024-1 && UCh[Count]; Count++ )
		Ch[Count] = FromUnicode( UCh[Count] );
	Ch[Count] = 0;
	return Ch;
	unguard;
#endif
}

/*-----------------------------------------------------------------------------
	Error handling.
-----------------------------------------------------------------------------*/

//
// Unwind the stack.
//
CORE_API void VARARGS appUnwindf( const TCHAR* Fmt, ... )
{
	// Does not set GIsCriticalError: unguard calls this for every exception through a guarded scope, and most
	// are recoverable (appThrowf is how a failed load unwinds; the loaders catch it). Once set, the flag makes
	// UObject::~UObject leave its slot in the object table and silences FOutputDeviceFile. A fatal error sets
	// it in FOutputDeviceAnsiError::Serialize before it throws.

	TCHAR TempStr[4096];
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), Fmt );

	// Accumulate the History chain only for a critical error, not for every recovered load failure.
	if( GIsCriticalError )
	{
		static INT Count=0;
		if( Count++ )
			appStrncat( GErrorHist, TEXT(" <- "), ARRAY_COUNT(GErrorHist) );
		appStrncat( GErrorHist, TempStr, ARRAY_COUNT(GErrorHist) );
	}

	debugf( NAME_Critical, TempStr );
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
