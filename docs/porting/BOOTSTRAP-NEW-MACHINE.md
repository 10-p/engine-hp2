# Bootstrap on a new machine — both HP2 variants published side by side

This file is the hand-over for a fresh Claude Code client on another machine. Goal: rebuild the HP2 web port from the
repositories and publish **both** variants side by side as two separate frontend TARGETS (since 2026-10-03):

| Target id (= publish folder) | What | Engine checkout (branch) | Assets |
|---|---|---|---|
| `hp2-proto` | the PROTOTYPE: the 2002-09-06 snapshot, original and unfinished but "proven" | `HarryPotter2` (`main` = `prototype`) | `HarryPotter2/Assets/` |
| `hp2-retail-exp` | the RETAIL disc + the retail engine behaviour: experimental, "unproven" | `HarryPotter2-retail` (`retail`, a git worktree) | `assets-retail/` (built from the ISO) |

Both targets live on the frontend branch `hp2-retail` (`hp2-prototype` = `hp2` carries `hp2-proto` only; `hp2-retail` is
rebased on it and adds `hp2-retail-exp`; both are rebased on ufrontend `main`, the project's master). One `publish-all`
writes `publish-hp2-site/{hp2-proto,hp2-retail-exp}/`; each target has its own storage prefixes (IndexedDB
`hp2-proto-user-data` / `hp2-retail-exp-user-data`), so the two never share saves (they are not interchangeable). The
publish folder carries NO root `index.html` (the owner keeps the two versions low-profile).

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
git -C HarryPotter2 checkout main && git -C HarryPotter2 worktree add ../HarryPotter2-retail retail   # hp2-retail-exp's engine
git clone -b hp2-v433-compat https://github.com/10-p/XOpenGLDrv.git XOpenGLDrv
git clone -b hp2 https://github.com/10-p/utglr-copy.git utglr-copy
git clone -b fix/cubemap-bind-active-unit https://github.com/10-p/gl4es.git gl4es
git clone https://github.com/10-p/unreal-frontend-arch-private.git ufrontend
cd HarryPotter2 && git fetch origin && git branch -a                   # main, prototype, retail
cd ../ufrontend && git fetch origin hp2 hp2-prototype hp2-retail && git checkout hp2-retail && yarn install
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
{ "out": "<W>/publish-hp2-site", "enginesRoot": "<W>", "emsdk": "<W>/emsdk",
  "assets": { "hp2-proto": "<W>/HarryPotter2/Assets", "hp2-retail-exp": "<W>/assets-retail" } }
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

## 8. Publish both targets (one run; each target builds its OWN engine checkout and refuses a branch mismatch)

```bash
source ~/emsdk/emsdk_env.sh; cd $W
# Configure each engine's web trees OUTSIDE yarn first: inside `yarn` CMake caches yarn's temporary node shim as
# NODE_EXECUTABLE (/tmp/xfs-*/node) and the next build fails with "node: not found".
for e in HarryPotter2 HarryPotter2-retail; do (cd $e/Source && cmake --preset wasm-perf -DNODE_EXECUTABLE=$(command -v node) \
    && cmake --preset wasm -DNODE_EXECUTABLE=$(command -v node)); done
cd ufrontend && yarn publish-all --out $W/publish-hp2-site --compress zstd \
    --target targets/hp2-proto/target.json --target targets/hp2-retail-exp/target.json
rm $W/publish-hp2-site/index.html        # the portal phase always writes one; this site has none on purpose
```
Expect `hp2-proto/: 460 manifest entries` and `hp2-retail-exp/: 515 manifest entries`, `no duplicate engines`, and in
`publish-provenance.json` engine `HarryPotter2 main` / `HarryPotter2-retail retail`, `dirty: false`.

## 9. Serve them locally (detached, TLS, all interfaces)

`ufront-serve` refuses a root without `index.html`, so serve a LOCAL root that symlinks the two folders (never published):
```bash
mkdir -p $W/serve-hp2-site && cd $W/serve-hp2-site && ln -sfn ../publish-hp2-site/hp2-proto && ln -sfn ../publish-hp2-site/hp2-retail-exp
printf '<!doctype html><meta charset="utf-8"><p><a href="hp2-proto/">hp2-proto</a> · <a href="hp2-retail-exp/">hp2-retail-exp</a>\n' > index.html
cd $W/ufrontend && setsid nohup node packages/build/bin/ufront-serve.mjs --tls --host 0.0.0.0 --port 8093 --root $W/serve-hp2-site > $W/serve-8093.log 2>&1 < /dev/null &
NODE_EXTRA_CA_CERTS="$(mkcert -CAROOT)/rootCA.pem" node packages/build/bin/ufront-verify-site.mjs --url https://localhost:8093 --out $W/publish-hp2-site
```
Expected: `ok 1019 … missing 2` — the two missing are the publish bookkeeping files at the root
(`.ufront-publish-root.json`, `publish-provenance.json`), which the local root does not expose.
Pages: `https://<host>:8093/hp2-proto/` and `https://<host>:8093/hp2-retail-exp/`.

## 10. What "where we were" looks like (acceptance)

- `hp2-proto`: a plain Start boots the title level (logo + "Start" + fly-through, title music); `-LOAD=0` (the
  default engine option) continues `Save/Save0.usa` when it exists. E2E (on `hp2-retail`, which carries both targets):
  `yarn build --target hp2-proto --engine-artifact $W/HarryPotter2/build-wasm-perf/SDLLaunch`, `yarn e2e --target hp2-proto`: 15/15.
- `hp2-retail-exp`: a plain Start is a NEW GAME — the Privet Drive intro (narration + subtitles); retail never shows the
  in-game title book (its Windows front-end launched `PrivetDr.unr` or `-LOAD=0`), so `LocalMap=PrivetDr.unr`; with a
  save, Start continues it. E2E: `yarn build --target hp2-retail-exp --engine-artifact
  $W/HarryPotter2-retail/build-wasm-perf/SDLLaunch` then `yarn e2e --target hp2-retail-exp`: 15/15.
- Both: WebGL 2 (XOpenGL) default renderer, UTGLR/SoftDrv/Null selectable, OpenAL audio (Ogg music, EA-XA sounds,
  lip-synced dialog), AutoFOV (Hor+ on wide windows), saves persisted in IndexedDB `hp2-proto-user-data` / `hp2-retail-exp-user-data`.
- New game over an existing save = Storage -> Clear User Data, then RELOAD the page (generic launcher gap, reported).

## 11. Gotchas we already paid for

- `yarn e2e` uses Playwright's webServer on :8080 with `reuseExistingServer`: if anything else listens there it tests
  THAT server. The hp2 spec reads `UFRONT_E2E_BASE_URL`; point it at a server of your own when :8080 is taken.
- After changing an engine web ini, clear that target's `<id>-user-data` store (a persisted Game.ini shadows Default.ini).
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
