# Common brief: retail vs prototype HP2 binaries (read this first)

Harry Potter and the Chamber of Secrets (PC, UE1 build 433) is being ported. The C++ SOURCE we have
(`/home/ion/hp2-ufrontend/HarryPotter2/Source/`, git branch `retail`) is the PROTOTYPE (2002-09-06). The retail game
(2002-10-09 build "HPCos_021009_1205-1") shipped changed binaries. We are porting the retail behaviour into our source.

- Prototype Win32 DLLs: `/home/ion/hp2-ufrontend/HarryPotter2/Assets/System/{Core,Engine,Render,ALAudio}.dll`
  (built from essentially our source; use them as the sanity check that you matched the right source function).
- Retail Win32 DLLs: `/home/ion/hp2-ufrontend/assets-retail/System/{Core,Engine,Render,ALAudio}.dll`.
- Ghidra decompiles (C) already generated: `/home/ion/hp2-ufrontend/iso-check/ghidra/dec/<set>/<hexaddr>.c`
  (sets: proto, retail, proto-named, retail-named, retail-named2, proto-render, retail-render, proto-alaudio, retail-alaudio).
  Ghidra is NOT to be run by you (its project is locked by others). For exact instructions use objdump:
  `i686-w64-mingw32-objdump -d -M intel --no-show-raw-insn --start-address=0xADDR --stop-address=0xEND <dll>`.
  Engine.dll is incrementally linked: exports are 1-instruction `jmp` stubs; bodies are unnamed FUN_xxx.
  Export names: `i686-w64-mingw32-objdump -p <dll>` (decorated MSVC names).
- Field offsets in the decompile are byte offsets on `this`. Map them to members with
  `/home/ion/hp2-ufrontend/HarryPotter2/Source/Engine/Src/EngineLayoutAsserts.cpp` (RETAIL layout: lines like
  `static_assert(UE_OFS(AActor,Location)==284 ...)`) and `Source/Engine/Inc/EngineClasses.h`. Retail added members:
  AActor.bAlignBottomAlways (bitfield, word at 512, after bAlignBottom), APlayerPawn.bShowLoadingScreen (at 2348; members
  after it +4), AInterpolationManager.Last (at 688; members after +4), new native class APatrolPoint (layout in
  EngineClasses.h). So many proto->retail differences are ONLY displacement changes caused by these shifts — those are
  NOT behaviour changes; say so and move on.
- Retail script changes are known (Engine.u text was kept): new events Actor.PreSaveGame()/PostSaveGame(), new native
  `native(330) final function bool GetCurrentKeyState(EInputKey Key)`, InterpolationManager.Init(D,Rate,bJumpToStart,
  optional L) sets Last (the point travelled FROM) and InterpolationPoint sets Other.Last=self; PatrolPoint got
  ActionKeyword; PlayerPawn.SaveGame() lost its int parameter.

YOUR JOB for each assigned pair (proto address -> retail address):
1. Identify the function in OUR source (file:line, function name). Confirm via strings/log text/calls/structure.
2. Classify each difference: (a) layout-only (offset shifts, inlined-header changes), or (b) REAL logic change.
3. For every (b): describe it precisely and give the change to make in OUR source as a unified diff snippet against the
   current file (read the file to get exact context), with a confidence level and any uncertainty spelled out.
4. Do NOT edit any repository file. Do NOT run builds. Read-only analysis; write your report to the path given in your
   task, then return a short summary (what changed for real, which patches, confidence).
