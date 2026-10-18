/*=============================================================================
	UnViewport.cpp: Generic Unreal viewport code
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnRender.h"
#include "UnNet.h"

/*-----------------------------------------------------------------------------
	URenderDevice.
-----------------------------------------------------------------------------*/

#define LINE_NEAR_CLIP_Z 1.f

//
// Init URenderDevice class.
//
void URenderDevice::StaticConstructor()
{
	guard(URenderDevice::StaticConstructor);

	new(GetClass(),TEXT("VolumetricLighting"),RF_Public)UBoolProperty(CPP_PROPERTY(VolumetricLighting), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("ShinySurfaces"),     RF_Public)UBoolProperty(CPP_PROPERTY(ShinySurfaces     ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("Coronas"),           RF_Public)UBoolProperty(CPP_PROPERTY(Coronas           ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("HighDetailActors"),  RF_Public)UBoolProperty(CPP_PROPERTY(HighDetailActors  ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("Description"),       RF_Public)UStrProperty(CPP_PROPERTY(Description        ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("DescFlags"),         RF_Public)UIntProperty(CPP_PROPERTY(DescFlags          ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("DetailTextures"),    RF_Public)UBoolProperty(CPP_PROPERTY(DetailTextures     ), TEXT("Client"), CPF_Config );

	DecompFormat = TEXF_P8;

	unguard;
}

//
// Draw a clipped 2D line.
//
void URenderDevice::Draw2DClippedLine
(
	FSceneNode*		Frame,
	FPlane			Color,
	DWORD			LineFlags,
	FVector			P1,
	FVector			P2
)
{
	guard(URenderDevice::Draw2DClippedLine);

	// X clip it.
	if( P1.X > P2.X )
		Exchange( P1, P2 );
	if( P2.X<0 || P1.X>Frame->FX )
		return;
	if( P1.X<0 )
	{
		if( Abs(P2.X-P1.X)<0.001f )
			return;
		P1.Y += (0-P1.X)*(P2.Y-P1.Y)/(P2.X-P1.X);
		P1.X  = 0;
	}
	if( P2.X>=Frame->FX )
	{
		if( Abs(P2.X-P1.X)<0.001f )
			return;
		P2.Y += ((Frame->FX-1.f)-P2.X)*(P2.Y-P1.Y)/(P2.X-P1.X);
		P2.X  = Frame->FX-1.f;
	}

	// Y clip it.
	if( P1.Y > P2.Y )
		Exchange( P1, P2 );
	if( P2.Y < 0 || P1.Y > Frame->FY )
		return;
	if( P1.Y < 0 )
	{
		if( Abs(P2.Y-P1.Y)<0.001f )
			return;
		P1.X += (0-P1.Y)*(P2.X-P1.X)/(P2.Y-P1.Y);
		P1.Y  = 0;
	}
	if( P2.Y >= Frame->FY )
	{
		if( Abs(P2.Y-P1.Y)<0.001f )
			return;
		P2.X += ((Frame->FY-1.f)-P2.Y)*(P2.X-P1.X)/(P2.Y-P1.Y);
		P2.Y  = Frame->FY-1.f;
	}

	// Assure no coordinates are out of bounds. 
	ClipFloatFromZero(P1.X,Frame->FX);
	ClipFloatFromZero(P2.X,Frame->FX);
	ClipFloatFromZero(P1.Y,Frame->FY);
	ClipFloatFromZero(P2.Y,Frame->FY);

	// Draw it.
	Draw2DLine( Frame, Color, LineFlags, P1, P2 );

	unguard;
}

UBOOL URenderDevice::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(UVRenderDevice::Exec);

	if( ParseCommand(&Cmd,TEXT("GetDetailTextures")) )
	{
		Ar.Logf( TEXT("%i"), DetailTextures );
		return 1;
	} 
	else if( ParseCommand(&Cmd,TEXT("ToggleDetailTextures")) )
	{
		DetailTextures ^= 1;
		SaveConfig();
		return 1;
	}
	else
		return 0;

	unguard;
}

//
// Draw a clipped 3D line.
//
void URenderDevice::Draw3DLine
(
	FSceneNode*		Frame,
	FPlane			Color,
	DWORD			LineFlags,
	FVector			P1,
	FVector			P2
)
{
	guard(URenderDevice::Draw3DLine);
	FLOAT SX2 = Frame->FX2;
	FLOAT SY2 = Frame->FY2;

	// Transform.
	if( !(LineFlags & LINE_PreTransformed) )
	{
		P1 = P1.TransformPointBy( Frame->Coords );
		P2 = P2.TransformPointBy( Frame->Coords );
	}
	if( Frame->Viewport->IsOrtho() )
	{
		// Zoom.
		P1 = P1 / Frame->Zoom + FVector( SX2, SY2, 0 );
		P2 = P2 / Frame->Zoom + FVector( SX2, SY2, 0 );

		// See if points form a line parallel to our line of sight (i.e. line appears as a dot).
		if( Abs(P2.X-P1.X)+Abs(P1.Y-P2.Y)<0.2 )
		{
			// Line is visible as a point.
			if( Frame->Viewport->Actor->OrthoZoom < ORTHO_LOW_DETAIL )
				Draw2DPoint( Frame, Color, LINE_None, P1.X-1, P1.Y-1, P1.X+1, P1.Y+1, 1.f );
			return;
		}
	}
	else
	{
		// Calculate delta, discard line if points are identical.
		FVector D = P2-P1;
		if( D.SizeSquared() < Square(0.01f) )
			return;

		// Clip to near clipping plane.
		if( P1.Z <= LINE_NEAR_CLIP_Z )
		{
			// Clip P1 to NCP.
			if( P2.Z<(LINE_NEAR_CLIP_Z-0.01f) )
				return;
			P1.X +=  (LINE_NEAR_CLIP_Z-P1.Z) * D.X/D.Z;
			P1.Y +=  (LINE_NEAR_CLIP_Z-P1.Z) * D.Y/D.Z;
			P1.Z  =  (LINE_NEAR_CLIP_Z);
		}
		else if( P2.Z<(LINE_NEAR_CLIP_Z-0.01f) )
		{
			// Clip P2 to NCP.
			P2.X += (LINE_NEAR_CLIP_Z-P2.Z) * D.X/D.Z;
			P2.Y += (LINE_NEAR_CLIP_Z-P2.Z) * D.Y/D.Z;
			P2.Z =  (LINE_NEAR_CLIP_Z);
		}

		// Calculate perspective.
		P1.Z = 1.f/P1.Z; P1.X = P1.X * Frame->Proj.Z * P1.Z + SX2; P1.Y = P1.Y * Frame->Proj.Z * P1.Z + SY2;
		P2.Z = 1.f/P2.Z; P2.X = P2.X * Frame->Proj.Z * P2.Z + SX2; P2.Y = P2.Y * Frame->Proj.Z * P2.Z + SY2;
	}

	// Clip it and draw it.
	Draw2DClippedLine( Frame, Color, LineFlags, P1, P2 );

	unguard;
}

void URenderDevice::DrawGouraudPolygon( FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, int NumPts, DWORD PolyFlags, FSpanBuffer* SpanBuffer )
{
	// Set up an indexed triangle list for a single polygon.
	DrawTriangles( Frame, Info, Pts, NumPts, NULL, NumPts, PolyFlags, SpanBuffer );
}

/*-----------------------------------------------------------------------------
	UViewport object implementation.
-----------------------------------------------------------------------------*/

void UViewport::Serialize( FArchive& Ar )
{
	guard(UViewport::Serialize);
	Super::Serialize( Ar );
	Ar << Console << MiscRes << Canvas << RenDev << Input;
	unguard;
}
void* UViewport::GetServer()
{
	guard(UViewport::GetServer);
	return NULL;
	unguard;
}
IMPLEMENT_CLASS(UViewport);

/*-----------------------------------------------------------------------------
	Dragging.
-----------------------------------------------------------------------------*/

//
// Set mouse dragging.
// The mouse is dragging when and only when one or more button is held down.
//
UBOOL UViewport::SetDrag( UBOOL NewDrag )
{
	guard(UViewport::SetDrag);
	UBOOL Result = Dragging;
	Dragging = NewDrag;
	if( GIsRunning )
	{
		if( Dragging && !Result )
		{
			// First hit.
			GetOuterUClient()->Engine->MouseDelta( this, MOUSE_FirstHit, 0, 0 );
		}
		else if( Result && !Dragging )
		{
			// Last release.
			GetOuterUClient()->Engine->MouseDelta( this, MOUSE_LastRelease, 0, 0 );
		}
	}
	return Result;
	unguard;
}

/*-----------------------------------------------------------------------------
	AutoFOV - aspect-correct field of view.
-----------------------------------------------------------------------------*/

static UBOOL  GAutoFovParsed   = 0;
static INT    GAutoFovMode     = AUTOFOV_Adaptive;
static FLOAT  GAutoFovRef      = 4.0f / 3.0f;
static INT    GAutoFovLastX    = -1;
static INT    GAutoFovLastY    = -1;
static FLOAT  GAutoFovLastBase = -1.0f;

static void ufrontParseAutoFov()
{
	guard(ufrontParseAutoFov);
	GAutoFovParsed = 1;

	// A plain config section, not class properties: no class of the shipped Engine.u may gain properties.
	INT   Mode = GAutoFovMode;
	FLOAT Ref  = GAutoFovRef;
	GConfig->GetInt  ( TEXT("ufront.AutoFOV"), TEXT("AutoFOV"),         Mode );
	GConfig->GetFloat( TEXT("ufront.AutoFOV"), TEXT("ReferenceAspect"), Ref  );
	GAutoFovMode = Clamp( Mode, (INT)AUTOFOV_Off, (INT)AUTOFOV_Adaptive );
	GAutoFovRef  = (Ref > 0.01f) ? Ref : (4.0f / 3.0f);

	debugf( NAME_Log, TEXT("[AutoFOV] mode=%i referenceAspect=%f"), GAutoFovMode, GAutoFovRef );
	unguard;
}

ENGINE_API FLOAT ufrontEffectiveFov( FLOAT BaseFov, INT SizeX, INT SizeY, UBOOL bLog )
{
	guard(ufrontEffectiveFov);
	if( !GAutoFovParsed )
		ufrontParseAutoFov();

	if( GAutoFovMode == AUTOFOV_Off || SizeX <= 0 || SizeY <= 0 )
		return BaseFov;

	// Guard the trig: a degenerate or hostile FOV must not produce a broken projection.
	FLOAT Base    = Clamp( BaseFov, 1.0f, 170.0f );
	FLOAT Aspect  = (FLOAT)SizeX / (FLOAT)SizeY;
	FLOAT HorPlus = (FLOAT)( appAtan( appTan( Base * PI / 360.0 ) * ( Aspect / GAutoFovRef ) ) * 360.0 / PI );
	FLOAT Eff     = ( GAutoFovMode == AUTOFOV_Adaptive ) ? Max( Base, HorPlus ) : HorPlus;
	Eff = Clamp( Eff, 1.0f, 170.0f );

	// Log when the inputs change: the viewport size (the E2E checks this line after a resize) or the base
	// FOV by more than half a degree (cameras and cut scenes interpolate it, so a change logs a few lines).
	if( bLog && ( SizeX != GAutoFovLastX || SizeY != GAutoFovLastY || Abs(Base - GAutoFovLastBase) > 0.5f ) )
	{
		GAutoFovLastX    = SizeX;
		GAutoFovLastY    = SizeY;
		GAutoFovLastBase = Base;
		debugf( NAME_Log, TEXT("[AutoFOV] viewport=%ix%i aspect=%f base=%f effective=%f"),
			SizeX, SizeY, Aspect, Base, Eff );
	}
	return Eff;
	unguard;
}

/*-----------------------------------------------------------------------------
	Scene frames.
-----------------------------------------------------------------------------*/

//
// Precompute rendering info.
//
void FSceneNode::ComputeRenderCoords( FVector& Location, FRotator& Rotation )
{
	guard(UViewport::ComputeRenderCoords);
	check(Viewport->Actor);
	Coords =
	(	Viewport->Actor->RendMap==REN_OrthXY ? FCoords(FVector(0,0,0),FVector(+1,0,0),FVector(0,+1,0),FVector(0,0,+1))
	:	Viewport->Actor->RendMap==REN_OrthXZ ? FCoords(FVector(0,0,0),FVector(+1,0,0),FVector(0,0,-1),FVector(0,+1,0))
	:	Viewport->Actor->RendMap==REN_OrthYZ ? FCoords(FVector(0,0,0),FVector(0,+1,0),FVector(0,0,-1),FVector(+1,0,0))
	:								           GMath.ViewCoords / Rotation) / Location;
	Uncoords = Coords.Transpose();
	ComputeRenderSize();
	unguard;
}

//
// Precompute sizing info.
//
void FSceneNode::ComputeRenderSize()
{
	guard(FSceneNode::ComputeRenderSize);

	// Precomputes.
	FX 			= (FLOAT)X;
	FY 			= (FLOAT)Y;
	FX2			= FX * 0.5f;
	FY2			= FY * 0.5f;	
	FX15		= (FX+1.0001f) * 0.5f;
	FY15		= (FY+1.0001f) * 0.5f;
	Proj		= FVector( 0.5f-0.5f*FX, 0.5f-0.5f*FY, 0.5f*FX / appTan(Viewport->Actor->FovAngle * PI/360.f) );
	RProj		= FVector( 1/Proj.X, 1/Proj.Y, 1/Proj.Z );
	Zoom 		= Viewport->Actor->OrthoZoom / (FX * 15.f);
	PrjXM		= (0  - FX2)*(-RProj.Z);
	PrjXP		= (FX - FX2)*(+RProj.Z);
	PrjYM		= (0  - FY2)*(-RProj.Z);
	PrjYP		= (FY - FY2)*(+RProj.Z);

	// Precompute side info.
	FLOAT TempSigns[2]={-1.f,+1.f};
	for( INT i=0; i<2; i++ )
	{
		for( INT j=0; j<2; j++ )
		{
			ViewSides[i*2+j] = FVector(TempSigns[i] * FX2, TempSigns[j] * FY2, Proj.Z).UnsafeNormal().TransformVectorBy(Uncoords);
		}
		ViewPlanes[i] = FPlane
		(
			Coords.Origin,
			FVector(0,TempSigns[i] / FY2,1.f/Proj.Z).UnsafeNormal().TransformVectorBy(Uncoords)
		);
		ViewPlanes[i+2] = FPlane
		(
			Coords.Origin,
			FVector(TempSigns[i] / FX2,0,1.f/Proj.Z).UnsafeNormal().TransformVectorBy(Uncoords)
		);
	}

	// Tell rendering device.
	Viewport->RenDev->SetSceneNode( this );

	unguard;
}

/*-----------------------------------------------------------------------------
	Custom viewport creation and destruction.
-----------------------------------------------------------------------------*/

//
// UViewport constructor.  Creates the viewport object but doesn't open its window.
//
UViewport::UViewport()
{
	guard(UViewport::UViewport);

	// Update viewport array.
	GetOuterUClient()->Viewports.AddItem( this );

	// Create canvas.
	UClass* CanvasClass = StaticLoadClass( UCanvas::StaticClass(), NULL, TEXT("ini:Engine.Engine.Canvas"), NULL, LOAD_NoFail, NULL );
	Canvas = CastChecked<UCanvas>(StaticConstructObject( CanvasClass ));
	Canvas->Init( this );

	// Create input system.
	UClass* InputClass = StaticLoadClass( UInput::StaticClass(), NULL, TEXT("ini:Engine.Engine.Input"), NULL, LOAD_NoFail, NULL );
	Input = (UInput*)StaticConstructObject( InputClass );

	// Set initial time.
	LastUpdateTime = appSeconds();

	unguard;
}

//
// Close a viewport.
//warning: Lots of interrelated stuff is happening here between the viewport code,
// object code, platform-specific viewport manager code, and Windows.
//
void UViewport::Destroy()
{
	guard(UViewport::Destroy);

	// Temporary for editor!!
	if( GetOuterUClient()->Engine->Audio && GetOuterUClient()->Engine->Audio->GetViewport()==this )
		GetOuterUClient()->Engine->Audio->SetViewport( NULL );

	// Close the viewport window.
	guard(CloseWindow);
	CloseWindow();
	unguard;

	#if _MSC_VER

	// Delete the input subsystem.
	guard(ExitInput);
	delete Input;
	unguard;

	// Delete the console.
	guard(DeleteConsole);
	if( Console )
		delete Console;
	unguard;

	// Delete the canvas.
	guard(DeleteCanvas);
	delete Canvas;
	unguard;

	#endif

	// Shut down rendering. Outside the _MSC_VER block: every viewport close/recreate (resolution change,
	// FLUSH, fullscreen toggle) must exit the device on every compiler, or it leaks and a second one starts.
	guard(DeleteRenDev);
	if( RenDev )
	{
		RenDev->Exit();
		delete RenDev;
	}
	unguard;

	// Remove from viewport list.
	GetOuterUClient()->Viewports.RemoveItem( this );

	Super::Destroy();
	unguardobj;
}

/*---------------------------------------------------------------------------------------
	Viewport information functions.
---------------------------------------------------------------------------------------*/

//
// Is this camrea a wireframe view?
//
UBOOL UViewport::IsWire()
{
	guard(UViewport::IsWire);
	return
	Actor &&
	(	Actor->GetLevel()->Model->Nodes.Num()==0
	||	Actor->RendMap==REN_OrthXY
	||	Actor->RendMap==REN_OrthXZ
	||	Actor->RendMap==REN_OrthYZ
	||	Actor->RendMap==REN_Wire );

	unguard;
}

/*-----------------------------------------------------------------------------
	Viewport locking & unlocking.
-----------------------------------------------------------------------------*/

//
// Lock the viewport for rendering.
//
UBOOL UViewport::Lock( FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData, INT* HitSize )
{
	guard(UViewport::Lock);
	check(RenDev);

	// Set info.
	CurrentTime = appSeconds();
	HitTesting = HitData!=NULL;
	RenderFlags = RenderLockFlags;
	ExtraPolyFlags = Actor && ((Actor->RendMap==REN_PolyCuts) || (Actor->RendMap==REN_Zones)) ? PF_NoMerge : 0;
	FrameCount++;

	// Lock rendering device.
	RenDev->Lock( FlashScale, FlashFog, ScreenClear, RenderLockFlags, HitData, HitSize );

	// Successfully locked it.
	return 1;
	unguard;
}

//
// Unlock the viewport.
//
void UViewport::Unlock( UBOOL Blit )
{
	guard(UViewport::Unlock);
	check(RenDev);
	check(HitSizes.Num()==0);

	// Unlock rendering device.
	RenDev->Unlock( Blit );

	// Update time.
	if( Blit )
		LastUpdateTime = CurrentTime;

	unguard;
}

UBOOL  UViewport::ScreenShot( const TCHAR* Filename )
{
	FMemMark Mark(GMem);
	FColor* Buf = new(GMem,SizeX*SizeY)FColor;
	RenDev->ReadPixels( Buf );
	FArchive* Ar = GFileManager->CreateFileWriter( Filename );
	if( Ar )
	{
		// Types.
		#if _MSC_VER
			#pragma pack (push,1)
		#endif
		struct BITMAPFILEHEADER
		{
			_WORD   bfType GCC_PACK(1);
			DWORD   bfSize GCC_PACK(1);
			_WORD   bfReserved1 GCC_PACK(1); 
			_WORD   bfReserved2 GCC_PACK(1);
			DWORD   bfOffBits GCC_PACK(1);
		} FH; 
		struct BITMAPINFOHEADER
		{
			DWORD  biSize GCC_PACK(1); 
			INT    biWidth GCC_PACK(1);
			INT    biHeight GCC_PACK(1);
			_WORD  biPlanes GCC_PACK(1);
			_WORD  biBitCount GCC_PACK(1);
			DWORD  biCompression GCC_PACK(1);
			DWORD  biSizeImage GCC_PACK(1);
			INT    biXPelsPerMeter GCC_PACK(1); 
			INT    biYPelsPerMeter GCC_PACK(1);
			DWORD  biClrUsed GCC_PACK(1);
			DWORD  biClrImportant GCC_PACK(1); 
		} IH;
		#if _MSC_VER
			#pragma pack (pop)
		#endif

		// File header.
		FH.bfType		= 'B' + 256*'M';
		FH.bfSize		= sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + 3 * SizeX * SizeY;
		FH.bfReserved1	= 0;
		FH.bfReserved2	= 0;
		FH.bfOffBits	= sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
		Ar->Serialize( &FH, sizeof(FH) );

		// Info header.
		IH.biSize			= sizeof(BITMAPINFOHEADER);
		IH.biWidth			= SizeX;
		IH.biHeight			= SizeY;
		IH.biPlanes			= 1;
		IH.biBitCount		= 24;
		IH.biCompression	= 0; //BI_RGB
		IH.biSizeImage		= SizeX * SizeY * 3;
		IH.biXPelsPerMeter	= 0;
		IH.biYPelsPerMeter	= 0;
		IH.biClrUsed		= 0;
		IH.biClrImportant	= 0;
		Ar->Serialize( &IH, sizeof(IH) );

		// Colors. BMP rows are bottom-up and the renderers' ReadPixels return top-down rows on every target.
		for( INT i=SizeY-1; i>=0; i-- )
			for( INT j=0; j<SizeX; j++ )
				Ar->Serialize( &Buf[i*SizeX+j], 3 );

		// Success.
		delete Ar;
	}
	Mark.Pop();
	return 1;
}

/*-----------------------------------------------------------------------------
	Command line.
-----------------------------------------------------------------------------*/

//
// UViewport command line.
//
UBOOL UViewport::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(UViewport::Exec);
	check(Actor);
	if( Input && Input->Exec(Cmd,Ar) )
	{
		return 1;
	}
	else if( RenDev && RenDev->Exec(Cmd,Ar) )
	{
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("GETPING")) )
	{
		Ar.Logf( TEXT("0") );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("INJECT")) )
	{
		UNetDriver* Driver = Actor->GetLevel()->NetDriver;
		if( Driver && Driver->ServerConnection )
		{
			Ar.Logf(TEXT("Injecting: %s"),Cmd);
			Driver->ServerConnection->Logf( Cmd );
		}
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("NETSPEED")) )
	{
		INT Rate = appAtoi(Cmd);
		UNetDriver* Driver = Actor->GetLevel()->NetDriver;
		GetDefault<UPlayer>()->ConfiguredInternetSpeed = Rate;
		GetDefault<UPlayer>()->SaveConfig();
		if( Rate>=500 && Driver && Driver->ServerConnection )
		{
			if( !Driver->ServerConnection->URL.HasOption(TEXT("LAN")) )
			{
				CurrentNetSpeed = Driver->ServerConnection->CurrentNetSpeed = Clamp( Rate, 500, Driver->MaxClientRate );
				Driver->ServerConnection->Logf( TEXT("NETSPEED %i"), Rate );
			}
		}
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("LANSPEED")) )
	{
		INT Rate = appAtoi(Cmd);
		UNetDriver* Driver = Actor->GetLevel()->NetDriver;
		GetDefault<UPlayer>()->ConfiguredInternetSpeed = Rate;
		GetDefault<UPlayer>()->SaveConfig();
		if( Rate>=500 && Driver && Driver->ServerConnection )
		{
			if( Driver->ServerConnection->URL.HasOption(TEXT("LAN")) )
			{
				CurrentNetSpeed = Driver->ServerConnection->CurrentNetSpeed = Clamp( Rate, 500, Driver->MaxClientRate );
				Driver->ServerConnection->Logf( TEXT("NETSPEED %i"), Rate );
			}
		}
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("SHOWALL")) )
	{
		for( INT i=0; i<Actor->GetLevel()->Actors.Num(); i++ )
			if( Actor->GetLevel()->Actors(i) )
				Actor->GetLevel()->Actors(i)->bHidden = 0;
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("REPORT")) )
	{
		FStringOutputDevice Str;
#if 1 //Logging added by Legend on 4/12/2000
		Str.Logf ( TEXT("Report: %s\r\n"), Cmd );
#else
		Str.Log ( TEXT("Report:\r\n") );
#endif
		Str.Logf( TEXT("   Version: %s %s\r\n"), appFromAnsi(__DATE__), appFromAnsi(__TIME__) );
		Str.Logf( TEXT("   Player class: %s\r\n"), Actor->GetClass()->GetName() );
		Str.Logf( TEXT("   URL: %s\r\n"), *Actor->GetLevel()->URL.String() );
		Str.Logf( TEXT("   Location: %i %i %i\r\n"), appRound(Actor->Location.X), appRound(Actor->Location.Y), appRound(Actor->Location.Z) );
		if( Actor->Level->Game==NULL )
		{
			Str.Logf( TEXT("   Network client\r\n") );
		}
		else
		{
			Str.Logf( TEXT("   Game class: %s\r\n"), Actor->Level->Game->GetClass()->GetName() );
			Str.Logf( TEXT("   Difficulty: %i\r\n"), Actor->Level->Game->Difficulty );
		}
#if 1 //Logging added by Legend on 4/12/2000
		Ar.Logf( *Str, Cmd );
#endif
		appClipboardCopy( *Str );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("SHOT")) )
	{
		// Screenshot.
		TCHAR File[512];
		if( ParseToken( Cmd, File, ARRAY_COUNT(File), 0 ) )
		{
			// user specified a filename so save in SaveSlotPath
			return ScreenShot( *(GSys->SaveSlotPath * File) );
		}
		else
		{
			// default ScreenShot
			for( INT i=0; i<256; i++ )
			{
				appSprintf( File, TEXT("%sShot%04i.bmp"), appUserDir(), i );
				if( GFileManager->FileSize(File) < 0 )
					break;
			}
			if( GFileManager->FileSize(File) < 0 )
				return ScreenShot( File );
		}
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("SNAP")) )
	{
		// Take a copy of the viewport image, shrink and store.
		//----------------------------------------------------
		INT shrinkFactor = appAtoi(Cmd);
		int shrinkage    = appPow(2, shrinkFactor);

		snapU = SizeX / shrinkage;
		snapV = SizeY / shrinkage;

		FPushMemTag Tag(TEXT("Snap"));
		FMemMark Mark(GMem);
		FColor* SrcBuff = new(GMem,SizeX*SizeY)FColor;
		RenDev->ReadPixels( SrcBuff );

		snapBuff.Empty ();
		snapBuff.Add (snapU*snapV);

		FColor * DstBuff = &snapBuff (0);

		for (unsigned int y = 0; y<snapV; ++y)
		{
			FColor * pDstLine = DstBuff + y*snapU;
			FColor * pSrcTopLine = SrcBuff + y*SizeX* shrinkage;
			
			for (unsigned int x = 0; x<snapU; ++x)
			{
				FColor * pSrcTopLeft = pSrcTopLine + x* shrinkage;

				// Calculate Dest pixel by adding all source pixels together
				int dest [4];
				for (unsigned int i=0; i<4; ++i)
					dest[i] = 0;

				for (int srcY = 0; srcY<shrinkage; ++srcY)
				{
					FColor * pSrcLine = pSrcTopLeft + srcY*SizeX;

					for (int srcX = 0; srcX<shrinkage; ++srcX)
					{
						dest[0] += pSrcLine[srcX].R;
						dest[1] += pSrcLine[srcX].G;
						dest[2] += pSrcLine[srcX].B;
						dest[3] += pSrcLine[srcX].A;
					}
				}

				// divide dest by total number of pixels in source block
				for (unsigned int i=0; i<4; ++i)
					dest [i] /= (shrinkage*shrinkage);

				pDstLine[x].R = dest[0];
				pDstLine[x].G = dest[1];
				pDstLine[x].B = dest[2];
				pDstLine[x].A = dest[3];
			}
		}

		Mark.Pop (); // destroy temporary buffer

		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("THUMBNAIL")) )
	{
		// Take a copy of the viewport image, shrink and store.
		//----------------------------------------------------
//		INT shrinkFactor = appAtoi(Cmd);
//		int shrinkage    = appPow(2, shrinkFactor);
		
		INT ThumbSizeX, ThumbSizeY;
		
		if( !Parse(Cmd, TEXT("x="), ThumbSizeX ) )
			ThumbSizeX = 160;
		
		if( !Parse(Cmd, TEXT("y="), ThumbSizeY ) )
			ThumbSizeY = 120;
		
		if( ThumbSizeX == 0 ) ThumbSizeX = 160;
		if( ThumbSizeY == 0 ) ThumbSizeY = 120;
		
		// Shrink by the factor that fits both axes and sample the centred block: SizeX/ThumbSizeX alone would
		// read past the end of SrcBuff on a viewport wider than 4:3 (1280x720 needs 960 rows).
		int shrinkage    = Max( 1, Min( SizeX / ThumbSizeX, SizeY / ThumbSizeY ) );
		const int SnapOffX = Max( 0, (SizeX - ThumbSizeX*shrinkage) / 2 );
		const int SnapOffY = Max( 0, (SizeY - ThumbSizeY*shrinkage) / 2 );
		
		snapU = ThumbSizeX;
		snapV = ThumbSizeY;
		
		FPushMemTag Tag(TEXT("Snap"));
		FMemMark Mark(GMem);
		FColor* SrcBuff = new(GMem,SizeX*SizeY)FColor;
		RenDev->ReadPixels( SrcBuff );

		snapBuff.Empty ();
		snapBuff.Add (snapU*snapV);

		FColor * DstBuff = &snapBuff (0);

		for (unsigned int y = 0; y<snapV; ++y)
		{
			FColor * pDstLine = DstBuff + y*snapU;
			FColor * pSrcTopLine = SrcBuff + (SnapOffY + y*shrinkage)*SizeX + SnapOffX;
			
			for (unsigned int x = 0; x<snapU; ++x)
			{
				FColor * pSrcTopLeft = pSrcTopLine + x* shrinkage;

				// Calculate Dest pixel by adding all source pixels together
				int dest [4];
				for (unsigned int i=0; i<4; ++i)
					dest[i] = 0;

				for (int srcY = 0; srcY<shrinkage; ++srcY)
				{
					FColor * pSrcLine = pSrcTopLeft + srcY*SizeX;

					for (int srcX = 0; srcX<shrinkage; ++srcX)
					{
						dest[0] += pSrcLine[srcX].R;
						dest[1] += pSrcLine[srcX].G;
						dest[2] += pSrcLine[srcX].B;
						dest[3] += pSrcLine[srcX].A;
					}
				}

				// divide dest by total number of pixels in source block
				for (unsigned int i=0; i<4; ++i)
					dest [i] /= (shrinkage*shrinkage);

				pDstLine[x].R = dest[0];
				pDstLine[x].G = dest[1];
				pDstLine[x].B = dest[2];
				pDstLine[x].A = dest[3];
			}
		}

		Mark.Pop (); // destroy temporary buffer

		return 1;
	}	
	else if( ParseCommand(&Cmd,TEXT("SAVETHUMBNAIL")) )
	{
		TCHAR File[64];
		if( ParseToken( Cmd, File, ARRAY_COUNT(File), 0 ) && snapBuff.Num () )
		{
			FArchive* Ar = GFileManager->CreateFileWriter( *(GSys->SaveSlotPath * File) );
			if( Ar )
			{
				// USE the REAL proportions of our snapshot
				unsigned int destWidth  = snapU;
				unsigned int destHeight = snapV;
				
				// Types.
				#if _MSC_VER
					#pragma pack (push,1)
				#endif
				struct BITMAPFILEHEADER
				{
					_WORD   bfType GCC_PACK(1);
					DWORD   bfSize GCC_PACK(1);
					_WORD   bfReserved1 GCC_PACK(1); 
					_WORD   bfReserved2 GCC_PACK(1);
					DWORD   bfOffBits GCC_PACK(1);
				} FH; 
				struct BITMAPINFOHEADER
				{
					DWORD  biSize GCC_PACK(1); 
					INT    biWidth GCC_PACK(1);
					INT    biHeight GCC_PACK(1);
					_WORD  biPlanes GCC_PACK(1);
					_WORD  biBitCount GCC_PACK(1);
					DWORD  biCompression GCC_PACK(1);
					DWORD  biSizeImage GCC_PACK(1);
					INT    biXPelsPerMeter GCC_PACK(1); 
					INT    biYPelsPerMeter GCC_PACK(1);
					DWORD  biClrUsed GCC_PACK(1);
					DWORD  biClrImportant GCC_PACK(1); 
				} IH;
				#if _MSC_VER
					#pragma pack (pop)
				#endif

				// File header.
				FH.bfType		= 'B' + 256*'M';
				FH.bfSize		= sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + 3 * destWidth * destHeight;
				FH.bfReserved1	= 0;
				FH.bfReserved2	= 0;
				FH.bfOffBits	= sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
				Ar->Serialize( &FH, sizeof(FH) );

				// Info header.
				IH.biSize			= sizeof(BITMAPINFOHEADER);
				IH.biWidth			= destWidth;
				IH.biHeight			= destHeight;
				IH.biPlanes			= 1;
				IH.biBitCount		= 24;
				IH.biCompression	= 0; //BI_RGB
				IH.biSizeImage		= destWidth * destHeight * 3;
				IH.biXPelsPerMeter	= 0;
				IH.biYPelsPerMeter	= 0;
				IH.biClrUsed		= 0;
				IH.biClrImportant	= 0;
				Ar->Serialize( &IH, sizeof(IH) );

				FColor * Buf = snapBuff.GetData ();

				// Copy the actual data
				for(int i=(int)snapV-1; i>=0; i-- )
				{
					unsigned int j;
					for( j=0; j<snapU; j++ )
						Ar->Serialize( &Buf[i*snapU+j], 3 );

					// fill extra space to make bitmap of texture proportions
					for (;j<destWidth; j++)
						Ar->Serialize(&Buf[0], 3 );
				}

				// Success.
				delete Ar;
			}
		}
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("SAVESNAP")) )
	{
		TCHAR File[64];
		if( ParseToken( Cmd, File, ARRAY_COUNT(File), 0 ) && snapBuff.Num () )
		{
			FArchive* Ar = GFileManager->CreateFileWriter( *(FString(appUserDir()) * File) );
			if( Ar )
			{
				// calculate proportions of final bmp to be suitable as a texture
				unsigned int destWidth, destHeight;
				int widthFactor =0;
				for (destWidth = snapU; destWidth; destWidth>>=1)
					widthFactor ++;
				destWidth = appPow(2, widthFactor);
				
				int heightFactor =0;
				for (destHeight = snapV; destHeight; destHeight>>=1)
					heightFactor ++;
				destHeight = appPow(2, heightFactor);

				// Types.
				#if _MSC_VER
					#pragma pack (push,1)
				#endif
				struct BITMAPFILEHEADER
				{
					_WORD   bfType GCC_PACK(1);
					DWORD   bfSize GCC_PACK(1);
					_WORD   bfReserved1 GCC_PACK(1); 
					_WORD   bfReserved2 GCC_PACK(1);
					DWORD   bfOffBits GCC_PACK(1);
				} FH; 
				struct BITMAPINFOHEADER
				{
					DWORD  biSize GCC_PACK(1); 
					INT    biWidth GCC_PACK(1);
					INT    biHeight GCC_PACK(1);
					_WORD  biPlanes GCC_PACK(1);
					_WORD  biBitCount GCC_PACK(1);
					DWORD  biCompression GCC_PACK(1);
					DWORD  biSizeImage GCC_PACK(1);
					INT    biXPelsPerMeter GCC_PACK(1); 
					INT    biYPelsPerMeter GCC_PACK(1);
					DWORD  biClrUsed GCC_PACK(1);
					DWORD  biClrImportant GCC_PACK(1); 
				} IH;
				#if _MSC_VER
					#pragma pack (pop)
				#endif

				// File header.
				FH.bfType		= 'B' + 256*'M';
				FH.bfSize		= sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + 3 * destWidth * destHeight;
				FH.bfReserved1	= 0;
				FH.bfReserved2	= 0;
				FH.bfOffBits	= sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
				Ar->Serialize( &FH, sizeof(FH) );

				// Info header.
				IH.biSize			= sizeof(BITMAPINFOHEADER);
				IH.biWidth			= destWidth;
				IH.biHeight			= destHeight;
				IH.biPlanes			= 1;
				IH.biBitCount		= 24;
				IH.biCompression	= 0; //BI_RGB
				IH.biSizeImage		= destWidth * destHeight * 3;
				IH.biXPelsPerMeter	= 0;
				IH.biYPelsPerMeter	= 0;
				IH.biClrUsed		= 0;
				IH.biClrImportant	= 0;
				Ar->Serialize( &IH, sizeof(IH) );

				FColor * Buf = snapBuff.GetData ();

				// fill extra space to make bitmap of texture proportions
				int i;
				for( i=(int)destHeight-1; i>=(int)snapV; i-- )
				{
					for( unsigned int j=0; j<destWidth; j++ )
						Ar->Serialize(&Buf[0], 3 );
				}

				// Copy the actual data
				for( i=(int)snapV-1; i>=0; i-- )
				{
					unsigned int j;
					for( j=0; j<snapU; j++ )
						Ar->Serialize( &Buf[i*snapU+j], 3 );

					// fill extra space to make bitmap of texture proportions
					for (;j<destWidth; j++)
						Ar->Serialize(&Buf[0], 3 );
				}

				// Success.
				delete Ar;
			}
		}
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("SHOWACTORS")) )
	{
		Actor->ShowFlags |= SHOW_Actors;
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("HIDEACTORS")) )
	{
		Actor->ShowFlags &= ~SHOW_Actors;
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("RMODE")) )
	{
		INT Mode = appAtoi(Cmd);
		if( Mode>REN_None && Mode<REN_MAX )
			Actor->RendMap = Mode;
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("EXEC")) )
	{
		TCHAR Filename[64];
		if( ParseToken( Cmd, Filename, ARRAY_COUNT(Filename), 0 ) )
			ExecMacro( Filename, Ar );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("SHOWBONES")) )
	{
		if (appAtoi(Cmd))
			Actor->ShowFlags |= SHOW_Bones;
		else
			Actor->ShowFlags &= ~SHOW_Bones;
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("SHOWBOUNDS")) )
	{
		if (appAtoi(Cmd))
			Actor->ShowFlags |= SHOW_Bounds;
		else
			Actor->ShowFlags &= ~SHOW_Bounds;
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("SHOWCOLL")) )
	{
		// Collision cylinders (URender::DrawActorSprite).
		if (appAtoi(Cmd))
			Actor->ShowFlags |= SHOW_Collision;
		else
			Actor->ShowFlags &= ~SHOW_Collision;
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("SHOWPATHS")) )
	{
		// Interpolation and patrol paths in game (URender::DrawActorSprite).
		if (appAtoi(Cmd))
			Actor->ShowFlags |= SHOW_Paths;
		else
			Actor->ShowFlags &= ~SHOW_Paths;
		return 1;
	}
	else if( Console && Console->ScriptConsoleExec(Cmd,Ar,Actor) )
	{
		return 1;
	}
	else if( UPlayer::Exec(Cmd,Ar) )
	{
		return 1;
	}
	else return 0;
	unguard;
}

