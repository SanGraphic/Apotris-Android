# Reimplementation task checklist

Use this as a project board. Check boxes in your tracker or PR descriptions as you complete work.

**Convention:** `P0` = blocker, `P1` = feature complete, `P2` = polish.

---

## Phase 0 — Baseline and tooling

- [ ] **T0.1** Document exact AGP / Kotlin / Compose versions from shipped APK (or pick compatible set with compileSdk 34). *Accept: versions listed in `INVENTORY-decompiled.md` or here.*
- [ ] **T0.2** Ensure `tools/decompile/jadx-phone` and `apktool-phone` are up to date for diff reference. *Accept: `Decompile-PhoneApk.ps1` runs clean.*
- [ ] **T0.3** Extract JNI method signatures from shipped `SDLActivity` and list expected native symbol names. *Accept: table in repo (append to INVENTORY or separate `JNI.md`).*

---

## Phase 1 — Native JNI bridge

- [ ] **T1.1** Implement `Java_org_libsdl_app_SDLActivity_nativeVirtualButtonEvent__IZ` (adjust for JNI export style your build uses). *Accept: `nm`/`objdump` shows symbol; no link error.*
- [ ] **T1.2** Map button ids `0–14` (see inventory) into engine input (SDL gamepad events or direct key injection into `currentKeys` / equivalent). *Accept: unit test or debug overlay shows correct action per id.*
- [ ] **T1.3** Implement `nativeSetPortraitMode(boolean)`. *Accept: game receives mode; portrait layout does not clip critical HUD (define screenshot test or manual checklist).*
- [ ] **T1.4** Audit `saving.cpp` / `GameKeys` interaction: remapped physical buttons must not break virtual ids. *Accept: document mapping; test after in-game rebind.*

---

## Phase 2 — SDL Java integration

- [ ] **T2.1** Add static `TouchOverlayManager` field and lifecycle (`onStart`/`onStop`) to **your** SDL activity class (prefer `ApotrisActivity` subclass pattern). *Accept: manager starts/stops without leak warnings.*
- [ ] **T2.2** Port `applyOrientationLayout` + call from `onCreate` (with initial `Configuration`) and `onConfigurationChanged`. *Accept: rotation transitions without black screen or duplicate surfaces.*
- [ ] **T2.3** Patch `SDLSurface` dispatch: on first `ACTION_DOWN` on game surface, call `onScreenTouch()`. *Accept: overlay reappears when expected (see PLAN).*
- [ ] **T2.4** Patch `handleKeyEvent`: on SDL joystick `ACTION_DOWN`, call `onControllerInput()`. *Accept: overlay hides when physical pad used.*
- [ ] **T2.5** Decide persistence strategy for SDL patches: **vendor copy** under `android-port/java/org/libsdl/app/` copied over SDL template in `build.ps1`, or **gradle source sets**. *Accept: `build.ps1` documents order of copy.*

---

## Phase 3 — Gradle / Kotlin enablement

- [ ] **T3.1** Add Kotlin Gradle plugin + stdlib to generated `app/build.gradle` (or stop overwriting blindly—use template file). *Accept: `.kt` compiles.*
- [ ] **T3.2** Add Coroutines (Android + core). *Accept: `TouchOverlayManager` scope builds.*
- [ ] **T3.3** Add dependencies required by RadialGamePad (AppCompat, material if needed, compose runtime if `GlassSurfaceKt` used). *Accept: no missing class at dex.*
- [ ] **T3.4** Set `namespace` / packages: migrate `com.example.apotris` → `com.apotris.android` consistently. *Accept: single R class package; manifest matches.*

---

## Phase 4 — Vend RadialGamePad + touch stack

- [ ] **T4.1** Import `com.swordfish.radialgamepad.library` sources or AARs into repo. *Accept: license file updated.*
- [ ] **T4.2** Import `com.swordfish.touchinput.radial` (if referenced) or trim unused Compose files. *Accept: no dead imports.*
- [ ] **T4.3** Port `TouchOverlayManager.kt` (prefer source over decompiled Java). *Accept: pads attach; events reach JNI.*
- [ ] **T4.4** Restore default `RadialGamePadConfig` (left cross + L/SELECT, right A/B + R/START) matching shipped layout. *Accept: visual parity screenshot.*

---

## Phase 5 — Controller monitor

- [ ] **T5.1** Port `ControllerMonitor.kt` with `InputDeviceListener` + gamepad source filter. *Accept: flow emits count; matches `adb shell` device list.*
- [ ] **T5.2** Subscribe `TouchOverlayManager` to count changes; `updateVisibility()` matches shipped logic. *Accept: connect/disconnect BT pad toggles overlay.*

---

## Phase 6 — Overlay editor and persistence

- [ ] **T6.1** Port `OverlayEditor.kt` with `SharedPreferences` name `overlay_layout` and keys `left_tx_`, `left_ty_`, `left_scale_`, `right_*` + `landscape`/`portrait`. *Accept: values survive process death.*
- [ ] **T6.2** Long-press (5s shipped) enters edit mode; drag + scale; save button applies `apply()`. *Accept: matches decompiled UX.*
- [ ] **T6.3** On orientation change, switch active editor instance; reload prefs. *Accept: landscape and portrait store independently.*

---

## Phase 7 — Integration testing and polish

- [ ] **T7.1** Full gameplay pass: marathon, menus, settings, pause. *Accept: no ANR; input latency acceptable.*
- [ ] **T7.2** Multi-window / picture-in-picture (if supported). *Accept: no crash on resize.*
- [ ] **T7.3** Haptics: optional parity with RadialGamePad haptic config. *Accept: P2 documented.*
- [ ] **T7.4** Update root `README.md` with link to `docs/reimplementation/` and high-level status. *Accept: contributors know where plan lives.*

---

## Optional / future

- [ ] **T8.1** User setting: “Always show touch controls” when gamepad connected.
- [ ] **T8.2** Cloud backup exclusion for `overlay_layout` if privacy-sensitive.
- [ ] **T8.3** Automated UI test (Espresso) for overlay visibility.

---

## Task dependency graph (short)

`T1.*` JNI → `T4.3` TouchOverlay events  
`T2.*` SDL layout → `T4.3` attach target  
`T3.*` Gradle → `T4.*` / `T5.*` / `T6.*` Kotlin  
`T5.*` depends on `T4.3`  
`T6.*` depends on `T4.3`
