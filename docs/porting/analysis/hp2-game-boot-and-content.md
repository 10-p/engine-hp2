# HP2 — how the game boots and what content it needs

Analysis key: `hp2-game-boot-and-content` · written 2026-09-26 against
`/home/ion/hp2-ufrontend/HarryPotter2` (build 433 drop of 2002-09-06, `Version.uc` = `020805 1508`),
the template `/home/ion/engines/engine-ut99` and the frontend clone `/home/ion/hp2-ufrontend/ufrontend`
(branch `hp2`). Every claim names a file and line; "VERIFY" marks what only a running build can settle.
Line numbers are those of the LF-normalised repo.

**Audience:** whoever writes `Source/Game/Src/Launch.cpp` (Phase B), `Config/System/web/*.ini` (Phase D),
`targets/hp2/{target.json,precache.json,file-manifest.json}` and the `NOpenALDrv` port (Phase E).

---

## 0. The ten facts that change the plan

| # | Fact | Where | Action |
|---|------|-------|--------|
| 1 | **The retail "main menu" is a Win32 dialog, not UnrealScript.** `Game.exe` with no `-SAVESLOT=`/`-NOFRONTEND` shows `WFrontEnd` (New Game / Load Game / Options, six save slots with thumbnails) and then **spawns a second `Game.exe`** with `PrivetDr.unr -SAVESLOT=<n>` (new game) or `-LOAD=0 -SAVESLOT=<n>` (load). The in-game UnrealScript menu (`FEMainPage`) is a dev level-jump list. | `Source/Engine/Inc/UnEngineWin.h:1740-1799`, `:312-337`, `:184-186`; `Assets/System/Game.exe` exports `WFrontEndDialog` symbols | The ufrontend Play panel **is** the front end. The web launcher never shows a dialog; it starts with a map on the command line (`PrivetDr.unr`) or with `-LOAD=0` and no map. |
| 2 | **Startup.unr is an attract/title fly-through, not a menu.** Booting into it (the `[URL] LocalMap`) creates the `FEBook` and immediately closes it; nothing opens the main page. | `HGame/Classes/Menu/HPConsole.uc:1128-1150` (`MenuBook.CloseBook()` right after creation), `FEBook.uc:328-411` | Phase B's "boots to the main menu" = `Game.log` reaches `Bringing Level startup.MyLevel up for play` + `Creating root window` (WindowConsole.uc:289) + no `Critical:`. Press **Esc** to see a menu page (`FEInGamePage`). |
| 3 | **Compressed sounds are EA-XA ADPCM, not MPEG Layer II.** Every `SF_Compressed` export's payload is 15-byte frames whose first byte is `0x2N`/`0x3N` (coefficient index / shift), produced by `System/sx.exe` (EA Sound eXchange) at import and decoded by the closed `coda` library. Plain sounds are RIFF WAVE PCM 22050 Hz mono 16-bit. | §7; `Assets/Sounds/*.uax` bytes; `Assets/System/makesoundpackages.bat`; `Source/Engine/Src/UnAudio.cpp:150-191` | Phase E: port/borrow an EA-XA decoder (vgmstream `EA_XA_V2` / ffmpeg's EA ADPCM family), **not** a Layer-II decoder. PLAN §2 "Audio" and §7 step 1 must be corrected. |
| 4 | **Script hard-codes Windows paths.** Cut scenes are read as `"Cutscenes\\" $FileName` (compiled into `HGame.u`, cannot be changed), the shipped dir is `System/CUTSCENES`, music is `..\music\<Song>[.ogg]`, level buttons use `Studies\\TestLevel.unr`, the default save dir is the literal `"..\\Save"`. Neither the template's `FFileManagerLinux::CreateFileReader` (plain `fopen`) nor FetchFS's `normalizePath` (splits on `/` only) folds `\`. | `HGame/Classes/CutScene.uc:97-101`, `ALAudio/Src/ALAudioSubsystem.cpp:1136`, `FEMainPage.uc:325`, `Core/Src/UnMisc.cpp:2011`; template `Core/Inc/FFileManagerLinux.h:216-228`; `ufrontend/packages/glue/src/fetchfs/index.js` `normalizePath` | The HP2 Unix file manager must convert `\`→`/` in **every** open/size/delete/mkdir (the template already has `appUnixPath()` in `UnUnix.cpp:840-850`; wire it in) **and** resolve case-insensitively natively (`Cutscenes` vs `CUTSCENES`, song names in maps differ in case from the files). On the web FetchFS's manifest already gives case-insensitivity once the slashes are fixed engine-side. |
| 5 | **`Game.ini`/`User.ini` are regenerated from `Default.ini`/`DefUser.ini` whenever the default is newer** (an HP2-only rule, epic v400 only regenerates when the file is missing). Preloaded MEMFS files are "newer" than a restored ini every boot → settings wiped every launch. | `Core/Src/UnMisc.cpp:1915-1921`, `:1929-1936` vs epic `Core/Src/UnMisc.cpp:1715-1725` | Restore epic's `FileSize(GIni)<0` rule in the port (or have the launcher `utime()` the restored inis). |
| 6 | **`AllDialog.uax` (97.4 MB) is loaded on demand** by `DynamicLoadObject("AllDialog."$cue)` at the first spoken line; it is *not* inside `HPSounds.u` (the `#exec OBJ LOAD` for it is commented out). Everything else the maps need from `Sounds/` is also baked into `HPSounds.u` (30.6 MB). | `Engine/Classes/Actor.uc:700,746`; `Source/HPSounds/Classes/PackSounds.uc:3` | Precache it (or background-prefetch after the menu) — an on-demand fetch of 97 MB stalls the engine thread at the Privet Drive narration. |
| 7 | **`Entry.unr` imports `HGame` (and `Editor`)** → the whole script set (`HGame`+`HPModels`+`HProps`+`HPSounds`+`HPParticle`+`UWindow`+`Fire`, 105.6 MB) plus `HP2_Master.utx` (19 MB) is opened before the first map. | §6; `updeps` parse of `Assets/Maps/Entry.unr` | The precache list is ≈139 MB for the menu, ≈242 MB for "menu + Privet Drive + dialog". Editor stays linked (PLAN decision confirmed: Entry, Quidditch and Quidditch_Intro import `Editor`). |
| 8 | **Saves are automatic and always `Save0.usa`** in the slot directory; the player never picks a slot in-game. Companion files: `GameSaveInfo0`, `Save0.bmp`, `<Map>_pa.usa` (+ `cache/`), hub-stack `Game<i>.usa`/`Save0<i>.usa`. Death reloads `Save0.usa`. | `Engine/Src/UnGame.cpp:1760-1779` (hard-coded slot 0), `:1967-2020`; `UnScript.cpp:2799`; `UnCamera.cpp:811-816`; `UnLevel.cpp:281-282`; `Harry.uc:1965` | `userFiles` needs six globs per slot directory (§3). Recommendation: v1 uses a single slot (no `-SAVESLOT=`, files in `Save/`). |
| 9 | **`Pickup.int` is missing from the drop** — the main page's legal line and the "find N wizard cards" strings resolve to `<?int?Pickup.all.legal_title_01?>`. `HGame.int` is missing too but every use is guarded. | `FEMainPage.uc:81`, `HPConsole.uc:1244-1256`; `ls Assets/System/*.int` | Cosmetic; optionally ship a 4-key `Pickup.int` in the web boot set. |
| 10 | **No mouse-gesture casting in HP2 PC.** `Gesture.CompareGesture` has no script caller; spells are "hold left mouse to aim, release to cast" (`HPMenu.int` `Controls_0008/0011`). `UGesture` only feeds particle patterns. | `grep CompareGesture Source/HGame` → none; `Engine/Classes/ParticleFX.uc:105` | Browser needs ordinary pointer lock + relative deltas for mouse-look and an absolute cursor when UWindow shows the mouse (`Viewport.bShowWindowsMouse`). Nothing exotic. |

---

## 1. The boot sequence on the original PC

### 1.1 Process 1: `Game.exe` (no arguments) — the front end

`Source/Launch/Src/Launch.cpp` (`WinMain`, not built by us; the semantics are what the SDL launcher must mimic):