//
// Execute a macro on this viewport.
//
void UViewport::ExecMacro( const TCHAR* Filename, FOutputDevice& Ar )
{
	guard(UViewport::ExecMacro);

	// Create text buffer and prevent garbage collection.
	UTextBuffer* Text = ImportObject<UTextBuffer>( GetTransientPackage(), NAME_None, 0, Filename );
	if( Text )
	{
		Text->AddToRoot();
		debugf( TEXT("Execing %s"), Filename );
		TCHAR Temp[256];
		const TCHAR* Data = *Text->Text;
		while( ParseLine( &Data, Temp, ARRAY_COUNT(Temp) ) )
			Exec( Temp, Ar );
		Text->RemoveFromRoot();
		delete Text;
	}
	else Ar.Logf( NAME_ExecWarning, LocalizeError("FileNotFound",TEXT("Core")), Filename );

	unguard;
}

/*-----------------------------------------------------------------------------
	UViewport FArchive interface.
-----------------------------------------------------------------------------*/

//
// Output a message on the viewport's console.
//
void UViewport::Serialize( const TCHAR* Data, EName MsgType )
{
	guard(UViewport::Serialize);

	// Pass to console.
	if( Console )
		Console->Serialize( Data, MsgType );

	unguard;
}

/*-----------------------------------------------------------------------------
	Input.
-----------------------------------------------------------------------------*/

