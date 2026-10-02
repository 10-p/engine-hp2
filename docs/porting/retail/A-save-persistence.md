# A — Saving, persistent actors, game state (Engine.dll, plus a Core.dll spill-over)

Analyst scope: proto `HarryPotter2/Assets/System/Engine.dll` vs retail `assets-retail/System/Engine.dll`
(Ghidra decompiles in `iso-check/ghidra/dec/*`, every load-bearing point re-checked with objdump; import thunks and
string literals resolved from the PE tables). Read-only; nothing in the repository was edited.

## 0. Function identification (export stub -> body, both builds)

| Our source | proto body | retail body | Verdict |
|---|---|---|---|
| `UGameEngine::SaveGame` (`Engine/Src/UnGame.cpp:2162`) | 103aa0a0 | 103ab430 | **REAL changes** (§1) |
| `UGameEngine::SetThumbnailForSaveSlot` (does not exist) | — | 103abb00 (stub 10301d7f, `?SetThumbnailForSaveSlot@UGameEngine@@QAE_NXZ` = public, non-virtual, returns `bool`) | **NEW** (§2) |
| `AActor::eventPreSaveGame / eventPostSaveGame` | — | 1031ae30 / 1031adf0 | already in our `EngineClasses.h:1126-1133` (committed in HEAD), bodies match retail (`ProcessEvent(FindFunctionChecked(ENGINE_xxx),NULL)`) — nothing to do |
| `ULevel::SavePersistentActors` (`Engine/Src/UnLevel.cpp:251`) | 103b72a0 | 103b9030 | **REAL change** (§3) |
| `ULevel::LoadPersistentActors` (`Engine/Src/UnLevel.cpp:410`) | 103b7fe0 | 103b9e40 | **REAL changes** (§4) |
| `ULevel::ScreenActorsByGameState` (`Engine/Src/UnLevel.cpp:118`) | 103b6b20 | 103b86d0 | **REAL changes** (§5) |
| `UGameEngine::Exec` SAVEGAME / LOADGAME / SAVEPACTORS branches (`UnGame.cpp:468-516`) | 103a0c30 | 103a20f0 | identical instruction-for-instruction (only absolute addresses differ) |
| `UGameEngine::Tick` "PlayerSaving" block (`UnGame.cpp:1951-1984`) | 103a8580 | 103a9910 | layout-only: `bQueuedToSaveGame` word 0x954 -> 0x958 (+4 from `bShowLoadingScreen`) |
| `UGameEngine::LoadMap` call sites of `LoadPersistentActors` / `ScreenActorsByGameState` (`UnGame.cpp:1210-1217`, `1514-1524`) | 103a3f70 | 103a5430 | unchanged (register allocation / EH-state numbers / assert line numbers only) |
| `AActor::SerializePersistence`, `execSaveGameSaveInfo`, `execLoadGameSaveInfo`, `execSaveGameExists` | 10380220, 10420440, 104206c0, 104189d0 | 103811d0, 104224e0, 10422760, 1041aa70 | byte-identical after address normalisation |
| Core.dll `SavePersistentActorCache` / `LoadPersistentActorCache` / `appPreExit` (`Core/Src/UnMisc.cpp:2158-2247`) | Core 10157780 / 10157b70 / 10157f90 | Core 10157790 / 10157e20 / 101580e0 | **REAL (small) changes** (§6) — outside Engine.dll, reported because it is the PA cache |

Sanity check: the proto bodies match our source line by line (same strings, same vtable slots: `PaintProgress` = UGameEngine
vtbl+0xc0, `ULevel::DestroyActor`/`CleanupDestroyed`/`SpawnActor` = vtbl+0x98/0x9c/0xa0, FFileManager `FileSize`/`Delete`/
`Copy`/`MakeDirectory` = +0x8/+0xc/+0x10/+0x20; UGameEngine `Client`=+0x54, `GLevel`=+0x78; UPlayer `Actor`=+0x30;
ALevelInfo `LevelEnterText`=0x458, `HubStackLevel`=0x4b4, `LevelAction`=0x4b8; APlayerPawn `bShowLoadingScreen` bit 0 of
0x92c = 2348).

