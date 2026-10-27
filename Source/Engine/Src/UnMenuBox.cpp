/*=============================================================================
	UnMenuBox.cpp: HGame's menu book in the largest 4:3 box of the screen.

	HPConsole.RenderUWindow (and FEBook.Paint) scale the UWindow root by the screen's width alone,
	SetScale(RealWidth/640), so on a screen wider than 4:3 the 640x480 book is taller than the screen. When the
	console class loads, RenderUWindow is wrapped natively: while the book is open its script runs on a canvas
	whose origin, clip and size are the largest 4:3 box of the screen, centred, with the absolute pointer moved
	into that box, and the rest of the screen is drawn black. The script is not changed; on a 4:3 screen the box
	is the whole screen and nothing is wrapped.
=============================================================================*/

#include "EnginePrivate.h"

// Found once, when the console class is wrapped.
static UFunction*		GRenderUWindow	= NULL;	// HPConsole.RenderUWindow
static UProperty*		GCanvasParm		= NULL;	// its Canvas parameter
static UObjectProperty*	GMenuBook		= NULL;	// HPConsole.menuBook
static UBoolProperty*	GBookIsOpen		= NULL;	// FEBook.bIsOpen
static UTexture*		GBarTexture		= NULL;	// UWindow.BlackTexture

void UConsole::BoxMenuBook( UClass* ConsoleClass )
{
	guard(UConsole::BoxMenuBook);
	if( !ConsoleClass || appStricmp( ConsoleClass->GetName(), TEXT("HPConsole") )!=0 )
		return;
	UFunction* Function = FindField<UFunction>( ConsoleClass, TEXT("RenderUWindow") );
	UProperty* CanvasParm = Function ? FindField<UProperty>( Function, TEXT("Canvas") ) : NULL;
	UObjectProperty* MenuBook = FindField<UObjectProperty>( ConsoleClass, TEXT("menuBook") );
	UBoolProperty* IsOpen = MenuBook && MenuBook->PropertyClass ? FindField<UBoolProperty>( MenuBook->PropertyClass, TEXT("bIsOpen") ) : NULL;
	UTexture* Bar = LoadObject<UTexture>( NULL, TEXT("UWindow.BlackTexture"), NULL, LOAD_NoWarn, NULL );
	if
	(	!Function || Function->GetOwnerClass()!=ConsoleClass || (Function->FunctionFlags & FUNC_Native)
	||	!CanvasParm || !(CanvasParm->PropertyFlags & CPF_Parm) || !Cast<UObjectProperty>(CanvasParm)
	||	!IsOpen || !Bar )
	{
		debugf( NAME_Init, TEXT("Menu book: not boxed (HPConsole.RenderUWindow, menuBook or FEBook.bIsOpen not found)") );
		return;
	}
	GRenderUWindow	= Function;
	GCanvasParm		= CanvasParm;
	GMenuBook		= MenuBook;
	GBookIsOpen		= IsOpen;
	GBarTexture		= Bar;
	Function->FunctionFlags |= FUNC_Native;
	Function->Func = (Native)&UConsole::execRenderUWindowBoxed;
	debugf( NAME_Init, TEXT("Menu book: drawn in the largest 4:3 box of the screen") );
	unguard;
}

// A black rectangle in screen pixels.
static void DrawBar( UCanvas* Canvas, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL )
{
	if( XL>0.f && YL>0.f )
		Canvas->DrawTile( GBarTexture, X, Y, XL, YL, 0.f, 0.f, GBarTexture->USize, GBarTexture->VSize, NULL, 1.f,
			FPlane(1.f,1.f,1.f,0.f), FPlane(0.f,0.f,0.f,0.f), PF_TwoSided );
}

void UConsole::execRenderUWindowBoxed( FFrame& Stack, RESULT_DECL )
{
	guard(UConsole::execRenderUWindowBoxed);
	UFunction* Function = GRenderUWindow;

	// Called from script the parameter is read from the caller's code; called as an event it is in the frame.
	const UBOOL AsEvent = Stack.Node==Function;
	UCanvas* Canvas = NULL;
	if( AsEvent )
		Canvas = *(UCanvas**)(Stack.Locals + GCanvasParm->Offset);
	else
	{
		P_GET_OBJECT(UCanvas,C);
		P_FINISH;
		Canvas = C;
	}

	// The largest 4:3 box of the screen, centred, while the book is open.
	UObject* Book = *(UObject**)((BYTE*)this + GMenuBook->Offset);
	const UBOOL Open = Book && (*(BITFIELD*)((BYTE*)Book + GBookIsOpen->Offset) & GBookIsOpen->BitMask);
	const INT SizeX = Canvas ? Canvas->X : 0, SizeY = Canvas ? Canvas->Y : 0;
	const FLOAT Scale = Min( SizeX/640.f, SizeY/480.f );
	const INT BoxXL = appRound( 640.f*Scale ), BoxYL = appRound( 480.f*Scale );
	const UBOOL Boxed = Open && Canvas && Canvas->Viewport && BoxXL>0 && BoxYL>0 && (BoxXL!=SizeX || BoxYL!=SizeY);
	const INT BoxX = (SizeX - BoxXL) / 2, BoxY = (SizeY - BoxYL) / 2;

	FLOAT OrgX=0.f, OrgY=0.f, ClipX=0.f, ClipY=0.f, MouseX=0.f, MouseY=0.f;
	if( Boxed )
	{
		OrgX = Canvas->OrgX;
		OrgY = Canvas->OrgY;
		ClipX = Canvas->ClipX;
		ClipY = Canvas->ClipY;
		MouseX = Canvas->Viewport->WindowsMouseX;
		MouseY = Canvas->Viewport->WindowsMouseY;
		DrawBar( Canvas, OrgX, OrgY, BoxX, SizeY );
		DrawBar( Canvas, OrgX + BoxX + BoxXL, OrgY, SizeX - BoxX - BoxXL, SizeY );
		DrawBar( Canvas, OrgX + BoxX, OrgY, BoxXL, BoxY );
		DrawBar( Canvas, OrgX + BoxX, OrgY + BoxY + BoxYL, BoxXL, SizeY - BoxY - BoxYL );
		Canvas->OrgX = OrgX + BoxX;
		Canvas->OrgY = OrgY + BoxY;
		Canvas->ClipX = BoxXL;
		Canvas->ClipY = BoxYL;
		Canvas->X = BoxXL;
		Canvas->Y = BoxYL;
		Canvas->Viewport->WindowsMouseX = MouseX - BoxX;
		Canvas->Viewport->WindowsMouseY = MouseY - BoxY;
	}

	// The script's own body.
	if( AsEvent )
		ProcessInternal( Stack, Result );
	else
	{
		BYTE* Locals = (BYTE*)appAlloca( Function->PropertiesSize );
		appMemzero( Locals, Function->PropertiesSize );
		*(UCanvas**)(Locals + GCanvasParm->Offset) = Canvas;
		FFrame NewStack( this, Function, 0, Locals, &Stack );
		ProcessInternal( NewStack, Result );
		for( UProperty* P=Function->ConstructorLink; P; P=P->ConstructorLinkNext )
			P->DestroyValue( Locals + P->Offset );
	}

	if( Boxed )
	{
		Canvas->OrgX = OrgX;
		Canvas->OrgY = OrgY;
		Canvas->ClipX = ClipX;
		Canvas->ClipY = ClipY;
		Canvas->X = SizeX;
		Canvas->Y = SizeY;
		Canvas->Viewport->WindowsMouseX = MouseX;
		Canvas->Viewport->WindowsMouseY = MouseY;
	}
	unguard;
}
