# Portrait radial gamepad — implementation plan

Actionable sequence to execute the work described in [portrait-radial-gamepad-overhaul-plan.md](./portrait-radial-gamepad-overhaul-plan.md). **Portrait only** unless noted.

---

## 0. Prerequisite: verify the library API

**Owner:** first task before coding themes.

1. Open the resolved **RadialGamePad 2.0.0** sources (Gradle cache or clone tag matching the artifact).
2. Confirm:
   - `RadialGamePadConfig` constructor includes `theme: RadialGamePadTheme`.
   - `CrossConfig` / `ButtonConfig` (or equivalents) accept optional `theme` overrides.
   - Whether `RadialGamePad` exposes any **public** theme update; if not, plan on **recreating** views when the user picks a new theme in edit mode.

**Acceptance:** Short comment in code or this doc stating “theme applied at construction only” or the actual API name if hot-swap exists.

---

## 1. Milestone M1 — Theme presets and wiring (no edit UI yet)

**Goal:** Pads look intentionally styled in portrait using library themes; behavior unchanged.

| Step | Task |
|------|------|
| 1.1 | Add `android-port/res/values/colors.xml` entries for each preset (normal, pressed, text, backgrounds, strokes). Name them `gamepad_theme_*` to avoid clashes. |
| 1.2 | Add `GamepadThemePresets.kt` (or inside a small `gamepad` package): `enum` or sealed class `PortraitGamepadThemeId` + `fun toRadialTheme(context): RadialGamePadTheme` for **Retro**, **Apotris**, **HighContrast**, **Glass**. |
| 1.3 | In `TouchOverlayManager`, build `leftConfig` / `rightConfig` using `theme = preset.toRadialTheme(activity)` when `isPortrait`; landscape keeps **current** config (no theme or Retro only). |
| 1.4 | Optionally pass a **slightly different** theme for right pad (warm vs cool) using two `RadialGamePadTheme` instances; keep WCAG-style contrast on text. |

**Acceptance:**

- Portrait: visible difference from default grey; landscape: unchanged layout and look.
- Virtual buttons still map to the same SDL ids; smoke-test in-game.

---

## 2. Milestone M2 — Persistence for portrait appearance

**Goal:** Theme + global scale + opacity survive restarts; no double-scaling bugs.

**Decision (recommended):**

- Store **`portrait_theme_id`** (string).
- Store **`portrait_global_scale`** as float default `1f` (discrete presets map to e.g. `0.85`, `1.0`, `1.15`, `1.3`).
- Store **`portrait_overlay_alpha`** float default `1f`.
- Keep existing **`left_scale_portrait`**, **`right_scale_portrait`**, translations as **relative** multipliers the user already has.

**Apply order in `OverlayEditor.applyTo` (portrait):**

`effectiveScale = portrait_global_scale * padState.scale` (clamp min/max).

**Migration (one-time in `load()`):**

- If `portrait_global_scale` absent: set to `1f`, leave per-pad scales as-is (no normalization) **or** document a one-time average split—pick **1f + unchanged** for lowest risk.

| Step | Task |
|------|------|
| 2.1 | Extend `OverlayEditor.load()` / `save()` to read/write portrait-only keys when `orientationKey == "portrait"`. |
| 2.2 | Add defaults and clamp ranges (e.g. global scale `[0.75f, 1.4f]`, alpha `[0.4f, 1f]`). |
| 2.3 | Apply alpha on overlay root or both `RadialGamePad` views in `TouchOverlayManager` after `attach` (not in edit mode preview unless specified). |

**Acceptance:**

- Kill app → reopen → portrait theme and scale/alpha restored.
- Landscape prefs keys untouched; landscape session unaffected.

---

## 3. Milestone M3 — Edit mode UI (portrait only)

**Goal:** Same 4s hold; expanded dock for theme, global size, opacity.

