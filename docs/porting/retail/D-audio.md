Record — kept as history; the current state is in README.md, BUILD.md and the change inventory.

# D-audio — retail `ALAudio.dll` vs prototype, and what NOpenALDrv should adopt

Scope: `UALAudioSubsystem` in the prototype `HarryPotter2/Assets/System/ALAudio.dll` (2002-09-06) vs the retail
`assets-retail/System/ALAudio.dll` (2002-10-09). Read-only analysis (Ghidra decompiles in
`iso-check/ghidra/dec/{proto,retail}-alaudio/`, every claim below re-checked against `objdump` of the two DLLs).
Our driver: `HarryPotter2/Source/NOpenALDrv/` (port of the PROTOTYPE ALAudio to openal-soft).

## TL;DR

The prototype C++ source **does** exist in the repo — `Source/ALAudio/Src/ALAudioSubsystem.cpp` (2225 lines) +
`Source/ALAudio/Inc/ALAudioSubsystem.h` — and the prototype DLL is a 1:1 build of it (same strings, same calls,
same control flow; e.g. `"WARNING: was check(0) from Vogel..."`, the `PLAYINGSOUNDSTATS` 32-iteration loop).
NOpenALDrv follows it closely, so every retail delta maps onto a known NOpenALDrv spot.

Real retail changes (all high confidence unless noted):

| # | Change | Where (retail) | Adopt in NOpenALDrv? |
|---|--------|----------------|----------------------|
| R1 | **Sound occlusion removed everywhere** (no `IsAudibleAt`/`PointRegion`/`CalculateRadiusMultiplier`/`OCCLUSION_FACTOR`; 5 `IsAudibleAt` calls in proto → 0; the 0.35 constant and the 3 `SoundOcclusion` guard strings are gone) | PlaySound (priority + initial radius), ModifySound(SOUND_Radius), Update (ambient candidates + per-voice radius) | **Yes** (fidelity, and it removes per-frame BSP line checks per voice — a web perf win) |
| R2 | **New console commands** `SETVOLUMES SOUND=<f> MUSIC=<f>`, `MUTESOUNDS`, `UNMUTESOUNDS`, `SOUNDSOURCESTATES`; `PLAYINGSOUNDSTATS` prints `Pri` | Exec (289 → 512 insns) | **Yes** — retail `HGame.u` sends `SETVOLUMES MUSIC=`/`SOUND=` from the options page and `MUTESOUNDS`/`UNMUTESOUNDS` around the cut-scene fast-forward skip |
| R3 | **New member `Muted`** (+0x5C): gain forced to 0 in PlaySound and Update while set | PlaySound, Update, Exec | **Yes** (part of R2) |
| R4 | **PlaySound registers the sound only after choosing and stopping a voice**; the "register failed → FindLeastImportantSound → retry" path is gone | PlaySound (704 → 571 insns) | **Yes** — also removes the root cause of the leaked unbound streams NOpenALDrv's `ReclaimIdleStream` mops up |
| R5 | **`FindLeastImportantSound(USound*, INT Id, FLOAT Priority)`** rewritten: only voices with a stored priority **below the incoming one** qualify; ambient voices grouped by sound; incoming USound unused. Because PlaySound calls it only when no voice has a lower priority, it practically always returns −1 → the prototype's "steal the 4th instance of a sound" is gone | 10007180 | **Yes** (literal port; effectively removes stealing) |
| R6 | **Ambient pass throttled to every 0.2 s** (new member `AmbientTimer` +0x54, ctor-initialised) — both starting new ambient voices and stopping out-of-range ones | Update | **Yes** |
| R7 | **At most 3 voices per `AmbientSound`**: candidates + already-playing ambient voices are grouped by sound (new 20-byte candidate struct with `SourceIndex`, second `Sort` with a mode flag, new retail-only `Sort` instantiation 100080d0); per group the 4th+ playing instance is stopped, new ones start only within the top 3; a refused `PlaySound` ends that group only (proto: ended the whole pass) | Update | **Yes** |
| R8 | Ambient candidate priority uses `SoundVolume/255 * global SoundVolume` (proto omitted the global) | Update | **Yes** (one factor) |
| R9 | **`SetViewport` stops music too** (proto spared `SF_Music`) and **no longer pre-registers every `USound` for `UsePrecache`** | SetViewport (122 → 53 insns) | **Yes, medium** — see risk note §5.1 |

Volume-scaling changes that NOpenALDrv **already** has in effect (no patch needed): ModifySound(SOUND_Volume) and the
ambient per-frame volume now multiply by the global `SoundVolume`; a new per-voice `BaseVolume` (+0x38) lets
`SETVOLUMES` rescale live voices; looping/music voices that stop are restarted (`Stream.Reset`). NOpenALDrv keeps the
unscaled volume per voice and applies `SoundVolume`/`MusicVolume` live (`SourceGain`), and `PumpStream` restarts any
stalled stream, so all of that is already equivalent.

Everything else is layout-only or log-text-only (§3).

The ready-to-apply patch (header + cpp, verified with `git apply --check` against `HarryPotter2` branch `retail` HEAD 67c5183; NOpenALDrv last changed in c7141b6) is in
§4 and in `iso-check/reports/D-audio.patch`. Not built (per brief).

## 1. Function map (proto → retail) and our sources

Export RVAs from `objdump -p` (ALAudio is not incrementally linked; names are real).

| Function | proto addr / size | retail addr / size | proto source (`Source/ALAudio/Src/ALAudioSubsystem.cpp`) | NOpenALDrv (`Source/NOpenALDrv/NOpenALDrv.cpp`) | Verdict |
|---|---|---|---|---|---|
| ctor `UALAudioSubsystem()` | 10002640 / 0x170 | 10002680 / 0x180 | :33 | :194 | +`AmbientTimer=0` (R6) |
| `Destroy` | 10002bb0 / 0x290 | 10002c00 / 0x290 | :85 | :275 | layout only |
| `Init` | 10002ef0 / 0x3f0 | 10002f40 / 0x3f0 | :154 | :348 | layout only |
| `SetViewport` | 100032e0 / 0x180 | 10003330 / 0xc0 | :274 | :492 | **R9** |
| `RegisterSound` | 10003470 / 0x9c0 | 10003400 / 0x9c0 | :315 | :803 | log text only |
| `UnregisterSound` | 10003e30 / 0x300 | 10003dc0 / 0x330 | :609 | :912 | entry log only |
| `PlaySound` | 10004130 / 0x960 | 100040f0 / 0x730 | :700 | :1007 | **R1, R3, R4, R5** |
| `ModifySound` | 10004c50 / 0x300 | 100049e0 / 0x190 | :1025 | :1290 | **R1**; volume×SoundVolume (equivalent already) |
| `PlayMusic` | 10005050 / 0x410 | 10004c70 / 0x450 | :1127 | :1413 | explicit `RegisterSound(Music)` dropped (consequence of R4; NOpenALDrv never had it) + debug log |
| `Update` | 10005670 / 0x1510 (+ Ghidra split tail 1000646a) | 100052d0 / 0x14e0 (+ tail 10006152) | :1238 | :1520 | **R1, R3, R6, R7, R8** |
| `Exec` | 10006b80 / 0x3a0 | 100067b0 / 0x640 | :1701 | :1871 | **R2** |
| `FindLeastImportantSound` | 100072b0 `(USound*)` | 10007180 `(USound*,INT,FLOAT)` / 0x350 | :2035 | :2039 | **R5** |
| `Sort<ALAmbient>` | 10008170 (16-byte elems, 1 caller) | 100080d0 (20-byte elems, 2 callers) | `Compare` :1233 | `Compare` :1515 | **R7** |
| `Compare(ALAmbient&,ALAmbient&)` | 100077a0 | 100076c0 (+ mode flag `DAT_1000e0c4`) | :1233 | :1515 | **R7** |
| StopSound×2, NoteDestroy, StopMusic, StopAllMusic, Pause/UnpauseSounds, StopSoundSource, OldStopSound, SoundPriority, GetNewStreamStruct, PostEditChange, ShutdownAfterError, StaticConstructor, FLipSyncAnimMgr | — | — | — | — | identical modulo layout (normalised asm diff) |

