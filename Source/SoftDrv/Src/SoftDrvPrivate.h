/*=============================================================================
	SoftDrvPrivate.h: Rendering package private header.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*------------------------------------------------------------------------------------
	Dependencies.
------------------------------------------------------------------------------------*/

#include "Render.h" // 433's private render header (Render/Inc)

#ifdef PLATFORM_WIN32
// The Win32 headers define IN and OUT as empty annotation macros, which would delete DrawSurf.cpp's local
// `IN`; nothing here uses the annotations.
#undef IN
#undef OUT
#endif

/*------------------------------------------------------------------------------------
	Software rendering private definitions.
------------------------------------------------------------------------------------*/

// This is the v400 SoftDrv, built with ASM=0: its portable #if !ASM paths replace the x86 __asm
// rasterization, and GIsMMX is forced off (__INTEL__ is set on every little-endian target) so the portable
// Pentium path is taken.
#if !ASM && __INTEL__
#define GIsMMX false
#endif

// Maximum supported sizes: the bounds of the static SizeY-indexed (Setup/MMXSetup/SpanIndex) and
// SizeX-indexed (UCoordTable/Photon) tables, which cost their full size at every resolution; set at build
// time with -DSOFTDRV_MAX_SCREEN=n. A larger viewport degrades rather than corrupting: SetRes clamps to the
// maximum, DrawTriangles draws nothing above MaximumYScreenSize and DrawTile clamps its span fill.
#ifndef SOFTDRV_MAX_SCREEN
#define SOFTDRV_MAX_SCREEN 4096
#endif
#define MaximumYScreenSize  SOFTDRV_MAX_SCREEN
#define MaximumXScreenSize  SOFTDRV_MAX_SCREEN


// Coarse fast square root approximation
#define FASTAPPROX_MAN_BITS 8   /* Number of bits of approximate square root mantissa, <=23 */
extern FLOAT FastSqrtTbl[2<<FASTAPPROX_MAN_BITS];

void SetupFastSqrt();

//
// _rotl / _rotr: from <stdlib.h> on Windows, <x86intrin.h> on x86, otherwise defined here.
//
#ifdef PLATFORM_WIN32
#include <stdlib.h> // for _rotl and _rotr
#elif defined(PLATFORM_X86)
#include <x86intrin.h> // for _rotl and _rotr
#else
inline unsigned int _rotl( unsigned int a, int s )
{
	s &= 31;
	if( s == 0 ) return a; // a shift by 32 is undefined
	return (a << s) | (a >> (32 - s));
}
inline unsigned int _rotr( unsigned int a, int s )
{
	s &= 31;
	if( s == 0 ) return a; // a shift by 32 is undefined
	return (a >> s) | (a << (32 - s));
}
#endif

//
// An 8-byte MMX structure.
//

union FMMX
{
	struct
	{
#if __INTEL_BYTE_ORDER__
		SWORD R, G, B, A;
#else
		SWORD A, B, G, R;
#endif
	};

	struct
	{
#if __INTEL_BYTE_ORDER__
		BYTE R1, G1, B1, A1, R2, G2, B2, A2;
#else
		BYTE A2, B2, G2, R2, A1, B1, G1, R1;
#endif
	};

	struct
	{
#if __INTEL_BYTE_ORDER__
		BYTE SB1, SG1, SR1, SA1, SR2, SG2, SB2, SA2;
#else
		BYTE SA2, SR2, SG2, SB2, SA1, SB1, SG1, SR1;
#endif
	};

	struct
	{
#if __INTEL_BYTE_ORDER__
		INT IL, IH;
#else
		INT IH, IL;
#endif
	};
	struct
	{
#if __INTEL_BYTE_ORDER__
		DWORD DL, DH;
#else
		DWORD DH, DL;
#endif
	};
	struct
	{
#if __INTEL_BYTE_ORDER__
		FLOAT FL, FH;
#else
		DWORD FH, FL;
#endif
	};
	DOUBLE D;
	QWORD  Q;
	SQWORD SQ;
};


// ColorMode device constants.
enum {
		MMX15,
		MMX16,
		MMXReserved,
		MMX32,
		Pentium15,
		Pentium16,
		PentiumReserved,
		Pentium32
};

// RenderMode constants.
enum {
		DRAWNORMAL,
		DRAWTRANSLUCENT,
		DRAWMODULATED,
		DRAWSTIPPLED,
		DRAWMASKED,
		DRAWRESERVED1,
		DRAWRESERVED2
};


//
// A 3D rendering device.
//
class DLL_EXPORT USoftwareRenderDevice : public URenderDevice
{
	DECLARE_CLASS(USoftwareRenderDevice,URenderDevice,CLASS_Config,SoftDrv)

	// Temp statistics.
	INT SurfPalBuilds;

	// Preference variables.
	BITFIELD		FastTranslucency;
	BITFIELD		HighResTextureSmooth;
	BITFIELD		LowResTextureSmooth;
	// Which filtering to use when HighRes/LowResTextureSmooth enable it for this resolution:
	// False = the 2x2 knight's-move coordinate dither (one texel), True = true bilinear (four texels
	// and a blend). Smoothing off is point sampling either way.
	BITFIELD		BilinearFiltering;
	// Mesh fog switch. SoftDrv-local: 433's URenderDevice has none, and adding one there would change the
	// layout every driver compiles against.
	BITFIELD		VolumetricLightingMeshes;
	BITFIELD		RAM32Mode;
	BITFIELD		TextureSmooth;
	FLOAT			DetailBias;

