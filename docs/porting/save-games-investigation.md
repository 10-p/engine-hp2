# Save-game storage: HP2 vs Unreal 1 vs UT99 OldSkool (+ the inis)

**Owner request O1 (PLAN §11), investigation only — nothing is implemented here.** 2026-09-26.
Question: is there one pattern ("persist and restore everything under `Save/`", plus the derived inis) the web
frontend could share across the three UE1 games it serves?

## 0. Short answer

1. **All three games write player state in exactly two places**: the save directory, which is `/Save` on the web
   for all three, and `System/*.ini`. Nothing else a player makes needs to survive a reload.
2. **HP2 is the outlier in shape, not in place.** An HP2 slot is a **directory** (`Save/Slot<N>/`, picked with
   `-SAVESLOT=<N>`), holding `Save0.usa`, a `Save0.bmp` thumbnail and `<Map>_pa.usa` per-level files, plus an
   engine scratch directory `cache/`. In Unreal 1 and UT99/OldSkool a slot is a **number** (`Save/Save<n>.usa`) and
   the slot's display name lives in an ini.
3. **The common pattern exists: "mirror every file under `Save/`, recursively, except `cache/` directories and
   `*.tmp`, plus the engine's derived inis."** That covers all three. The descriptor cannot express it today,
   because the glue allows `*`/`?` only in the file name, so it needs a small glue change (§9 B).
4. **What the web frontend misses today is small.** The web launcher never passes `-SAVESLOT=`, so HP2 on the
   web saves flat into `/Save`, and `Save/*.usa` + `Save/*.bmp` already cover that (the save E2E is green). The gaps
   only appear with `-SAVESLOT=0` or `-SAVESLOT=7` and above, or with a future slot picker.
5. **An engine bug turned up along the way, and the frontend cannot work around it.** On Linux and the web,
   HP2's persistent-actor files never reach the slot. `FFileManagerLinux::FindFiles` cannot match the pattern
   `*_pa.*`, so per-level state (collected Jellybeans, secret areas) is lost after a reload or a `LOADGAME`. It
   was measured natively (§2.4). The Windows build is not affected.
6. **The inis already follow one naming rule.** Each game writes `System/<Module>.ini` from `Default.ini` (HP2
   `Game.ini`, U1 `Unreal.ini`, UT99 `UnrealTournament.ini`), plus `System/User.ini` from `DefUser.ini` in HP2 and
   UT99; Unreal 1 v200 has no User.ini. The frontend's rule of watching the literal `.ini` entries of
   `manifestExclude` covers all three. The remaining gaps are UT99-only: a conversion's `-ini=<X>.ini` and a mod's
   served ini.

## 1. Method and sources

- HP2: `Source/Core/Src/UnMisc.cpp` (`appInit` 2087-2150, `appSetSaveSlotDir` 1915, `Save/LoadPersistentActorCache`
  2158-2230), `Source/Engine/Src/UnGame.cpp` (Init `-LOAD=` 261, `SAVEGAME`/`LOADGAME`/`SAVEPACTORS` 460-516,
  `pop`/`load=` 711-775, `push` 1096, queued save 1923, `SaveGame` 2128), `UnLevel.cpp` (`Save/LoadPersistentActors`
  251/410), `UnCamera.cpp` (`SHOT`/`THUMBNAIL`/`SAVETHUMBNAIL` 670-830), `UnScript.cpp` (`SaveGameExists`,
  `Save/LoadGameSaveInfo`), `Core/Inc/FFileManagerLinux.h` (`FindFiles` 379), HGame/Engine script (`PlayerPawn.uc`
  `SaveGame`/`Thumbnail`, `Harry.uc`, `SavePoint.uc`, `CutScript.uc`, `TriggerChangeLevel.uc`, `HPConsole.uc`,
  `FEBook.uc`), the retail Windows launcher `Source/FrontEnd/Source/FE_Base.cpp` + `FrontEnd.h`,
  `Config/System/{,web/}Default.ini`, `Assets/Save`, `docs/porting/logs/native-first-boot.log`.
- Unreal 1: `/home/ion/engines/engine-unreal1` (v200; read only): `Engine/Src/UnGame.cpp`, `Core/Src/UnPlat.cpp`,
  `UnObj.cpp`, `UnFile.cpp`, `Source/Unreal/Classes/Unreal{Slot,Save,Load}Menu.uc`, `Engine/Config/{,web/}Default.ini`.
