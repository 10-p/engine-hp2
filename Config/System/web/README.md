# Config/System/web — the web boot config (build-consumed)

`Default.ini` + `DefUser.ini` here are the **web** configuration: the Emscripten link of `Source/Game`
`--preload-file`s them to `/System/` (and lists them in `LINK_DEPENDS`, so editing one relinks), and HP2's
`appInit( TEXT("Game") )` derives `/System/Game.ini` and `/System/User.ini` from them on the first boot. The
frontend then persists the derived pair in IndexedDB (`hp2-user-data`) — so after changing this folder during
bring-up, clear that store (or the site data), or the change never reaches an existing profile.

`../Default.ini` / `../DefUser.ini` are HP2's **retail** inis, used by the native run tree
(`tools/dev/mkrun-hp2.sh`); the launcher normalises their Win32 driver names at boot.

Rules for `Default.ini` (each one is load-bearing — docs/porting/analysis/web-layer-template.md section 9.2):
- **Exactly six `Paths=` lines, retail order, `../Save/*.usa` last.** `appInit` rewrites the *last* `Paths=`
  of `Game.ini` with the save-slot path on every boot, and indexes `Paths[5]` when `appUserDir()` is set.
  Never append a seventh line (music is opened by path, not through `Paths=`).
- On-disk file case (`LocalMap=startup.unr`); no `CdPath`.
- `AudioDevice=SoundDrv.NullAudioSubsystem` until the Phase-E `NOpenALDrv` links (keep it equal to the
  launcher's override); `ServerActors` absent (IpServer/UWeb do not exist in HP2); `DownloadManagers` is
  `Engine.ChannelDownload` only.
- `[NSDLDrv.NSDLClient]` carries HP2's two extra `UClient` keys (`NoFractalAnim`, `ParticleDensity`) and matches
  `UNSDLClient::StaticConstructor`'s compiled defaults.
- UT99/Windows sections (`WinDrv`, `D3DDrv`, `Galaxy`, `IpServer`, `UWeb`, `UBrowser`, the GPU-quirk list) are
  omitted.

`DefUser.ini` is HP2's retail file (player class `HGame.harry`, the spell/broom/duel bindings).

Limitations: no native Asian/Thai fonts on the web (`UViewport::CreateNativeFont` has no SDL implementation).
**Edit this folder when the web config changes — never the asset tree.**
