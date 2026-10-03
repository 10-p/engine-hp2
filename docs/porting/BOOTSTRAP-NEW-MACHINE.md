# Bootstrap on a new machine — reach "prototype on :8092, retail on :8095"

This file is the hand-over for a fresh Claude Code client on another machine. Goal: rebuild the HP2 web port from the
repositories and serve **both** variants exactly as on the original box (2026-10-02/03):

| Variant | Engine branch | Frontend branch | Assets | Publish folder | Served on |
|---|---|---|---|---|---|
| prototype (the 2002-09-06 snapshot, bugs and all) | `main` (= `prototype`) | `hp2-prototype` (= `hp2`) | `HarryPotter2/Assets/` | `publish-hp2` | `https://<host>:8092/hp2/` |
| retail (the retail disc, retail engine behaviour) | `retail` | `hp2-retail` | `assets-retail/` (built from the ISO) | `publish-hp2-retail` | `https://<host>:8095/hp2/` |

## 1. What this project is (read before touching anything)

Harry Potter and the Chamber of Secrets (PC, 2002, Unreal Engine 1 build 433) ported to the web (WebAssembly +
WebGL, Emscripten) on the template of the owner's UT99 port (`engine-ut99` + the `ufrontend` launcher shell). The
engine repo is the original HP2 C++ source (proprietary — the repo is PRIVATE, keep it so) adapted to GCC/clang,
Linux, the web and MinGW. The frontend (`ufrontend`, a monorepo shared with other targets) hosts per-game
descriptors; hp2 lives in `targets/hp2/` on the `hp2-*` branches only.

Read, in this order: `HarryPotter2/CLAUDE.md` (rules), `HarryPotter2/BUILD.md` (builds), `docs/porting/PLAN.md` §12
(phase checklist), the last ~10 entries of `docs/porting/LOG.md`, `docs/porting/retail-assets.md` (why two branches,
every prototype->retail difference and the commit porting it), then on the frontend `targets/hp2/README.md`.

## 2. Rules the owner set (they were in the old machine's memory, which does not travel)

- Write only inside the workspace directory (default `~/hp2-ufrontend`). Other projects on the machine are read-only.
- Never push unless the owner asks. Never deploy/upload (Azure etc.) without an explicit go-ahead.
- Game content is never in git: `HarryPotter2/Assets/` is gitignored; the retail tree and the ISO live outside every repo.
- Commit identity: author/committer `10-p <neogcar@hotmail.com>`; conventional commits; never `git commit -a` blindly
  (another agent may have WIP); end commit messages with the Co-Authored-By line the harness asks for.