---

## 1. `UGameEngine::SaveGame` — Pre/PostSaveGame, conditional PaintProgress, thumbnail (REAL)

Retail = proto + three insertions; the rest (log line, mkdirs, BeginSlowTask, brush tracker, `CleanupDestroyed(1)`,
`SavePackage`, hub-stack copy loop, Mover `SavedPos` reset, `EndSlowTask`, `LevelAction=None`, `GCache.Flush()`,
`SavePersistentActorCache()`) is unchanged (the first `GetLevelInfo()` `IsA` is now an out-of-line `UObject::IsA` call —
codegen only).

1. **PreSaveGame** — right after `LevelAction=LEVACT_Saving`, before anything else:
   `for(i=0;i<GLevel->Actors.Num();i++) if(GLevel->Actors(i)) GLevel->Actors(i)->eventPreSaveGame();`
   (objdump 103ab56a-103ab59d: `FindFunctionChecked(ENGINE_PreSaveGame,0)` then vtbl+0x10 `ProcessEvent(f,NULL,NULL)`;
   `Actors.Num()` re-read every iteration; no `bDeleteMe`/class filter.)
2. **PaintProgress made conditional** (103ab59f-103ab5c7):
   `if( Client && Client->Viewports.Num() && Client->Viewports(0)->Actor && !Client->Viewports(0)->Actor->bShowLoadingScreen ) PaintProgress();`
3. **After** `SavePersistentActorCache()`: `SetThumbnailForSaveSlot();` (return value ignored), then **PostSaveGame** on
   every non-NULL `GLevel->Actors(i)` (same loop shape, `ENGINE_PostSaveGame`).

### Exactly who gets Pre/PostSaveGame, and when
* Recipients: **every non-NULL slot of `GLevel->Actors`** (LevelInfo, brushes, actors pending destroy — PreSaveGame runs
  *before* `CleanupDestroyed(1)` compacts the list, PostSaveGame *after*). Not GEntry. No filter on class/state.
  An actor spawned by a PreSaveGame handler is appended and also gets PreSaveGame.
* PreSaveGame: after `LevelAction=LEVACT_Saving`, before PaintProgress / BeginSlowTask / `SavePackage` — so script
  changes made there are written into `Save<N>.usa`.
* PostSaveGame: at the very end — after SavePackage (called whether SavePackage succeeded or not), hub copy,
  `LevelAction=LEVACT_None`, `GCache.Flush()`, `SavePersistentActorCache()` and `SetThumbnailForSaveSlot()`.