//
// Read input from the viewport.
//
void UViewport::ReadInput( FLOAT DeltaSeconds )
{
	guard(UViewport::ReadInput);
	check(Input);

	// Update input.
	if( DeltaSeconds!=-1.f )
		UpdateInput( 0 );

	// Get input from input system.
	Input->ReadInput( DeltaSeconds, *GLog );

	unguard;
}

/*-----------------------------------------------------------------------------
	Viewport hit testing.
-----------------------------------------------------------------------------*/

//
// Viewport hit-test pushing.
//
void UViewport::PushHit( const HHitProxy& Hit, INT Size )
{
	guard(UViewport::PushHit);

	Hit.Size = Size;
	HitSizes.AddItem(Size);
	RenDev->PushHit( (BYTE*)&Hit, Size );

	unguard;
}

//
// Pop the most recently pushed hit.
//
void UViewport::PopHit( UBOOL bForce )
{
	guard(UViewport::PopHit);
	checkSlow(RenDev);
	checkSlow(HitTesting);
	check(HitSizes.Num());

	RenDev->PopHit( HitSizes.Pop(), bForce );

	unguard;
}

//
// Execute all hits in the hit buffer.
//
void UViewport::ExecuteHits( const FHitCause& Cause, BYTE* HitData, INT HitSize )
{
	guard(UViewport::ExecuteHits);

	// String together the hit stack.
	HHitProxy* TopHit=NULL;
	while( HitSize>0 )
	{
		HHitProxy* ThisHit = (HHitProxy*)HitData;
		HitData           += ThisHit->Size;
		HitSize           -= ThisHit->Size;
		ThisHit->Parent    = TopHit;
		TopHit             = ThisHit;
	}
	check(HitSize==0);

	// Call the innermost hit.
	if( TopHit )
		TopHit->Click( Cause );

	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
