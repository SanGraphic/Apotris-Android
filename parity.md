# Apotris Android — parity write-up

This document summarizes what was implemented to bring the **Meson-built `libmain.so` + Gradle APK** in this repository closer to the **shipped / decompiled** `com.example.apotris`–style Android build: single native library, touch overlay, portrait layout hooks, filesystem-backed audio, GLES shaders, and orientation-specific scaling.

A separate reference for “what shipped” lives under [`docs/reimplementation/`](docs/reimplementation/) (inventory, JNI names, tasks). This file focuses on **what the current repo does** and how it maps to that target.

For **adding more CPU ABIs** (armeabi-v7a, x86, universal APK), see [`docs/android-multi-architecture-apk.md`](docs/android-multi-architecture-apk.md).

---

## 1. Build and packaging

- **`build.ps1`** drives the full pipeline: Meson cross-compile for `aarch64-linux-android` → `libmain.so` copied to **`app/src/main/jniLibs/arm64-v8a/`** → Gradle `assembleRelease` → Uber APK Signer → optional `adb install -r`.
- **`Ensure-AndroidGradleProject`** bootstraps from SDL’s `android-project` template, then **`Apply-ApotrisAndroidPort`** overlays the long-lived **`android-port/`** tree (manifest, Java, Kotlin, resources) into the generated app `src/main`.
- **`Sync-ApotrisGameAssets`** copies `main/apotris/assets` into **`app/src/main/assets/apotris_game_assets/assets/`** so bundled files (shaders, and any `.wav`/music you add) ship inside the APK.
- **`Sync-ApotrisLauncherIcons`** overwrites SDL **`mipmap-*`** PNGs from repo-root **`newlogo.png`**, then **`gameicon.png`**, then **`apotris.png`** (first found wins).
- **Launcher foreground (what the home screen uses):** **`android-port/res/drawable-nodpi/newlogo.png`** (or the icon you copy there) plus **`ic_launcher_foreground.xml`** referencing **`@drawable/newlogo`**; **`ic_launcher.xml` / `ic_launcher_round.xml`** layer a background color under that drawable. Manifest uses **`@drawable/ic_launcher`** / **`@drawable/ic_launcher_round`**.

---

## 2. Native entry and SDL linkage

- **Problem:** SDL’s Android Java glue looks up **`SDL_main`** via `dlsym`. The desktop game only exposed `main()`.
- **Fix:** In `main/apotris/source/main.cpp`, under `#ifdef ANDROID`, an **`extern "C" int SDL_main(int argc, char *argv[])`** wrapper calls `main()` and returns `0`.
- **`ApotrisActivity`** overrides **`getLibraries()`** to load **`"main"`** only (game + SDL statically linked into one `.so`, matching the working single-library model).

---

## 3. Android shell (`android-port/`)

- **Manifest:** `com.apotris.android`, `SplashActivity` launcher, `ApotrisActivity` for SDL, `fullSensor`, USB intent filter, **`uses-feature android:glEsVersion="0x00020000"`** (OpenGL ES 2.0).
- **Patched SDL Java:** `SDLActivity` / `SDLSurface` include **`TouchOverlayManager`**, **`nativeVirtualButtonEvent`**, **`nativeSetPortraitMode`**, and layout behavior aligned with the decompiled app (portrait stacked vs landscape fullscreen surface) per [`INVENTORY-decompiled.md`](docs/reimplementation/INVENTORY-decompiled.md).
- **Kotlin:** `TouchOverlayManager`, `ControllerMonitor`, `OverlayEditor` — RadialGamePad-based overlay, gamepad device counting, overlay layout persistence (`SharedPreferences` keys documented in the inventory).
- **C++ JNI:** `main/apotris/source/liba_android_jni.cpp` exports the natives expected by that Java/Kotlin layer; **`liba_window.cpp`** implements **`androidVirtualButtonEvent`**, **`androidSetPortraitMode`**, **`androidConsumeLayoutRefresh`**, **`androidPortraitStackedLayout()`** (portrait flag used by the Java side for layout).

---

## 4. Game assets, working directory, and audio

- **Problem:** SoLoud and shader loading use **`fopen` / `opendir`** on real paths. APK **`assets/`** are not a normal filesystem path.
- **Fix:**
  - **`ApotrisAssetLoader`** (`android-port/java/.../ApotrisAssetLoader.java`) runs in **`ApotrisActivity.onCreate` before `super.onCreate()`**, recursively copying **`apotris_game_assets`** into **`getFilesDir()/game_assets/`**, with a **version stamp** (`.apotris_assets_version` / `ASSET_PACK_VERSION`) to force refresh when packaging changes.
  - Native code **`chdir()`** to **`SDL_AndroidGetInternalStoragePath()/game_assets`** in **`windowInit()`** so paths like **`assets/...`** resolve.
  - **`SDL_INIT_AUDIO`** is included in **`SDL_Init`** on Android so the audio subsystem is initialized for the SoLoud/SDL stack.
