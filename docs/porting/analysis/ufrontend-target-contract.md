# ufrontend target contract for `hp2` (Harry Potter and the Chamber of Secrets, UE1 build 433)

Key: `ufrontend-target-contract` — analyst report (re-run, Opus), written 2026-09-26.
A previous run's partial report (no status line) was backed up outside the repo before this one replaced it; every
statement below was re-derived from source.

Conventions: `UF` = `/home/ion/hp2-ufrontend/ufrontend` (branch `hp2`); `HP2` = `/home/ion/hp2-ufrontend/HarryPotter2`;
`T99` = `/home/ion/engines/engine-ut99` (read only); `INFRA` = `/home/ion/unreal-frontend-azure-infra` (read only).
References are `path:line`. Every finding ends with an **ACTION**. **TO-VERIFY** = not provable from source; says how to prove it.
Nothing was executed: no cmake/make/yarn, no git writes.

## 0. Sources read and verdict

**Sources read (all READ ONLY except this report):**

| Area | Files |
|---|---|
| ufrontend clone `UF` (branch `hp2`, HEAD `1bb9431` "docs(2.79): §21.10 — released …", i.e. identical to upstream 2.79 — no hp2 commit yet) | `CLAUDE.md`; `docs/{05-engine-repos,06-targets-and-branding,07-build-and-ci,08,09,WHERE-THINGS-GO,CONVENTIONS,README}.md`; `docs/migration/{STATE,ROADMAP}.md` (2.53, 2.75, 2.79, add-a-target, publish-all, portal entries); `targets/{ut99,unreal1,ut2004,ut99-436-compat}/**` (target.json, README.md, branding/*, portal-notes.json, precache.json, manifests); `test/skeleton.test.mjs`; `packages/build/{README.md,bin/*.mjs,src/**}` (build-inputs, publish/{args,run,engine,execute,state,verify,commands,freshness}); `packages/portal/{bin/build-portal.mjs,src/render.mjs}`; `packages/glue/bin/{generate-manifest,generate-deps,generate-travel,generate-precache,generate-quickshots}.cjs`, `packages/glue/src/{target,idb,cache,fetchfs,webrtc,config,artifact}/**`, `packages/glue/test/*`; `packages/frontend/{vite.config.ts,index.html,src/**,test/**}`; `packages/e2e/{playwright.config.js,*-wasm-boot.spec.js,lib/**,tools/**,README.md}`; `.ufront/{gates,hooks}/*`, `.ufront/README.md`, `install-engine-hooks.sh` |
| HP2 engine repo `HP2` | `CLAUDE.md`, `docs/SOURCE-LAYOUT.md`, `Config/System/{Default,DefUser}.ini`, `Source/{Core,Engine,Game,UWindow}/…` (the lines cited per row), `Source/CMakePresets.json`, `Source/Game/CMakeLists.txt`, `vendor/glue/*`, `Assets/**` (measured) |
| Infra `INFRA` | `README.md`, `CLAUDE.md`, `config.json`, `docs/{ARCHITECTURE,RUNBOOK}.md`, `infra/main.bicep` + `infra/modules/{site,frontdoor,apps,logs,logs-access,gameproxy}.bicep`, `bin/{deploy-ufront.py,verify-site.mjs,apply-outputs.sh,infra-deploy.sh,set-cache-headers.py}`, `peerjs/server.js`, `relay/src/server.mjs` |

**Verdict: adding `hp2` is mostly DATA work, with four small generic code changes, and all of it is blocked on the engine's first web artifact.**

| # | Finding | Blocking? | ACTION (section) |
|---|---|---|---|
| V1 | `yarn build --target hp2` needs **no code change**: targets are discovered by folder (`ufront-build.mjs:63-65`). It needs `targets/hp2/` (§1–§3) + `.ufront/publish.local.json` (§7) + the engine artifact `HP2/build-wasm-perf/SDLLaunch/` with a valid `meta.json`. **Today only `HP2/build-native/` exists.** | yes (engine) | engine Phase "web build" first; then §5.1 |
| V2 | The descriptor is fully specified in §1.3: `dbPrefix`/`keyPrefix` `hp2` ⇒ IndexedDB **`hp2-user-data`**, `hp2-mods`, `hp2-network-cache`, OPFS `pkg/hp2/`, localStorage `hp2_*` (§1.2). `userFiles` = `Save/*.usa|bmp` + `Save/Slot1..6/*.usa|bmp`. `.ogg → Music`. No `conversions`, no proxy networking, no audio controls until NOpenALDrv. | no | commit §1.3 |
| V3 | The three generated files were **dry-run** (§3): manifest 460 files / 476 MB, deps 714/714 resolved, precache 231 files / 131.7 MB, travel candidates 0/0. | no | run §3 commands verbatim |
| V4 | `test/skeleton.test.mjs:76` hard-codes the four targets and `:102-103` require non-empty `travel.modes`/`toggles`; HP2 has **no** GameInfo/Mutator subclass to offer. | yes (unit gate) | §4.3 (discover targets; allow an all-empty travel block; `PlayPanel.vue` hides empty fieldsets) or the §1.5 data fallback |
| V5 | **No publish-at-root mode exists.** `publish-all` always renders the portal over `<out>/index.html` (`run.mjs:647-666`). | yes for Track B | Track A (folder `hp2`, upload `<out>/hp2/`) works today; Track B = `publish.folder "."` generic change (§6.2, 13 items) |
| V6 | Engine prerequisites that every `iniPath` / `userFiles` / `logPath` value assumes: `appUserDir()` returns `""` on web, `appInit(TEXT("Game"),…)` instead of `"UnrealTournament"` (`HP2/Source/Game/Src/Launch.cpp:883`), and `appApplyIniOverrides` ported into HP2 Core. | yes (engine) | §1.4, §11 Q3/Q5 |
| V7 | E2E: no hp2 spec ⇒ `yarn e2e --target hp2` runs **zero tests and passes**. Create `packages/e2e/hp2-wasm-boot.spec.js` + `lib/engines/hp2.profile.js` from `ut99-wasm-boot.spec.js` test 1 + `unreal1`'s SoftDrv test. | yes (e2e-pre-push) | §5.3 |
| V8 | Other hard-coded target lists: 4 frontend/glue test loops, `precache-baseline.cjs`, `precache-capture.cjs`, `publish-boot-check.cjs`, the `no-engine-specifics` gate regexes (would let an `hp2` literal pass unnoticed), plus infra `peerjs ID_PREFIXES` / `requiredFolders`. | partly | §8.1, §9.1 |
| V9 | Azure: **Option A** = same Front Door Standard profile `unrealfd`, a second endpoint + a second storage account `$web`, reusing the `coiheaders` rule set (COOP/COEP), the zstd upload convention, the `relay-og` telemetry origin group and the `unrealpeer-we` broker (origins list extended). Plan only. | no (plan) | §10 |
| V10 | The three renderer forks are **still inside the ufrontend clone** (`UF/XOpenGLDrv`, `UF/utglr-copy`, `UF/gl4es`, untracked `??` in `git -C UF status`, re-checked 2026-09-26), not at `/home/ion/hp2-ufrontend/<fork>` as the task text and `HP2/CLAUDE.md` state. That breaks `yarn lint` and makes every publish "dirty". | yes (lint gate) | move them (§7, §11 Q10) |

Re-checked for this section (2026-09-26): `UF` HEAD `1bb9431`, `targets/` = the four existing folders only, `.ufront/`
has no `publish.local.json`; `HP2/vendor/glue/ue_abi.h` is byte-identical to `UF/packages/glue/abi/ue_abi.h` (`cmp`);
`HP2/vendor/glue/webrtc-shim.js` vs `UF/packages/glue/engine-shim/webrtc-shim.js` are **token-identical** once
whitespace, comments and prettier's trailing commas are removed (so §11 Q16 is closed: formatting-only drift, no
action for the target contract).

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

### 5.0 Prerequisites (one-time, in `UF`)

| Step | Command / fact | Evidence |
|---|---|---|
| deps | `cd /home/ion/hp2-ufrontend/ufrontend && yarn install` (Yarn 4.17, node-modules linker; `node_modules/` already present in the clone) | `UF/package.json` `packageManager` |
| machine paths | write `.ufront/publish.local.json` (§7) — `yarn build` has **no default target and no default paths** and dies without `--assets`/`assets.hp2` and `--engine-artifact`/`enginesRoot` | `UF/packages/build/src/build-inputs.mjs:73-117`, README "★ yarn build has NO default target" |
| engine artifact | `HP2/build-wasm-perf/SDLLaunch/{SDLLaunch.js,SDLLaunch.wasm,SDLLaunch.data,meta.json}` must exist (built in the ENGINE repo with `cmake --preset wasm-perf` — not by the frontend). Today **only `HP2/build-native/` exists** (`ls -d HP2/build-*`), so nothing below can run until Phase "web build" lands | `build-inputs.mjs:103-109` (`<enginesRoot>/<repo>/build-wasm-perf/<subpath>`) |
| meta.json | written POST_BUILD by the engine's emitter with `abiVersion` = glue `ABI_VERSION`, `loaderStyle: "global-module"`, sha256 checksums; `validateArtifactMeta` rejects anything else | `UF/packages/glue/src/artifact/meta.js:14,64-66`, README step 1.5 |
| wasm limits | no function may declare > 50,000 locals (V8); the build fails naming it, warns from 80 %. Relevant for HP2 because gl4es + ASYNCIFY at `-O0` crossed it on ut2004 — **never ship `wasm-debug` (-O0)** | `UF/packages/build/src/wasm-limits.mjs`, README step 2.19c |
| debug sibling | optional: `HP2/build-wasm/SDLLaunch/` (`wasm` preset, `-O2 -g`) is copied in as `SDLLaunch-debug.*` when present | `build-inputs.mjs:120-126`, README step 2.54 |

### 5.1 `yarn build --target hp2`

```bash
cd /home/ion/hp2-ufrontend/ufrontend
yarn build --help                                  # prints usage, builds nothing
yarn build --target hp2                            # paths from .ufront/publish.local.json (§7) → serve-web/
# or, explicit (no local config):
yarn build --target hp2 \
     --assets /home/ion/hp2-ufrontend/HarryPotter2/Assets \
     --engine-artifact /home/ion/hp2-ufrontend/HarryPotter2/build-wasm-perf/SDLLaunch \
     --out serve-web-hp2                           # side-by-side root (NOT what yarn e2e serves)
```

What happens, in order (`UF/packages/build/bin/ufront-build.mjs`):

1. `knownTargets` = every `targets/<d>/` holding a `target.json` (`:63-65`) → `hp2` is known as soon as the folder exists. **No code list to edit.**
2. `resolveBuildInputs` (`build-inputs.mjs:73-129`): assets ← `--assets` | `assets.hp2`; engine repo ← `engines.hp2` | `enginesRoot + "/" + publish.engine.repo` (`HarryPotter2`) | `UFRONT_ENGINES_DIR`; artifact ← `<repo>/build-wasm-perf/<engineArtifactSubpath||'SDLLaunch'>`. Each input printed with its source (`describeInputs`, `:132-145`).
3. Asset-tree proof (`ufront-build.mjs:91-110`): `sha256(Assets/System/Core.u)` vs `assetMarkers.fingerprint.sha256`; **a hash equal to another target's fingerprint is fatal**; `require` files must exist (case-sensitive — `Maps/startup.unr` lower-case), `forbid` must not.
4. Frontend build with `UFRONT_TARGET=hp2` (Vite bakes `targets/hp2/target.json` → `__UE_TARGET__` & co., §1.1), stamp `packages/frontend/.ufront-dist-target`; glue + cache-worker bundles.
5. Assemble `serve-web/`: Vite `dist/`, `SDLLaunch.{js,wasm,data}` + `meta.json` (validated), optional debug sibling, `file-manifest.json` (`:340`), `ufront-target.json` = `{target:"hp2", assets, builtAt}` (`:348-351`), `dependency-manifest.json` if present, `precache-list.json` from `targets/hp2/precache.json`, `theme.css` (**fatal if missing**, `:395-398`), then **symlinks** the manifest's top-level dirs from the asset tree: `Maps Music Sounds System Textures` (`:405-414`).
6. Refuses an origin-absolute `src=`/`href=` in the assembled `index.html` (`:327-337`) — irrelevant to hp2 data, but it is why a root publish and a subfolder publish are the same bytes (§6).

ACTION: none in code for `yarn build`; only the target folder (§1–§3) and `.ufront/publish.local.json` (§7).

### 5.2 `yarn serve`

```bash
yarn serve                                         # serve-web/ on :8080, COOP same-origin + COEP require-corp
yarn serve --root serve-web-hp2 --port 8093        # a side-by-side root
yarn serve --root serve-web-hp2 --port 8093 --tls --host 0.0.0.0   # from a phone/tablet/LAN IP (mkcert certs in .ufront/certs/)
yarn serve --content-encoding zstd                 # rehearse the Azure "Content-Encoding: zstd" mode on a publish tree
```

`UF/packages/build/bin/ufront-serve.mjs` → `src/static-server.mjs` (directory index, `/dir`→`/dir/` redirect that keeps the query, `.wasm` typed `application/wasm`, traversal refused, symlinks followed). ⚠ Plain HTTP only works on `localhost`: on a LAN IP COOP is ignored, `crossOriginIsolated` is false and the threaded engine dies with `DataCloneError: SharedArrayBuffer transfer requires self.crossOriginIsolated` (`yarn serve --help`).

Open `http://localhost:8080/`. Storage lands in `hp2-user-data` / `hp2-mods` / `hp2-network-cache` + OPFS `pkg/hp2/` (§1.2) — isolated from any other target served on the same `localhost:8080` origin earlier.

Hands-on boot check (before any spec): press Start with an empty map field → the engine must log, in order,
`Init: Name subsystem initialized` (`HP2/Source/Core/Src/UnName.cpp:157`), `Init: Game engine initialized`
(`HP2/Source/Engine/Src/UnGame.cpp:242`), the renderer's init line (`XOpenGL::SetRes…` for the default renderer —
TO-VERIFY in the HP2 fork's log), `Log: Bringing Level startup.MyLevel up for play (…)...` (`UnGame.cpp:1272`:
`debugf(NAME_Log, TEXT("Bringing %s up for play (%i)..."), GLevel->GetFullName(), …)` — GetFullName of a ULevel is
`Level startup.MyLevel`), `Log: Possessed PlayerPawn: …` (`HP2/Source/Engine/Src/UnActor.cpp:940`). The log file is
`<appUserDir()><appPackage()>.log` (`HP2/Source/Core/Inc/FOutputDeviceFile.h:40-49`), i.e. `/System/this.program.log`
on Emscripten exactly like ut99 **once `appUserDir()` returns `""` on web** (§1.4 ACTION).

### 5.3 Which E2E spec proves "the engine booted to the menu" headless

- `yarn e2e --target <id>` → `packages/build/bin/ufront-e2e.mjs` refuses unless `serve-web/ufront-target.json` says the same target (`:35-61`), then runs Playwright with `UFRONT_E2E_TARGET=<id>`; `packages/e2e/playwright.config.js:10,22-27` picks `<id>-wasm-{boot,net,benchmark,proxynet}.spec.js`. **A target with no spec file runs zero tests and passes** — the hp2 spec must be created.
- The webServer is `ufront-serve.mjs --port 8080` on the **default `serve-web/`** (`playwright.config.js:38-43`, `reuseExistingServer: true`). Build with the default out dir, not `--out serve-web-hp2`.
- Browser: Chromium `--headless=new --use-gl=angle --use-angle=vulkan …` (`playwright.config.js:44-60`); the specs delete `DISPLAY`/`WAYLAND_DISPLAY` so ANGLE binds the NVIDIA GPU. `lib/gpu.js assertHardwareRenderer` fails on a SwiftShader fallback — a GPU-free smoke must skip it and pick `Software (CPU)` or `Null (no rendering)` in `#render-device`.
- Closest template: **`packages/e2e/ut99-wasm-boot.spec.js` test 1** ("boots to the intro and renders via WebGL on the GPU (first light)", `:62-82`) with `lib/engines/ut99.profile.js` — same UE1 v400 log lines, same XOpenGL/UTGLR renderers, milestones read from the MEMFS log (`logPath: '/System/this.program.log'`) because a long synchronous boot overflows the DevTools console buffer. `unreal1-wasm-boot.spec.js:170-192` is the template for the SoftDrv discriminator (HP2 has SoftDrv; ut99 does not).

ACTION — add two files (both e2e DATA/spec, no generic code):

`packages/e2e/lib/engines/hp2.profile.js` (shape = `ut99.profile.js`):

```js
// Engine profile: hp2 (UE1 build 433 / Harry Potter and the Chamber of Secrets, 2002). Single-player: no net milestones.
const testMap = 'PrivetDr';
const bootMilestones = [
    { pattern: /Name subsystem initialized/, name: 'NAME_INIT' },           // Core/Src/UnName.cpp:157
    { pattern: /Init: Game engine initialized/, name: 'ENGINE_INIT' },      // Engine/Src/UnGame.cpp:242
    { pattern: /XOpenGL::SetRes|Initializing OpenGLDrv|Got OpenGL.*WebGL/i, name: 'RENDER_READY' }, // TO-VERIFY vs a real hp2 log
    { pattern: /Bringing Level startup\.MyLevel up for play/i, name: 'MENU_UP' }, // UnGame.cpp:1272 — the menu book map
    { pattern: new RegExp('(Browse|LoadMap):\\s*' + testMap, 'i'), name: 'BROWSE' },
    { pattern: /Bringing Level .* up for play/i, name: 'MATCH_START' },
    { pattern: /Possessed PlayerPawn/i, name: 'POSSESSED' },                // Engine/Src/UnActor.cpp:940
];
module.exports = {
    id: 'hp2', dbPrefix: 'hp2', peerPrefix: 'hp2',
    logPath: '/System/this.program.log',          // TO-VERIFY: needs appUserDir()=="" on web
    softwareRendererArg: '-SOFTWARERENDERER',
    bootMilestones, hostMilestones: [], clientMilestones: [],
    bootSurface: ['[FetchFS]', '[SHELL]', '[Prefetch]', '[PackageCache]', 'Bringing', 'Possessed', 'XOpenGL', 'OpenGLDrv', 'Got OpenGL', '[appThrowf]'],
    netSurface: [], testMap, dmGameArgs: () => '',
};
```

`packages/e2e/hp2-wasm-boot.spec.js` — start with three tests, add more as features land:

| # | Test | Drives | Asserts |
|---|---|---|---|
| 1 | boots to the menu (first light, GPU) | `goto('/')`, `assertHardwareRenderer`, click `#start-btn` with an empty map | `RENDER_READY`, `MENU_UP`, `POSSESSED`; > 3 `[FetchFS] Downloaded`; no `Aborted`/`RuntimeError` in `t.errors` |
| 2 | Software renderer, GPU-free | Advanced → `#render-device` = `Software (CPU)`, Start | `MENU_UP` reached, `RENDER_READY` **not** reached (copy `unreal1-wasm-boot.spec.js:170-192`) |
| 3 | direct map load | `goto('/?map=PrivetDr')`, Start | `BROWSE` + `MATCH_START` for PrivetDr, `POSSESSED`, no abort |

Then the shared suites that make sense for a single-player UE1 target (each is a `register…Tests(test, profile, …)` in `packages/e2e/lib/`): `registerLauncherUiTests`, `registerConfigPersistenceTests` (proves `hp2-user-data` round-trips `Game.ini`/`User.ini`), `registerSaveGameTests` (proves `userFiles` Save/Slot globs — the key HP2 feature), `registerIniSettingsTests` (frame-rate cap), `registerDebugEngineTests`, `registerTelemetrySilenceTest`. Skip `registerAutoFovTests`, `registerPlayerNameTests` (no multiplayer UI in-game), `registerMoverKeyframeTests` (ut99-specific repro), all net specs. ⚠ Each `register…` reads profile fields/selectors written for DM maps — read each before enabling (TO-VERIFY per helper).

Run:

```bash
yarn build --target hp2                           # the DEFAULT serve-web/ — the E2E serves only that
node packages/e2e/tools/e2e-gpu-guard.mjs -- --target hp2 hp2-wasm-boot.spec.js    # exit 2 = GPU lost, reboot + retry
# or without the guard:
yarn e2e --target hp2 hp2-wasm-boot.spec.js -g "boots to the menu"
```

### 5.4 The signaling server (only relevant if multiplayer is ever exposed)

HP2 is single-player, so no `hp2-wasm-net.spec.js` is needed. For reference: the net specs self-host a per-worker static server + a PeerJS broker (`packages/e2e/lib/net-harness.js`); for hands-on play `node packages/e2e/tools/peer-broker.cjs` (WSS :9443, mkcert certs) or `--port 9060 --no-tls`, and point the page at it with `?peerhost=<ip>&peerport=9443&peersecure=true` (`packages/e2e/README.md:1666-1690`; `frontend/src/config/controller.ts:149-167`). The production broker is the Azure `peerjs` container (§10).

### 5.5 Precache capture needs the same menu milestone

`packages/e2e/tools/precache-baseline.cjs:168-229` needs an `hp2` branch that waits for `MENU_UP` (above) then `settle()`s (§3.3 Step B). Reuse the profile's regex so the two stay identical.

## 6. Publish-at-root plan (no portal, no game selector)

**Verdict: there is no way today to publish one target at the publish root without a portal.** The app itself is
position-independent (Vite `base: './'`, `%BASE_URL%` in `index.html`, a build refusal of origin-absolute refs —
`docs/07` §8 "Mounting it at a subpath"; `ufront-build.mjs:327-337`), so the SAME bytes work at `https://host/` and at
`https://host/hp2/`. What forces a subfolder + portal is `publish-all` alone:

| Where | What it does now | Why it blocks a root publish |
|---|---|---|
| `packages/build/src/publish/args.mjs:217` | `if (!json.publish.folder) throw … publish.folder is required` | "" is refused; "." passes the check but… |
| `args.mjs:322-323` | `folder: json.publish.folder, outDir: \`${out.value}/${json.publish.folder}\`` | …`"."` gives `<out>/.` = the root (works by accident, never tested) |
| `packages/build/src/publish/run.mjs:647-666` | ALWAYS runs `build-portal.mjs --out <out>` with the cards from `discoverPublishedFolders` | writes `<out>/index.html` **over the game's own `index.html`** — the fatal step |
| `run.mjs:522-545` `discoverPublishedFolders` | scans SUBDIRECTORIES for `ufront-target.json` + `index.html` | a root game is never a card → a 0-card portal (`build-portal.mjs:45` then dies: "at least one --game … is required") — either way the run fails or the page is destroyed |
| `packages/build/src/publish/state.mjs:74-90` `claimOutRoot` | the root record's target list is a UNION keyed by folder | a `"."` record mixed with `u1/ ut99/ …` in one `--out` would be accepted silently |
| `packages/portal/bin/build-portal.mjs:99-102` | temp+rename `index.html` at `--out` | same overwrite |
| `run.mjs:396, 446, 491` | dry-run prints `folder <out>/<folder>`, `[portal] …` | cosmetic (`<out>/.`), and it would announce a portal that must not run |
| `/home/ion/unreal-frontend-azure-infra/bin/deploy-ufront.py:237-240` | refuses a root lacking `config.json storage.requiredFolders` = `u1 ut99 ut99-436 ut2004` | an HP2-only tree is refused by the uploader (infra side, §10) |

### 6.1 Track A — works today, zero ufrontend code (use until 6.2 lands)

```bash
# target.json: "publish": { "folder": "hp2", … }   (instead of ".")
yarn publish-all --out /home/ion/publish-hp2 --target targets/hp2/target.json --dry-run
yarn publish-all --out /home/ion/publish-hp2 --target targets/hp2/target.json
#   → /home/ion/publish-hp2/index.html        one-card portal  — NOT uploaded
#   → /home/ion/publish-hp2/hp2/              the game          — uploaded AS THE SITE ROOT
yarn serve --root /home/ion/publish-hp2/hp2 --tls          # boot it at "/" exactly as the domain will
yarn verify-site --out /home/ion/publish-hp2/hp2 --url https://<hp2-host>   # walks OUT, requests <url>/<rel>
```

Costs: an extra 1-card `index.html` + `.ufront-publish-root.json` + `publish-provenance.json` one level up that must
not be uploaded (they are not in `hp2/`), so an uploader pointed at `<out>/hp2` is correct. `hp2/` itself contains the
per-target `.ufront-publish.json` and `ufront-target.json` (small, harmless if uploaded — the multi-game site uploads
the same files today). Provenance of the run stays in `<out>/publish-provenance.json`, off the site.

⚠ With Track A, `publish.folder` must be `"hp2"`, i.e. §1.3's `"."` is replaced; record which track is live in
`targets/hp2/README.md`.

### 6.2 Track B — first-class root publish (recommended; generic, no game name in code)

Rule: **a target whose `publish.folder` is `"."` IS the site.** It publishes directly into `--out`, alone, and no
portal is rendered. Changes (all in `packages/build/src/publish/` + tests + docs; `packages/portal` unchanged):

| # | File:line | Change |
|---|---|---|
| 1 | `args.mjs:217` | keep the required check; add: `const isRoot = (f) => f === '.';` and, when any descriptor has `isRoot(folder)`, `if (descriptors.length !== 1) throw new Error(\`${id}: publish.folder "." publishes this target AS the site root — it must be published alone (got ${ids.join(', ')})\`)`. Also refuse `"/"`, `""`, `".."`, anything with `/` (a folder is a single path segment). |
| 2 | `args.mjs:323` | `outDir: isRoot(json.publish.folder) ? out.value : \`${out.value}/${json.publish.folder}\`` (no `<out>/.`). |
| 3 | `args.mjs:331-357` | return `siteRoot: targets.length === 1 && isRoot(targets[0].folder) ? targets[0].id : null` in the plan. |
| 4 | `run.mjs:40-59` `checkOutOwnership` (or a new check beside it, called from the planning pass at `run.mjs:349`) | **refuse mixing both shapes in one `--out`**: in site-root mode, a subdirectory holding `ufront-target.json` is a multi-game tree → refuse; in portal mode, `<out>/ufront-target.json` present → a root-game tree, the portal would overwrite its `index.html` → refuse. Both messages name the other shape and say "use another --out". |
| 5 | `state.mjs:74-90` `claimOutRoot` | record `{folder: '.'}`; if `prior.targets` contains a different folder than this run's shape allows, refuse (same message as #4). Keeps "the union is load-bearing for the portal" true for portal roots only. |
| 6 | `run.mjs:647-666` | `if (plan.siteRoot) log(\`· portal    none — ${plan.siteRoot} is published AT the root (publish.folder "."); no game selector\`); else { …existing discover + portalArgv… }`. `phaseVerify`, `writeProvenance`, `clearIncomplete` run unchanged. |
| 7 | `run.mjs:396, 446, 491` | dry-run: print `folder <out>   (site root)` and `[portal] none (site root)` instead of the `build-portal` command. |
| 8 | `execute.mjs:292-360` `phaseVerify` / `verify.mjs:139-170` | no logic change; message prefix `${target.folder}/` reads `./` — print `(root)` instead. The byte-identical-engine sweep (`verify.mjs:119-137`) is per run and still fine. |
| 9 | `ufront-build.mjs:256-282` (deployable refresh) | **no change needed**: with `--no-asset-links` it deletes only its own `WRITES` from `out`, so `zstd/`, `.ufront-publish*.json`, `publish-provenance.json`, `.ufront-publish-incomplete` survive. Worth a comment that `out` may be a site root. |
| 10 | `packages/e2e/tools/symbolize-wasm.cjs:200-209` | `publishFolder()` returns `"."`; make the URL builder drop a `.` segment (`folder === '.' ? site : site + '/' + folder`) instead of relying on curl's dot-segment squashing. |
| 11 | `packages/e2e/tools/live-session.cjs:80` | default `--path '/ut2004/'`; for hp2 pass `--path '/'` (no code change; document). |
| 12 | `packages/build/bin/ufront-publish.mjs` (help) + `packages/build/README.md` § Publishing + `docs/07` §8 + `docs/06` (publish block) | document `"."`, the alone-rule, the no-portal rule, the mixing refusal. `doc-sync` requires the doc in the same commit (§9). |
| 13 | tests | `packages/build/test/publish-args.test.mjs` (next to `:103` "the output folder comes from publish.folder"): `"."` → `outDir === out`; `"."` + a second target → throws; `"a/b"`, `".."` → throw. `publish-state.test.mjs`: mixing refused. A pure test for the portal skip (factor the portal decision into a pure `portalPlan(plan, cards)` in `run.mjs` so it is testable without a filesystem). |

Resulting daily command (with §7's local config, where `out` is the HP2 tree):

```bash
yarn publish-all --target targets/hp2/target.json --dry-run    # expect: "folder /home/ion/publish-hp2 (site root)", "portal none"
yarn publish-all --target targets/hp2/target.json
yarn serve --root /home/ion/publish-hp2 --tls                   # boot at "/"
yarn verify-site --url https://<hp2-host>                       # out + siteUrl from publish.local.json
```

★ The storage state files at the site root (`.ufront-publish.json`, `.ufront-publish-root.json`,
`.ufront-publish-incomplete`, `publish-provenance.json`) would be uploaded by a naive sync. They reveal the monorepo
sha, dirty-file list and local paths (`collectProvenance`, `execute.mjs:363-…`: `monorepo.dirtyFiles`, asset paths).
In the four-game tree they sit at the root too, next to the portal, so this is not new — but ACTION (infra): have
the HP2 uploader skip `^\.ufront-` and `publish-provenance.json` (or confirm the owner accepts publishing them).

### 6.3 What does NOT need to change for a root publish

- The frontend: every runtime URL is relative (`fetch('precache-list.json')`, `new Worker('ufront-cache-worker.js')`,
  `s.src='SDLLaunch.js'`, FetchFS `baseUrl ''`) — `docs/07` §8.
- Storage names: per-origin anyway (§1.2).
- The telemetry meta tag: injected into the game's own `index.html` by the app phase (`execute.mjs:166-175`), so a
  root game reports exactly like a subfolder game; the portal's inlined copy is simply absent.
- Invite links (`?connect=`, `?room=`): built from `location` (`frontend/src/stores/net.ts:292-296`), so they carry
  the root URL. The `/dir`→`/dir/` redirect concern (`README` step 2.36a) disappears at the root.

## 7. `.ufront/publish.local.json` for this workspace

Path: `/home/ion/hp2-ufrontend/ufrontend/.ufront/publish.local.json` — **does not exist yet** (`ls .ufront/` shows
`README.md gates hooks install-engine-hooks.sh not-yet.mjs`). Gitignored (`UF/.gitignore`: `.ufront/publish.local.json`),
so it is never committed and never trips a gate.

Keys actually read (grep of `localConfig.*` in `packages/build/**` + `cfg.*` in `ufront-verify-site.mjs` + the two
e2e tools): `out`, `assets.<id>`, `enginesRoot`, `engines.<id>`, `siteUrl`, `telemetryEndpoint`, `gameProxyList`
(and the refused legacy `gameProxyUrl`), `compress`, `level`, `jobs`, `engineJobs`, `emsdk`, `install`.

```json
{
  "out": "/home/ion/publish-hp2",
  "assets": {
    "hp2": "/home/ion/hp2-ufrontend/HarryPotter2/Assets"
  },
  "enginesRoot": "/home/ion/hp2-ufrontend",
  "emsdk": "/home/ion/emsdk"
}
```

Add after the HP2 domain exists (§10), exactly as `apply-outputs.sh` would write them:

```json
  "siteUrl": "https://<hp2-endpoint-host>",
  "telemetryEndpoint": "https://<hp2-endpoint-host>/e"
```

| Key | Value | Who reads it / derivation |
|---|---|---|
| `out` | `/home/ion/publish-hp2` — a NEW directory, never `/home/ion/publish` (the four-game tree) | `args.mjs:245-255` (`--out` flag wins); `ufront-verify-site.mjs:52`. Must be empty or already owned (`.ufront-publish-root.json`, `run.mjs:40-59`). Track A (§6.1) deploys `<out>/hp2/`; Track B (§6.2) deploys `<out>/`. |
| `assets.hp2` | `/home/ion/hp2-ufrontend/HarryPotter2/Assets` | `build-inputs.mjs:81-84` (yarn build) and `args.mjs:300-311` (publish). Keyed by the target **id** `hp2`, not the folder. Proven by `assetMarkers` before anything runs. |
| `enginesRoot` | `/home/ion/hp2-ufrontend` | + `publish.engine.repo` `HarryPotter2` → `/home/ion/hp2-ufrontend/HarryPotter2` (`build-inputs.mjs:94-102`, `args.mjs:292-317`). ⚠ Without it publish-all's default is `<repoRoot>/../engines` = `/home/ion/hp2-ufrontend/engines`, which does not exist. |
| `engines.hp2` | (omit) | per-target override, only if the repo ever moves away from `enginesRoot`. |
| `publish.engine.presetDir` | `Source` (in target.json, not here) | `engine.mjs:63,77`: `cmake --preset wasm-perf` / `wasm` run in `HarryPotter2/Source`; `buildDir = <repo>/build-<preset>` (`engine.mjs:64`) = `HarryPotter2/build-wasm-perf`, matching `HP2/Source/CMakePresets.json:51-55` `binaryDir ${sourceDir}/../build-wasm-perf`. Presets `wasm` + `wasm-perf` both exist (`:26,51`). |
| `emsdk` | `/home/ion/emsdk` (exists) | `args.mjs:345-350`; default is `$HOME/emsdk` anyway — listed for `--dry-run` clarity. |
| `siteUrl` | the HP2 Front Door endpoint | `yarn verify-site`, `live-session.cjs:61-76`, `symbolize-wasm.cjs:183-195`. |
| `telemetryEndpoint` | `<siteUrl>/e` | `args.mjs:259-267`; injected into `index.html` by the app phase. **Absent ⇒ the published HP2 page is silent** (dormant client). |
| `gameProxyList` | **omit** | HP2 declares no `network.proxy`, and Proxy networking is for native servers (none exist for HP2). |
| `compress`/`level`/`jobs`/`engineJobs` | omit (zstd -19, half the threads, all threads) | `args.mjs:290-291, 343-344`. |

⚠ **Infra coupling.** `/home/ion/unreal-frontend-azure-infra/bin/apply-outputs.sh:15, 65-66` writes `siteUrl` +
`telemetryEndpoint` into `<config.json .ufrontend>/.ufront/publish.local.json`, and `config.json` has
`"ufrontend": "/home/ion/ufrontend"` — the UPSTREAM clone. An HP2 site added to that repo must write into THIS clone's
file (a per-site `ufrontend` path), or the two values are typed by hand. ACTION (infra, §10).

⚠ **Engine tree must be clean for publish-all.** `run.mjs:312-316` refuses a dirty engine repo (`--allow-dirty-engines`
records it instead). `git -C HP2 status --short` currently shows modified `docs/porting/analysis/*.md` and an
untracked `LightBits` — commit or ignore before the first publish. The analysis reports themselves make the tree
dirty until committed.

⚠ **The renderer forks are NOT where the task/CLAUDE.md says.** `ls /home/ion/hp2-ufrontend` lists only
`HarryPotter2 ufrontend`; the forks are INSIDE the frontend clone: `UF/XOpenGLDrv` (branch `hp2-v433-compat`),
`UF/utglr-copy` (`hp2`), `UF/gl4es` (`hp2`) — each its own git repo, and `git -C UF status` lists them as untracked
(`?? XOpenGLDrv/ gl4es/ utglr-copy/`). Consequences for the ufrontend side:
1. `yarn lint` = `eslint . && prettier --check .` walks them: `UF/XOpenGLDrv/glm/doc/**` carries ~100 `.js/.css`
   files (jquery.js, doxygen.css…) that neither `.prettierignore` nor `eslint.config.mjs` ignores → the `lint` gate
   (`.ufront/gates/lint.sh`) fails on vendored code.
2. A `git add -A` in `UF` would try to add three nested repos (as gitlinks) to the monorepo.
3. `publish-all` hashes the monorepo's dirty state into the app fingerprint (`freshness.mjs:157-160`) and records
   `dirtyFiles` in the provenance; untracked dirs make every publish "dirty".
4. `HP2/Source/CMakeLists.txt:249` expects them "at the sibling clones under /home/ion/hp2-ufrontend".
ACTION: move them to `/home/ion/hp2-ufrontend/{XOpenGLDrv,utglr-copy,gl4es}` (plain `mv`; they are independent
repos) — do NOT add them to the monorepo's ignore files as a workaround. Verify: `git -C UF status --short` empty
of them; `yarn lint` green.

## 8. Places that hardcode the existing targets

Method: `grep -rnE "ut99-436-compat|'unreal1'|\"unreal1\"|'ut2004'|\"ut2004\"|'ut99'|\"ut99\"" packages test .ufront`
(code/test/shell/json, excluding generated manifests) → 60 files; each was opened and classified. Most hits are
comments, fixtures or per-target profiles, which are correct as-is. **What actually discovers targets by folder
(so needs nothing):** `ufront-build.mjs:63-65` (`knownTargets`), `ufront-build.mjs:91-110` (sibling fingerprints
from every descriptor), `build-inputs.mjs:76` (unknown-target error lists them), `publish-all`'s fingerprint
sweep (all descriptors on disk, README "Fingerprints are compared against every target the repo declares"),
`run.mjs:533` (`targets/<marker.target>/target.json`), `vite.config.ts:25` (`UFRONT_TARGET` → any folder).

### 8.1 MUST touch (a fifth target is silently skipped or broken otherwise)

| File:line | Hard-coded | ACTION for hp2 |
|---|---|---|
| `test/skeleton.test.mjs:76` | `const TARGETS = ['unreal1','ut99','ut99-436-compat','ut2004']` | add `'hp2'` or discover (§4.3 #1) |
| `test/skeleton.test.mjs:200-209` | `CAPTURED` telemetry fixture rows per target | add verbatim hp2 rows once captured (§4.2) |
| `packages/frontend/test/capabilities.test.ts:63` | `for (const t of ['ut2004','ut99','ut99-436-compat','unreal1'])` | add `'hp2'` (or read `targets/`) |
| `packages/frontend/test/net-notices.test.ts:119` (UE1 set) and `:146` (all) | the same lists | add `'hp2'` to both |
| `packages/frontend/test/proxy-net-notices.test.ts:170` | `['unreal1','ut99']` — targets WITHOUT Proxy networking | add `'hp2'` (it declares no `network.proxy`) |
| `packages/glue/test/_target-fixtures.cjs` + `target-config.test.cjs:76-95` | UT2004/UT99/UNREAL1 fixtures | add an HP2 fixture (§4.4) — proves `hp2-user-data`, `.ogg → Music`, `.usa → Save`, the Slot globs |
| `packages/e2e/playwright.config.js:10,22-27` | spec name pattern `<id>-wasm-*.spec.js` | no edit — **create** `packages/e2e/hp2-wasm-boot.spec.js` + `lib/engines/hp2.profile.js` (§5.3) |
| `packages/e2e/tools/precache-baseline.cjs:41,168-229` | `TARGET === 'unreal1' / 'ut99' / 'ut99-436-compat'`, else **ut2004 menu clicks** | add an `hp2` branch (wait for the startup level, settle) — else the precache capture drives UT2004 keys into HP2 (§3.3 B) |
| `packages/e2e/tools/precache-capture.cjs:111-113,202` | waits for `UT2k4MainMenu.Opened`, loads the ut2004 profile | hp2 branch or use `precache-baseline.cjs` only |
| `packages/e2e/tools/publish-boot-check.cjs:17-29` | `TARGETS = [{dir:'u1'},{dir:'ut99'},{dir:'ut99-436'},{dir:'ut2004',…}]` against `PUBLISH_URL` | add `{ dir: '.', ready: /Possessed PlayerPawn/i, renderer: 'Software (CPU)' }` for a root publish (or make the list come from the served `ufront-target.json`/descriptors) — this is the "boot the publish folder before uploading" proof |
| `/home/ion/unreal-frontend-azure-infra/peerjs/server.js:110` | `ID_PREFIXES = ['u1','ut99','ut99-436-compat','ut2k4']`, `ID_ENFORCE=reject` | add `'hp2'` **only if** HP2 ever exposes P2P; otherwise hp2 peer ids are rejected (harmless for single-player) — infra repo, §10 |
| `/home/ion/unreal-frontend-azure-infra/config.json` `storage.requiredFolders` + `bin/deploy-ufront.py:237-240` | the four folders | per-site config (§10) |

### 8.2 SHOULD touch (tooling that defaults to/knows only the four; hp2 works via flags)

| File:line | What | ACTION |
|---|---|---|
| `.ufront/install-engine-hooks.sh:19-24` | default repo list = the four `/home/ion/engines/engine-*` | run it with an explicit path: `.ufront/install-engine-hooks.sh /home/ion/hp2-ufrontend/HarryPotter2` (installs the engine pre-push guard as a symlink into `HP2/.git/hooks/pre-push`; allowed — it writes only the hook) — or add the path to the default list in this branch |
| `packages/build/bin/ufront-e2e.mjs:31`, `playwright.config.js:10`, `vite.config.ts:25` | default target `ut2004` when none is given | always pass `--target hp2`; no change |
| `packages/e2e/tools/asset-audit.mjs:36`, `missing-deps-report.mjs:43` | `TARGETS = {id: '/home/ion/<x>-assets'}` path maps | pass `--target hp2 --assets /home/ion/hp2-ufrontend/HarryPotter2/Assets`; optionally read `.ufront/publish.local.json` `assets.*` instead of a path map |
| `packages/e2e/tools/native-smoke.mjs:73-140` | engine entries unreal1/ut99/ut2004 | add an hp2 native entry when the native Linux build boots |
| `packages/e2e/tools/encoding-bench.mjs:49`, `relay-conformance.cjs:24` | defaults `ut99` | pass `--target hp2` / `--profile …` |
| `packages/e2e/tools/symbolize-wasm.cjs:200-209` | `publish.folder` → URL segment | handle `"."` (§6.2 #10) |
| `packages/e2e/tools/live-session.cjs:80` | default `--path '/ut2004/'` | pass `--path '/'` for hp2 |

### 8.3 No change (comments, fixtures, per-target files)

`packages/build/{bin,src/publish}/*` hits (`ufront-build.mjs:343`, `ufront-publish.mjs:60-61`, `args.mjs:230,309`,
`engine.mjs:157-158`, `execute.mjs:350-351`, `state.mjs:78`, `verify.mjs:123`) are explanatory comments/messages
about the ut99 vs ut99-436 identity problem; `packages/build/test/publish-*.test.mjs` use the four as fixtures (a
new root-publish test belongs beside them, §6.2 #13); `packages/glue/test/{artifact-meta,ini-identity,wasmfs-backend}.test.cjs`
fixture data; `packages/portal/test/render.test.mjs` card fixtures; `packages/e2e/lib/engines/*.profile.js`,
`*.proxynet.js`, `lib/gui/shared.js:5`, `lib/autofov-tests.js:27` per-target data/comments;
`packages/glue/bin/generate-travel.cjs:40,325` comments; `packages/frontend/test/network-section.test.ts:50,197,277`
(proxy `profile: 'ut2004'` fixtures).

### 8.4 Docs that enumerate "the four targets" (prose; update when hp2 lands so doc-sync reviewers are not misled)

| File:line | Text | ACTION |
|---|---|---|
| `docs/README.md:69` | architecture diagram boxes ut2004 / ut99 / unreal1 | add an `hp2 (UE1 433)` box |
| `CLAUDE.md` (UF) "What this is" | "four yarn-workspace packages" (packages, not targets) | none |
| `packages/frontend/README.md:500` | "the deployment is four games side by side under one origin" | add: "hp2 is published at the root of its own origin (publish.folder ".")" |
| `packages/e2e/README.md:89,123,157,952,1047-1092,1381,1494,1693` | "all four boot specs", the per-target save table (`:1091-1092`) | add hp2 rows as each shared suite is registered in `hp2-wasm-boot.spec.js`; the save table needs an hp2 row (menu book → save slot; `Save/Slot<N>/Save<i>.usa`) |
| `docs/06-targets-and-branding.md` §3 (travel), publish block | the travel rule and `publish.folder` semantics | §4.3 #3 and §6.2 #12 |
| `docs/07-build-and-ci.md:146-160, 228-251` | the four-target publish command; "drop the four folders side by side" | add the single-site command (§6) |
| `packages/build/README.md` § Publishing + `.ufront/publish.local.json` example | four `assets.*` keys | add the site-root mode + an hp2 example |

## 9. Gates, hooks, CHANGELOG/STATE rules

### 9.0 Hooks are not installed in this clone

`git -C UF config --get core.hooksPath` → empty, and `UF/.git/hooks/` holds only `*.sample`. So today **no gate runs
on commit/push in the hp2 clone**; only `yarn gates` (CI tier) runs them, by hand. ACTION (owner, one-time, per
`.ufront/README.md` "Enable the git hooks"): `git -C /home/ion/hp2-ufrontend/ufrontend config core.hooksPath .ufront/hooks`.
For the engine: `.ufront/install-engine-hooks.sh /home/ion/hp2-ufrontend/HarryPotter2` (§8.2).

### 9.1 Gate-by-gate: what a new `targets/hp2/` does to each

| Gate (file) | Tier | Trips on the hp2 work? | What must accompany it |
|---|---|---|---|
| `no-engine-specifics` (`.ufront/gates/no-engine-specifics.sh`) | hard, `yarn gates` | **No** — it scans only `packages/{frontend,glue}` (check 1, `:20-30`) and `packages/glue/src` + `packages/frontend/{src,public}` (check 2, `:33-41`); `targets/` is DATA. ⚠ But its patterns know only `ut2004\|ut2k4\|ut99\|unreal1` (`:27`, `:38`), so an `'hp2'` literal/branch in generic code would **pass unnoticed**. (`grep -rniE hp2 packages/{glue,frontend}/src packages/frontend/public` → 0 hits today, so adding it causes no false positive.) | ACTION: add `hp2` to both regexes in the same commit that adds the target: `…(ut2004\|ut99\|unreal1\|hp2)…` at `:27` and `"ut2004\|ut2k4\|ut99\|unreal1\|hp2"` at `:38`; update the header comment (`:7`). |
| `doc-sync` (`gates/doc-sync.sh:12-21`) | hard, pre-commit | Only when `packages/<p>/(src\|bin\|lib)/` is staged without `packages/<p>/(README.md\|CHANGELOG.md\|docs/)`. `targets/**`, `test/**`, `packages/*/test/**`, `packages/e2e/*.spec.js` (package root) and `packages/e2e/tools/**` do **not** trigger it. They DO trigger: `packages/e2e/lib/engines/hp2.profile.js` (→ `packages/e2e/README.md`), `PlayPanel.vue` (`packages/frontend/src/` → `packages/frontend/README.md`), every §6.2 change (`packages/build/src/publish/*`, `bin/ufront-publish.mjs` → `packages/build/README.md`). | stage the matching README section in the same commit |
| `lint` (`gates/lint.sh` → `yarn lint` = `eslint . && prettier --check .`) | hard, pre-commit when js/ts/vue staged + `yarn gates` | New `hp2-wasm-boot.spec.js`, `hp2.profile.js`, `branding/theme.css` must be prettier-clean (4 spaces, 120 cols, single quotes); `*.json`/`*.md`/`*.svg`/`*.html` are prettier-ignored (`.prettierignore`), manifests eslint-ignored (`eslint.config.mjs:48`). ⚠ **Fails today regardless** because of the nested fork checkouts (`UF/XOpenGLDrv/glm/doc/**/*.js`, §7). | `yarn lint:fix` on the new files; move the forks out |
| `unit` (`gates/unit.sh:23-27`) | hard, `yarn gates` | `yarn test` (skeleton) fails if `'hp2'` is in `TARGETS` and `targets/hp2/file-manifest.json` is missing, or with an empty `travel` block before §4.3 #3; the frontend/glue loops only include hp2 once added (§8.1). | order of §4.5 |
| `abi-parity` (`gates/abi-parity.sh`) | hard, `yarn gates` | No — no ABI change. (Engine side: `HP2/vendor/glue/ue_abi.h` is byte-identical to `UF/packages/glue/abi/ue_abi.h`, checked with `cmp`.) | — |
| `placement-lint` (`gates/placement-lint.sh`) | warn, pre-commit | Only code files at the repo ROOT. `targets/hp2/*` is fine. | — |
| `conventional-commits` | warn, commit-msg | Use `feat(hp2): add the hp2 target descriptor …`, `test(skeleton): …`, `feat(publish): site-root publish (publish.folder ".")`, `docs(…)`. | — |
| `e2e-pre-push` (`gates/e2e-pre-push.sh:11-20`) | **hard, pre-push** | Blocks any push unless `.ufront/e2e-stamp` (gitignored, local) contains HEAD's sha. | run the hp2 boot spec (§5.3) green, append HEAD to the stamp; the docs-only-carry convention is in STATE ("validated at <sha>; HEAD differs by docs-only") |
| `state-updated` (`gates/state-updated.sh`) | warn, pre-push | warns if `docs/migration/STATE.md` did not change in HEAD | add the STATE entry (9.3) in the last commit of the push |
| `engine-pre-push` (`hooks/engine-pre-push`) | hard, engine clones | Only once installed in `HP2/.git/hooks`; blocks a force-push that drops a remote commit's content. | install (§9.0) |

### 9.2 CHANGELOG rules

`docs/08` §5 (`:75`): "A short CHANGELOG per package records **contract-affecting** changes (especially
`ABI_VERSION`)". Only `packages/glue/CHANGELOG.md` exists. Adding a target is DATA → **no CHANGELOG entry**. An HP2
glue fixture/test → none. The §6.2 site-root publish is a `packages/build` contract change (a new legal
`publish.folder` value + refusals): document it in `packages/build/README.md` (doc-sync satisfies with the README;
`packages/build` has no CHANGELOG — do not create one unless the owner wants it).

### 9.3 STATE.md / steps / ROADMAP rules

- `docs/09` §2 (`:40-63`): **every step updates `STATE.md`**; `docs/08` §6 (`:77-83`): read STATE first, update
  STATE/CHANGELOG last. STATE is **newest-first**: a new top entry under the header block, shaped like the
  existing ones (`STATE.md:369-405` for 2.75): `★★★ **<date> (<step>) — <TITLE IN CAPS>**`, `Record:
  steps/<step>-<slug>.md. In one screen:`, then a fenced block with `report / cause / fix / e2e / swept / shipped / open`.
- ACTION: create `docs/migration/steps/<next-step>-hp2-target.md` (the record), add a `ROADMAP.md` checkbox, and a
  STATE top entry naming: the branch `hp2`, the engine repo `HarryPotter2 @ <sha>`, the hp2 boot spec result
  (`N/N at <sha>`), the stamp, and **open items** (the TO-VERIFY list of §11).
- Relevant STATE precedents to cite in the step record: 2.53 (telemetry patterns are DATA — `STATE.md:785`),
  2.75 (`userFiles`, `STATE.md:369-405` — note `LoadMap ..\Save\Save0.usa` works on the UE1 web build, which is
  evidence for the HP2 `..\\Save` path question in §1.4), 2.79 (network modes: absent `network.proxy` ⇒ P2P only).

## 10. Azure: a second, HP2-only domain (plan only)

Sources (READ ONLY): `/home/ion/unreal-frontend-azure-infra/{README.md, CLAUDE.md, config.json, docs/ARCHITECTURE.md,
docs/RUNBOOK.md, infra/main.bicep, infra/modules/{site,frontdoor,apps,logs,logs-access,gameproxy}.bicep,
bin/deploy-ufront.py, bin/verify-site.mjs, bin/apply-outputs.sh, peerjs/server.js, relay/src/server.mjs}`.
Nothing was executed against Azure. `INFRA` below = that repo.

### 10.1 What exists today (the one site)

| Piece | Name (config.json) | Declared in | Relevant fact for HP2 |
|---|---|---|---|
| static site | storage `unrealwebassets`, container `$web`, `indexDocument index.html`, `requiredFolders [u1, ut99, ut99-436, ut2004]` | `infra/modules/site.bicep:13-47`; static-website switched on by `bin/infra-deploy.sh:76-77` (data-plane) | one `$web` = one root; HP2 at the root needs its own `$web` |
| CDN | Front Door **Standard** profile `unrealfd`, endpoint `unrealgames` (`unrealgames-bubshdc7hmg8h7b8.z03.azurefd.net`) | `frontdoor.bicep:21-33` | a Standard profile holds several endpoints; the base fee is per profile |
| origin groups | `storage-og` (probe `/u1/index.html`, 240 s), `relay-og` (probe `/healthz`) | `frontdoor.bicep:37-95` | HP2 needs its own storage origin group (a probe path that exists at ITS root) |
| COOP/COEP | rule set `coiheaders`, rule `addcoiheaders`: Overwrite `Cross-Origin-Opener-Policy: same-origin` + `Cross-Origin-Embedder-Policy: require-corp` on every response | `frontdoor.bicep:97-130` | rule sets are profile-level ⇒ **reuse it** on the HP2 route |
| routes | `default-route` `/*` → storage-og, HTTPS redirect, `IgnoreQueryString`, compression for `frontDoor.compress` types; `telemetry-route` `/e`, `/e/*` → relay-og, not cached | `frontdoor.bicep:132-166` | HP2 needs both on its endpoint |
| zstd | `.zst` uploaded as `application/octet-stream` + `Content-Encoding: zstd`, `Cache-Control public, max-age=86400`; `.wasm` stored brotli (`Content-Encoding: br`), app files `no-cache` | `bin/deploy-ufront.py:91-103`, `config.json cachePolicy` | same uploader behaviour serves HP2 (the glue byte path accepts both encoded and opaque, `docs/07` §8) |
| broker | App Service `unrealpeer-we` (`wss://unrealpeer-we.azurewebsites.net/peerjs`), `ALLOWED_ORIGINS = siteOrigin + extraAllowedOrigins`, `ID_ENFORCE=reject`, `ID_PREFIXES ['u1','ut99','ut99-436-compat','ut2k4']` | `apps.bicep:27-55`, `peerjs/server.js:45-75,110-114` | the glue default broker host is `unrealpeer-we.azurewebsites.net` (`glue/src/webrtc/index.js:181`) — HP2 pages will talk to it as-is |
| relay | App Service `unrealtel-we`, `ALLOWED_ORIGINS = siteOrigin` (CORS fallback only; same-origin through Front Door is the shipped path), accepts any origin's POST otherwise | `apps.bicep:59-90`, `relay/src/server.mjs:12-25,65-75` | reusable unchanged if HP2 gets its own `/e` route |
| logs/alerts | workspace `law-unreal-signal` (0.05 GB/day cap), FD access logs at PROFILE scope (`frontdoor.bicep:169-176`), `alert-telemetry-silence` counts GETs matching `^https?://[^/?]+/(u1\|ut99\|ut99-436\|ut2004)?/?(index[.]html)?([?].*)?$` (`logs.bicep:171-185`) | | ⚠ that regex matches **`/` on any host** — HP2's root page loads count as "pages" |
| writeback | `bin/apply-outputs.sh:15,65-66` writes `siteUrl`/`telemetryEndpoint` into `<config.ufrontend>/.ufront/publish.local.json`; `config.ufrontend = /home/ion/ufrontend` | | wrong clone for HP2 |

### 10.2 Options

| Option | What | Verdict |
|---|---|---|
| **A. Same profile `unrealfd`, second endpoint + second storage account** | new storage `hp2webassets` (`$web`), new FD endpoint `hp2games`, origin group `hp2-storage-og` → `hp2webassets.z6.web.core.windows.net`, route `hp2-route` `/*` (reuses `coiheaders`, same cache/compression), route `hp2-telemetry-route` `/e`,`/e/*` → existing `relay-og` | **Recommended.** No new profile fee; complete isolation of `$web` (the four-game mirror-sync can never delete HP2 blobs); own hostname = own origin (own IndexedDB/OPFS). |
| B. Same profile, same storage, HP2 under `$web/hp2/`, route with `originPath: '/hp2'` | one storage, FD rewrites `/x` → `/hp2/x` | Not recommended: `deploy-ufront.py` mirror-syncs the WHOLE container (`to_delete = remote − local`, `deploy-ufront.py:259`), so the next four-game upload proposes deleting every `hp2/` blob; HP2 also appears at `unrealgames…/hp2/`; the `requiredFolders` guard and the silence regex both need per-folder special cases. |
| C. A second Front Door profile | separate everything | Pays a second Standard base fee for no gain; loses the shared rule set/relay origin group. |

### 10.3 Option A — changes, file by file (plan only)

**`INFRA/config.json`** — add one block (every name lives here, `CLAUDE.md` rule 1), e.g.:

```json
"hp2Site": {
  "_comment": "Harry Potter and the Chamber of Secrets, published at the ROOT of its own endpoint (no portal). Built by the hp2 clone of ufrontend.",
  "ufrontend": "/home/ion/hp2-ufrontend/ufrontend",
  "publishFolder": "/home/ion/publish-hp2",
  "siteUrl": "",
  "storage": { "account": "hp2webassets", "container": "$web", "webHost": "", "sku": "Standard_LRS", "indexDocument": "index.html",
               "requiredFiles": ["index.html", "SDLLaunch.wasm", "SDLLaunch.js", "file-manifest.json", "zstd"] },
  "frontDoor": { "endpoint": "hp2games", "endpointHost": "", "originGroup": "hp2-storage-og", "origin": "hp2-storage-origin",
                 "healthProbe": { "path": "/index.html", "intervalSeconds": 240 },
                 "routes": { "site": "hp2-route", "telemetry": "hp2-telemetry-route" } }
}
```

(Track A of §6.1 makes `publishFolder` `/home/ion/publish-hp2/hp2`; Track B `/home/ion/publish-hp2`.)

| File | Change |
|---|---|
| `infra/modules/site.bicep` | parameterise by a `s` object (today `var s = cfg.storage`, `:11`) and instantiate twice from `main.bicep` (`cfg.storage`, `cfg.hp2Site.storage`); output each `webHost`. |
| `infra/modules/frontdoor.bicep` | add the HP2 endpoint, origin group + origin (probe `/index.html`), `hp2-route` (`patternsToMatch ['/*']`, `ruleSets [ruleSet.id]` — the SAME `coiheaders`, `IgnoreQueryString`, `compressionSettings` from `fd.compress`, `httpsRedirect Enabled`, `forwardingProtocol HttpsOnly`) and `hp2-telemetry-route` (`fd.telemetryPaths` → `ogRelay`). Cleaner: factor a `frontdoor-site.bicep` module taking the profile, rule-set id and relay-og id, called once per site. Output `hp2EndpointHost`. |
| `infra/main.bicep:79,107` | pass `siteOrigins: ['https://<main>', 'https://<hp2>']` to `apps.bicep`; add `output hp2SiteUrl`. |
| `infra/modules/apps.bicep:27,85` | `peerOrigins = join(concat(siteOrigins, extraAllowedOrigins), ',')`; relay `ALLOWED_ORIGINS = join(siteOrigins, ',')` (fallback CORS only). |
| `infra/modules/logs.bicep:171-185` | make the silence alert per host, or add the hp2 host to the page regex deliberately. ⚠ If HP2 is published WITHOUT `telemetryEndpoint`, its root page GETs still count as pages while producing no opens → the alert fires. **Publish HP2 with telemetry** (§7) or scope the regex to `unrealgames…` host. |
| `bin/infra-deploy.sh:76-77` | enable static website on BOTH accounts. |
| `bin/apply-outputs.sh:30-70` | write `hp2Site.siteUrl`/`endpointHost`/`storage.webHost` back, and write `siteUrl` + `telemetryEndpoint = <hp2 siteUrl>/e` into `hp2Site.ufrontend/.ufront/publish.local.json` (the hp2 clone), not the upstream one. |
| `bin/deploy-ufront.py:47-58,237-243` | add `--site hp2` (default = today's site) selecting account/container/endpoint and replacing the `requiredFolders` guard with `requiredFiles` for a root site (refuse a folder that has `u1/`… or lacks `index.html`/`SDLLaunch.wasm`/`zstd/`). Skip `.ufront-publish*.json`, `.ufront-publish-incomplete` (refuse to upload an incomplete tree!) and `publish-provenance.json` for the public site, or confirm with the owner (§6.2 ★). Purge on the HP2 endpoint. |
| `bin/set-cache-headers.py:27-29`, `bin/verify-site.mjs:52-68` | the same `--site` selector. |
| `bin/telemetry.mjs` | a target/host filter so HP2 sessions can be listed separately (the envelope carries the target's telemetry id — TO-VERIFY the column name in `queries/*.kql`). |
| `peerjs/server.js:110` | add `'hp2'` to `ID_PREFIXES` **only if** HP2 exposes P2P hosting (it is single-player; without it, a player who presses Host is refused by the broker — `ID_ENFORCE=reject`). Discovery (`/peerjs/peers`) from the HP2 origin needs the origin in `ALLOWED_ORIGINS` (above) or it fails CORS silently. |
| game proxy (`gameProxy.*`) | **no change** — HP2 declares no `network.proxy`; do not publish HP2 with `--game-proxy-list`. |
| `docs/ARCHITECTURE.md`, `docs/RUNBOOK.md` § "Publish a new build", `docs/LOG.md` | the second site, its publish/upload/verify commands, the dated log line. |

### 10.4 Custom domain ("its own domain")

The endpoint host `hp2games-<hash>.z0N.azurefd.net` is Azure-assigned and changes on recreation
(`config.json frontDoor._comment`). A real domain (e.g. `hp2.<owner-domain>`) = `Microsoft.Cdn/profiles/customDomains`
(AFD-managed TLS certificate) + a DNS `CNAME` to the endpoint + a `_dnsauth` TXT for validation, and
`customDomains: [ { id } ]` on both HP2 routes. Then `siteUrl` = the custom domain everywhere (broker/relay
`ALLOWED_ORIGINS`, publish.local.json). This is also the "recorded way out" of hostname coupling
(`INFRA/CLAUDE.md` rules; `RUNBOOK.md` "A custom domain would make the URL itself survive a recreation"). DNS
provider and domain are OWNER decisions — TO-VERIFY.

### 10.5 End-to-end runbook once A (+ §6) exists

```bash
# infra (once)
cd /home/ion/unreal-frontend-azure-infra
bin/infra-deploy.sh --what-if     # expect: + storage hp2webassets, + endpoint hp2games, + og/origin, + 2 routes; apps: ALLOWED_ORIGINS modified
bin/infra-deploy.sh               # apply-outputs writes hp2Site.siteUrl + the hp2 clone's publish.local.json
bin/deploy-peerjs.sh              # only if peerjs/server.js changed
# publish (every build)
cd /home/ion/hp2-ufrontend/ufrontend
yarn publish-all --target targets/hp2/target.json --dry-run && yarn publish-all --target targets/hp2/target.json
yarn serve --root /home/ion/publish-hp2 --tls                         # boot it locally at "/" first
cd /home/ion/unreal-frontend-azure-infra
bin/deploy-ufront.py --site hp2 /home/ion/publish-hp2 --dry-run && bin/deploy-ufront.py --site hp2 /home/ion/publish-hp2
node bin/verify-site.mjs --site hp2                                    # asset differs 0
node /home/ion/hp2-ufrontend/ufrontend/packages/e2e/tools/live-session.cjs --url <hp2 siteUrl> --path '/' --cap 120
#   expect: 'Log: Bringing Level startup.MyLevel up for play', no crash panel; beacons with the new build sha
```

Costs (qualitative; not priced here): a second LRS storage account (GB stored ≈ HP2's zstd payload, a few hundred MB
— TO-VERIFY after the first publish) + Front Door request/egress charges; no new profile fee; no new App Service.
⚠ The workspace's 0.05 GB/day cap is SHARED: HP2 page opens and FD access-log rows (one per request, ~1 KB; a cold
HP2 boot fetches ~230-460 files) add to the same budget — watch `node bin/telemetry.mjs cost` after launch.

## 11. Open questions / TO-VERIFY

| # | Question / risk | Why it matters | How to verify / decide | Owner |
|---|---|---|---|---|
| Q1 | **Root-publish track**: Track A (`folder "hp2"`, upload `<out>/hp2/`) or Track B (`folder "."`, §6.2 code)? | §1.3 was written for B; A needs `publish.folder` changed to `"hp2"` | owner decision; record in `targets/hp2/README.md` | owner |
| Q2 | `travel`: all-empty block + generic test/PlayPanel change (§4.3 #3) vs the truthful minimal block (§1.5) | the skeleton test fails at `test/skeleton.test.mjs:102` with an empty block | owner decision | owner |
| Q3 | `appUserDir()` must return `""` on Unix/web and `appInit(TEXT("Game"),…)` must replace `"UnrealTournament"` (`HP2/Source/Game/Src/Launch.cpp:883`) | every `iniPath`, `userFiles` glob and `logPath` (`/System/this.program.log`) in this report assumes `/System/Game.ini`, `/System/User.ini`, `/Save/…` | after the first web boot: `FS.readdir('/System')`, `FS.readdir('/Save')` in devtools; the user-data store's keys in `hp2-user-data` | engine analyst |
| Q4 | HP2 is single-player but the P2P panel always renders (`App.vue:117` renders `NetworkSection` unconditionally; every target must carry `network.peerPrefix/defaultServerName`, `glue/src/target/config.js:42-94`) | a "Host"/"Join" UI on a single-player game; hosting would be refused by the broker (`ID_PREFIXES` lacks `hp2`) | owner: accept, or add a generic DATA switch (e.g. `network.p2p: false` → hide `NetworkSection`) — frontend change + README (doc-sync) + test | owner |
| Q5 | `appApplyIniOverrides` (the `-ini:<File>:[Section]:Key=` seam) is absent from HP2 Core (§1.4 row `network.playerName`) | without it the frontend's frame-rate cap, player name, server name are silently ignored | grep HP2 `Core/Src/UnMisc.cpp` for `appApplyIniOverrides` after the port; E2E `registerIniSettingsTests` | engine analyst |
| Q6 | `notices` / `telemetry` patterns and the three `CAPTURED` fixtures | docs/06 §1: verified against a real log of THIS build | capture from a real hp2 web console (§4.2) | first boot |
| Q7 | Save slots: 6? and does `..\\Save\\Slot<N>` map to `/Save/Slot<N>` (not a literal backslash dir) | `userFiles` globs list Slot1..6 | 2.75 precedent: UE1 web logged `LoadMap ..\Save\Save0.usa` fine (`STATE.md:369-405`); verify by saving in each slot and listing `/Save` | first boot |
| Q8 | Renderer init milestone for XOpenGL on HP2 (`XOpenGL::SetRes…`) | `RENDER_READY` in the hp2 profile (§5.3) | read the first hp2 log | first boot |
| Q9 | Menu music `.ogg` and the real precache list (Step B, §3.3) | warm-start guarantee | `precache-baseline.cjs` hp2 branch (§8.1) | after boot |
| Q10 | The renderer forks live INSIDE the ufrontend clone (`UF/XOpenGLDrv`, `UF/utglr-copy`, `UF/gl4es`), not at `/home/ion/hp2-ufrontend/*` as `HP2/CLAUDE.md:29` and `HP2/Source/CMakeLists.txt:249` say | breaks `yarn lint`; risks committing nested repos; makes every publish "dirty" | `mv` them to `/home/ion/hp2-ufrontend/`; re-run `git -C UF status` | owner |
| Q11 | `branding.displayName` wording / trademark; theme palette | public page title | owner | owner |
| Q12 | Uploading `.ufront-publish*.json` + `publish-provenance.json` (monorepo dirty-file list, local asset paths) to a public site | information exposure (the same happens on the four-game site today) | owner decision; uploader skip-list (§10.3) | owner |
| Q13 | Static `<title>Unreal Tournament 2004</title>` in `packages/frontend/index.html:6` before branding runs; no favicon | link previews / crawlers on the HP2 domain show UT2004 | optional generic Vite `transformIndexHtml` (§2.5) | frontend |
| Q14 | Custom domain + DNS provider for "its own domain" | the azurefd.net host changes on endpoint recreation | owner (§10.4) | owner |
| Q15 | `alert-telemetry-silence` counts `/` GETs on every host of the profile | an HP2 publish without telemetry would trip it | publish HP2 with `telemetryEndpoint`, or scope the regex per host (§10.3) | infra |
| Q16 | `webrtc-shim.js` vendored in HP2 differs textually from `UF/packages/glue/engine-shim/webrtc-shim.js` (HP2's is byte-identical to `engine-ut99/vendor/glue/webrtc-shim.js`; the monorepo copy is re-formatted) | shim drift is the engine's abi-pin concern, not a target-descriptor one | `diff -w` the two and confirm only formatting differs (engine analyst) | engine analyst |

Addendum (2026-09-26, resumed run): **Q16 closed**: the two `webrtc-shim.js` copies are token-identical after stripping
whitespace, `//` comments and trailing commas (checked with a `tr -d ' \t\n'` normalisation); the drift is prettier
formatting only. **Q10 re-confirmed open**: the forks are still nested in `UF` (`git -C UF status --short` →
`?? XOpenGLDrv/ ?? gl4es/ ?? utglr-copy/`).

## 12. Checklist — adding `hp2` to the ufrontend clone (in order)

- [ ] Engine: `appUserDir()` = `""` on web, `appInit(TEXT("Game"),…)`, `appApplyIniOverrides` ported, `wasm-perf` preset builds `HP2/build-wasm-perf/SDLLaunch/{SDLLaunch.js,.wasm,.data,meta.json}` (§1.4, §5.0)
- [ ] Move the renderer forks out of `UF` (§7, Q10)
- [ ] `.ufront/publish.local.json` (§7)
- [ ] `targets/hp2/target.json` (§1.3; pick Track A `folder "hp2"` or Track B `"."`, Q1), `README.md` (§1.6), `branding/{theme.css,logo.svg,help.md,portal-notes.json}` (§2)
- [ ] `targets/hp2/{file-manifest,dependency-manifest,precache}.json` via the §3 commands
- [ ] `test/skeleton.test.mjs` (§4.3) + `PlayPanel.vue` empty-fieldset guard + `packages/frontend/README.md` line (or §1.5 fallback travel data)
- [ ] Frontend/glue test loops + HP2 glue fixture (§4.4, §8.1)
- [ ] `.ufront/gates/no-engine-specifics.sh` regexes learn `hp2` (§9.1)
- [ ] `yarn build --target hp2`, `yarn serve`, hands-on boot (§5.1-5.2)
- [ ] `packages/e2e/lib/engines/hp2.profile.js` + `hp2-wasm-boot.spec.js` + README (§5.3); `precache-baseline.cjs` hp2 branch (§3.3 B)
- [ ] Capture real telemetry lines → `CAPTURED` fixtures (§4.2); verify `notices` (Q6)
- [ ] Track B code (§6.2) if chosen; `publish-boot-check.cjs` root entry (§8.1)
- [ ] `docs/06`, `docs/07`, `packages/build/README.md`, `packages/e2e/README.md` prose (§8.4); STATE/ROADMAP/step record (§9.3); `.ufront/e2e-stamp` before push
- [ ] Infra Option A (§10.3), custom domain (§10.4), publish + upload + verify (§10.5)

## Status: complete
