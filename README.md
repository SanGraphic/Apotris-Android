# Apotris Android

> **Unofficial Android port** — a passion project compilation of [Apotris](https://apotris.com) for Android, based on the latest upstream source as of **April 7, 2026**, compiled from the Linux ARM64 variant.

This is not affiliated with or endorsed by the original Apotris developer. All game code, mechanics, assets, and design belong to [akouzoukos](https://gitea.com/akouzoukos/apotris).

For a **technical map** of the current Android stack (native entry, GLES 2, touch overlay, scaling, assets), see [`parity.md`](parity.md). For **extra ABIs** (armeabi-v7a, x86, universal APK), see [`docs/android-multi-architecture-apk.md`](docs/android-multi-architecture-apk.md). For **shipped-APK reference** (JNI names, inventory), see [`docs/reimplementation/`](docs/reimplementation/).

---

## What is Apotris?

Apotris is a feature-rich block stacking game originally built for the Game Boy Advance. It has since been ported to PC, Switch, web, and now Android. It features:

- Multiple game modes: Marathon, Sprint, Ultra, Blitz, Combo, Dig, Survival, Master, Death, Zone, Tower, Zen, and more
- Full rotation systems: SRS, ARS, SDRS, BARS
- Highly customizable handling (DAS, ARR, SFR, drop protection)
- Custom skins, palettes, and color editor
- Background gradients and shader support
- Replay system
- Multiplayer (local wireless on GBA — not applicable on Android)
- Full save system — settings, high scores, replays, and customizations persist across sessions

**Audio:** The Android pipeline copies `main/apotris/assets` into the APK and mirrors it to app storage at runtime (`ApotrisAssetLoader`). Music and SFX match the PC build **when the same `.wav` / tracker files** (and layout under `assets/...`) are present in that tree. If the repo only ships shaders and small files, audio will be quiet until you add the full asset set. Details are in [`parity.md`](parity.md) §4.

---

## Android-specific features

- **Touchscreen overlay** — RadialGamePad–based GBA-style controls (`TouchOverlayManager`, `OverlayEditor`). **Hold any control for about 4 seconds** to enter customize mode and adjust HUD layout and button sizes; settings are saved per orientation
- **Splash screen** — `SplashActivity` launcher before SDL (`ApotrisActivity`)
- **External controllers** — Bluetooth / USB; `ControllerMonitor` and native logic prefer non-virtual gamepads when SDL reports multiple devices
- **Portrait and landscape** — landscape fullscreen; portrait stacked (game + controls); scaling hooks in native code align with that layout
- **OpenGL ES 2.0** — Shaders use GLSL ES 1.00 on Android; see `parity.md` §6

---

## Supported devices

| Requirement | Minimum |
|---|---|
| Android version | 5.0 (API 21, Lollipop) — `minSdk` 21 in Gradle |
| OpenGL ES | 2.0 (declared in the manifest) |
| RAM | ~128 MB free recommended |

### Which APK for which device?

| Prebuilt APK (`releases/` or nightly) | CPU ABIs included | Use when |
|---|---|---|
| **`apotris-arm64-v8a.apk`** | **arm64-v8a** only | Most **phones and tablets from ~2016 onward** (64-bit ARM). Smaller download. |
| **`apotris-universal.apk`** | **arm64-v8a**, **armeabi-v7a**, **x86**, **x86_64** | **Older 32-bit ARM** devices, **x86/x86_64 emulators**, or if the arm64-only build does not install. Larger download. |

**Typical coverage**

- **arm64-v8a** — nearly all current Android phones and many tablets (Snapdragon, MediaTek, Google Tensor, Samsung Exynos 64-bit builds, etc.).
- **armeabi-v7a** — legacy 32-bit ARM phones and some low-end tablets still on 32-bit userspace.
- **x86 / x86_64** — Android Studio emulators and rare x86-based devices.

**Build scripts:** **`build.ps1`** produces the arm64-v8a-only APK. **`build-universal.ps1`** produces the fat APK (same `minSdk`; isolated `build-android-universal/` tree). Details: [`docs/android-multi-architecture-apk.md`](docs/android-multi-architecture-apk.md).

**Testing:** Smoke-tested on real hardware (Android 12+) and emulators; any device meeting the table above should be compatible. Split APKs / Play **AAB** are not automated in this repo yet.

---

## Downloads
![DOWNLOADS](https://img.shields.io/github/downloads/SanGraphic/Apotris-Android/total)
Tracked, signed APKs live in the repo folder [`releases/`](https://github.com/SanGraphic/Apotris-Android/tree/main/releases) (e.g. **`apotris-arm64-v8a.apk`**, **`apotris-universal.apk`**). The [**nightly** release](https://github.com/SanGraphic/Apotris-Android/releases/tag/nightly) can attach the same files when you run the **Publish nightly GitHub Release** workflow.

**Automated nightlies:** the [Nightly Android build](.github/workflows/nightly-android.yml) workflow runs on a schedule (UTC), uploads APKs as **workflow artifacts** (Actions → workflow → latest run → Artifacts), and also updates the **`nightly` GitHub Release** with the generated APK assets. You can also start it manually (**Run workflow**). Optional checkbox **Also build multi-ABI universal APK** runs the fat `build-universal.ps1` job (slow), and when enabled the universal APK is added to the same nightly release.
**Automated nightlies (CI artifacts):** the [Nightly Android build](.github/workflows/nightly-android.yml) workflow runs on a schedule (UTC) and uploads APKs as **workflow artifacts** (Actions → workflow → latest run → Artifacts). You can also start it manually (**Run workflow**). Optional checkbox **Also build multi-ABI universal APK** runs the fat `build-universal.ps1` job (slow).

| Variant | Description |
|---|---|
| `releases/apotris-arm64-v8a.apk` | 64-bit ARM only — most modern phones |
| `releases/apotris-universal.apk` | Multi-ABI fat APK — see **Supported devices** above |

### CI signing (optional)

If you do not add secrets, CI uses a **cached debug-style keystore** (`changeme` passwords) so APKs are installable for testing; consecutive runs reuse the same key via Actions cache. For **Play-style signing**, add repository secrets: `APOTRIS_KEYSTORE_B64` (base64 of your `.jks`), `APOTRIS_KEYSTORE_PASS`, `APOTRIS_KEY_ALIAS`, `APOTRIS_KEY_PASS`.

**If the workflow failed:** this repo vendors `main/apotris` without a nested `.git`, so CI **must not** run `git submodule` inside that folder (older workflows did and failed). **Gitea-only subprojects** (`general-tools`, **`openmpt`** / libopenmpt, **`SoLoud`**) are listed in `main/apotris/.gitmodules` but **not** committed here (no Meson wrap for those names). CI runs **`tools/ci/sync-apotris-subprojects.ps1`** to shallow-clone them before Meson. The workflow resolves **NDK 26.x** under `$ANDROID_SDK_ROOT/ndk` if the patch folder name varies. The optional universal job only runs when you use **Run workflow** and set **Also build multi-ABI universal APK** to true (boolean inputs are compared as the string `true` in GitHub’s API).

---

## Building from source

### Requirements

- Windows 10/11 with PowerShell 7+
- Android NDK 26.1 (installed via Android Studio or SDK Manager)
- Android SDK with `build-tools` 34
- Java 17 (JDK)
- Python 3 with `ninja` (`pip install ninja`)

### Quick build

```powershell
# Full build: compile C++ + build APK + sign
.\build.ps1

# Skip C++ recompile (Kotlin/Java/resources only)
.\build.ps1 -SkipNative

# Build + install to connected device
.\build.ps1 -Install
```

The signed APK is written to the **repository root** as:

`apotris-arm64-v8a-YYYY-MM-DD.apk`

(date from the day you run the script). Unsigned Gradle output lives under `main/apotris/build-android/project/app/build/outputs/apk/release/` (gitignored).

### Universal (multi-ABI) build — separate script

**`build-universal.ps1`** builds up to four ABIs into one fat APK using **`main/apotris/build-android-universal/`** and **`main/apotris/meson/universal-generated/`** only. It does **not** change `build.ps1` or `main/apotris/build-android/`.

```powershell
.\build-universal.ps1
.\build-universal.ps1 -Abis arm64-v8a,armeabi-v7a
```

Output: **`apotris-universal-YYYY-MM-DD.apk`**. Details: [`docs/android-multi-architecture-apk.md`](docs/android-multi-architecture-apk.md) §0.

### Keystore / signing

On first run, `build.ps1` can auto-generate `apotris-release.jks` with default credentials (gitignored). For a real release, set these environment variables before running:

```powershell
$env:APOTRIS_KEYSTORE      = "path/to/your.jks"
$env:APOTRIS_KEYSTORE_PASS = "yourpassword"
$env:APOTRIS_KEY_ALIAS     = "youralias"
$env:APOTRIS_KEY_PASS      = "yourkeypassword"
```

---

## Project structure

| Path | Role |
|------|------|
| **`android-port/`** | **Source of truth** for the Android app layer: manifest, Java/Kotlin, patched SDL Java (`SDLActivity` / `SDLSurface`), resources, launcher drawables. Copied into the Gradle tree on every `build.ps1` run. |
| **`android-port/java/com/apotris/android/`** | `ApotrisActivity` (loads **`libmain.so` only** — static SDL + game), `SplashActivity`, `ApotrisAssetLoader` (APK assets → internal `game_assets/`) |
| **`android-port/kotlin/com/apotris/android/`** | `TouchOverlayManager`, `OverlayEditor`, `ControllerMonitor` (RadialGamePad, preferences) |
| **`main/apotris/`** | Upstream Apotris tree (Meson project, C++ sources, `assets/`) |
| **`main/apotris/build-android/`** | Meson output (`libmain.so`, object files) and **generated** SDL + Gradle project under `build-android/project/` |
| **`build.ps1`** | Meson cross-compile → copy `libmain.so` (+ `libc++_shared.so` when found) to `jniLibs/arm64-v8a` → overlay `android-port` → `assembleRelease` → Uber APK Signer |

**Native packaging:** Meson produces a single shared library (`shared_library('main', …)`). There is no separate `libSDL2.so` in the APK; `ApotrisActivity` overrides `getLibraries()` to return only `"main"`.

**Icons:** `build.ps1` runs `Sync-ApotrisLauncherIcons` — prefers repo-root `newlogo.png`, then `gameicon.png`, then `apotris.png` for `mipmap-*`. Foreground drawable for adaptive icons is under `android-port/res/` (see `parity.md` §1).

---

## Cleaning the repository

Most build artifacts are **gitignored** (see [`.gitignore`](.gitignore)). They can still accumulate on disk and slow searches or confuse diffs. Safe cleanup:

1. **Gradle / APK intermediates** (keeps Meson `libmain.so` build if you only want to drop Java rebuild cruft):

   ```powershell
   Remove-Item -Recurse -Force `
     main\apotris\build-android\project\.gradle,
     main\apotris\build-android\project\build,
     main\apotris\build-android\project\app\build,
     main\apotris\build-android\project\.cxx -ErrorAction SilentlyContinue
   ```

2. **Full native + Android project regeneration** — removes Meson build dir and the generated Gradle tree; the next `.\build.ps1` re-bootstraps from SDL’s `android-project` and reapplies `android-port/`:

   ```powershell
   Remove-Item -Recurse -Force main\apotris\build-android -ErrorAction SilentlyContinue
   ```

3. **Root outputs** (optional): delete dated `apotris-arm64-v8a-*.apk`, `uber-apk-signer.jar`, `signed_output/`, and any local `*.jks` you do not need. These are ignored by git but clutter the checkout.

Do **not** delete `android-port/` or `main/apotris/` sources. Older exploratory notes under `main/implementation_plan.md` (Lemuroid/Compose) are **superseded** by `docs/reimplementation/` and `parity.md`; they are not part of the active build.

---

## Credits

- **Apotris** by [akouzoukos](https://apotris.com) — original game, mechanics, assets, and design
- **SDL2** — Android windowing and input layer
- **RadialGamePad** by [Swordfish90](https://github.com/Swordfish90/RadialGamePad) — virtual gamepad library
- **[Lemuroid](https://github.com/Swordfish90/Lemuroid)** — inspiration for GBA-style on-screen touch control layout and polish (this port does not bundle Lemuroid)
- **Gemini CLI** (Google) and **Claude Opus 4.6** (Anthropic) — used as development assistants during parts of this port (code generation, debugging, and documentation)

The maintainer of this repository **does not claim personal credit** for the game, engine, libraries, or upstream design; those belong to the authors above. The Android packaging here is an unofficial fan effort built on those foundations and tools.

---

## Disclaimer

This is an unofficial port. No warranty is provided. The original game is available at [apotris.com](https://apotris.com). Please support the original developer.
