# Source layout — where the drop's folders live

In the source drop, a 2002 Visual SourceSafe checkout, everything sits in `HarryPotter2/Unreal/<47 folders>`:
sources, build outputs, game content, closed libraries and VSS bookkeeping side by side. The repository sorts them
into the four homes of the engine-ut99 layout: `Source/`, `Thirdparty/`, `Config/System/` and `Assets/`. The repository's
first commit moves the files without editing them; `/home/ion/WorkUnreal/epic-sources/hp2-ue1-433-proto` is that
tree, the baseline the engine's changes are compared with.

## Source/ — every source folder of the drop, under its own name

| Folder | What it is | Built? |
|--------|-----------|--------|
| `Core`, `Engine`, `Render`, `Fire`, `SoftDrv`, `Editor` | the engine packages (C++ `Inc/`+`Src/`, script `Classes/`) | yes (Editor as the no-GUI variant) |
| `Launch`, `UCC` | the original Win32 launcher (`Game.exe`) and the commandlet host | `UCC` natively on Linux; the SDL launcher is `Source/Game` |
| `ALAudio` | the shipped audio driver (OpenAL 1.0, EAX, vorbisfile, coda) | no; `NOpenALDrv` is its port to openal-soft |
| `WinDrv`, `Window`, `D3DDrv`, `UnrealEd`, `FrontEnd`, `DebuggerLaunch`, `PackageTool`, `CutCompiler`, `Setup`, `GestureComp`, `GestureDrawingTool`, `EdSrc`, `Tools` | Windows-only drivers, the editor and tools (`EdSrc` is the VB6 UnrealEd 1 shell; `Tools` holds BuildLocalDialog and TooLame) | no, reference |
| `HGame`, `HPModels`, `HPParticle`, `HProps`, `HPSounds`, `HPDialog`, `UWindow`, `UnrealShare` | the game's UnrealScript packages (`Classes/*.uc`; their binary import sources are under `Assets/PackageSources/`) | no; the asset tree's `System/*.u` are loaded |
| `CutScenes` | the cut-scene scripts (`*.txt`) CutCompiler compiles into `System/CUTSCENES/*.int` | no |
| `Unreal.dsw`, `_FullBuild.dsp`, `forEA.bat` | the original MSVC 6 workspace and the EA drop script | no |

Beside them sit the port's own folders: the CMake build (`CMakeLists.txt`, `CMakePresets.json`, `cmake/`), the SDL
launcher `Game`, the drivers `NSDLDrv`, `NOpenALDrv`, `NullDrv`, `SoundDrv`, `IpDrv` and `WebRTCDrv`, and the build
glue of the fetched renderers `XOpenGLDrv` and `OpenGLDrv`.

The drop's `Unreal/Engine/Textures` and `Unreal/Editor/Textures` (the `.pcx` import sources of `Engine.u` and
`Editor.u`) belong in `Assets/PackageSources/{Engine,Editor}/Textures`; the headers `Source/Engine/Inc/S3tc.h` and
`Source/Engine/Inc/ENCVAG.H` stay with the sources that include them.

## Thirdparty/ — libraries the original build linked

| Folder | From | Note |
|--------|------|------|
| `OpenAL` | `Unreal/OpenAL` | OpenAL 1.0 SDK headers (`altypes.h` era) + `OpenAL32.lib`/`eax.lib`/`eaxguid.lib` |
| `Vorbis` | `Unreal/Vorbis` | Xiph headers + Release/Debug `.lib`s (+ a stray `vorbisfile.c` under `Lib/Save`) |
| `EAAudioCodec` | `Unreal/EAAudioCodec` | EA `coda` codec: `include/coda.h` + `codapcz.lib`/`codapcd.lib`, no source |
| `DirectX7` | `Unreal/DirectX7` | the D3D7/DDraw SDK `D3DDrv` compiles against |
| `DirectX3`, `DirectX5` | `Unreal/WinDrv/DirectX3`, `…/DirectX5` (`WinDrv/Lib` in `DirectX5/Lib`) | the DirectX headers `WinDrv` compiles against |
| `s3tc`, `ENCVAG` | `Unreal/Engine/Lib` | the S3 texture-compression library (editor encode path), the Sony VAG encoder (PS2) |
| `MPeg` | `Unreal/Engine/MPeg` | `MpegLib.lib` + thin wrappers (`scompmpg.cpp`, `TbFile.cpp`, `SoundFile.cpp`): MP2 sound-compression tooling |

No target of the CMake build links any of them.

## Config/System/ — the shipped inis

`Default.ini` and `DefUser.ini` from `Unreal/System`. The engine derives `Game.ini`/`User.ini` from them on the first
run (the same `appInit` scheme as UT99's `UnrealTournament.ini`). `web/` holds the tracked web boot set
(`Config/System/web/README.md`).

## Assets/ — game content (gitignored; see `Assets/README.md`)

`Maps`, `Textures`, `Sounds`, `Music`, `System` (without the two inis), `SystemLocalized`, `Fonts`, `Help`, `Save`
(the drop's `SAVE`), `DialogSources` (the drop's `Dialog`), and `PackageSources/<Pkg>/…` for the packages' binary
import trees. Only `Assets/README.md` is tracked; the runtime part is frozen as
`/home/ion/WorkUnreal/assets/hp2-proto-assets`.

## Not kept from the drop

`GestureComp/Release/GestureComp.exe`, `GestureDrawingTool/Release/GestureDrawingTool.exe`, `Tools/oggdrop.exe`,
`Tools/PackageTool.exe` (prebuilt tools; their sources or upstream projects exist), the VSS files (`vssver.scc`,
`mssccprj.scc`, `*.vsscc`), the MSVC scratch files (`*.aps`, `*.plg`, `*.opt`, `*.clw`) and `System/System.dsp`.
