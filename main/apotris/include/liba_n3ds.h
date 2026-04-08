#pragma once

#include "audio_files.h"
#include "n3ds/renderer.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

#include <map>
#include <string>

extern "C" {

#include <3ds/svc.h>
#include <3ds/types.h>

#include <3ds/services/hid.h>
}

#ifdef DEBUG_UI
#define TLN_EXCLUDE_WINDOW
#include <Tilengine.h>
#else

// Let's forget that type-punning via union is technically UB in C++ --
// it is supported as a GCC extension...
union Tile {
    uint32_t value;
    struct {
        uint16_t index;
        union {
            uint16_t flags;
            struct {
                uint8_t unused : 4;
                uint8_t palette : 4;
                uint8_t tileset : 3;
                bool masked : 1;
                bool priority : 1;
                bool rotated : 1;
                bool flipy : 1;
                bool flipx : 1;
            };
        };
    };
};

#endif

// #define SCREEN_WIDTH 400
// #define SCREEN_HEIGHT 240
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 160

typedef uint8_t u8;
typedef uint16_t u16, COLOR;
typedef uint32_t u32, uint;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;

#define IWRAM_CODE

#define ATTR0_AFF 0
#define ATTR0_AFF_DBL 0

#define CLR_MASK 0x001F

#define RED_MASK 0x001F
#define RED_SHIFT 0
#define GREEN_MASK 0x03E0
#define GREEN_SHIFT 5
#define BLUE_MASK 0x7C00
#define BLUE_SHIFT 10

extern int blendInfo;

typedef struct TILE {
    u32 data[8];
} TILE;

static int spriteIdCounter = 0;

class SpriteAttributes {
public:
    int id = spriteIdCounter++;
    bool enabled = false;
    int tile_start = 0;
    int x = 0;
    int y = 0;
    int sx = 0;
    int sy = 0;
    bool affine = false;
    bool mosaic = false;
    float scalex = 0.0f;
    float scaley = 0.0f;
    int palette = 0;
    int priority = 0;
    bool flipx = false;
    bool flipy = false;
    bool rotate = false;
    bool blend = false;
    bool doubleSize = false;
    float affine_mat[4] = {1, 0, 0, 1}; // row-major affine transform
};

typedef SpriteAttributes OBJ_ATTR;

#define INLINE static inline

typedef int FIXED;

#define FIXED_BITS 8
#define float2fx(f) (FIXED)(f * ((float)(1 << FIXED_BITS)))
#define int2fx(i) ((int)(i) << FIXED_BITS)
#define fx2int(f) ((int)(f) >> FIXED_BITS)
#define fx2float(f) (float)(f) / (1 << FIXED_BITS)

INLINE FIXED fxmul(FIXED fa, FIXED fb) { return (fa * fb) >> FIXED_BITS; }

INLINE FIXED fxdiv(FIXED fa, FIXED fb) {
    return ((fa) * (1 << FIXED_BITS)) / (fb);
}

#define ALIGN(n) __attribute__((aligned(n)))

static inline void log(std::string str) {
    if (str[str.size() - 1] != '\n') {
        str.push_back('\n');
    }
    svcOutputDebugString(str.c_str(), str.size());
    fputs(str.c_str(), stdout);
}

static inline void log(int n) { log(std::to_string(n)); }

#ifdef DEBUG_UI
extern TLN_Palette palettes[32];

extern TLN_Tileset tilesets[4];

extern TLN_Tilemap tilemaps[32];

extern TLN_SpriteData spriteData[128];

extern TLN_Spriteset spriteset;

extern TLN_Bitmap spriteBitmap;
#endif

// #define RENDERER_TLN 1

extern u32 Sqrt(u32 n);

#ifdef DEBUG_UI
INLINE void setTileTLN(int sbb, int x, int y, u32 value);
#endif

