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