`1000646a` (proto) / `10006152` (retail) are not real functions: Ghidra split the tail of `Update` (the per-voice
3D block after the streaming branch) at a `return`. Proto's tail calls `IsAudibleAt` + `UModel::PointRegion` +
`CalculateRadiusMultiplier` (occlusion); retail's does not (R1). `100080d0` is the retail `Sort<>` instantiation for
the new 20-byte ambient candidate (R7).

Imports: retail adds `FString::operator*` (PlayMusic log) and `Parse(const TCHAR*,const TCHAR*,FLOAT&)` (SETVOLUMES);
nothing removed. Strings added: `SETVOLUMES`, `SOUND=`, `MUSIC=`, `SOUNDSOURCESTATES`, `Source %d, State = %d`,
`Source %d not active`, `MUTESOUNDS`, `UNMUTESOUNDS`, `No empty slots in Sources() - try killing a sound`,
`Unable to kill a sound - returning (Index == -1)`, `FindLeastImportantSound() returning %d, pMaxCountSound = %d`,
`RegisterSound(): %s`, `UnregisterSound(): %s`, `PlayMusic(%s) - name = %s`, `Pri =%d`/`Pri = %d`. Removed: 3×
`SoundOcclusion` (guard names), `WARNING: was check(0) from Vogel...`. Constant removed: `0.35f` (OCCLUSION_FACTOR).
Constant added: `0.2f` (ambient interval), `1000.0f` (Pri scale).

## 2. Retail layout (offsets on the `UObject` base)

`Exec`'s `this` is the `FExec` sub-object at +0x28; the table is normalised to the UObject base.

| Member | proto | retail | note |
|---|---|---|---|
| Viewport / DummyViewport / LastTime(FTime) / LastPosition / Initialized / LastRealtime / LastPaused | 0x2C…0x50 | same | |
| **AmbientTimer** (FLOAT, name ours) | — | **0x54** | ctor sets 0; `+= DeltaTime`, `> 0.2` → pass, reset (R6) |
| RestartSounds | 0x54 | 0x58 | |
| **Muted** (UBOOL, name ours) | — | **0x5C** | MUTESOUNDS=1 / UNMUTESOUNDS=0 (R3) |
| OldListener | 0x58 | 0x60 | |
| RegionListener (12 B) | 0x5C | 0x64 | |
| Sources / Buffers / Streams (TArray) | 0x68 / 0x74 / 0x80 | 0x70 / 0x7C / 0x88 | |
| SoundDevice … UseMMSYSTEM | 0x8C … 0xBC | 0x94 … 0xC4 | +8 shift |

`ALSource` grew 0x58 → 0x5C: a new FLOAT at **+0x38** (`BaseVolume`, name ours) = the caller's raw `Volume` (before
`× SoundVolume`), or `MusicVolume` for `SF_Music`; everything from `FadeDuration` on moves +4 (Flags 0x44→0x48,
Id 0x48→0x4C, Started 0x4C→0x50, Paused 0x50→0x54, LipSyncAnimMgr 0x54→0x58). Only `SETVOLUMES` reads it.
`ALAmbient` grew 16 → 20 bytes: `{Actor, Priority, Flags, Id, SourceIndex}` (R7). `ALStream`/`ALBuffer` unchanged.

All other offset differences in the decompiles are these shifts — not behaviour.

## 3. Layout-only / log-only functions (no action)

- **Init, Destroy**: byte-identical modulo the +8 member shift and the 0x5C element size (normalised diff empty).
- **RegisterSound**: same size and flow; only `debugf(NAME_DevSound)` texts (`RegisterSound(): %s` at entry, the
  failure messages now name `CreateStream()`/`GetNewStreamStruct()`, one Ogg failure log dropped).
- **UnregisterSound**: `UnregisterSound(): %s` log at entry; same logic (Ghidra just restructured the loops).
- **PlayMusic**: un-commented `PlayMusic(%s) - name = %s` log; the explicit `RegisterSound(Music)` before
  `PlaySound` is gone (asm: proto calls vtable +0x68 then +0x70, retail only +0x70) — required by R4, otherwise the
  song's stream would be registered twice; NOpenALDrv's `PlayMusic` already never calls it.
- **Update, finished-voice check**: retail no longer skips `SF_Looping|SF_Music` voices: a stopped looping/music voice
  gets `Streams(h).Reset = 1` (restart next frame); a stopped streaming one-shot that is not `Alive` is unregistered
  without the `check(0) from Vogel` log. NOpenALDrv's `PumpStream` (`NOpenALDrv.cpp:741-785`) already restarts every
  stalled stream (music included) and frees played-out ones — equivalent, nothing to do.
- **Volume scaling** (ModifySound volume `Clamp(Value*SoundVolume,0.01,1)`, ambient `Volume = SoundVolume_byte/255 *
  SoundVolume`, `BaseVolume`): NOpenALDrv stores the unscaled volume and applies `SoundVolume`/`MusicVolume` in
  `SourceGain` live — the audible result is the same. (One retail quirk not reproduced: `SETVOLUMES` reverts a voice
  that `ModifySound` changed to its PlaySound volume, because `BaseVolume` is not updated by ModifySound.)
- StopSound ×2, NoteDestroy, StopMusic, StopAllMusic, Pause/UnpauseSounds, StopSoundSource, OldStopSound,
  SoundPriority, GetNewStreamStruct, PostEditChange, ShutdownAfterError, StaticConstructor (still registers
  `UsePrecache` etc.), FLipSyncAnimMgr: identical except displacements / register allocation from the new element size.

## 4. Real changes in detail

### R1 — occlusion removed
Proto (source :759-775, :828-844, :1064-1077, :1373-1390, :1612-1632) shrinks the radius by
`CalculateRadiusMultiplier(zone→zone) × 0.35` when `!IsAudibleAt`. Retail:
- PlaySound: priority = `SoundPriority(Viewport, Location, Clamp(Volume*SoundVolume,0,1), Radius, Flags)`;
  `ZoneRadius = UsedRadius = WantedRadius = Radius` (asm 100041be-10004256, 100043ba-100043de).
- ModifySound(SOUND_Radius): `ZoneRadius = UsedRadius = WantedRadius = Value; AL_REFERENCE_DISTANCE = Value*DF`
  (asm 10004ac2-10004ade). No SEH guard any more.
- Update per voice (3D): `Radius = Sources(i).Radius; ZoneRadius = Radius;` then the unchanged 1-second
  `WantedRadius/LastChange/Lerp(UsedRadius,…)` smoothing and `AL_REFERENCE_DISTANCE = Radius*DF` (asm 100063af-1000642a).