- **Note:** The repo’s `main/apotris/assets` may only contain shaders and small files until you add the same **`.wav` / tracker music** the PC build uses; without those files, music/SFX will still be missing even though the pipeline is correct.

---

## 5. Video scaling and fullscreen (Android parity behavior)

Scaling uses **`windowScale`** and **`rowStart` / `rowEnd`** in `liba_window.cpp` (same model as desktop: destination rect from `512 * windowScale`, row band from `screenHeight / windowScale`).

**When the save uses auto zoom (`zoom <= -1`) and integer scaling** (default on Android for new saves):

| Orientation | Condition | `windowScale` |
|-------------|-----------|----------------|
| Landscape (`screenWidth >= screenHeight`) | — | **5** |
| Portrait | `GameScene` **and** `!paused` (live play) | **6** |
| Portrait | Menus, title, settings, **pause menu** (`paused` or not `GameScene`) | **4** |

**Refresh triggers (so pause/menu vs play actually updates scale):**

- **`refreshWindowSize()`** after **`scene->init()`** in **`menuSystem.cpp`** `changeScene` (`#ifdef ANDROID`).
- At the start of **`updateWindow()`** on Android: refresh on **portrait ↔ landscape** change, and when the **portrait “active play”** flag (`GameScene` + `!paused`) **toggles** (cheap state compare; avoids editing every `paused = …` site).

**Defaults for new saves (`saving.cpp`):**

- **`fullscreen = true`** on Android.
- **`integerScale = true`** (same as desktop default; matches “integer on” for the table above).
- **Aspect ratio** defaults to **4:3** (`aspectRatio = 1`; N3DS path uses **`savedAspectRatio = 1`** as well). Existing save files keep their stored value until reset or graphics reset.

Users with **old saves** that override zoom or turn integer scale off will **not** get the fixed 4/5/6 table until they align those video settings or reset save data.

---

## 6. OpenGL ES 2.0 and shaders

The game targets **OpenGL ES 2.0** end-to-end on Android so behavior matches **`uses-feature glEsVersion 0x00020000`**, improves support on older GPUs, and avoids GLES3-only features (e.g. core VAOs, `GL_UNSIGNED_INT` elements without extension).

**Earlier problems addressed:**

- Android builds with **`PC` + `ANDROID` + `GL`**: the wrong Glad path (**desktop `gl.c` / `gladLoadGL`**) was used against an **EGL GLES** context — fixed by building **`gles2.c`** and **`gladLoadGLES2`** on Android.
- **`glShaderSource`** was incorrectly passed **`&std::string`** cast to `const char**`; drivers could crash or miscompile. It now passes **`str.c_str()`** via a **`const char*`** pointer.
- **CRT-Pi / zfast LCD** (and similar) showed only the **clear color** (teal `glClearColor`) when GLES 3 + `in`/`out` paths disagreed with drivers; with **GLES 2 + GLSL ES 1.00**, the RetroArch-style **`__VERSION__ < 130`** branches (**`attribute` / `varying` / `texture2D` / `gl_FragColor`**) are used consistently.

**Current Android GLES / shader pipeline:**

- **`gl.c`:** compiled only when **`defined(PC) && !defined(ANDROID)`**.
- **`gles2.c`:** compiled when **`PORTMASTER || ANDROID`**; **`gladLoadGLES2`** after creating the shader GL context.
- **`liba_window.h`:** **`glad/gles2.h`** when **`ANDROID && GL`**, else **`glad/gl.h`**.
- **`liba_window.cpp` `windowInit`:** **`SDL_GL_CONTEXT_PROFILE_ES`** with **`MAJOR 2` / `MINOR 0`** (OpenGL ES 2.0).
- **`shader.cpp` (Android):**
  - Shader preamble **`#version 100`** (GLSL ES 1.00) so **`__VERSION__ == 100`** and bundled `.glsl` files use the legacy compatibility path.
  - **`translateLine`:** optional **`mediump`** on the first **`uniform vec2`** per line on Android (unchanged idea).
  - **No VAOs:** `glGenVertexArrays` / `BindVertexArray` are **not** used on Android; vertex state is rebound in **`drawWithShaders`** with cached attrib locations.
  - **Index buffer:** **`uint16_t`** indices and **`GL_UNSIGNED_SHORT`** (`GLES 2` does not require **`GL_UNSIGNED_INT`** without **`OES_element_index_uint`**).
