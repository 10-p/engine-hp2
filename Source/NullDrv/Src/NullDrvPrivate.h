/*=============================================================================
	NullDrvPrivate.h: Null render device (GPU-free) private header.
=============================================================================*/

// 433 has no Render/Src/RenderPrivate.h; FSceneNode/FSurfaceInfo/FTransTexture/FSpanBuffer are declared
// in Engine's UnRender.h.
#include "Engine.h"
#include "UnRender.h"

/*------------------------------------------------------------------------------------
	UNullRenderDevice.
------------------------------------------------------------------------------------*/

//
// A null render device: the 433 URenderDevice interface with no-op draw calls and no GL context, selected
// by -NULLRENDERER. The NSDL viewport picks GL or software from the render device name alone
// (UNSDLViewport::OpenWindow, before Init runs): "NullDrv.NullRenderDevice" lacks "OPENGL", so it gets an
// SDL_Texture software surface and no WebGL context.
//
class DLL_EXPORT UNullRenderDevice : public URenderDevice
{
	DECLARE_CLASS(UNullRenderDevice,URenderDevice,CLASS_Config,NullDrv)

	// Constructors.
	void StaticConstructor();

	// URenderDevice interface. Only Init/SetRes do anything (claim a software surface); every draw is a no-op.
	UBOOL Init( UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen );
	UBOOL SetRes( INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen );
	void Exit();
	void Flush( UBOOL AllowPrecache );
	void Lock( FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData, INT* HitSize );
	void Unlock( UBOOL Blit );
	void DrawComplexSurface( FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet, DWORD PolyFlags, BYTE cAlpha );
	// Render sizes its mesh/particle vertex buffers from MaxVertices() and fills them before calling the
	// device, so it must be > 0 -- 256, like every retail HP2 device. DrawGouraudPolygon is a non-pure base
	// forwarder to DrawTriangles; devices do not declare it.
	INT MaxVertices();
	void DrawTriangles( FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, _WORD* Indices, INT NumIndices, DWORD PolyFlags, FSpanBuffer* Span );
	void DrawTile( FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, FSpanBuffer* Span, FLOAT Z, FPlane Light, FPlane Fog, DWORD PolyFlags );
	void Draw2DLine( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2 );
	void Draw2DPoint( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z );
	void ClearZ( FSceneNode* Frame );
	void PushHit( const BYTE* Data, INT Count );
	void PopHit( INT Count, UBOOL bForce );
	void GetStats( TCHAR* Result );
	void ReadPixels( FColor* Pixels );
};

/*------------------------------------------------------------------------------------
	The End.
------------------------------------------------------------------------------------*/