- Never patch shipped game-DATA bugs in the engine; fix only port (code) bugs, report data faults with file fingerprints.
- Don't wait on other agents' runs; just never share ports or files with them (another checkout's E2E owns :8080).
- Never `pkill -f <pattern>`: it matches and kills your own shell. Stop a server by the PID `ss -ltnp` shows for its port.
- Long-lived servers must be started detached (`setsid nohup … &`); a harness background task dies at its time limit.

## 3. Inputs that are NOT in git (the owner copies them over)

- The prototype asset tree -> `~/hp2-ufrontend/HarryPotter2/Assets/` (≈796 MiB, 3,059 files; `System/Core.u` sha256
  `e0812a19…d56e8`, 61,685 B). It came with the source drop.
- The retail disc image -> `~/hp2-ufrontend/HarryPotter2.iso` (536,709,120 B, volume `HPCOS`).
- (Optional) a mkcert root CA already trusted by the devices that will open the pages (copy `$(mkcert -CAROOT)`),
  otherwise create a new one and install it on those devices.

## 4. Toolchain (Ubuntu-like host; versions used on the original box)

gcc 15 + `gcc-multilib g++-multilib`, i386 dev libs `libsdl2-dev:i386 libopenal-dev:i386 libgl1-mesa-dev:i386`
(32-bit native oracle build), cmake ≥ 4.2, node 22 + yarn 4 via corepack, python3, zstd 1.5.x, `p7zip-full` (7z),
`xvfb`, `mkcert`, git + `gh` (authenticated as `10-p`), emsdk **5.0.7** at `~/emsdk` (`./emsdk install 5.0.7 &&
./emsdk activate 5.0.7`; then `source ~/emsdk/emsdk_env.sh; embuilder build sdl2` once). Optional: `mingw-w64`
(Windows build), `wine` (Windows smoke), Ghidra 12 + JDK 21 (only for binary analysis), Playwright's Chromium
(`cd ufrontend && yarn playwright install chromium`) for the E2E.

## 5. Clone the workspace (the CMake presets expect these SIBLING folders)

```bash
W=~/hp2-ufrontend; mkdir -p $W && cd $W
git clone https://github.com/10-p/engine-hp2.git HarryPotter2          # folder name MUST be HarryPotter2
git clone -b hp2-v433-compat https://github.com/10-p/XOpenGLDrv.git XOpenGLDrv
git clone -b hp2 https://github.com/10-p/utglr-copy.git utglr-copy
git clone -b fix/cubemap-bind-active-unit https://github.com/10-p/gl4es.git gl4es
git clone https://github.com/10-p/unreal-frontend-arch-private.git ufrontend
cd HarryPotter2 && git fetch origin && git branch -a                   # main, prototype, retail
cd ../ufrontend && git fetch origin hp2 hp2-prototype hp2-retail && yarn install
```
openal-soft comes from `https://github.com/10-p/openal-soft-web` (`ufront/web`) via FetchContent automatically.
Reference only (READ-ONLY; `CLAUDE.md` cites them at `/home/ion/engines/engine-ut99` and `/home/ion/epic-sources`):
the template engine is the private `https://github.com/10-p/engine-ut99` — clone it OUTSIDE the workspace if you need
the "why" behind a convention; nothing in the build reads it.
If the workspace is NOT `~/hp2-ufrontend`, every absolute path below (and in `.ufront/publish.local.json`) changes
accordingly; docs mention `/home/ion/hp2-ufrontend` (the original box) — read them as "the workspace".

## 6. Machine-local frontend config (gitignored)

`ufrontend/.ufront/publish.local.json`:
```json
{ "out": "<W>/publish-hp2", "assets": { "hp2": "<W>/HarryPotter2/Assets" }, "enginesRoot": "<W>", "emsdk": "<HOME>/emsdk" }
```
TLS certificate for the servers (the browser only honours COOP/COEP — needed for threads — on https or localhost):
```bash
mkdir -p ufrontend/.ufront/certs
mkcert -cert-file ufrontend/.ufront/certs/dev-cert.pem -key-file ufrontend/.ufront/certs/dev-key.pem \
       localhost 127.0.0.1 ::1 "$(hostname)" $(hostname -I)
```

## 7. Build the retail asset tree

```bash
cd $W && git clone --depth 1 https://github.com/twogood/unshield.git iso-check/unshield-src
cmake -S iso-check/unshield-src -B iso-check/unshield-src/build -DUSE_OUR_OWN_MD5=ON && cmake --build iso-check/unshield-src/build -j
HarryPotter2/tools/dev/retail/assemble-assets.sh HarryPotter2.iso iso-check assets-retail iso-check/unshield-src/build/src/unshield
```
Expect `retail tree: assets-retail (556 files, 463M)`; `System/Core.u` 61,684 B, sha256 `c480e9a6…`.

## 8. Publish both variants (one after the other — publish-all builds the CHECKED-OUT engine branch and refuses a
mismatch with the descriptor's `publish.engine.branch`: prototype descriptor says `main`, retail says `retail`)

```bash
source ~/emsdk/emsdk_env.sh; cd $W
# prototype
git -C HarryPotter2 checkout main && git -C ufrontend checkout hp2-prototype
(cd ufrontend && yarn publish-all --out $W/publish-hp2 --target targets/hp2/target.json \
    --asset-path-hp2=$W/HarryPotter2/Assets)
# retail
git -C HarryPotter2 checkout retail && git -C ufrontend checkout hp2-retail
(cd ufrontend && yarn publish-all --out $W/publish-hp2-retail --target targets/hp2/target.json \
    --asset-path-hp2=$W/assets-retail)
```
Each run builds the web engine (`build-wasm-perf`, `build-wasm` debug), assembles the app, writes zstd-compressed
assets and verifies every manifest entry (prototype: 460 files; retail: 515 files). The first run takes a while
(emscripten port builds, zstd -19 of ~0.5 GB). Check `publish-*/publish-provenance.json`: engine sha/branch as expected,
`dirty: false`.

## 9. Serve them (detached, TLS, all interfaces)

```bash
cd $W/ufrontend
setsid nohup node packages/build/bin/ufront-serve.mjs --tls --host 0.0.0.0 --port 8092 --root $W/publish-hp2 > $W/serve-8092.log 2>&1 < /dev/null &
setsid nohup node packages/build/bin/ufront-serve.mjs --tls --host 0.0.0.0 --port 8095 --root $W/publish-hp2-retail > $W/serve-8095.log 2>&1 < /dev/null &
NODE_EXTRA_CA_CERTS="$(mkcert -CAROOT)/rootCA.pem" node packages/build/bin/ufront-verify-site.mjs --url https://localhost:8092 --out $W/publish-hp2
NODE_EXTRA_CA_CERTS="$(mkcert -CAROOT)/rootCA.pem" node packages/build/bin/ufront-verify-site.mjs --url https://localhost:8095 --out $W/publish-hp2-retail
```
Expected: `ok 485 … missing 0` and `ok 540 … missing 0`. Pages: `https://<host>:8092/hp2/` and `…:8095/hp2/`.
Different ports = different browser origins on purpose: prototype and retail save files are NOT interchangeable.

## 10. What "where we were" looks like (acceptance)

- prototype (:8092): a plain Start boots the title level (logo + "Start" + fly-through, title music); `-LOAD=0` (the
  default engine option) continues `Save/Save0.usa` when it exists. E2E (`git -C ufrontend checkout hp2-prototype`,
  `yarn build --target hp2`, `yarn e2e --target hp2`): 14/14.
- retail (:8095): a plain Start is a NEW GAME — the Privet Drive intro (narration + subtitles); retail never shows the
  in-game title book (its Windows front-end launched `PrivetDr.unr` or `-LOAD=0`), so `LocalMap=PrivetDr.unr`; with a
  save, Start continues it. E2E on `hp2-retail` with `yarn build --target hp2 --assets $W/assets-retail
  --engine-artifact $W/HarryPotter2/build-wasm-perf/SDLLaunch` then `yarn e2e --target hp2`: 14/14.
- Both: WebGL 2 (XOpenGL) default renderer, UTGLR/SoftDrv/Null selectable, OpenAL audio (Ogg music, EA-XA sounds,
  lip-synced dialog), AutoFOV (Hor+ on wide windows), saves persisted in IndexedDB `hp2-user-data`.
- New game over an existing save = Storage -> Clear User Data, then RELOAD the page (generic launcher gap, reported).

## 11. Gotchas we already paid for

- `yarn e2e` uses Playwright's webServer on :8080 with `reuseExistingServer`: if anything else listens there it tests
  THAT server. The hp2 spec reads `UFRONT_E2E_BASE_URL`; point it at a server of your own when :8080 is taken.
- After changing an engine web ini, clear the `hp2-user-data` store (a persisted Game.ini shadows Default.ini).
- Wine's file lookup is case-insensitive: Windows runs prove nothing about path case; test natively or on the web.
- The 32-bit native build (`cmake --preset native`) is the oracle: run trees via `tools/dev/mkrun-hp2.sh`
  (`HP2_ASSETS_DIR=$W/assets-retail` for retail); `EXEC=<file>` runs console commands at startup.
- Retail game packages ship without script source: read their bytecode with `tools/dev/retail/ucdis.py`.

## 12. Open items at hand-over

- Retail: play-test the `Entryhall_hub` password cut scene and the ghosts (prototype data bugs; retail data fixes the
  markers/dialog) from a real save.
- Phase G (site-root publish + Azure) waits for the owner. Generic frontend gaps were reported to the ufrontend owner:
  Clear User Data does not clear the running page; the E2E reuses any server on :8080.
- The web failed-load fix (`c677b8c`, retail) could also go to `prototype` if the owner wants.
