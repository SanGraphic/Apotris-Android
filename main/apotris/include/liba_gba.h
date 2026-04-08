#pragma once
#include "audio_files.h"
#include "interrupt.h"
#include "gba-rumble.h"
#include "gba-rumble-cart.h"
#include "mm_types.h"
#include <stdint.h>
#include <string>
#include <tonc.h>
/* #include "soundbank.h" */
#include <map>
#include <memory>
#include <posprintf.h>

static inline void setTile(int sbb, int x, int y, uint32_t value) {
    se_mem[sbb][y * 32 + x] = value & 0xffff;
}

static inline uint32_t tileBuild(int index, bool flipx, bool flipy,
                                 int palette) {
    return 0x1000 * palette + 0x400 * flipx + 0x800 * flipy + index;
}

INLINE u32 tileBuild(u16 value) { return value; }

INLINE void setSpritePixel(int index, u8 tx, u8 ty, u8 width, u8 x, u8 y,
                           u8 color) {
    TILE* t = &tile_mem[4][index + ty * width + tx];

    if (color)
        t->data[y] |= color << (x * 4);
    else
        t->data[y] &= ~(0xf << (x * 4));
}

INLINE void setSpriteTile(int index, u8 tx, u8 ty, u8 width, const void* src) {
    memcpy32(&tile_mem[4][index + ty * width + tx], src, 8);
}

INLINE void clearSpriteTile(int index, u8 tx, u8 ty, u8 width) {
    memset32(&tile_mem[4][index + ty * width + tx], 0, 8);
}

INLINE void setTileRow(int tileset, int index, int row, u32 data) {
    TILE* tile = &tile_mem[tileset][index];
    tile->data[row] = data;
}

INLINE void setTilesetPixel(int cbb, int index, int x, int y, int color) {
    TILE* tile = &tile_mem[cbb][index];

    tile->data[y] |= (color) << (x * 4);
}

INLINE void color_fade_fast(int palette, int index, COLOR* src, COLOR color,
                            int count, u32 alpha) {
    clr_fade_fast(src, color, &pal_bg_mem[palette * 16 + index], count, alpha);
}

INLINE s16 sinLut(int angle) { return sin_lut[angle]; }

INLINE void setSpriteTileRow(int index, int y, int n) {
    TILE* tile = &tile_mem[4][index];

    tile->data[y] |= n;
}

extern u32 sleep_combo;

u16 get_sleep_combo(u32 index);

u32 get_autosleep_mins(u32 index);

// Hard waits until all keys in the supplied mask are released, with an optional timeout.
void await_key_release(u32 keys, int timeout_secs);

bool sleep_combo_check(u32 keys);

void initRumbleCart();

extern OBJ_AFFINE* obj_aff_buffer;

extern mm_word myEventHandler(mm_word msg, mm_word param);

#define blendInfo REG_BLDCNT

IWRAM_CODE void memcpy16_fast(void* dest, const void* src, int hwCount);
IWRAM_CODE void memcpy32_fast(void* dest, const void* src, int wCount);
IWRAM_CODE void memset32_fast(void* dest, int word, int wCount);

INLINE u32 keys_raw() {
    return REG_KEYINPUT;
};

extern u16 gradientTable[192 + 1];

extern std::map<int, std::string> keyToString;

#define UNBOUND 0

extern void toggleSpriteSorting(bool state);
void setUpLinkUniversal(bool detectEmus);
