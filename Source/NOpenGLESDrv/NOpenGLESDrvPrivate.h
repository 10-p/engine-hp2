/*------------------------------------------------------------------------------------
	Dependencies.
------------------------------------------------------------------------------------*/

#ifdef __EMSCRIPTEN__
// On the web Emscripten provides the GLES2 API as link-time symbols (-sFULL_ES2), so no loader is compiled;
// that also keeps this driver from clashing with XOpenGLDrv's glad in the static binary.
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>   // GL_BGRA_EXT and the other extension tokens
#else
#include "glad.h"
#endif
#include "glm/matrix.hpp"
// 433 has no Render/Src/RenderPrivate.h; FSceneNode/FSurfaceInfo/FTransTexture are declared in Engine's UnRender.h.
#include "Engine.h"
#include "UnRender.h"

/*------------------------------------------------------------------------------------
	OpenGL rendering private definitions.
------------------------------------------------------------------------------------*/

enum EUniformIndex
{
	UF_Mtx,
	UF_Brightness,
	UF_Texture0,
	UF_Texture1,
	UF_Texture2,
	UF_Texture3,
	UF_Count
};

enum EShaderFlags : DWORD
{
	SF_Texture0  = 1 << 0,
	SF_Texture1  = 1 << 1,
	SF_Texture2  = 1 << 2,
	SF_Texture3  = 1 << 3,
	SF_VtxColor  = 1 << 4,
	SF_AlphaTest = 1 << 5,
	SF_Lightmap  = 1 << 6,
	SF_Fogmap    = 1 << 7,
	SF_Detail    = 1 << 8,
	SF_VtxFog    = 1 << 9,
	SF_Max       = SF_VtxFog,
	SF_Count     = 10,
};

enum EShaderAttribs
{
	AT_Position,
	AT_TexCoord0,
	AT_TexCoord1,
	AT_TexCoord2,
	AT_TexCoord3,
	AT_VtxColor,
	AT_VtxFog,
	AT_Count
};

// HP2's straight-alpha blend (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA): fading actors (PF_Highlighted|PF_Translucent,
// alpha in FTransTexture::Light.W) and Lumos surfaces (opacity from cAlpha). Carried on the one EPolyFlags bit 433
// leaves free; content never sets it.
#define PF_StraightAlpha 0x20000000

//
// GLES2 renderer based on NOpenGLDrv.
//
class DLL_EXPORT UNOpenGLESRenderDevice : public URenderDevice
{
	DECLARE_CLASS(UNOpenGLESRenderDevice, URenderDevice, CLASS_Config, NOpenGLESDrv)

	static constexpr INT MaxTexUnits = 4;

	// Options. DetailTextures is URenderDevice's own config property on 433.
	UBOOL NoFiltering;
	// Image quality.
	//   MaxAnisotropy: EXT_texture_filter_anisotropic level, clamped at Init to the driver's maximum.
	//   NumAASamples: antialiasing samples (0 = off). On the web Init asks ufront_backbuffer_samples for a
	//     multisampled back buffer; natively the viewport made the context first, so it is only logged.
	FLOAT MaxAnisotropy;
	INT NumAASamples;
	INT SwapInterval;
	UBOOL UseBGRA;
	UBOOL Overbright;
	UBOOL UseVAO;

	// All currently cached textures. NoSmooth is the filter the texture object has now: the draw's PF_NoSmooth
	// picks it, so a texture drawn both ways switches back and forth.
	struct FCachedTexture
	{
		GLuint Id;
		INT BaseMip;
		INT MaxLevel;
		UBOOL NoSmooth;
	};
	TMap<QWORD, FCachedTexture> BindMap;
	TArray<GLuint> TexAlloc;

	// Currently bound textures.
	struct FTexInfo
	{
		QWORD CurrentCacheID;
		FLOAT UMult;
		FLOAT VMult;
		FLOAT UPan;
		FLOAT VPan;
	} TexInfo[MaxTexUnits];

	// All currently compiled shaders.
	struct FCachedShader
	{
		DWORD Flags;
		GLuint Prog;
		GLint Uniforms[UF_Count];
		UBOOL Attribs[AT_Count];
		DWORD NumFloats;
	};
	TMap<DWORD, FCachedShader> ShaderMap;

	BYTE UniformsChanged[UF_Count];

	// Currently bound shader.
	FCachedShader* ShaderInfo;

	// Texture upload buffer.
	BYTE* Compose;
	DWORD ComposeSize;

