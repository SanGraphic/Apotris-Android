# Implementation Plan - Lemuroid Touchscreen Integration

**Superseded for parity work:** see the full gap analysis and numbered tasks in [docs/reimplementation/](../docs/reimplementation/README.md) (shipped APK vs current `android-port` / SDL template).

This plan outlines the steps to integrate the high-quality, glassy touchscreen UI from Lemuroid into the Apotris Android port. The UI will be implemented as a Jetpack Compose overlay on top of the SDL2 surface and will dynamically toggle based on controller connectivity.

## Proposed Changes

### Build System & Dependencies
- **[MODIFY] [build.gradle](file:///c:/Users/sanay/ApotrisAndroid/main/apotris/build-android/project/app/build.gradle)**
    - Add Kotlin Android plugin.
    - Enable Jetpack Compose and specified versions.
    - Add dependencies for `androidx.compose.ui`, `androidx.compose.material`, and `gg.padkit`.
    - Add JitPack repository for PadKit.

### UI Components Porting
- **[NEW] [GlassSurface.kt](file:///c:/Users/sanay/ApotrisAndroid/main/apotris/build-android/project/app/src/main/java/com/swordfish/touchinput/radial/ui/GlassSurface.kt)**
    - Port the glassy button rendering logic from Lemuroid.
- **[NEW] [TouchController.kt](file:///c:/Users/sanay/ApotrisAndroid/main/apotris/build-android/project/app/src/main/java/org/libsdl/app/TouchController.kt)**
    - Implement the GBA-style layout (D-pad, A, B, L, R, Start, Select) using Lemuroid's design language.

### Android Integration
- **[MODIFY] [SDLActivity.java](file:///c:/Users/sanay/ApotrisAndroid/main/apotris/build-android/project/app/src/main/java/org/libsdl/app/SDLActivity.java)**
    - Initialize a `ComposeView` overlay.
    - Add logic to show/hide the overlay in `onInputDeviceAdded` and `onInputDeviceRemoved`.
    - Implement a JNI bridge to send touch button events to the C++ engine.

### C++ Input Bridge
- **[MODIFY] [liba_window.cpp](file:///c:/Users/sanay/ApotrisAndroid/main/apotris/source/liba_window.cpp)**
    - Add a JNI method to inject virtual button presses into the `currentKeys` set, mimicking physical controller input.

## User Review Required

> [!IMPORTANT]
> **Performance Overhead**: Adding Jetpack Compose and Kotlin will slightly increase the APK size and memory usage. However, it provides the most "authentic" Lemuroid look and feel.
> 
> **PadKit Dependency**: PadKit is an external library used by Lemuroid. I will attempt to pull the compatible version from JitPack.

## Open Questions

- Should the touch controls have any haptic feedback (vibration) when pressed, similar to Lemuroid?

## Verification Plan

### Manual Verification
- Deploy to Android device.
- Success: Virtual buttons appear when no controller is connected.
- Success: Virtual buttons disappear immediately upon plugging in a USB/Bluetooth controller.
- Success: Touching virtual buttons triggers game actions (movement, rotation, etc.) with low latency.
