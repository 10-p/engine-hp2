/*=============================================================================
	UnGame.cpp: Unreal game engine.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnRender.h"
#include "UnNet.h"

/*-----------------------------------------------------------------------------
	Object class implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UGameEngine);

/*-----------------------------------------------------------------------------
	cleanup!!
-----------------------------------------------------------------------------*/

void UGameEngine::PaintProgress()
{
	guard(PaintProgress);

	if( GIsDrawing )
		return;
	FPlane LoadFog(0.f,0.1f,0.25f,0.2f);
	UViewport* Viewport=Client->Viewports(0);
	Exchange(Viewport->Actor->FlashFog,LoadFog);
	Draw( Viewport );
	Exchange(Viewport->Actor->FlashFog,LoadFog);

	unguard;
}

INT UGameEngine::ChallengeResponse( INT Challenge )
{
	guard(UGameEngine::ChallengeResponse);
	return (Challenge*237) ^ (0x93fe92Ce) ^ (Challenge>>16) ^ (Challenge<<16);
	unguard;
}

void UGameEngine::UpdateConnectingMessage()
{
	guard(UGameEngine::UpdateConnectingMessage);
	if( GPendingLevel && Client && Client->Viewports.Num() )
	{
		APlayerPawn* Actor = Client->Viewports(0)->Actor;
		if( Actor->ProgressTimeOut<Actor->Level->TimeSeconds )
		{
			TCHAR Msg1[256], Msg2[256];
			if( GPendingLevel->DemoRecDriver )
			{
				appSprintf( Msg1, TEXT("") );
				appSprintf( Msg2, *GPendingLevel->URL.Map );
			}
			else
			{
				appSprintf( Msg1, LocalizeProgress("ConnectingText") );
				appSprintf( Msg2, LocalizeProgress("ConnectingURL"), *GPendingLevel->URL.Host, *GPendingLevel->URL.Map );
			}
			SetProgress( Msg1, Msg2, 60.f );
		}
	}
	unguard;
}
void UGameEngine::BuildServerMasterMap( UNetDriver* NetDriver, ULevel* InLevel )
{
	guard(UGameEngine::BuildServerMasterMap);
	check(NetDriver);
	check(InLevel);
	BeginLoad();
	{
		// Init LinkerMap.
		check(InLevel->GetLinker());
		NetDriver->MasterMap->AddLinker( InLevel->GetLinker() );

		// Load server-required packages.
		for( INT i=0; i<ServerPackages.Num(); i++ )
		{
			debugf( TEXT("Server Package: %s"), *ServerPackages(i) );
			ULinkerLoad* Linker = GetPackageLinker( NULL, *ServerPackages(i), LOAD_NoFail, NULL, NULL );
			if( NetDriver->MasterMap->AddLinker( Linker )==INDEX_NONE )
				debugf( TEXT("   (server-side only)") );
		}

		// Add GameInfo's package to map.
		check(InLevel->GetLevelInfo());
		check(InLevel->GetLevelInfo()->Game);
		check(InLevel->GetLevelInfo()->Game->GetClass()->GetLinker());
		NetDriver->MasterMap->AddLinker( InLevel->GetLevelInfo()->Game->GetClass()->GetLinker() );

		// Precompute linker info.
		NetDriver->MasterMap->Compute();
	}
	EndLoad();
	unguard;
}

/*-----------------------------------------------------------------------------
	Game init and exit.
-----------------------------------------------------------------------------*/

//
// Construct the game engine.
//
UGameEngine::UGameEngine()
: LastURL(TEXT(""))
, ServerActors( E_NoInit )
, ServerPackages( E_NoInit )
{}

//
// Class creator.
//
void UGameEngine::StaticConstructor()
{
	guard(UGameEngine::StaticConstructor);

	UArrayProperty* A = new(GetClass(),TEXT("ServerActors"),RF_Public)UArrayProperty( CPP_PROPERTY(ServerActors), TEXT("Settings"), CPF_Config );
	A->Inner = new(A,TEXT("StrProperty0"),RF_Public)UStrProperty;

	UArrayProperty* B = new(GetClass(),TEXT("ServerPackages"),RF_Public)UArrayProperty( CPP_PROPERTY(ServerPackages), TEXT("Settings"), CPF_Config );
	B->Inner = new(B,TEXT("StrProperty0"),RF_Public)UStrProperty;

	unguard;
}

//
// Initialize the game engine.
//
void UGameEngine::Init()
{
	guard(UGameEngine::Init);
	check(sizeof(*this)==GetClass()->GetPropertiesSize());

	// Call base.
	UEngine::Init();

	// Init variables.
	GLevel = NULL;
	GViewport = NULL;
	GIsDrawing = false;

	// Delete temporary files in cache.
	appCleanFileCache();

	// If not a dedicated server.
	if( GIsClient )
	{	
		// Init client.
		UClass* ClientClass = StaticLoadClass( UClient::StaticClass(), NULL, TEXT("ini:Engine.Engine.ViewportManager"), NULL, LOAD_NoFail, NULL );
		Client = ConstructObject<UClient>( ClientClass );
		Client->Init( this );

		// Init rendering.
		UClass* RenderClass = StaticLoadClass( URenderBase::StaticClass(), NULL, TEXT("ini:Engine.Engine.Render"), NULL, LOAD_NoFail, NULL );
		Render = ConstructObject<URenderBase>( RenderClass );
		Render->Init( this );
	}

	// Load the entry level.
	FString Error;
	if( Client )
	{
//		FPushMemTag Push(TEXT("LoadEntry"));
		if( !LoadMap( FURL(TEXT("Entry")), NULL, NULL, Error ) )
			appErrorf( LocalizeError("FailedBrowse"), TEXT("Entry"), *Error );
		Exchange( GLevel, GEntry );
	}

	// Create default URL.
	FURL DefaultURL;
	DefaultURL.LoadURLConfig( TEXT("DefaultPlayer"), TEXT("User") );

	// Enter initial world.
	TCHAR Parm[4096]=TEXT("");
	const TCHAR* Tmp = appCmdLine();
	if
	(	!ParseToken( Tmp, Parm, ARRAY_COUNT(Parm), 0 )
	||	(appStricmp(Parm,TEXT("SERVER"))==0 && !ParseToken( Tmp, Parm, ARRAY_COUNT(Parm), 0 ))
	||	Parm[0]=='-'
	// A global option (appIsGlobalOptionToken, e.g. CPUSPEED=3000) is not a map either.
	||	appIsGlobalOptionToken(Parm) )
		appStrcpy( Parm, *FURL::DefaultLocalMap );

	FString ReplayFile;
	if( Parse( appCmdLine(), TEXT("-REPLAY="), ReplayFile ) )
	{
		// Start replay if specified.
		if( Replay.Replay( *ReplayFile, *GLog ) )
			// Retrieve the URL from it.
			appStrcpy( Parm, *Replay.GetURLStr() );
	}
	else if( Parse( appCmdLine(), TEXT("-RECORD="), ReplayFile ) )
	{
		// Start recording, with this URL.
		Replay.Record( *ReplayFile, Parm, *GLog );
	}
	

	FURL URL( &DefaultURL, Parm, TRAVEL_Partial );
	if( !URL.Valid )
		appErrorf( LocalizeError("InvalidUrl"), Parm );
		
	UBOOL Success = Browse( URL, NULL, Error );

	// If waiting for a network connection, go into the starting level.
	if( !Success && Error==TEXT("") && appStricmp( Parm, *FURL::DefaultLocalMap )!=0 )
		Success = Browse( FURL(&DefaultURL,*FURL::DefaultLocalMap,TRAVEL_Partial), NULL, Error );

	// Handle failure.
	if( !Success )
#ifdef __EMSCRIPTEN__
	{
		// A bad startup map is not fatal on the web: appErrorf's throw would abort the module (catching is
		// off). LoadMap returns NULL for it, so fall back to the Entry level (the menu) already in GEntry.
		debugf( NAME_Warning, TEXT("Startup map '%s' failed to load (%s) — falling back to the menu"), Parm, *Error );
		if( !GLevel )
			GLevel = GEntry;
	}
#else
		appErrorf( LocalizeError("FailedBrowse"), Parm, *Error );
#endif
	

	// Open initial Viewport.
	if( Client )
	{
		// Init input.!!Temporary
		UInput::StaticInitInput();

		// Create viewport.
		UViewport* Viewport = GViewport = Client->NewViewport( NAME_None );

		// Create console.
		UClass* ConsoleClass = StaticLoadClass( UConsole::StaticClass(), NULL, TEXT("ini:Engine.Engine.Console"), NULL, LOAD_NoFail, NULL );
		UConsole::BoxMenuBook( ConsoleClass );
		Viewport->Console = ConstructObject<UConsole>( ConsoleClass );
		Viewport->Console->_Init( Viewport );

		// Spawn play actor.
		FString Error;
		if( !GLevel->SpawnPlayActor( Viewport, ROLE_SimulatedProxy, URL, Error ) )
			appErrorf( TEXT("%s"), *Error );
		Viewport->Input->Init( Viewport );
		Viewport->OpenWindow( 0, 0, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE );
		GLevel->DetailChange( Viewport->RenDev->HighDetailActors );
		InitAudio();
		if( Audio )
			Audio->SetViewport( Viewport );
	}
	debugf( NAME_Init, TEXT("Game engine initialized") );
	
	//***************************
	//HP2: load game command line parameter support
	FString LoadGameSlot;
	if( Parse( appCmdLine(), TEXT("-LOAD="), LoadGameSlot ) )
	{
		TCHAR Filename[512];
		GFileManager->MakeDirectory( *GSys->SavePath, 1 );
		GFileManager->MakeDirectory( *GSys->SaveSlotPath, 1 );
		appSprintf( Filename, TEXT("%s") PATH_SEPARATOR TEXT("Save%i.usa"), *GSys->SaveSlotPath, appAtoi(*LoadGameSlot) );
		
		// Continue only a save that exists: the frontend's default options pass -LOAD=0 on every start, and without a
		// save the boot level is played as a new game (a failing load would leave the level LEVACT_Loading).
		if( GFileManager->FileSize( Filename ) > 0 )
		{
			debugf( NAME_Log, TEXT("Loading game... filename: %s"), Filename );
			GLevel->GetLevelInfo()->eventServerTravel(Filename, 0);
		}
		else debugf( NAME_Log, TEXT("No saved game to continue (%s): starting a new game"), Filename );
	}
	//***************************

	unguard;
}

//
// Pre exit.
//
void UGameEngine::Exit()
{
	guard(UGameEngine::Exit);
	Super::Exit();

	// Exit net.
	if( GLevel->NetDriver )
	{
		delete GLevel->NetDriver;
		GLevel->NetDriver = NULL;
	}

	unguard;
}

//
// Game exit.
//
void UGameEngine::Destroy()
{
	guard(UGameEngine::Destroy);

	// Game exit.
	if( GPendingLevel )
		CancelPending();
	GLevel = NULL;
	debugf( NAME_Exit, TEXT("Game engine shut down") );

	Super::Destroy();
	unguard;
}

