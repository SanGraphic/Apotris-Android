# Decompiled APK inventory (reference)

Derived from `tools/decompile/jadx-phone/sources/` and `apktool-phone/` unless noted.

## App package (`com.example.apotris` → use `com.apotris.android` in repo)

| Class | Role |
|-------|------|
| `SplashActivity` | AndroidX splash theme, animated asset, then starts SDL game activity |
| `TouchOverlayManager` | Builds left/right `RadialGamePad`, subscribes to events, calls JNI `nativeVirtualButtonEvent`, toggles overlay vs controller |
| `ControllerMonitor` | `InputManager.InputDeviceListener`; counts devices with `SOURCE_GAMEPAD` / `SOURCE_JOYSTICK`; exposes `StateFlow<Int>` |
| `OverlayEditor` | Long-press edit mode, drag/scale pads, **SharedPreferences** persistence per orientation |

## Third-party / embedded UI (`com.swordfish.*`)

Shipped APK contains **RadialGamePad** and touch layout helpers (e.g. `com.swordfish.radialgamepad.library.*`, `com.swordfish.touchinput.radial.*`). These are **not** in git today; they must be **vendored** (source or AAR) or **replaced** with an equivalent overlay.

## Patched SDL Java (`org.libsdl.app`)

Compared to stock SDL 2.28.x `android-project`, the shipped `SDLActivity` / `SDLSurface` include:

| Change | Where (decompiled) |
|--------|-------------------|
| `TouchOverlayManager mTouchOverlayManager` | `SDLActivity` static field, constructed in `onCreate` |
| `nativeVirtualButtonEvent(int id, boolean pressed)` | `SDLActivity` JNI declaration |
| `nativeSetPortraitMode(boolean portrait)` | `SDLActivity` JNI declaration |
| `applyOrientationLayout(Configuration)` | Rebuilds `mLayout`: portrait = vertical `LinearLayout` (game half + controls half), landscape = fullscreen surface + overlay on root |
| `onConfigurationChanged` | `detach()` overlay, `applyOrientationLayout` |
| `onStart` / `onStop` | `TouchOverlayManager.start()` / `stop()` |
| Controller key down | `handleKeyEvent`: if SDL joystick device, `onControllerInput()` on `ACTION_DOWN` |
| Screen touch | `SDLSurface`: on touch `ACTION_DOWN`, `mTouchOverlayManager.onScreenTouch()` to re-show overlay after touch-first UX |

**Repo status:** stock SDL template from `build.ps1` bootstrap **does not** include these hooks or natives.

## JNI / native (not in repo C++ today)

The shipped app expects **libmain** (or SDL glue) to export JNI implementations registered for `org/libsdl/app/SDLActivity`:

- `nativeVirtualButtonEvent` — map synthetic button ids to the same input path as physical gamepad buttons
- `nativeSetPortraitMode` — inform the game/renderer/layout logic that Android is in stacked portrait mode (two-band UI)

**Action:** locate symbols in `libmain.so` from the phone APK (`nm`, Ghidra, or `javap -s` + `readelf`) or re-implement by mirroring shipped behavior and testing.

## Virtual button IDs (`TouchOverlayManager`)

Aligned with **SDL gamepad-style** indices used in `handleEvent` (A is read from field `BTN_A`; constructor does not always show an explicit `iput` in smali—runtime default for `int` is **0**, consistent with `SDL_CONTROLLER_BUTTON_A`):

| Constant / usage | Value | Notes |
|------------------|------:|--------|
| A | `0` | Primary face button |
| B | `1` | |
| SELECT | `4` | |
| START | `6` | |
| L | `9` | Left shoulder |
| R | `10` | Right shoulder |
| D-pad UP / DOWN / LEFT / RIGHT | `11`–`14` | Direction events clear all four then set edges per axis |

## Overlay layout persistence (`OverlayEditor`)

- **Preferences file:** `overlay_layout` (`Context.getSharedPreferences("overlay_layout", MODE_PRIVATE)`)
- **Orientation suffix:** constructor takes `"landscape"` or `"portrait"`
- **Keys** (string + orientationKey), floats:

| Key prefix | Meaning |
|------------|---------|
| `left_tx_` | Left pad translation X (px) |
| `left_ty_` | Left pad translation Y |
| `left_scale_` | Left pad scale (default 1.0) |
| `right_tx_` | Right pad |
| `right_ty_` | |
| `right_scale_` | |

Examples: `left_tx_landscape`, `right_scale_portrait`.

**Not the same as** Apotris **game** saves in `main/apotris/source/saving.cpp` (settings, key bindings, profiles). Overlay prefs are **Android-only UI state**.

## Game save / key bindings (C++)

`saving.cpp` packs `GameKeys` and controller hints into the desktop save format. Touch overlay buttons must ultimately feed the **same logical actions** the game expects (rotate, drop, hold, etc.) via whatever mapping the JNI layer applies—either by emitting SDL controller events or by writing into the engine’s key state.

## Dependencies visible in APK metadata

Compose, AppCompat, AndroidX Core/SplashScreen, Coroutines, **RadialGamePad** stack. Exact Gradle coordinates should be recovered from the original Kotlin project or POM files inside the APK if present; otherwise match versions compatible with AGP **8.1.x** / compileSdk **34** used in `build.ps1`.
