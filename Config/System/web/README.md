# Config/System/web — the web boot config

`Default.ini` and `DefUser.ini` here are the web configuration. The Emscripten link of `Source/Game`
`--preload-file`s them to `/System/` and lists them in `LINK_DEPENDS`, so editing one relinks. They come from here,
not from the asset tree: a game install's inis carry none of the web settings (the GL render devices, the
`[WebRTCDrv.WebRTCNetDriver]` section). HP2's `appInit( TEXT("Game") )` derives `/System/Game.ini` from
`Default.ini` only when `Game.ini` does not exist, and `/System/User.ini` from `DefUser.ini` when `User.ini` does
not exist or `Game.ini` has just been derived; a newer `Default.ini` does not re-derive them, because the file manager's
`GetGlobalTime` is always 0. The frontend keeps the derived pair in IndexedDB (`<target-id>-user-data`:
`hp2-proto-user-data`, `hp2-retail-exp-user-data`), so after changing this folder clear that store (or the site
data), or an existing profile keeps its old copy.

`../Default.ini` and `../DefUser.ini` are HP2's retail inis with this port's keys, which the native release archives
ship in `System/` and the native run tree uses (`tools/dev/mkrun-hp2.sh` copies them); the launcher replaces the Win32
driver name they carry (`WinDrv.WindowsClient`) with the client this build contains.
`../Default.ini` names `NOpenALDrv.NOpenALAudioSubsystem` with its section, as here, turns antialiasing off
(XOpenGL's and UTGLR's `UseAA=False`, NOpenGLES's `NumAASamples=0`) and sets the sticks' scales of 100.
`DefUser.ini` here is the same file as `../DefUser.ini` (player class `HGame.harry`, the spell, broom and duel
bindings, the pad's speeds). Both `DefUser.ini` files set `[Engine.PlayerPawn] bMaxMouseSmoothing=False`: the view and the camera
stop on the frame the mouse stops (the script's default keeps turning for up to `MouseSmoothThreshold` after it).

The driver sends a stick axis as its tilt times `ScaleXYZ` (the move stick) or `ScaleRUV` (the look stick) times 60
per second, so at full tilt an `Axis` binding reads 12 × scale × `Speed`, whatever the frame rate; a key's `Axis`
alias reads 20 × `Speed` (6000 for `MoveForward` and the strafes). The stick speeds are for the scales of 100 that
every `Default.ini` sets (the compiled ones are 200 and 500).

The look stick (`JoyU`, `JoyV`) is bound to the mouse's axes, `aMouseX` and `aMouseY`, which the camera turns and
tilts with (it reads no other look axis), so the mouse sensitivity scales it too. The move stick strafes with its X
(`JoyX`: `aStrafe`, as the strafe keys) and moves ahead and back with its Y (`JoyY`: `aBaseY`, as the arrow keys),
both at the keys' 6000 at full tilt, so Harry moves in the stick's direction as briskly as with the keys. On the broom
its X yaws and its Y pitches (`aJoyBroomYaw`, `aJoyBroomPitch`): in free flight the broom turns at the arrow keys'
rate when they read 16384, which they do at full tilt, and takes them before the mouse's broom axes. While Harry
follows the Snitch in a match he reads only the mouse's broom axes and the arrow keys, so the stick drives those axes
too (`aBroomYaw`, `aBroomPitch`): the follow code adds them to his offset from the Snitch once per frame, and their
speeds give the arrow keys' 350 units per second at 60 frames per second (proportionally more above, less below). In
a spell lesson the stick traces (`aSpellLessonX`, `aSpellLessonY`).

## Rules for `Default.ini`

- **Exactly six `Paths=` lines, in retail order, `../Save/*.usa` last.** At the end of `appInit` the engine writes
  the save-slot path (`../Save`, or `../Save/Slot<n>` with `-SAVESLOT=<n>`) followed by `*.usa` into
  `[Core.System] Paths`, and a write to a key that occurs several times replaces its last occurrence. A seventh
  line would be the one replaced; the music folder is opened by path (`../Music/<Song>.ogg`), not through
  `Paths=`. `appInit` also sets `GSys->Paths[5]` to a per-user save folder when `appUserDir()` is non-empty; on
  every target built here (`Source/Core/Src/UnUnix.cpp`) it returns an empty string, so saves stay under `../Save`.
- On-disk file case (`LocalMap=startup.unr`); no `CdPath`.
- `GameRenderDevice`, `WindowedRenderDevice` and `RenderDevice` name `XOpenGLDrv.XOpenGLRenderDevice`, the default
  device of every build (`../Default.ini` too). On every boot the launcher writes its choice of device to the three
  keys before the engine reads them.
- `AudioDevice=NOpenALDrv.NOpenALAudioSubsystem`, equal to the value the web launcher forces, with its
  `[NOpenALDrv.NOpenALAudioSubsystem]` section: ALAudio's key names and float volumes 0..1 (HGame's options page
  scales them by 100) plus `OutputRate`, `UseHRTF` and `UseReverb`. `[ALAudio.ALAudioSubsystem]` stays because
  `appInit` sizes the file-streaming channels from its `Channels`.
- `NetworkDevice=WebRTCDrv.WebRTCNetDriver` and its section; no `ServerActors` (HP2 has no IpServer or UWeb);
  `DownloadManagers=Engine.ChannelDownload` only.
- `[NSDLDrv.NSDLClient]` carries HP2's two extra `UClient` keys (`NoFractalAnim`, `ParticleDensity`) and the values
  of `UNSDLClient::StaticConstructor`'s compiled defaults, except `ScaleXYZ`/`ScaleRUV` (100 and 100 here, 200 and
  500 compiled).
- No Windows sections (`WinDrv`, `D3DDrv`, `Galaxy`, `IpServer`, `UWeb`, `UBrowser`, the GPU-quirk list).
- The renderer sections (`[XOpenGLDrv.XOpenGLRenderDevice]`, `[OpenGLDrv.OpenGLRenderDevice]`,
  `[NOpenGLESDrv.NOpenGLESRenderDevice]`, `[SoftDrv.SoftwareRenderDevice]`, `[NullDrv.NullRenderDevice]`) are
  described with their drivers: `Source/XOpenGLDrv/WEB_INTEGRATION.md`, `Source/OpenGLDrv/WEB-INTEGRATION.md`, and
  `BUILD.md` (NOpenGLESDrv).

There are no native Asian or Thai fonts: `UViewport::CreateNativeFont` returns NULL and NSDLDrv, the viewport driver
of every build here, does not override it.

Change the web config here, never in the asset tree. `../android/` is this pair with the Android deltas applied by
`tools/dev/sync-android-ini.py`; change it there, by changing this folder or the script's delta list.

## Config version

Every default ini a build ships (this pair, `../Default.ini` and `../DefUser.ini`, and `../android/`) opens with a
section of its own (in `Default.ini` here, after the file's opening comment):

```ini
[ConfigVersion]
Version=1
```

`appInit( TEXT("Game") )` makes a player's `Game.ini` and `User.ini` from `Default.ini` and `DefUser.ini` when they are
absent and never refreshes them, so the `Version` they keep says which defaults they were copied from. A live ini
without the section was made before the number. Right after the config cache starts, before the `-ini:` overrides,
`appInit` adds `Version=0` to each live ini (`Game.ini` and `User.ini`, or the files `INI=` and `USERINI=` name) when it
has no `[ConfigVersion] Version` key and writes that file alone, as any save writes it (comment lines left out), logging
`Config: <file> has no [ConfigVersion]: Version=0 added`; the section then comes last in the file, and a `Version` that
is there is never changed. That marker is the only thing the engine writes into a player's ini for the config version:
nothing reads the number to change anything, and a player's ini is never migrated. No class owns the section, so no
`SaveConfig` or `ResetConfig` touches it and the config cache writes it back as it is. All six files carry the same
number; `tools/dev/sync-android-ini.py` keeps the section first. `retail` shares this number: it merges `main`, and a
bump made there alone is merged back by taking the higher number plus one.

The rule: a commit that adds a default, changes a default value or removes a key in one of these inis, or adds or
changes a compiled or script default of a setting the engine saves to the ini, bumps `Version` by one in all of them
and adds the new number's row below, saying what a player on the version before lacks. Comment-only and
whitespace-only changes do not bump. The frontend's `ini-config-version` pre-commit gate checks the inis; its
`docs/CONVENTIONS.md` § Ini config version has the rule in full.

| Version | What a player on the version before lacks |
|---------|-------------------------------------------|
| 1 | The defaults when the number was introduced: a live ini without the section may lack any of them. |
