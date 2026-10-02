# HP2 port — progress log

One dated line per green milestone (PLAN §10). Newest last.

- 2026-09-26 — **Phase A / Core compiles** (native gcc -m32 and wasm emcc, 0 errors): the template's
  `UnGnuG.h`/`UnUnix.h` + the 433 time API (`FTime appSeconds()`, `FLOAT GSecondsPerCycle`); 3-way merges of
  every Core header/source (template-core §9); `UnTemplate.h` = the resolved `UnTemplate.fixed.h` +
  `TTransArray::Insert`; `UnUnix.cpp` = template + the 433 additions (`appUserDir()=""`,
  `appProcessSeconds`, 3-arg `appCreateProc`/`appGetProcReturnCode`, `FFileStream` stubs, `IsOSVer2kOrXP`,
  counter-rebased `FTime appSeconds()`); layout guard in `UStruct(ENativeConstructor)` + `UStruct::Link`;
  native-bool BitMask restore; iterative GC tagger keeping HP2's `RefObj`; the web NULL-return loader sites;
  `appInit` fixes (gk block `Default.ini` + write-on-success, `appApplyIniOverrides` after it, `..`/`Save`
  separators, `*SaveSlotPath` varargs trap, stream-wait `#if _MSC_VER`). `__PSX2_EE__`, `__STATIC_LINK`,
  `PLATFORM_DREAMCAST`/`PLATFORM_LOW_MEMORY` groups removed from every built Core file
  (`tools/dev/unifdef-false.py`). No duplicate `GStaticExport` in `libCore.a`.
- 2026-09-26 — **Phase A / Render, Fire, SoftDrv compile** (native + wasm, 0 errors; agents): Render `69d41a3`
  (one `Merge` with `bDarkLight`, `MAX_RASTER_Y`, C-only `Pipe`, 29 for-scope hoists; the PLAN's `DrawWorld`
  `GetClass()` guard was NOT added — the template dropped it as a Dreamcast leftover that skipped overlays),
  Fire `bccbe83` (`GCC_ALIGN(4)` measured: `SparkType` 214→216, `sizeof(UFireTexture)` 1288→1292 on both
  layouts), SoftDrv `b6d2e31` (template's nine files + HP2's 433 face; `nm` shows the three 433 members, no
  `DrawGouraudPolygon`). `UnParticleRn.cpp` compiled unchanged.
- 2026-09-26 — **Phase B drivers compile** (native + wasm): NullDrv/SoundDrv `4810553`, NSDLDrv `5b96cbc`,
  IpDrv/WebRTCDrv `722df06`, launcher + UCC `1d536b1` (object files only — the link waits for Engine/Editor),
  Core `FixPath` `bd246cf`, run tree `2b167ff`, web inis `5a57c30`, artifact meta `2b33dc8`.
  `tools/dev/varargs-census.sh` (§3.7 gate): 0 non-POD varargs hits in Core, Render, Fire, SoftDrv and every
  driver. NOpenALDrv forced OFF on every target until Phase E.
- 2026-09-26 — **Phase A complete; native Game + ucc-bin LINK** (tree `build-native-b`, configured
  `cmake --preset native -B ../build-native-b -DBUILD_XOPENGLDRV=OFF -DBUILD_NOPENGLDRV=OFF` so it does not
  collide with the fork builds in `build-native`). Engine by agent (`8c61cab`…`094ae01`, 1,063 layout asserts
  pass native + `-fpack-struct=4`), Editor by agent (`20cb362`…`d1fc0cb`), Engine/Editor duplicate symbols
  `a23b0f1`. Run tree `/home/ion/hp2-ufrontend/run-native` (`tools/dev/mkrun-hp2.sh build-native-b <dir>`).
  UCC smoke: `ucc-bin help` OK, `Core.SimpleCommandlet` golden lines OK, `batchexport HGame.u class uc` exports
  812 classes, binds Core/Engine/Editor/Render/Fire/IpDrv; the [layout] guard found `UIceTexture` 280≠284 →
  pad `9cbb6ff`; now **zero `[layout]`**.
- **RESUME HERE (usage limit hit):** (1) first client boot: `cd run-native/System && xvfb-run -s "-screen 0
  1280x720x24" ./Game -SOFTWARERENDERER -NOMSGBOX -log` → Acceptance B lines (PLAN §4). (2) Phase C fork
  agents were STOPPED mid-work: XOpenGLDrv (`hp2-v433-compat`, was committing wrapper docs) and UTGLR (`hp2`,
  was at H4 alpha-blend) — check `git -C ../XOpenGLDrv log/status`, `git -C ../utglr-copy log/status`,
  finish uncommitted edits. (3) ufrontend `targets/hp2` agent STOPPED (was starting branding) — check
  `git -C ../ufrontend status/log`, resume from contract §12.
