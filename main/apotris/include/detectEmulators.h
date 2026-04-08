#pragma once

#ifdef GBA

#include <tonc.h>

#ifdef __cplusplus
extern "C" {
#endif
bool ram_overclock();
bool detect_inaccurate_emulator();

#ifdef __cplusplus
}
#endif

#endif