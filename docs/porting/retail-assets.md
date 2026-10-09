# The retail branch: the HP2 port on the RETAIL assets

**Why.** `Assets/` (and the source tree) are a **pre-release snapshot** dated 2002-09-06. The retail disc
(`$W/HarryPotter2.iso`, sha256 `b5267c81…a590f898`, volume `HPCOS`, build string `HPCos_021009_1205-1`, InstallShield cabs
2002-10-15) fixes shipped-data bugs the snapshot has (the `Entryhall_hub` password cut scene: markers without
`CutName`, two dialog lines with neither text nor audio) — see LOG 2026-10-02. The owner chose two branches:

| Branch (engine / frontend) | Assets | Purpose |
|---|---|---|
| `prototype` (= `main`) / ufrontend target `hp2-proto` | `Assets/` (snapshot); frozen: `$W/asset-folders/hp2-proto` | the original prototype, bugs and all — "proven" |
| `retail` (worktree `$W/HarryPotter2-retail`) / ufrontend target `hp2-retail-exp` | `$W/assets-retail`; frozen: `$W/asset-folders/hp2-retail-exp` | the retail packages + the retail engine behaviour — experimental, "unproven" |

(`$W` = the workspace, `~/WorkHP2` on the current machine. Both ufrontend targets live on its branch `hp2-retail`; until
2026-10-03 the two variants were one target id `hp2` on the branches `hp2-prototype` / `hp2-retail`.)

## The retail asset tree

`tools/dev/retail/assemble-assets.sh <iso> <work-dir> <out-dir> <unshield>` (unshield 1.6.2, built from source in the
workspace's `iso-check/unshield-src`). Component map in the script header. US English: `Language=usa` (the dialog
audio is the US recording `AllDialog.USA_uax`; `.usa` text for HpDialog/HpMenu/BumpDialog/hpcredits, `.int`
fallback). Structural retail differences: every sound effect lives in `System/HPSounds.u` (44 MB; no `Sounds/*.uax`
but `AllDialog.uax`); `System/Thumbnails/*.bmp` (level thumbnails copied into the save slot);
`Textures/MenuArt.utx` is imported by `PrivetDr.unr`. Never in git (lives outside every repo).

## What differs between the prototype and retail builds (and what the `retail` branch does about it)

Script interfaces (`tools/dev/retail/layout-diff.py`, `ucc batchexport` of both trees): **Core, Fire, Editor are
layout-identical**; Engine changed (Engine/Core/Editor/Fire kept their script text; HGame/UWindow/HP* shipped without
it — read with `tools/dev/retail/ucdis.py`, a bytecode disassembler):

- `Actor.bAlignBottomAlways` (bitfield after `bAlignBottom`), `PlayerPawn.bShowLoadingScreen` (+4),
  `InterpolationManager.Last` (+4), `PatrolPoint` made native (+`ActionKeyword`), `cFacialAnimChannel`,
  `BlockPlayer`, `SpecialEvent` script-only members; new class `MessageTrigger`.
- natives: only `native(330) GetCurrentKeyState(EInputKey)`. Events: `Actor.PreSaveGame/PostSaveGame`.
- `EngineClasses.h` + `EngineLayoutAsserts.cpp` regenerated from the retail `Engine.u` (1,084 asserts).

C++ behaviour (Ghidra 12.1.3 headless on both builds' DLLs: `tools/dev/retail/ghidra/` dumps a normalised
fingerprint per function and pairs the builds; ~25 real changes in Engine, few in Render, 9 in ALAudio, ~0 in Core).
Each was mapped onto our source by a read-only analysis (reports in `docs/porting/retail/`) and applied:

| Area | Change | Commit |
|---|---|---|
| collision/movement | `IsOverlapping(Other, Expand)` via the other's primitive; MoveActor touch filter; untouch slack 0.1/0.5; physWalking stops on a fall; interpolation uses `Last`; SLOT_Talk never occluded; movers re-add related movers | `b781456` |
| save/persistence | Pre/PostSaveGame around SaveGame; `SetThumbnailForSaveSlot`; PA records carry the class and respawn missing actors; `GAMESTATE=`; OnResolveGameState for every actor; Core cache wipe on exit | `ed906e7` |
| engine/render | loading screen (`LEVACT_Loading`), `bAlignBottomAlways` in mesh placement, bone relink on AnimBone, Delete-pause only in debug, font glyph row, `SHOWCOLL`/`SHOWPATHS` | `272bada` |
| audio | no occlusion, `SETVOLUMES`/`MUTESOUNDS`/`UNMUTESOUNDS`, priority/stealing rework, ambient throttle + 3-voice cap, SetViewport stops music | `4fbb31e` |
| boot | `LocalMap=PrivetDr.unr`: retail closes the title book on the first frame (its front-end launched PrivetDr or `-LOAD=0`) | `fee0f66` |

Ours, on top of retail (all marked in the code): bounds checks on the PA loader's raw string reads, PA class lookup
limited to classes, null checks in `FlushAllRelatedMovers`, the dead `IPSnitch` log not ported.

## Building and running the retail branch

The `retail` branch is checked out in its own worktree, `$W/HarryPotter2-retail`, so its build trees
(`build-native`, `build-wasm-perf`, `build-wasm`, `build-windows`) never touch the prototype's in `$W/HarryPotter2`.
Run tree: `HP2_ASSETS_DIR=$W/asset-folders/hp2-retail-exp tools/dev/mkrun-hp2.sh build-native <run-dir>`.
Frontend (ufrontend branch `hp2-retail`, target `hp2-retail-exp`; its `publish.engine.repo` is `HarryPotter2-retail`):
`yarn build --target hp2-retail-exp --engine-artifact $W/HarryPotter2-retail/build-wasm-perf/SDLLaunch` (assets from
`.ufront/publish.local.json` `assets.hp2-retail-exp`); one `yarn publish-all --target targets/hp2-proto/target.json
--target targets/hp2-retail-exp/target.json --out $W/publish-hp2-site` publishes both targets side by side. Each target
has its own storage prefix (`hp2-retail-exp-user-data` vs `hp2-proto-user-data`), so retail and prototype saves — not
interchangeable — never mix, even on one origin.