* Script overrides present in the retail packages (parsed from the export tables and bytecode of the retail `.u`):
  `Actor.PreSaveGame/PostSaveGame` (empty events, Engine.u); `harry.PreSaveGame` = `PreviousLevelName=""; SloMo(1.0);
  CopyAllStatusFromManagerToHarry();`; `harry.PostSaveGame` = `bShowLoadingScreen=false;`; `Characters.PreSaveGame` =
  `Super.PreSaveGame(); if(managerVendor!=None){ managerVendor.Destroy(); managerVendor=None; }`.
  (Retail `PlayerPawn.SaveGame()` lost its slot param, its `SloMo(1.0)` and the `Thumbnail(120,90,"Save"$N$".bmp")`
  screenshot — that work moved to `harry.PreSaveGame` and to the engine's `SetThumbnailForSaveSlot`.)

### Patch (confidence: high)
```diff
--- a/Source/Engine/Src/UnGame.cpp
+++ b/Source/Engine/Src/UnGame.cpp
@@ -2176,7 +2176,18 @@ void UGameEngine::SaveGame( INT Position )
 	debugf( NAME_Log, TEXT("Saving game... filename: %s"), Filename );
 
 	GLevel->GetLevelInfo()->LevelAction=LEVACT_Saving;
-	PaintProgress();
+
+	// Retail (HPCos 021009): every actor prepares for the save (Actor.PreSaveGame) before anything is written.
+	for( INT iActor=0; iActor<GLevel->Actors.Num(); iActor++ )
+		if( GLevel->Actors(iActor) )
+			GLevel->Actors(iActor)->eventPreSaveGame();
+
+	// Retail: no progress paint while the player's own loading screen is up.
+	if( Client && Client->Viewports.Num() && Client->Viewports(0)->Actor
+	&&	!Client->Viewports(0)->Actor->bShowLoadingScreen )
+		PaintProgress();
 	GWarn->BeginSlowTask( LocalizeProgress("Saving"), 1, 0 );
 	if( GLevel->BrushTracker )
 	{
@@ -2215,6 +2226,51 @@ void UGameEngine::SaveGame( INT Position )
 	// Now that we are done saving our game, save all of our pa files from cache into our SaveGamePath
 	SavePersistentActorCache();
 	// ****
 
+	// Retail: give the save slot its picture, then tell every actor the save is over (Actor.PostSaveGame).
+	SetThumbnailForSaveSlot();
+	for( INT iActor=0; iActor<GLevel->Actors.Num(); iActor++ )
+		if( GLevel->Actors(iActor) )
+			GLevel->Actors(iActor)->eventPostSaveGame();
+
 	unguard;
 }
+
+//
+// Retail (HPCos 021009): copy the level's pre-made thumbnail into the save slot as Save0.bmp, the picture the
+// save/load screens show (UnEngineWin.h SAVE_FILENAME_THUMBNAIL, FrontEnd DEFAULT_THUMBNAIL_FILENAME). The level
+// name is LevelInfo.LevelEnterText (HP2 stores the map name there, see LoadMap) without directory and extension:
+// <appBaseDir>/Thumbnails/<Level>.bmp, else <appBaseDir>/Thumbnails/Default.bmp. False only when neither exists.
+//
+bool UGameEngine::SetThumbnailForSaveSlot()
+{
+	// Retail tests the two string POINTERS (never NULL), not their contents; kept as shipped.
+	if( *GSys->SaveSlotPath && *GLevel->GetLevelInfo()->LevelEnterText )
+	{
+		FString LevelName = GLevel->GetLevelInfo()->LevelEnterText;
+		LevelName = LevelName.Mid( LevelName.InStr( TEXT("\\"), 1 ) + 1 );
+		if( LevelName.InStr( TEXT(".") ) >= 0 )
+			LevelName = LevelName.Left( LevelName.InStr( TEXT(".") ) );
+
+		FString Thumbnail = appBaseDir();
+		Thumbnail = Thumbnail * TEXT("Thumbnails") * LevelName + TEXT(".bmp");
+		if( GFileManager->FileSize( *Thumbnail ) <= 0 )
+		{
+			Thumbnail = appBaseDir();
+			Thumbnail = Thumbnail * TEXT("Thumbnails") * TEXT("Default.bmp");
+			if( GFileManager->FileSize( *Thumbnail ) <= 0 )
+				return false;
+		}
+		GFileManager->Copy( *(GSys->SaveSlotPath * TEXT("Save0.bmp")), *Thumbnail, 1, 1 );
+		return true;
+	}
+	return false;
+}
```
```diff
--- a/Source/Engine/Inc/UnGame.h
+++ b/Source/Engine/Inc/UnGame.h
@@ -65,4 +65,5 @@ class ENGINE_API UGameEngine : public UEngine
 	virtual void BuildServerMasterMap( UNetDriver* NetDriver, ULevel* InLevel );
 	virtual void NotifyLevelChange();
 	void FixUpLevel();
+	bool SetThumbnailForSaveSlot();	// retail HPCos 021009 (non-virtual: vtable unchanged)
 };
```
Notes: (a) `iActor` instead of `i` to avoid clashing with the existing `INT i` / `for(INT i…)` in that function.
(b) Reconstruction details from objdump 103abb00-103abfd7: no guard; reverse search for `"\\"` is the inlined
`InStr(…,Right=1)`, `Mid(pos+1)` (MAXINT count); the `"."` test is the inlined `InStr()` (`appStrstr`) `>= 0`, then the
out-of-line `InStr(".")` + `Left`; path = `FString(appBaseDir()) * "Thumbnails" * LevelName + ".bmp"`; `Copy(Dest=SaveSlotPath*"Save0.bmp",
Src=Thumbnail, Replace=1, EvenIfReadOnly=1, Attributes=0, Progress=NULL)`, result ignored; returns `al=1`/`al=0`.
(c) Port caveats: the 29 thumbnails ship in retail `System/Thumbnails/` (already in `assets-retail`, assemble-assets.sh
mentions them) and must be in the published asset set; our Linux/web file manager already maps `\`->`/` and folds case,
so `Adv1Willow.bmp` etc. resolve. If `LevelEnterText` can ever carry a `/` path in the port, retail's `"\\"`-only strip
would yield `Thumbnails/<dir>/<map>.bmp` -> falls back to Default.bmp (harmless; add a `PATH_SEPARATOR` strip only if
that is observed). Maps whose `LevelEnterText` has no matching file (e.g. `GreatHall_G`, `Sepia_Hallway` vs
`GreatHall.bmp`/`SepiaHallway.bmp`) get Default.bmp — shipped data, not to be patched.

---

## 2. `SetThumbnailForSaveSlot` — see §1 (patch included there). `retail-named2/103abb00.c` is the same body.

---

## 3. `ULevel::SavePersistentActors` — actor CLASS name added to every PA record (REAL)

Everything is identical (PA0 id, map name, map FileSize, FileTime, count, per-actor size-placeholder/SerializePersistence/
back-patched size, EOF token, cleanup) **except** that after the actor-name block retail writes the actor's class name
the same way (objdump 103b9762-103b97d1: `[edi+0x24]`=Class, `->Name` (+0x20) -> `appSprintf("%s")`, `(appStrlen+1)*sizeof(TCHAR)`,
size then string), reusing the actor-name error strings (the compiler pooled them: 0x104dd1ec / 0x104dd158).

New record layout: `INT nameBytes, TCHAR name[], INT classBytes, TCHAR className[], INT propBytes, props…`.

### Patch (confidence: high)
```diff
--- a/Source/Engine/Src/UnLevel.cpp
+++ b/Source/Engine/Src/UnLevel.cpp
@@ -350,6 +350,15 @@ bool ULevel::SavePersistentActors( const FString& MapName )
 			pActAr->Serialize(&buffer, iToSave );
 			IF_TRUE_MSG_AND_BREAK(pActAr->GetError(),TEXT("ULevelBase::SavePersistentActors ERROR: Saving name of actor!") ) 
 			
+			// Block 3.1i' ) Retail (HPCos 021009): then the size of the actor's class name and the class name, so
+			// LoadPersistentActors can respawn an actor that is not in the map (Case #3)
+			appSprintf( buffer, TEXT("%s"), pAct->GetClass()->GetName() );
+			iToSave = (appStrlen(buffer)+1)*sizeof(TCHAR);
+			pActAr->Serialize(&iToSave, sizeof(iToSave) );
+			IF_TRUE_MSG_AND_BREAK(pActAr->GetError(),TEXT("ULevelBase::SavePersistentActors ERROR: Saving size of name of actor!") ) 
+			
+			pActAr->Serialize(&buffer, iToSave );
+			IF_TRUE_MSG_AND_BREAK(pActAr->GetError(),TEXT("ULevelBase::SavePersistentActors ERROR: Saving name of actor!") ) 
 			
 			// Block 3.2i ) Save a DUMMY var for the size of the taggedProperties
 			iPosSize = pActAr->Tell();
```
Format compatibility: the PA identifier stays `"PA0"` in retail (no version bump), so **PA files our port already wrote
(proto layout) are unreadable by the new loader** — it would take the property-block size as the class-name length and
read that many bytes into a 256-TCHAR stack buffer (overflow). Retail PA files from Windows are not usable anyway
(port uses ANSI TCHAR). See the optional hardening in §4.

---

## 4. `ULevel::LoadPersistentActors` — reads the class name; Case #3 now SPAWNS the missing actor (REAL)

Relative to the prototype logic (our own per-actor resync and the FFileManagerLinux FName serialization are ours and
stay):

1. New local `TCHAR strClassName[256]` (frame `[ebp-0x4b0]`, 0x200 bytes).
2. In Block 3, after reading the actor name: read `INT` class-name size (error text "Loading size of actors name!") and
   the class name into `strClassName` ("Loading actors name!"), then the property size as before.
3. **Case #3** (name not in `ActorsToLoad`): log the same "*** Actor is NOT in level %s but is in pa file %s! \n. " line,
   then `UClass* C = (UClass*)UObject::StaticFindObject(NULL, ANY_PACKAGE, strClassName)` (class arg NULL, outer -1,
   ExactClass 0 — objdump 103ba6da-103ba6e7). If NULL: `Seek(Tell()+iPropSize); continue;` (old behaviour). Else
   `pAct = SpawnActor(C, FName(strName), NULL, NULL, FVector(0,0,0), FRotator(0,0,0), NULL, 1 /*bNoCollisionFail*/, 0)`
   (vtbl+0xa0); NULL -> new error "ULevelBase::LoadPersistentActors ERROR: Spawning the the actor!" + break; success ->
   **skips the name check** and joins the Case #1 path: `ActorsToLoad.Remove(strName)`, `SerializePersistence`,
   `bScriptInitialized=false`.
4. Case #1/#2, header checks, EOF check, destroy loop, cleanup: unchanged. (The retail test after the Find is literally
   `if( !pAct && strClassName )` — the array address, always true; omitted.) The prolog differs only by an inlined
   `FString::operator*`.

### Patch (confidence: high; keeps our resync, which now also covers spawned actors)
```diff
--- a/Source/Engine/Src/UnLevel.cpp
+++ b/Source/Engine/Src/UnLevel.cpp
@@ -418,6 +418,7 @@ bool ULevel::LoadPersistentActors( const FString& MapName )
 	SQWORD		FileTime;
 	int			iPActorsInFile, iToLoad, iPropSize;
 	TCHAR		strName[256];
+	TCHAR		strClassName[256];	// retail: every PA record also carries its actor's class
 	bool		bError = false;
 
 	TMultiMap<FName,AActor*> ActorsToLoad;
@@ -525,6 +526,13 @@ bool ULevel::LoadPersistentActors( const FString& MapName )
 			pActAr->Serialize(&strName, iToLoad );
 			IF_TRUE_MSG_AND_BREAK(pActAr->GetError(),TEXT("ULevelBase::LoadPersistentActors ERROR: Loading actors name!") ) 
 			
+			// Block 3.1i' ) Retail (HPCos 021009): then the size of the actor's class name and the class name
+			pActAr->Serialize(&iToLoad, sizeof(iToLoad));
+			IF_TRUE_MSG_AND_BREAK(pActAr->GetError(),TEXT("ULevelBase::LoadPersistentActors ERROR: Loading size of actors name!") ) 
+			
+			pActAr->Serialize(&strClassName, iToLoad );
+			IF_TRUE_MSG_AND_BREAK(pActAr->GetError(),TEXT("ULevelBase::LoadPersistentActors ERROR: Loading actors name!") ) 
 			
 			// Block 3.2i ) Get the size of this actor's taggedProperties ( because we may need to skip it )
 			pActAr->Serialize(&iPropSize, sizeof(iPropSize));
@@ -568,14 +576,25 @@ bool ULevel::LoadPersistentActors( const FString& MapName )
 				// Destroy our temp actor
 				TempActor.Destroy();
 */				
-				// Seek past the actor in the level_pa.usa file
-				pActAr->Seek( pActAr->Tell() + iPropSize );
-				continue;
+				// Retail: respawn it from the class the save recorded, under its saved name (e.g. beans a chest
+				// spawned), then load it like a Case #1 actor below. An unknown class is skipped as before.
+				UClass* pClass = (UClass*)StaticFindObject( NULL, ANY_PACKAGE, strClassName );
+				if( pClass == NULL )
+				{
+					// Seek past the actor in the level_pa.usa file
+					pActAr->Seek( pActAr->Tell() + iPropSize );
+					continue;
+				}
+				pAct = SpawnActor( pClass, FName(strName), NULL, NULL, FVector(0,0,0), FRotator(0,0,0), NULL, true );
+				IF_TRUE_MSG_AND_BREAK( pAct == NULL, TEXT("ULevelBase::LoadPersistentActors ERROR: Spawning the the actor!") )
 			}