- 2026-09-26 — **Acceptance B met (native, SoftDrv + NullDrv)**: `Bringing Level startup.MyLevel up for play`,
  `Creating root window`, `Possessed PlayerPawn`, zero `[layout]`, no `Critical:`; Esc opens `FEInGamePage`;
  `Ch1Rictusempra` and `PrivetDr.unr -SAVESLOT=2` play (cut scenes + subtitles; `Save Slot Path: ../Save/Slot2`,
  `Save/Slot2/cache/` created). Fixture: `docs/porting/logs/native-first-boot.log`. Reference screenshots (game
  art — kept out of git) in `/home/ion/hp2-ufrontend/reference-shots/`. Fixed on the way: `UIceTexture` pad
  `9cbb6ff`, `MeshRn::ProcessVert` exact-division segfault `2afdc93` (pointer difference across buffers).
  RSS 168–204 MB on the heaviest maps (O10: no `-LAZY`). Phase C forks compile (XOpenGL `5b376cc`, UTGLR
  `48b2681`; HP2 docs `90db56a`, `2e7c8be`) — first light pending. PLAN's `DrawWorld GetClass()` guard is
  stale (the template dropped it). Next: W1 wasm link.
- 2026-09-26 — **Acceptance C met natively**: `Ch1Rictusempra` renders on XOpenGLDrv and UTGLR (Mesa llvmpipe
  4.5 under Xvfb), no GL errors. **W1**: `build-wasm/SDLLaunch/{js,wasm,data,meta.json}` (targetId hp2, repo
  HarryPotter2), depth-24 rewrite applied, no `_webrtc_deferred_open`. **W2**: headless smoke (inis only) →
  `Name subsystem initialized` … all 15 packages bound … `LoadMap: Entry` → `FailedBrowse`; no UWeb.
  ufrontend `targets/hp2` complete (460 files / 476 MB served, 714/714 refs, precache 231 files / 131.7 MB
  without AllDialog.uax), `yarn gates` green; web log name fixed (`GModule="Game"`, `36e321c`).
- 2026-09-26 — **THE WEB BUILD RUNS.** `build-wasm-perf` → `yarn build --target hp2` → `yarn e2e --target hp2`:
  **6/6 pass** (XOpenGL WebGL 2 first light to `startup.MyLevel` + `Possessed PlayerPawn`; SoftDrv discriminator;
  `?map=PrivetDr` direct load; launcher UI + telemetry silence). Visually confirmed in headless Chromium (GPU via
  ANGLE/Vulkan): the title level on XOpenGL (`Created WebGL 2.0 context on the worker`) and on UTGLR/gl4es
  (`Created WebGL 1.0 context`, `GL_VERSION 2.1 gl4es wrapper 1.1.7`), `Ch1Rictusempra` gameplay on XOpenGL,
  NullDrv boots. Silent (SoundDrv) until Phase E. Remaining for Acceptance D: key-binding persistence across a
  reload, the W3 loader sweep, `docs` (BUILD.md web section).
- 2026-09-26 — **Phase F**: `cmake --preset windows` builds `Game.exe` (50 MB, static) first time; imports only
  stock system DLLs. Wine 10 (prefix `/home/ion/hp2-ufrontend/.wine-hp2`): `Detected: Windows`, `startup.MyLevel`
  on SoftDrv, `Ch1Rictusempra` on XOpenGL (Mesa 4.3). Wine exposed a real bug on every target: HP2's 160×120
  save thumbnail read 960 rows from a 720-row 16:9 frame → fixed `0bda916` (web/native artifacts need a rebuild
  to carry it). Also: web persistence + save/load E2E green (ufrontend `42012c7`), W3 L10 `fd6910c`, BUILD.md.
- 2026-09-26 — **Phase E (audio) complete** (agent): EA-XA decoder reproduces the shipped lip-sync oracle
  (r 0.9959–0.9997 on the §11.1 sounds; min 0.9934 over 50 lines; all 2,994 XA sounds decode to their declared
  length); NOpenALDrv = HP2's ALAudio on openal-soft 1.25.2 (EFX, no EAX), Ogg by path, lip sync; native capture
  confirms music (`Music_StoryBook_v2.ogg`, r 0.997), XA owl hoots, Dobby/narrator lines; a 240 s PrivetDr →
  FlyingFordCutScene → Adv1Willow run clean; O13 stereo sign correct. Web: hp2 E2E **10/10** incl. the new audio
  test; Windows `Game.exe` + `OpenAL32.dll` under wine plays XA + lip-synced dialog. Corrections to the plan:
  the title song is `Music_StoryBook_v2.ogg` (not `Intro_Movie_Music_Edit`); `Peeves_chase_v2 _mx.ogg` has a
  space in its name (FetchFS encodes it); `PrivetDr` asks for `Music_temp_E3_cut_scene`, absent from retail.
  Owner decisions O1–O5 applied (`-DEBUGMODE` `febcffb`; multiplayer hidden, ufrontend `6662d08`; gl4es original).
