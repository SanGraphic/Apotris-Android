# Porting Apotris Android to multiple CPU ABIs (universal APK)

This repository currently ships **one** native library: **`libmain.so`** built for **arm64-v8a** only. Gradle’s **`ndk { abiFilters 'arm64-v8a' }`** matches that. A **universal APK** (or an AAB with all selected ABIs) must contain a **matching `libmain.so` (and usually `libc++_shared.so`) under each ABI folder** Gradle packages.

Below is a practical recipe using the **same Meson + NDK LLVM** flow as `build.ps1`, without changing game C++ unless you hit rare 32-bit issues.

---

## 0. Automated universal build (isolated from `build.ps1`)

**`build-universal.ps1`** (repo root) produces a **fat APK** with up to four ABIs. It is fully separate from the default pipeline so **`build.ps1` and `main/apotris/build-android/` are untouched**:

| Path | Purpose |
|------|--------|
| `main/apotris/build-android-universal/` | One Meson output dir per ABI (`meson-arm64`, `meson-armv7`, …) + Gradle tree `project/` |
| `main/apotris/meson/universal-generated/` | Generated Meson cross INIs (gitignored) |

**Usage:**

```powershell
.\build-universal.ps1
.\build-universal.ps1 -SkipNative
.\build-universal.ps1 -Install
.\build-universal.ps1 -Abis arm64-v8a,armeabi-v7a
```

Default **`-Abis`**: `arm64-v8a,armeabi-v7a,x86,x86_64`. Keystore env vars match **`build.ps1`** (`APOTRIS_KEYSTORE`, etc.). Signed output: **`apotris-universal-YYYY-MM-DD.apk`** at the repo root. Uber signer staging uses **`signed_output_universal/`** (not `signed_output/`) so parallel runs do not clash.

When **`ANDROID_NDK_HOME` is unset**, the script **prefers the newest NDK `26.*`** under `%LOCALAPPDATA%\Android\Sdk\ndk`. **NDK 29+** marks **`ALooper_pollAll`** unavailable and breaks bundled **SDL 2.28.5** unless SDL is upgraded. Set **`ANDROID_NDK_HOME`** explicitly to pick another NDK.

The generated universal **`app/build.gradle`** sets **`versionName '4.1.1b-universal'`** and **`abiFilters`** to match the ABIs you built. **`applicationId`** is still **`com.apotris.android`** — you cannot install the universal and arm64-only APK side by side without changing the id in one of the pipelines.

---

## 1. ABI folders Gradle expects

Under:

`main/apotris/build-android/project/app/src/main/jniLibs/`

typical layouts:

| Folder | Architecture | Typical devices |
|--------|----------------|------------------|
| `arm64-v8a` | AArch64 | Most phones/tablets 2017+ |
| `armeabi-v7a` | 32-bit ARM (ARMv7-A) | Older 32-bit-only ARM devices |
| `x86` | 32-bit Intel | Some emulators, rare devices |
| `x86_64` | 64-bit Intel | Emulators, some Chromebooks |

The Play Store **does not** use deprecated `armeabi` or `mips` anymore; ignore those.

For a **fat universal APK**, populate **every** ABI you list in **`abiFilters`** (or omit **`abiFilters`** to include all copied folders—Gradle packages whatever exists under `jniLibs/`).

---

## 2. NDK toolchain triples and API level

`build.ps1` uses **NDK r26+**-style Clang wrappers and **`api = 34`** in the Meson cross file. For each ABI, the compiler name pattern is:

`$TOOLCHAIN/<triple><api>-clang++.cmd` (Windows) or the same without `.cmd` on Linux/macOS.

| ABI | Meson `cpu_family` / `cpu` | Clang triple (example) |
|-----|----------------------------|-------------------------|
| arm64-v8a | `aarch64` / `aarch64` | `aarch64-linux-android` |
| armeabi-v7a | `arm` / `armv7a` | `armv7a-linux-androideabi` |
| x86 | `x86` / `i686` | `i686-linux-android` |
| x86_64 | `x86_64` / `x86_64` | `x86_64-linux-android` |

**`libc++_shared.so`** lives next to the **same triple** under the NDK sysroot, e.g.:

`toolchains/llvm/prebuilt/<host>/sysroot/usr/lib/<triple>/libc++_shared.so`

Copy the **matching** library into each **`jniLibs/<abi>/`** next to **`libmain.so`**.

---

## 3. Meson cross files (one per ABI)

Today `Write-ApotrisMesonCrossIni` in **`build.ps1`** writes **`main/apotris/meson/android-arm64-auto.ini`**.

