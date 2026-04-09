# Android: per-app FPS-ish metrics and power estimates

Retail Android builds do not offer a stable, documented **“watts for package X”** syscall or Java API. What you get instead is:

- **Frame performance** from the graphics pipeline (`gfxinfo`, Perfetto SurfaceFlinger frame timeline, Android Studio).
- **Energy *models*** from the system battery service (milliamp-hours per UID over long windows), not a lab-grade wattmeter on the app alone.

This repo includes **`tools/android/android-perf-snapshot.ps1`** to capture the adb side in one go.

## Quick: script snapshot

From the repo root (with the game **in the foreground** during the wait), run:

```
.\tools\android\android-perf-snapshot.ps1 -Package com.apotris.android -WaitSeconds 90 -Perfetto
```

Outputs under `android-perf-snapshots/`:

| File | What it is |
|------|----------------|
| `*-gfxinfo.txt` | `dumpsys gfxinfo` — total frames, janky %, percentile frame times. |
| `*-batterystats.txt` | Filtered `dumpsys batterystats <package>`; includes **Estimated power use (mAh)** and a line for your UID (`u0a…`). |
| `*-meminfo.txt` | `dumpsys meminfo` for PSS. |
| `*-trace.pb` | (with `-Perfetto`) trace for [Perfetto UI](https://ui.perfetto.dev/). |

**UID line:** the script parses `userId` from `dumpsys package` and highlights the matching `UID u0aNNN:` row under “Estimated power use (mAh)”. That value is a **model estimate** over battery history, not an instantaneous watt readout.

Traces are captured with `adb shell perfetto -c - -o -` and saved on your PC. **Windows adb** often turns line feeds in the binary stream into **CRLF**, which breaks the protobuf; the script repairs that before writing the file. If you have an older broken `.pb` from before this fix, record again with the current script. If recording still fails, remove the `android.power` `data_sources` block from `tools/android/perfetto-game.textproto` (power rails vary by device).

## Better FPS: Studio Profiler / Macrobenchmark

- **Android Studio Profiler → CPU / GPU / Energy**: attaches to the process and shows frame timing and system energy attribution where the platform supports it.
- **Macrobenchmark** (separate test module): can run scripted gameplay and record metrics including frame timing; useful for before/after comparisons on a fixed scenario.

## Better power attribution: Battery Historian

For deep “what consumed the battery” views:

1. `adb bugreport bugreport.zip` (or the older `bugreport.txt` flow).
2. Parse with [Battery Historian](https://github.com/google/battery-historian) (Docker or local build).

That is the usual way teams correlate **UID / package** with **estimated mAh** and wakeups over hours, not seconds.

## What is *not* realistic without hardware

True **per-app watts** at a instant, comparable across phones, generally needs **instrumented devices** (e.g. Google reference devices with exposed rails) or external power measurement — not something `adb` alone guarantees on every OEM ROM.
