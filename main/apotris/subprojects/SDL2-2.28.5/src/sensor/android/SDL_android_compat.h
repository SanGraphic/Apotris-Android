/*
 * SDL_android_compat.h -- NDK compatibility shims for SDL2 2.28.5
 *
 * ALooper_pollAll() was deprecated in API 29 and its declaration was silently
 * removed from the NDK r29 sysroot headers (android/looper.h) when
 * __ANDROID_API__ >= 30.  ALooper_pollOnce() is the direct replacement:
 * with timeout=0 it gives the same non-blocking poll-and-return behaviour that
 * SDL_androidsensor.c relies on.
 *
 * Guard: __NDK_MAJOR__ is defined by the NDK toolchain header <android/ndk-version.h>
 * and is the clean way to detect the toolchain generation without guessing
 * from API levels.  On NDK 26 the function still exists in looper.h, so we
 * only inject the shim starting from NDK 29.
 */
#pragma once

#include <android/looper.h>

/* NDK r29+ (version 29.0.0 and above) no longer declare ALooper_pollAll in
 * their looper.h when the target API >= 30.  Inject a thin wrapper. */
#if defined(__NDK_MAJOR__) && (__NDK_MAJOR__ >= 29)
static inline int ALooper_pollAll(int timeoutMillis, int *outFd,
                                  int *outEvents, void **outData)
{
    /* ALooper_pollOnce with the same zero timeout is functionally identical
     * for the non-blocking use in SDL_androidsensor.c (timeout == 0). */
    return ALooper_pollOnce(timeoutMillis, outFd, outEvents, outData);
}
#endif /* __NDK_MAJOR__ >= 29 */
