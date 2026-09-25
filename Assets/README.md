# Assets/ — the game content (never in git)

Everything in this folder except this file is **gitignored**. It is the retail Harry Potter and the Chamber
of Secrets (PC, Unreal Engine 1 build 433) content plus the binary import sources that the UnrealScript
packages were built from. The engine reads the runtime tree from here (the launcher's `Paths=` are the
stock relative `../Maps/*.unr` etc.), and `yarn publish-all` in the `ufrontend` clone points at it as the
`hp2` asset path.

| Folder | What | Origin in the raw drop |
|--------|------|------------------------|
| `System/` | the runtime System dir: `*.u` packages, `*.int` localisation, `CUTSCENES/*.int`, `EditorRes/`, fonts (`*.ttf`, `SAPFont.*`), splash bitmaps, the original `*.dll`/`*.exe` | `Unreal/System/` minus `Default.ini`/`DefUser.ini` (→ `Config/System/`) |
| `Maps/` | `*.unr` levels (+ `Oldfiles/`, `Studies/`, `templates/` dev maps) | `Unreal/Maps/` |
| `Textures/` `Sounds/` `Music/` | `*.utx`, `*.uax`, `*.ogg` | same names |
| `SystemLocalized/` | per-language `.det/.est/.frt/.itt` map localisation | `Unreal/SystemLocalized/` |
| `Fonts/` `Help/` `Save/` | TrueType sources, help bitmaps, the (empty) save dir | same names / `SAVE` |
| `DialogSources/` | the dialog database (`.mdb`) the BuildLocalDialog tool consumed | `Unreal/Dialog/` |
| `PackageSources/<Pkg>/` | `Models/` (`.psk/.psa/.3d`), `Textures/` (`.bmp/.pcx`), `Patterns/`, `SOUNDS/` that the `#exec` lines in `Source/<Pkg>/Classes/*.uc` import | `Unreal/<Pkg>/{Models,Textures,…}` |

To re-run `ucc make` on a content package, symlink the matching `PackageSources/<Pkg>/*` dirs back beside
`Source/<Pkg>/Classes/` (the `#exec … FILE=Textures\x.pcx` paths are relative to the package dir).

The shipped `System/*.u` are the packages the engine loads; nothing here is rebuilt by the CMake build.
