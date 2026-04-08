#pragma once
#include "def.h"

#ifdef GBA

#include <string>

#define VOLADDR(a, t) (*(t volatile*)(a))
#define REG_MGBA_ENABLE VOLADDR(0x04FFF780, u16)
#define REG_MGBA_FLAGS VOLADDR(0x04FFF700, u16)
#define MGBA_LOG_OUT ((char*)0x04FFF600)

inline bool logInitMgba(void) {
    REG_MGBA_ENABLE = 0xC0DE;

    return REG_MGBA_ENABLE == 0x1DEA;
}

static void logOutputMgba(u8 level, const char* message) {
    for (int i = 0; message[i] && i < 256; i++) {
        MGBA_LOG_OUT[i] = message[i];
    }

    REG_MGBA_FLAGS = (level - 1) | 0x100;
}

static inline void log(int n) {
#ifdef NO_DIAGNOSE
    return;
#endif

    std::string str = std::to_string(n);

    logOutputMgba(4, str.c_str());
}

static inline void log(std::string str) {
#ifdef NO_DIAGNOSE
    return;
#endif

    logOutputMgba(4, str.c_str());
}

#endif