	// Variables.
	DWORD			RenderMode;
	DWORD			ColorMode;
	INT				FrameLocksCounter;
	INT				GByteStride;
	FVector			FlashScale, FlashFog;
	FLOAT			PerspError;
	FLOAT			GMasterScale;
	FVector			GFloatScale, GFloatFog, GMaxColor, GFloatRange;
	TArray<BYTE>	HitStack;
	BYTE*			HitData;
	INT*			HitSize;
	INT				HitCount;

	// Constructors.	
	void StaticConstructor();

	// URenderDevice interface.
	UBOOL Init( UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen );
	UBOOL SetRes( INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen );
	void Exit();
	void Flush(	UBOOL );
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar );
	void Lock( FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* InHitData, INT* InHitSize );
	void Unlock( UBOOL Blit );
	// The 433 render face (Engine/Inc/UnRenDev.h). No DrawGouraudPolygon: URenderDevice's forwards it to
	// DrawTriangles as a fan.
	void DrawComplexSurface( FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet, DWORD PolyFlags, BYTE cAlpha );
	INT MaxVertices();
	void DrawTriangles( FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, _WORD* Indices, INT NumIndices, DWORD PolyFlags, FSpanBuffer* Span );
	void DrawTile( FSceneNode* Frame, FTextureInfo& Texture, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, FSpanBuffer* Span, FLOAT Z, FPlane Light, FPlane Fog, DWORD PolyFlags );
	void GetStats( TCHAR* Result );
	void Draw2DLine( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2 );
	void Draw2DPoint( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z );
	void PushHit( const BYTE* Data, INT Count );
	void PopHit( INT Count, UBOOL bForce );
	void ReadPixels( FColor* Pixels );
	void ClearZ( FSceneNode* Frame ){};

private:
	// USoftwareRenderDevice interface.
	void InitDrawSurf();

	void MMXFlashTriangle( FSceneNode* Frame, FTextureInfo& Texture, DWORD PolyFlags, FSpanBuffer* SpanBuffer );
	void PentiumFlashTriangle( FSceneNode* Frame, FTextureInfo& Texture, DWORD PolyFlags, FSpanBuffer* SpanBuffer );

	void MMXDrawRectangle( FSceneNode* Node, FTextureInfo& Texture,FTransTexture* Pts, DWORD PolyFlags, FSpanBuffer* SpanBuffer );
	void PentiumDrawRectangle( FSceneNode* Node, FTextureInfo& Texture,FTransTexture* Pts, DWORD PolyFlags, FSpanBuffer* SpanBuffer );
	void InitMMXFlashes( FLOAT Brightness, INT ColorBytes, DWORD Caps);
	void InitColorTables( FLOAT Brightness, INT ColorBytes, DWORD Caps);
	void InitDepthFogTable();

	void InnerGouraudMMX32(DWORD PolyFlags, INT MinY, INT MaxY, FSceneNode* Frame,FMipmapBase* Mip,FSpanBuffer* SpanBuffer);
    void InnerGouraudMMX16(DWORD PolyFlags, INT MinY, INT MaxY, FSceneNode* Frame,FMipmapBase* Mip,FSpanBuffer* SpanBuffer);
	void InnerGouraudMMX15(DWORD PolyFlags, INT MinY, INT MaxY, FSceneNode* Frame,FMipmapBase* Mip,FSpanBuffer* SpanBuffer);

	void BlitTile32(FSpanBuffer* Span);
	void BlitMask32(FSpanBuffer* Span);
	void BlitAlpha32(FSpanBuffer* Span);

	void BlitTile1516(FSpanBuffer* Span);
	void BlitMask1516(FSpanBuffer* Span);
	void BlitAlpha15(FSpanBuffer* Span);
	void BlitAlpha16(FSpanBuffer* Span);

	void ClearScreenFast16(_WORD* Dest, DWORD Color);
	void ClearScreenFast32(DWORD* Dest, DWORD Color);

	void RenderSurfSpansMMX32( FRainbowPtr& DestPtr, FSurfaceInfo& Surface,INT TaskStartY, INT TaskEndY);
	void RenderSurfSpansMMX15( FRainbowPtr& DestPtr, FSurfaceInfo& Surface,INT TaskStartY, INT TaskEndY);
	void RenderSurfSpansMMX16( FRainbowPtr& DestPtr, FSurfaceInfo& Surface,INT TaskStartY, INT TaskEndY);
};




#if ASM

//
// Fast floating point power routines.
// Made coarse but fast for per-span subdivision requirement determination....
// About 12 cycles on the Pentium.
//

inline FLOAT FastSqrt(FLOAT F)
{
	__asm
	{
		mov  eax,[F]                        // get float as int.
		shr  eax,(23 - FASTAPPROX_MAN_BITS) - 2 // shift away unused low mantissa.
		mov  ebx,[F]						// get float as int.
		and  eax, ((1 << (FASTAPPROX_MAN_BITS+1) )-1) << 2 // 2 to avoid "[eax*4]".
		and  ebx, 0x7F000000				// 7 bit exp., wipe low bit+sign.
		shr  ebx, 1							// exponent/2.
		mov  eax,DWORD PTR FastSqrtTbl [eax]	// index hi bit is exp. low bit.
		add  eax,ebx						// recombine with exponent.
		mov  [F],eax						// store.
	}
	return F;								// compiles to fld [F].
}

#else

inline FLOAT FastSqrt(FLOAT F) {return appSqrt(F);}

#endif


/*------------------------------------------------------------------------------------
	The End.
------------------------------------------------------------------------------------*/