| Step | Code | What happens | Web port |
|------|------|--------------|----------|
| SafeDisc marker | `Launch.cpp:54-65,87-95` `CDAPFN_DECLARE_GLOBAL(CheckSafeDiskProtection…)`; the function body `return 0` | The real check was injected by the SafeDisc post-processor into the retail exe; the source is a no-op wrapper (`CdaPfn.h`). | Drop entirely. |
| Single instance | `:98-127` finds a `WLog` window with prop `IsBrowser` and forwards the command line via `WM_COPYDATA` | Win32 only. | Drop. |
| `appInit` | `:141` `appInit(GPackage, CmdLine, …)`; `GPackage = appPackage()` = exe base name → **`Game`** → inis are `<UserDir>Game.ini` / `<UserDir>User.ini` (`Core/Src/UnMisc.cpp:1912-1936`); `appUserDir()` = `%USERPROFILE%\My Documents\Harry Potter and the Chamber of Secrets\` (`Core/Src/UnVcWin32.cpp:1048-1070`) | Game.ini/User.ini derived from `Default.ini`/`DefUser.ini` in `System/` when missing **or older** (fact 5). | Executable named `Game` (decided). `appUserDir()` does not exist in `UnUnix.cpp` (only in `UnVcWin32.cpp`) — add one returning `""` so the inis land in `System/` like the template. |
| Language | `UnMisc.cpp:1956-1964` copies `[Engine.Engine] Language` from `Default.ini` **over** `Game.ini` on every boot ("gk 10/15/01"); `:1968-1971` `UObject::SetLanguage` | `Language=int` (`Config/System/Default.ini:28`). | Keep. Only `int` content exists (§4). |
| Save paths | `UnMisc.cpp:1996-2064` (§3) | `SavePath`, `SaveSlotPath`(+`cache/`), `MapPath`, persistent-actor cache, `Paths=` rewrite | Fix the `"..\\Save"` literal; keep `-SAVESLOT=` parsing (it is in Core, not the launcher). |
| Mode flags | `Launch.cpp:143-148` `GIsServer=1; GIsClient=!-SERVER; GLazyLoad = !GIsClient || -LAZY` | | As template. |
| RunCount + splash 1 | `:154-181` `[Engine.Engine] RunCount` incremented and written; `..\Help\EALogo<lang>.bmp` (fallback `EALogo1.bmp`) shown for `EASplashWaitTime` (1.5 s, `Default.ini:20`) — skipped when `-SAVESLOT=` or `-NOFRONTEND` or `-LOG`/`-server`/`TestRenDev` | | No splash. `RunCount` may stay (harmless ini write). `Help/` is not needed by the web build at all. |
| Splash 2 | `:183-199` `..\Help\WBLegal<lang>.bmp` for `WBSplashWaitTime` (1.5 s) | | Drop. |
| Log window | `:202-209` `WLog` | | GLog→stdout (template). |
| Demo probes | `:211-233` sizes of `Engine.dll`, `HPMenu.u`, `HarryPotter.u` (HP1 names) logged; the refusal is commented out | | Drop. |
| `InitEngine()` | `:236` → `Engine/Inc/UnEngineWin.h:1657-1878` | see 1.2 | Reimplement the non-UI parts in `Source/Game/Src/Launch.cpp`. |
| `-EXEC=` | `:244-251` `Viewports(0)->Exec("exec <file>")` | | Keep (cheap). |
| `MainLoop` | `:255` → `UnEngineWin.h:1885-1975` (tick, `GetMaxTickRate`, message pump) | | Template main loop + `FrameRateCap`. |
| Shutdown | `:259-270` deletes `<UserDir>Running.ini`, `Engine->Audio->SetViewport(NULL)`, `appPreExit`, `appExit` | | Template. |

`InitEngine()` (`UnEngineWin.h`):

| Lines | What | Web port |
|-------|------|----------|
| 1668 | `CreateMutexX("UnrealIsRunning")` (installer hint) | drop |
| 1676-1682 | reads `[FirstRun] FirstRun`, `Reconfig`, `ForceSoftware` (`Default.ini:14-17`); `Reconfig` only toggled by the (dead) HP1 renderer page (`:1260-1262`); `ForceSoftware` never read again | keep the section; stamp `FirstRun=433` (`:1818-1820`) exactly as the template does |
| 1685-1697 | `#if 0` save-game migration (HP1) | — |
| 1699-1705 | `consolecommand=<cmd>` → `GExec->Exec`, then **exit** (mplayer/heat) | drop |
| 1708-1737 | `testrendev=<class>` → constructs the render device, writes `Detected.ini`, exits (used by `CreateDetected()` `:153-163` which spawns `Game.exe testrendev=D3DDrv.D3DRenderDevice log=Detected.log` from the Options page) | drop |
| **1740-1799** | **Config UI.** Unless `-SAVESLOT=` or `-NOFRONTEND` (or `GDebugger`): modal `WFrontEnd` with `WFrontEndPageMainMenu`; **`DoModal()==0` → `InitEngine` returns NULL → the process exits**. Pages: New Game (`:341-435`, 6 slot buttons; clicking a used slot asks to overwrite then `CleanSaveSlotDir`), Load Game (`:438-529`), Options → Video (`:670-877`: D3D vs Software radio, resolution list from `WinDrv`, windowed) / Sound (`:531-668`: `UseSound`, volumes) / Joystick. Titles from `Assets/System/Startup.int` `[IDDIALOG_FrontEnd*]`. | The ufrontend Play panel replaces all of it. Accept and ignore `-NOFRONTEND`. |
| 312-323 | `LaunchNewGame(iSlot)`: `ShellExecute(Game.exe, "<original cmdline> PrivetDr.unr -SAVESLOT=<i>")` (`NEWGAME_COMMAND_LINE_TOKEN = "PrivetDr.unr"`, `:184`) | **New game = launch `PrivetDr.unr`.** |
| 326-337 | `LaunchLoadGame(iSlot)`: `"… -LOAD=0 -SAVESLOT=<i>"` (`LOADGAME_COMMAND_LINE_TOKEN = "-LOAD=0"`, `:185`) | **Continue = launch with `-LOAD=0` (+ slot).** |
| 235-258 | slot dirs `GSys->SavePath * "Slot<i>"`, i = 1..6 (`NUM_SAVE_SLOTS = 7`, loop from 1); a slot is "used" when `Slot<i>\Save0.usa` exists (`SAVE_FILENAME`, `:177`); thumbnail `Save0.bmp` (`:178`) | §3 |
| 1801-1810 | `..\Help\splashint.bmp` shown while loading (unless `-LOG`) | drop |
| 1813-1815 | `<UserDir>Running.ini` semaphore (crash detection for the never-shown safe-mode page) | drop |
| 1823-1845 | CD check: only if `[Engine.Engine] CdPath` is set **and** `..\Textures\Palettes.utx` is missing → "Insert CD" box. `CdPath` is not in `Default.ini`. | nothing to neutralise; keep `Textures/Palettes.utx` in the manifest anyway (421 KB) |
| 1866-1874 | `StaticLoadClass(… "ini:Engine.Engine.GameEngine")` → `Engine->Init()` | template |

### 1.2 Process 2: `Game.exe PrivetDr.unr -SAVESLOT=1` — the game

`UGameEngine::Init` (`Source/Engine/Src/UnGame.cpp`):

| Lines | What |
|-------|------|
| 150-162 | `ViewportManager` (`ini:Engine.Engine.ViewportManager`) and `Render` classes constructed |
| 164-172 | **`LoadMap(FURL("Entry"))` → `GEntry`** (`Assets/Maps/Entry.unr`, 6,626 B, package version 76; imports `Editor Engine HGame HP2_Master UnrealShare`; contains `LevelInfo0`, `PlayerStart0`, `Harry0`) |
| 174-186 | `DefaultURL.LoadURLConfig("DefaultPlayer","User")` → `User.ini [DefaultPlayer] Name=Player Class=HGame.harry team=255` (`DefUser.ini:1-4`); `Parm` = first command-line token, or **`FURL::DefaultLocalMap` = `Startup.unr`** (`Default.ini:6`) when there is none or it starts with `-` |
| 188-200 | `-REPLAY=`/`-RECORD=` |
| 202-214 | `Browse(URL)`; on failure with empty error falls back to `DefaultLocalMap`; `FailedBrowse` is fatal |
| 217-241 | viewport, **console class `ini:Engine.Engine.Console` = `HGame.HPConsole`** (`Default.ini:27`), `SpawnPlayActor`, `Viewport->OpenWindow`, `InitAudio()` |
| **244-255** | **HP2: `-LOAD=<n>`** → `<SaveSlotPath>/Save<n>.usa` → `LevelInfo.eventServerTravel(Filename, 0)`; the travel happens on the next tick (`:1858`) via `Browse` → `LoadMap` of the `.usa` package (FURL keeps the `.usa` extension: `UnURL.cpp:238-239`, `[URL] SaveExt=usa`) |

Log milestones (all `NAME_Log`, i.e. `Log:` lines): `Browse: <url>` (`UnGame.cpp:651`), `LoadMap: <url>` (`:842`), `Bringing Level <pkg>.MyLevel up for play` (`UnLevel.cpp`, as UT), `Game engine initialized` (`:242`), `Loading game... filename: <path>` (`:253,474`), `Saving game... filename: <path>` (`:1976`); UnrealScript `Log()` lines carry `ScriptLog:` — `Changing level to:<map>,<bool>` (`HPConsole.uc:307`), `Creating root window: <class>` (`WindowConsole.uc:289`), `LanguageCode=int` (`HPConsole.uc:903`), `OpenBook <page>` (`FEBook.uc:480`), `ChangePage <page>` (`:139`), `autoplay` (`:404`).

### 1.3 Inside the engine: `HPConsole` and the `FEBook`

`Console=HGame.HPConsole` extends `baseConsole` extends `UWindow.WindowConsole`.

1. `HPConsole.Tick` (`HPConsole.uc:330-350`): on the first tick `LaunchUWindow()` (`WindowConsole.uc:243-260`: `bShowWindowsMouse=True`, `GotoState('UWindow')`).
2. In state `UWindow`, `PostRender` → `HPConsole.RenderUWindow` (`:1105-1200`): creates the root window (`WindowConsole.CreateRootWindow`, `:279-337` — `RootWindow` class from ini, the console window), `SetupLanguage()` (`:896-1000`, picks fonts per `GetLanguage()`), creates **`FEBook`** (`:1132`), then **`MenuBook.bGamePlaying=true; MenuBook.CloseBook(); HideConsole();`** (`:1145-1147`). The old "open the main page when the level is Startup" branch is commented out (`:1055-1080`, `:1135-1144`).
3. `FEBook.Created` (`FEBook.uc:328-411`) builds the pages: `FEInGamePage`, `FEFolioPage`, `FEInputPage`, `FESoundVideoPage`, `FEQuidPage`, `FEDuelPage`, `FEHousepointsPage`, `FEMainPage`, `FEMapPage`, `FECreditsPage`, `FESoundBrowser` (as `LangPage`); `OpenBook("MAIN")`; if the local URL contains `AUTOPLAY` → `CloseBook()`.
4. `FEMainPage` (`FEMainPage.uc:70-185`): **no New Game/Load Game buttons are created** (the vars exist, `ChangePageNamed("load")` logs `UnknownPage`); it creates **26 level-jump buttons** (`defaultproperties :306-335`, 15 "official" + 11 "preview") plus **Exit** (`Root.DoQuitGame()`), a version label (`Version.uc` `020805 1508`), and the legal line (`Localize("all","legal_title_01","Pickup")` → unresolved, fact 9). Lang/Credits/Version buttons are hidden unless `bDebugMode`.
5. Keys (`HPConsole.KeyEvent`, `:600-870`; `state UWindow` `:504-560`): **Esc** → `MenuBook.EscFromConsole()` → `OpenBook("INGAME")` + pause (`FEBook.uc:559-569`); **`=`** → map page (`:573-583`); **F11** → quit confirmation (`:585-597`); **F4** → debug ShortCut window (level list from `GetMapName`, `ShortCutListLevels.uc:17-43`); **F7** toggles `bDebugMode`; **Tab** → console (debug only); **Insert** → `sshot()`; **Space** → skip cut-scene / broom boost (`bSkipKeyPressed`); **PageUp/PageDown** → `SloMo` game-speed steps (debug leftover, always active); **Pause/Cancel** → `pause`; NumPad keys → debug camera; F6/F9 give health/all spells (debug). `bDebugMode` comes from `[HGame.baseConsole] bDebugMode=true` in the shipped `Default.ini:506-507` and `Version.bDebugEnabled=TRUE` (compiled).
6. In-game page (`FEInGamePage.uc`): status icons, Folio (wizard cards), Map, Quidditch, Duel, House points, Input, Sound&Video, **Quit → `Root.DoQuitGame()`** (`:334-344`, process exit; the "back to startup.unr" variant is commented out in `FEOptionsPage.uc:222-236`).

