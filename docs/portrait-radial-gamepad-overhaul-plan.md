# Portrait radial gamepad: visual overhaul and customization plan

This document is scoped to **portrait mode only** (the bottom half of the stacked layout in `SDLActivity.applyOrientationLayout`: `controlsContainer` is the lower `LinearLayout` child with weight `1`).

## 1. Current implementation (baseline)

| Piece | Role |
|--------|------|
| [`TouchOverlayManager.kt`](../android-port/kotlin/com/apotris/android/TouchOverlayManager.kt) | Builds two [`RadialGamePad`](https://github.com/Swordfish90/RadialGamePad) views (left: cross + L/SELECT, right: A/B + R/START), `8f` dp margins, bottom gravity (`gravityY = 1f`), horizontal `LinearLayout` split 50/50 inside a `FrameLayout`. Does **not** set `RadialGamePadConfig.theme`; library defaults produce the flat grey look. |
| [`OverlayEditor.kt`](../android-port/kotlin/com/apotris/android/OverlayEditor.kt) | **4s hold** on any virtual button enters edit mode. Persists per-orientation `translation` + **per-pad** `scale` in `SharedPreferences` (`overlay_layout`, keys `left_*` / `right_*` + `portrait` / `landscape`). Edit UI: dim overlay, centered panel, **SeekBar** for size after tapping a half-screen drag zone, Save / Reset. |
| [`SDLActivity.java`](../android-port/java/org/libsdl/app/SDLActivity.java) | Portrait: vertical `LinearLayout` with game and controls each `layout_weight = 1` (roughly **half screen** each). `controlsContainer` is plain black; no styling beyond that. |

**RadialGamePad note:** `RadialGamePadConfig` supports a `theme: RadialGamePadTheme` (normal / pressed / text / background / stroke colors, stroke width). Per-dial configs can override theme where the library merges `configuration.theme ?: gamePadConfig.theme`. Runtime theme changes likely require **rebuilding** the `RadialGamePad` instances or calling into library internals—verify on the pinned dependency (`com.github.Swordfish90:RadialGamePad:2.0.0`) before assuming a live `setTheme` API.

## 2. Goals

1. **Look better** in the bottom band: color, depth, and optional alignment with Apotris’ in-game palette (purples / warm UI), without hurting readability or touch accuracy.
2. **More customization** exposed to the user, especially **inside the existing 4s-hold edit flow** (today that flow only moves pads and scales them independently).
3. **Presets + scaling** that respect **portrait** thumb reach, different phone widths, and optional **system display size / font scale** (test on small and large devices).
4. **Portrait-only** behavior and persistence so landscape can stay unchanged until explicitly ported.

## 3. Design direction (bottom half only)

### 3.1 Container (“chrome”)

- Keep game surface untouched; style **`controlsContainer`** and/or the overlay root:
  - Subtle **top edge** (1dp line or soft shadow) separating game from controls.
  - Optional **vertical gradient** or dark vignette behind pads (still high contrast for labels).
  - Respect **display cutouts / navigation bar insets** (`WindowInsetsCompat`) so pads are not drawn under gesture bars unless intentional.

### 3.2 Radial pads (library-driven)

- Define **named theme presets** implemented as `RadialGamePadTheme(...)` instances, for example:
  - **Retro grey** (near current defaults).
  - **Apotris purple** (background + strokes aligned with HUD accents from the game).
  - **High contrast** (accessibility: stronger text / stroke).
  - **Glass** (higher alpha backgrounds, lighter strokes)—tune for OLED black bottom bar.
- Optionally differentiate **left vs right** pad with mirrored hues (e.g. cool vs warm) while keeping label legibility.
- Consider **per-control** overrides if the library allows (`ButtonConfig` / `CrossConfig` theme fields in 2.0.0) for A/B vs menu buttons.

### 3.3 Edit mode (4s hold) UX

Extend the editor panel (portrait only) into a small **“control dock”**:

- **Theme**: horizontal **chip row** or dropdown listing presets; applying updates prefs and refreshes pads (see §4).
- **Global scale preset**: `Small / Medium / Large / Extra large` as discrete steps applied to **both** pads (and optionally a fine **SeekBar** for global multiplier). This avoids uneven left/right sizes unless the user re-enters per-pad adjust.
- **Opacity**: slider for idle alpha (separate from edit-mode `0.7f` preview).
- **Position reset**: keep **Reset**; add **“Reset position only”** vs **“Reset all”** if you add more prefs.
- **Hold duration** (optional advanced): rarely needed; skip unless users ask.

Visual feedback while editing:

- **Tint** or outline on the **selected** pad; show **snap guides** (optional) at bottom-center / corners for alignment.
- Short **onboarding string**: “Portrait controls — drag pads, pick a theme, then Save.”

## 4. Data model and persistence (portrait-only keys)

Add keys under the same `SharedPreferences` file or a dedicated `gamepad_prefs` file, all suffixed or namespaced for portrait, e.g.:

| Key idea | Purpose |
|----------|---------|
| `portrait_theme_id` | Enum string for preset |
| `portrait_global_scale` | Float multiplier (default `1f`) |
| `portrait_overlay_alpha` | Float `0.35–1.0` |
| Existing `left_*_portrait` / `right_*_portrait` | Keep for per-pad translation + scale |

**Migration:** On first launch after update, if only legacy keys exist, derive `portrait_global_scale` from average of left/right scale or default to `1f` and normalize per-pad scales to avoid double-scaling—document the chosen rule.

**Landscape:** Do not read portrait theme keys when `orientationKey == "landscape"` (or gate UI), so landscape behavior stays stable.

## 5. Positioning and layout math (portrait)

- **Thumb zones:** Default gravity stays **bottom-weighted**; presets can shift **translationY** upward slightly on tall phones so the cross sits above the home gesture area.
- **50/50 split:** The control band height is ~half the screen; **global scale** should clamp so the two pads’ bounding boxes stay inside the container (optional post-layout clamp using `View` bounds or max scale from `min(width/2, bandHeight)` vs dial max size).
- **Safe minimum touch targets:** Enforce a **minimum scale** from dp (e.g. never below 0.85 of default) via SeekBar limits or preset floors.
- **Density independence:** Continue using the library’s dp-based drawing; combine **global scale** with `configuration.density` or `scaleX`/`scaleY` consistently (prefer one mechanism to avoid compounding bugs).

## 6. Implementation phases

### Phase A — Theming without editor UI

- Add `RadialGamePadTheme` presets in Kotlin (colors as `Color.parseColor` or `ContextCompat.getColor`).
- Pass `theme = …` into `RadialGamePadConfig` for left/right (shared or split).
- Wire **one** default portrait preset; verify no JNI/event regressions.

### Phase B — Portrait edit panel: theme + global scale

- In `OverlayEditor`, when `orientationKey == "portrait"` (or a dedicated `PortraitOverlayEditor`):
  - Load/save `portrait_theme_id`, `portrait_global_scale`, `portrait_overlay_alpha`.
  - On change: apply theme (likely **replace** `RadialGamePad` children in `TouchOverlayManager` or add a `applyTheme()` path if you extend the library).
  - Apply global scale: multiply stored per-pad scale, or apply a single `View.scaleX/Y` on a **wrapper** `FrameLayout` around both pads to avoid desync with drag state—pick one model and document it.

### Phase C — Container polish

- Style `controlsContainer` in `applyOrientationLayout` (gradient, divider) **only for portrait** branch.
- Optional: light **haptic** alignment with `HapticConfig` if still exposed on 2.0.0.

### Phase D — QA matrix

- Small phone, large phone, **display size** Large, **gesture navigation**, **3-button nav**.
- Verify 4s hold still enters edit mode and **does not** fire game inputs spuriously (existing `editor.isEditing()` guard on events).
- Verify physical gamepad hide/show unchanged.

## 7. Risks and decisions

| Risk | Mitigation |
|------|------------|
| Theme not hot-swappable on `RadialGamePad` | Recreate pads on theme change; re-subscribe `events()` flow; re-apply `OverlayEditor` transforms from prefs. |
| Double scaling (global × per-pad × library margin) | Single source of truth: e.g. prefs store **normalized** per-pad scale; global scale applied only in `applyTo` or on wrapper. |
| Edit panel obscures pads | Use **bottom sheet** or **compact chips** + collapsible advanced section for portrait. |
| Scope creep to landscape | Gate all new UI and prefs with `portrait` / `isPortrait` checks. |

## 8. Files likely touched

- `android-port/kotlin/com/apotris/android/TouchOverlayManager.kt` — themes, optional wrapper views, portrait-only container callbacks.
- `android-port/kotlin/com/apotris/android/OverlayEditor.kt` (or new `PortraitGamepadEditor.kt`) — extended edit UI and prefs.
- `android-port/java/org/libsdl/app/SDLActivity.java` — portrait `controlsContainer` background / insets.
- Optional: `android-port/res/values/colors.xml` — centralize preset colors.

---

*This plan assumes the project continues to use **RadialGamePad 2.x** from Gradle; confirm theme constructor parameters match the exact artifact revision in your build.*
