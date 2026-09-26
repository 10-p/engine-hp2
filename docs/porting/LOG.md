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