-			
-			// Check to make sure our names match
-			IF_TRUE_MSG_AND_BREAK( appStrcmp(pAct->GetName(),strName) != 0, 
-				TEXT("Level actor's name != persistent actor's name!!!!!") ) 
+			else
+			{
+				// Check to make sure our names match
+				IF_TRUE_MSG_AND_BREAK( appStrcmp(pAct->GetName(),strName) != 0, 
+					TEXT("Level actor's name != persistent actor's name!!!!!") ) 
+			}
 		
 			// Remove this actor from the ActorsToLoad map
 			ActorsToLoad.Remove( strName );
```
(`IF_TRUE_MSG_AND_BREAK` inside the if/else still breaks the `for` — same control flow as retail's.)

Uncertainties / port notes:
* `StaticFindObject(NULL,…)` accepts any object with that name and casts it to `UClass*` — shipped behaviour. A safer,
  behaviour-equivalent-in-practice variant is `FindObject<UClass>(ANY_PACKAGE, strClassName)`; your call (fidelity vs safety).
* The spawn happens inside `LoadMap` before `InitLevel` (same place as retail), so `SpawnActor`'s script events
  (Spawned/PreBeginPlay/BeginPlay…) run at that point, and `bScriptInitialized=false` makes LoadMap initialise the
  actor again — that is what retail does; flagging it only because it is a new code path for the port.
* Optional hardening (OURS, not retail; recommended because of the §3 format change): before each of the two raw string
  reads (and the EOF one) reject `iToLoad <= 0 || iToLoad > sizeof(strName)` with the existing error message + break.
  Without it, a proto-layout `_pa.usa` still sitting in a save slot or in `cache/` (they are copied around by
  `Load/SavePersistentActorCache`) overflows `strClassName`. With it, such a file just logs an error and leaves that
  level's persistent actors at map defaults.

---

## 5. `ULevel::ScreenActorsByGameState` — GAMESTATE= override, OnResolveGameState for ALL actors (REAL)

Layout-only: `CurrentGameState` 0x940->0x944, `GameStateMasterList` 0x934->0x938, `GameStateTokenLen` 0x930->0x934
(+4 from `bShowLoadingScreen`); the `"GSTATE"` search went from inlined `appStrstr` to the imported `FString::InStr` (codegen).

Real changes (objdump 103b87ed-103b8a49 and 103b8d61-103b8d8b):
1. The "no player" case is now its own check with new text `"**** Can't find a valid player!!!! PP == NULL "` (NAME_Log),
   *before* the new override.
2. **New command-line override** between the player check and the master-list check:
   `FString S; if( Parse(appCmdLine(), TEXT("GAMESTATE="), S) && pPPawn->GameStateMasterList.InStr(*S) != 0 )
   { CurGState = S.Caps(); pPPawn->CurrentGameState = CurGState; }` — note the shipped test is `!= 0`
   (`cmp eax,0; je skip`), not `>= 0`: a state found at position 0 of the master list is NOT applied, a state not
   found at all IS applied and then fails the master-list check below ("Caution…" + return). The search is
   case-sensitive on the un-capitalised argument. Keep as shipped (or change to `>= 0` if you prefer the evident intent —
   it is a debug/dev switch).
3. **`eventOnResolveGameState()` is now called for every non-NULL actor (in game, `!GIsEditor`)**, not only for actors
   that have a GState token/exclude list; `bInCurrentGameState` is still written only when `bHasGState`. Consequence:
   actors without any GState now get the event with their untouched (default `true`) `bInCurrentGameState` — e.g.
   `CutScene.OnResolveGameState` takes its "IN the current GameState" branch (`GotoState('idle')`, and for
   `bDelayLevelFadeIn && bLevelLoadStarts` cut scenes sets `harry.FadeRate=0`) for cut scenes with no GState group.
   This is retail behaviour and may matter for the cut-scene fade-in triage.
4. A dead `if( CurGState.Len() == -1 ) return;`-shaped test right after the NONE check (`dec eax; cmp eax,-1`) — can
   never fire; omitted.

### Patch (confidence: high)
```diff
--- a/Source/Engine/Src/UnLevel.cpp
+++ b/Source/Engine/Src/UnLevel.cpp
@@ -142,15 +142,34 @@ void ULevel::ScreenActorsByGameState( void )
 	if( CurGState == TEXT("NONE") )
 		return;
 