- **PortMaster** (non-Android) still uses **`#version 300 es`** and the prior VAO + `GL_UNSIGNED_INT` path where applicable.
- **`SDL_GL_MakeCurrent`**, **`shaderHostWindow`**, **`refreshShaderResolution`** only when **`shaderStatus == OK`**; **`glBindFramebuffer(0)`** / **`GL_UNPACK_ALIGNMENT`** on Android before texture upload.
- **`liba_window.cpp`:** After **`initShaders`** succeeds, **`androidReleaseRendererForGLES()`** destroys the **SDL texture + renderer**; **`shaderDeinit` / `freeShaders`** recreates the renderer when shaders are off. **`freeShaders`** deletes GPU objects while the context is current, then deletes the context.

---

## 7. Input: physical gamepads vs SDL “virtual” joysticks

After **touch → rotation → plugging a real pad**, SDL can report **multiple joysticks** (e.g. Android virtual / uinput-style devices at **index 0**). The old logic used **`SDL_GameControllerOpen(0)`** and a hotplug branch that almost never reopened the right device.

**Fixes in `liba_window.cpp` (Android):**

- **`openPreferredGameController()`** — opens the first **game controller** that does **not** look virtual (name checks: `Virtual`, `virtual`, `uinput`), then falls back to any game controller.
- **`SDL_CONTROLLERDEVICEADDED` / `SDL_CONTROLLERDEVICEREMOVED`** in **`handleInput`** — attach to the correct device by index / instance id; rescan with **`openPreferredGameController()`** after removal.
- **Hotplug fallback** at the end of **`updateWindow`** — if joystick count changes, detach, or **virtual-only `controller` while a real pad exists**, close and **`openPreferredGameController()`** again.

---

## 8. Other fixes and hygiene

- **`ApotrisAssetLoader`:** missing **`java.io.OutputStream`** import (fixed) for Gradle `compileReleaseJavaWithJavac`.
- **`ControllerMonitor`:** Kotlin collection typing fix where applicable.

---

## 9. Intentional gaps / follow-ups

- **No `libmain.so` from the shipped APK** is checked into this repo; scaling numbers **4 / 5 / 6** and behavior were taken from **product intent + your prior working session**, not from a full native disassembly. To prove bit-for-bit parity you’d still **`nm` / Ghidra** the reference `.so` or diff behavior on device.
- **Adaptive icons / store assets:** launcher uses **bitmap foreground** under `drawable-nodpi`; swap assets or colors in **`android-port/res`** as needed.
- **Larger game binaries** (all SFX/music modules): must live under **`main/apotris/assets`** with the expected **`assets/...`** layout; bump **`ASSET_PACK_VERSION`** when the bundled tree changes so installs refresh internal storage.
- **Single ABI today:** Gradle **`abiFilters 'arm64-v8a'`** only; see [`docs/android-multi-architecture-apk.md`](docs/android-multi-architecture-apk.md) for a **universal** or **multi-ABI** APK.

---

## 10. Primary files touched (reference)

| Area | Paths |
|------|--------|
| Entry / SDL | `main/apotris/source/main.cpp` |
| Window, scale, Android input, gamepad logic | `main/apotris/source/liba_window.cpp`, `main/apotris/include/liba_window.h` |
| JNI | `main/apotris/source/liba_android_jni.cpp` |
| Scene transitions → scale | `main/apotris/source/menuSystem.cpp` |
| Defaults | `main/apotris/source/saving.cpp` |
| GLES / shaders | `main/apotris/source/shader.cpp`, `main/apotris/source/gl.c`, `main/apotris/source/gles2.c`, `main/apotris/assets/shaders/*.glsl` |
| PortMaster refresh guard | `main/apotris/source/liba_portmaster.cpp` |
| Android app / assets / launcher | `android-port/**`, `build.ps1` (`Sync-ApotrisGameAssets`, `Sync-ApotrisLauncherIcons`, `Apply-ApotrisAndroidPort`) |
| Meson Android | `main/apotris/meson.build` (`liba_android_jni.cpp`), `main/apotris/meson/android-arm64-auto.ini` (from `build.ps1`) |

---

*Last updated: GLES 2.0 + GLSL ES 1.00 shader path, shader source bugfix, CRT-Pi/zfast compatibility, gamepad hotplug, default 4:3, launcher icon pipeline, and pointer to multi-arch doc.*