- 2026-09-26 — **Publish folder (O2, no upload)**: `yarn publish-all --out /home/ion/hp2-ufrontend/publish-hp2
  --target targets/hp2/target.json` rebuilt both web engines at HEAD and wrote a verified 369 MB tree (zstd -19
  assets, 460/460 manifest entries, 714/714 deps, one-card portal one level up). `publish-hp2/hp2/` served as a
  site root boots `Ch1Rictusempra` on WebGL 2 with audio bound and no debug overlay. Upload/Azure await the owner.
- 2026-09-26 — Save-game investigation (owner O1 enhancement, report only): `docs/porting/save-games-investigation.md`
  (`33ad149`) — all three UE1 games write player files only under `Save/` + `System/<Game>.ini`/`User.ini`; a
  recursive `Save/**` rule (skip `cache/`, `*.tmp`) is the shared pattern, needing a glue glob extension (not
  implemented). It surfaced a port bug, fixed: `FindFiles` could not match `*_pa.*`, so HP2's per-level progress
  (Jellybeans, secrets) never reached the save slot (`79b21ff`). After it: hp2 E2E 10/10, Windows rebuilt, and the
  publish folder re-published at `150e6fb` and boot-checked (title level, WebGL 2, openal-soft, music, debug off).
- 2026-09-26 — **Rebased on ufrontend 2.82 + continue on Start.** The `hp2` branch rebased onto `origin/main`
  `bca749d` (one STATE.md conflict); hp2 `defaults.engineOptions` = `CPUSPEED=3000 -LOAD=0` (`abbda65`), help/README
  refreshed (`ecfe6c2`). A plain Start boots the title level; `-LOAD=0` continues `Save/Save0.usa` when it exists and
  is a harmless `Failed to load` when it does not (native run in a fresh tree). hp2 E2E 11/11 (+ first-light
  options check, + `continueOnStart`), gates green. Found: Clear User Data leaves the restored save in the engine
  filesystem until a page reload (generic; reported). Publish folder rebuilt from these HEADs and re-served on :8092.
- 2026-09-26 — **AutoFOV + a persistent-actor crash.** AutoFOV (`c8566eb`): the template's scoped Hor+ override in
  `UGameEngine::Draw`, view part only (no viewmodel pass — HP2 draws through a camera `ViewTarget`; no AutoFOVUI —
  no `HudScale`); native, wine and web builds log `base=90 effective=106.26` at 1280x720. Crash (`f536996`, reported
  from the web: re-entering `Entryhall_hub` after `transition`): `FFileManagerLinux.h` lacked retail's
  `operator<<(FName&)` on the file reader/writer, so `<Map>_pa.usa` stored no property names and the load read an
  uninitialised `FPropertyTag::Name`; ported with validation + a per-actor resync. Same report, triaged as SHIPPED
  DATA, not the port: `Entryhall_hub`'s `CutMark12/13/14/16` have no `CutName` (the Password cut scene's four
  `teleport` commands fail, so Hermione/Fred/George/Ron stay put), and `PC_Hry_Password_10` / `Pc_Hry_Password_14`
  exist in neither `hpdialog.int` nor `AllDialog.uax` (a `<?int?HPdialog.all.…?>` subtitle and silence). Still open:
  three ghosts "fell out of the world" in `Entryhall_hub` at `GSTATE010` (not reproduced at `gstate060`).
- 2026-10-02 — **Our `Assets/` are NOT retail — a pre-release snapshot.** Compared with the retail install disc
  (`/home/ion/hp2-ufrontend/HarryPotter2.iso`, volume `HPCOS`, InstallShield cabs dated 2002-10-15; extracted with a
  locally built unshield into `/home/ion/hp2-ufrontend/iso-check/`, never in git): of the files both trees have, 114
  are identical and 358 differ (41/42 maps, 182 System, 106 Music, 28 Textures); `HGame.u` 19,888,599 (ours,
  2002-09-06) vs 17,559,707 (retail `setup/hgame.u`); retail packs every sound effect into a 44 MB `System/HPSounds.u`
  (no `Sounds/*.uax` but `AllDialog`), and ships no dev leftovers (`Maps/Oldfiles/E3Demo.unr`, `BioRifle.uax`, ...).
  The cut-scene bugs are FIXED in retail data: `CUTSCENES/1040Password.int` is rewritten (3,209 -> 6,019 bytes; no
  `PC_Hry_Password_10`/`_14`, Hermione/Ron teleport to `HermioneLoc`/`RonLOC`), and retail `Entryhall_hub.unr` names
  the markers (`CutMark12/13/14/16` CutName `HermioneLOC/FredLoc/GeorgeLoc/RonLOC`). The source tree matches OUR
  snapshot: against retail `Engine.u` the layout guard flags `PlayerPawn` (+`bShowLoadingScreen`), `Camera` (its
  subclass) and `InterpolationManager` (2396/796 vs 2400/800), and retail maps use HGame classes ours lack
  (`ResolveHPointTies`). Moving to retail assets = engine header updates + frontend target data regeneration
  (manifests, precache, `assetMarkers` — `Core.u` differs too) + re-verification. Owner decision pending.