-	// If we didn't find the player or the player's CurGState is not valid then return
-	if( pPPawn == NULL || (pPPawn->GameStateMasterList.InStr( CurGState ) < 0))
+	// If we didn't find the player then return
+	if( pPPawn == NULL )
+	{
+		debugf( NAME_Log, TEXT("**** Can't find a valid player!!!! PP == NULL ") );
+		return;
+	}
+
+	// Retail (HPCos 021009): GAMESTATE=<token> on the command line overrides the player's CurrentGameState.
+	// The shipped test is "!= 0" (sic), not ">= 0": a token at the very start of the master list is ignored.
+	FString CmdLineGState;
+	if( Parse( appCmdLine(), TEXT("GAMESTATE="), CmdLineGState ) )
+	{
+		if( pPPawn->GameStateMasterList.InStr( *CmdLineGState ) != 0 )
+		{
+			CurGState = CmdLineGState.Caps();
+			pPPawn->CurrentGameState = CurGState;
+		}
+	}
+
+	// If the player's CurGState is not valid then return
+	if( pPPawn->GameStateMasterList.InStr( CurGState ) < 0 )
 	{
-		if( pPPawn == NULL)
-			debugf( NAME_Log, TEXT("**** Can't find a valid player!!!! ") );
-		else
-			debugf( NAME_Log, TEXT("**** Caution: The player pawn has an Invalid CurrentGameState for this package Load."));
+		debugf( NAME_Log, TEXT("**** Caution: The player pawn has an Invalid CurrentGameState for this package Load."));
 		return;
 	}
 	