- Ambient candidates: owned → as before; not owned → only `FDistSquared ≤ (SoundRadius*GAudioMaxRadiusMultiplier)²`
  (asm 10005a41-10005a9a), priority radius = `SoundRadius`.
`RegionListener` is still computed every frame (only for `SetI3DL2Listener`, a no-op; NOpenALDrv uses it for reverb).

### R2/R3 — Exec + Muted (asm 100067b0-10006d93)
- `SETVOLUMES`: `Parse(Cmd,"SOUND=",f)` → `SoundVolume = Clamp(f,0,1)`; `Parse(Cmd,"MUSIC=",f)` → `MusicVolume =
  Clamp(f,0,1)`; then for every voice with `Id`: `Volume = (Flags&SF_Music) ? MusicVolume : Clamp(BaseVolume*SoundVolume,0,1)`,
  `alSourcef(AL_GAIN, Volume)` (no Muted check — retail quirk).
- `SOUNDSOURCESTATES`: per voice `Ar.Logf("Source %d, State = %d", i, AL_SOURCE_STATE)` or `"Source %d not active"`.
- `MUTESOUNDS`: `Muted = 1`, `AL_GAIN = 0` on every voice with `Id`. `UNMUTESOUNDS`: `Muted = 0`, `AL_GAIN = Volume`.
- `Muted` is honoured by PlaySound (`AL_GAIN = Muted ? 0 : (FadeIn ? 0 : Volume)`, asm 100045c5-100045f5) and by
  Update (`if(!Muted) alSourcef(AL_GAIN,…)`, asm 10006388-100063a2). Not by ModifySound/SETVOLUMES (retail bug; the
  patch guards ModifySound and ApplyVolumes so a mute holds — deviation flagged).
- `PLAYINGSOUNDSTATS` adds `Pri = (INT)(Priority*1000)`.
- Callers (retail `assets-retail/System/HGame.u` strings): `SETVOLUMES MUSIC=` / `SETVOLUMES SOUND=` right next to
  `set ini:Engine.Engine.AudioDevice MusicVolume|SoundVolume` (the options page, our `FESoundVideoPage.uc:617/623`
  only has the `set ini:` half); `MUTESOUNDS`/`UNMUTESOUNDS` next to `*****STARING FASTFORWARD****` /
  `*****FINISHED FASTFORWARD****` / `SkipCS_0001` (the retail cut-scene skip). The prototype `HGame.u` has neither.
  Without R2, retail scripts get "unrecognised command" and a skipped cut-scene's sounds blare through the
  fast-forward. (`UEngine::Exec` forwards to `Audio->Exec`, `UnEngine.cpp:254`.)

### R4 — PlaySound order (asm 100040f0-1000431a, 10004330-10004345)
Retail: WorkingFlags → `if(!Viewport||!Sound) return -1` → scaled volume → priority (Talk 100 / ambient InPriority /
SoundPriority) → FreeSlot Id → voice scan (unchanged) → if none: log + `FindLeastImportantSound(Sound, Id, Priority)`,
`-1` → log + return −1 → `StopSoundSource(Index)` → **`RegisterSound(Sound)` (vtable +0x68); fail → return −1** →
BindStream … (unchanged). The proto's up-front `RegisterSound` + `FindLeastImportantSound(Sound)` + retry (source
:719-735) is gone. Effect: an early return (non-interruptable slot, no voice) no longer leaves a registered,
unbound stream (RefCount++) behind, and the voice's own stream is freed before a new one is needed.
NOpenALDrv note: when the same XA sound is replayed in the same slot, `StopSoundSource` now runs before
`RegisterSound`, so `UnloadSoundData` (M5-6) drops and the next `OpenStream` re-`Load()`s the lazy data — a re-read
of the same bytes, not a correctness issue.

### R5 — FindLeastImportantSound (asm 10007180-100074c3)
```
count instances per Sound over voices with Id && (Id&14)==SLOT_Ambient*2; pMaxCountSound = first sound reaching the
max (start 0, strictly greater)
for each voice with Id:
    candidate = pMaxCountSound ? voice.Sound == pMaxCountSound
              : (IncomingId&14)==SLOT_Ambient*2 ? (voice.Id&14)==SLOT_Ambient*2 : true
    if candidate && voice.Priority < IncomingPriority:
        p = SoundPriority(Viewport, voice.Location, voice.Volume, voice.Radius, voice.Sound->WorkingFlags)
        keep the lowest p (start 1000)
debugf(DevSound,"FindLeastImportantSound() returning %d, pMaxCountSound = %d", iRet, pMaxCountSound!=NULL)
```
The `pIncomingSound` argument is never read; the SLOT_Talk exclusion is gone (Talk's 100 protects it). Since
PlaySound only calls it when **no** voice has `Priority < incoming` (its own scan, `fcomp`/C0 at 100042a8), the
`voice.Priority < IncomingPriority` filter can only pass on an x87 80-bit vs 32-bit rounding tie — i.e. retail
effectively never steals here. Ported literally (keeps the logs).

### R6/R7/R8 — ambient pass (asm 100058fc-10005e81, compare 100076c0)
```
AmbientTimer += DeltaTime            // the clamped 0.0001..1 s frame delta
if AmbientTimer > 0.2:
    AmbientTimer = 0
    if Realtime && !Paused:
        list = new candidates (as before, no occlusion, priority volume ×SoundVolume, SourceIndex=-1)
             + every playing ambient voice {Actor, voice.Priority, voice.Sound->WorkingFlags, voice.Id, SourceIndex=i}
        SortMode=0; Sort(list)        // by Actor->AmbientSound (pointer order)
        for each run of equal Actor->AmbientSound [first,end):
            SortMode=1; if n>1 Sort(run)   // by Priority, highest first (same comparator as proto)
            if n>3: for k in first+3..end-1: if SourceIndex!=-1: StopSoundSource(SourceIndex)
            for k in first..min(first+2,end-1): if SourceIndex==-1:
                if PlaySound(Actor, Id, Actor->AmbientSound, …, Flags, 0, Priority) == -1: next run
    stop ambient voices out of ZoneRadius*GAudioMaxRadiusMultiplier or whose AmbientSound changed  // same test as proto, now inside the 0.2 s block
```
Patch deviations (both defensive, behaviour-neutral in normal play): runs are ordered by `USound::GetIndex()` instead
of pointer value (equally arbitrary, deterministic across native/web); a recorded `SourceIndex` is only stopped if that
voice still carries the entry's `Id` (retail could stop a voice an earlier run's `PlaySound` had just taken over);
playing voices whose actor has no `AmbientSound` any more are not listed (retail would compare a NULL; the stop loop
right after kills them anyway).

### R9 — SetViewport (asm 10003330-100033ef)
```
for each voice: if Started: StopSoundSource(i)      // proto: && !(Flags & SF_Music)
if Viewport != InViewport: Viewport = InViewport; RegionListener = 0   // UsePrecache RegisterSound loop removed
```

## 5. Risks / open points