	// Vertex buffer.
	GLuint GLBuf;
	GLfloat* VtxData;
	GLfloat* VtxDataEnd;
	GLfloat* VtxDataPtr;
	DWORD VtxDataSize;
	DWORD VtxPolyVerts;
	UBOOL InPoly;

	// Index buffer.
	GLushort* IdxData;
	GLushort* IdxDataEnd;
	GLushort* IdxDataPtr;
	DWORD IdxDataSize;
	GLushort IdxCount;
	GLushort IdxBase;

	// Current state.
	DWORD CurrentShaderFlags;
	DWORD CurrentPolyFlags;
	FLOAT CurrentBrightness;
	FLOAT RProjZ, Aspect;
	FLOAT RFX2, RFY2;
	glm::mat4 MtxProj;
	glm::mat4 MtxMVP;
	FPlane ColorMod;

	struct FCachedSceneNode
	{
		FLOAT FovAngle;
		FLOAT FX, FY;
		INT X, Y;
		INT XB, YB;
		INT SizeX, SizeY;
	} CurrentSceneNode;

	// Constructors.
	UNOpenGLESRenderDevice();
	// Registers the config options and their compiled defaults (433 calls StaticConstructor once per class).
	void StaticConstructor();

	// URenderDevice interface: the 433 face. No DrawGouraudPolygon: URenderDevice's forwards it to DrawTriangles
	// as a fan.
	virtual UBOOL Init( UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen ) override;
	virtual UBOOL SetRes( INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen ) override;
	virtual void Exit() override;
	virtual void Flush( UBOOL AllowPrecache ) override;
	virtual void PostEditChange() override;
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar ) override;
	virtual void Lock( FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* InHitData, INT* InHitSize ) override;
	virtual void Unlock( UBOOL Blit ) override;
	virtual void DrawComplexSurface( FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet, DWORD PolyFlags, BYTE cAlpha ) override;
	virtual INT MaxVertices() override;
	virtual void DrawTriangles( FSceneNode* Frame, FTextureInfo& Texture, FTransTexture** Pts, INT NumPts, _WORD* Indices, INT NumIndices, DWORD PolyFlags, FSpanBuffer* Span ) override;
	virtual void DrawTile( FSceneNode* Frame, FTextureInfo& Texture, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, FSpanBuffer* Span, FLOAT Z, FPlane Light, FPlane Fog, DWORD PolyFlags ) override;
	virtual void EndFlash() override;
	virtual void GetStats( TCHAR* Result ) override;
	virtual void Draw2DLine( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2 ) override;
	virtual void Draw2DPoint( FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z ) override;
	virtual void PushHit( const BYTE* Data, INT Count ) override;
	virtual void PopHit( INT Count, UBOOL bForce ) override;
	virtual void ReadPixels( FColor* Pixels ) override;
	virtual void ClearZ( FSceneNode* Frame ) override;
	virtual void SetSceneNode( FSceneNode* Frame ) override;

	// UNOpenGLESRenderDevice interface.
	void UpdateUniforms();
	GLuint CompileShader( GLenum Type, const char* Text );
	FCachedShader* CreateShader( DWORD ShaderFlags );
	void SetShader( DWORD ShaderFlags );
	void SetBlend( DWORD PolyFlags, UBOOL InverseOrder = false );
	void SetTexture( INT TMU, FTextureInfo& Info, DWORD PolyFlags, FLOAT PanBias );
	void ResetTexture( INT TMU );
	void UploadTexture( FTextureInfo& Info, UBOOL Masked, UBOOL NewTexture );
	void SetTextureFilter( const FTextureInfo& Info, UBOOL NoSmooth );
	void UpdateSwapInterval();
	// HP2's blend modes on the draw's poly flags, as in its D3D7 driver and SoftDrv.
	DWORD HP2AdjustPolyFlags( DWORD PolyFlags, UBOOL IsTile, const FTextureInfo& Info );