**What the player sees on a cold web boot with no map:** Entry → `Startup.unr` ("Fireseed Challenge Two" by Ben Golus: three `CutScene` actors with inline scripts `capture basecam / basecam flyto cutmark01 * / basecam target flyto cutmark02`, HP_Titles logos, `Hedwig`, song `Music_StoryBook_v2.ogg`) with the book closed. Esc opens the in-game page. This is fine as the Phase B/D smoke target, but **the product boot is "Play → PrivetDr.unr"**.

### 1.4 Console commands and travel calls the menus issue (grep of `HGame/Classes`)

| Caller | Command / call | Notes |
|--------|----------------|-------|
| `HPConsole.ChangeLevel` (`:305-310`) | `viewport.Actor.Level.ServerTravel(lev, flag)` | every level change goes through here (`baseConsole.ChangeLevel` stub `:44-46`) |
| `FEBook.RunURL`/`Tick` (`:702-708`, `:292-325`) | `ChangeLevel(_URLToLoad, bTravelItems)`; sets `HPConsole.bInHubFlow` | level buttons call `RunURL(url,false)` |
| `FEMainPage.Notify` (`:215-265`) | `RunURL(levelList[i].levelurl, false)`; `RunURL("e3demo.unr")` (E3 mode) | |
| `FEDuelPage` (`:174-179`) | `ServerTravel("Duel0"$(curWizardDuel+1)$".unr", true)` | `Duel01..Duel10.unr` |
| `FEQuidPage` (`:241-244`) | `ServerTravel("Quidditch.unr", true)` | |
| `TriggerChangeLevel.ProcessTrigger` (`:24-79`) | `ConsoleCommand("SavePActors")` then `ChangeLevel(NewMapName, true)`; if `NewMapName` contains `STARTUP` → `MenuBook.OpenBook("Main")` | the in-map hub doors |
| `FEBook.WindowDone` (`:528-550`) | `ChangeLevel("startup.unr", false)` then `OpenBook(); ChangePage(MainPage)` | only path that shows the level list after play |
| `Harry` death loop (`Harry.uc:1940-1990`) / `SleepingGoyle.uc:111` | `ConsoleCommand("LoadGame 0")` | **no save → `LoadMap` fails → `?failed` → Entry (black screen)** |
| `SavePoint.OnSaveGame` (`SavePoint.uc:36-48`), `Harry.TravelPostAccept` (`Harry.uc:1052-1064`, `smartStart.bDoLevelSave`), cut-script `SAVEGAME` (`CutScript.uc:488-490`) | `playerHarry.SaveGame(0)` → `PlayerPawn.SaveGame` (`PlayerPawn.uc:276-293`): `Thumbnail(120,90,"Save0.bmp")` + `bQueuedToSaveGame=true` → engine tick `SaveGame(0)` (`UnGame.cpp:1760-1783`) + `Exec("RestartSounds")` | slot number ignored: always `Save0.usa` |
| `HPConsole.doLevelSave` (`:384-472`) | `playerPawn.SaveGame(i)`; `SaveGameSaveInfo("GameSaveInfo"$i, gameSaveInfo)` | |
| Engine `Exec` (`UnGame.cpp:441-497`) | `SAVEGAME <n>`, `LOADGAME <n>` (→ `LoadPersistentActorCache()` + `ServerTravel(<SaveSlotPath>/Save<n>.usa)`), `SAVEPACTORS` (→ `SavePersistentActors(LevelEnterText)`) | |
| `FESoundVideoPage` (`:314-623`) | `get/set ini:Engine.Engine.AudioDevice MusicVolume|SoundVolume` (0..1 floats ×100), `get ini:Engine.Engine.ViewportManager StartupFullscreen|FullscreenViewportX/Y|WindowedViewportX/Y|Brightness|TextureDetail`, `get ini:Engine.Engine GameRenderDevice`, `GetRes`, `GetCurrentRes`, `GetColorDepths`, `GetCurrentColorDepth`, `SetRes WxHxD`, `FLUSH`, `set ini:HGame.Harry ObjectDetail` | **NSDLClient / NOpenALDrv sections must carry these keys and execs** or the pages show garbage / revert |
| `FEInputPage` (`:638-1047`) | `KEYNAME <i>`, `KEYBINDING <key>`, `SET Input <key> <alias>`, `set ini:Engine.PlayerPawn bInvertMouse`, `set ini:HGame.Harry bAutoCenterCamera|bAutoQuaff|bMoveWhileCasting|Difficulty` | |
| `Harry.uc:543` | `get ini:Engine.Engine.AudioDevice Use3dHardware` | add the key to the NOpenALDrv section (bool) |
| `Harry.uc:2299` | `ConsoleCommand("exit")` | |
| `HPConsole.uc:513` | `SetRes <saved>` (resolution-change timeout revert) | |
| `FEMainPage`/`FEInGamePage`/`FEOptionsPage` | `Root.DoQuitGame()` | process exit → on the web `appRequestExit` (template) |

`Difficulty`: `PlayerPawn.Difficulty` is `globalconfig` (`PlayerPawn.uc:116-121`) → stored under `[Engine.PlayerPawn]` (`DefUser.ini:234`), used by `Harry.uc:2607-2613` for damage scaling. `?Difficulty=` on a URL only sets `GameInfo.Difficulty` (bot noise, `GameInfo.uc:576`).

### 1.5 Command-line switches (what the SDL launcher must honour)

| Switch | Parsed where | Keep? |
|--------|--------------|-------|
| `<Map>[.unr][?opts]` first token | `UnGame.cpp:180-186` | yes |
| `-SAVESLOT=<n>` | `Core/Src/UnMisc.cpp:2018-2031` (`SaveSlotPath = SavePath/Slot<n>`); also `Launch.cpp:166` and `UnEngineWin.h:1752` (skip splash/front end) | yes (Core does the work) |
| `-LOAD=<n>` | `UnGame.cpp:246-254` | yes |
| `-NOFRONTEND` | `Launch.cpp:166`, `UnEngineWin.h:1753` | accept, no-op |
| `-EXEC=<file>` | `Launch.cpp:246` | optional |
| `-LOG`, `-SERVER`, `-LAZY`, `MEMSTAT`, `-window`, `INI=`, `USERINI=`, `CDPATH=`, `-STRICT`, `-REPLAY=`, `-RECORD=` | `Launch.cpp:139-154`, `UnMisc.cpp:1911,1927,1980`, `UnGame.cpp:188-200` | template equivalents (`-log`, `-ini`, `-userini`) |
| `consolecommand=`, `testrendev=`, `-nodetect`, `changevideo`, `-safe`, `readini`, `NewWindow` | `UnEngineWin.h:1699-1737,153-163`, `Launch.cpp:98-102` | drop |
| `RunAMap.bat`: `start game %1 -NOFRONTEND -window` (`Assets/System/RunAMap.bat`) | | confirms the dev launch shape |

---

## 2. `autoplay.unr`

