#pragma once
#include <algorithm>
#include <iostream>
#include <list>
#include <string>
#include <vector>

#define TLN_EXCLUDE_WINDOW // needs to be before "Tilengine.h"

#include "Tilengine.h"
#include <cstring>

#if defined(PC)

#define SCREEN_WIDTH 512
#define SCREEN_HEIGHT 512

#elif defined(SWITCH)

#define SCREEN_WIDTH 512
#define SCREEN_HEIGHT 512

#elif defined(MM)

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 180

#elif defined(PORTMASTER)

#define SCREEN_WIDTH 360
#define SCREEN_HEIGHT 360

#else

#define SCREEN_WIDTH 512
#define SCREEN_HEIGHT 512

#endif

typedef unsigned char u8;
typedef unsigned short u16, COLOR;
typedef unsigned int u32, uint;

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;

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
    float angle = 0.0f;
    bool blend = false;
    bool doubleSize = false;
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

static inline void log(int n) { std::cout << std::to_string(n) << "\n"; }

static inline void log(std::string str) { std::cout << str << "\n"; }

extern TLN_Palette palettes[32];

extern TLN_Tileset tilesets[4];

extern TLN_Tilemap tilemaps[32];

extern TLN_SpriteData spriteData[128];

extern TLN_Spriteset spriteset;

extern TLN_Bitmap spriteBitmap;

extern u32 Sqrt(u32 n);

INLINE void setTile(int sbb, int x, int y, u32 value) {

    TLN_Tile t = new Tile();
    t->value = value;

    // TODO:
    if (y < 0)
        y += 64;

    TLN_SetTilemapTile(tilemaps[sbb], y, x, t);

    delete t;
}

INLINE u32 tileBuild(int index, bool flipx, bool flipy, int palette) {
    TLN_Tile t = new Tile();
    t->index = index & 0xffff;
    t->palette = palette & 0xf;
    t->flipx = flipx;
    t->flipy = flipy;

    u32 result = t->value;

    delete t;

    return result;
}

INLINE u32 tileBuild(u16 value) {
    TLN_Tile t = new Tile();
    t->index = value & 0xff;
    t->palette = (value >> 12) & 0xf;
    t->flipx = ((value >> 8) & 0xf) & 0x4;
    t->flipy = ((value >> 8) & 0xf) & 0x8;

    u32 result = t->value;

    delete t;

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

INLINE COLOR RGB15(int red, int green, int blue) {
    return red + (green << 5) + (blue << 10);
}

INLINE COLOR RGB15_SAFE(int red, int green, int blue) {
    return (red & 31) + ((green & 31) << 5) + ((blue & 31) << 10);
}

INLINE int min(int a, int b) { return (a < b) ? a : b; }

INLINE int max(int a, int b) { return (a > b) ? a : b; }

INLINE void setSpriteTile(int index, u8 tx, u8 ty, u8 width, const void* src) {
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
}

INLINE void setSpritePixel(int index, u8 tx, u8 ty, u8 width, u8 x, u8 y,
                           u8 color) {
    u8* dst = TLN_GetBitmapPtr(spriteBitmap, (tx + index) * 8 + x, ty * 8 + y);

    *dst = color;
}

INLINE void setSpriteTileRow(int index, int y, int n) {
    u8* dst = TLN_GetBitmapPtr(spriteBitmap, (index) * 8, y);

    for (int i = 0; i < 8; i++) {
        *dst++ |= (n >> (i * 4)) & 0xf;
    }
}

INLINE void setTilesetPixel(int cbb, int index, int x, int y, u8 color) {
    u8* dst = TLN_GetTilesetPtr(tilesets[cbb], index, x, y);

    *dst = color;
}

INLINE void setTileRow(int tileset, int index, int row, u32 data) {
    u8* dst = TLN_GetTilesetPtr(tilesets[tileset], index, 0, row);

    for (int i = 0; i < 8; i++)
        *dst++ = (data >> (4 * i)) & 0xf;
}

INLINE u32 bf_clamp(int x, uint len) {
    u32 y = x >> len;
    if (y)
        x = (~y) >> (32 - len);
    return x;
}

// [min,max)
INLINE int clamp(int x, int min, int max) {
    return (x >= max) ? (max - 1) : ((x < min) ? min : x);
}

extern void color_fade_fast(int palette, int index, const COLOR* src,
                            COLOR color, int count, u32 alpha);

extern void clr_rgbscale(COLOR* dst, const COLOR* src, uint nclrs, COLOR clr);

extern void rumble_set_state(int);

extern "C" {
extern void posprintf(char* buff, const char* str, ...);
}

extern u16 gradientTable[SCREEN_HEIGHT + 1];

extern void setSpriteMaskRegion(int shift);

#define UNBOUND -2

enum class ShaderStatus {
    NOT_INITED,
    NO_GL,
    NO_SHADER_FILES,
    SHADER_FILE_ERROR,
    OK,
};

extern ShaderStatus shaderStatus;