- UT99: `/home/ion/engines/engine-ut99` and `engine-ut99-436-compat` (read only): `UnGame.cpp`, `UnMisc.cpp`,
  `UnObj.cpp`, `Config/System/*.ini`.
- **OldSkool is present locally, but only as binaries**, in the 436 asset tree (`oldskool.u`, `olroot.u`, `olextras.u`,
  `oldskool.ini`; e.g. `/home/ion/native-servers/_agent/436-280/System/`, published as
  `/home/ion/publish/ut99-436/zstd/System/olroot.u.zst`). It is only on the `ut99-436-compat` target, not on the base
  `ut99` one. I read its UnrealScript source text (the packages carry it) with `strings`. **I did not run OldSkool
  myself**; the runtime facts come from ufrontend's 2.75 OldSkool E2E (`docs/migration/steps/2.75-save-games-persist.md`).
- Frontend: the ufrontend clone at `hp2` `2ae1b17` (read only). I read `targets/*/target.json`,
  `packages/glue/src/target/user-files.js`, `idb/user-data.js`, `idb/store.js` (restore), `fetchfs/wasmfs-backend.js`,
  `packages/frontend/vite.config.ts`, `src/shell/boot.ts`, `src/config/{iniSettings,editor,files}.ts`,
  `packages/e2e/lib/save-game-test.js`, `hp2-wasm-boot.spec.js`, `targets/hp2/README.md` and the 2.75 step doc.
- **Native runs** (HP2 native build, in a scratch copy `run-native-inv` of `run-native`, deleted afterwards; the
  original tree was not touched). Commands were given with `EXEC=<file>` (`Game/Src/Launch.cpp:790`), then `exit`:
  1. `./Game PrivetDr.unr -SAVESLOT=2 -SOFTWARERENDERER -NOMSGBOX -log EXEC=` {`savepactors`, `savegame 0`, `savegame 3`}
  2. `./Game -LOAD=0 -SAVESLOT=2 ...`, with `Slot2/cache/Bogus_pa.usa` and `Slot2/Fake_pa.usa` planted beforehand
  3. `./Game Ch1Rictusempra.unr -SAVESLOT=2 ... EXEC=` {`savepactors`, `savegame 0`}, then a plain reboot of slot 2

## 2. Harry Potter 2 (UE1 build 433, this port)

### 2.1 Where saves go

