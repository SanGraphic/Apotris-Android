/**
 * Detects MyBoy and gpSP emulators through exploiting a bug in ram
 * overclocking. Credit to Evan Bowman from the gbadev discord
 */

#ifdef GBA
#include "detectEmulators.h"

EWRAM_DATA static int _ewram_static_data = 0xFF;

bool ram_overclock() {
    vu32* memctrl_register = (vu32*)(unsigned*)(0x4000800);
    *memctrl_register = 0x0E000020;

    vs32* ewram_static_data = (vs32*)&_ewram_static_data;
    *ewram_static_data = 1;

    if (*ewram_static_data != 1) {
        *memctrl_register = 0x0D000020;
        return false;
    } else {
        return true;
    }
}

bool detect_inaccurate_emulator() {
    const u16 prev_dispcnt = REG_DISPCNT;

    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;

    // RAM overclocking in MyBoy! erroneously clears REG_DISPCNT.
    ram_overclock();

    const bool detected = !(REG_DISPCNT & DCNT_BG0);

    REG_DISPCNT = prev_dispcnt;
    return detected;
}
#endif