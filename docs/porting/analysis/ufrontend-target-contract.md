# ufrontend target contract for `hp2` (Harry Potter and the Chamber of Secrets, UE1 build 433)

Key: `ufrontend-target-contract` — analyst report (re-run, Opus), written 2026-09-26.
A previous run's partial report (no status line) was backed up outside the repo before this one replaced it; every
statement below was re-derived from source.

Conventions: `UF` = `/home/ion/hp2-ufrontend/ufrontend` (branch `hp2`); `HP2` = `/home/ion/hp2-ufrontend/HarryPotter2`;
`T99` = `/home/ion/engines/engine-ut99` (read only); `INFRA` = `/home/ion/unreal-frontend-azure-infra` (read only).
References are `path:line`. Every finding ends with an **ACTION**. **TO-VERIFY** = not provable from source; says how to prove it.
Nothing was executed: no cmake/make/yarn, no git writes.

## 0. Sources read and verdict

(pending)

## 1. `targets/hp2/target.json` — full skeleton with derivations

### 1.1 How the descriptor is consumed (so every field below has a reader)

| Reader | Fields it reads | Where |
|---|---|---|
| Vite bake (`__UE_TARGET__`, glue's `targetConfig`) | `storage`, `network`, `mods`, `fetchStubs`, `userFiles` | `UF/packages/frontend/vite.config.ts:40-47` |
| Vite bake (frontend-only constants) | `branding` (`__UE_BRANDING__`), `travel`, `videoDrivers`, `conversions`, `frameRateCap`, `audioHrtf`, `audioBuffer`, `defaults`, `branding.helpDoc` (`__UE_HELP__`), `telemetry` (+`id`), `notices`, `capabilities`, and **every literal `*.ini` in `manifestExclude`** (`__UE_ENGINE_FILES__`) | `vite.config.ts:37-39, 51-102, 169-184` |
| glue `assertTargetConfig` (throws at boot if missing) | `storage.{dbPrefix,keyPrefix}` strings, `network.{peerPrefix,defaultServerName}` strings, `mods.extToFolder` object, `fetchStubs` object, `userFiles` (compiled; a bad glob throws), optional `network.proxy` | `UF/packages/glue/src/target/config.js:42-94` |
| `yarn build` | `publish.engine.repo`, `engineArtifactSubpath`, `assetMarkers`, `manifest`, `precache`, `conversions[].precache`, `branding/theme.css` (hard fail if missing) | `UF/packages/build/src/build-inputs.mjs:79-126`, `bin/ufront-build.mjs:95-112, 128-130, 374-398` |
| `yarn publish-all` | `id`, `publish.{folder,engine.{repo,branch,presetDir,assetDirVar}}`, `engineArtifactSubpath`, `assetMarkers`, `manifestExclude` (`--exclude-from`), `precache`, `conversions[].precache` | `src/publish/args.mjs:212-329`, `engine.mjs:35-84,164-180`, `run.mjs:192-247` |
| portal | `publish.{folder,name,short,mark,chip,chipWarn,markSmall,accent,order}` + `branding/portal-notes.json` | `UF/packages/portal/src/render.mjs:18-39`, `bin/build-portal.mjs:48-70` |
| unit tests | `travel`, `telemetry`, `fetchStubs.Maps`, `file-manifest.json`, `capabilities`, `notices` | `UF/test/skeleton.test.mjs:76-272`; frontend tests (§8) |

⚠ **`__UE_TRAVEL__` must exist with all four arrays.** `PlayPanel.vue:25` does `const travel = __UE_TRAVEL__;` and
the template iterates `travel.modes/toggles/presets/mutators` unguarded (`PlayPanel.vue:125-173`); `vite.config.ts:55`
bakes `_descriptor.travel` with no fallback. An absent `travel` block would bake `undefined` and crash the panel.

### 1.2 Resulting storage names (exact, derived from code)

`dbPrefix = "hp2"`, `keyPrefix = "hp2"` give:

| Store | Name | Composed at |
|---|---|---|
| IndexedDB — user data (inis, saves, logs; never evicted) | **`hp2-user-data`** | `glue/src/idb/user-data.js:36` (`dbPrefix + '-user-data'`), also read pre-boot by `glue/src/config/ini-identity.js:207` |
| IndexedDB — uploaded mods | **`hp2-mods`** | `glue/src/idb/mods.js:19` |
| IndexedDB — WebRTC download cache | **`hp2-network-cache`** | `glue/src/idb/network-cache.js:20` |
| Package cache | **OPFS `pkg/hp2/<lower-cased path>` + index `pkg/hp2/index.json`** — NOT an IndexedDB `hp2-package-cache` any more (moved to OPFS in 2.13.4; PLAN §2 and `frontend/src/config/keys.ts:14`'s comment are stale on this) | `glue/src/cache/opfs-layout.js:28-35`, `opfs-package-store.js:68` |
| localStorage | `hp2_config`, `hp2_cache_budget`, `hp2_memfs_budget`, `hp2_net_room`, `hp2_net_peerhost`, `hp2_net_peerport`, `hp2_net_peersecure`, `hp2_net_stun`, `hp2_net_turn_url`, `hp2_net_turn_user`, `hp2_net_turn_cred`, `hp2_capability_warning_dismissed` | `frontend/src/config/keys.ts:10-12` (`keyPrefix + '_' + name`); glue `opfs-package-store.js:78-79`, `fetchfs/index.js:55`, `webrtc/index.js:81,964,1010`; callers in `frontend/src/stores/{storage,play,net}.ts`, `config/controller.ts`, `shell/capabilities.ts:337` |
| WebRTC peer ids | `hp2-<room>_<serverSlug>-srv` (host), `hp2-…` (clients/discovery) | `docs/06` §3 + `glue/src/webrtc/index.js:938-992` |

On its **own origin** (a separate Azure domain) HP2 cannot collide with the other four games anyway (IndexedDB,
OPFS and localStorage are per-origin), but the prefix is still required data and keeps local `yarn serve`
sessions on `localhost:8080` isolated from the other targets.

User-data key format: the daemon keys entries by **absolute VFS path**, e.g. `/System/Game.ini`,
`/System/User.ini`, `/Save/Slot1/Save0.usa` (`ini-identity.js:208-209`, `user-data.js`). So `iniPath` values below are
relative to the VFS root and must match where the ENGINE writes (see 1.4, `appUserDir()`).

### 1.3 The skeleton (`targets/hp2/target.json`)

JSON has no comments, so rationale goes in `_…Comment` keys exactly as the existing descriptors do
(`targets/ut99/target.json` is the shape). Values marked `TO-VERIFY` in the table in 1.4 must be checked before
the first commit of the file.

```json
{
  "id": "hp2",
  "engineFamily": "UE1",
  "abiVersion": 1,

  "branding": {
    "displayName": "Harry Potter and the Chamber of Secrets",
    "logo": "branding/logo.svg",
    "theme": "branding/theme.css",
    "helpDoc": "branding/help.md"
  },

  "_publishComment": "Everything `yarn publish-all` needs that is NOT derivable. folder \".\" = THIS TARGET IS THE SITE ROOT: it is published directly into --out with no portal page (it has its own domain and no game selector) — requires the packages/build change in the ufrontend-target-contract report §6; until that lands use folder \"hp2\" and deploy <out>/hp2/ as the site root. engine.repo is the engine checkout's FOLDER NAME under enginesRoot (the identity check compares basename(realpath(repo))). The portal-card fields are kept although no portal is rendered, so the target can join a multi-game portal later without a data change.",
  "publish": {
    "folder": ".",
    "name": "Harry Potter and the Chamber of Secrets",
    "short": "HP2",
    "mark": "HP",
    "chip": "433",
    "chipWarn": false,
    "accent": "#c9a227",
    "order": 50,
    "engine": { "repo": "HarryPotter2", "branch": "main", "presetDir": "Source" }
  },

  "_assetMarkersComment": "How build/publish prove an asset path is THIS game's tree. System/Core.u of HP2 (UE1 build 433, package version 79) is 61,685 bytes and hashes differently from every other target's Core.u. require = files only HP2 ships; forbid = the other UE1 games' core packages.",
  "assetMarkers": {
    "fingerprint": {
      "path": "System/Core.u",
      "sha256": "e0812a1931322d99d8b85db2f42fb2dacc8df33e919ea566a64e7a834e0d56e8",
      "note": "HP2 PC retail Core.u (61,685 bytes, package version 79)"
    },
    "require": ["System/HGame.u", "System/HPModels.u", "Maps/startup.unr"],
    "forbid": ["System/BotPack.u", "System/UnrealI.u", "System/XGame.u"]
  },

  "storage": {
    "dbPrefix": "hp2",
    "keyPrefix": "hp2",
    "cacheBudgetMb": 2048,
    "memfsBudgetMb": 1024
  },

  "network": {
    "_playerNameComment": "Where the ENGINE persists the player name: HP2's appInit creates User.ini from DefUser.ini ([DefaultPlayer] Name=Player). iniToken is the <File> half of -ini:<File>:[<Section>]:<Key>=, which FConfigCacheIni::Find translates ('User' -> the user ini).",
    "_publicRoomComment": "The shared room a fresh install starts on; the transport prepends the peer prefix.",
    "publicRoom": "default-public-room",
    "playerName": {
      "iniPath": "System/User.ini",
      "iniToken": "User",
      "section": "DefaultPlayer",
      "key": "Name",
      "prefix": "Player"
    },
    "_serverNameComment": "HP2 HAS Engine.GameReplicationInfo (Source/Engine/Classes/GameReplicationInfo.uc:19, `globalconfig string ServerName`), so the name lives in the SYSTEM ini like ut99's, and WebRTCDrv reads exactly that section at listen time (template WebRTCDrv.cpp:277-278). `stock` must equal the value Config/System/web/Default.ini ships; the retail Default.ini still carries UT's 'Another UT Server'.",
    "serverName": {
      "iniPath": "System/Game.ini",
      "iniToken": "System",
      "section": "Engine.GameReplicationInfo",
      "key": "ServerName",
      "stock": "Another UT Server"
    },
    "peerPrefix": "hp2",
    "defaultServerName": "HP2 Server"
  },

  "_modsComment": "The 'sort by type' table for uploaded files (step 1.7). HP2's own asset types only: music is Ogg Vorbis streamed by name from Music/ (UnAudio.h PlayMusic(FString Song,…)); there is no .umx.",
  "mods": {
    "extToFolder": {
      ".unr": "Maps",
      ".utx": "Textures",
      ".uax": "Sounds",
      ".ogg": "Music",
      ".u": "System",
      ".int": "System",
      ".ini": "System",
      ".usa": "Save"
    }
  },

  "fetchStubs": {
    "System": [".int"],
    "Maps": [".unr"]
  },

  "manifest": "file-manifest.json",
  "precache": "precache.json",

  "_manifestExcludeComment": "What the site must NOT serve (step 2.15). Literal .ini entries double as the list of engine inis the user-data daemon watches (vite.config.ts engineFiles -> boot.ts UserData.watch), so System/Game.ini and System/User.ini MUST stay literal here or settings do not survive a reload. Default.ini/DefUser.ini come from the engine's preloaded Config/System/web set. Everything else: native Win32 binaries/installer files, build-time sources (Fonts/, PackageSources/, DialogSources/, *.ttf, System/EditorRes/), unused content (dev maps under Maps/Oldfiles|Studies|templates, the localized MenuArt.<lang>_utx set that no package imports, Unreal-1 leftovers in SystemLocalized/), Windows splash bitmaps (Help/, System/*.bmp), and the retail Save/ leftovers.",
  "manifestExclude": [
    "System/Default.ini", "System/DefUser.ini", "System/Game.ini", "System/User.ini",
    "*.so", "*.so.*", "*.dll", "*.exe", "*.pdb", "*.dylib",
    "*.bin", "*-bin", "*-Linux", "*.bat", "*.com", "*.msi", "*.isu", "*.icd",
    "*bak", "*.tmp", "*.orig", "*.duplicate1", "*.log", "*.zip",
    "cdkey", "*.key",
    "Icon_", "Icon?", ".DS_Store", "Thumbs.db", "desktop.ini",
    "README.md", "*.inf", "*.ico", "*.ttf", "*.exec", "SAPFont.*",
    "System/*.bmp", "System/EditorRes/**",
    "Maps/Oldfiles/**", "Maps/Studies/**", "Maps/templates/**",
    "Textures/MenuArt.*",
    "SystemLocalized/**", "Fonts/**", "Help/**", "DialogSources/**", "PackageSources/**",
    "Logs/**", "Save/**", "*.usa"
  ],

  "_userFilesComment": "What the PLAYER makes and a reload must give back (ufront 2.75). HP2 writes saves under GSys->SaveSlotPath: ../Save when no -SAVESLOT= is given, ../Save/Slot<N> otherwise (Core/Src/UnMisc.cpp:1834-1841, 2001-2037); Save<i>.usa + Save<i>.bmp thumbnail + <Map>_pa.usa per-level state; Save/Slot<N>/cache/*_pa.usa is rebuilt from the slot at boot and deliberately NOT kept. The glue allows a glob only in the file name, so each slot directory is listed (hp2-game-boot-and-content.md §3.3).",
  "userFiles": [
    "Save/*.usa", "Save/*.bmp",
    "Save/Slot1/*.usa", "Save/Slot1/*.bmp",
    "Save/Slot2/*.usa", "Save/Slot2/*.bmp",
    "Save/Slot3/*.usa", "Save/Slot3/*.bmp",
    "Save/Slot4/*.usa", "Save/Slot4/*.bmp",
    "Save/Slot5/*.usa", "Save/Slot5/*.bmp",
    "Save/Slot6/*.usa", "Save/Slot6/*.bmp"
  ],

  "_defaultsComment": "The Play panel's hints. HP2 boots its menu book from LocalMap=Startup.unr with an empty map field; PrivetDr is the first playable level and loads directly. No game class: every HP2 map runs Engine.GameInfo, so the travel hint falls back to the neutral 'Key=Value?Key2=Value2'.",
  "defaults": {
    "map": "PrivetDr",
    "game": "",
    "travelOptions": ""
  },

  "_videoDriversComment": "Owner's renderer list for HP2: XOpenGL (WebGL 2, default), UTGLR/OpenGLDrv via gl4es (WebGL 1), HP2's own SoftDrv, NullDrv. No NOpenGLES. Switches are the launcher's table (Source/Game/Src/Launch.cpp:275-286).",
  "videoDrivers": [
    { "label": "XOpenGL (WebGL 2)", "args": "-XOPENGLRENDERER" },
    { "label": "UTGLR / OpenGL (WebGL 1)", "args": "-UTGLRRENDERER" },
    { "label": "Software (CPU)", "args": "-SOFTWARERENDERER" },
    { "label": "Null (no rendering)", "args": "-NULLRENDERER" }
  ],

  "frameRateCap": { "iniPath": "System/Game.ini", "iniToken": "System", "section": "Engine.Engine", "key": "FrameRateCap", "default": 120 },

  "_audioComment": "audioBuffer / audioHrtf are ADDED ONLY WHEN Phase E lands NOpenALDrv (openal-soft web backend). With SoundDrv.NullAudioSubsystem an absent block correctly hides both Advanced controls (vite.config.ts:70-77).",

  "_conversionsComment": "EMPTY BY FACT: HP2 ships no total conversions (no alternate -ini= System inis in Assets/System).",
  "conversions": [],

  "_travelComment": "EMPTY BY FACT (single-player). HP2 registers NO GameInfo or Mutator subclass (no `MetaClass=Engine.GameInfo`/`Engine.Mutator` in any System/*.int; no `extends GameInfo` under Source/*/Classes), so generate-travel yields nothing, and the one URL option HP2 honours that a pill could set (GameSpeed) is not worth a panel. Requires the skeleton-test + PlayPanel change in report §4 (or the fallback in §1.5).",
  "travel": { "modes": [], "toggles": [], "presets": [], "mutators": [] },

  "_capabilitiesComment": "Same required set as every threaded build (pthread + PROXY_TO_PTHREAD + OFFSCREENCANVASES_TO_PTHREAD + OPFS worker + emsdk-5 wasm baseline). offscreenWebgl2 is RECOMMENDED because the default renderer (XOpenGL) is WebGL 2; UTGLR/Software/Null run without it.",
  "capabilities": {
    "required": ["secureContext", "crossOriginIsolated", "sharedArrayBuffer", "webAssembly", "wasmThreads", "wasmFeatures", "workers", "atomicsWait", "offscreenCanvas", "offscreenWebgl", "opfsSyncAccess"],
    "recommended": ["gpuAcceleration", "offscreenWebgl2", "indexedDb", "webAudio", "webrtc", "pointerLock"]
  },

  "_noticesComment": "TO-VERIFY against a captured console. Taken from HP2 SOURCE, same UE1 client paths as ut99: Engine/Src/UnPenLev.cpp:88 `PendingLevel received: %s` (NAME_DevNet, visible: HP2 suppresses only DevNetTraffic), UPGRADE/FAILURE/FAILCODE at UnPenLev.cpp:91-113, Engine.int:36 `Pending=Pending connect to '%s' failed; %s`, UnLevel.cpp:970 `Failed to listen: %s`.",
  "notices": [
    {"join-failed": ["^Log: Pending connect to '[^']*' failed; (.*)$"]},
    {"join-refused": ["^DevNet: PendingLevel received: FAILURE ?(.*)$"]},
    {"wrong-password": ["^DevNet: PendingLevel received: FAILCODE WRONGPW"]},
    {"version-mismatch": ["^DevNet: PendingLevel received: UPGRADE"]},
    {"host-failed": ["^Log: Failed to listen: (.*)$"]}
  ],

  "_telemetryComment": "TO-VERIFY against a captured console from THIS build before committing fixtures (docs/06 §1: never copied from a sibling). Source evidence: Engine/Src/UnGame.cpp:651 debugf(TEXT(\"Browse: %s\")) and :842 debugf(NAME_Log, TEXT(\"LoadMap: %s\")) -> 'Log: Browse:' / 'Log: LoadMap:'; the crash reason line is whatever the ported web error device prints ('Critical: ' + reason after 'Critical: appError called:' in the template).",
  "telemetry": [
    { "browse": { "travel": ["^Log: Browse: (.*)$"], "load": ["^Log: LoadMap: (.*)$"] } },
    { "crash": ["^Critical: (?!appError called)(.*)$"] }
  ]
}
```

Fields deliberately **absent** (and why):

| Field | Why absent |
|---|---|
| `engineArtifactSubpath` | HP2's CMake puts the artifact at `<binaryDir>/SDLLaunch` (`HP2/Source/Game/CMakeLists.txt:221-228`: `OUTPUT_NAME SDLLaunch`, `RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/SDLLaunch`), the convention's default (`build-inputs.mjs:79`, `engine.mjs:35-38`). |
| `publish.engine.assetDirVar` | only for an engine that `--preload-file`s from the asset tree (ut2004). HP2 preloads only `Config/System/web/*` (`HP2/Source/Game/CMakeLists.txt:196-213`). |
| `publish.markSmall` | optional card flag (portal only). |
| `network.modes` / `network.networkDevice` / `network.proxy` | Proxy networking (2.79) is for joining NATIVE servers; there are no native HP2 servers. Absent ⇒ the P2P panel only (`frontend/src/components/NetworkSection.vue:23-30`). |
| `audioBuffer`, `audioHrtf` | only once NOpenALDrv exists (Phase E). Absent ⇒ controls hidden. |
| `overlay` | retired in 2.15d (`test/skeleton.test.mjs:42-44`). |

### 1.4 Derivation / verification table (field by field)

| Field | Value | Derivation / evidence | Status · ACTION |
|---|---|---|---|
| `id` | `hp2` | owner (PLAN §2). Must equal the folder name `targets/hp2/` (`ufront-build.mjs:63-66` lists known targets by folder with a `target.json`). | ✅ |
| `engineFamily` | `UE1` | `glue/src/artifact/meta.js:14` enum; HP2 is UE1 build 433. Informational. | ✅ |
| `abiVersion` | `1` | `HP2/vendor/glue/ue_abi.h` ABI 1; the emitter passes `--abi=${UE_ABI_VERSION}` (`HP2/Source/Game/CMakeLists.txt:276-279`). Build validates `meta.abiVersion === glue ABI_VERSION` (`meta.js:64-66`). | ✅ |
| `branding.displayName` | full title | shown in chrome + `<title>` (`frontend/src/branding.ts:3-7`, `App.vue:34`). | ⚠ owner: exact wording/trademark; `"HP2 Web"` is the ut2004-style alternative |
| `publish.folder` | `"."` (root) | see §6. Today `folder` is required and always a subdirectory (`args.mjs:217, 323`). | ⚠ needs §6 code change; fallback `"hp2"` |
| `publish.engine.repo` | `HarryPotter2` | build/publish resolve `<enginesRoot>/<repo>` (`build-inputs.mjs:94-102`, `args.mjs:312-317`); publish asserts `basename(realpath(engineRepo)) === repo` (`run.mjs:102`, `engine.mjs:164-172`). | ✅ (PLAN §2) |
| `publish.engine.branch` | `main` | asserted against the checkout's branch (`engine.mjs:173-178`); HP2 is on `main` (`git -C HP2 rev-parse --abbrev-ref HEAD` → `main`). | ✅ |
| `publish.engine.presetDir` | `Source` | `cmake --preset` runs in `<repo>/Source` (`engine.mjs:63,77`); HP2's presets are `HP2/Source/CMakePresets.json` with `binaryDir ${sourceDir}/../build-<preset>` (lines 10,30,47,55,63). | ✅ |
| `assetMarkers.fingerprint.sha256` | `e0812a19…d56e8` | measured: `sha256sum HP2/Assets/System/Core.u`, 61,685 bytes, header tag `9e2a83c1` version 79. | ✅ re-measure if the asset tree is ever rebuilt |
| `assetMarkers.require` | `HGame.u`, `HPModels.u`, `Maps/startup.unr` | present in `HP2/Assets` (the map is lower-case `startup.unr` on disk; `exists()` is case-sensitive). | ✅ |
| `storage.*Budget*` | 2048 / 1024 MB | the served payload is **460 files, 476 MB** raw with the excludes above (simulated with the generator's exact glob rules, §3); unreal1 (456 MB) uses 2048/1024. Largest single package `Sounds/AllDialog.uax` 97 MB, `HPModels.u` 35 MB. | ✅ (tunable) |
| `network.playerName` | `System/User.ini [DefaultPlayer] Name` | `HP2/Config/System/DefUser.ini` has `[DefaultPlayer] Name=Player Class=HGame.harry`; `HP2/Source/Core/Src/UnMisc.cpp:1933-1946` writes `User.ini` from it; token translation `HP2/Source/Core/Inc/FConfigCacheIni.h:119-123`. | ⚠ engine must implement `appApplyIniOverrides` (template `engine-ut99/Source/Core/Src/UnMisc.cpp:1115-1150`, absent in HP2 Core) — otherwise every ini-backed field (player name, server name, frame cap, HRTF) is silently ignored |
| `network.serverName` | `System/Game.ini [Engine.GameReplicationInfo] ServerName`, stock `Another UT Server` | GRI exists (`HP2/Source/Engine/Classes/GameReplicationInfo.uc:4,19`); retail `HP2/Config/System/Default.ini:196-198` has `ServerName=Another UT Server`. | ⚠ `stock` must be re-copied from `Config/System/web/Default.ini` once that file exists (setting it to e.g. `HP2 Server` there is fine — keep the two equal) |
| `…iniPath` = `System/Game.ini` | ini name | `appInit(InPackage)` writes `<appUserDir()><InPackage>.ini` (`HP2/Source/Core/Src/UnMisc.cpp:1913-1916`); the launcher currently passes `"UnrealTournament"` (`HP2/Source/Game/Src/Launch.cpp:883`, template copy). | ⚠ ACTION (engine): `appInit(TEXT("Game"),…)`; and `appUserDir()` must return `""` on Unix/web (only `UnVcWin32.cpp:1048` defines it) so the inis land in `/System` and saves in `/Save` — otherwise every `iniPath` and `userFiles` path here is wrong |
| `mods.extToFolder` | 8 entries | HP2 asset extensions measured: `.unr .utx .uax .ogg .u .int` (+ `.usa` saves, `.ini`). | ✅ |
| `fetchStubs` | `System [.int]`, `Maps [.unr]` | the map dropdown reads `fetchStubs.Maps` (`frontend/src/stores/play.ts:125-128`, `config/maps.ts`); under WasmFS the backend creates entries for EVERY manifest file anyway (`glue/src/fetchfs/index.js:463-467`), so no `Music`/`CUTSCENES` stub is needed. | ✅ |
| `manifestExclude` | list above | per-folder measurement in §3; `Textures/MenuArt.*` and `SystemLocalized/**` per the content report (verified here: no `.u/.unr/.utx` other than `MenuArt.utx` itself contains the string `MenuArt`); `Help/**`, `System/*.bmp` only read by the Win32 launcher (`HP2/Source/Launch/Src/Launch.cpp:157`, `FrontEnd/Source/FrontEnd.h:40-41`). | ✅ (drop `Textures/MenuArt.*` if the owner wants the unused package served) |
| `userFiles` | Save + Slot1..6 | `HP2/Source/Core/Src/UnMisc.cpp:1834-1841` (`SaveSlotPath = SavePath * "Slot" + N`), `:2001-2011` (`SavePath = "..\\Save"` when `appUserDir()` is empty); filenames `HP2/Source/Engine/Src/UnGame.cpp:252,451,699,1974`; glob rules `glue/src/target/user-files.js:49-77`. | ⚠ TO-VERIFY: slot count (6) and that the Unix file manager maps `..\\Save` to `/Save` (not a literal backslash directory). Verify: save in the web build, then list `/Save` from devtools (`FS.readdir('/Save')`) |
| `defaults.map` | `PrivetDr` | first story level (`HP2/Assets/Maps/PrivetDr.unr`); loads directly (content report §1). | ✅ |
| `videoDrivers` | 4 | launcher switch table `HP2/Source/Game/Src/Launch.cpp:275-286`. | ✅ (the NOpenGLES row stays in the launcher table; it is simply not offered) |
| `frameRateCap` | `System/Game.ini [Engine.Engine] FrameRateCap` 120 | read by the launcher `HP2/Source/Game/Src/Launch.cpp:686-699` (`GConfig->GetFloat("Engine.Engine","FrameRateCap")`, default 120). | ✅ once `-ini:` works |
| `capabilities` | as ut99 | probe ids exist in `frontend/src/shell/capabilities.ts` (checked by `frontend/test/capabilities.test.ts:63-69` — add `hp2` there). | ✅ |
| `notices` | 5 kinds | HP2 source lines cited in `_noticesComment`. | ⚠ TO-VERIFY (no HP2 capture until P2P runs; HP2 is single-player, so this is low priority) |
| `telemetry` | as ut99 | HP2 `UnGame.cpp:651,842`. | ⚠ TO-VERIFY and add verbatim fixtures (§4) |

### 1.5 Fallback if the owner wants ZERO frontend/test code change for `travel`

`test/skeleton.test.mjs:102-103` requires `travel.modes.length > 0` and `travel.toggles.length > 0` for every target in
`TARGETS`, every mode's `game` must be `Package.Class` whose package is served under `System/` (`:143-163`), and every
toggle key must be unique (`:118-123`). A truthful minimal block that passes with no code change:

```json
"travel": {
  "modes": [ { "label": "Story", "game": "Engine.GameInfo", "mapPrefix": "", "tip": "The single-player adventure — every HP2 map runs the base GameInfo." } ],
  "toggles": [ { "label": "Fast game", "set": "GameSpeed=1.3", "tip": "Run the world 30% faster (GameInfo.uc:584 parses GameSpeed)." } ],
  "presets": [],
  "mutators": []
}
```

It is honest (Engine.u is served under `System/`; `GameSpeed` is parsed at `HP2/Source/Engine/Classes/GameInfo.uc:584`),
but it adds a Mode select whose only option is a no-op. **Recommended: the empty block + the two small generic
changes in §4.3** (the test learns that a single-player target may declare no quick options; `PlayPanel.vue` hides
empty fieldsets). Either way the owner decides once; record it in `targets/hp2/README.md`.

### 1.6 `targets/hp2/README.md` (required companion, same shape as `targets/ut99/README.md`)

Sections: file table (target.json, branding/*, file-manifest.json, dependency-manifest.json, precache.json);
engine artifact (`/home/ion/hp2-ufrontend/HarryPotter2/build-wasm-perf/SDLLaunch/`, consumed via `yarn build --target hp2`);
assets (`/home/ion/hp2-ufrontend/HarryPotter2/Assets`, 476 MB served of ~796 MiB on disk, never in git); renderers (XOpenGL
default, UTGLR, SoftDrv, NullDrv; no NOpenGLES); **Publishing** (root publish, `folder "."`, own domain, no portal);
**Asset identity** (Core.u sha); **Travel: none, and that is the finished state** (single-player, why); **Conversions: none**;
**Saves** (slot directories, `appUserDir()` dependency).

## 2. Branding files

Four files under `targets/hp2/branding/`, shaped exactly like `targets/ut99/branding/` (all four existing targets
ship the same four). What actually reads each one:

| File | Consumer | Required? | Shape to copy |
|---|---|---|---|
| `theme.css` | copied to `<out>/theme.css` by `ufront-build.mjs:395-398` (**build dies if missing**), linked by `packages/frontend/index.html:10` (`%BASE_URL%theme.css`); PrimeVue preset maps tokens (`frontend/src/theme/preset.ts:22-31`); `public/splash.css:466` | **hard** | `targets/ut99/branding/theme.css` (a `:root{}` block of 12 tokens) |
| `help.md` | baked as `__UE_HELP__` (`vite.config.ts:96-102`), rendered by `frontend/src/components/HelpDialog.vue:12` with `marked` (raw HTML trusted) | soft (empty → "No help content yet") | `targets/ut99/branding/help.md` |
| `logo.svg` | declared in `branding.logo`, **read by no code today** (grep of `packages/frontend/src` + `index.html` + `ufront-build.mjs`: zero hits); only `test/skeleton.test.mjs:36-38` requires the KEY, and only for ut2004 | keep for parity | `targets/ut99/branding/logo.svg` (a `<text>` SVG using `var(--ufront-accent)`/`var(--ufront-text)`) |
| `portal-notes.json` | `packages/portal/bin/build-portal.mjs:56-64` — **optional** (warns, card gets no prose); with a root publish no portal is rendered at all | optional | `targets/ut99/branding/portal-notes.json` (`_comment`, `blurb` escaped, `notes[]` raw HTML) |

### 2.1 `theme.css` — the token vocabulary (all 12, as every target defines them)

```css
/* targets/hp2/branding/theme.css — per-target theming via CSS (docs/06 §1/§3, ADR-13).
 * Harry Potter and the Chamber of Secrets palette: deep night-blue/ink surfaces + Gryffindor gold accent
 * (distinct from UT99's bronze, Unreal's teal and UT2004's red). */
:root {
    /* Surfaces */
    --ufront-bg: #0b0a12;              /* page background */
    --ufront-surface: #16141f;         /* inputs / panels */
    --ufront-surface-border: #3a2f4a;  /* input / panel borders */
    --ufront-panel-track: #211d2d;     /* progress-bar track, secondary buttons */

    /* Text */
    --ufront-text: #efe9dc;            /* primary text (parchment) */
    --ufront-text-muted: #b3a88f;      /* labels */
    --ufront-text-dim: #857c6a;        /* status / hints */

    /* Accent (primary action — buttons, focus, progress fill) */
    --ufront-accent: #c9a227;          /* Gryffindor gold */
    --ufront-accent-hover: #ddb73c;
    --ufront-accent-active: #a8861c;
    --ufront-accent-contrast: #140f02; /* text on the accent */

    /* Feedback */
    --ufront-success: #37c837;
}
```

Colour values are a proposal (owner's call); the **token names are the contract**. The frontend additionally reads
`--ufront-border`, `--ufront-warn`, `--ufront-error`, `--ufront-danger` **with fallbacks** (`grep -rhoE 'var\(--ufront-'
packages/frontend/src`); no existing target defines them, so HP2 need not either. `.prettierrc` formats `*.css`, so
the file must pass `prettier --check` (4-space indent, the `yarn lint` gate — `.ufront/gates/lint.sh`).

### 2.2 `logo.svg`

```svg
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 320 48" role="img" aria-label="Harry Potter and the Chamber of Secrets">
  <text x="0" y="36" font-family="Georgia, 'Times New Roman', serif" font-size="30" font-weight="700"
        fill="var(--ufront-accent, #c9a227)">Harry Potter<tspan fill="var(--ufront-text, #efe9dc)" font-size="18"> and the Chamber of Secrets</tspan></text>
</svg>
```
No trademark artwork (the other targets use plain text logos, never the games' own logos).

### 2.3 `help.md`

Mirror `targets/ut99/branding/help.md`: a title, a "placeholder until Phase 3" note, then **Play** (the story starts
from the menu book with an empty map field; any `Maps/*.unr` can be typed to jump in, e.g. `PrivetDr`), **Saving**
(the in-game book's slots persist in the browser under `hp2-user-data`; clearing site data deletes them), **Storage /
Advanced** (cache, ini editor, renderer: XOpenGL default, UTGLR WebGL1, Software, Null), **Audio** (silent until
NOpenALDrv lands, then Ogg music + effects), **Multiplayer**: say plainly that HP2 is single-player (the P2P panel
still renders because every target carries a `network` block — see §11 Q4).

### 2.4 `portal-notes.json`

Only used if HP2 is ever a card on a multi-game portal. Keep it for data completeness:

```json
{
    "_comment": "Prose for this target's card on a portal page (packages/portal). Unused while hp2 publishes at the root of its own domain (publish.folder \".\"). `notes` accept inline HTML; `blurb` is escaped.",
    "blurb": "The 2002 PC adventure on Unreal Engine 1 (build 433), in the browser.",
    "notes": [
        "This is the <b>PC retail release</b> (UE1 build 433) — single-player only.",
        "Saves are kept in your browser; clearing this site's data deletes them."
    ]
}
```

### 2.5 Two generic chrome gaps that matter more on a dedicated domain (not blockers)

1. **The static `<title>` is `Unreal Tournament 2004`** (`packages/frontend/index.html:6`) until `applyBranding()`
   runs (`frontend/src/branding.ts:6-9`). A crawler / link preview of the HP2 domain would show the UT2004 title.
   ACTION (generic, optional): bake `__UE_BRANDING__.displayName` into the HTML at build time (a Vite
   `transformIndexHtml` hook in `vite.config.ts`), plus a `<meta name="description">`; `doc-sync` then requires a
   `packages/frontend/README.md` line.
2. **No favicon**: `index.html` has no `<link rel="icon">`, so every page load 404s `/favicon.ico` (same on the
   existing site). ACTION (optional): add `branding.favicon` DATA + copy it in `ufront-build.mjs` next to `theme.css`,
   or accept the 404.

## 3. Generating file-manifest.json / dependency-manifest.json / precache.json

★ **All four commands below were dry-run for this report** with the `targets/hp2/target.json` skeleton of §1.3 as
`--exclude-from`, writing only into a scratch directory (no file in `UF` or `HP2` was touched, no `yarn`, no cmake).
The results are quoted verbatim, so the committed outputs should reproduce them exactly.

Paths: `UF=/home/ion/hp2-ufrontend/ufrontend`, `SRC=/home/ion/hp2-ufrontend/HarryPotter2/Assets`, cwd = `UF`.
The generators are node-stdlib CommonJS (`packages/glue/bin/*.cjs`), so they run without `yarn install`.

### 3.1 `targets/hp2/file-manifest.json`

```bash
cd /home/ion/hp2-ufrontend/ufrontend
SRC=/home/ion/hp2-ufrontend/HarryPotter2/Assets
node packages/glue/bin/generate-manifest.cjs "$SRC" targets/hp2/file-manifest.json \
     --exclude-from targets/hp2/target.json
# measured: "Generated manifest: 460 files, 476.0 MB total / Excluded: 2599 files (--exclude)"
#           by top dir: System 221 (11 .u + 25 top-level .int + 185 CUTSCENES/*.int), Music 111 .ogg,
#           Textures 51, Maps 48, Sounds 29 · 139 packages carry a `guid` (package version 79/76 ≥ 68)
```

- **Use `--exclude-from`, not the `mapfile` loop in `docs/06` §3.** The generator reads `manifestExclude` itself
  (`generate-manifest.cjs:48-55, 142-157`); `docs/07` §8 and `packages/build/README.md` ("What it refuses to make
  configurable") both say the shell-expanded form is how native binaries leaked back in.
- This is the **dev** manifest (uncompressed, `path` = logical path). The **publish** manifest is regenerated by
  `publish-all` into `<out>/file-manifest.json` with `--compress zstd --level 19 --output-dir <out>/zstd --subfolder zstd
  --reuse-existing` (`src/publish/commands.mjs:62-85`); nobody commits that one.
- The manifest also decides which top-level dirs `yarn build` symlinks into `serve-web/` (`ufront-build.mjs:405-414`):
  here `Maps Music Sounds System Textures`.
- ⚠ Case: the manifest keeps disk case (`Maps/startup.unr`, `System/CUTSCENES/…`). FetchFS resolves case-insensitively
  through the lower-case key, but HP2's own code opens `Cutscenes\\<file>` with a backslash (content report §4.3) —
  an engine-side fix, not a manifest one.

### 3.2 `targets/hp2/dependency-manifest.json` (step 2.18, optional but recommended)

```bash
node packages/glue/bin/generate-deps.cjs "$SRC" targets/hp2/file-manifest.json targets/hp2/dependency-manifest.json
# measured: "Dependency manifest: 138 entries with dependencies, 322 empty entries omitted (139 packages,
#            321 non-packages, 0 malformed, 0 missing)
#            references: 714 total, 714 resolved, 0 unresolved, 0 ambiguous
#            integrity: all 139 packages match the length their export table declares"
```

- **0 unresolved**: the exclusion list removes nothing that a served package imports (no `--allow-unresolved`, so no
  "DEGRADED" build warning, `ufront-build.mjs:359-365`).
- HP2 packages are version 79 (maps 76/79): ≥ 64, so the compact-index name table branch of the parser
  (`generate-deps.cjs:365`) — the same branch UT99 (69) uses.
- Notable closures (from the generated file): `Maps/Entry.unr` imports **`System/Editor.u`** (so the web build must
  serve `Editor.u` and link the stripped Editor, PLAN §2 "Editor linked ON" is confirmed by data); `Maps/startup.unr`
  → 18 files / 129.2 MB; `Maps/PrivetDr.unr` → 21 files / 133.5 MB.

### 3.3 `targets/hp2/precache.json`

Two-step, like every target (`packages/build/README.md` "A precache list that has to be generated is announced as
weaker"):

**Step A — a correct-by-construction first list (commit this with the target):**

```bash
cat > /tmp/hp2-boot-closure.json <<'JSON'
["Maps/Entry.unr","Maps/startup.unr",
 "Textures/3DWizardCards.utx","Textures/CVresearch.utx","Textures/HP2_Master.utx","Textures/HP_Titles.utx",
 "Textures/HPEdit.utx","Textures/SkyBox.utx","Textures/SpellShapes.utx","Textures/UWindowFonts.utx"]
JSON
node packages/glue/bin/generate-precache.cjs "$SRC" --system-files \
     --append-list /tmp/hp2-boot-closure.json \
     --file-info-source targets/hp2/file-manifest.json -o targets/hp2/precache.json
# measured: "[precache] 231 files — uncompressed 131.7 MB"
#   (--system-files alone: 221 files, 101.4 MB — every System/*.u + *.int INCLUDING the 185 System/CUTSCENES/*.int,
#    because isSystemTopDir() only checks the `system/` prefix, generate-precache.cjs:47-53, 137)
```

The append list is the dependency closure of the two boot maps (`Entry` is always loaded first; `LocalMap=Startup.unr`,
`HP2/Config/System/Default.ini:6`) computed from the dependency manifest above, plus `Textures/UWindowFonts.utx`,
which UWindow loads with `DynamicLoadObject` and no package imports (`HP2/Source/UWindow/Classes/UWindowRootWindow.uc:421-437`,
content report §5). Every entry lower-cases to a manifest key (checked).

**Step B — the real list, captured from a browser session once the web build boots to the menu.** The capture tools
are target-specific today and need an `hp2` branch first (§8): `packages/e2e/tools/precache-baseline.cjs:168-229`
branches on `TARGET === 'unreal1' | 'ut99' | 'ut99-436-compat'` and otherwise runs the **ut2004** menu clicks;
`tools/precache-capture.cjs:111-113, 202` waits for `UT2k4MainMenu.Opened` and loads the ut2004 profile. ACTION: add
an `hp2` branch to `precache-baseline.cjs` that waits for `Bringing Level startup.MyLevel up for play` (TO-VERIFY the
exact case/spelling from a real log) and then `settle()`s; then

```bash
yarn build --target hp2          # default serve-web/ (the tool reads serve-web/file-manifest.json)
yarn serve &                     # :8080
TARGET=hp2 PROBE_OUT=/tmp/pc-hp2 node packages/e2e/tools/precache-baseline.cjs
node packages/glue/bin/generate-precache.cjs "$SRC" --system-files --append-list /tmp/pc-hp2.json \
     --file-info-source targets/hp2/file-manifest.json -o targets/hp2/precache.json
```

(Menu music is an `.ogg` opened by name at run time, `UnAudio.h PlayMusic(FString Song,…)`; only the capture can say
which one the menu plays — add it then.)

### 3.4 Travel candidates (documentation, proves `travel` is empty by fact)

```bash
node packages/glue/bin/generate-travel.cjs "$SRC" --manifest targets/hp2/file-manifest.json
# measured: "game types: 0 emitted … 0 declared-but-unreachable … / mutators: 0 emitted …"  → {"modes":[],"mutators":[]}
```

`generate-quickshots.cjs` is UT2004-only (map-browser screenshots from `.ucl` records, `generate-quickshots.cjs:1-20`):
**not applicable** to HP2.

### 3.5 What the build does with them

| File | Dev build (`yarn build`) | Publish (`publish-all`) |
|---|---|---|
| `file-manifest.json` | copied to `serve-web/file-manifest.json` (`ufront-build.mjs:340`) | regenerated compressed into `<out>/file-manifest.json` (`commands.mjs:62-85`) |
| `dependency-manifest.json` | copied if present (`ufront-build.mjs:356-368`) | regenerated from the PUBLISHED manifest (`commands.mjs:92-102`) |
| `precache.json` | copied to `serve-web/precache-list.json` (`ufront-build.mjs:374-376`) | copied to `<out>/precache-list.json`; if absent, a `--system-files`-only list is generated and announced as weaker (`run.mjs:226-247`) |

The three files are **JSON, not prettier-checked** (`.prettierignore` has `**/*.json`) and **eslint-ignored**
(`eslint.config.mjs:48` ignores `**/file-manifest.json`); commit them as the generators write them (single-line JSON).

## 4. test/skeleton.test.mjs — assertions and fixtures for a new target

`UF/test/skeleton.test.mjs` (272 lines) is the root `yarn test` (`package.json` `"test": "node --test test/*.test.mjs"`),
run by the `unit` gate (`.ufront/gates/unit.sh:23`). **It does not discover targets**: every per-target block loops over
a hard-coded list, so a fifth target is invisible to it until added.

```js
// test/skeleton.test.mjs:76
const TARGETS = ['unreal1', 'ut99', 'ut99-436-compat', 'ut2004'];
```

### 4.1 What it asserts once `'hp2'` is in `TARGETS`

| Test (line) | Assertion | hp2 with the §1.3 skeleton |
|---|---|---|
| `targets/hp2: travel DATA is well-shaped` (`:94-141`) | `travel` object; 4 arrays; **`modes.length > 0` (`:102`) and `toggles.length > 0` (`:103`)**; each mode `label`/`tip` strings, `game` matches `/^[\w]+\.[\w]+$/`; mutator `class` likewise; unique toggle keys; preset `Game=` must be a declared mode; no whitespace in `set`, no leading `?` | **FAILS at `:102`** with the empty block → §4.3 change (or the §1.5 fallback data) |
| `every travel class is served where the engine looks` (`:143-163`) | each `modes[].game` / `mutators[].class` package is `System/<pkg>.u` in `file-manifest.json` | vacuous pass (empty); with the §1.5 fallback: `Engine.GameInfo` → `System/Engine.u` ✓ |
| `every declared mode mapPrefix has maps` (`:165-185`) | a non-empty `mapPrefix` prefixes some served map (`fetchStubs.Maps` extensions) | vacuous pass |
| `telemetry DATA is well-shaped and matches real engine output` (`:220-271`) | `telemetry` array of objects; categories ⊂ `page cmdline browse crash net` (`:197`); ids match `/^[A-Za-z0-9_.:-]{1,64}$/`; every pattern compiles and has a `(`; **both `browse` and `crash` present** (`:255-257`); every `CAPTURED` line for this target matches (`:259-263`); no crash pattern matches any `HEALTHY` line (`:264-270`) | passes with the ut99 patterns — **but the `CAPTURED` check is VACUOUS for a target with no fixture rows** (`if (tgt !== target) continue;`, `:260`) |

It also requires `targets/hp2/file-manifest.json` to exist and parse (`manifestIndex`, `:79-91`) — so the manifest of
§3.1 must be committed in the same commit that adds `'hp2'` to `TARGETS`.

The first three tests (`:14-67`) are unaffected (the descriptor-shape test is ut2004-only).

### 4.2 Fixtures it needs (verbatim captured lines — never typed from source)

Append to `CAPTURED` (`:200-209`) — the values below are the EXPECTED SHAPE only; replace each with the exact line from
a real hp2 web console (docs/06 §1: "Patterns are verified against a real captured log from the build they describe"):

```js
    ['hp2', 'browse', 'Log: Browse: <verbatim from an hp2 capture, e.g. a travel to PrivetDr>'],
    ['hp2', 'browse', 'Log: LoadMap: <verbatim — the boot load of Entry / startup>'],
    ['hp2', 'crash', 'Critical: <verbatim — the reason line after "Critical: appError called:">'],
```

How to capture: boot the hp2 web build (`yarn build --target hp2 && yarn serve`), open the console, type a map in the
Play panel (or `open PrivetDr` in the engine console) for the travel lines; for the crash line use the engine's
`CRASH` console command (HP2 keeps `appErrorf("Unreal crashed at your request")`, `HP2/Source/Core/Src/UnUnix.cpp:129`)
through Advanced → engine console, then copy the `Critical:` lines. Source already guarantees the prefixes:
`Engine/Src/UnGame.cpp:651` `debugf( TEXT("Browse: %s"), … )` (default `NAME_Log`), `:842` `debugf( NAME_Log,
TEXT("LoadMap: %s"), … )`.

`HEALTHY` (`:212-218`) needs nothing new; optionally add one HP2 healthy line (e.g. the `Init: Command line:` line) so a
widened crash pattern is also checked against HP2 text.

### 4.3 Recommended test/UI changes (generic, data-driven — no game name in generic code)

1. **Discover targets instead of listing them** (so the next target cannot be silently skipped):
   ```js
   import { readdirSync } from 'node:fs';
   const TARGETS = readdirSync(join(root, 'targets')).filter((d) => existsSync(join(root, 'targets', d, 'target.json')));
   ```
   (mirrors how `ufront-build.mjs:63-65` finds known targets).
2. **Close the vacuous-fixture hole**: assert every target has ≥1 `CAPTURED` `browse` row and ≥1 `crash` row
   (`assert.ok(CAPTURED.some(([t, c]) => t === target && c === 'browse'), 'no captured browse line for ' + target)`).
   This is what forces the real HP2 capture before the target is "done".
3. **Let a single-player target declare no quick options** — replace `:100-103` with:
   ```js
   // A single-player target may declare NO quick options at all (hp2: no GameInfo/Mutator is registered in its
   // install — generate-travel emits nothing). Partially-empty is still a mistake: a target with modes must have toggles.
   const none = ['modes', 'toggles', 'presets', 'mutators'].every((k) => t.travel[k].length === 0);
   if (!none) {
       assert.ok(t.travel.modes.length > 0, 'travel.modes must not be empty');
       assert.ok(t.travel.toggles.length > 0, 'travel.toggles must not be empty');
   }
   ```
   and in `packages/frontend/src/components/PlayPanel.vue` hide the two fieldsets when their arrays are empty
   (`<Fieldset v-if="travel.modes.length || travel.toggles.length || travel.presets.length" …>` at `:125`, and
   `v-if="travel.mutators.length"` on the Mutators fieldset at `:162`). `doc-sync` then requires a
   `packages/frontend/README.md` (or `docs/`) line in the same commit (`.ufront/gates/doc-sync.sh:12-21`); also update
   `docs/06` §3 "Travel quick-options are DATA" with "an all-empty block = no quick-options panel (single-player)".
   ⚠ Check `packages/frontend/test/` for a PlayPanel test that counts the fieldsets
   (`grep -rn "Quick options\|Mutators" packages/frontend/test packages/e2e/lib/launcher-ui-test.js`) and update it.

### 4.4 The other unit suites that enumerate targets (add `'hp2'`)

| File:line | Loop | hp2 result with §1.3 data |
|---|---|---|
| `packages/frontend/test/capabilities.test.ts:63` | `for (const t of ['ut2004','ut99','ut99-436-compat','unreal1'])` — every capability id is a known probe | passes |
| `packages/frontend/test/net-notices.test.ts:119` | UE1 targets: `FAILURE …`, `FAILCODE WRONGPW`, `UPGRADE …` → `['join-refused','wrong-password','version-mismatch']` | passes (same patterns as ut99) |
| `packages/frontend/test/net-notices.test.ts:146` | all targets: `mustStayQuiet` lines raise nothing; every pattern compiles and names a kind with wording | passes |
| `packages/frontend/test/proxy-net-notices.test.ts:170` | targets without Proxy networking carry no `server-anticheat`/`server-native-content` kinds | passes |
| `packages/glue/test/_target-fixtures.cjs` + `target-config.test.cjs:76-95` | per-target fixture proving db names / mod sort (UT2004, UT99, UNREAL1) | **ACTION (recommended)**: add an `HP2` fixture (`dbPrefix/keyPrefix 'hp2'`, `peerPrefix 'hp2'`, the §1.3 `extToFolder`, `userFiles` with the Slot globs) and a test asserting `stores.cache.scope === 'hp2'`, `hp2-user-data`, `hp2-mods`, `hp2-network-cache`, `resolveModPath('Music_x.ogg','sort') === 'Music/Music_x.ogg'`, `resolveModPath('Save0.usa','sort') === 'Save/Save0.usa'`, and `assertTargetConfig` accepting the Slot globs |

The e2e specs are not unit tests (they need a GPU and a served build); §5 covers them.

### 4.5 Order of the first commit (so `yarn test` never goes red)

1. `targets/hp2/{target.json, README.md, branding/*, file-manifest.json, dependency-manifest.json, precache.json}`.
2. `test/skeleton.test.mjs` (§4.3 items 1 and 3; item 2 only once the capture exists, or it goes red by design).
3. `PlayPanel.vue` + its README line (doc-sync).
4. The four frontend test loops + the glue fixture.
5. `docs/06` §3/§6 and `docs/migration/STATE.md` entry (§9).
`yarn test && yarn workspace @ufront/frontend test && yarn workspace @ufront/glue test && yarn lint` (= most of `yarn gates`).

## 5. Local build, serve, E2E boot smoke

(pending)

## 6. Publish-at-root plan (no portal, no game selector)

(pending)

## 7. `.ufront/publish.local.json` for this workspace

(pending)

## 8. Places that hardcode the existing targets

(pending)

## 9. Gates, hooks, CHANGELOG/STATE rules

(pending)

## 10. Azure: a second, HP2-only domain (plan only)

(pending)

## 11. Open questions / TO-VERIFY
