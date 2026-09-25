# Source layout — how the raw drop was cleaned up

The drop arrived as `HarryPotter2/Unreal/<47 folders>`: sources, build outputs, game content, closed
libraries and VSS bookkeeping side by side, the way the 2002 tree was checked out of Visual SourceSafe.
The first commit moved everything into the four homes the engine-ut99 template uses. Nothing was edited in
the move; only VSS files (`vssver.scc`, `mssccprj.scc`, `*.vsscc`), MSVC scratch files (`*.aps`, `*.plg`,
`*.opt`, `*.clw`) and four prebuilt tool executables were deleted.

## Source/ — every source folder, unchanged inside
| Folder | What it is | Built by us? |
|--------|-----------|--------------|
| `Core`, `Engine`, `Render`, `Fire`, `SoftDrv`, `Editor` | the engine packages (C++ `Inc/`+`Src/`, script `Classes/`) | yes (Editor stripped, like the template) |
| `Launch`, `UCC` | the original Win32 launcher (`Game.exe`) and commandlet host | UCC yes; the SDL launcher is new (`Source/Game`) |
| `ALAudio` | the shipped audio driver (OpenAL 1.0 + EAX + vorbisfile + coda) | reference for the portable `NOpenALDrv` |
| `WinDrv`, `Window`, `D3DDrv`, `UnrealEd`, `FrontEnd`, `DebuggerLaunch`, `PackageTool`, `CutCompiler`, `Setup`, `GestureComp`, `GestureDrawingTool`, `EdSrc`, `Tools` | Windows-only drivers, editor, tools (`EdSrc` = the VB6 UnrealEd 1 shell; `Tools` = BuildLocalDialog + TooLame) | no — reference |
| `HGame`, `HPModels`, `HPParticle`, `HProps`, `HPSounds`, `HPDialog`, `UWindow`, `UnrealShare` | the game's UnrealScript packages (`Classes/*.uc`; their binary import sources moved to `Assets/PackageSources/`) | no — the shipped `Assets/System/*.u` are loaded |
| `CutScenes` | the cut-scene scripts (`*.txt`) CutCompiler compiled into `System/CUTSCENES/*.int` | no |
| `Unreal.dsw`, `_FullBuild.dsp`, `forEA.bat` | the original MSVC 6 workspace + EA drop script | no |

`Engine/Textures` and `Editor/Textures` (`.pcx` import sources for `Engine.u`/`Editor.u`) went to
`Assets/PackageSources/{Engine,Editor}/Textures`; the headers `Engine/Inc/S3tc.h` and `ENCVAG.H` stayed
with the sources that include them.

## Thirdparty/ — libraries the original build linked
| Folder | From | Note |
|--------|------|------|
| `OpenAL` | `Unreal/OpenAL` | OpenAL 1.0 SDK headers (`altypes.h` era) + `OpenAL32.lib`/`eax.lib`/`eaxguid.lib` |
| `Vorbis` | `Unreal/Vorbis` | Xiph headers + Release/Debug `.lib`s (+ a stray `vorbisfile.c` under `Lib/Save`) |
| `EAAudioCodec` | `Unreal/EAAudioCodec` | EA `coda` codec: `include/coda.h` + `codapcz.lib`/`codapcd.lib`, **no source** |
| `DirectX7` | `Unreal/DirectX7` | D3D7/DDraw SDK used by `D3DDrv` |
| `DirectX3`, `DirectX5` | `Unreal/WinDrv/DirectX3`, `…/DirectX5` (+ `WinDrv/Lib` → `DirectX5/Lib`) | the DX headers `WinDrv` compiled against |
| `s3tc`, `ENCVAG` | `Unreal/Engine/Lib` | S3 texture-compression lib (editor encode path), Sony VAG encoder (PS2) |
| `MPeg` | `Unreal/Engine/MPeg` | `MpegLib.lib` + thin wrappers (`scompmpg.cpp`, `TbFile.cpp`) — MP2 sound compression tooling |

## Config/System/ — the shipped inis
`Default.ini`, `DefUser.ini` from `Unreal/System`. The engine derives `Game.ini`/`User.ini` from them on
first run (the same `appInit` scheme as UT99's `UnrealTournament.ini`). `web/` holds the tracked web boot
set once the web build lands.

## Assets/ — game content (gitignored; see `Assets/README.md`)
`Maps`, `Textures`, `Sounds`, `Music`, `System` (minus the two inis), `SystemLocalized`, `Fonts`, `Help`,
`Save` (was `SAVE`), `DialogSources` (was `Dialog`), and `PackageSources/<Pkg>/…` for the packages' binary
import trees.

## Deleted
`GestureComp/Release/GestureComp.exe`, `GestureDrawingTool/Release/GestureDrawingTool.exe`,
`Tools/oggdrop.exe`, `Tools/PackageTool.exe` (prebuilt tools; sources or upstream projects exist), the VSS
and MSVC scratch files, `System/System.dsp`.