//
// Progress text.
//
void UGameEngine::SetProgress( const TCHAR* Str1, const TCHAR* Str2, FLOAT Seconds )
{
	guard(UGameEngine::SetProgress);
	// The viewport can have a NULL Actor (DissociateViewports during a failed travel/open): only message a live one.
	if( Client && Client->Viewports.Num() && Client->Viewports(0)->Actor )
	{
		APlayerPawn* Actor = Client->Viewports(0)->Actor;
		if( Seconds==-1.f )
		{
			// Upgrade message.
			Actor->eventShowUpgradeMenu();
		}
		Actor->ProgressMessage[0] = Str1;
		Actor->ProgressColor[0].R = 255;
		Actor->ProgressColor[0].G = 255;
		Actor->ProgressColor[0].B = 255;

		Actor->ProgressMessage[1] = Str2;
		Actor->ProgressColor[1].R = 255;
		Actor->ProgressColor[1].G = 255;
		Actor->ProgressColor[1].B = 255;

		Actor->ProgressTimeOut    = Actor->Level->TimeSeconds + Seconds;
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	Command line executor.
-----------------------------------------------------------------------------*/

// WHEREAMI / TELEPORT <x> <y> <z> (UnCheats.cpp) and the frame rate in the log,
// [Engine.GameEngine] FpsLogSeconds / FPSLOG (UnFpsLog.cpp).
ENGINE_API UBOOL UfrontCheatExec( UEngine* Engine, const TCHAR* Cmd, FOutputDevice& Ar );
ENGINE_API void UfrontFpsLogTick( UEngine* Engine );
ENGINE_API UBOOL UfrontFpsLogExec( const TCHAR* Cmd, FOutputDevice& Ar );

//
// This always going to be the last exec handler in the chain. It
// handles passing the command to all other global handlers.
//
UBOOL UGameEngine::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(UGameEngine::Exec);
	const TCHAR* Str=Cmd;
	// First, because the web's ue_exec_command reaches this engine only (never the viewport); the typed
	// console arrives here too, after the viewport, the player and the scripts.
	if( UfrontCheatExec( this, Cmd, Ar ) )
		return 1;
	if( UfrontFpsLogExec( Cmd, Ar ) )
		return 1;
	if( ParseCommand( &Str, TEXT("OPEN") ) )
	{
		FString Error;
		if( Client && Client->Viewports.Num() )
			SetClientTravel( Client->Viewports(0), Str, 0, TRAVEL_Partial );
		else
		if( !Browse( FURL(&LastURL,Str,TRAVEL_Partial), NULL, Error ) && Error!=TEXT("") )
			Ar.Logf( TEXT("Open failed: %s"), *Error );
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("START") ) )
	{
		FString Error;
		if( Client && Client->Viewports.Num() )
			SetClientTravel( Client->Viewports(0), Str, 0, TRAVEL_Absolute );
		else
		if( !Browse( FURL(&LastURL,Str,TRAVEL_Absolute), NULL, Error ) && Error!=TEXT("") )
			Ar.Logf( TEXT("Start failed: %s"), *Error );
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("SERVERTRAVEL") ) && (GIsServer && !GIsClient) )
	{
		GLevel->GetLevelInfo()->eventServerTravel(Str,0);
		return 1;
	}
	else if( (GIsServer && !GIsClient) && ParseCommand( &Str, TEXT("SAY") ) )
	{
		GLevel->GetLevelInfo()->eventBroadcastMessage(Str,1,NAME_None);
		return 1;
	}
	else if( ParseCommand(&Str, TEXT("DISCONNECT")) )
	{
		FString Error;
		if( Client && Client->Viewports.Num() )
		{
			if( GLevel && GLevel->NetDriver && GLevel->NetDriver->ServerConnection && GLevel->NetDriver->ServerConnection->Channels[0] )
			{
				GLevel->NetDriver->ServerConnection->Channels[0]->Close();
				GLevel->NetDriver->ServerConnection->FlushNet();
			}
			if( GPendingLevel && GPendingLevel->NetDriver && GPendingLevel->NetDriver->ServerConnection && GPendingLevel->NetDriver->ServerConnection->Channels[0] )
			{
				GPendingLevel->NetDriver->ServerConnection->Channels[0]->Close();
				GPendingLevel->NetDriver->ServerConnection->FlushNet();
			}
			SetClientTravel( Client->Viewports(0), TEXT("?failed"), 0, TRAVEL_Absolute );
		}
		else
		if( !Browse( FURL(&LastURL,TEXT("?failed"),TRAVEL_Absolute), NULL, Error ) && Error!=TEXT("") )
			Ar.Logf( TEXT("Disconnect failed: %s"), *Error );
		return 1;
	}
	else if( ParseCommand(&Str, TEXT("RECONNECT")) )
	{
		FString Error;
		if( Client && Client->Viewports.Num() )
			SetClientTravel( Client->Viewports(0), *LastURL.String(), 0, TRAVEL_Absolute );
		else
		if( !Browse( FURL(LastURL), NULL, Error ) && Error!=TEXT("") )
			Ar.Logf( TEXT("Reconnect failed: %s"), *Error );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("EXIT")) || ParseCommand(&Cmd,TEXT("QUIT")))
	{
		if( GLevel && GLevel->NetDriver && GLevel->NetDriver->ServerConnection && GLevel->NetDriver->ServerConnection->Channels[0] )
		{
			GLevel->NetDriver->ServerConnection->Channels[0]->Close();
			GLevel->NetDriver->ServerConnection->FlushNet();
		}
		if( GPendingLevel && GPendingLevel->NetDriver && GPendingLevel->NetDriver->ServerConnection && GPendingLevel->NetDriver->ServerConnection->Channels[0] )
		{
			GPendingLevel->NetDriver->ServerConnection->Channels[0]->Close();
			GPendingLevel->NetDriver->ServerConnection->FlushNet();
		}
		Ar.Log( TEXT("Closing by request") );
		appRequestExit( 0 );
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("GETCURRENTTICKRATE") ) )
	{
		Ar.Logf( TEXT("%f"), CurrentTickRate );
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("GETMAXTICKRATE") ) )
	{
		Ar.Logf( TEXT("%f"), GetMaxTickRate() );
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("GSPYLITE") ) )
	{
		FString Error;
		appLaunchURL( TEXT("GSpyLite.exe"), TEXT(""), &Error );
		return 1;
	}
#if 1 // added by Legend on 4/12/2000
	else if( ParseCommand(&Str,TEXT("OBJCLEAN")) )
	{
		// quick hack to cleanup destroyed objects 
		if( GLevel )
			GLevel->CleanupDestroyed(1);
		return 1;
	}
