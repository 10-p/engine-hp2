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