INLINE void setTile(int sbb, int x, int y, u32 value) {
    Tile t{};
    t.value = value;

    u16 gbaTile =
        0x1000 * t.palette + 0x400 * t.flipx + 0x800 * t.flipy + t.index;
    tng::bg_screenblocks[sbb][((y < 0) ? (y + 32) : y) * 32 + x] = gbaTile;

    tng::bg_screenblock_fill_screen_p[sbb] = false;

#ifdef DEBUG_UI
    setTileTLN(sbb, x, y, value);
#endif
}

#ifdef DEBUG_UI
INLINE void setTileTLN(int sbb, int x, int y, u32 value) {
    Tile t{};
    t.value = value;

    // TODO:
    if (y < 0)
        y += 64;

    TLN_SetTilemapTile(tilemaps[sbb], y, x, &t);
}
#endif

INLINE u32 tileBuild(int index, bool flipx, bool flipy, int palette) {
    Tile t{};
    t.index = index & 0xffff;
    t.palette = palette & 0xf;
    t.flipx = flipx;
    t.flipy = flipy;

    u32 result = t.value;

    return result;
}

INLINE u32 tileBuild(u16 value) {
    Tile t{};
    t.index = value & 0xff;
    t.palette = (value >> 12) & 0xf;
    t.flipx = ((value >> 8) & 0xf) & 0x4;
    t.flipy = ((value >> 8) & 0xf) & 0x8;

    u32 result = t.value;

    return result;
}

INLINE void memcpy16(void* dst, const void* src, int count) {
    memcpy(dst, src, count * 2);
}

INLINE void memcpy32(void* dst, const void* src, int count) {
    memcpy(dst, src, count * 4);
}

INLINE void memset16(void* dst, u16 n, int count) {
    u16* dest = (u16*)dst;
    while (count--) {
        *dest++ = n;
    }
}

INLINE void memset32(void* dst, u32 n, int count) {
    u32* dest = (u32*)dst;
    while (count--) {
        *dest++ = n;
    }
}

INLINE void memcpy16_fast(void* dest, const void* src, int hwCount) {
    memcpy16(dest, src, hwCount);
}
INLINE void memcpy32_fast(void* dest, const void* src, int wCount) {
    memcpy32(dest, src, wCount);
}
INLINE void memset32_fast(void* dest, int word, int wCount) {
    memset32(dest, word, wCount);
}

constexpr COLOR RGB15(int red, int green, int blue) {
    return red + (green << 5) + (blue << 10);
}

constexpr COLOR RGB15_SAFE(int red, int green, int blue) {
    return (red & 31) + ((green & 31) << 5) + ((blue & 31) << 10);
}

constexpr int min(int a, int b) { return (a < b) ? a : b; }

constexpr int max(int a, int b) { return (a > b) ? a : b; }

INLINE void setSpriteTile(int index, u8 tx, u8 ty, u8 width, const void* src) {
    memcpy32(&tng::sprite_tile_images[index + ty * width + tx], src, 8);

#ifdef DEBUG_UI
    u8* source = (u8*)src;
    uint8_t tmp[8 * 8];
    memset(tmp, 0, 64);
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 4; j++) {
            tmp[i * 8 + j * 2] = source[i * 4 + j] & 0xf;
            tmp[i * 8 + j * 2 + 1] = (source[i * 4 + j] >> 4) & 0xf;
        }
    }

    TLN_SpriteData d;
    memset(d.name, 0, 64);
    d.w = 8;
    d.h = 8;
    d.x = 8 * (tx + index);
    d.y = 8 * ty;

    TLN_SetSpritesetData(spriteset, 0, &d, tmp, 8);
#endif
}

INLINE void setSpritePixel(int index, u8 tx, u8 ty, u8 width, u8 x, u8 y,
                           u8 color) {
    tng::ImageTile* t = &tng::sprite_tile_images[index + ty * width + tx];

    if (color)
        t->rows[y] |= color << (x * 4);
    else
        t->rows[y] &= ~(0xf << (x * 4));

#ifdef DEBUG_UI
    u8* dst = TLN_GetBitmapPtr(spriteBitmap, (tx + index) * 8 + x, ty * 8 + y);

    *dst = color;
#endif
}

