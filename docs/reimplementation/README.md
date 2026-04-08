# Apotris Android — full feature reimplementation

This folder describes how to close the gap between the **shipped, decompiled** `com.example.apotris` APK (touch overlay, portrait layout, JNI virtual buttons, overlay persistence) and the **current repo** build (C++ game + SDL shell + minimal `android-port`).

## Documents

| File | Purpose |
|------|---------|
| [PLAN.md](./PLAN.md) | Architecture comparison, phases, risks, verification |
| [TASKS.md](./TASKS.md) | Numbered checklist with acceptance criteria |
| [INVENTORY-decompiled.md](./INVENTORY-decompiled.md) | Class map, preference keys, JNI surface, button IDs |
| [JNI.md](./JNI.md) | Shipped `nativeVirtualButtonEvent` / `nativeSetPortraitMode` signatures |

## Source of truth for “what shipped”

After running `tools/decompile/Decompile-PhoneApk.ps1` (or equivalent):

- **JADX (Java/Kotlin shape):** `tools/decompile/jadx-phone/sources/`
- **Apktool (resources, smali):** `tools/decompile/apktool-phone/`

The repo’s **generated** Gradle tree (`main/apotris/build-android/project/`) is overwritten by `build.ps1`; long-lived app code should live under **`android-port/`** and be merged by `Apply-ApotrisAndroidPort`.

## Related

- Older touch/Compose notes: `main/implementation_plan.md` (Lemuroid-oriented; partial vs shipped APK).