- **Not present**: `Assets/Maps` has no `autoplay.unr` (48 top-level maps, §9).
- `[URL] Map=autoplay.unr` (`Default.ini:5`) is `FURL::DefaultMap` (`Engine/Src/UnURL.cpp:35`), the map component a URL gets when it has none (`:97`, `:120`). Boot uses `LocalMap=Startup.unr` (`DefaultLocalMap`, `:36`), so `autoplay.unr` is never requested at boot. The string also sits in every map's name table (the editor saves the default URL) — `strings` shows it in all 48 files; it is not a link.
- `FEBook.Created` (`FEBook.uc:400-408`) checks `InStr(caps(GetLocalUrl()),"AUTOPLAY")` and closes the book: the dev/QA (and now the modding community's — see the ModDB "AutoPlay2.unr" addons, §10) convention is to name a map `autoplay.unr` and run `Game.exe autoplay.unr` to skip menus.
- **Action:** do not `require` it; do not precache it; if the mods feature lets a player drop an `autoplay.unr` into `Maps/`, "Play autoplay" just works.

---

## 3. Save games

### 3.1 Paths (`Core/Src/UnMisc.cpp:1996-2064`, `appInit`)

```
1998  if( *appUserDir() ) { GSys->SavePath = appUserDir() * "Save"; GSys->Paths[5] = SavePath * "*.usa"; }
2011  else                  GSys->SavePath = TEXT("..\\Save");                       // literal backslash
2020  if( Parse(cmdline,"SAVESLOT=",iSlot) ) appSetSaveSlotDir(iSlot);   // SavePath * "Slot<n>"
2025  else GSys->SaveSlotPath = GSys->SavePath;
2036  MakeDirectory( SaveSlotPath * "cache" );
2043  GSys->MapPath = <baseDir up to "System"> + "Maps"   (derived from the Paths entry ending in ".unr")
2060  LoadPersistentActorCache();
2062  GConfig->SetString("Core.System","SavePath", SaveSlotPath);
2063  GConfig->SetString("Core.System","paths",   SaveSlotPath * "*.usa");   // replaces ONE Paths= line (VERIFY which)
```
The ini's own `[Core.System] SavePath=../Save` (`Default.ini:42`) is **overwritten** by the code above. `[URL] SaveExt=usa` (`:10`). `Paths=../save/*.usa` (`:50`, lowercase `save`) — on a case-sensitive FS the directory is `Save`; the web ini must spell it `../Save/*.usa` and the port should read `SavePath` from the ini instead of the literal.

### 3.2 Files the player creates (all under `SaveSlotPath`, i.e. `Save/` or `Save/Slot<n>/`)

| File | Writer | Notes |
|------|--------|-------|
| `Save0.usa` | `UGameEngine::SaveGame(0)` (`UnGame.cpp:1967-1980`) from the tick queue (`:1772,1779` hard-code slot 0) | the save; `SAVEGAME <n>` console writes `Save<n>.usa` |
| `Save0<i>.usa` / `Game<i>.usa` | hub-stack copies (`:1990-2001`, `:696-735`) | only if `HubStackLevel>0` (`?pop` travel) — VERIFY whether HP2 ever pushes hubs |
| `GameSaveInfo<n>` (no extension) | `AActor::execSaveGameSaveInfo` (`UnScript.cpp:2792-2812`: `CreateFileWriter(SaveSlotPath * Dir)`) | level name + save-point id for the (unused) slot page |
| `Save0.bmp` | `SAVETHUMBNAIL` (`UnCamera.cpp:811-870`) after `Thumbnail(120,90,…)` | 120×90 BMP |
| `<Map>_pa.usa` | `ULevel::SavePersistentActors` → `SaveSlotPath/cache/<Map>_pa.usa` (`UnLevel.cpp:281-282`), copied up by `SavePersistentActorCache()` (`UnMisc.cpp:2072-2105`) after every save; `LoadPersistentActorCache()` (`:2108-2141`) copies them back into `cache/` at boot and before `LOADGAME` | the "persistent actors" mechanism (`TriggerChangeLevel` `SavePActors`); they validate against the **map file size and mtime** (`UnLevel.cpp:488-492`) — a served map whose mtime differs from the one recorded invalidates the `_pa` file (VERIFY on the web: FetchFS mtimes come from the manifest) |
| `Shot%04i.bmp` | `SSHOT` without a name → `appUserDir()` (`UnCamera.cpp:655-670`) = `System/` when the user dir is empty; with a name → `SaveSlotPath/<name>` | Insert key; not worth persisting |
| `Game.ini`, `User.ini`, `Game.log` | Core | `System/` (handled like the template's `UnrealTournament.ini`/`User.ini`) |
| `Running.ini`, `Detected.ini` | Win32 launcher only | gone |

### 3.3 `userFiles` for `targets/hp2/target.json`

The glue allows wildcards in the file name only (`docs/06-targets-and-branding.md:266-275`). Single-slot v1 (no `-SAVESLOT=`):

```json
"userFiles": ["Save/Save*.usa", "Save/Game*.usa", "Save/*_pa.usa", "Save/GameSaveInfo*", "Save/Save*.bmp", "Save/cache/*_pa.usa"]
```
If slots are exposed later, repeat the six entries for `Save/Slot1/` … `Save/Slot6/` (36 more globs) or teach the daemon a directory wildcard. `manifestExclude` must drop `Save/**` (the drop's `Save/` is empty apart from `SGS`, a 17 KB stray) and `*.usa`.

### 3.4 Quirks to carry into the design
- "New game" over a used slot: retail wipes the slot dir (`CleanSaveSlotDir`, `UnEngineWin.h:289-309`) before launching. On the web, "Play → PrivetDr" simply overwrites `Save0.usa` at the first save point; stale `<Map>_pa.usa` from an old game would survive — mirror the wipe in the launcher/Play panel for a "New game" preset (delete `Save/*` before boot) or accept it.
- Death without a save (`LoadGame 0` on a map launched directly, `Harry.uc:1965`) drops to Entry. A "Continue" only makes sense when `Save/Save0.usa` exists; the Play panel could grey it out by listing the user store.
- `PlayerPawn.Password` "for restarting coop savegames" (`PlayerPawn.uc:14`) — unused.

---

## 4. Localisation

- `Language=int` (`Default.ini:28`), forced from `Default.ini` on every boot (`UnMisc.cpp:1956-1964`). `GLanguage` default is `int` (`Core/Src/UnObj.cpp:24`).
- **.int discovery** (`UObject::CacheDrivers`, `UnObj.cpp:3716-3760`): for every `Paths=` entry, `FindFiles(<dir>/*.int)` then reads `[Public]` of `<file>.<lang>` falling back to `.int` — i.e. **every `System/*.int` is opened at boot** (25 files, 476 KB) → all in the precache; `fetchStubs: {"System": [".int"]}` as the template (the enumeration needs the stubs).
- **`Localize(Section, Key, Package)`** (`UnMisc.cpp:1222-1250`) opens `<Package>.<lang>` via the config cache, falls back to `<Package>.int`, else returns `<?int?Package.Section.Key?>` and logs `Localization: No localization: …` (suppressed by `Suppress=Localization`, `Default.ini:64`).
- Packages the scripts localise from (grep of `Localize(` in `HGame`, `UWindow`, `Engine` classes):

| Package | Uses | File in `Assets/System` | Status |
|---------|------|-------------------------|--------|
| `HPdialog` | 26 (subtitles for every spoken cue: `Actor.uc:695,741`, `HChar.uc`, `HPawn.uc`, `FEInGamePage.uc:130`) | `hpdialog.int` (157 KB, 1,930 lines) | ✓ |
| `HPMenu` | 7 (+ every `GetLocalFEString`, `baseFEPage.uc:11-14`) | `HPMenu.int` (28 KB) | ✓ |
| `Cutscenes\<File>` | `CutScene.uc:97-101`, `CutScriptDisk.uc:25-36` (`[thread_N] line_i=`) | `CUTSCENES/*.int` (185 files) | ✓ **but the path has a backslash and the directory is upper-case** (fact 4) |
| `SAPFont` | 8 (Asian languages only, `HPConsole.uc:920-927`) | `SAPFont.jap/kor/sim/tha/tra` | not needed for `int` |
| `Pickup` | 4 (`FEMainPage.uc:81` legal line; `HPConsole.uc:1244-1256` guarded) | **missing** | cosmetic; ship a stub (`[all] legal_title_01=…`, `find_wizard_text_01..`, `find_secret_text_01..`) |
| `HGame` | 2 (`HPConsole.uc:1224,1232`, guarded by `Left(text,1)!="<"`) | missing | fine |
| `Dobby` | 2 (`HPConsole.uc:1221,1241` level index/secrets — HP1 level names) | `Dobby.int` | ✓ (HP1 leftovers) |
| `BumpDialog`, `HPKeys`, `HPCredits`, `Core` | 2/1/1/2 | `BumpDialog.int`, `Hpkeys.int`, `hpcredits.int`, `Core.int` | ✓ |
| `ABC.INT` | `EkocTrigger.uc:7` (`[LevelEnable] Enable`) | `abc.int` | ✓ (quirky name) |
| Dialog line sets | `Duelset.int`, `QuidSet.int`, `BumpSet.int`, `HPDialogKeys.int` are read by `Localize` calls in `Characters`/`Quidditch` classes (random line pools) | present | ✓ |

- **Language-specific content in the drop** (all optional for an English build):
  - `Textures/MenuArt.<lang>_utx` ×22 + `MenuArt.utx` — resolved by `appFindPackageFile` trying `<name>.<lang>_utx` before `.utx` when `Language!=int` (`UnMisc.cpp:1448-1500`). **No script or map imports `MenuArt`** (grep) → likely an HP1 leftover; exclude the `*_utx` variants (≈9 MB), keep `MenuArt.utx` (578 KB) or drop it too after a VERIFY run.
  - `Help/splash<lang>.bmp`, `EALogo1.bmp`, `WBLegal.bmp`, `Background/button*.bmp` — Win32 launcher only → exclude `Help/**`.
  - `System/SAPFont.*`, `System/Simyou.ttf`, `Fonts/*.ttf` (gulim, MHEIMD, HGRGMSJ = CJK) — native-font path only → exclude.
  - `SystemLocalized/*.det|est|frt|itt` — **Unreal/UT-era files** (Bluff, Chizra, Nyleve, IpDrv, UnrealI…), nothing HP2 → exclude the folder.
  - `Source/HPDialog/classes/{Eng,Fre,Ger,Ita}Dialog.uc` — compile-time dialog tables; no `HPDialog.u` is shipped.
  - Cut scenes exist in English only (`CUTSCENES/*.int`).
- **Conclusion:** the web build ships `int` and nothing else is possible from this drop.

---

## 5. Fonts

| Font | Origin | Runtime need |
|------|--------|--------------|
| `HugeInkFont`, `BigInkFont`, `MedInkFont`, `SmallInkFont`, `TinyInkFont` (Times New Roman 24/18/14/12/10, ASCII, no AA) | baked into `HGame.u` by `#exec new TrueTypeFontFactory` (`HGame/Classes/baseHud.uc:14-18`); chosen for `INT`/`ENG` in `HPConsole.SetupLanguage` (`:988-994`) | none |
| `AsianFontHuge/Big/Med` (Gulim, `RenderNative=1`) | `baseHud.uc:20-22` | native-font path, Asian only |
| `FontHPMenuLarge`, `FontHPMenuMedium` (Times New Roman 18/14) | baked into `UWindow.u` (`UWindowRootWindow.uc:19-20`) | none |
| `UWindowFonts.Tahoma10/20/30`, `TahomaB10/20/30`, `UTFont12/24/40` | `Textures/UWindowFonts.utx` (1,755,486 B) loaded by `DynamicLoadObject` in `UWindowRootWindow.SetupFonts` (`:423-433`) for `INT` | **serve + precache** |
| `HPFonts.PolFont*` | not shipped (recipe only: `System/PolFont.exec`) | Polish only |
| `CreateNativeFont(name,height)` | `Console.uc:163` → `UnCon.cpp:88-98` → `UViewport::CreateNativeFont` (default `NULL`, `UnCamera.h:288`); Win32 GDI in `WinDrv/Src/WinViewport.cpp:870-1016` | only for SIM/CHI/TRA/KOR/THA/JAP (`HPConsole.uc:912-945`); NSDLDrv may return NULL |
| `Editor.UTrueTypeFontFactory` (`Editor/Src/TTFontImport.cpp`, GDI `CreateFont`) | import-time only | stub under NullEdHook |

No TrueType is loaded at runtime for English. Exclude `Fonts/**`, `System/*.ttf`, `System/SAPFont.*`, `System/PolFont.exec` from the manifest.

---

## 6. The boot working set (→ `targets/hp2/precache.json`)

Package versions: every shipped `.u` and all but two maps are **file version 79** (`Entry.unr`, `Greathall.unr` are 76); `PACKAGE_FILE_VERSION` is runtime-set (`Core/Inc/UnObjVer.h:37`). Dependency lists below come from parsing the import tables (script `updeps.py`, not `strings`).

### 6.1 Imports

| Package | Top-level imports |
|---------|-------------------|
| `Maps/Entry.unr` | `Editor Engine HGame HP2_Master UnrealShare` |
| `Maps/startup.unr` | `CVresearch Engine HGame HP2_Master HPSounds HP_Titles SkyBox UnrealShare` |
| `System/HGame.u` | `Core Engine Fire HPEdit HPModels HPParticle HPSounds HProps SpellShapes UWindow` |
| `System/HPParticle.u` | `3DWizardCards Core Engine Fire HPSounds` |
| `System/HProps.u` | `Core Engine HPModels` · `UWindow.u`: `Core Engine HPSounds` · `Editor.u`: `Core Editor Engine` · `Fire.u`, `UnrealShare.u`, `HPModels.u`, `HPSounds.u`: `Core Engine` |
| `Maps/PrivetDr.unr` (new game) | `CVresearch Engine GenFX HGame HP2_Master HPParticle HPSounds HP_Titles Intro_CS PrivetDrive SkyBox UnrealShare` |
| on first Esc | `HP_Menu.utx` via `DynamicLoadObject("HP_Menu.Hud.*")` (`FEInGamePage.uc:221-260`, `FEOptionsPage.uc:101-103`) |
| on first spoken line | `Sounds/AllDialog.uax` (`DynamicLoadObject("AllDialog."$cue)`) |

### 6.2 Sizes (bytes, from `Assets/`)

**Menu set (Entry + startup.unr):**

| File | Bytes |
|------|------:|
| System/Core.u | 61,685 |
| System/Engine.u | 1,454,448 |
| System/Editor.u | 126,401 |
| System/Fire.u | 14,926 |
| System/UWindow.u | 878,746 |
| System/UnrealShare.u | 6,877 |
| System/HGame.u | 19,888,599 |
| System/HPModels.u | 35,251,862 |
| System/HPParticle.u | 2,381,764 |
| System/HProps.u | 14,951,508 |
| System/HPSounds.u | 30,558,933 |
| **all `.u`** | **105,575,749** |
| System/*.int (25 files: BumpDialog 34,411 · BumpSet 63,780 · Core 3,239 · D3DDrv 374 · Dobby 1,724 · Duelset 4,050 · Editor 5,196 · Engine 5,455 · Galaxy 273 · Game 227 · HPDialogKeys 57,945 · HPMenu 27,954 · Hpkeys 71,197 · QuidSet 19,313 · Setup 7,236 · SglDrv 450 · SoftDrv 329 · Startup 6,299 · UMenu 335 · UnrealEd 38 · WinDrv 681 · Window 2,091 · abc 28 · hpcredits 6,513 · hpdialog 157,400) | 476,538 |
| Maps/Entry.unr | 6,626 |
| Maps/startup.unr | 83,009 |
| Textures/HP2_Master.utx | 18,988,237 |
| Textures/CVresearch.utx | 2,711,924 |
| Textures/HP_Titles.utx | 182,957 |
| Textures/SkyBox.utx | 4,480,754 |
| Textures/HPEdit.utx | 140,041 |
| Textures/SpellShapes.utx | 465,088 |
| Textures/3DWizardCards.utx | 2,926,870 |
| Textures/UWindowFonts.utx | 1,755,486 |
| Textures/HP_Menu.utx (first Esc) | 1,747,498 |
| Music/Music_StoryBook_v2.ogg | 1,381,107 |
| **menu total** | **≈ 140.9 MB** |

**First play (PrivetDr.unr) adds:** `Maps/PrivetDr.unr` 1,313,075 · `Textures/PrivetDrive.utx` 2,332,478 · `Textures/Intro_CS.utx` 997,748 · `Textures/GenFX.utx` 9,803 · `System/CUTSCENES/00001PrivetIntro.int` 11,504 + `00001PrivetIntroPart2.int` 9,601 · **`Sounds/AllDialog.uax` 97,423,956** · (the map's `Song` is `Music_temp_E3_cut_scene`, which does not exist in `Music/` — silent) → **≈ 102.1 MB**, i.e. **≈ 243 MB** for menu + intro. Recommend precaching the menu set and `AllDialog.uax`, and letting the map-specific textures/cut-scene ints stream.

Asset tree totals for the manifest budget: `Maps` (48 top-level) 81.0 MB (+ `Oldfiles/`, `Studies/`, `templates/` to exclude), `Textures` 72 files 132.4 MB, `Sounds` 29 files 128.0 MB, `Music` 111 files 61.2 MB, `System` 120 MB (of which `.u` 105.6 MB; the rest are DLL/EXE/BMP/TTF to exclude).

### 6.3 Manifest excludes (proposal)

```
*.dll *.exe *.bat *.dsp *.inf *.ico *.scc *.log
System/Default.ini System/DefUser.ini System/Game.ini System/User.ini
System/*.bmp System/*.ttf System/SAPFont.* System/PolFont.exec System/EditorRes/** System/*.dsp
Help/** Fonts/** DialogSources/** PackageSources/** SystemLocalized/**
Maps/Oldfiles/** Maps/Studies/** Maps/templates/**
Textures/MenuArt.*_utx
Save/** *.usa
```
Keep: `Textures/Palettes.utx` (CD-check probe target), `Textures/AOLDemo_HP2.utx` + `Maps/AOLDemo.unr` (a loadable demo level), `Music/Peeves_chase_v2` **renamed to `Peeves_chase_v2.ogg`** (the file has no extension; `PlayMusic` appends `.ogg` — `ALAudioSubsystem.cpp:1134-1140`).

### 6.4 Other `target.json` facts

- `assetMarkers.fingerprint`: `System/Core.u` sha256 `e0812a1931322d99d8b85db2f42fb2dacc8df33e919ea566a64e7a834e0d56e8` (61,685 B); `HGame.u` sha256 `6c996cd90d7841cba6ea1bc5edece67471307ff47fc6d856774106b01afc22a2`.
- `require`: `["System/HGame.u", "Maps/startup.unr", "Maps/PrivetDr.unr"]` (note the lower-case `startup.unr` on disk; `Default.ini` says `Startup.unr` — the manifest's case-insensitive view covers it).
- `fetchStubs`: `{"System": [".int"], "Maps": [".unr"]}` — `CacheDrivers` enumerates `System/*.int`; `GetMapName` (`ShortCutListLevels.uc:24-31`, debug F4) enumerates `../Maps/*.unr`. `System/CUTSCENES/*.int` are opened by name (no enumeration) — VERIFY FetchFS handles a sub-directory of a stubbed folder.
- `network.playerName`: `System/User.ini` `[DefaultPlayer] Name` (`DefUser.ini:2`), token `User`. `network.serverName`: `System/Game.ini` `[Engine.GameReplicationInfo] ServerName` (class exists: `Engine/Classes/GameReplicationInfo.uc:19`), stock `Another UT Server` (`Default.ini:197`).
- `telemetry`: `{ "browse": { "travel": ["^Log: Browse: (.*)$"], "load": ["^Log: LoadMap: (.*)$"] } }`, `{ "crash": ["^Critical: (?!appError called)(.*)$"] }`, plus HP2-specific `{ "save": ["^Log: Saving game\\.\\.\\. filename: (.*)$"] }`, `{ "loadgame": ["^Log: Loading game\\.\\.\\. filename: (.*)$"] }`, `{ "level": ["^ScriptLog: Changing level to:(.*)$"] }` — mark TO-VERIFY until a captured log confirms the exact spelling.
- `mods.extToFolder`: `.unr Maps · .utx Textures · .uax Sounds · .ogg Music · .u/.int/.ini System · .usa Save`.

---

## 7. Sound and music formats (evidence from bytes)

`USound::Serialize` (`Engine/Src/UnAudio.cpp:150-191`) writes: `FileType` (FName), `CoreFlags` (INT), `Duration` (FLOAT), `raw_NumSamples` (ver>76), `raw_BitsPerSample`, `raw_NumChannels` (ver>77), `raw_SampleRate` (ver>78), then the lazy `Data` array and, if `SF_HasLipSync` (0x80), `FLipSyncData`. Flags (`Engine/Inc/UnAudio.h:27-40`): `SF_Looping 2, SF_Streaming 4, SF_Music 8, SF_No3D 16, SF_UpdatePitch 32, SF_NoUpdates 64, SF_HasLipSync 128, SF_Compressed 256`.

| Package | Export | Header bytes (after the `None` property terminator) | Reading |
|---------|--------|------------------------------------------------------|---------|
| `Menu_sfx.uax` | `add_item_to_HUD` | `02 · 00000000 · 7cc6dc3e · 0 0 0 0 · <lazy> RIFF….WAVEfmt 10000000 0100 0100 22560000 44ac0000` | `CoreFlags=0`, raw fields 0, **payload = RIFF WAVE PCM, 1 ch, 22050 Hz, 44100 B/s → 16-bit** |
| `Ch1Rictusempra.uax` | `hidden_gear04` | `01 · 04010000 · 4779c93f · 93870000 · 10000000 · 01000000 · 22560000 · <lazy 18600> 27 00 0f dd ee de fe fe da cc ee e0 f1 01 1f 27 11 20 … 27 …` | `CoreFlags=0x104 = SF_Compressed|SF_Streaming`, 34,707 samples, 16-bit, mono, 22050 Hz; **payload = 15-byte frames, header nibble pair (coef idx 2, shift 7) → EA-XA ADPCM** |
| `Ch1Rictusempra.uax` | `booby_trap_reset` | … `25 00 00 01 f0 01 21 c2 1f 1e 04 fd 00 1e d3 26 2d bf 24 …` | same, headers `0x25`,`0x26` |
| `AllDialog.uax` | `pc_hry_gotoquid_01` | `01 · 84010000 · …` | `0x184 = SF_Compressed|SF_HasLipSync|SF_Streaming` — dialog carries lip-sync data after the payload |

Corroboration: `Assets/System/makesoundpackages.bat` builds every package with `ucc pkg import sound ..\Sounds\<Pkg> <src> compress [lipsync|nolipsync]`; `System/sx.exe` (548,864 B, EA "Sound eXchange") and `toolame.exe` sit beside it; the runtime decoder is the closed `coda` library (`Thirdparty/EAAudioCodec`, initialised in `Core/Src/UnVcWin32.cpp:1479-1486` only). Counts: `General.uax` 4 RIFF of 74 sounds, `AllDialog.uax` 77 RIFF of ~2,400, `Ch1Rictusempra.uax` 0 RIFF. The MP2 path (`Editor/Src/UnEdFact.cpp:966 SetupVarsForMP2`, `Thirdparty/MPeg`) is an import-side leftover, not what the shipped data uses.

**Action (Phase E):** decode `SF_Compressed` payloads with an EA-XA ADPCM decoder (vgmstream's `ea_xa_v2` / ffmpeg's EA ADPCM family: 1 header byte = coefficient index (high nibble, table `{0,240,460,392,0,0,-208,-220}`) + shift (low nibble), 14 bytes = 28 nibble samples, mono, no per-file header). VERIFY by decoding `hidden_gear04` with vgmstream after wrapping it in a minimal EA header, or by listening to the port. Keep `FLipSyncData` parsing (mouth animation reads it, `UnAudio.h`).

**Music:** `Music/*.ogg` (111 files, 61.2 MB), Ogg Vorbis streamed by `UALAudioSubsystem::PlayMusic` (`ALAudioSubsystem.cpp:1127-1160`): `Filename = "..\\music\\" + Song (+ ".ogg" unless the name already ends in ogg)`, `USound(Filename, SF_Streaming|SF_Music)`. `LevelInfo.Song` is a **string** (`LevelInfo.uc:71`), `NewMusicTrigger.Song` too (`NewMusicTrigger.uc:5,44` → `Actor.PlayMusic(string,float)` native `Actor.uc:1146`). Names in maps differ in case from the files (`Entryhall_dungeonhall_music.ogg` vs `EntryHall_DungeonHall_music.ogg`, `sm_scl_daywander_01.ogg` vs `sm_scl_DayWander_01.ogg`, `SM_SCL_StealthSneak_01_E.ogg` has no matching file) → the NOpenALDrv port must open `../Music/<name>` **case-insensitively**. There is no `.umx`; `Paths=../Music/*.umx` (`Default.ini:49`) is dead and can become `../Music/*.ogg` for manifest purposes.

**Dialog:** one package, `Sounds/AllDialog.uax` (97,423,956 B, ~2,400 cues named `PC_<Char>_<Scene>_<n>`), loaded on demand (§0 fact 6). Subtitles from `hpdialog.int` with `[Mood]` prefixes.

---

## 8. Input

### 8.1 Default bindings (`Config/System/DefUser.ini`)

| Key | Binding (`[Engine.Input]`) | Line |
|-----|----------------------------|------|
| LeftMouse | `AltFire \| bMouseAltFire \| Button bBroomBrake \| Button bVendorReply` — **cast: press to aim, release to cast** (`HPMenu.int` `Controls_0008..0011`) | 49 |
| RightMouse | `jump \| Button bBroomBoost \| Button bDuelCycleSpell` | 48 |
| MouseX / MouseY | `Axis aMouseX Speed=4.0 \| Axis aBroomYaw Speed=4.0` / `Axis aMouseY Speed=4.0 \| Axis aBroomPitch Speed=-4.0` | 105-106 |
| W/S/A/D, Up/Down/Left/Right | move / rotate (+ broom pitch/yaw, spell-lesson arrows) | 62-65, 118-136 |
| Ctrl | `Jump` (alias `Jump` = `Jump \| Axis aUp \| Button bBroomAction \| Button bSpellBallAction`) | 54, 15 |
| Alt, Slash | `AltFire \| Button bKeyAltFire` / `AltFire` | 55, 101 |
| 1 / 2 / 3 | duel spells `bDuelRictusempra / bDuelMimblewimble / bDuelExpelliarmus` | 69-71 |
| Q | `Button bDrinkWiggenwell` · Z `Button bBroomBrake` · Enter `LookToggle` · Comma/Period strafe | 131, 84, 52, 98-100 |
| **Escape** | **`quit`** (Input) — never reached: `HPConsole.KeyEvent` consumes `IK_Escape` on press (`HPConsole.uc:775-780`) | 58 |
| Joy1..4, JoyX/Y/U/V | Fire/Jump/AltFire/Duck; move + broom + look | 186-207 |
| `[Engine.PlayerPawn] Difficulty=DifficultyEasy`, `[HGame.Harry] bInvertBroomPitch/bAutoCenterCamera/bMoveWhileCasting/bAutoQuaff`, damage multipliers | 234, 240-247 |

Hard-coded in `HPConsole.KeyEvent` (not rebindable): Esc menu, `=` map, F4/F6/F7/F9/F11, Tab, Insert screenshot, Space skip/boost, PageUp/PageDown game speed, NumPad debug camera, Pause.

### 8.2 Gestures
`Engine/Src/UnGesture.cpp` implements `UGesture::CompareGesture/CompareGesturePoint` (natives 426/427, `Engine/Classes/Gesture.uc:13-14`) on a 3×3 segment grid — **no script calls them** (`grep -rn CompareGesture Source/HGame Source/Engine/Classes` → only the declaration). `Gesture` objects are used as `ParticleFX.Pattern` emission point sets (`ParticleFX.uc:105`) and `GestureSprite` draws the spell shape at the target (`SpellCursor.uc:84-148`). Casting = `AltFire` hold/release with the `SpellCursor` line-of-sight target (`SpellCursor.uc`). `Harry.uc:5310-5311` forwards `aMouseX/aMouseY` to the pawn for camera control only.

### 8.3 What the browser needs
- Pointer lock + raw relative deltas while the world is active (`Viewport.bShowWindowsMouse=False`); absolute cursor (`WindowsMouseX/Y` → `Root.MoveMouse`, `HPConsole.uc:1101-1105`) whenever a UWindow page, the vendor bar (`bVendorBar`) or the cut console is up (`bShowWindowsMouse=True`). This is the template's NSDLDrv contract (relative mode keyed on `bShowWindowsMouse`) — VERIFY that HP2's `UViewport` still has `bShowWindowsMouse`/`WindowsMouseX/Y` (`UnCamera.h`, PLAN B.3 already diffs it).
- `LaunchUWindow()` runs on the **first tick** (`HPConsole.uc:339-343`) and toggles the cursor on/off before any user gesture — pointer lock cannot be acquired until a click; harmless, just expect the first click to lock.
- Keep F4/F7/F11/Tab/PageUp/PageDown from the browser (F11 = quit-confirm in game; browsers use F11 for fullscreen). The frontend should `preventDefault` on F-keys while the canvas is focused.
- Mouse wheel unused (`MouseW=`, `MouseZ=`, wheel bindings empty).
- Gamepad optional (SDL joystick → `Joy*`).

---

## 9. Maps, travel and level options

### 9.1 The 48 top-level maps (`Assets/Maps`, sizes in §6.2 style available from `updeps`)

| Map | Role | Directly loadable? |
|-----|------|--------------------|
| `Entry.unr` (6.6 KB, v76) | engine entry level | engine only |
| `startup.unr` (83 KB) | title fly-through + `Music_StoryBook_v2.ogg` | yes (the no-map boot) |
| `PrivetDr.unr` (1.31 MB) | **chapter 1 / New Game**; cut scenes `00001PrivetIntro`, `00001PrivetIntroPart2` | **yes — the New Game target** |
| `FlyingFordCutScene.unr` (1.95 MB), `AdvFlyingFord.unr` (1.36 MB) | flying Ford Anglia cut scene / flight sequence (`00020FlyingFordIntro/End`) | yes |
| `Adv1Willow.unr` (1.90 MB) | Whomping Willow (`00030WhompCrash`, `00050WhompFree`) → `Grounds_Night.unr` | yes |
| `Grounds_Night.unr` (1.98 MB) | night grounds hub → `Adv8Forest`, `Entryhall_hub` | yes |
| `Entryhall_hub.unr` (4.97 MB) | main castle hub (20 `LevelStart_EntryHallHub_GState*` cut scenes) → Adv6Goyle, Adv7SlythComRoom, Arena, BeanRewardRoom, Ch1, Ch2, Ch4, Ch7, Credits, Grandstaircase_hub, Grounds_hub, Grounds_Night, Transition | yes, but see game states |
| `Grandstaircase_hub.unr` (4.67 MB) | upper castle hub → Adv11aCorridor, Ch1, Ch2, Ch4, Ch6, Entryhall_hub, Sepia_Hallway, Transition | yes, see game states |
| `Grounds_hub.unr` (3.18 MB) | grounds hub → Adv4Greenhouse, Ch3Diffindo, Entryhall_hub, Quidditch_Intro, Quidditch | yes, see game states |
| `Transition.unr` (769 KB) | hub-to-hub corridor (`01050TransitionA` … `17200TransitionJ`) | yes |
| `Ch1Rictusempra.unr` (3.96 MB) | Rictusempra challenge (DADA) | yes |
| `Ch2Skurge.unr` (5.09 MB) | Skurge challenge (Charms) | yes |
| `Adv3DungeonQuest.unr` (4.53 MB) | potion ingredient (dungeons) | yes |
| `Ch3Diffindo.unr` (4.77 MB) | Diffindo challenge (Herbology) | yes |
| `Adv4Greenhouse.unr` (1.06 MB) | greenhouse ingredient | yes |
| `Arena.unr` (503 KB) | Duelling club | yes |
| `Duel01.unr` … `Duel10.unr` (≈245 KB each) | wizard duels (opponent by number; unlocked by `lastUnlockedDuelist`, `FEDuelPage.uc:80-120`) | yes |
| `Adv6Goyle.unr` (5.16 MB) | Bit of Goyle (polyjuice) | yes |
| `Adv7SlythComRoom.unr` (3.42 MB) | Slytherin common room → `Transition` | yes |
| `Sepia_Hallway.unr` (438 KB) | Tom Riddle diary flashback | yes |
| `Ch4Spongify.unr` (3.83 MB) | Spongify challenge | yes |
| `Adv8Forest.unr` (2.28 MB) → `Adv9Aragog.unr` (400 KB) | forest / Aragog | yes |
| `Adv11aCorridor.unr` (3.03 MB) → `Adv11bSecrets.unr` (1.82 MB) → `Adv12Chamber.unr` (378 KB) | the Chamber | yes |
| `Credits.unr` (78 KB) | credits roll | yes |
| `Quidditch_Intro.unr` (382 KB), `Quidditch.unr` (1.52 MB) | Quidditch lesson / matches (`FEQuidPage`) | yes |
| `BeanRewardRoom.unr` (1.01 MB), `Ch6WizardCard.unr` (5.81 MB), `Ch7Gryffindor.unr` (2.27 MB) | bean bonus room, gold card challenge, Gryffindor challenge chamber | yes |
| `GreatHall_G/R/S/h.unr` (≈610 KB), `Greathall.unr` (200 KB, v76) | house-cup ceremony variants (per winning house), old great hall | yes (cut-scene rooms) |
| `AOLDemo.unr` (2.08 MB) | AOL demo level (needs `AOLDemo_HP2.utx`) | yes |

`Maps/Oldfiles/` (7 dev maps incl. `Demo_PC.unr`, `E3Demo.unr`), `Maps/Studies/` (44 test maps incl. `TestLevel.unr` that the "Test Level" preview button targets as `Studies\\TestLevel.unr`) and `Maps/templates/` (t3d/2ds) are excluded.

Story order inferred from the cut-scene numbering (`System/CUTSCENES/00001…18040`) and the hub graph: PrivetDr → FlyingFord → Adv1Willow → Grounds_Night → Entryhall_hub (GSTATE010/020) → Ch1Rictusempra → Ch2Skurge → Adv3DungeonQuest → Ch3Diffindo → Adv4Greenhouse → Arena/Duels → Adv6Goyle → Adv7SlythComRoom → Sepia_Hallway → Ch4Spongify → Adv8Forest → Adv9Aragog → Adv11a → Adv11b → Adv12Chamber → GreatHall_* → Credits, with Quidditch and the bonus rooms branching from the hubs. VERIFY the exact order in play.

### 9.2 Game states — why "open <hub>" is not the same as playing there
`PlayerPawn.CurrentGameState` (travel string, `PlayerPawn.uc:200`, default `None`, master list `GSTATE000…GSTATE180`, `:5145-5148`) is advanced by cut-script `CHANGEGAMESTATE` (`CutScript.uc:492`) and the console exec `SetGState <token>` (`HPConsole.uc:176`). On level load `ULevel::ScreenActorsByGameState` (`Engine/Src/UnLevel.cpp:150-240`) sets `bInCurrentGameState` on every actor that carries `GameStates` tokens (`ExcludeGameStates`, `Actor.uc:104,195`) and calls `OnResolveGameState`; `CutScene` actors not in the state go to `disabled` (`CutScene.uc:52-75`). Hubs carry one `LevelStart_<Hub>_GState<nnn>` cut scene per state. **Consequence:** `open Entryhall_hub` with `CurrentGameState=None` runs no story scripts (nothing matches) — the map loads and is walkable, but objectives are inert; challenge maps (`Ch*`, `Adv*`) are self-contained and play from the start. A "Maps" panel should therefore offer challenges/duels/Quidditch/bonus rooms as "directly playable" and hubs as "explore only", or pre-issue `SetGState GSTATE0xx` (VERIFY which token each hub expects: the `LevelStart_*` file names list them).

### 9.3 URL options and travel parsing
- `GameInfo.InitGame` (`Engine/Classes/GameInfo.uc:552-630`) parses `Difficulty`, `AdminPassword`, `GameSpeed`, `Mutator`, `GamePassword`, `LocalLog`, `WorldLog`; `Login` (`:849-854`) `Name`, `Password`, `Skin`, `Face`, `Checksum`; `Class` from `[DefaultPlayer]` (`HGame.harry`). `DefaultGame=Engine.GameInfo` (`Default.ini:33`); no `GameInfo` subclass exists in `HGame` → `Game=` must stay unset.
- `UGameEngine::Browse` (`UnGame.cpp:623-760`): `?failed`, `?entry`, `?pop`, `?restart`, **`?load=<n>`** (→ `<SaveSlotPath>/Save<n>.usa?load`).
- `LevelInfo.GetLocalURL()` (`LevelInfo.uc:158`) is used by `FEBook` (AUTOPLAY) and `FEMapPage.GetCurrentUnrFileName` (`:341-347`); the level identity the game persists is `LevelInfo.LevelEnterText` (`Harry.uc:985-993`, `HPConsole.uc:424-433`, `UnGame.cpp:492-497`) — each map's `LevelEnterText` is its own file name.
- `FEMainPage` level URLs are plain file names (`PrivetDr.unr`, `adv1Willow.unr`, … case varies); `FEMapPage.HarryLocInfo` (`:511-538`) maps 23 `.unr` names to map-page pins.

### 9.4 Proposal for the Play panel (`targets/hp2` `defaults`/`travel`)
```json
"defaults": { "map": "PrivetDr", "game": "", "travelOptions": "" },
"travel": {
  "modes": [],
  "toggles": [ { "label": "Medium difficulty", "set": "Difficulty=1", "tip": "GameInfo difficulty; Harry's damage scale is the in-game Options → Difficulty" } ],
  "presets": [
    { "label": "New game · Privet Drive", "set": "", "map": "PrivetDr", "tip": "Chapter 1. The engine auto-saves to Save/Save0.usa at save books." },
    { "label": "Continue (last auto-save)", "map": "", "args": "-LOAD=0", "tip": "Needs Save/Save0.usa." },
    { "label": "Duelling club", "map": "Arena" }, { "label": "Quidditch", "map": "Quidditch" },
    { "label": "Rictusempra challenge", "map": "Ch1Rictusempra" }, { "label": "Skurge challenge", "map": "Ch2Skurge" },
    { "label": "Diffindo challenge", "map": "Ch3Diffindo" }, { "label": "Spongify challenge", "map": "Ch4Spongify" },
    { "label": "Gold wizard card", "map": "Ch6WizardCard" }, { "label": "Gryffindor challenge", "map": "Ch7Gryffindor" }
  ],
  "mutators": []
}
```
`buildCmdLine` (`ufrontend/packages/frontend/src/config/cmdline.ts:48-60`) only emits `Map?Key=Val`; **engine switches (`-LOAD=0`, `-SAVESLOT=n`) need the Advanced engine-options path or a new `presets[].args` field** — the contract report must pick one. The character allow-list (`sanitizeCmdValue`) already admits `-` and `=`.

---

## 10. Mods, level packs, multiplayer (web search, 2026-09-26)

| What | Where | Needs | Worth a `conversions[]` entry? |
|------|-------|-------|--------------------------------|
| **ModDB addons** (23 addons, 242 MB): custom levels (`HP2 Custom Level 1 "secret"`, `Harry Potter II Custom Level` with playable Hermione, `BlizmedStairManager demo map`, `Harry Potter 2 Map AutoPlay2.Unr`), tools | https://www.moddb.com/games/harry-potter-and-the-chamber-of-secrets/addons (403 to scripted fetch; listing from search snippets) | stock engine for plain `.unr` levels; several need Jack's / Metallicafan212's editor to *build* | No — single `.unr` files are exactly what the mods drop-zone (`mods.extToFolder`) handles; not conversions. |
| **HP2 PC Extended Mod + "Cut & Restored" maps** (new/revised cut scenes, day/night hubs, new story segment; multi-language) | https://hp-games.net/mods-hp2-en (also Google Drive / Mega / archive.org mirrors) | **Metallicafan212's M212 engine** (DX11, 64-bit) — replaces `Game.exe`/dlls | Not now: it targets a different engine build; revisit only if its packages load on 433 (unlikely — it uses the M212 script decompile). |
| ~40 community level packs (Arachne Spider, CandyMaks, HPLabs challenge mods 380 MB–1.1 GB, David Kapitan "Autumn Adventures" 520 MB, Dmitry "Lemon" 16 challenge levels, …) | https://hp-games.net/mods-hp2-en , https://hp-games.net/mods-cs-en , https://hp-games.net/all-mods-en | mixed; the big ones are M212-era | Candidates for a later "level packs" catalogue once the stock engine runs; each is a set of `Maps/*.unr` + `Textures/*.utx` (a `conversions[]` entry per pack with its own precache list would fit the ut99-436 pattern). |
| **HP2 Co-op / Multiplayer mod** (MIT): two instances sync Harry over UDP with a custom text protocol (`HPCOOP`), ships a rebuilt `HGame.u`, `Host.bat`/`Join.bat`, `HP2Coop.ini` | https://github.com/jenyaalexanov/hp2-coop-multiplayer | M212 `Game.exe` + `UCC.exe` + **IpDrv.dll/IpDrv.u from M212** | Not directly (needs M212 + its IpDrv). Relevant as prior art: it shows the stock game has no replication for Harry and that a co-op needs script work; our WebRTCDrv + the 433-adapted IpDrv would be the transport it lacks on the web. |
| ModDB "Co-Op Mod" (second playable character with spells) | https://www.moddb.com/mods/co-op-mod/downloads | unknown engine (403) | Investigate after the web build runs. |
| HP2 ESRGAN Upscale Pack v4012022 (textures) | https://www.gamepressure.com/download/harry-potter-and-the-chamber-of-secrets-hp2-esrgan-upscale-pack/z513a88 | replaces `Textures/*.utx` (large) | No (bandwidth); could be an optional texture pack later. |
| Metallicafan212's HP2 engine/editor (v3.0C; DX11, HDR, widescreen, uncapped FPS, controller; Windows 10/11; closed, link via Google Doc), `HP2UScriptDecompile` (decompiled KW packages, needs M212), `M212-Editor-Issue-Tracker`, `HP2-RNG-Sim` | https://github.com/metallicafan212/HarryPotterUnrealWiki/wiki/Main-Resources , https://hp-games.net/metallicafan212-s-harry-potter-2-en , https://github.com/metallicafan212 | — | Reference only (closed engine). The community's compatibility bar is "runs on M212 or on retail 433"; we are the latter. |
| OldUnreal thread on HP2 (engine 433 Unicode, partially stripped script sources, editor.u present) | https://www.oldunreal.com/phpBB3/viewtopic.php?t=2228 | — | Background. |
| Wayback mirror of all ModDB HP files (2021) | https://archive.org/details/moddbdotcom-harry-potter-pc-games-mods-maps-in-the-waybackmachine | — | Source for offline testing of community maps. |

**Recommendation:** `conversions: []` for the first publish (as ut99 did, "empty by fact"); note the `autoplay.unr` convention in `branding/help.md`; plan a "community levels" catalogue after Phase D using plain `.unr` packs that load on 433.

---

## 11. Anti-piracy / CD checks

| Check | Code | Web port |
|-------|------|----------|
| SafeDisc API marker | `Source/Launch/Src/Launch.cpp:54-65,87-95`, `Source/Launch/Src/CdaPfn.h` | not compiled (Launch is reference only); the SDL launcher has none |
| CD path / "Insert CD" | `Core/Src/UnMisc.cpp:1980-1989` (`CDPATH=` / `[Engine.Engine] CdPath`, cleared when `..\Textures\Palettes.utx` exists), `UnEngineWin.h:1823-1845`, `Engine/Src/UnPawn.cpp:440-449` and `UnMisc.cpp:1458,1516` (`GCdPath` search fallback) | `CdPath` absent from `Default.ini` → `GCdPath=""`; leave the code; keep `Textures/Palettes.utx` |
| Demo lock-outs | `Core/Inc/UnBuild.h:75-82` `DEMOVERSION 0`, `HPDEMO 0`; `UnGame.cpp:918-965` map allow-lists; `Engine/Inc/UnEngine.h:46,75` | keep both 0 |
| Retail/demo file-size probes | `Launch.cpp:211-233` (commented out) | — |
| `UnrealIsRunning` mutex, `Running.ini` crash flag | `UnEngineWin.h:1668,1813`, `Launch.cpp:262` | drop |
| `AutoRun.inf` → `setup.exe` | `Assets/System/AutoRun.inf` | exclude |

Nothing checks a CD at package-load time; there is no CD-key.

---

## 12. Engine/launcher actions this analysis adds to the plan

- [ ] **Path normalisation (fact 4):** in the HP2 `FFileManagerLinux` (or a thin wrapper) convert `\`→`/` for `CreateFileReader/Writer`, `FileSize`, `Delete`, `MakeDirectory`, `FindFiles`, `GetGlobalTime`; add case-insensitive resolution for native runs (readdir walk, cached) — needed for `Cutscenes\<file>.int` (`CUTSCENES/`), `Studies\TestLevel.unr`, music names, `../Save`. On the web FetchFS supplies the case fold once the separator is fixed (`fetchfs/index.js:270-283`). The run-tree script can additionally symlink `System/Cutscenes → CUTSCENES` and `music → Music` as a belt-and-braces measure.
- [ ] **`appUserDir()` for Unix** (`Core/Inc/UnFile.h:235` declares it; only `UnVcWin32.cpp:1048` defines it): return `""` → `Game.ini`/`User.ini`/`Game.log` in `System/`; make `appInit` use the ini's `SavePath` (`../Save`) instead of `"..\\Save"` (`UnMisc.cpp:2011`).
- [ ] **Ini regeneration rule (fact 5):** drop the `GetGlobalTime(Default.ini) > GetGlobalTime(Game.ini)` and `bDefaultIniChanged` clauses (`UnMisc.cpp:1915-1921, 1929-1936`).
- [ ] **Web inis:** `[URL] LocalMap=Startup.unr` keep; `[Core.System] Paths=../Save/*.usa`, `../Music/*.ogg` (drop `*.umx`); `[HGame.baseConsole] bDebugMode=true` keep during bring-up (Tab console, F4 level list), flip to `false` for publish; `[Engine.Engine] EASplashWaitTime/WBSplashWaitTime` harmless; NSDLClient section must define `StartupFullscreen`, `FullscreenViewportX/Y`, `WindowedViewportX/Y`, `Brightness`, `TextureDetail`; NOpenALDrv section must define `MusicVolume` (0.5), `SoundVolume` (0.9), `Use3dHardware`, `Channels` (`appInit` reads `[ALAudio.ALAudioSubsystem] Channels` for `FFileStream::Init`, `UnMisc.cpp:1993-1996` — rename that lookup to the new section or keep a dummy `[ALAudio.ALAudioSubsystem] Channels=32`).
- [ ] **Execs the menus call** (§1.4): `GetRes`, `GetCurrentRes`, `GetColorDepths`, `GetCurrentColorDepth`, `SetRes`, `FLUSH`, `KEYNAME`, `KEYBINDING`, `RestartSounds`, `SAVETHUMBNAIL`, `SSHOT` — confirm NSDLViewport/NOpenALDrv answer them (return sane strings rather than nothing).
- [ ] **Precache/manifest/userFiles** as in §3.3, §6.3, §6.4; `AllDialog.uax` precached or prefetched.
- [ ] **`Pickup.int` stub** in `Config/System/web/` (copied into `System/` of the MEMFS boot set) — optional.
- [ ] **Rename `Music/Peeves_chase_v2` → `.ogg`** in the asset tree (document in `Assets/README.md`).
- [ ] **Phase B acceptance wording:** "boots to the main menu" → "boots to `startup.MyLevel` with `Creating root window` logged and Esc opening the in-game page"; the map to load-and-tick for 30 s: `Ch1Rictusempra` (self-contained) or `PrivetDr` (needs `AllDialog.uax` + cut-scene ints to be found — a good end-to-end path test).
- [ ] **PLAN §2/§7 audio correction:** EA-XA ADPCM, not Layer II.

---

## 13. Open questions (VERIFY on the first native boot)

1. Does `GConfig->SetString("Core.System","paths",…)` (`UnMisc.cpp:2063`) replace the last `Paths=` line (TMultiMap newest-first hash chain) or the first? Read the written `Game.ini`.
2. Which `Paths=` index is `GSys->Paths[5]` after the web ini edits (the code assumes the 6th entry is the `.usa` one, `:2006`).
3. Does anything load `MenuArt.utx`? (`grep` says no script does; a map might via `DynamicLoadObject` strings — check the log for `MenuArt`.)
4. Persistent-actor files validate the map's mtime (`UnLevel.cpp:492`): on the web FetchFS/OPFS mtimes may differ between sessions → `_pa` files rejected → hub state lost after reload. Test: save in a hub, reload the page, re-enter.
5. `HubStackLevel`/`?pop` — does HP2 push hubs (extra `Game<i>.usa` files) or is `HubStackLevel` always 0?
6. Exact tokens each hub's `LevelStart_*` cut scene expects, for a "start at hub with state X" preset.
7. FetchFS stubs for `System/CUTSCENES/*.int` (sub-directory of a stubbed folder).
8. `SetRes`/`GetRes` behaviour of NSDLViewport on the web (the Sound&Video page reverts after a timeout if `SetRes` "fails", `HPConsole.uc:508-516`).
