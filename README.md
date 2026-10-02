# Call of Duty: Black Ops Zombies for Nintendo Switch

A Switch homebrew port, version **0.5.0**. The original Android game's ARM32 code
runs directly on the CPU, without Dynarmic. No game files are included.

## Install

You need a Switch running Atmosphère, sphaira, your own **BOZ Android v1.0.11
APK**, and the game's **`blackops_etc.dz`** graphics data archive. This APK does
not contain that archive: copy it from your own installed Android game data.
Other game versions are rejected rather than patched with incompatible hooks.

1. Download `codboz.nro` from this repository's Releases.
2. Create `sd:/switch/boz-native/` and put these three files in it:

   ```text
   sd:/switch/boz-native/
     codboz.nro
     boz.apk
     blackops_etc.dz
   ```

3. In sphaira, go to **Homebrew → CoD Black Ops Zombies → Install Forwarder**.
4. Launch the new game icon from the Switch HOME menu. First launch extracts
   the APK, prepares the game image and music, builds the archive index, installs
   the ARM32 application, then starts the game. Keep the files on the SD card.

No PC extraction is needed. Allow at least **700 MB of free SD space** before
copying those files. If setup is interrupted, launch it again to finish.
Start from the game's own HOME icon; launching through the Homebrew Menu does
not run the ARM32 game. The folder name preserves compatibility with earlier
builds; the launcher filename is `codboz.nro`.

## Controls and settings

| Input | Action |
|---|---|
| Left stick / click | Move / sprint |
| Right stick / click | Look / melee |
| ZR / ZL | Fire / aim down sights |
| Y | Tap to reload; hold to buy, repair or open |
| X | Change weapon |
| A / D-pad down | Crouch / prone |
| R / L | Grenade / tactical grenade |
| D-pad up / left / right | Tactical / alternate fire / reload |
| Plus | Pause |
| Hold Minus for 2 seconds | Port settings |
| Hold Minus + Plus for 1 second | Quit |

**L + R + Minus toggles the touchscreen cursor.** Release the combo, move with
the left stick, hold A or ZR to tap/drag, and hold ZL for precise movement.
Use the same combo to return to gameplay controls. Touchscreen input also works.

In port settings, horizontal and vertical aim sensitivity each support
**50–250%**. A single **Render resolution** setting cycles through
**360p / 540p / 720p / 1080p**; restart the game to apply it.

| Render setting | Game render size | Docked output buffer | Handheld output buffer |
|---|---|---|---|
| 360p | 640 × 360 | 1280 × 720 | 1280 × 720 |
| 540p | 960 × 540 | 1280 × 720 | 1280 × 720 |
| 720p (default) | 1280 × 720 | 1280 × 720 | 1280 × 720 |
| 1080p | 1920 × 1080 | 1920 × 1080 | 1280 × 720 |

Lower presets use bilinear upscaling; 1080p in handheld downsamples to 720p.
The Switch/TV can also scale a docked 720p buffer to the system's TV resolution.
Output size is selected at startup; restart after docking or undocking when
using 1080p. If the required framebuffer support fails, the port logs the
failure and falls back to direct output. 1080p costs more GPU time; 60 FPS at
every resolution or in every scene is not guaranteed. The port does not set clocks.
The **Field of view** slider applies **60–110°** live to the first-person camera.
**Original FOV** restores the game's default. The port keeps the original ADS
transition and preserves its lens magnification relative to the chosen FOV.
Wider views can increase scene rendering work. **Show on-screen sticks** is
checked to show the sticks and unchecked to hide them at every render resolution.
This release plays with one player and one view. Split screen is under development
and is disabled in release builds, including when an older config enables its diagnostics.
Known issues being tracked: some game UI text clips at higher render resolutions,
and automatic weapons can fire unreliably when holding ZR.

## Updating and troubleshooting

Keep your APK, data, `config.txt`, and `save/` folder. Replace `codboz.nro` with
the new release. An installed forwarder already starts its cached ARM32 payload,
so replacing the NRO alone does **not** update that payload:

1. Close the game and read `sd:/switch/boz-native/title_id.txt`.
2. Remove only `sd:/atmosphere/contents/<that title ID>/exefs.nsp`.
3. Launch the same HOME icon again to install the new payload.

If an older build has no `title_id.txt`, install a new forwarder for `codboz.nro`.
Keep the old working icon until the new one is tested. If startup fails, check
the launcher's error and share `debug.log`, `crash.log` (if present), and
`startup_stage.txt` from the game folder, plus the matching release/build ID.

## Building from source

On Windows, install Git, Docker Desktop, Python 3, and devkitPro with
`switch-dev` and `switch-portlibs` (including minizip/zlib). Set `DEVKITPRO` to
your installation directory, or pass `-DevkitPro C:/devkitPro` to `build.ps1`.

```powershell
git clone --recurse-submodules <your-repository-url>
cd <your-repository-folder>
./tools/setup.ps1
./build.ps1 -Launcher
```

The result is `launcher/codboz.nro`. Toolchain, library revisions and checksums
are pinned in `dependencies.json`. The source tree contains the ARM32 port in
`source/`, APK setup in `launcher/source/`, host checks in `tests/`, and shared
platform/launcher support in the `runtime/` submodule.

Optional staging and release packaging:

```powershell
./tools/prepare_sd.ps1 -Apk C:/my-game/boz.apk -Data C:/my-game/blackops_etc.dz
./tools/package_release.ps1
```

Staging writes `dist/sdmc/switch/boz-native/` without replacing saves or settings.
Release packaging writes `release/0.5.0/BOZ-NX-0.5.0.zip`, containing the NRO,
this README and an NRO SHA-256 checksum. APKs, extracted game
assets, local cover art, build output, dependencies, logs and research documents
are ignored by Git. A fresh source checkout uses the default libnx icon; an
optional local `launcher/icon.jpg` (256 × 256) replaces it when building.

Run `./tools/check.ps1` for host ABI, cursor, input and graphics helper checks.
Unicorn is used only by PC tests (`python -m pip install -r tests/requirements.txt`).
Checks with owner-supplied game files are optional; see `tools/check.ps1` parameters.
Host checks do not replace Switch hardware testing.
The release version is stored in `VERSION`. Normal builds disable experimental
split-screen diagnostics. For development only, enable them with
`./build.ps1 -Launcher -MakeArgs SPLIT_DIAGNOSTICS=1`.
With an owner-supplied image, checks also validate the split-screen probe's game
anchors, render-hook boundary and spawn/name-registration instructions using
SDK and component fixtures. Optional `-GameImage` plus `-Snapshot` checks construct
the real loaded player prefab in a PC memory copy, with SDK services modeled.
They check construction and expose shared ownership; they do not test playable co-op.
PC socket checks use loopback TCP/UDP; they
do not establish working game co-op or mDNS discovery on Switch.

Credits: the SDK implementation comes from
[r4lix/codboz-nx](https://github.com/r4lix/codboz-nx), with
[android32](https://github.com/aks796/android32),
[libnx32](https://github.com/aks796/libnx32), and
[mesa32](https://github.com/aks796/mesa32). Third-party notices remain with their
source. The port is unofficial and is not affiliated with Activision.

You can support this and my other projects on [Ko-fi](https://ko-fi.com/kawaiibunga).