To add **armeabi-v7a**, duplicate that pattern into e.g. **`android-armv7-auto.ini`**:

- **`[binaries]`** — use **`armv7a-linux-androideabi`** + `api` + `-clang` / `-clang++.cmd` (same `api` as arm64 for consistency).
- **`[host_machine]`** — `cpu_family = 'arm'`, `cpu = 'armv7a'` (or `'arm'` per Meson docs for your NDK), `endian = 'little'`.

For **x86** / **x86_64**, set **`cpu_family` / `cpu`** to `x86` / `i686` or `x86_64` / `x86_64` and the matching **`i686-linux-android`** or **`x86_64-linux-android`** binaries.

**Important:** use a **separate build directory per ABI**, e.g.:

- `main/apotris/build-android-arm64`
- `main/apotris/build-android-armv7`
- `main/apotris/build-android-x86`
- `main/apotris/build-android-x86_64`

Reusing one build dir and only swapping the cross file will confuse Meson’s cached host configuration. From repo root, for each ABI:

```text
python -m mesonbuild.mesonmain setup main/apotris/build-android-armv7 main/apotris \
  --cross-file main/apotris/meson/android-armv7-auto.ini \
  -Db_lto=false --buildtype=release
python -m mesonbuild.mesonmain compile -C main/apotris/build-android-armv7 main
```

Then copy:

- `build-android-armv7/libmain.so` → `project/app/src/main/jniLibs/armeabi-v7a/libmain.so`
- matching **`libc++_shared.so`** → same folder.

Repeat for each ABI.

---

## 4. Automating in `build.ps1` (sketch)

A full multi-ABI script would:

1. Loop over a list `('arm64-v8a', 'aarch64', ...)`, `('armeabi-v7a', 'armv7a', ...)`, etc.
2. Emit or select the right cross INI (or template with placeholders).
3. **`meson setup`** / **`compile`** into **`build-android-<name>`**.
4. **`Copy-Item`** `libmain.so` and **`libc++_shared.so`** into **`jniLibs/<abi>/`**.
5. **`llvm-strip`** each **`libmain.so`** (paths differ per ABI in the NDK prebuilt tree).

Optional: add **`-Param $Abis`** defaulting to `arm64-v8a` so CI can pass `-Abis arm64-v8a,armeabi-v7a`.

---

## 5. Gradle: universal vs split APKs

**Universal (single APK, larger download):**

```gradle
ndk {
    abiFilters 'arm64-v8a', 'armeabi-v7a', 'x86', 'x86_64'
}
```

(or remove **`abiFilters`** entirely if you only want “whatever is in `jniLibs`”.)

**Smaller downloads (Play feature):** ship an **Android App Bundle (AAB)**; Play generates per-ABI splits. Locally you can use **`splits { abi { ... } }`** in Gradle to produce one APK per ABI—still “universal” from the user’s perspective if they install from the right split.

---

## 6. 32-bit ARM specifics

- **Pointer width:** Game code is largely pointer-safe; if you add native code that assumes 64-bit `size_t`, audit those sites.
- **Performance:** 32-bit builds may need **lower default shader complexity** or **smaller textures** on very old GPUs—usually not required for Apotris’s 512×512 framebuffer path.
- **GLES 2.0:** The current pipeline is already **GLES 2 + GLSL ES 1.00** on Android; the same shaders apply to **armeabi-v7a** devices that only expose ES 2.

---

## 7. Testing matrix (minimal)

| Target | How |
|--------|-----|
| arm64-v8a | Physical phone (most common) |
| armeabi-v7a | Old 32-bit device or emulator system image **without** 64-bit ABI |
| x86_64 | Android Studio emulator with Google APIs |

Verify: cold start, audio, **shaders on/off**, gamepad after rotation (see **`parity.md`** input section).

---

## 8. Checklist summary

1. Add Meson **cross file(s)** for each new ABI.  
2. **Separate `build-android-<abi>`** directory per compile.  
3. Copy **`libmain.so` + `libc++_shared.so`** into **`jniLibs/<abi>/`**.  
4. Update **`abiFilters`** (or AAB / splits) in **`app/build.gradle`** (generated block in **`build.ps1`** today).  
5. Re-run **`assembleRelease`** (or your **`build.ps1`** once extended).  
6. Confirm installed APK contains all **`lib/<abi>/libmain.so`** via **`unzip -l`** or Android Studio **APK Analyzer**.

This is enough for a **universal** native footprint; Java/Kotlin code in **`android-port/`** is already architecture-neutral.