| | Windows 2k/XP (retail, and this port's Windows build) | Win9x / no personal folder | Linux native + web (this port) |
|---|---|---|---|
| `appUserDir()` | `My Documents\Harry Potter and the Chamber of Secrets\` | `""` | `""` (`UnUnix.cpp:914`) |
| `SavePath` | `<userdir>\Save` | `..\Save` | `../Save` → web `/Save` |
| `SaveSlotPath` | `SavePath` without `-SAVESLOT=`, else `SavePath\Slot<N>` | same | same |
| `Game.ini` / `User.ini` | `<userdir>\Game.ini`, `<userdir>\User.ini` | `System\` | `System/` |

- **`SavePath` is hard-coded in `appInit`**, so `[Core.System] SavePath=` in `Default.ini` is never used. `appInit` then
  writes the slot path back into `Game.ini`, measured with `-SAVESLOT=2`: `SavePath=../Save/Slot2`, and the
  **last** `Paths=` becomes `../Save/Slot2/*.usa` (the retail value is `../save/*.usa`, lowercase). That is why the web
  `Default.ini` keeps exactly six `Paths=` (`Config/System/web/README.md`).
- At every boot `appInit` creates `SavePath`, `SaveSlotPath` and `SaveSlotPath/cache` (one mkdir each, in order), then
  calls `LoadPersistentActorCache()`.
- **The retail slot model lives outside the engine**, in the Windows launcher `Source/FrontEnd` (not ported):
  - It lists `Save\Slot1`..`Slot5`: `NUM_SAVE_SLOTS 6`, with the loop starting at 1.
  - A slot counts as used when `Slot<N>\Save0.usa` has a size above 0, and it shows `Slot<N>\Save0.bmp`.
  - **New Game** runs `EmptySaveSlotFiles` ("we can't have any pa files lingering"), which deletes every file in the
    slot, then launches `game.exe PrivetDr.unr -SAVESLOT=N`.
  - **Continue** launches `game.exe -LOAD=0 -SAVESLOT=N`.

  `PlayerPawn.uc` says the same: "slots are just directories, the save game Number that the front end will always
  look for is zero". The in-engine menu book has no slot page: `FESlotPage` is referenced in `FEBook.uc` but exists in
  neither the source nor the retail `HGame.u`, so `SlotPage` stays `None`.
- **On the web** the launcher passes no `-SAVESLOT=`, so everything lands flat in `/Save`: one slot.

### 2.2 The files

| File | Directory | Written by | When | Read by |
|---|---|---|---|---|
| `Save<n>.usa` (n is 0 on every game path) | `SaveSlotPath` | C++ `UGameEngine::SaveGame` → `SavePackage`, via `Save.tmp` + rename | Script: SavePoint touch, a level start with `bDoLevelSave`, CutScript `SAVEGAME` → `Harry.SaveGame(0)` → `PlayerPawn.SaveGame` sets `bQueuedToSaveGame` → the engine saves `Save0` at the end of that tick. Also the console `SAVEGAME <n>` | `LOADGAME <n>`, `-LOAD=<n>`, death (`Harry.uc`: "Always load save0.usa"): a ServerTravel to the file; also `?load=<n>` |
| `Save<n>.bmp` (120×90, 24-bit, 32,454 B) | `SaveSlotPath` | C++ `SAVETHUMBNAIL` (after `THUMBNAIL x=120 y=90`) | Script `PlayerPawn.SaveGame` → `Thumbnail()`, just before the queued save. **The console `SAVEGAME` makes none** | Only the retail Windows FrontEnd; the engine never reads it |
| `<Map>_pa.usa` (e.g. 61,012 B for Ch1Rictusempra) | written to `SaveSlotPath/cache/`; copied to `SaveSlotPath/` on save | C++ `ULevel::SavePersistentActors` (console `SAVEPACTORS`); the copy is `SavePersistentActorCache` at the end of `SaveGame` | Script `TriggerChangeLevel` issues `SavePActors` when leaving a level | Every non-Entry `LoadMap` reads `cache/<Map>_pa.usa` and checks the map's size + `GetGlobalTime` (0 on GCC). At boot and on `LOADGAME`, `LoadPersistentActorCache` wipes `cache/*.*` and copies `SaveSlotPath/*_pa.*` into it |
| `Save<n><i>.usa`, `Game<i>.usa` | `SaveSlotPath` | C++ hub stack (stock) | `?push` travel, save and load | `?pop`, `?load=`. HP2 maps are not seen to push; kept for completeness |
| `Save.tmp` | next to the target | C++ `SavePackage` | during a save; renamed at the end | — (HP2 and UT99 never clean up a stray one; U1 does at init) |
| `GameSaveInfo<i>` (no extension) | `SaveSlotPath` | script native `SaveGameSaveInfo` | only from `HPConsole.doLevelSave`, whose one caller is commented out: **dead** | `LoadGameSaveInfo`, also unused |
| `Shot####.bmp` | `appUserDir()`, i.e. `System/` on Unix and the web | `SHOT` | the player's screenshot key | — (not a save; `SHOT <name>` without the number writes into `SaveSlotPath`) |

`Assets/Save/SGS lev3_troll.unr.bmp` is a retail leftover that no current code path writes. It is not served
(`manifestExclude` `Save/**`).

### 2.3 Measured natively (2026-09-26)

- **Run 1 (PrivetDr, slot 2):** `Save/Slot2/Save0.usa` and `Save3.usa`, 824,222 B each. The log shows
  `Moving '../Save/Slot2/Save.tmp' to '../Save/Slot2/Save0.usa'`. `Save/Slot2/cache/` stays empty (PrivetDr has no
  `bPersistent` actor). `Game.ini` was rewritten as described in §2.1.
- **Run 2 (`-LOAD=0 -SAVESLOT=2`):** the log shows `Loading game... filename: ../Save/Slot2/Save0.usa`, then
  `LoadMap: ../Save/Slot2/Save0.usa?Name=Player?...`, then `Possessed PlayerPawn: harry Save0.Harry1`. The planted
  `cache/Bogus_pa.usa` was **deleted**, so the boot wipe works. The planted `Slot2/Fake_pa.usa` was **not copied**
  into `cache/`, which is the bug in §2.4. When a save loads, the pa lookup uses the save's own name
  (`cache/Save0.usa_pa.usa`, "file is probably missing"): a save carries its level state inside itself.
- **Run 3 (Ch1Rictusempra, slot 2):** `SAVEPACTORS` wrote `cache/Ch1Rictusempra_pa.usa` (61,012 B), and
  `Save0.usa` came out at 2,699,121 B. **No `Slot2/Ch1Rictusempra_pa.usa` was produced.** After a plain reboot,
  `cache/` was empty again, so the level's persistent state is gone.
- Side note, not investigated: an exec file that ran `thumbnail`/`savethumbnail` **before the first frame**
  segfaulted. The in-game path always has a frame; the console path is an edge case.

### 2.4 Engine bug: `*_pa.*` never matches on Linux or the web — **FIXED in `79b21ff`**

> Status 2026-09-26: `FindFiles` now does a real case-insensitive `*`/`?` glob (Win32 `FindFirstFile`
> semantics). Verified natively: a slot's `Test_pa.usa` is copied into `cache/` at boot and back on `savegame`.
> The "New Game must empty the slot" note below still applies once slots are used on the web.


`FFileManagerLinux::FindFiles` (`Core/Inc/FFileManagerLinux.h:379`, inherited verbatim from engine-ut99's) handles
`*`, `*.*`, `*.ext`, `name.*` and `a*b`. A pattern that starts with `*` takes the `*.ext` branch, so `*_pa.*` becomes
`strstr(name, "_pa.*")`, which contains a literal `*` and never matches. Both copies in `UnMisc.cpp` are therefore
no-ops on Linux and on the web (the launcher `Game/Src/Launch.cpp:170` uses `FFileManagerLinux`):

- the slot→cache copy at boot and on `LOADGAME`;
- the cache→slot copy on save, including the delete of stale slot pa files.

The cache wipe uses the pattern `*.*`, which does work. As a result the `bPersistent` actors (`Jellybean`,
`SecretAreaMarker`, `ChallengeScoreManager`) remember their state within one session only. The Windows build uses
`FindFirstFile` and is correct. The fix belongs in the engine: a real wildcard match in `FindFiles`, or `*_pa.usa`,
which the `*.ext` branch would accept. **No frontend rule can compensate**, because the files are never created.
Once the engine is fixed:

- `Save/*.usa` already catches the slot's `<Map>_pa.usa`.
- **A "New Game" must empty the slot**, as the retail FrontEnd's `EmptySaveSlotFiles` does. Otherwise the boot copy
  would bring the previous game's per-level state into the new game. On the web that means the one flat `/Save`.

### 2.5 The inis

`Game.ini` (the module name is forced to `Game`) and `User.ini` are created from `Default.ini`/`DefUser.ini`, which
come from the engine's preloaded `Config/System/web` set. `UCC` uses `HP.ini`, which never runs on the web. HP2 adds
one rule to the template: it regenerates both inis when `Default.ini` is newer, and on GCC this is inert because
`GetGlobalTime` is 0. The only thing HP2 rewrites at every boot is `SavePath` and the last `Paths=` (§2.1).
**The naming follows `<Module>.ini` + `User.ini`.**

### 2.6 What the frontend persists today, and what it would miss

- **`userFiles`** (14 globs): `Save/*.usa`, `Save/*.bmp`, `Save/Slot1..6/*.usa`, `Save/Slot1..6/*.bmp`.
- **Watched inis:** `/System/Default.ini`, `/System/DefUser.ini`, `/System/Game.ini`, `/System/User.ini`. The list
  also picks up `/desktop.ini`, a literal `.ini` in `manifestExclude`; that is harmless.
- **Kept on the web today:** `/Save/Save0.usa` (E2E-verified) and `/Save/Save0.bmp` (the glob exists, but the E2E
  saves from the console, which makes no thumbnail). The globs would also keep `/Save/*_pa.usa`, but the engine
  never writes it (§2.4).
- **Would miss:**
  - `Save/Slot0/` and `Save/Slot7+/`, i.e. any `-SAVESLOT=` outside 1..6. The retail slots are 1..5, so `Slot6` is
    one directory more than needed.
  - Any extension other than `.usa`/`.bmp` inside a slot (only the dead `GameSaveInfo<i>`).
  - `Save/cache/` and `Save/Slot<N>/cache/`, which are skipped **by design**.
- **Correction to the descriptor comment.** `_userFilesComment` says persisting `cache/` "would resurrect unsaved
  progress". It would not: the engine wipes `cache/*.*` at every boot and on `LOADGAME` (verified in run 2).
  Persisting it would only cost write churn (one ~60 KB write per level exit), a restore, then a delete. Skipping it
  is still the right call.

## 3. Unreal 1 (`engine-unreal1`, v200)

- **Path:** `[Core.System] SavePath=..\Save` (`Engine/Config/web/Default.ini:50`) → `/Save`. It is flat: no slot
  directories, no thumbnails, no caches.
- **Files:**
  - `Save<n>.usa` for n = 0..8 (`UnrealSaveMenu`: `DelayedCommand = "SaveGame "$(Selection-1)`, 9 slots), written
    through `Save.tmp` + rename (`UnObj.cpp:2116`). A stray `Save.tmp` is removed at init (`UnPlat.cpp:585`).
  - The hub stack: `Save<n><i>.usa` and `Game<i>.usa`.
- **Who writes what:** C++ `UGameEngine::SaveGame` (`UnGame.cpp:1334`) writes the file; the script menu or the
  console asks for it. The slot **names** come from the script: `UnrealSaveMenu.SaveConfig()` writes
  `[UnrealI.UnrealSlotMenu] SlotNames[0..8]` (globalconfig) into `Unreal.ini`.
- **Load:** `?load=<n>` (`UnrealLoadMenu`: `ClientTravel`, or `ServerTravel` for "Net:" slots) →
  `LoadMap(<SavePath>\Save<n>.usa?load)`, then the hub-stack copy `Save<n><i>` → `Game<i>` and the removal of leftover
  `Game<i>`. The load list comes from the ini slot names, not from the directory.
- **Inis:** one only, `Unreal.ini` (`appPackage()+".ini"`, copied from `Default.ini`). **v200 has no User.ini**: the
  `User`/`System` override tokens both mean the default ini (`UnFile.cpp:1507`). It follows `<Module>.ini` but not the
  pair.
- **Frontend:** `userFiles` `Save/*.usa` covers every file the engine writes (`Save.tmp` is excluded by its
  extension). The watched inis are `Default`, `DefUser`, `User` (never written), `Unreal`. The E2E covers the console
  and the menu, and the slot name travels with the save. **Nothing is missed.**

## 4. UT99 and OldSkool

- **Engine** (both UT99 trees): `SavePath=../Save` from the ini. `SaveGame` creates the directory tree
  (`UnGame.cpp:1824`, for Spatial Fear's `../spatial-fear/save`) and saves through `Save.tmp` + rename
  (`UnObj.cpp:2583`). `?load=` gets the Unix URL fix (base `5674fc7`). There are no slot directories, thumbnails or pa
  files. The inis are `UnrealTournament.ini` + `User.ini`, overridable with `-ini=` / `-userini=`
  (`UnMisc.cpp:1797-1830`).
- **OldSkool** is script only, and not verified at runtime by me:
  - Slots are numbers: 50 map packs × 20 slots, and `OldSkoolSaveGameClientWindow` issues `SaveGame <0..999>`.
  - Quicksave: `mutate quicksave` → `SaveGame 1000` (`Save1000.usa`). Loads use `?load=<n>` / `?load=1000`.
  - Names: `olroot.OldSkoolSlotClientWindow` is `config(oldskool)` with globalconfig `SlotNames[1000]`,
    `Packsaves[50]` and `quicksavetype`, all in `System/oldskool.ini`. **That file is served**, which is why
    ufrontend detaches it from the on-demand backend.
  - OldSkool also rewrites `[Core.System] SavePath` from `..\save` to `..\Save` (and `:save` → `:Save`). The mod
    itself makes `Save` the canonical spelling.
- **Conversions** (`ut99-436-compat` only): `-ini=Infiltration.ini`, `7Bullets.ini`, `spatialfear.ini`, `TeamOrbit.ini`.
  For that session the "derived" ini is a **served** `System/<X>.ini`, and nothing persists it (2.75 §8 "Open").
  Spatial Fear saves under `spatial-fear/save/` (lowercase).
- **Frontend:** `ut99` has `Save/*.usa`. `ut99-436-compat` has `Save/*.usa`, `spatial-fear/save/*.usa` and
  `System/oldskool.ini`. It misses only the conversion inis and Spatial Fear's user ini (both already listed as open
  in 2.75 §8).

## 5. The frontend mechanism, as it is (ufrontend `hp2` `2ae1b17`)

- **The `userFiles` grammar** (`glue/src/target/user-files.js`):
  - paths are relative to the VFS root, and **`*`/`?` are allowed in the last segment only**;
  - `.` and `..` segments are rejected;
  - the **directory is matched exactly, case-sensitively**; the file name is matched case-insensitively;
  - an invalid entry throws.
- **The daemon** (`idb/user-data.js`):
  - `watch()` splits literal paths from globs. Literal files are hashed on a 30 s clock.
  - Glob directories are listed every 3 s with `FetchFS.originalReaddir` + `stat`. A file's identity is
    `ino:size:mtime`, and a file is kept only once it has been quiet for about 2 s.
  - A deletion needs the file's own `stat` to fail inside a directory that still exists.
  - A change found by the scan runs the whole cycle, including the engine's `flushconfig`. The exit hooks are best
    effort.
- **Restore** (`idb/store.js`):
  - It writes **every** row of `<prefix>-user-data` back at boot, not only the rows today's globs match.
  - It creates parent directories itself (mkdir -p), replaces files with `O_TRUNC`, and runs twice (before and after
    `FetchFS.init`).
  - **It is eager:** every stored byte lands in the in-memory VFS.
- **Served files:** a served file that a `userFiles` entry names is kept on the default backend
  (`wasmfs-backend.js`), so that engine writes into it are visible.
- **Inis:**
  - `engineFiles` is the literal `.ini` entries of `manifestExclude` (`vite.config.ts:37`); these are watched.
  - Launcher settings reach the engine as `-ini:<File>:[Section]:Key=Value` seeds, with the generic tokens `System`
    and `User` (`iniSettings.ts`, `controller.ts`; engine side `appApplyIniOverrides`).
  - The ini editor (`config/editor.ts`) lists `.ini`/`.log` files from the manifest, the engine layer and the stores,
    and saves edits into user-data, which outranks everything at restore.
- **The E2E** (`lib/save-game-test.js`): fresh profile, save path absent, the engine saves, the daemon stores it, the
  tab is closed, restore alone brings it back, and the engine loads it. For hp2 that is `savegame 0` in
  Ch1Rictusempra → `/Save/Save0.usa` → `loadgame 0` from PrivetDr.

## 6. Side by side

| | HP2 | Unreal 1 (v200) | UT99 / OldSkool |
|---|---|---|---|
| Save dir (web) | `/Save`, or `/Save/Slot<N>` with `-SAVESLOT=` | `/Save` | `/Save` (Spatial Fear: `/spatial-fear/save`) |
| Where `SavePath` comes from | hard-coded in `appInit` | ini `[Core.System]` | ini `[Core.System]` (OldSkool fixes the case) |
| What a slot is | a directory | a number, 0..8 | a number, 0..999, plus quicksave 1000 |
| Files | `Save0.usa`, `Save0.bmp`, `<Map>_pa.usa`, `cache/` | `Save<n>.usa`, hub `Save<n><i>`/`Game<i>` | `Save<n>.usa`, hub files |
| Temporary | `Save.tmp`, `cache/*` | `Save.tmp` (cleaned at init) | `Save.tmp` |
| Who knows the slot list | the directory (the FrontEnd probes `Save0.usa`) | `Unreal.ini` SlotNames | `oldskool.ini` SlotNames/Packsaves |
| Derived inis | `System/Game.ini` + `System/User.ini` | `System/Unreal.ini` | `System/UnrealTournament.ini` + `System/User.ini` (or the `-ini=` file) |
| Frontend `userFiles` | 14 per-directory globs | `Save/*.usa` | `Save/*.usa` (+ `spatial-fear/save/*.usa`, `System/oldskool.ini`) |
| Missed today | `Slot0`, `Slot7+`; pa files (engine bug) | nothing | conversion inis (known open item) |

## 7. The common pattern

**Every UE1 game here writes player state only under its save directory and into `System/*.ini`.** A rule that
covers all three is:

> **Mirror every regular file under `Save/`, recursively. Skip directories named `cache` and files named `*.tmp`.
> Keep watching `System/<Module>.ini` + `System/User.ini`, and any mod ini named by hand.**

- It is **correct for all three**:
  - it reduces to `Save/*.usa` for U1/UT99;
  - it takes HP2's slot directories whatever `N` is;
  - it keeps thumbnails, and pa files once the engine is fixed;
  - it removes the need for the per-slot list.
- The **slot names still travel with the save**, because a save sets off a full cycle that flushes and stores the
  inis too (already true today).
- The directory really is **"what exists"**, so a launcher-side save list (an HP2 slot picker in the spirit of the
  retail FrontEnd: `Slot<N>/Save0.usa` + `Save0.bmp`) could be drawn straight from user-data. For U1 and OldSkool the
  display names would still come from their inis. That is an idea only, outside this report.

## 8. Risks

1. **Size and memory.** Restore is eager, so every stored save is written into the wasm VFS (RAM) at boot, whether or
   not it is ever loaded:
   - HP2: about 1–3 MB per save + 32 KB thumbnail + about 60 KB per visited level, × 5 slots → roughly 20 MB;
   - Unreal 1: 9 × 1–9 MB;
   - OldSkool: up to 1,001 × 1–9 MB, which can reach GBs against a 2 GB memfs budget.

   The user-data store is never evicted and has no quota policy, so browser origin quota applies. The recursive rule
   does not make U1/UT99 worse (their directory is flat already). It is the OldSkool scale that needs a lazy restore,
   or a cap.
2. **Temporary files.**
   - `Save.tmp` is renamed within the save; a crash leaves it behind, and HP2/UT99 never clean it up. Exclude `*.tmp`.
   - HP2's `cache/` is engine scratch, wiped at every boot, so persisting it is harmless but churny. Skip it.
   - Hub-stack `Game<i>.usa` belongs to the running game and is harmless to keep. The daemon already mirrors the
     engine's own deletion of it.
3. **Directories.** Restore creates parent directories itself, so nested slots are fine; empty directories are not
   stored and do not need to be. HP2 recreates `Save`, `Slot<N>` and `Slot<N>/cache` at every boot. The deletion proof
   must stay per file ("its own `stat` is gone, in a directory that exists") at every level of a recursive walk.
4. **Case.** The VFS is case-sensitive and the glob directory is matched exactly. All three canonical spellings are
   `Save`:
   - HP2 hard-codes it and also folds case engine-side (`FixPath` resolves each component case-insensitively);
   - U1/UT99 take it from the ini, and OldSkool normalises it.

   The exceptions are data: Spatial Fear's `spatial-fear/save`, and the retail HP2 ini's `Paths=../save/*.usa`
   (a search path, not a write target). HP2's `Save/cache` must not be confused with UE1's `CachePath=../Cache`
   (network downloads, their own `-network-cache` store).
5. **Glue restrictions.** The glue allows no directory globs (`*`/`?` in the file name only). `FS.readdir` through
   FetchFS answers `['.','..']` for a missing directory, and WasmFS lists only materialised nodes; a recursive walk
   must keep using `originalReaddir` + `stat`. A served file under a recursive entry would be detached, which costs
   one synchronous download each; today nothing under `Save/` is served on any target.
6. **The quiet-second rule.** A save the player closes the tab on within about 2–3 s is lost (by design, unchanged).
7. **HP2 "New Game".** Once the engine bug is fixed, a new game on the web inherits the previous game's pa files,
   unless the launcher or the engine empties the slot, as retail does (§2.4).

## 9. Recommendation (not implemented)

**A. Descriptor-only, available today.**
- `hp2`:
  - add `Save/Slot0/*.usa` and `Save/Slot0/*.bmp` if `-SAVESLOT=0` should work;
  - `Slot6` can stay (harmless) or go (retail uses 1..5);
  - reword `_userFilesComment` about `cache/` (§2.6).
- `unreal1`, `ut99`: nothing to do.
- The descriptor cannot say "all of `Save/`": that needs B.

**B. Glue-level: the common pattern.** This is the minimal change, and it keeps the "DATA, not code" rule:
1. `user-files.js`: allow **one trailing `/**`** meaning "every file under this directory, recursively"
   (`Save/**`), and a sibling key such as `"userFilesSkip": {"dirs": ["cache"], "names": ["*.tmp"]}`. Directory names
   would match case-insensitively. Everything else would still throw.
2. `matchesUserFile` / `matchOne`: for a recursive entry, match when the directory is the entry's directory or lies
   under it (`dir === c.dir || dir.startsWith(c.dir + '/')`), then apply the skip rules.
3. `scanUserFiles`: walk the subdirectories with `originalReaddir` + `stat`, with a bounded depth (e.g. 4), and do not
   descend into skipped directories. The per-file identity and deletion rules stay unchanged.
4. Tests: glue unit tests for the grammar; the existing four save E2Es unchanged; an hp2 run with `-SAVESLOT=3`.

The descriptors would then read:

| Target | `userFiles` | `userFilesSkip` |
|---|---|---|
| `hp2` | `["Save/**"]` | `{"dirs": ["cache"], "names": ["*.tmp"]}` |
| `unreal1`, `ut99` | `["Save/**"]` | `{"names": ["*.tmp"]}` |
| `ut99-436-compat` | `["Save/**", "spatial-fear/save/**", "System/oldskool.ini"]` | `{"names": ["*.tmp"]}` |

Optional, for OldSkool scale: a lazy or budgeted restore of `userFiles` rows.

**C. Inis.** Keep the current convention: literal `System/<Module>.ini` and `System/User.ini` in `manifestExclude`,
which the watch list is derived from. All three already follow it; Unreal 1's listed `User.ini` is harmless. Two
gaps remain, both UT99-436:
- A conversion's `-ini=<X>.ini` session ini is not persisted. A frontend change could add the active conversion's
  `-ini=`/`-userini=` file (parsed from `conversions[].param`) to `watch()` and to the served-file detach at launch.
- Mod inis stay hand-named in `userFiles`, as `oldskool.ini` is.

Do **not** widen this to `System/*.ini`: the 436 target serves dozens of inis, and each would become a synchronous
boot download.

**D. HP2 engine follow-ups** (PLAN §11 candidates, not frontend):
1. Wildcard matching in `FFileManagerLinux::FindFiles`, or the `*_pa.usa` pattern (§2.4).
2. Once (1) is done, an equivalent of the retail New-Game slot wipe.
3. Optionally, a guard on `THUMBNAIL`/`SAVETHUMBNAIL` before the first frame.

## 10. Loading a save at startup (checked 2026-09-26)

| Engine | Startup form | Evidence |
|---|---|---|
| HP2 | `Game ?load=<n>` — straight into `<SaveSlotPath>/Save<n>.usa` | **run natively**: `LoadMap: ../Save/Save0.usa?load` → `Possessed PlayerPawn: harry Save0.Harry0` |
| HP2 | `Game -LOAD=<n>` (retail "Continue"; with `-SAVESLOT=<k>` for a slot folder) — boots the title level, then travels into the save | **run natively**: `Loading game... filename: ../Save/Save0.usa` → `Possessed … Save0.Harry0` |
| Unreal 1 | `Unreal ?load=<n>` | code only (tree is read-only here): the first token starts with `?` so `UGameEngine::Init` takes it as the URL, and `Browse`'s `load=` branch (`UnGame.cpp:424`) loads `Save<n>.usa` |
| UT99 / OldSkool / 436 | `UnrealTournament ?load=<n>` | code only: same `Browse` branch (`UnGame.cpp:589` / `:1026`); `appIsGlobalOptionToken` returns false for a token starting with `?`. The frontend's 2.75 E2E proves the in-session `open ?load=0` |
| UT2004 | n/a | the engine keeps `SAVEGAME`/`?load=` from UE2, but UT2004 ships no saved games — single-player progress is profiles (`Saves/*.uvx`), chosen in the menu |

**On the web** (the generic launcher, so the same for every target): page-URL parameters become travel options, but
**only when a `map` is present** — `/?load=0` alone reaches the engine as `Name=… CPUSPEED=3000` and boots the default
level. `/?map=<any>&load=<n>` works: verified on hp2 (`Command line: PrivetDr?load=0 …` → `LoadMap:
../Save/Save0.usa?load`, the saved Ch1Rictusempra state came up although the URL named PrivetDr). The Play panel's
travel-options field (`load=0` + any map) is the same path. A launcher change that forwards travel options without a
map (or a "Continue" button that picks the map itself) would make `?load=<n>` alone work — not implemented.

## 11. Next: the single-player save list (designed, not implemented)

The owner's follow-up is to let the launcher list saves and launch straight into one, driven by descriptor data.
It is specified for the implementing agent in the ufrontend clone:
`docs/migration/steps/SAVE-GAMES-KICKOFF-PROMPT.md` (branch `hp2`, rev 3 `9f33da3`). In short:
- an optional `saveGames` block; absent = off (ut99, ut2004);
- `layout: "slotsIni"` for unreal1 and OldSkool: the slot names in `Unreal.ini` / `oldskool.ini` are the list;
- `layout: "slotsFolder"` for HP2 with the fixed folder `Save`: ONE save, `load=0`, no `-SAVESLOT`;
- a new HP2 game = the Storage panel's "Clear User Data";
- the launch is always `<map>?load=<n>`, never a bare `load=`.
