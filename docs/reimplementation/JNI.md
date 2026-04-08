# JNI surface (shipped vs repo)

Java declarations from decompiled `org.libsdl.app.SDLActivity` (`tools/decompile/jadx-phone/sources/org/libsdl/app/SDLActivity.java`):

```java
public static native void nativeSetPortraitMode(boolean portrait);
public static native void nativeVirtualButtonEvent(int buttonId, boolean pressed);
```

## JNI names (typical `javah` style)

If registered implicitly by name (not `RegisterNatives`), the VM looks for:

- `Java_org_libsdl_app_SDLActivity_nativeSetPortraitMode` with signature `(Z)V`
- `Java_org_libsdl_app_SDLActivity_nativeVirtualButtonEvent` with signature `(IZ)V`

**Caveat:** If you move methods to `ApotrisActivity` in another package, native names **change** unless you use `RegisterNatives` from `JNI_OnLoad`.

## Repo status

`main/apotris` source grep shows **no** current implementations of these symbols. They must be added to the same shared library that loads for `SDLActivity` (here: **`libmain.so`**).

## Next steps (task T0.3 / T1.1)

1. Run `nm -D` / `llvm-nm` on phone `libmain.so` vs locally built `libmain.so` and diff undefined vs defined JNI stubs.
2. Or add logging stubs that print `buttonId`/`pressed` and confirm calls from Java before wiring to `key_poll` / SDL event queue.

See [PLAN.md](./PLAN.md) §4.1 and [TASKS.md](./TASKS.md) Phase 1.