@@ -228,17 +247,15 @@ void ULevel::ScreenActorsByGameState( void )
 		}while( 1 );
 		
 		// Test to see if this actor has a GState and its in the currentGState
 		if( bHasGState )
 		{
 			// Set the bInCurrentGameState boolean
 			pAct->bInCurrentGameState = bInCurGState;
-			
-			// If we are in the game then call the resolveGameState function.
-			if( !GIsEditor )
-			{
-				// Call this actor's OnResolveGameState function so it can handle itself accordingly
-				pAct->eventOnResolveGameState();
-			}
 		}
 
+		// Retail: in the game EVERY actor resolves its game state (the prototype only called actors with a GState)
+		if( !GIsEditor )
+			pAct->eventOnResolveGameState();
+
 	}// end for AllActors
 }
```

---

## 6. Core.dll persistent-actor cache (REAL, small; outside Engine.dll — hand to whoever owns Core)

* `LoadPersistentActorCache`: its first half ("delete everything in `<SaveSlotPath>/cache/*.*`") became a separate,
  unexported helper (retail Core 10157b90, reached via thunk 1010369d) with **no** `SaveSlotPath` guard;
  `LoadPersistentActorCache` keeps its guard, calls the helper, then copies `<slot>/*_pa.*` into `cache/` as before.
* **`appPreExit` now calls that helper first thing inside its guard** (before `debugf(NAME_Exit,"Preparing to exit.")`):
  the session cache is wiped on a normal exit. (Startup already wipes and refills it, so the visible effect is only that
  no `cache/` leftovers stay on disk.)
* All four `if( GFileManager->FileSize(*PAFileName) )` tests (Save: delete + copy; helper: delete; Load: copy) became
  `> 0` (`je` -> `jle`): a file whose size query fails (-1) is no longer deleted/copied.
* `SavePersistentActorCache` gained a trailing `Files.Empty()` — no behaviour.

Patch sketch (confidence: high; the `GSys &&` guard in the helper is ours — appPreExit can run on ucc/early-exit paths):
```diff
--- a/Source/Core/Src/UnMisc.cpp
+++ b/Source/Core/Src/UnMisc.cpp
@@ (SavePersistentActorCache) @@
-		if( GFileManager->FileSize( *PAFileName ) )
+		if( GFileManager->FileSize( *PAFileName ) > 0 )		// x2 (delete loop and copy loop)
@@ -2193,8 +2193,26 @@
+// Retail (HPCos 021009): empty <SaveSlotPath>/cache. Split out of LoadPersistentActorCache and also run from appPreExit.
+static void ClearPersistentActorCache( void )
+{
+	if( !GSys )	// ours: appPreExit may run before/without appInit's GSys
+		return;
+	FString			Spec  = GSys->SaveSlotPath * TEXT("cache") * TEXT("*.*");
+	TArray<FString> Files = GFileManager->FindFiles( *Spec, true, false );
+	FString			PAFileName;
+	for( INT j=0; Files.IsValidIndex(j); j++ )
+	{
+		PAFileName = GSys->SaveSlotPath * TEXT("cache") * Files(j);
+		if( GFileManager->FileSize( *PAFileName ) > 0 )
+			GFileManager->Delete( *PAFileName, false, true );
+	}
+}
+
 CORE_API void LoadPersistentActorCache( void )
 {
 	if( !*GSys->SaveSlotPath )
 		return;
-	// Delete all cache _pa files  ... (the whole first FindFiles/Delete loop + Files.Empty())
+	// Delete all cache _pa files
+	ClearPersistentActorCache();
+	TArray<FString> Files;
+	FString			Spec, PAFileName;
 	// Copy pa files from our saveGameSlot to our cache
 	...
-		if( GFileManager->FileSize( *PAFileName ) )
+		if( GFileManager->FileSize( *PAFileName ) > 0 )
@@ CORE_API void appPreExit()
 	guard(appPreExit);
 
+	ClearPersistentActorCache();	// retail: leave no session PA cache behind
 	debugf( NAME_Exit, TEXT("Preparing to exit.") );
```
(Exact hunk context left loose on purpose — this is Core and may already be on another agent's list.)

---

## 7. Layout-only / no-change items (no action)
* SaveGame: `GetLevelInfo()` check uses out-of-line `UObject::IsA` once — codegen.
* Tick PlayerSaving block: `bQueuedToSaveGame` offset shift only. Exec SAVEGAME/LOADGAME/SAVEPACTORS: identical.
  LoadMap's `LoadPersistentActors(URL.Map)` / `ScreenActorsByGameState()` call sites and their `Entry` guards: identical.
* `AActor::SerializePersistence`, `execSaveGameSaveInfo`, `execLoadGameSaveInfo`, `execSaveGameExists`: identical.
* `eventPreSaveGame`/`eventPostSaveGame`/`ENGINE_PreSaveGame`/`ENGINE_PostSaveGame`, `bShowLoadingScreen`: already in our tree.
* Our own changes (LoadPersistentActors per-actor resync, FFileManagerLinux FName serialization, MakeDirectory Tree=1,
  the Tick NULL checks) are not retail and are kept by every patch above.
