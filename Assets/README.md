# Assets/ — a local copy of the game content (optional, never in git)

Everything in this folder except this file is gitignored, and nothing in the CMake build reads it. The engine and
the frontend run from the frozen asset trees of the workspace:

| Tree | Content | Paired with |
|---|---|---|
| `/home/ion/WorkUnreal/assets/hp2-proto-assets` | the prototype drop's runtime tree: `System Maps Textures Sounds Music Save` | this repository's `main` branch, frontend target `hp2-proto` |
| `/home/ion/WorkUnreal/assets/hp2-retail-exp-assets` | the retail disc's installed tree | the `retail` branch (`/home/ion/WorkUnreal/engine-hp2-retail`), frontend target `hp2-retail-exp` |

Each tree is a read-only image with a writable overlay on top (`/home/ion/WorkUnreal/assets/mount-assets.sh`, run
after a reboot): whatever the engine writes (inis, logs, saves) lands in the overlay, and the image stays pristine.
Each tree's own `README.md` gives its fingerprint and layout.

- The frontend reads the trees from its `.ufront/publish.local.json` (`assets.hp2-proto`, `assets.hp2-retail-exp`),
  or from `yarn build --assets <dir>`.
- `tools/dev/mkrun-hp2.sh` reads the tree from `UE_ASSETS_DIR` and falls back to this folder:
  `UE_ASSETS_DIR=/home/ion/WorkUnreal/assets/hp2-proto-assets tools/dev/mkrun-hp2.sh build-native run-hp2`.
  It symlinks the read-only folders and copies `System/` (without the retail `*.dll`/`*.exe`/`*.bat`), then the
  retail inis from `Config/System/`.

## A full local copy

A copy of the whole drop here holds the runtime tree plus the source material the frozen trees leave out:

| Folder | What |
|--------|------|
| `System/` | the runtime System dir: `*.u` packages, `*.int` localisation, `CUTSCENES/*.int`, `EditorRes/`, fonts (`*.ttf`, `SAPFont.*`), splash bitmaps, the original `*.dll`/`*.exe`; the inis are in `Config/System/` |
| `Maps/` | `*.unr` levels (with the `Oldfiles/`, `Studies/` and `templates/` dev maps) |
| `Textures/` `Sounds/` `Music/` | `*.utx`, `*.uax`, `*.ogg` |
| `Save/` | the save folder |
| `SystemLocalized/` | per-language `.det/.est/.frt/.itt` map localisation |
| `Fonts/` `Help/` | TrueType sources, help bitmaps |
| `DialogSources/` | the dialog database (`.mdb`) the BuildLocalDialog tool reads |
| `PackageSources/<Pkg>/` | `Models/` (`.psk/.psa/.3d`), `Textures/` (`.bmp/.pcx`), `Patterns/`, `SOUNDS/`: what the `#exec` lines in `Source/<Pkg>/Classes/*.uc` import |

To re-run `ucc make` on a content package, symlink the matching `PackageSources/<Pkg>/*` folders beside
`Source/<Pkg>/Classes/` (the `#exec … FILE=Textures\x.pcx` paths are relative to the package folder). The shipped
`System/*.u` are the packages the engine loads; the CMake build rebuilds none of them.