#endif
	else if( ParseCommand(&Str,TEXT("SAVEGAME")) )
	{
		if( appIsDigit(Str[0]) )
		{
			GFileManager->MakeDirectory( *GSys->SavePath, 1 );
			GFileManager->MakeDirectory( *GSys->SaveSlotPath, 1 );
			
			
			//DEBUG
			TCHAR Filename[256];
			appSprintf( Filename, TEXT("%s") PATH_SEPARATOR TEXT("Save%i.usa"), *GSys->SaveSlotPath, appAtoi(Str) );
			debugf( NAME_Log, TEXT("Saving game...(before call) Filename: %s"), Filename );
			//
			SaveGame( appAtoi(Str) );
			
			//	The sounds are getting turned off during the save.  THis will restart them.
			//
			Exec(TEXT("RestartSounds"));
		}
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("LOADGAME")) )
	{
		if( appIsDigit(Str[0]) )
		{
			// Whenever we try to load a game we should first load our persistentActorCache
			LoadPersistentActorCache();
			
			TCHAR Filename[256];
			GFileManager->MakeDirectory( *GSys->SavePath, 1 );
			GFileManager->MakeDirectory( *GSys->SaveSlotPath, 1 );
			appSprintf( Filename, TEXT("%s") PATH_SEPARATOR TEXT("Save%i.usa"), *GSys->SaveSlotPath, appAtoi(Str) );
			
			debugf( NAME_Log, TEXT("Loading game... filename: %s"), Filename );
			GLevel->GetLevelInfo()->eventServerTravel(Filename,0);
		}
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("SAVEPACTORS")) )
	{
		if( !GLevel )
			return 0;
		
		// Make sure we have the save directory

		GFileManager->MakeDirectory( *GSys->SavePath, 1 );
		GFileManager->MakeDirectory( *GSys->SaveSlotPath, 1 );
		
		// ********* Persistent Actor  *****************
		
		// Make sure to get the LevelEnterText for the name of the level 
		// (not URL->Map because it can be Save0.usa when loading a map)
				
		// Save our persistent actors
		GLevel->SavePersistentActors( GLevel->GetLevelInfo()->LevelEnterText );
		return 1;
		// *********************************************
	}
	else if( ParseCommand( &Cmd, TEXT("CANCEL") ) )
	{
		static UBOOL InCancel = 0;
		if( !InCancel )	
		{
			//!!Hack for broken Input subsystem.  JP.
			//!!Inside LoadMap(), ResetInput() is called,
			//!!which can retrigger an Exec call.
			InCancel = 1;
			if( GPendingLevel )
			{
				if( GPendingLevel->TrySkipFile() )
				{
					InCancel = 0;
					return 1;
				}
				SetProgress( LocalizeProgress("CancelledConnect"), TEXT(""), 2.f );
			}
			else
				SetProgress( TEXT(""), TEXT(""), 0.f );
			CancelPending();
			InCancel = 0;
		}
		return 1;
	}
	else if( GLevel && GLevel->Exec( Cmd, Ar ) )
	{
		return 1;
	}
	else if( GLevel && GLevel->GetLevelInfo()->Game && GLevel->GetLevelInfo()->Game->ScriptConsoleExec(Cmd,Ar,NULL) )
	{
		return 1;
	}
	else
	{
		// disallow set of pawn property if network game
		INT AllowSet = 1;
		if ( GLevel && (GLevel->GetLevelInfo()->NetMode == NM_Client) )
		{
			const TCHAR *Str = Cmd;
			if ( ParseCommand(&Str,TEXT("SET")) )
			{
				TCHAR ClassName[256];
				UClass* Class;
				if
				(	ParseToken( Str, ClassName, ARRAY_COUNT(ClassName), 1 )
				&&	(Class=FindObject<UClass>( ANY_PACKAGE, ClassName))!=NULL )
				{
					if ( Class->IsChildOf(AActor::StaticClass()) 
						&& !Class->IsChildOf(AGameInfo::StaticClass()) )
						AllowSet = 0;
				}
			}
		}
		if( AllowSet && UEngine::Exec( Cmd, Ar ) )
		{
			return 1;
		}
		else
			return 0;
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	Serialization.
-----------------------------------------------------------------------------*/

//
// Serializer.
//
void UGameEngine::Serialize( FArchive& Ar )
{
	guard(UGameEngine::Serialize);
	Super::Serialize( Ar );

	Ar << GLevel << GEntry << GPendingLevel;

	unguardobj;
}

/*-----------------------------------------------------------------------------
	Game entering.
-----------------------------------------------------------------------------*/

//
// Cancel pending level.
//
void UGameEngine::CancelPending()
{
	guard(UGameEngine::CancelPending);
	if( GPendingLevel )
	{
		if( GPendingLevel->NetDriver && GPendingLevel->NetDriver->ServerConnection && GPendingLevel->NetDriver->ServerConnection->Channels[0] )
		{
			GPendingLevel->NetDriver->ServerConnection->Channels[0]->Close();
			GPendingLevel->NetDriver->ServerConnection->FlushNet();
		}
		delete GPendingLevel;
		GPendingLevel = NULL;
	}
	unguard;
}

//
// Match Viewports to actors.
//
static void MatchViewportsToActors( UClient* Client, ULevel* Level, const FURL& URL )
{
	guard(MatchViewportsToActors);
	for( INT i=0; i<Client->Viewports.Num(); i++ )
	{
		FString Error;
		UViewport* Viewport = Client->Viewports(i);
		debugf( NAME_Log, TEXT("Spawning new actor for Viewport %s"), Viewport->GetName() );
		if( !Level->SpawnPlayActor( Viewport, ROLE_SimulatedProxy, URL, Error ) )
			appErrorf( TEXT("%s"), *Error );
	}
	unguardf(( TEXT("(%s)"), *Level->URL.Map ));
}

//
// Browse to a specified URL, relative to the current one.
//
UBOOL UGameEngine::Browse( FURL URL, const TMap<FString,FString>* TravelInfo, FString& Error )
{
	guard(UGameEngine::Browse);
	Error = TEXT("");
	const TCHAR* Option;

	// Convert .unreal link files.
	const TCHAR* LinkStr = TEXT(".unreal");//!!
	if( appStrstr(*URL.Map,LinkStr)-*URL.Map==appStrlen(*URL.Map)-appStrlen(LinkStr) )
	{
		debugf( TEXT("Link: %s"), *URL.Map );
		FString NewUrlString;
		if( GConfig->GetString( TEXT("Link")/*!!*/, TEXT("Server"), NewUrlString, *URL.Map ) )
		{
			// Go to link.
			URL = FURL( NULL, *NewUrlString, TRAVEL_Absolute );//!!
		}
		else
		{
			// Invalid link.
			guard(InvalidLink);
			Error = FString::Printf( LocalizeError("InvalidLink"), *URL.Map );
			unguard;
			return 0;
		}
	}

	// Crack the URL.
	debugf( TEXT("Browse: %s"), *URL.String() );

	// Handle it.
	if( !URL.Valid )
	{
		// Unknown URL.
		guard(UnknownURL);
		Error = FString::Printf( LocalizeError("InvalidUrl"), *URL.String() );
		unguard;
		return 0;
	}
	else if( URL.HasOption(TEXT("failed")) || URL.HasOption(TEXT("entry")) )
	{
		// Handle failure URL.
		guard(FailedURL);
		debugf( NAME_Log, LocalizeError("AbortToEntry") );
		if( GLevel && GLevel!=GEntry )
		{
			if( GLevel->BrushTracker )
			{
				delete GLevel->BrushTracker;
				GLevel->BrushTracker = NULL;
			}
			ResetLoaders( GLevel->GetOuter(), 1, 0 );
		}
		NotifyLevelChange();
		GLevel = GEntry;
		GLevel->GetLevelInfo()->LevelAction = LEVACT_None;
		check(Client && Client->Viewports.Num());
		MatchViewportsToActors( Client, GLevel, URL );
		if( Audio )
			Audio->SetViewport( Audio->GetViewport() );
		//CollectGarbage( RF_Native ); // Causes texture corruption unless you flush.
		if( URL.HasOption(TEXT("failed")) )
		{
			if( !GPendingLevel )
				SetProgress( LocalizeError("ConnectionFailed"), TEXT(""), 6.f );
		}
		unguard;
		return 1;
	}
	else if( URL.HasOption(TEXT("pop")) )
	{
		// Pop the hub.
		guard(PopURL);
		if( GLevel && GLevel->GetLevelInfo()->HubStackLevel>0 )
		{
			TCHAR Filename[256], SavedPortal[256];
			appSprintf( Filename, TEXT("%s") PATH_SEPARATOR TEXT("Game%i.usa"), *GSys->SaveSlotPath, GLevel->GetLevelInfo()->HubStackLevel-1 );
			appStrcpy( SavedPortal, *URL.Portal );
			// The save path goes on Map directly: with '/' as PATH_SEPARATOR, URL text parsing would mangle it.
			// TRAVEL_Partial carries the base's options minus push/pop/peer/load.
			FURL PopURL( &URL, TEXT(""), TRAVEL_Partial );
			PopURL.Map    = Filename;
			PopURL.Portal = SavedPortal;
			PopURL.Valid  = 1;
			URL = PopURL;
		}
		else return 0;
		unguard;
	}
	else if( URL.HasOption(TEXT("restart")) )
	{
		// Handle restarting.
		guard(RestartURL);
		URL = LastURL;
		unguard;
	}
	else if( (Option=URL.GetOption(TEXT("load="),NULL))!=NULL )
	{
		// Handle loadgame.
		guard(LoadURL);
		FPushMemTag Push(TEXT("LoadGame"));
		FString Error;
		// The save path (e.g. "../Save/Slot1/Save9.usa") goes on Map directly, not through URL text parsing,
		// which reads a '/' PATH_SEPARATOR as host/portal syntax. The URL carries the base's options plus ?load.
		FURL LoadURL( &LastURL, TEXT("?load"), TRAVEL_Partial );
		LoadURL.Map = FString::Printf( TEXT("%s") PATH_SEPARATOR TEXT("Save%i.usa"), *GSys->SaveSlotPath, appAtoi(Option) );
		if( LoadMap(LoadURL,NULL,NULL,Error) )
		{
			// Copy the hub stack.
			INT i;
			for( i=0; i<GLevel->GetLevelInfo()->HubStackLevel; i++ )
			{
				TCHAR Src[256], Dest[256];//!!
				appSprintf( Src, TEXT("%s") PATH_SEPARATOR TEXT("Save%i%i.usa"), *GSys->SaveSlotPath, appAtoi(Option), i );
				appSprintf( Dest, TEXT("%s") PATH_SEPARATOR TEXT("Game%i.usa"), *GSys->SaveSlotPath, i );
				GFileManager->Copy( Src, Dest );
			}
			while( 1 )
			{
				FString Temp = FString::Printf( TEXT("%s") PATH_SEPARATOR TEXT("Game%i.usa"), *GSys->SaveSlotPath, i++ );
				if( GFileManager->FileSize(*Temp)<=0 )
					break;
				GFileManager->Delete( *Temp );
			}
			LastURL = GLevel->URL;
			return 1;
		}
		else return 0;
		unguard;
	}

	// Handle normal URL's.
	if( URL.IsLocalInternal() )
	{
		// Local map file.
		guard(LocalMapURL);
		return LoadMap( URL, NULL, TravelInfo, Error )!=NULL;
		unguard;
	}
	else if( URL.IsInternal() && GIsClient )
	{
		// Network URL.
		guard(NetworkURL);
		if( GPendingLevel )
			CancelPending();
		GPendingLevel = new UNetPendingLevel( this, URL );
		if( !GPendingLevel->NetDriver )
		{
			SetProgress( TEXT("Networking Failed"), *GPendingLevel->Error, 6.f );
			delete GPendingLevel;
			GPendingLevel = NULL;
		}
		return 0;
		unguard;
	}
	else if( URL.IsInternal() )
	{
		// Invalid.
		guard(InvalidURL);
		Error = LocalizeError("ServerOpen");
		unguard;
		return 0;
	}
	else
	{
		// External URL.
		guard(ExternalURL);
		appLaunchURL( *URL.String(), TEXT(""), &Error );
		unguard;
		return 0;
	}
	unguard;
}

//
// Notify that level is changing
//
void UGameEngine::NotifyLevelChange()
{
	guard(UGameEngine::NotifyLevelChange);
	if( Client && Client->Viewports.Num() && Client->Viewports(0)->Console )
		Client->Viewports(0)->Console->eventNotifyLevelChange();
	unguard;	
}

// Fixup a map
// hack to post release fix map actor problems without breaking compatibility
void UGameEngine::FixUpLevel( )
{
	if ( appStricmp(GLevel->GetFullName(), TEXT("Level CTF-Coret.MyLevel"))==0 )
	{
		debugf(TEXT("Fixing up CTF-Coret"));

		ANavigationPoint *Nav = GLevel->GetLevelInfo()->NavigationPointList;
		while (Nav)
		{
			if ( appStricmp(Nav->GetName(), TEXT("AlternatePath12"))==0
				|| appStricmp(Nav->GetName(), TEXT("AlternatePath13"))==0 )
			{
				debugf(NAME_Log, TEXT("Fixed up %s"),Nav->GetName());
				Nav->bTwoWay = 1;
			}
			else if ( appStricmp(Nav->GetName(), TEXT("PlayerStart5"))==0 )
			{
				APlayerStart *PS = Cast<APlayerStart>(Nav);
				if ( PS )
				{
					PS->bEnabled = false;
					debugf(NAME_Log, TEXT("Fixed up %s"),Nav->GetName());
				}
			}

			Nav = Nav->nextNavigationPoint;
		}
	}
	debugf(TEXT("Level is %s"), GLevel->GetFullName());

}
//
// Load a map.
//
#ifdef __EMSCRIPTEN__
// What LoadMap's catch( const TCHAR* ) does after a failed load, for the web build's early exits (exception catching
// is off there): without it the level stays LEVACT_Loading and the console paints its loading screen forever.
static void ufrontRestoreAfterFailedLoad( UClient* Client, ULevel* Level )
{
	if( Client && Client->Viewports.Num()>0 && Level && Level->GetLevelInfo() )
	{
		UViewport* Viewport=Client->Viewports(0);
		Level->GetLevelInfo()->LevelAction = LEVACT_None;
		if( Viewport->Actor )
		{
			Viewport->Actor->FlashFog.W = 1.f;
			Viewport->Actor->ConstantGlowFog.W = 0.f;
			Viewport->Actor->FadeRate = 0.f;
		}
	}
}
#endif

ULevel* UGameEngine::LoadMap( const FURL& URL, UPendingLevel* Pending, const TMap<FString,FString>* TravelInfo, FString& Error )
{
	guard(UGameEngine::LoadMap);
	FPushMemTag Push(TEXT("LoadMap"));
	Error = TEXT("");

	if( GLevel )
		GMalloc->DumpAllocs();
	debugf( NAME_Log, TEXT("LoadMap: %s"), *URL.String() );
	FTime LoadTime[2] = { appSeconds(), appProcessSeconds() };
	GInitRunaway();

	// Remember current level's stack level.
	INT SavedHubStackLevel = GLevel ? GLevel->GetLevelInfo()->HubStackLevel : 0;

	// Display loading screen.
	guard(LoadingScreen);
	if( Client && Client->Viewports.Num() && GLevel )
	{
		// The retail order: hide the menu, fade out, then flag the old level LEVACT_Loading and draw one more frame, so
		// the console paints its loading screen.
		UViewport* Viewport=Client->Viewports(0);
		APlayerPawn* PP = Viewport->Actor;
		if( PP )
			PP->bShowMenu = 0;

		if( Viewport )
		{
			// Fade out if needed.
			if( Viewport->Console->FadeoutTime > 0.f && Viewport->Console->DrewWorld() )
			{
				FTime Prev = appSeconds();
				while( Viewport->Actor->FlashFog.W > 0.f )
				{
					Draw( Viewport );
					FTime Cur = appSeconds();
					Viewport->Actor->FlashFog.W -= (Cur-Prev) / Viewport->Console->FadeoutTime;
					Prev = Cur;
				}
			}

			Viewport->Actor->FlashFog.W = 0.f;
			Draw( Viewport );
		}

		GLevel->GetLevelInfo()->Pauser = TEXT("");
		GLevel->GetLevelInfo()->LevelAction = LEVACT_Loading;
		Draw( Viewport );

		if( Audio )
			Audio->SetViewport( Audio->GetViewport() );
	}
	unguard;

	// Get network package map.
	UPackageMap* PackageMap = NULL;
	if( Pending )
		PackageMap = Pending->GetDriver()->ServerConnection->PackageMap;

	// Verify that we can load all packages we need.
	UObject* MapParent = NULL;
	guard(VerifyPackages);
	try
	{
		BeginLoad();
		if( Pending )
		{
			// Verify that we can load everything needed for client in this network level.
			INT i;
			for( i=0; i<PackageMap->List.Num(); i++ )
				PackageMap->List(i).Linker = GetPackageLinker
				(
					PackageMap->List(i).Parent,
					NULL,
					LOAD_Verify | LOAD_Throw | LOAD_NoWarn | LOAD_NoVerify,
					NULL,
					&PackageMap->List(i).Guid
				);
#ifdef __EMSCRIPTEN__
			// With catching off, GetPackageLinker returns NULL for a package that is missing or in another version
			// than the server's. Fail the join here with the reason the native throw carries, before VerifyLinker
			// dereferences it and before the live level is torn down.
			for( i=0; i<PackageMap->List.Num(); i++ )
				if( !PackageMap->List(i).Linker )
				{
					const TCHAR* Name = PackageMap->List(i).Parent->GetName();
					TCHAR Found[256];
					const UBOOL HaveFile = appFindPackageFile( Name, &PackageMap->List(i).Guid, Found );
					Error = FString::Printf( HaveFile ? LocalizeError("PackageVersion") : LocalizeError("PackageNotFound"), Name );
					EndLoad();
					SetProgress( LocalizeError(TEXT("UrlFailed"),TEXT("Core")), *Error, 6.0 );
					ufrontRestoreAfterFailedLoad( Client, GLevel );
					return NULL;
				}
#endif
			for( i=0; i<PackageMap->List.Num(); i++ )
				VerifyLinker( PackageMap->List(i).Linker );
			if( PackageMap->List.Num() )
				MapParent = PackageMap->List(0).Parent;
		}
		ULevel* Verified = LoadObject<ULevel>( MapParent, TEXT("MyLevel"), *URL.Map, LOAD_Verify | LOAD_Throw | LOAD_NoWarn, NULL );
		EndLoad();
#ifdef __EMSCRIPTEN__
		// With catching off, a failed verify of a missing or bad map returns NULL here (the catch below never
		// runs). Bail before DissociateViewports/ExitLevel tear down the live level and player actor, or an
		// `open <missing>` leaves Client->Viewports(0)->Actor dangling for the next Tick.
		if( !Verified )
		{
			Error = FString(TEXT("Could not load map ")) + URL.Map;
			SetProgress( LocalizeError(TEXT("UrlFailed"),TEXT("Core")), *URL.Map, 6.0 );
			ufrontRestoreAfterFailedLoad( Client, GLevel );
			return NULL;
		}
#else
		(void)Verified;
#endif

#if HPDEMO
		if( !Pending || !Pending->DemoRecDriver )
		{
			FString FileName(FString(TEXT("../Maps/"))+URL.Map);
			if( FileName.Right(4).Caps() != TEXT(".UNR"))
				FileName = FileName + TEXT(".unr");
			INT FileSize = GFileManager->FileSize( *FileName );
			debugf(TEXT("Looking for file: %s %d"), *FileName, FileSize);
			if( //FileSize != 0 &&
				( FileName.Caps() != TEXT("../MAPS/DEMO_PC.UNR")	) &&
				( FileName.Caps() != TEXT("../MAPS/ENTRY.UNR") )&&
				( FileName.Caps() != TEXT("../MAPS/STARTUP.UNR") )
				)
			{
				Error = TEXT("Sorry, only the retail version of UT can load third party maps.");
				SetProgress( LocalizeError(TEXT("UrlFailed"),TEXT("Core")), *Error, 6.f );
				return NULL;
			}
		}
#endif


#if DEMOVERSION
		// If we area demo, prevent third party maps from being loaded.
		if( !Pending || !Pending->DemoRecDriver )
		{
			FString FileName(FString(TEXT("../Maps/"))+URL.Map);
			if( FileName.Right(4).Caps() != TEXT(".UNR"))
				FileName = FileName + TEXT(".unr");
			INT FileSize = GFileManager->FileSize( *FileName );
			debugf(TEXT("Looking for file: %s %d"), *FileName, FileSize);
			if( //FileSize != 0 &&
				( FileName.Caps() != TEXT("../MAPS/DM-TURBINEDEMO.UNR")	|| FileSize != 2135105 ) &&
				( FileName.Caps() != TEXT("../MAPS/DM-PHOBOSDEMO.UNR")	|| FileSize != 1618994 ) &&
				( FileName.Caps() != TEXT("../MAPS/DM-MORPHEUSDEMO.UNR")|| FileSize != 1193759 ) &&
				( FileName.Caps() != TEXT("../MAPS/DM-TEMPESTDEMO.UNR")	|| FileSize != 2152238 ) &&
				( FileName.Caps() != TEXT("../MAPS/CTF-CORETDEMO.UNR")	|| FileSize != 3498978 ) &&
				( FileName.Caps() != TEXT("../MAPS/DOM-SESMARDEMO.UNR")	|| FileSize != 2155658 ) &&
				( FileName.Caps() != TEXT("../MAPS/ENTRY.UNR")			|| FileSize != 34822 ) &&
				( FileName.Caps() != TEXT("../MAPS/UT-LOGO-MAP.UNR")	|| FileSize != 34884 ) )
			{
				Error = TEXT("Sorry, only the retail version of UT can load third party maps.");
				SetProgress( LocalizeError(TEXT("UrlFailed"),TEXT("Core")), *Error, 6.f );
				return NULL;
			}
		}
#endif
	}
	// const on every compiler: the linker throws both appThrowf's TCHAR* and `throw( LocalizeError(…) )`'s
	// const TCHAR*, and under GCC/clang a non-const handler lets the const ones escape past EndLoad().
	catch( const TCHAR* CatchError )
	{
		// Safely failed loading.
		EndLoad();
		Error = CatchError;
		SetProgress( LocalizeError(TEXT("UrlFailed"),TEXT("Core")), CatchError, 6.f );

		if( Client && Client->Viewports.Num()>0 )
		{
			// Restore visibility.
			UViewport* Viewport=Client->Viewports(0);
			GLevel->GetLevelInfo()->LevelAction = LEVACT_None;
			Viewport->Actor->FlashFog.W = 1.f;
			Viewport->Actor->ConstantGlowFog.W = 0.f;
			Viewport->Actor->FadeRate = 0.f;
		}
		return NULL;
	}
	unguard;

	// Notify of the level change, before we dissociate Viewport actors
	guard(NotifyLevelChange);
	if( GLevel )
		NotifyLevelChange();
	unguard;

	// Dissociate Viewport actors.
	guard(DissociateViewports);
	if( Client )
	{
		for( INT i=0; i<Client->Viewports.Num(); i++ )
		{
			APlayerPawn* Actor          = Client->Viewports(i)->Actor;
			ULevel*      Level          = Actor->GetLevel();
			Actor->Player               = NULL;
			Client->Viewports(i)->Actor = NULL;
			Level->DestroyActor( Actor );
		}
	}
	unguard;

	// Clean up game state.
	guard(ExitLevel);
	if( GLevel )
	{
		// Shut down.
		ResetLoaders( GLevel->GetOuter(), 0, 1 );
		if( GLevel->BrushTracker )
		{
			delete GLevel->BrushTracker;
			GLevel->BrushTracker = NULL;
		}
		if( GLevel->NetDriver )
		{
			delete GLevel->NetDriver;
			GLevel->NetDriver = NULL;
		}
		if( GLevel->DemoRecDriver )
		{
			delete GLevel->DemoRecDriver;
			GLevel->DemoRecDriver = NULL;
		}
		if( URL.HasOption(TEXT("push")) )
		{
			// Save the current level minus players actors.
			GLevel->CleanupDestroyed( 1 );
			TCHAR Filename[256];
			appSprintf( Filename, TEXT("%s") PATH_SEPARATOR TEXT("Game%i.usa"), *GSys->SaveSlotPath, SavedHubStackLevel );
			SavePackage( GLevel->GetOuter(), GLevel, 0, Filename, GLog );
		}
		GLevel = NULL;
	}
	unguard;

	// Load the level and all objects under it, using the proper Guid.
	ALevelInfo* Info = LoadObject<ALevelInfo>( MapParent, TEXT("LevelInfo0"), *URL.Map, LOAD_NoFail, NULL );
	
	if( Info )
	{
		// Save the original map name in LevelInfo, so that it's retrievable from save games.
		if( Info->LevelEnterText == TEXT("") )
			Info->LevelEnterText = URL.Map;
		// No "draw level info" frame here, as in the retail Engine: it would present a cleared frame over the loading
		// screen drawn above.
	}

	guard(LoadLevel);
	GLevel = LoadObject<ULevel>( MapParent, TEXT("MyLevel"), *URL.Map, LOAD_NoFail, NULL );
	unguard;

#ifdef __EMSCRIPTEN__
	// With catching off the load returns NULL for a missing package or map, even under LOAD_NoFail. Return
	// NULL (UGameEngine::Init then falls back to the Entry menu) instead of failing check(GLevel) below.
	if( !GLevel )
	{
		Error = FString(TEXT("Could not load map ")) + URL.Map;
		SetProgress( LocalizeError(TEXT("UrlFailed"),TEXT("Core")), *URL.Map, 6.0 );
		return NULL;
	}
#endif
	
	// If pending network level.
	if( Pending )
	{
		// If playing this network level alone, ditch the pending level.
		if( Pending && Pending->LonePlayer )
			Pending = NULL;
		
		// Setup network package info.
		PackageMap->Compute();
		for( INT i=0; i<PackageMap->List.Num(); i++ )
			if( PackageMap->List(i).LocalGeneration!=PackageMap->List(i).RemoteGeneration )
				Pending->GetDriver()->ServerConnection->Logf( TEXT("HAVE GUID=%s GEN=%i"), PackageMap->List(i).Guid.String(), PackageMap->List(i).LocalGeneration );
	}

	// Verify classes.
	guard(VerifyClasses);
	VERIFY_CLASS_OFFSET( A, Actor,       Owner         );
	VERIFY_CLASS_OFFSET( A, Actor,       TimerCounter  );
	VERIFY_CLASS_OFFSET( A, PlayerPawn,  Player        );
	VERIFY_CLASS_OFFSET( A, PlayerPawn,  MaxStepHeight );
	unguard;

	// Get LevelInfo.
	check(GLevel);
	Info = GLevel->GetLevelInfo();
	Info->ComputerName = appComputerName();

	// Handle pushing.
	guard(ProcessHubStack);
	Info->HubStackLevel
	=	URL.HasOption(TEXT("load")) ? Info->HubStackLevel
	:	URL.HasOption(TEXT("push")) ? SavedHubStackLevel+1
	:	URL.HasOption(TEXT("pop" )) ? Max(SavedHubStackLevel-1,0)
	:	URL.HasOption(TEXT("peer")) ? SavedHubStackLevel
	:	                              0;
	unguard;

	// Handle pending level.
	guard(ActivatePending);
	if( Pending )
	{
		check(Pending==GPendingLevel);

		// Hook network driver up to level.
		GLevel->NetDriver = Pending->NetDriver;
		if( GLevel->NetDriver )
			GLevel->NetDriver->Notify = GLevel;

		// Hook demo playback driver to level
		GLevel->DemoRecDriver = Pending->DemoRecDriver;
		if( GLevel->DemoRecDriver )
			GLevel->DemoRecDriver->Notify = GLevel;

		// Setup level.
		GLevel->GetLevelInfo()->NetMode = NM_Client;
	}
	else check(!GLevel->NetDriver);
	unguard;

	// ********* Persistent Actor Support ***********
	if( appStricmp(GLevel->GetOuter()->GetName(),TEXT("Entry"))!=0 )
	{
		// load the persistent actors
		guard(LoadPersistentActors);
		GLevel->LoadPersistentActors( URL.Map );
		unguard;
	}
	// *********************************************

	// Set level info.
	guard(InitLevel);
	if( !URL.GetOption(TEXT("load"),NULL) )
		GLevel->URL = URL;
	Info->EngineVersion = FString::Printf( TEXT("%i"), ENGINE_VERSION );
	Info->MinNetVersion = FString::Printf( TEXT("%i"), ENGINE_MIN_NET_VERSION );
	GLevel->Engine = this;
	if( TravelInfo )
		GLevel->TravelInfo = *TravelInfo;
	unguard;

	// Purge unused objects and flush caches.
	guard(Cleanup);
	if( appStricmp(GLevel->GetOuter()->GetName(),TEXT("Entry"))!=0 )
	{
		Flush(0);
		{for( TObjectIterator<AActor> It; It; ++It )
			if( It->IsIn(GLevel->GetOuter()) )
				It->SetFlags( RF_EliminateObject );}
		{for( INT i=0; i<GLevel->Actors.Num(); i++ )
			if( GLevel->Actors(i) )
				GLevel->Actors(i)->ClearFlags( RF_EliminateObject );}
		CollectGarbage( RF_Native );
	}
	unguard;

	// Tell the audio driver to clean up.
	if( Audio )
		Audio->CleanUp();

	// Init collision.
	GLevel->SetActorCollision( 1 );

	// Setup zone distance table for sound damping. Fast enough: Approx 3 msec.
	guard(SetupZoneTable);
	QWORD OldConvConn[64];
	QWORD ConvConn[64];
	INT i, j;
	for( i=0; i<64; i++ )
	{
		for( j=0; j<64; j++ )
		{
			OldConvConn[i] = GLevel->Model->Zones[i].Connectivity;
			if( i == j )
				GLevel->ZoneDist[i][j] = 0;
			else
				GLevel->ZoneDist[i][j] = 255;
		}
	}
	for( i=1; i<64; i++ )
	{
		for( INT j=0; j<64; j++ )
			for( INT k=0; k<64; k++ )
				if( (GLevel->ZoneDist[j][k] > i) && ((OldConvConn[j] & ((QWORD)1 << k)) != 0) )
					GLevel->ZoneDist[j][k] = i;
		for( j=0; j<64; j++ )
			ConvConn[j] = 0;
		for( j=0; j<64; j++ )
			for( INT k=0; k<64; k++ )
				if( (OldConvConn[j] & ((QWORD)1 << k)) != 0 )
					ConvConn[j] = ConvConn[j] | OldConvConn[k];
		for( j=0; j<64; j++ )
			OldConvConn[j] = ConvConn[j];
	}
	unguard;

	// Update the LevelInfo's time.
	GLevel->UpdateTime(Info);

	// Init the game info.
	TCHAR Options[1024]=TEXT("");
	TCHAR GameClassName[256]=TEXT("");
	FString Error=TEXT("");
	guard(InitGameInfo);
	for( INT i=0; i<URL.Op.Num(); i++ )
	{
		appStrcat( Options, TEXT("?") );
		appStrcat( Options, *URL.Op(i) );
		Parse( *URL.Op(i), TEXT("GAME="), GameClassName, ARRAY_COUNT(GameClassName) );
	}
	if( GLevel->IsServer() && !Info->Game )
	{
		// Get the GameInfo class.
		UClass* GameClass=NULL;
		if( !GameClassName[0] )
		{
			GameClass=Info->DefaultGameType;
			if( !GameClass )
				GameClass = StaticLoadClass( AGameInfo::StaticClass(), NULL, Client ? TEXT("ini:Engine.Engine.DefaultGame") : TEXT("ini:Engine.Engine.DefaultServerGame"), NULL, LOAD_NoFail, PackageMap );
		}
		else GameClass = StaticLoadClass( AGameInfo::StaticClass(), NULL, GameClassName, NULL, LOAD_NoFail, PackageMap );

		// Spawn the GameInfo.
		debugf( NAME_Log, TEXT("Game class is '%s'"), GameClass->GetName() );
		Info->Game = (AGameInfo*)GLevel->SpawnActor( GameClass );
		check(Info->Game!=NULL);
	}
	unguard;

	// Listen for clients.
	guard(Listen);
	if( !Client || URL.HasOption(TEXT("Listen")) )
	{
		if( GPendingLevel )
		{
			guard(CancelPendingForListen);
			check(!Pending);
			delete GPendingLevel;
			GPendingLevel = NULL;
			unguard;
		}
		FString Error;
		if( !GLevel->Listen( Error ) )
			appErrorf( LocalizeError("ServerListen"), *Error );
	}
	unguard;

	// Init detail.
	Info->bHighDetailMode = 1;
	if
	(	Client
	&&	Client->Viewports.Num()
	&&	Client->Viewports(0)->RenDev
	&&	!Client->Viewports(0)->RenDev->HighDetailActors )
		Info->bHighDetailMode = 0;

	// Init level gameplay info.
	guard(BeginPlay);
	GLevel->iFirstDynamicActor = 0;
	if( !Info->bBegunPlay )
	{
		// fix up level problems
		FixUpLevel();

		// Lock the level.
		debugf( NAME_Log, TEXT("Bringing %s up for play (%i)..."), GLevel->GetFullName(), appRound(GetMaxTickRate()) );
		GLevel->TimeSeconds = 0.f;
		GLevel->GetLevelInfo()->TimeSeconds = 0;

		// Init touching actors.
		for( INT i=0; i<GLevel->Actors.Num(); i++ )
			if( GLevel->Actors(i) )
				for( INT j=0; j<ARRAY_COUNT(GLevel->Actors(i)->Touching); j++ )
					GLevel->Actors(i)->Touching[j] = NULL;

		// Kill off actors that aren't interesting to the client.
		INT i;
		if( !GLevel->IsServer() )
		{
			for( i=0; i<GLevel->Actors.Num(); i++ )
			{
				AActor* Actor = GLevel->Actors(i);
				if( Actor )
				{
					if( Actor->bStatic || Actor->bNoDelete )
						Exchange( Actor->Role, Actor->RemoteRole );
					else
						GLevel->DestroyActor( Actor );
				}
			}
		}
		
		// Init scripting.
		for( i=0; i<GLevel->Actors.Num(); i++ )
			if( GLevel->Actors(i) )
				GLevel->Actors(i)->InitExecution();

		// Enable actor script calls.
		Info->bBegunPlay = 1;
		Info->bStartup = 1;
		
		// Init the game.
		if( Info->Game )
			Info->Game->eventInitGame( Options, Error );

		// Send PreBeginPlay.
		for( i=0; i<GLevel->Actors.Num(); i++ )
			if( GLevel->Actors(i) && !GLevel->Actors(i)->bScriptInitialized )
				GLevel->Actors(i)->eventPreBeginPlay();

		// Set BeginPlay.
		for( i=0; i<GLevel->Actors.Num(); i++ )
			if( GLevel->Actors(i) && !GLevel->Actors(i)->bScriptInitialized )
				GLevel->Actors(i)->eventBeginPlay();

		// Set zones.
		for( i=0; i<GLevel->Actors.Num(); i++ )
			if( GLevel->Actors(i) && !GLevel->Actors(i)->bScriptInitialized )
				GLevel->SetActorZone( GLevel->Actors(i), 1, 1 );

		// Post begin play.
		for( i=0; i<GLevel->Actors.Num(); i++ )
			if( GLevel->Actors(i) && !GLevel->Actors(i)->bScriptInitialized )
				GLevel->Actors(i)->eventPostBeginPlay();

		// Begin scripting.
		for( i=0; i<GLevel->Actors.Num(); i++ )
			if( GLevel->Actors(i) && !GLevel->Actors(i)->bScriptInitialized )
				GLevel->Actors(i)->eventSetInitialState();

		// Find bases
		for( i=0; i<GLevel->Actors.Num(); i++ )
		{
			if( GLevel->Actors(i) ) 
			{
				if ( GLevel->Actors(i)->AttachTag != NAME_None )
				{
					//find actor to attach self onto
					for( INT j=0; j<GLevel->Actors.Num(); j++ )
					{
						if( GLevel->Actors(j) && (GLevel->Actors(j)->Tag == GLevel->Actors(i)->AttachTag) )
						{
							GLevel->Actors(i)->SetBase(GLevel->Actors(j), 0);
							break;
						}
					}
				}
				else if( !GLevel->Actors(i)->Base && GLevel->Actors(i)->bCollideWorld 
				 && (GLevel->Actors(i)->IsA(ADecoration::StaticClass()) || GLevel->Actors(i)->IsA(AInventory::StaticClass()) || GLevel->Actors(i)->IsA(APawn::StaticClass())) 
				 &&	((GLevel->Actors(i)->Physics == PHYS_None) || (GLevel->Actors(i)->Physics == PHYS_Rotating)) )
				{
					 GLevel->Actors(i)->FindBase();
					 if ( GLevel->Actors(i)->Base == Info )
						 GLevel->Actors(i)->SetBase(NULL, 0);
				}
			}
		}
		Info->bStartup = 0;
	}
	else GLevel->TimeSeconds = GLevel->GetLevelInfo()->TimeSeconds;
	unguard;

	// Rearrange actors: static first, then others.
	guard(Rearrange);
	TArray<AActor*> Actors;
	Actors.AddItem(GLevel->Actors(0));
	Actors.AddItem(GLevel->Actors(1));
	INT i;
	for( i=2; i<GLevel->Actors.Num(); i++ )
		if( GLevel->Actors(i) && GLevel->Actors(i)->bStatic && !GLevel->Actors(i)->bAlwaysRelevant )
			Actors.AddItem( GLevel->Actors(i) );
	GLevel->iFirstNetRelevantActor=Actors.Num();
	for( i=2; i<GLevel->Actors.Num(); i++ )
		if( GLevel->Actors(i) && GLevel->Actors(i)->bStatic && GLevel->Actors(i)->bAlwaysRelevant )
			Actors.AddItem( GLevel->Actors(i) );
	GLevel->iFirstDynamicActor=Actors.Num();
	for( i=2; i<GLevel->Actors.Num(); i++ )
		if( GLevel->Actors(i) && !GLevel->Actors(i)->bStatic )
			Actors.AddItem( GLevel->Actors(i) );
	GLevel->Actors.Empty();
	GLevel->Actors.Add( Actors.Num() );
	for( i=0; i<Actors.Num(); i++ )
		GLevel->Actors(i) = Actors(i);
	unguard;
	
	
	
	// Cleanup profiling.
#if DO_GUARD_SLOW
	guard(CleanupProfiling);
	for( TObjectIterator<UFunction> It; It; ++It )
		It->Calls = It->Cycles=0;
	GTicks=1;
	unguard;
#endif


	// Client init.
	guard(ClientInit);
	if( Client )
	{
		// Match Viewports to actors.
		MatchViewportsToActors( Client, GLevel->IsServer() ? GLevel : GEntry, URL );

		// Init brush tracker.
		if( appStricmp(GLevel->GetOuter()->GetName(),TEXT("Entry"))!=0 )//!!
			GLevel->BrushTracker = GNewBrushTracker( GLevel );

		// Set up audio.
		if( Audio )
			Audio->SetViewport( Audio->GetViewport() );

		// Reset viewports.
		for( INT i=0; i<Client->Viewports.Num(); i++ )
		{
			UViewport* Viewport = Client->Viewports(i);
			Viewport->Input->ResetInput();
			if( Viewport->RenDev )
				Viewport->RenDev->Flush(1);
		}
	}
	unguard;
	


	// ********** GameState Support ****************
	if( appStricmp(GLevel->GetOuter()->GetName(),TEXT("Entry"))!=0 )
	{
		// Check gameState compared to the actor list
		// This needs to be done *AFTER* the Client init 
		// so that the gamestate has a chance to travel.
		guard(ScreenActorsByGameState);
		GLevel->ScreenActorsByGameState();
		unguard;
	}
	// *********************************************

	// Init detail.
	GLevel->DetailChange( Info->bHighDetailMode );
	
	
	
	// Remember the URL.
	guard(RememberURL);
	LastURL = URL;
	unguard;

	// Remember DefaultPlayer options.
	if( GIsClient )
	{
		URL.SaveURLConfig( TEXT("DefaultPlayer"), TEXT("Name" ), TEXT("User") );
		URL.SaveURLConfig( TEXT("DefaultPlayer"), TEXT("Team" ), TEXT("User") );
		URL.SaveURLConfig( TEXT("DefaultPlayer"), TEXT("Class"), TEXT("User") );
		URL.SaveURLConfig( TEXT("DefaultPlayer"), TEXT("Skin" ), TEXT("User") );
		URL.SaveURLConfig( TEXT("DefaultPlayer"), TEXT("Face" ), TEXT("User") );
		URL.SaveURLConfig( TEXT("DefaultPlayer"), TEXT("Voice" ), TEXT("User") );
		URL.SaveURLConfig( TEXT("DefaultPlayer"), TEXT("OverrideClass" ), TEXT("User") );
	}
	
	debugf( TEXT("Load time %s: %f seconds total, %f app"), 
		*URL.Map, appSeconds()-LoadTime[0], appProcessSeconds()-LoadTime[1] );

	// Successfully started local level.
	return GLevel;
	unguard;
}

/*-----------------------------------------------------------------------------
	Game Viewport functions.
-----------------------------------------------------------------------------*/

//
// Draw a global view.
//
//
// Scoped AutoFOV override. Everything that consumes the FOV during a frame reads Viewport->Actor->FovAngle:
// the clip planes (FSceneNode::ComputeRenderSize) and each render device. Overriding it for the draw keeps
// culling and projection in agreement with no per-driver change; the destructor restores it on every exit.
// Script run inside the draw (HUD PostRender, DrawActor) sees the corrected angle it is projected with;
// script run in the tick sees the real one (hprop's fly-to-HUD target, computed at pickup, so on a wide
// window the flying prop ends a little nearer the centre than its HUD icon).
//
struct FScopedAutoFov
{
	APlayerPawn* Actor;
	FLOAT        Saved;
	FScopedAutoFov( UViewport* Viewport )
	:	Actor ( Viewport->Actor )
	,	Saved ( Viewport->Actor->FovAngle )
	{
		Actor->FovAngle = ufrontEffectiveFov( Saved, Viewport->SizeX, Viewport->SizeY, 1 );
	}
	~FScopedAutoFov()
	{
		Actor->FovAngle = Saved;
	}
};

void UGameEngine::Draw( UViewport* Viewport, UBOOL Blit, BYTE* HitData, INT* HitSize )
{
	guard(UGameEngine::Draw);

	// If not up and running yet, don't draw.
	if( !GIsRunning || GIsDrawing )
		return;
	GIsDrawing = 1;
	FPushMemTag Push(TEXT("UGameEngine::Draw"));
	UpdateConnectingMessage();

	// Get view location.
	AActor*      ViewActor    = Viewport->Actor;
	FVector      ViewLocation = ViewActor->Location;
	FRotator     ViewRotation = ViewActor->Rotation;
	Viewport->Actor->eventPlayerCalcView( ViewActor, ViewLocation, ViewRotation );
	check(ViewActor);

	// AutoFOV: armed after script has calculated the view (PlayerCalcView still sees the real FOV) and
	// before Viewport->Lock, because a render device may latch the FOV during Lock.
	FScopedAutoFov AutoFovScope( Viewport );

	if( Viewport->RenDev->PrecacheOnFlip && !Viewport->bSuspendPrecaching && ViewActor->Level->LevelAction == LEVACT_None )
	{
		Viewport->RenDev->PrecacheOnFlip = 0;
		if ( !ViewActor->Level->bNeverPrecache )
			Render->Precache( Viewport );
	}

	// See if viewer is inside world.
	DWORD LockFlags=0;
	FCheckResult Hit;
	if( !GLevel->Model->PointCheck(Hit,NULL,ViewLocation,FVector(0,0,0),0) )
		LockFlags |= LOCKR_ClearScreen;

#if defined(LEGEND)
	if( Viewport->Actor->IsA( APlayerPawn::StaticClass() ) )
	{
		// call the PlayerPawn Render Control Interface (RCI) to assess clear-screen operations
		if( Viewport->Actor->ClearScreen() )
		{
			LockFlags |= LOCKR_ClearScreen;
		}

		// call the PlayerPawn Render Control Interface (RCI) to assess lighting recomputation
		//
		// WARNING: RecomputeLighting() should *not* return false regularly, or rendering 
		//          performance will be severly compromised
		if( Viewport->Actor->RecomputeLighting() )
		{
			guard(RecomputeLighting);
			Flush();
			unguard;
		}
	}
#endif

	// Lock the Viewport.
	check(Render);
	FPlane FlashScale = FPlane( Client->ScreenFlashes ? Clamp( 0.5f*Viewport->Actor->FlashFog.W, 0.f, 1.f ) : 0.5f );
	FPlane FlashFog   = Client->ScreenFlashes ? Viewport->Actor->FlashFog : FVector(0,0,0);
	FlashFog.X   = Clamp( FlashFog.X  , 0.f, 1.f );
	FlashFog.Y   = Clamp( FlashFog.Y  , 0.f, 1.f );
	FlashFog.Z   = Clamp( FlashFog.Z  , 0.f, 1.f );
	FlashFog.W   = 0.f;
	if( Viewport->Lock(FlashScale,FlashFog,FPlane(0,0,0,0),LockFlags,HitData,HitSize) )
	{
		// Setup rendering coords.
		FSceneNode* Frame = Render->CreateMasterFrame( Viewport, ViewLocation, ViewRotation, NULL );

		// Update level audio.
		if( Audio )
		{
			clock(GLevel->AudioTickCycles);
			Audio->Update( Frame );
			unclock(GLevel->AudioTickCycles);
		}

		// Render.
		Render->PreRender( Frame );
		Viewport->Canvas->Render = Render;
		if( Viewport->Console )
			Viewport->Console->PreRender( Frame );
		Viewport->Canvas->Update( Frame );
		Viewport->Actor->eventPreRender( Viewport->Canvas );
		if( Frame->X>0 && Frame->Y>0 && (!Viewport->Console || Viewport->Console->GetDrawWorld()) )
			Render->DrawWorld( Frame );
		Viewport->RenDev->EndFlash();
		Viewport->Actor->eventPostRender( Viewport->Canvas );
		if( Viewport->Console )
		{
			Viewport->Console->PostRender( Frame );
			Viewport->Console->eventPostRender( Viewport->Canvas );
		}
		if( Replay.Replaying() || Replay.Recording() )
		{
			// Show replay message.
			UCanvas* Canvas = Frame->Viewport->Canvas;
			Canvas->Color = FColor(255,255,255);
			Canvas->CurX=0;
			Canvas->CurY=Canvas->ClipY-40;
			Canvas->WrappedPrintf( Canvas->SmallFont, 0, TEXT("%s F=%d T=%.3f"), 
				Replay.Recording() ? TEXT("Recording") : TEXT("Replaying"),
				Replay.GetFrameNum(), GLevel->TimeSeconds.GetFloat() );
		}
		if( Audio )
			Audio->PostRender( Frame );

#if 0
/* BEGIN BETA VERSION */
		if(GLevel && GLevel->GetLevelInfo() && GLevel->GetLevelInfo()->Game && FString(GLevel->GetLevelInfo()->Game->GetClass()->GetName()) == FString(TEXT("UTIntro")))
		{
			if ( ((AGameInfo*) AGameInfo::StaticClass()->GetDefaultObject())->DemoBuild == 0 )
			{
				// "BETA VERSION" XOR'd with BetaDecoder
				static TCHAR BetaCypher[] = { 67, 4, 50, 41, 108, 125, 82, 27, 46, 55, 121, 25 };
				static TCHAR BetaDecoder[] = { 1, 65, 102, 104, 76, 43, 23, 73, 125, 126, 54, 87, 33, 78, 0 };
				static TCHAR BetaDecoded[] = TEXT("            "); // gets replaced with "BETA VERSION"

				for(INT i=0; BetaDecoded[i]; i++)
						BetaDecoded[i] = BetaCypher[i] ^ BetaDecoder[i];
			
				Frame->Viewport->Canvas->Color = FColor(255,255,255);
				Frame->Viewport->Canvas->CurX=0;
				Frame->Viewport->Canvas->CurY=0;
				Frame->Viewport->Canvas->WrappedPrintf( Frame->Viewport->Canvas->SmallFont, 0, BetaDecoded );
				Frame->Viewport->Canvas->CurX=Frame->Viewport->Canvas->ClipX - 72;
				Frame->Viewport->Canvas->CurY=0;
				Frame->Viewport->Canvas->WrappedPrintf( Frame->Viewport->Canvas->SmallFont, 0, BetaDecoded );
				Frame->Viewport->Canvas->CurX=0;
				Frame->Viewport->Canvas->CurY=Frame->Viewport->Canvas->ClipY - 10;
				Frame->Viewport->Canvas->WrappedPrintf( Frame->Viewport->Canvas->SmallFont, 0, BetaDecoded );
				Frame->Viewport->Canvas->CurX=Frame->Viewport->Canvas->ClipX - 72;
				Frame->Viewport->Canvas->CurY=Frame->Viewport->Canvas->ClipY - 10;
				Frame->Viewport->Canvas->WrappedPrintf( Frame->Viewport->Canvas->SmallFont, 0, BetaDecoded );
			}
		}
/* END BETA VERSION */
#endif

		Viewport->Canvas->Render = 0;
		Render->PostRender( Frame );
		Viewport->Unlock( Blit );
		Render->FinishMasterFrame();
	}

	GIsDrawing = 0;
	unguard;
}

void ExportTravel( FOutputDevice& Out, AActor* Actor )
{
	guard(ExportTravel);
	debugf( TEXT("Exporting travelling actor of class %s"), Actor->GetClass()->GetPathName() );//!!xyzzy
	check(Actor);
	if( !Actor->bTravel )
		return;
	Out.Logf( TEXT("Class=%s Name=%s\r\n{\r\n"), Actor->GetClass()->GetPathName(), Actor->GetName() );
	for( TFieldIterator<UProperty> It(Actor->GetClass()); It; ++It )
	{
		for( INT Index=0; Index<It->ArrayDim; Index++ )
		{
			TCHAR Value[1024];
			if
			(	(It->PropertyFlags & CPF_Travel)
			&&	It->ExportText( Index, Value, (BYTE*)Actor, &Actor->GetClass()->Defaults(0), 0 ) )
			{
				Out.Log( It->GetName() );
				if( It->ArrayDim!=1 )
					Out.Logf( TEXT("[%i]"), Index );
				Out.Log( TEXT("=") );
				UObjectProperty* Ref = Cast<UObjectProperty>( *It );
				if( Ref && Ref->PropertyClass->IsChildOf(AActor::StaticClass()) )
				{
					UObject* Obj = *(UObject**)( (BYTE*)Actor + It->Offset + Index*It->ElementSize );
					Out.Logf( TEXT("%s\r\n"), Obj ? Obj->GetName() : TEXT("None") );
				}
				Out.Logf( TEXT("%s\r\n"), Value );
			}
		}
	}
	Out.Logf( TEXT("}\r\n") );
	unguard;
}

//
// Jumping viewport.
//
void UGameEngine::SetClientTravel( UPlayer* Player, const TCHAR* NextURL, UBOOL bItems, ETravelType TravelType )
{
	guard(UGameEngine::SetClientTravel);
	check(Player);

	UViewport* Viewport    = CastChecked<UViewport>( Player );
	Viewport->TravelURL    = NextURL;
	Viewport->TravelType   = TravelType;
	Viewport->bTravelItems = bItems;

	unguard;
}

/*-----------------------------------------------------------------------------
	Tick.
-----------------------------------------------------------------------------*/

//
// Get tick rate limitor.
//
FLOAT UGameEngine::GetMaxTickRate()
{
	guard(UGameEngine::GetMaxTickRate);
	static UBOOL LanPlay = ParseParam(appCmdLine(),TEXT("lanplay"));
	if( GLevel && GLevel->NetDriver && !GIsClient )
		return Clamp( LanPlay ? GLevel->NetDriver->LanServerMaxTickRate : GLevel->NetDriver->NetServerMaxTickRate, 10, 120 );
	else if( GLevel && GLevel->NetDriver && GLevel->NetDriver->ServerConnection )
		return GLevel->NetDriver->ServerConnection->CurrentNetSpeed/64;
	else if( GLevel && GLevel->DemoRecDriver && !GLevel->DemoRecDriver->ServerConnection )
		return Clamp( LanPlay ? GLevel->NetDriver->LanServerMaxTickRate : GLevel->DemoRecDriver->NetServerMaxTickRate, 10, 120 );
	else
		return 0;
	unguard;
}

//
// Update everything.
//
void UGameEngine::Tick( FLOAT DeltaSeconds )
{
	guard(UGameEngine::Tick);
	UfrontFpsLogTick( this );	// FpsLogSeconds / FPSLOG; does nothing unless enabled

	// Read or write frame tick for replay.
	if( Replay.Paused() )
		return;
	Replay.SerializeFrameTick( DeltaSeconds );

	INT LocalTickCycles=0;
	clock(LocalTickCycles);

	// If all viewports closed, time to exit.
	if( Client && Client->Viewports.Num()==0 )
	{
		debugf( TEXT("All Windows Closed") );
		appRequestExit( 0 );
		return;
	}

#ifdef PLATFORM_SDL
	// Pointer capture: captured (SDL relative mode) while a live level is up and no menu shows, released
	// in a menu. NSDLViewport::SetMouseCapture sets bWindowsMouseAvailable = !Capture, so a menu cursor follows
	// the absolute WindowsMouseX/Y and gameplay gets relative deltas. Acts on state changes only.
	{
		static INT LastCapture = -1;
		static INT LastMenu = -1;
		UViewport* MouseViewport = ( Client && Client->Viewports.Num()==1 ) ? Client->Viewports(0) : NULL;
		INT Menu = ( MouseViewport && MouseViewport->Actor && ( MouseViewport->Actor->bShowMenu || MouseViewport->bShowWindowsMouse ) ) ? 1 : 0;
		INT Want = ( MouseViewport && GLevel && MouseViewport->Actor && !Menu ) ? 1 : 0;
		if( MouseViewport && ( Want != LastCapture || Menu != LastMenu ) )
		{
			if( GUfrontLog )
				debugf( TEXT("[mouse] relative-capture=%i menu=%i (bShowMenu=%i bShowWinMouse=%i)"),
					Want, Menu, MouseViewport->Actor ? (INT)MouseViewport->Actor->bShowMenu : -1, (INT)MouseViewport->bShowWindowsMouse );
			if( Want != LastCapture )
			{
				MouseViewport->SetMouseCapture( Want, Want, 0 );
				LastCapture = Want;
			}
			LastMenu = Menu;
		}
	}
#endif

	// Update subsystems.
	UObject::StaticTick();				
	GCache.Tick();

	// Update the level.
	guard(TickLevel);
	GameCycles=0;
	clock(GameCycles);
	if( GLevel )
		GLevel->Tick( LEVELTICK_All, DeltaSeconds );
	if( GEntry && GEntry!=GLevel )
		GEntry->Tick( LEVELTICK_All, DeltaSeconds );
	if( Client && Client->Viewports.Num() && Client->Viewports(0)->Actor->GetLevel()!=GLevel )
		Client->Viewports(0)->Actor->GetLevel()->Tick( LEVELTICK_All, DeltaSeconds );
	unclock(GameCycles);
	unguard;


	// HP2 specific - Handle player saving
	guard(PlayerSaving);
	// A dedicated server/ucc has no Client, and after DissociateViewports the viewport's Actor is NULL.
	APlayerPawn* Player = ( Client && Client->Viewports.Num() ) ? Client->Viewports(0)->Actor : NULL;
	if( Player && Player->bQueuedToSaveGame )
	{
		// We are queued to save the game...
	
		// Make sure we have a path to save to
		GFileManager->MakeDirectory( *GSys->SavePath, 1 );
		GFileManager->MakeDirectory( *GSys->SaveSlotPath, 1 );
		
		//DEBUG
		TCHAR Filename[256];
		appSprintf( Filename, TEXT("%s") PATH_SEPARATOR TEXT("Save%i.usa"), *GSys->SaveSlotPath, 0 );
		debugf( NAME_Log, TEXT("Saving game...(before call) Filename: %s"), Filename );
		
		// Make sure we set this to false BEFORE we save the game.
		Player->bQueuedToSaveGame = false;
		
		// *** SAVE THE GAME
		SaveGame( 0 );
		
		//	The sounds are getting turned off during the save.  THis will restart them.
		//
		Exec(TEXT("RestartSounds"));
		
		// Flush our engine caches ( this fixes some graphical glitches on some vid cards )
		Flush(1);

		return;
	}
	unguard;


	// Handle server travelling.
	guard(ServerTravel);
	if( GLevel && GLevel->GetLevelInfo()->NextURL!=TEXT("") )
	{
		if( (GLevel->GetLevelInfo()->NextSwitchCountdown-=DeltaSeconds) <= 0.f )
		{
			// Travel to new level, and exit.
			TMap<FString,FString> TravelInfo;
			if( GLevel->GetLevelInfo()->NextURL==TEXT("?RESTART") )
			{
				TravelInfo = GLevel->TravelInfo;
			}
			else if( GLevel->GetLevelInfo()->bNextItems )
			{
				TravelInfo = GLevel->TravelInfo;
				for( INT i=0; i<GLevel->Actors.Num(); i++ )
				{
					APlayerPawn* P = Cast<APlayerPawn>( GLevel->Actors(i) );
					if( P && P->Player )
					{
						// Export items and self.
						FStringOutputDevice PlayerTravelInfo;
						ExportTravel( PlayerTravelInfo, P );
						for( AActor* Inv=P->Inventory; Inv; Inv=Inv->Inventory )
							ExportTravel( PlayerTravelInfo, Inv );
						TravelInfo.Set( *P->PlayerReplicationInfo->PlayerName, *PlayerTravelInfo );

						// Prevent local ClientTravel from taking place, since it will happen automatically.
						if( Cast<UViewport>( P->Player ) )
							Cast<UViewport>( P->Player )->TravelURL = TEXT("");
					}
				}
			}
			debugf( TEXT("Server switch level: %s"), *GLevel->GetLevelInfo()->NextURL );
			FString Error;
			Browse( FURL(&LastURL,*GLevel->GetLevelInfo()->NextURL,TRAVEL_Relative), &TravelInfo, Error );
			GLevel->GetLevelInfo()->NextURL = TEXT("");
			return;
		}
	}
	unguard;

	// Handle client travelling.
	guard(ClientTravel);
	if( Client && Client->Viewports.Num() && Client->Viewports(0)->TravelURL!=TEXT("") )
	{
		// Travel to new level, and exit.
		UViewport* Viewport = Client->Viewports( 0 );
		TMap<FString,FString> TravelInfo;

		// Export items.
		if( appStricmp(*Viewport->TravelURL,TEXT("?RESTART"))==0 )
		{
			TravelInfo = GLevel->TravelInfo;
		}
		else if( Viewport->bTravelItems )
		{
			debugf( TEXT("Export travel for: %s"), *Viewport->Actor->PlayerReplicationInfo->PlayerName );
			FStringOutputDevice PlayerTravelInfo;
			ExportTravel( PlayerTravelInfo, Viewport->Actor );
			for( AActor* Inv=Viewport->Actor->Inventory; Inv; Inv=Inv->Inventory )
				ExportTravel( PlayerTravelInfo, Inv );
			TravelInfo.Set( *Viewport->Actor->PlayerReplicationInfo->PlayerName, *PlayerTravelInfo );
		}
		FString Error;
		Browse( FURL(&LastURL,*Viewport->TravelURL,Viewport->TravelType), &TravelInfo, Error );
		Viewport->TravelURL=TEXT("");

		return;
	}
	unguard;

	// Update the pending level.
	guard(TickPending);
	if( GPendingLevel )
	{
		GPendingLevel->Tick( DeltaSeconds );
		if( GPendingLevel->Error!=TEXT("") )
		{
			// Pending connect failed.
			guard(PendingFailed);
			SetProgress( LocalizeError("ConnectionFailed"), *GPendingLevel->Error, 4.f );
			debugf( NAME_Log, LocalizeError("Pending"), *GPendingLevel->URL.String(), *GPendingLevel->Error );
			// Notify console.
			if( GPendingLevel->FailCode!=TEXT("") && Client && Client->Viewports(0) && Client->Viewports(0)->Console )
				Client->Viewports(0)->Console->eventConnectFailure( GPendingLevel->FailCode, GPendingLevel->FailURL );

			delete GPendingLevel;
			GPendingLevel = NULL;
			unguard;
		}
		else if( GPendingLevel->Success && !GPendingLevel->FilesNeeded && !GPendingLevel->SentJoin )
		{
			// Attempt to load the map.
			FString Error;
			guard(AttemptLoadPending);
			LoadMap( GPendingLevel->URL, GPendingLevel, NULL, Error );
			if( Error!=TEXT("") )
			{
				SetProgress( LocalizeError("ConnectionFailed"), *Error, 4.f );
			}
			else if( !GPendingLevel->LonePlayer )
			{
				// Show connecting message, cause precaching to occur.
				GLevel->GetLevelInfo()->LevelAction = LEVACT_Connecting;
				GEntry->GetLevelInfo()->LevelAction = LEVACT_Connecting;
				if( Client )
					Client->Tick();

				// Send join.
				GPendingLevel->SendJoin();
				GPendingLevel->NetDriver = NULL;
				GPendingLevel->DemoRecDriver = NULL;
			}
			unguard;

			// Kill the pending level.
			guard(KillPending);
			delete GPendingLevel;
			GPendingLevel = NULL;
			unguard;
		}
	}
	unguard;

	// Render everything.
	guard(ClientTick);
	INT LocalClientCycles=0;
	if( Client )
	{
		clock(LocalClientCycles);
		UViewport* Viewport=Client->Viewports(0);
		Viewport->Actor->eventViewFlash( DeltaSeconds );
		Client->Tick();
		unclock(LocalClientCycles);
	}
	ClientCycles=LocalClientCycles;
	unguard;

	unclock(LocalTickCycles);
	TickCycles=LocalTickCycles;

	// Replay input here.
	if( Replay.Replaying() )
	{
		FReplay::FInputEvent IE;
		while( Replay.SerializeFrameInput( IE ) )
		{
			if( IE.State == IST_MAX )
			{
				// Special input functions.
				if( IE.iKey == IK_MouseX )
					GViewport->WindowsMouseX = IE.Delta;
				else if( IE.iKey == IK_MouseY )
					GViewport->WindowsMouseY = IE.Delta;
				else
					Key( GViewport, IE.iKey );
			}
			else
				InputEvent( GViewport, IE.iKey, IE.State, IE.Delta );
		}
	}

	GTicks++;
	unguard;
}

/*-----------------------------------------------------------------------------
	Saving the game.
-----------------------------------------------------------------------------*/

//
// Save the current game state to a file.
//
void UGameEngine::SaveGame( INT Position )
{
	guard(UGameEngine::SaveGame);

	TCHAR Filename[256];
	// Tree=1: SaveSlotPath (../Save/SlotN) nests one level deeper than a single mkdir creates, and a missing
	// directory makes the save throw "Error opening file". Every SavePath/SaveSlotPath MakeDirectory here passes 1.
	GFileManager->MakeDirectory( *GSys->SavePath, 1 );
	GFileManager->MakeDirectory( *GSys->SaveSlotPath, 1 );
	appSprintf( Filename, TEXT("%s") PATH_SEPARATOR TEXT("Save%i.usa"), *GSys->SaveSlotPath, Position );
	
	debugf( NAME_Log, TEXT("Saving game... filename: %s"), Filename );

	GLevel->GetLevelInfo()->LevelAction=LEVACT_Saving;

	// Every actor prepares for the save (Actor.PreSaveGame) before anything is written: harry clears
	// PreviousLevelName, resets SloMo and copies the status managers into himself; Characters drop their vendors.
	for( INT iActor=0; iActor<GLevel->Actors.Num(); iActor++ )
		if( GLevel->Actors(iActor) )
			GLevel->Actors(iActor)->eventPreSaveGame();

	// No progress paint while the player's own loading screen is up.
	if( Client && Client->Viewports.Num() && Client->Viewports(0)->Actor
	&&	!Client->Viewports(0)->Actor->bShowLoadingScreen )
		PaintProgress();
	GWarn->BeginSlowTask( LocalizeProgress("Saving"), 1, 0 );
	if( GLevel->BrushTracker )
	{
		delete GLevel->BrushTracker;
		GLevel->BrushTracker = NULL;
	}
	GLevel->CleanupDestroyed( 1 );
	if( SavePackage( GLevel->GetOuter(), GLevel, 0, Filename, GLog ) )
	{
		// Copy the hub stack.
		INT i;
		for( i=0; i<GLevel->GetLevelInfo()->HubStackLevel; i++ )
		{
			TCHAR Src[256], Dest[256];
			appSprintf( Src, TEXT("%s") PATH_SEPARATOR TEXT("Game%i.usa"), *GSys->SaveSlotPath, i );
			appSprintf( Dest, TEXT("%s") PATH_SEPARATOR TEXT("Save%i%i.usa"), *GSys->SaveSlotPath, Position, i );
			GFileManager->Copy( Src, Dest );
		}
		while( 1 )
		{
			appSprintf( Filename, TEXT("%s") PATH_SEPARATOR TEXT("Save%i%i.usa"), *GSys->SaveSlotPath, Position, i++ );
			if( GFileManager->FileSize(Filename)<=0 )
				break;
			GFileManager->Delete( Filename );
		}
	}
	for( INT i=0; i<GLevel->Actors.Num(); i++ )
		if( Cast<AMover>(GLevel->Actors(i)) )
			Cast<AMover>(GLevel->Actors(i))->SavedPos = FVector(-1,-1,-1);
	GLevel->BrushTracker = GNewBrushTracker( GLevel );
	GWarn->EndSlowTask();
	GLevel->GetLevelInfo()->LevelAction=LEVACT_None;
	GCache.Flush();

	// ****
	// Now that we are done saving our game, save all of our pa files from cache into our SaveGamePath
	SavePersistentActorCache();
	// ****

	// Give the save slot its picture, then tell every actor the save is over (Actor.PostSaveGame; harry clears
	// bShowLoadingScreen), whether SavePackage succeeded or not.
	SetThumbnailForSaveSlot();
	for( INT iActor=0; iActor<GLevel->Actors.Num(); iActor++ )
		if( GLevel->Actors(iActor) )
			GLevel->Actors(iActor)->eventPostSaveGame();

	unguard;
}

//
// Copy the level's pre-made thumbnail into the save slot as Save0.bmp (the retail PlayerPawn.SaveGame takes no
// screenshot): <appBaseDir>/Thumbnails/<Level>.bmp, else Thumbnails/Default.bmp, where <Level> is LevelEnterText (the
// map name LoadMap stores) without directory and extension. False when there is no slot, no name or no thumbnail.
//
bool UGameEngine::SetThumbnailForSaveSlot()
{
	guard(UGameEngine::SetThumbnailForSaveSlot);
	if( *GSys->SaveSlotPath && *GLevel->GetLevelInfo()->LevelEnterText )
	{
		FString LevelName = GLevel->GetLevelInfo()->LevelEnterText;
		LevelName = LevelName.Mid( LevelName.InStr( TEXT("\\"), 1 ) + 1 );
		if( LevelName.InStr( TEXT(".") ) >= 0 )
			LevelName = LevelName.Left( LevelName.InStr( TEXT(".") ) );

		FString Thumbnail = FString(appBaseDir()) * TEXT("Thumbnails") * LevelName + TEXT(".bmp");
		if( GFileManager->FileSize( *Thumbnail ) <= 0 )
		{
			Thumbnail = FString(appBaseDir()) * TEXT("Thumbnails") * TEXT("Default.bmp");
			if( GFileManager->FileSize( *Thumbnail ) <= 0 )
				return false;
		}
		GFileManager->Copy( *(GSys->SaveSlotPath * TEXT("Save0.bmp")), *Thumbnail, 1, 1 );
		return true;
	}
	return false;
	unguard;
}

/*-----------------------------------------------------------------------------
	Input overrides, incorporating replays and mouse feedback.
-----------------------------------------------------------------------------*/

//
// Mouse delta while dragging.
//
void UGameEngine::MouseDelta( UViewport* Viewport, DWORD ClickFlags, FLOAT DX, FLOAT DY )
{
	// No replay functionality needed.
	guard(UGameEngine::MouseDelta);
	if
	(	(ClickFlags & MOUSE_FirstHit)
	&&	Client
	&&	Client->Viewports.Num()==1
	&&	GLevel
	&&	!Client->Viewports(0)->IsFullscreen()
	&&	GLevel->GetLevelInfo()->Pauser==TEXT("")
	&&	!Viewport->Actor->bShowMenu
	&&  !Viewport->bShowWindowsMouse )
	{
		Viewport->SetMouseCapture( 1, 1, 1 );
	}
	else if( (ClickFlags & MOUSE_LastRelease) && !Client->CaptureMouse )
	{
		Viewport->SetMouseCapture( 0, 0, 0 );
	}
	unguard;
}

//
// Absolute mouse position.
//
void UGameEngine::MousePosition( UViewport* Viewport, DWORD ClickFlags, FLOAT X, FLOAT Y )
{
	guard(UGameEngine::MousePosition);

	if( Viewport )
	{
		Viewport->WindowsMouseX = X;
		Viewport->WindowsMouseY = Y;
		if( Replay.Recording() )
		{
			Replay.SerializeFrameInput( IK_MouseX, IST_MAX, X );
			Replay.SerializeFrameInput( IK_MouseY, IST_MAX, Y );
		}
	}

	unguard;
}

//
// Mouse clicking.
//
void UGameEngine::Click( UViewport* Viewport, DWORD ClickFlags, FLOAT X, FLOAT Y )
{
	// No replay functionality needed.
	guard(UGameEngine::Click);
	unguard;
}

UBOOL UGameEngine::Key( UViewport* Viewport, EInputKey Key )
{
	// Intercept, and record handled input keys.
	if( UEngine::Key( Viewport, Key ) )
	{
		if( Replay.Recording() )
			Replay.SerializeFrameInput( Key, IST_MAX );
		return true;
	}
	return false;
}

UBOOL UGameEngine::InputEvent( UViewport* Viewport, EInputKey iKey, EInputAction State, FLOAT Delta )
{
	// Nasty nasty hack for special pause.
	// Only in debug mode: [HGame.baseConsole] bDebugMode=True in the system ini (the launcher writes it from -DEBUGMODE).
	if (State == IST_Press && iKey == IK_Delete)
	{
		FString DebugMode = GConfig->GetStr( TEXT("HGame.baseConsole"), TEXT("bDebugMode") );
		if( DebugMode == TEXT("True") )
			GLevel->bInSpecialPauseMode = !GLevel->bInSpecialPauseMode;
	}



	if( Replay.Replaying() && !Replay.InReplay() )
	{
		// Ignore real input, except for replay control keys.
		if( State == IST_Press && iKey == IK_Escape )
			Replay.Stop();
		else if( State == IST_Press && iKey == IK_Pause )
			Replay.Pause( !Replay.Paused() );
		else if( State == IST_Press && iKey == IK_ScrollLock )
			if( Replay.Paused() )
				// Single step.
				Replay.Pause( true, 1 );
			
		return true;
	}
	else if( Replay.Recording() )
	{
		if( State == IST_Press && iKey == IK_Backslash )
		{
			Replay.Stop();
			return true;
		}
	}

	// Intercept, and record handled input keys.
	if( Replay.Recording() )
		Replay.SerializeFrameInput( iKey, State, Delta );
	return UEngine::InputEvent( Viewport, iKey, State, Delta );
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