| Step | Task |
|------|------|
| 3.1 | In `enterEditMode`, branch UI: **landscape** = current minimal panel; **portrait** = extended panel (ScrollView if needed). |
| 3.2 | **Theme:** `RadioGroup`, `Material` chips, or spinner bound to `PortraitGamepadThemeId`. On selection change: call back into `TouchOverlayManager` to **rebuild pads** with new config + re-`attach` event flows + `editor.applyTo`. |
| 3.3 | **Global scale:** four preset buttons + optional SeekBar; update `portrait_global_scale` in memory and refresh `applyTo` immediately. |
| 3.4 | **Opacity:** SeekBar → `portrait_overlay_alpha` + set `View.alpha` on pad container or pads. |
| 3.5 | **Reset split:** `Reset all` (existing + new prefs to defaults); `Reset position` (tx/ty only); optional `Reset size` (scales only). |
| 3.6 | Copy: one line hint (“Pick theme and size, drag pads, Save”). |

**TouchOverlayManager contract:**

- Add something like `fun applyPortraitAppearance(themeId, onReady: () -> Unit)` that:
  - Removes old `RadialGamePad` instances from containers (or replaces row).
  - Instantiates new pads with correct `RadialGamePadConfig` + theme.
  - Re-merges `left.events()` / `right.events()` into the same coroutine scope (cancel previous job if any).
  - Calls `currentEditor?.applyTo(left, right)`.

**Acceptance:**

- Changing theme in edit mode updates visuals before Save; Save persists.
- No duplicate events / crashes when switching theme repeatedly.
- Exiting edit mode restores full alpha on pads (except user-chosen idle alpha < 1).

---

## 4. Milestone M4 — Container chrome and insets

**Goal:** Bottom band matches product quality; safe on notched / gesture devices.

| Step | Task |
|------|------|
| 4.1 | In `SDLActivity.applyOrientationLayout`, for portrait only: set `controlsContainer` background to a `GradientDrawable` or theme attribute; add a 1dp top divider view or `outline`. |
| 4.2 | Apply `WindowInsetsCompat` to `controlsContainer` or overlay root: `paddingBottom` = `navigationBars()` inset (and optional `displayCutout` if pads sit too low). |
| 4.3 | If padding reduces drawable area, verify `OverlayEditor` drag bounds still feel correct (pads may need default `translationY` tweak—optional follow-up). |

**Acceptance:**

- No overlap with gesture bar on a gesture-nav device (Pixel-like).
- No regression in landscape layout.

---

## 5. Milestone M5 — QA and polish

| Check | |
|-------|---|
| Small + large width phones; display size “Large” in system settings. |
| 4s hold: no accidental `ACTION_UP` game events while entering edit (existing guards). |
| Physical controller connect/disconnect still hides/shows overlay. |
| Orientation change: portrait prefs apply; landscape prefs isolated. |
| ProGuard/R8: no shrink issues on new enum names (keep if needed). |

---

## 6. Suggested implementation order (summary)

1. **M0** — Library API check.  
2. **M1** — Colors + `RadialGamePadTheme` + portrait branch in `TouchOverlayManager`.  
3. **M2** — Prefs + apply order + alpha.  
4. **M3** — Edit UI + pad rebuild callback.  
5. **M4** — `SDLActivity` chrome + insets.  
6. **M5** — QA pass.

---

## 7. Files checklist

| File | Changes |
|------|---------|
| `android-port/res/values/colors.xml` | New palette entries. |
| New: `GamepadThemePresets.kt` (or similar) | Presets → `RadialGamePadTheme`. |
| `TouchOverlayManager.kt` | Portrait theme configs, alpha, rebuild API, coroutine subscription lifecycle. |
| `OverlayEditor.kt` | Portrait prefs, extended panel, callbacks to manager. |
| `SDLActivity.java` | Portrait container background, divider, insets listener. |

---

## 8. Out of scope (defer explicitly)

- Landscape theme editor parity.  
- Custom per-button SVGs / icons (library may support labels only).  
- Cloud backup of layout.  
- Changing the 50/50 split ratio (game vs controls) — separate product decision.

---

*Reference design rationale: [portrait-radial-gamepad-overhaul-plan.md](./portrait-radial-gamepad-overhaul-plan.md).*