INLINE void setSpriteTileRow(int index, int y, int n) {
    tng::ImageTile* tile = &tng::sprite_tile_images[index];

    tile->rows[y] |= n;

#ifdef DEBUG_UI
    u8* dst = TLN_GetBitmapPtr(spriteBitmap, (index) * 8, y);

    for (int i = 0; i < 8; i++) {
        *dst++ |= (n >> (i * 4)) & 0xf;
    }
#endif
}

INLINE void setTilesetPixel(int cbb, int index, int x, int y, u8 color) {
    tng::ImageTile* tile = &tng::bg_charblocks[cbb][index];

    tile->rows[y] |= (color) << (x * 4);

#ifdef DEBUG_UI
    u8* dst = TLN_GetTilesetPtr(tilesets[cbb], index, x, y);

    *dst = color;
#endif
}

INLINE void setTileRow(int tileset, int index, int row, u32 data) {
    tng::ImageTile* tile = &tng::bg_charblocks[tileset][index];

    tile->rows[row] = data;

#ifdef DEBUG_UI
    u8* dst = TLN_GetTilesetPtr(tilesets[tileset], index, 0, row);

    for (int i = 0; i < 8; i++)
        *dst++ = (data >> (4 * i)) & 0xf;
#endif
}

INLINE u32 bf_clamp(int x, uint len) {
    u32 y = x >> len;
    if (y)
        x = (~y) >> (32 - len);
    return x;
}

INLINE int clamp(int x, int min, int max) {
    return (x >= max) ? (max - 1) : ((x < min) ? min : x);
}

extern void color_fade_fast(int palette, int index, const COLOR* src,
                            COLOR color, int count, u32 alpha);

extern void clr_rgbscale(COLOR* dst, const COLOR* src, uint nclrs, COLOR clr);

extern "C" {
extern void posprintf(char* buff, const char* str, ...);
}

extern u16 gradientTable[SCREEN_HEIGHT + 1];

extern void setSpriteMaskRegion(int shift);

// ******************
// Key/Input
// ******************

constexpr u32 KEY_FULL = 0xffffffffu;
constexpr u32 VALID_KEY_MASK = 0b11111111'00010000'11001111'11111111u;
constexpr u32 UNBOUND = 0;

#define KEY_DOWN ((s32)KEY_DOWN)

extern void key_poll();

extern uint32_t key_is_down(uint32_t);
extern uint32_t key_hit(uint32_t);
extern uint32_t key_released(uint32_t);
extern uint32_t keys_raw();

extern std::map<int, std::string> keyToString;

// ******************
// N3DS-specific
// ******************

#define DATA_DIR_BASE "/3ds/Apotris"
#define SAVE_FILE_NAME "Apotris.sav"
#define SAVE_FILE_PATH DATA_DIR_BASE "/" SAVE_FILE_NAME

constexpr const char* SDMC_SAVE_FILE_PATH = "sdmc:" SAVE_FILE_PATH;

namespace n3ds {

extern bool wideModeSupported;

extern int savedAspectRatio;

extern bool wasAppPaused;

enum class ScaleMode : u8 {
    UNSCALED = 0,  //< 1x
    SCALED_LINEAR, //< 1.5x (linear filtering)
    SCALED_SHARP,  //< 1.5x (2x then downscale with linear filtering)
    SCALED_ULTRA,  //< 1.5x (3x then downscale with linear filtering)
    NUM_ENTRIES,
};

inline void setSbbFillScreen(int sbb) {
    tng::bg_screenblock_fill_screen_p[sbb] = true;
}

bool keyboardNameInput(std::string* result, size_t length);

} /* namespace n3ds */