private:
	// Fixed function mode emulation.
	inline void FlushTriangles()
	{
		if( IdxCount )
		{
			check( IdxDataPtr <= IdxDataEnd );
			check( VtxDataPtr <= VtxDataEnd );
			// Each batch fills the buffer whole: a glBufferSubData into the buffer the previous batch's draw still
			// reads makes the driver copy it or wait.
			if ( UseVAO )
				glBufferData( GL_ARRAY_BUFFER, ( (BYTE*)VtxDataPtr - (BYTE*)VtxData ), VtxData, GL_STREAM_DRAW );
			glDrawElements( GL_TRIANGLES, IdxDataPtr - IdxData, GL_UNSIGNED_SHORT, IdxData );
			IdxCount = 0;
		}
		VtxDataPtr = VtxData;
		IdxDataPtr = IdxData;
	}

	// Flush before a polygon that may not fit: the batch buffers are fixed (MAX_VERTS entries) and every
	// Attrib*/EndPoly write is unchecked. A close-up mesh, subdivided in proportion to the screen width, can
	// exceed the index buffer in one batch. The bound is generous: UE1 polys carry at most 16 vertices and
	// AttribSizes sum to 19 floats. Vertex indices stay below 65536 (GL_UNSIGNED_SHORT).
	enum { MAX_POLY_VERTS = 64, MAX_POLY_FLOATS = MAX_POLY_VERTS * 20, MAX_POLY_INDICES = ( MAX_POLY_VERTS - 2 ) * 3 };
	inline void BeginPoly()
	{
		if( IdxDataPtr + MAX_POLY_INDICES > IdxDataEnd || VtxDataPtr + MAX_POLY_FLOATS > VtxDataEnd || (INT)IdxCount + MAX_POLY_VERTS > 65535 )
			FlushTriangles();
		VtxPolyVerts = 0;
		IdxBase = IdxCount;
	}

	// Flush before a DrawTriangles batch that may not fit: NumVerts vertices of at most 20 floats and NumIndices
	// indices, with every vertex index below 65536.
	inline void BeginBatch( INT NumVerts, INT NumIndices )
	{
		if( IdxDataPtr + NumIndices > IdxDataEnd || VtxDataPtr + NumVerts * 20 > VtxDataEnd || (INT)IdxCount + NumVerts > 65535 )
			FlushTriangles();
		VtxPolyVerts = 0;
		IdxBase = IdxCount;
	}

	inline void AttribFloat2( FLOAT X, FLOAT Y )
	{
		*VtxDataPtr++ = X;
		*VtxDataPtr++ = Y;
	}

	inline void AttribFloat2( const FLOAT *V )
	{
		*VtxDataPtr++ = V[0];
		*VtxDataPtr++ = V[1];
	}

	inline void AttribFloat3( FLOAT X, FLOAT Y, FLOAT Z )
	{
		*VtxDataPtr++ = X;
		*VtxDataPtr++ = Y;
		*VtxDataPtr++ = Z;
	}

	inline void AttribFloat3( const FLOAT* V )
	{
		*VtxDataPtr++ = V[0];
		*VtxDataPtr++ = V[1];
		*VtxDataPtr++ = V[2];
	}

	inline void AttribFloat4( FLOAT X, FLOAT Y, FLOAT Z, FLOAT W )
	{
		*VtxDataPtr++ = X;
		*VtxDataPtr++ = Y;
		*VtxDataPtr++ = Z;
		*VtxDataPtr++ = W;
	}

	inline void AttribFloat4( const FLOAT* V )
	{
		*VtxDataPtr++ = V[0];
		*VtxDataPtr++ = V[1];
		*VtxDataPtr++ = V[2];
		*VtxDataPtr++ = V[3];
	}

	inline void PolyVertex()
	{
		++VtxPolyVerts;
	}

	inline void EndPoly()
	{
		*IdxDataPtr++ = IdxCount++;
		*IdxDataPtr++ = IdxCount++;
		*IdxDataPtr++ = IdxCount++;
		for( DWORD i = 3; i < VtxPolyVerts; ++i )
		{
			*IdxDataPtr++ = IdxBase;
			*IdxDataPtr++ = IdxCount - 1;
			*IdxDataPtr++ = IdxCount++;
		}
	}

	// The batch's vertices are written; add the indices of a triangle list (Indices into those vertices, Count a
	// multiple of 3).
	inline void EndIndexedList( const _WORD* Indices, INT Count )
	{
		for( INT i = 0; i < Count; ++i )
			*IdxDataPtr++ = (GLushort)( IdxBase + Indices[i] );
		IdxCount = (GLushort)( IdxBase + VtxPolyVerts );
	}

	// The batch's vertices are written: one triangle per three of them.
	inline void EndPlainList()
	{
		for( DWORD i = 0; i + 2 < VtxPolyVerts; i += 3 )
		{
			*IdxDataPtr++ = (GLushort)( IdxBase + i );
			*IdxDataPtr++ = (GLushort)( IdxBase + i + 1 );
			*IdxDataPtr++ = (GLushort)( IdxBase + i + 2 );
		}
		IdxCount = (GLushort)( IdxBase + VtxPolyVerts );
	}
};