1. **R9 and music at map start (medium).** Our (prototype) `UGameEngine::LoadMap` calls
   `Audio->SetViewport(Audio->GetViewport())` twice: at the loading screen (`UnGame.cpp:912`) and after
   `BeginPlay/PostBeginPlay` (`:1499`). With R9 a song started from PreBeginPlay/BeginPlay/PostBeginPlay would be cut at
   `:1499`. HP2 starts music from `NewMusicTrigger.ProcessTrigger` (trigger events, after the player spawns),
   `SpellLessonTrigger` and `PlayerPawn.ClientSetMusic`, so normal play is unaffected — and retail behaves this way.
   Also `UNSDLClient::TryRenderDevice` (`NSDLClient.cpp:455/470`) does SetViewport(NULL)/SetViewport(Viewport), so a
   runtime renderer switch now ends the song (as WinDrv's fullscreen toggle did in retail). Ask the Engine agent to
   confirm the retail `Engine.dll` LoadMap still makes those two calls; if it does, keep R9. Verify on a map whose
   music starts immediately after load.
2. R4/R5 change voice stealing: with 32 busy voices, a lower-or-equal-priority new sound is now dropped instead of
   stealing the most distant of ≥4 instances of one sound. Retail-faithful; audible only in very busy scenes.
3. R7 caps each AmbientSound at 3 voices (e.g. many torches/fountains sharing one sound): retail-faithful, intended.
4. The `Muted` guards in ModifySound(SOUND_Volume) and ApplyVolumes (so SETVOLUMES/UNMUTE ordering cannot un-mute
   a voice mid-skip) are a deliberate fix of a retail inconsistency; drop them if strict bug-for-bug is wanted.
5. `UsePrecache` stays an ini key but is now unused (retail too; shipped value False).
6. No build was run (brief). The patch only touches `Source/NOpenALDrv/NOpenALDrv.cpp` and `NOpenALDrvPrivate.h`;
   `OCCLUSION_FACTOR` and `INITIAL_MIN_COUNT` have no other users (grep). `Parse(const TCHAR*,const TCHAR*,FLOAT&)`
   exists in Core (`UnFile.h:361`).

## 6. Patch (against HarryPotter2 `retail` HEAD 67c5183; also `iso-check/reports/D-audio.patch`)

Apply from `HarryPotter2/` with `git apply -p1 ../iso-check/reports/D-audio.patch` (checked with `git apply --check`).

```diff
--- a/Source/NOpenALDrv/NOpenALDrvPrivate.h
+++ b/Source/NOpenALDrv/NOpenALDrvPrivate.h
@@ -38,8 +38,13 @@
 #define DEFAULT_STREAM_BUFFERS     4
 #define DEFAULT_STREAM_CHUNKSIZE   32768
 
-// Occlusion factor.
-#define OCCLUSION_FACTOR 0.35f
+// (Retail ALAudio dropped the prototype's sound occlusion -- IsAudibleAt / CalculateRadiusMultiplier /
+// OCCLUSION_FACTOR 0.35 -- from PlaySound, ModifySound and Update; so does this driver.)
+
+// Retail: ambient sounds are (re)considered every AMBIENT_UPDATE_INTERVAL seconds, not every frame, and at
+// most MAX_AMBIENT_INSTANCES voices play one AmbientSound at a time (the highest priorities).
+#define AMBIENT_UPDATE_INTERVAL 0.2f
+#define MAX_AMBIENT_INSTANCES   3
 
 // Priority modifier for playing sounds.
 #define PLAYING_PRIORITY_MULTIPLIER 1.0f
@@ -143,13 +148,15 @@
 	void*			Ogg;			// OggVorbis_File* (heap; see NOpenALDrv.cpp for why it is opaque here)
 };
 
-// An ambient sound candidate (ALAudio's ALAmbient).
+// An ambient sound candidate (ALAudio's ALAmbient; retail added SourceIndex, 16 -> 20 bytes: the ambient
+// sounds already playing are listed too, so each AmbientSound can be capped at MAX_AMBIENT_INSTANCES).
 struct FNALAmbient
 {
 	AActor*		Actor;
 	FLOAT		Priority;
 	DWORD		Flags;
 	INT			Id;
+	INT			SourceIndex;	// the voice it plays on, or -1 for a candidate not yet started
 };
 
 /*------------------------------------------------------------------------------------
@@ -184,7 +191,9 @@
 	UBOOL		Initialized;
 	UBOOL		LastRealtime;
 	UBOOL		LastPaused;
+	FLOAT		AmbientTimer;		// retail (+0x54): seconds since the last ambient pass (AMBIENT_UPDATE_INTERVAL)
 	UBOOL		RestartSounds;
+	UBOOL		Muted;				// retail (+0x5C): MUTESOUNDS/UNMUTESOUNDS -- every voice's gain held at 0
 	UBOOL		OggUsable;			// libvorbisfile's struct layout matches ours (the A5 guard)
 	FPointRegion RegionListener;
 
@@ -246,7 +255,7 @@
 	FLOAT SoundPriority( UViewport* Viewport, FVector Location, FLOAT Volume, FLOAT Radius, INT Flags );
 	INT GetNewStreamStruct();
 	UBOOL alError( const TCHAR* Text, UBOOL Log = true );
-	INT FindLeastImportantSound( USound *pIncomingSound );
+	INT FindLeastImportantSound( USound *pIncomingSound, INT Id, FLOAT Priority );	// retail signature
 
 	// Internals (the port's).
 	FLOAT SourceGain( INT Index, FLOAT Volume );
--- a/Source/NOpenALDrv/NOpenALDrv.cpp
+++ b/Source/NOpenALDrv/NOpenALDrv.cpp
@@ -16,8 +16,11 @@
 	                                                    ../Music/<Song>.ogg then ../Music/<Song>
 	  listener axes tuned for Creative's DS3D           the template's right-handed conversion (Z negated)
 
-	What stays ALAudio's: voice allocation and priorities (SLOT_Talk boost, FindLeastImportantSound), the
-	occlusion/radius smoothing, fades (FADE_In/FADE_Out), ModifySound, flag-driven stops, ambient sounds, the
+	What stays ALAudio's (the RETAIL 2002-10-09 ALAudio.dll where it differs from the prototype source: no sound
+	occlusion, register-after-voice-choice in PlaySound, ambient pass every 0.2 s capped at 3 voices per
+	AmbientSound, MUTESOUNDS/UNMUTESOUNDS/SETVOLUMES/SOUNDSOURCESTATES, music stopped by SetViewport -- read off
+	the retail binary): voice allocation and priorities (SLOT_Talk boost, FindLeastImportantSound), the
+	radius smoothing, fades (FADE_In/FADE_Out), ModifySound, flag-driven stops, ambient sounds, the
 	PlayMusic/StopMusic handle contract (index+1, 0 = failure), the USound per-Id stream bookkeeping
 	(AddStreamHandle/BindStream/GetHandle), FLipSyncAnimMgr, the Exec commands, DISTANCE_FACTOR and
 	AL_INVERSE_DISTANCE_CLAMPED with reference distance = radius.
@@ -217,7 +220,9 @@
 	Initialized		= 0;
 	LastRealtime	= 0;
 	LastPaused		= 0;
+	AmbientTimer	= 0.f;
 	RestartSounds	= 0;
+	Muted			= 0;
 	OggUsable		= 0;
 	SoundDevice		= NULL;
 	SoundContext	= NULL;
@@ -493,9 +498,10 @@
 {
 	guard(UNOpenALAudioSubsystem::SetViewport);
 
-	// Stop playing sounds.
+	// Stop playing sounds -- the music too: retail dropped the prototype's "&& !(Flags & SF_Music)", so a map
+	// change (UGameEngine::LoadMap calls SetViewport(GetViewport())) or a device switch ends the song.
 	for( INT i=0; i<Sources.Num(); i++ )
-		if( Sources(i).Started && !(Sources(i).Flags & SF_Music) )
+		if( Sources(i).Started )
 			StopSoundSource( i );
 
 	if( Viewport != InViewport )
@@ -506,14 +512,8 @@
 		// Set listener region to 0.
 		appMemzero( &RegionListener, sizeof(RegionListener) );
 
-		if( Viewport && UsePrecache && Initialized )
-		{
-			// Register everything that is a plain buffer. (Streams are per play; registering every XA sound
-			// would only exhaust the stream table -- retail shipped UsePrecache=False.)
-			for( TObjectIterator<USound> SoundIt; SoundIt; ++SoundIt )
-				if( !(SoundIt->CoreFlags & SF_Streaming) )
-					RegisterSound( *SoundIt );
-		}
+		// (Retail also dropped the UsePrecache pass that registered every USound here; the key is still
+		// read from the ini but no longer used.)
 	}
 
 	unguard;
@@ -996,7 +996,7 @@
 void UNOpenALAudioSubsystem::ApplyVolumes()
 {
 	guard(UNOpenALAudioSubsystem::ApplyVolumes);
-	if( !Initialized )
+	if( !Initialized || Muted )
 		return;
 	for( INT i=0; i<Sources.Num(); i++ )
 		if( Sources(i).Id && Sources(i).FadeMode == FADE_None )
@@ -1020,17 +1020,8 @@
 	// Add any resident flags from the sound -tg
 	Sound->WorkingFlags = Sound->CoreFlags | Flags;
 
-	// If we failed to register the sound, kill the least important playing sound (if any are "least
-	// important") and try again.
-	if( !RegisterSound( Sound ) )
-	{
-		INT iIndex = FindLeastImportantSound( Sound );
-		if( iIndex == -1 )
-			return -1;
-		StopSoundSource( iIndex );
-		if( !RegisterSound( Sound ) )
-			return -1;
-	}
+	// (Retail registers the sound only once a voice is chosen and freed -- see below. The prototype registered
+	// first, and on failure stole a voice through FindLeastImportantSound and retried.)
 
 	Volume = Clamp( Volume, 0.f, 1.f );
 
@@ -1048,20 +1039,8 @@
 	}
 	else
 	{
-		// Calculate occlusion before priority.
-		FLOAT OcclusionRadius = Radius;
-		if( !(Flags & SF_No3D) && Actor && Actor->GetLevel() )
-		{
-			guard(SoundOcclusion);
-			if( !Actor->GetLevel()->IsAudibleAt( Location, LastPosition, Actor, (ESoundOcclusion) Actor->SoundOcclusion ) )
-			{
-				FPointRegion RegionSource = Actor->GetLevel()->Model->PointRegion( Actor->Level, Location );
-				OcclusionRadius = Radius * Actor->GetLevel()->CalculateRadiusMultiplier( RegionSource.ZoneNumber, RegionListener.ZoneNumber );
-				OcclusionRadius *= OCCLUSION_FACTOR;
-			}
-			unguard;
-		}
-		Priority = SoundPriority( Viewport, Location, Volume * SoundVolume, OcclusionRadius, Flags );
+		// (Retail: no occlusion pass before the priority.)
+		Priority = SoundPriority( Viewport, Location, Volume * SoundVolume, Radius, Flags );
 	}
 
 	FLOAT BestPriority = Priority;
@@ -1092,15 +1071,25 @@
 		}
 	}
 
-	// If no room to play the sound, see if we can make some room by stopping a sound that is currently
-	// playing multiple instances.
-	if( Index == -1 )
-		Index = FindLeastImportantSound( Sound );
+	// If no room to play the sound, see if we can make some room by killing a less important sound (retail).
 	if( Index == -1 )
-		return -1;
+	{
+		debugf( NAME_DevSound, TEXT("No empty slots in Sources() - try killing a sound") );
+		Index = FindLeastImportantSound( Sound, Id, Priority );
+		if( Index == -1 )
+		{
+			debugf( NAME_DevSound, TEXT("Unable to kill a sound - returning (Index == -1)") );
+			return -1;
+		}
+	}
 
 	StopSoundSource( Index );
 
+	// Retail registers here, after the voice (and the stream it held) is free: the early returns above no
+	// longer leave a registered-but-unbound stream behind, and a full stream table has just got one back.
+	if( !RegisterSound( Sound ) )
+		return -1;
+
 	// Here's where we bind the stream to the sound (if a streaming sound).
 	FNALStream* Stream = NULL;
 	if( Sound->CoreFlags & SF_Streaming )
@@ -1136,27 +1125,11 @@
 
 	FNALSource& Src = Sources(Index);
 
-	// Set default values.
+	// Set default values. (Retail: no initial occlusion pass -- the radius is the radius.)
 	Src.ZoneRadius		= Radius;
 	Src.UsedRadius		= Radius;
 	Src.WantedRadius	= Radius;
 
-	// Calculate initial radius.
-	if( !(Flags & SF_No3D) && Actor && Actor->GetLevel() )
-	{
-		guard(SoundOcclusion);
-		if( !Actor->GetLevel()->IsAudibleAt( Location, LastPosition, Actor, (ESoundOcclusion) Actor->SoundOcclusion ) )
-		{
-			FPointRegion RegionSource = Actor->GetLevel()->Model->PointRegion( Actor->Level, Location );
-			FLOAT TempRadius = Radius * Actor->GetLevel()->CalculateRadiusMultiplier( RegionSource.ZoneNumber, RegionListener.ZoneNumber );
-			Src.ZoneRadius	= TempRadius;
-			TempRadius *= OCCLUSION_FACTOR;
-			Src.UsedRadius	= TempRadius;
-			Src.WantedRadius	= TempRadius;
-		}
-		unguard;
-	}
-
 	// Setup the voice.
 	Src.Sound			= Sound;
 	Src.Id				= Id;
@@ -1182,7 +1155,7 @@
 	alSourceStop( sid );
 	alSourcei( sid, AL_BUFFER, 0 );
 	alSourcef( sid, AL_REFERENCE_DISTANCE, Src.UsedRadius * DISTANCE_FACTOR );
-	alSourcef( sid, AL_GAIN, Src.FadeMode == FADE_In ? 0.f : SourceGain( Index, Src.Volume ) );
+	alSourcef( sid, AL_GAIN, ( Muted || Src.FadeMode == FADE_In ) ? 0.f : SourceGain( Index, Src.Volume ) );
 	alSourcef( sid, AL_PITCH, Pitch );
 	alSourcei( sid, AL_LOOPING, ( !Stream && (Src.Flags & SF_Looping) ) ? AL_TRUE : AL_FALSE );
 	const FVector ALLocation = ToALSpace( Src.Location );
@@ -1306,29 +1279,17 @@
 			switch( parameter )
 			{
 			case SOUND_Volume:
+				// (Retail scales by SoundVolume here; SourceGain does that live. A muted voice stays silent.)
 				Sources(i).Volume = Clamp<FLOAT>( Value, 0.01f, 1.0f );
-				alSourcef( sid, AL_GAIN, SourceGain( i, Sources(i).Volume ) );
+				if( !Muted )
+					alSourcef( sid, AL_GAIN, SourceGain( i, Sources(i).Volume ) );
 				break;
 
 			case SOUND_Radius:
+				// (Retail: no occlusion pass.)
 				Sources(i).ZoneRadius	= Value;
 				Sources(i).UsedRadius	= Value;
 				Sources(i).WantedRadius	= Value;
-				if( !(Sources(i).Flags & SF_No3D) && Sources(i).Actor && Sources(i).Actor->GetLevel() )
-				{
-					guard(SoundOcclusion);
-					ULevel* Level = Sources(i).Actor->GetLevel();
-					if( !Level->IsAudibleAt( Sources(i).Actor->Location, LastPosition, Sources(i).Actor, (ESoundOcclusion) Sources(i).Actor->SoundOcclusion ) )
-					{
-						FPointRegion RegionSource = Level->Model->PointRegion( Sources(i).Actor->Level, Sources(i).Actor->Location );
-						FLOAT TempRadius = Value * Level->CalculateRadiusMultiplier( RegionSource.ZoneNumber, RegionListener.ZoneNumber );
-						Sources(i).ZoneRadius	= TempRadius;
-						TempRadius *= OCCLUSION_FACTOR;
-						Sources(i).UsedRadius	= TempRadius;
-						Sources(i).WantedRadius	= TempRadius;
-					}
-					unguard;
-				}
 				alSourcef( sid, AL_REFERENCE_DISTANCE, Sources(i).UsedRadius * DISTANCE_FACTOR );
 				break;
 
@@ -1512,8 +1473,15 @@
 	Update: the per-frame loop (ALAudio's), plus the stream pump.
 ------------------------------------------------------------------------------------*/
 
+// Retail sorted the candidates twice behind a global mode flag: by AmbientSound (so the entries of one sound form
+// one run; retail ordered the runs by pointer value, here by object index -- deterministic, equally arbitrary),
+// then each run by priority, highest first. One composite key does both.
 static INT Compare( FNALAmbient& A, FNALAmbient& B )
 {
+	USound* SA = A.Actor->AmbientSound;
+	USound* SB = B.Actor->AmbientSound;
+	if( SA != SB )
+		return SA->GetIndex() < SB->GetIndex() ? -1 : 1;
 	return (B.Priority - A.Priority >= 0) ? 1 : -1;
 }
 
@@ -1579,79 +1547,113 @@
 	}
 	unguard;
 
-	// See if any new ambient sounds need to be started.
-	guard(HandleAmbience);
-	if( Realtime && !Paused )
-	{
-		TArray<FNALAmbient> AmbientSounds;
-
-		for( INT a=0; a<Level->Actors.Num(); a++ )
-		{
-			AActor* Actor = Level->Actors(a);
-			if( !Actor || !Actor->AmbientSound )
-				continue;
-			const INT Id = Actor->GetIndex()*16 + SLOT_Ambient*2;
-			UBOOL Playing = 0;
-			for( INT j=0; j<Sources.Num() && !Playing; j++ )
-				Playing = Sources(j).Id == Id;
-			if( Playing )
-				continue;
-			if( Actor->IsOwnedBy( ViewTarget ) )
-			{
-				INT iAmbient = AmbientSounds.Add( 1 );
-				AmbientSounds(iAmbient).Actor		= Actor;
-				AmbientSounds(iAmbient).Priority	= SoundPriority( Viewport, Actor->Location, Actor->SoundVolume/255.f, Actor->SoundRadius, SF_Looping | SF_No3D | SF_UpdatePitch );
-				AmbientSounds(iAmbient).Flags		= SF_Looping | SF_No3D | SF_UpdatePitch;
-				AmbientSounds(iAmbient).Id			= Id;
-			}
-			else
-			{
-				// Sound occlusion.
-				FLOAT RM = 1.f;
-				if( !Level->IsAudibleAt( Actor->Location, ViewLocation, Actor, (ESoundOcclusion) Actor->SoundOcclusion ) )
+	// Ambient sounds -- retail: a pass every AMBIENT_UPDATE_INTERVAL seconds (the prototype ran it every frame),
+	// both the starting and the out-of-range stopping below.
+	AmbientTimer += DeltaTime;
+	if( AmbientTimer > AMBIENT_UPDATE_INTERVAL )
+	{
+		AmbientTimer = 0.f;
+
+		// See if any new ambient sounds need to be started.
+		guard(HandleAmbience);
+		if( Realtime && !Paused )
+		{
+			TArray<FNALAmbient> AmbientSounds;
+
+			for( INT a=0; a<Level->Actors.Num(); a++ )
+			{
+				AActor* Actor = Level->Actors(a);
+				if( !Actor || !Actor->AmbientSound )
+					continue;
+				const INT Id = Actor->GetIndex()*16 + SLOT_Ambient*2;
+				UBOOL Playing = 0;
+				for( INT j=0; j<Sources.Num() && !Playing; j++ )
+					Playing = Sources(j).Id == Id;
+				if( Playing )
+					continue;
+				// (Retail: the priority volume includes the global SoundVolume, as a playing voice's does; and
+				// no occlusion -- a plain SoundRadius*GAudioMaxRadiusMultiplier range check.)
+				if( Actor->IsOwnedBy( ViewTarget ) )
 				{
-					FPointRegion RegionSource = Level->Model->PointRegion( Actor->Level, Actor->Location );
-					RM *= Level->CalculateRadiusMultiplier( RegionSource.ZoneNumber, RegionListener.ZoneNumber );
+					INT iAmbient = AmbientSounds.Add( 1 );
+					AmbientSounds(iAmbient).Actor		= Actor;
+					AmbientSounds(iAmbient).Priority	= SoundPriority( Viewport, Actor->Location, Actor->SoundVolume/255.f * SoundVolume, Actor->SoundRadius, SF_Looping | SF_No3D | SF_UpdatePitch );
+					AmbientSounds(iAmbient).Flags		= SF_Looping | SF_No3D | SF_UpdatePitch;
+					AmbientSounds(iAmbient).Id			= Id;
+					AmbientSounds(iAmbient).SourceIndex	= -1;
 				}
-				if( FDistSquared(ViewLocation,Actor->Location) <= Square(RM*Actor->SoundRadius*GAudioMaxRadiusMultiplier) )
+				else if( FDistSquared(ViewLocation,Actor->Location) <= Square(Actor->SoundRadius*GAudioMaxRadiusMultiplier) )
 				{
 					INT iAmbient = AmbientSounds.Add( 1 );
 					AmbientSounds(iAmbient).Actor		= Actor;
-					AmbientSounds(iAmbient).Priority	= SoundPriority( Viewport, Actor->Location, Actor->SoundVolume/255.f, RM*Actor->SoundRadius, SF_Looping | SF_UpdatePitch );
+					AmbientSounds(iAmbient).Priority	= SoundPriority( Viewport, Actor->Location, Actor->SoundVolume/255.f * SoundVolume, Actor->SoundRadius, SF_Looping | SF_UpdatePitch );
 					AmbientSounds(iAmbient).Flags		= SF_Looping | SF_UpdatePitch;
 					AmbientSounds(iAmbient).Id			= Id;
+					AmbientSounds(iAmbient).SourceIndex	= -1;
 				}
 			}
-		}
 
-		// Sort ambient sounds by priority to avoid thrashing of channels.
-		if( AmbientSounds.Num() )
-		{
-			Sort( &AmbientSounds(0), AmbientSounds.Num() );
-			for( INT iAmbient=0; iAmbient<AmbientSounds.Num(); iAmbient++ )
+			// Retail: the ambient voices already playing compete with the new candidates.
+			for( INT i=0; i<Sources.Num(); i++ )
 			{
-				AActor* Actor = AmbientSounds(iAmbient).Actor;
-				// Early out if sound couldn't be played because of priority.
-				if( PlaySound( Actor, AmbientSounds(iAmbient).Id, Actor->AmbientSound, Actor->Location, Actor->SoundVolume/255.f,
-					Actor->SoundRadius, Actor->SoundPitch / 64.f, AmbientSounds(iAmbient).Flags, 0.f, AmbientSounds(iAmbient).Priority ) == -1 )
-					break;
+				if( Sources(i).Id && (Sources(i).Id&14) == SLOT_Ambient*2 && Sources(i).Actor && Sources(i).Actor->AmbientSound )
+				{
+					INT iAmbient = AmbientSounds.Add( 1 );
+					AmbientSounds(iAmbient).Actor		= Sources(i).Actor;
+					AmbientSounds(iAmbient).Priority	= Sources(i).Priority;
+					AmbientSounds(iAmbient).Flags		= Sources(i).Flags;
+					AmbientSounds(iAmbient).Id			= Sources(i).Id;
+					AmbientSounds(iAmbient).SourceIndex	= i;
+				}
+			}
+
+			// Per AmbientSound, keep the MAX_AMBIENT_INSTANCES highest priorities: stop the playing voices past
+			// them, start the candidates among them. A refusal from PlaySound ends that sound's run only.
+			if( AmbientSounds.Num() )
+			{
+				Sort( &AmbientSounds(0), AmbientSounds.Num() );
+				for( INT First=0; First<AmbientSounds.Num(); )
+				{
+					USound* RunSound = AmbientSounds(First).Actor->AmbientSound;
+					INT End = First;
+					while( End<AmbientSounds.Num() && AmbientSounds(End).Actor->AmbientSound == RunSound )
+						End++;
+					for( INT k=First+MAX_AMBIENT_INSTANCES; k<End; k++ )
+					{
+						// (Retail stopped the recorded index blindly; a voice an earlier run's PlaySound took over
+						// meanwhile carries another Id and is left alone.)
+						const INT Idx = AmbientSounds(k).SourceIndex;
+						if( Idx != -1 && Sources(Idx).Id == AmbientSounds(k).Id )
+							StopSoundSource( Idx );
+					}
+					for( INT k=First; k<Min(End, First+MAX_AMBIENT_INSTANCES); k++ )
+					{
+						if( AmbientSounds(k).SourceIndex != -1 )
+							continue;
+						AActor* Actor = AmbientSounds(k).Actor;
+						if( PlaySound( Actor, AmbientSounds(k).Id, Actor->AmbientSound, Actor->Location, Actor->SoundVolume/255.f,
+							Actor->SoundRadius, Actor->SoundPitch / 64.f, AmbientSounds(k).Flags, 0.f, AmbientSounds(k).Priority ) == -1 )
+							break;
+					}
+					First = End;
+				}
 			}
 		}
-	}
-	unguard;
+		unguard;
 
-	// Update all playing ambient sounds.
-	guard(UpdateAmbience);
-	for( INT i=0; i<Sources.Num(); i++ )
-	{
-		if( (Sources(i).Id&14) == SLOT_Ambient*2 && Sources(i).Actor )
+		// Update all playing ambient sounds.
+		guard(UpdateAmbience);
+		for( INT i=0; i<Sources.Num(); i++ )
 		{
-			FLOAT NewDist = FDistSquared( ViewLocation, Sources(i).Actor->Location );
-			if( (NewDist > Square(Sources(i).ZoneRadius*GAudioMaxRadiusMultiplier)) || (Sources(i).Actor->AmbientSound != Sources(i).Sound) )
-				StopSoundSource( i );
+			if( (Sources(i).Id&14) == SLOT_Ambient*2 && Sources(i).Actor )
+			{
+				FLOAT NewDist = FDistSquared( ViewLocation, Sources(i).Actor->Location );
+				if( (NewDist > Square(Sources(i).ZoneRadius*GAudioMaxRadiusMultiplier)) || (Sources(i).Actor->AmbientSound != Sources(i).Sound) )
+					StopSoundSource( i );
+			}
 		}
+		unguard;
 	}
-	unguard;
 
 	// Update all active sounds.
 	guard(UpdateSounds);
@@ -1722,22 +1724,15 @@
 			Volume *= (1.f - Sources(i).FadeTime / Sources(i).FadeDuration);
 		}
 
-		alSourcef( Sources(i).Source, AL_GAIN, SourceGain( i, Volume ) );
+		// (Retail: a muted voice keeps gain 0 -- MUTESOUNDS.)
+		if( !Muted )
+			alSourcef( Sources(i).Source, AL_GAIN, SourceGain( i, Volume ) );
 
 		if( !(Sources(i).Flags & SF_No3D) )
 		{
-			// Sound occlusion.
+			// (Retail: no occlusion -- the zone radius is the radius.)
 			FLOAT Radius = Sources(i).Radius;
-			if( !Level->IsAudibleAt( Sources(i).Location, ViewLocation, Sources(i).Actor,
-				Sources(i).Actor ? (ESoundOcclusion) Sources(i).Actor->SoundOcclusion : OCCLUSION_Default ) )
-			{
-				FPointRegion RegionSource = Level->Model->PointRegion( Viewport->Actor->Level, Sources(i).Location );
-				Radius *= Level->CalculateRadiusMultiplier( RegionSource.ZoneNumber, RegionListener.ZoneNumber );
-				Sources(i).ZoneRadius = Radius;
-				Radius *= OCCLUSION_FACTOR;
-			}
-			else
-				Sources(i).ZoneRadius = Radius;
+			Sources(i).ZoneRadius = Radius;
 
 			// Smooth transition between radii.
 			if( Sources(i).WantedRadius != Radius )
@@ -1754,8 +1749,9 @@
 			alSourcef( Sources(i).Source, AL_REFERENCE_DISTANCE, Radius * DISTANCE_FACTOR );
 		}
 
-		// Update the priority.
-		Sources(i).Priority = SoundPriority( Viewport, Sources(i).Location, Sources(i).Volume * SoundVolume, Sources(i).ZoneRadius, Sources(i).Flags );
+		// Update the priority. (Retail's voice volume is the scaled one: SoundVolume for a sound, MusicVolume for
+		// a song -- SourceGain.)
+		Sources(i).Priority = SoundPriority( Viewport, Sources(i).Location, SourceGain( i, Sources(i).Volume ), Sources(i).ZoneRadius, Sources(i).Flags );
 
 		if( !(Sources(i).Flags & SF_No3D) )
 		{
@@ -1905,6 +1901,50 @@
 		StopSound( NULL, NULL );
 		return 1;
 	}
+	else if( ParseCommand(&Str,TEXT("SETVOLUMES")) )
+	{
+		// Retail: HGame's FESoundVideoPage sends "SETVOLUMES MUSIC=<f>" / "SETVOLUMES SOUND=<f>" (then writes the
+		// ini with "set ini:Engine.Engine.AudioDevice ..."): new global volumes, applied to the playing voices.
+		FLOAT Value;
+		if( Parse( Str, TEXT("SOUND="), Value ) )
+			SoundVolume = Clamp( Value, 0.f, 1.f );
+		if( Parse( Str, TEXT("MUSIC="), Value ) )
+			MusicVolume = Clamp( Value, 0.f, 1.f );
+		ApplyVolumes();
+		return 1;
+	}
+	else if( ParseCommand(&Str,TEXT("SOUNDSOURCESTATES")) )
+	{
+		// Retail debug command.
+		for( INT i=0; i<Sources.Num(); i++ )
+		{
+			if( Sources(i).Id )
+			{
+				ALint State = 0;
+				alGetSourcei( Sources(i).Source, AL_SOURCE_STATE, &State );
+				Ar.Logf( TEXT("Source %d, State = %d"), i, State );
+			}
+			else
+				Ar.Logf( TEXT("Source %d not active"), i );
+		}
+		return 1;
+	}
+	else if( ParseCommand(&Str,TEXT("MUTESOUNDS")) )
+	{
+		// Retail: HGame mutes everything while a cut-scene is fast-forwarded (SkipCS), UNMUTESOUNDS after.
+		// Voices keep playing; PlaySound/Update/ApplyVolumes leave their gain at 0 until then.
+		Muted = 1;
+		for( INT i=0; i<Sources.Num(); i++ )
+			if( Sources(i).Id )
+				alSourcef( Sources(i).Source, AL_GAIN, 0.f );
+		return 1;
+	}
+	else if( ParseCommand(&Str,TEXT("UNMUTESOUNDS")) )
+	{
+		Muted = 0;
+		ApplyVolumes();		// fading voices get theirs from the next Update
+		return 1;
+	}
 	else if( ParseCommand(&Str,TEXT("NOPENALSTATS")) )
 	{
 		INT Playing = 0, OpenStreams = 0;
@@ -1936,15 +1976,15 @@
 					iALBufferSize += iTemp;
 				}
 				iOverhead += StreamChunkSize;
-				Ar.Logf( TEXT("Streaming Sound:  Name = %s  Sound data = %iK  ALAudio buffer = %iK  Streaming buffer = %iK"),
-					Sound->GetName(), iSoundDataSize / 1024, iALBufferSize / 1024, StreamChunkSize / 1024 );
+				Ar.Logf( TEXT("Streaming Sound:  Name = %s  Pri =%d Sound data = %iK  ALAudio buffer = %iK  Streaming buffer = %iK"),
+					Sound->GetName(), (INT)(Sources(i).Priority * 1000.f), iSoundDataSize / 1024, iALBufferSize / 1024, StreamChunkSize / 1024 );
 			}
 			else if( Sound->GetHandle() > 0 && Sound->GetHandle() <= Buffers.Num() )
 			{
 				ALint iTemp = 0;
 				alGetBufferi( Buffers( Sound->GetHandle() - 1 ).Buffer, AL_SIZE, &iTemp );
 				iALBufferSize = iTemp;
-				Ar.Logf( TEXT("Sound:  Name = %s  Sound data = %iK  ALAudio buffer = %iK"), Sound->GetName(), iSoundDataSize / 1024, iALBufferSize / 1024 );
+				Ar.Logf( TEXT("Sound:  Name = %s  Pri = %d Sound data = %iK  ALAudio buffer = %iK"), Sound->GetName(), (INT)(Sources(i).Priority * 1000.f), iSoundDataSize / 1024, iALBufferSize / 1024 );
 			}
 			iOverhead += iALBufferSize;
 			iTotalMem += iALBufferSize + iSoundDataSize;
@@ -2034,20 +2074,24 @@
 	return true;
 }
 
-#define INITIAL_MIN_COUNT	3	// we need to find at least this many playing instances of a sound before we'll kill it
-
-INT UNOpenALAudioSubsystem::FindLeastImportantSound( USound *pIncomingSound )
+// Retail's version (FindLeastImportantSound(USound*,INT,FLOAT)): only a voice LESS important than the incoming
+// sound may go. Among the AMBIENT voices, the AmbientSound with the most playing instances is culled first; with
+// no ambient voice playing, an incoming ambient sound finds nothing and any other sound may take any (less
+// important) voice. The incoming USound is no longer consulted, and SLOT_Talk is protected by its priority of
+// 100 instead of by slot. PlaySound only calls this after its own scan found no voice of lower priority, so in
+// practice it returns -1 -- retail too: the prototype's "steal the 4th instance of a sound" is gone.
+INT UNOpenALAudioSubsystem::FindLeastImportantSound( USound *pIncomingSound, INT Id, FLOAT Priority )
 {
 	guard(UNOpenALAudioSubsystem::FindLeastImportantSound);
 	INT					iRet = -1;
 	TMap<USound*, INT>	SoundInstances;
-	INT					iMaxCount = INITIAL_MIN_COUNT;
+	INT					iMaxCount = 0;
 	USound*				pMaxCountSound = NULL;
 
-	// Count the number of instances of each playing sound.
+	// Count the number of instances of each playing ambient sound.
 	for( INT i=0; i<Sources.Num(); i++ )
 	{
-		if( Sources(i).Id != 0 && Sources(i).Sound )
+		if( Sources(i).Id != 0 && (Sources(i).Id & 14) == SLOT_Ambient*2 && Sources(i).Sound )
 		{
 			INT iNumInstances = SoundInstances.FindRef( Sources(i).Sound ) + 1;
 			SoundInstances.Set( Sources(i).Sound, iNumInstances );
@@ -2059,25 +2103,29 @@
 		}
 	}
 
-	// If we found a sound with enough playing instances AND it's not the same sound we're trying to play,
-	// determine the most distant instance and return its Sources() index.
-	if( pMaxCountSound && pMaxCountSound != pIncomingSound )
+	FLOAT fLowestPriority = 1000.f;
+	for( INT i=0; i<Sources.Num(); i++ )
 	{
-		FLOAT fLowestPriority = 1000.f;
-		for( INT i=0; i<Sources.Num(); i++ )
+		if( Sources(i).Id == 0 || !Sources(i).Sound )
+			continue;
+		UBOOL bCandidate;
+		if( pMaxCountSound )
+			bCandidate = Sources(i).Sound == pMaxCountSound;
+		else if( (Id & 14) == SLOT_Ambient*2 )
+			bCandidate = (Sources(i).Id & 14) == SLOT_Ambient*2;
+		else
+			bCandidate = 1;
+		if( bCandidate && Sources(i).Priority < Priority )
 		{
-			// Don't even check sounds that are playing in SLOT_Talk - we never want to kill them.
-			if( Sources(i).Id != 0 && ((Sources(i).Id & 14) != (SLOT_Talk * 2)) && pMaxCountSound == Sources(i).Sound )
+			FLOAT fPriority = SoundPriority( Viewport, Sources(i).Location, SourceGain( i, Sources(i).Volume ), Sources(i).Radius, Sources(i).Sound->WorkingFlags );
+			if( fPriority < fLowestPriority )
 			{
-				FLOAT fPriority = SoundPriority( Viewport, Sources(i).Location, Sources(i).Volume, Sources(i).Radius, Sources(i).Sound->WorkingFlags );
-				if( fPriority < fLowestPriority )
-				{
-					fLowestPriority = fPriority;
-					iRet = i;
-				}
+				fLowestPriority = fPriority;
+				iRet = i;
 			}
 		}
 	}
+	debugf( NAME_DevSound, TEXT("FindLeastImportantSound() returning %d, pMaxCountSound = %d"), iRet, pMaxCountSound != NULL );
 	return iRet;
 	unguard;
 }
```
