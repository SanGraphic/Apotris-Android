#include "platform.hpp"
#include <string>

#ifdef TE
#include "Tilengine.h"
#include "def.h"
#include "logging.h"
#include "scene.hpp"
#include "stdarg.h"
#include "stddef.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <list>

#include <map>

static int offsetx = (SCREEN_WIDTH - 240) / 2;
static int offsety = (SCREEN_HEIGHT - 160) / 2;

#if defined(__ANDROID__)
#include <dlfcn.h>

using TLN_SetSpriteRotationFn = bool (*)(int, float);
using TLN_ResetSpriteRotationFn = bool (*)(int);

static TLN_SetSpriteRotationFn resolveSetSpriteRotation() {
    static auto fn = reinterpret_cast<TLN_SetSpriteRotationFn>(
        dlsym(RTLD_DEFAULT, "TLN_SetSpriteRotation"));
    return fn;
}

static TLN_ResetSpriteRotationFn resolveResetSpriteRotation() {
    static auto fn = reinterpret_cast<TLN_ResetSpriteRotationFn>(
        dlsym(RTLD_DEFAULT, "TLN_ResetSpriteRotation"));
    return fn;
}

#define TLN_HAS_SPRITE_ROTATION_SAFE()                                          \
    ((resolveSetSpriteRotation() != nullptr) &&                                 \
     (resolveResetSpriteRotation() != nullptr))
#define TLN_SET_SPRITE_ROTATION_SAFE(sprite, angle)                             \
    do {                                                                         \
        auto fn = resolveSetSpriteRotation();                                    \
        if (fn)                                                                  \
            fn(sprite, angle);                                                   \
    } while (0)
#define TLN_RESET_SPRITE_ROTATION_SAFE(sprite)                                   \
    do {                                                                         \
        auto fn = resolveResetSpriteRotation();                                  \
        if (fn)                                                                  \
            fn(sprite);                                                          \
    } while (0)
#else
#define TLN_HAS_SPRITE_ROTATION_SAFE() (true)
#define TLN_SET_SPRITE_ROTATION_SAFE(sprite, angle) \
    TLN_SetSpriteRotation(sprite, angle)
#define TLN_RESET_SPRITE_ROTATION_SAFE(sprite) TLN_ResetSpriteRotation(sprite)
#endif

int spriteVOffset = 0;

static bool blendSprites = false;

int blendInfo = 0;

void onHBlank(int line);
uint8_t customBlend(uint8_t a, uint8_t b);
void showFPS();

std::list<float> fpsHistory;

class ColorSplit {

public:
    int r = 0;
    int g = 0;
    int b = 0;

    // Red and Blue are swapped intentionally to fix Tilengine -> SFML issue
    ColorSplit(int _r, int _g, int _b) {
        b = _r;
        g = _g;
        r = _b;
    }

    ColorSplit(u16 c) {
        b = (((c) & 0x1f) << 3);
        g = (((c >> 5) & 0x1f) << (3));
        r = (((c >> 10) & 0x1f) << (3));
    }

    ColorSplit(u32* c) {
        b = (*c >> (16 + 3)) & 0x1f;
        g = (*c >> (8 + 3)) & 0x1f;
        r = (*c >> (0 + 3)) & 0x1f;
    }
};

static inline int rgb5to8(u16 color) {
    return (((color) & 0x1f) << 3) + (((color >> 5) & 0x1f) << (3 + 8)) +
           (((color >> 10) & 0x1f) << (3 + 16));
}

class LayerInfo {
public:
    int id = -1;
    bool enabled = true;
    int priority = -1;
    int sbb = 0;
    int cbb = 0;
};

TLN_Palette palettes[32];

TLN_Tileset tilesets[4];

TLN_Tilemap tilemaps[32];

TLN_Spriteset spriteset;

TLN_SpriteData spriteData[128];

TLN_Bitmap spriteBitmap;

LayerInfo layerInfo[4];

u8* framebuffer;

#define TILEMAP_SIZE 64

void platformInit() {

    TLN_Init(SCREEN_WIDTH, SCREEN_HEIGHT, 4, 128, 0);
    framebuffer = (uint8_t*)malloc(4 * SCREEN_WIDTH * SCREEN_HEIGHT);
    TLN_SetRenderTarget(framebuffer, 4 * SCREEN_WIDTH);

    for (int i = 0; i < 32; i++) {
        palettes[i] = TLN_CreatePalette(16);
        if (i < 16)
            TLN_SetGlobalPalette(i, palettes[i]);
    }

    for (int i = 0; i < 4; i++) {
        tilesets[i] = TLN_CreateTileset(512, 8, 8, palettes[0], NULL, NULL);
    }

    Tile tiles[TILEMAP_SIZE * TILEMAP_SIZE];
    for (int i = 0; i < TILEMAP_SIZE; i++)
        for (int j = 0; j < TILEMAP_SIZE; j++)
            tiles[i * TILEMAP_SIZE + j].value = 0;

    for (int i = 0; i < 32; i++) {
        tilemaps[i] = TLN_CreateTilemap(64, 64, tiles, 0, tilesets[0]);
    }

    spriteBitmap = TLN_CreateBitmap(1024 * 8, 64, 8);

    TLN_SetBitmapPalette(spriteBitmap, palettes[16]); //  NECESSARY ! ! !
    spriteset = TLN_CreateSpriteset(spriteBitmap, spriteData, 128);

    TLN_SetRasterCallback(onHBlank);

    TLN_SetCustomBlendFunction(customBlend);

    windowInit();
}

void deinitialize() {
    TLN_Deinit();

    for (int i = 1; i < 4; i++)
        TLN_DeleteTileset(tilesets[i]);

    for (int i = 0; i < 32; i++)
        TLN_DeleteTilemap(tilemaps[i]);

    TLN_DeleteSpriteset(spriteset);

    for (int i = 0; i < 32; i++)
        TLN_DeletePalette(palettes[i]);

    free(framebuffer);
}

void toggleBG(int layer, bool state) {
    int n = layerInfo[layer].id;

    if (state) {
        TLN_EnableLayer(n);
    } else {
        TLN_DisableLayer(n);
    }

    layerInfo[layer].enabled = state;
}

void toggleSprites(bool state) {}

INLINE u8 buildLayerPrio(int layer, int priority) {
    return ((priority & 0x3) << 4) + (layer & 0x3);
}

void buildBG(int layer, int cbb, int sbb, int size, int prio, int mos) {
    int p = buildLayerPrio(layer, prio);

    layerInfo[layer].priority = p;
    layerInfo[layer].sbb = sbb;
    layerInfo[layer].cbb = cbb;

    int a[4] = {-1, -1, -1, -1};

    for (int l = 0; l < 4; l++) {
        for (int pos = 0; pos < 4; pos++) {
            if (a[pos] == -1) {
                a[pos] = l;
                break;
            } else if (layerInfo[a[pos]].priority > layerInfo[l].priority) {
                for (int rpos = 3; rpos > pos; rpos--) {
                    a[rpos] = a[rpos - 1];
                }
                a[pos] = l;
                break;
            }
        }
    }

    for (int pos = 0; pos < 4; pos++) {
        if (a[pos] == -1)
            break;

        // log("layer " + std::to_string(a[pos]) + " prio " +
        // std::to_string((layerInfo[a[pos]].priority >> 4) & 0x3));
        layerInfo[a[pos]].id = pos;
        TLN_SetTilemapTileset(tilemaps[layerInfo[a[pos]].sbb],
                              tilesets[layerInfo[a[pos]].cbb]);
        TLN_SetLayerPriority(pos,
                             (((layerInfo[a[pos]].priority >> 4) & 0x3) == 0));
        TLN_SetLayerTilemap(pos, tilemaps[layerInfo[a[pos]].sbb]);

        // toggleBG(pos,layerInfo[pos].enabled);
    }

    enableBlend(blendInfo);
}

void vsync() {
    onVBlank();

    showFPS();

    TLN_UpdateFrame(0);

    updateWindow(framebuffer);
}

void showSprites(int n) {
    std::list<int> spriteList;

    for (int i = 4; i >= 1; i--) {
        for (int counter = 127; counter >= 0; counter--) {
            OBJ_ATTR* sprite = &obj_buffer[counter];
            if (sprite->enabled && sprite->priority == i && sprite->id >= 0 &&
                sprite->id < 128) {
                spriteList.push_back(sprite->id);
            }
        }
    }

    for (int i = 0; i < 128; i++) {
        TLN_DisableSprite(i);
    }

    int counter = 0;
    for (auto const& id : spriteList) {
        OBJ_ATTR* sprite = &obj_buffer[id];

        if (!sprite->enabled) {
            counter++;
            continue;
        }

        spriteData[counter].x = (sprite->tile_start) * 8;
        spriteData[counter].y = 0;
        spriteData[counter].w = sprite->sx;
        spriteData[counter].h = sprite->sy;

        TLN_SetSpriteSet(counter, spriteset);
        TLN_SetSpritesetData(spriteset, counter, &spriteData[counter], 0, 0);
        TLN_SetSpritePicture(counter, counter);
        TLN_SetSpritePalette(counter, palettes[sprite->palette + 16]);
        if (sprite->affine && !sprite->rotate) {
            TLN_RESET_SPRITE_ROTATION_SAFE(counter);
            TLN_SetSpritePivot(counter, 0.5, 0.5);

            TLN_SetSpriteScaling(counter, sprite->scalex, sprite->scaley);
            float scaleOffset = (1 + sprite->doubleSize) / 2.0;

            TLN_SetSpritePosition(
                counter, sprite->x + offsetx + sprite->sx * scaleOffset,
                sprite->y + offsety + sprite->sy * scaleOffset);
        } else if (sprite->affine && sprite->rotate &&
                   TLN_HAS_SPRITE_ROTATION_SAFE()) {
            TLN_SetSpritePivot(counter, 0.5, 0.5);
            TLN_SetSpriteScaling(counter, sprite->scalex, sprite->scaley);

            float scaleOffset = (sprite->doubleSize) ? 0 : 0.5;

            const int sprite_x = sprite->x + offsetx - sprite->sx * scaleOffset;
            const int sprite_y = sprite->y + offsety - sprite->sy * scaleOffset;

            TLN_SetSpritePosition(counter, sprite_x, sprite_y);
            TLN_SET_SPRITE_ROTATION_SAFE(counter, sprite->angle);
        } else {
            TLN_RESET_SPRITE_ROTATION_SAFE(counter);
            TLN_SetSpritePosition(counter, sprite->x + offsetx,
                                  sprite->y + offsety);
            TLN_SetSpritePivot(counter, 0.0, 0.0);
            TLN_SetSpriteScaling(counter, 1.0, 1.0);
        }

        if (!(blendSprites || sprite->blend))
            TLN_SetSpriteBlendMode(counter, BLEND_NONE, 0);
        else
            TLN_SetSpriteBlendMode(
                counter, (blendSprites) ? BLEND_CUSTOM : BLEND_MIX75, 0);

        TLN_SetSpriteFlags(
            counter,
            (sprite->flipx * FLAG_FLIPX) + (sprite->flipy * FLAG_FLIPY) +
                ((sprite->priority == 1) * FLAG_PRIORITY) + FLAG_MASKED);

        counter++;
    }
}

void loadPalette(int palette, int index, const void* src, int count) {
    for (int i = 0; i < (int)count; i++) {
        u16 r = (u16)((u16*)src)[i];
        ColorSplit c = ColorSplit(r);
        int p = palette + i / 16;

        TLN_SetPaletteColor(palettes[p], index + i % 16, c.r, c.g, c.b);
    }

    if (palette == 0 && index == 0 && !gradientEnabled) {
        u16 r = (u16)((u16*)src)[0];
        ColorSplit c = ColorSplit(r);

        TLN_SetBGColor(c.r, c.g, c.b);
    }
}

void loadTiles(int tileset, int index, const void* src, int count) {
    u8 tmp[8 * 8];
    u8* source = (u8*)src;
    for (int ii = 0; ii < count; ii++) {
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 4; j++) {
                tmp[i * 8 + j * 2] = source[ii * 32 + i * 4 + j] & 0xf;
                tmp[i * 8 + j * 2 + 1] =
                    (source[ii * 32 + i * 4 + j] >> 4) & 0xf;
            }
        }

        TLN_SetTilesetPixels(tilesets[tileset], index + ii, tmp, 8);
    }
}

void loadTiles8(int tileset, int index, const void* src, int count) {
    u8* source = (u8*)src;
    for (int i = 0; i < count; i++) {
        TLN_SetTilesetPixels(tilesets[tileset], index + i, (u8*)&source[64 * i],
                             8);
    }
}

void loadTilemap(int tilemap, int index, const void* src, int count) {
    u16* source = (u16*)src;

    for (int i = 0; i < count; i++) {
        setTile(tilemap, (i + index) % TILEMAP_SIZE, (i + index) / TILEMAP_SIZE,
                source[i]);
    }
}

void clearTilemap(int tilemap) {
    for (int i = 0; i < TILEMAP_SIZE; i++) {
        for (int j = 0; j < TILEMAP_SIZE; j++) {
            setTile(tilemap, i, j, 0);
        }
    }
}

void sprite_hide(OBJ_ATTR* sprite) {
    if (sprite->enabled) {
        sprite->enabled = false;
    }
};

void sprite_unhide(OBJ_ATTR* sprite, int mode) { sprite->enabled = true; };

void sprite_set_pos(OBJ_ATTR* sprite, int x, int y) {
    sprite->x = x;
    sprite->y = y + spriteVOffset;
};

void sprite_set_attr(OBJ_ATTR* sprite, int shape, int size, int tile_start,
                     int palette, int priority) {
    sprite->sx = sizeTable[shape][size][0];
    sprite->sy = sizeTable[shape][size][1];

    sprite->tile_start = tile_start;
    sprite->palette = palette;
    sprite->priority = priority + 1;
    sprite->affine = false;
    sprite->doubleSize = false;
    sprite->flipx = false;
    sprite->flipy = false;
    sprite->rotate = false;
    sprite->scalex = 1;
    sprite->scaley = 1;
    sprite->blend = false;
};

void setLayerScroll(int layer, int x, int y) {
    if (!layerInfo[layer].enabled)
        return;

    TLN_SetLayerPosition(layerInfo[layer].id, x - offsetx, y - offsety);
}

void loadSpriteTiles(int index, const void* src, int lengthX, int lengthY) {

    u8* source = (u8*)src;
    const int count = lengthX * lengthY;
    uint8_t tmp[8 * 8 * count];
    for (int ii = 0; ii < count; ii++) {
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 4; j++) {
                tmp[ii * 64 + i * 8 + j * 2] =
                    source[ii * 32 + i * 4 + j] & 0xf;
                tmp[ii * 64 + i * 8 + j * 2 + 1] =
                    (source[ii * 32 + i * 4 + j] >> 4) & 0xf;
            }
        }
    }

    TLN_SpriteData d;
    memcpy(d.name, "tile", 5);
    d.w = 8;
    d.h = 8;

    int counter = 0;
    for (int i = 0; i < lengthY; i++) {
        for (int j = 0; j < lengthX; j++) {
            d.x = 8 * (j + index);
            d.y = 8 * i;

            TLN_SetSpritesetData(spriteset, 127, &d, &tmp[counter * 64], 8);
            counter++;
        }
    }
}

void loadSpriteTilesPartial(int index, const void* src, int tx, int ty,
                            int lengthX, int lengthY, int rowLength) {
    u8* source = (u8*)src;
    const int count = lengthX * lengthY;
    uint8_t tmp[8 * 8 * count];
    for (int ii = 0; ii < count; ii++) {
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 4; j++) {
                tmp[ii * 64 + i * 8 + j * 2] =
                    source[ii * 32 + i * 4 + j] & 0xf;
                tmp[ii * 64 + i * 8 + j * 2 + 1] =
                    (source[ii * 32 + i * 4 + j] >> 4) & 0xf;
            }
        }
    }

    TLN_SpriteData d;
    memcpy(d.name, "tile", 5);
    d.w = 8;
    d.h = 8;

    int counter = 0;
    for (int i = 0; i < lengthY; i++) {
        for (int j = 0; j < lengthX; j++) {
            d.x = 8 * (j + index + tx);
            d.y = 8 * (i + ty);

            TLN_SetSpritesetData(spriteset, 127, &d, &tmp[counter * 64], 8);
            counter++;
        }
    }
}

void enableBlend(int info) {

    int top = info & 0x3f;
    // int bot = (info >> 8) & 0x3f;
    int bot = 0;
    // int mode = (info >> 6) & 0x3;

    for (int i = 0; i < 4; i++) {
        if (((top >> i) & 0b1)) {
            TLN_SetLayerBlendMode(layerInfo[i].id, BLEND_MIX75, 0);
        } else if (((bot >> i) & 0b1))
            TLN_SetLayerBlendMode(layerInfo[i].id, BLEND_MIX, 0);
        else {
            // log("disabling blend for layer: " + std::to_string(i));
            TLN_SetLayerBlendMode(layerInfo[i].id, BLEND_NONE, 0);
        }
    }

    blendSprites = (top >> 4) & 0b1;

    blendInfo = info;
}

void setTiles(int tilemap, int index, int count, u32 value) {
    if (count == 32 * 32)
        count = TILEMAP_SIZE * TILEMAP_SIZE;

    TLN_Tile t = new Tile();
    t->value = value;

    for (int i = index; i < count + index; i++) {
        TLN_SetTilemapTile(tilemaps[tilemap], (i / TILEMAP_SIZE),
                           (i % TILEMAP_SIZE), t);
    }

    delete t;
}

void clearSprites(int n) {
    for (int counter = 0; counter < n; counter++) {
        sprite_hide(&obj_buffer[counter]);
    }
}

void setPaletteColor(int palette, int index, u16 color, int count) {
    ColorSplit c = ColorSplit(color);
    if (palette == 0 && index == 0)
        TLN_SetBGColor(c.r, c.g, c.b);

    for (int i = 0; i < count; i++) {
        TLN_SetPaletteColor(palettes[palette], index + i, c.r, c.g, c.b);
    }
}

void clearSpriteTiles(int index, int lengthX, int lengthY) {
    uint8_t tmp[8 * 8];
    memset(tmp, 0, 8 * 8);

    TLN_SpriteData d;
    memcpy(d.name, "clear", 6);
    d.w = 8;
    d.h = 8;

    for (int i = 0; i < lengthY; i++) {
        d.y = 8 * i;
        for (int j = 0; j < lengthX; j++) {
            d.x = 8 * (j + index);

            TLN_SetSpritesetData(spriteset, 127, &d, tmp, 8);
        }
    }
}

void sprite_enable_mosaic(OBJ_ATTR* sprite) {}

void sprite_set_size(OBJ_ATTR* sprite, FIXED size, int aff_id) {
    float n = fx2float(size);
    if (size == 0 || n == 0)
        return;

    float scale = 1 / (n);

    sprite->scalex = scale;
    sprite->scaley = scale;
}

void sprite_rotscale(OBJ_ATTR* sprite, FIXED sizex, FIXED sizey, int angle,
                     int aff_id) {
    sprite->scalex = fx2float(sizex);
    sprite->scaley = fx2float(sizey);
    sprite->angle = ((float)angle * 360.0f) /
                    65536.0f; // Convert from fixed-point to degrees
    sprite->affine = true;
    sprite->rotate = true;
    // sprite->rotate = (sprite->angle != 0.0f);
};

void sprite_enable_affine(OBJ_ATTR* sprite, int affineId, bool doubleSize) {
    sprite->affine = true;
    sprite->doubleSize = doubleSize;
}

void sprite_enable_flip(OBJ_ATTR* sprite, bool flipX, bool flipY) {
    sprite->flipx = flipX;
    sprite->flipy = flipY;
}

extern "C" {

void posprintf(char* buff, const char* str, ...) {

    va_list args;
    va_start(args, str);
    vsnprintf(buff, 64, str, args);
    va_end(args);
}
}

u32 Sqrt(u32 n) { return std::sqrt(n); }

void color_fade_palette(int palette, int index, const COLOR* src, COLOR color,
                        int count, u32 alpha) {
    u16 temp[count];

    color_fade(temp, src, color, count, alpha);

    int remaining = index + count;

    int counter = 0;
    while (remaining > 0) {

        int m = remaining % 16;
        if (m) {

            loadPalette(palette, index, temp, m);

            remaining -= m;
        } else if (counter == 0 && index) {
            loadPalette(palette, index, &temp[0], 16 - index);
            remaining -= 16;
        } else {
            loadPalette(palette + counter, 0, &temp[count - remaining], 16);
            remaining -= 16;
        }

        counter++;
    }
};

void color_fade(COLOR* dst, const COLOR* src, COLOR color, int count,
                u32 alpha) {
    if (!count)
        return;

    u32 clra, parta, part;
    const u32 rbmask = (RED_MASK | BLUE_MASK), gmask = GREEN_MASK;
    const u32 rbhalf = 0x4010, ghalf = 0x0200;

    // precalc color B parts
    u32 partb_rb = color & rbmask, partb_g = color & gmask;

    for (int ii = 0; ii < count; ii++) {
        clra = src[ii];

        // Red and blue
        parta = clra & rbmask;
        part = (partb_rb - parta) * alpha + parta * 32 + rbhalf;
        color = (part / 32) & rbmask;

        // Green
        parta = clra & gmask;
        part = (partb_g - parta) * alpha + parta * 32 + ghalf;
        color |= (part / 32) & gmask;

        dst[ii] = color;
    }
}

void color_blend(COLOR* dst, const COLOR* srca, const COLOR* srcb, int nclrs,
                 u32 alpha) {
    if (!nclrs)
        return;

    int ii;
    u32 clra, clrb, clr;
    u32 parta, partb, part;
    const u32 rbmask = (RED_MASK | BLUE_MASK), gmask = GREEN_MASK;
    const u32 rbhalf = 0x4010, ghalf = 0x0200;

    for (ii = 0; ii < nclrs; ii++) {
        clra = srca[ii];
        clrb = srcb[ii];

        // Red and blue
        parta = clra & rbmask;
        partb = clrb & rbmask;
        part = (partb - parta) * alpha + parta * 32 + rbhalf;
        clr = (part / 32) & rbmask;

        // Green
        parta = clra & gmask;
        partb = clrb & gmask;
        part = (partb - parta) * alpha + parta * 32 + ghalf;
        clr |= (part / 32) & gmask;

        dst[ii] = clr;
    }
}

void color_adj_MEM(COLOR* dst, const COLOR* src, u32 count, u32 bright) {
    u32 ii, clr;
    int rr, gg, bb;

    bright >>= 3;
    for (ii = 0; ii < count; ii++) {
        clr = src[ii];

        rr = ((clr) & 31) + bright;
        gg = ((clr >> 5) & 31) + bright;
        bb = ((clr >> 10) & 31) + bright;

        dst[ii] = RGB15(bf_clamp(rr, 5), bf_clamp(gg, 5), bf_clamp(bb, 5));
    }
}

void color_adj_brightness(int palette, int index, const COLOR* src, u32 count,
                          FIXED bright) {
    COLOR dst[count];

    color_adj_MEM(dst, src, count, bright);

    loadPalette(palette, index, dst, count);
}

void clr_grayscale(COLOR* dst, const COLOR* src, u32 nclrs) {
    u32 ii;
    u32 clr, gray, rr, gg, bb;

    for (ii = 0; ii < nclrs; ii++) {
        clr = *src++;

        // Do RGB conversion in .8 fixed point
        rr = ((clr) & 31) * 0x4C;       // 29.7%
        gg = ((clr >> 5) & 31) * 0x96;  // 58.6%
        bb = ((clr >> 10) & 31) * 0x1E; // 11.7%
        gray = (rr + gg + bb + 0x80) >> 8;

        *dst++ = RGB15(gray, gray, gray);
    }
}

void clr_rgbscale(COLOR* dst, const COLOR* src, uint nclrs, COLOR clr) {
    uint ii;
    u32 rr, gg, bb, scale, gray;

    // Normalize target color
    rr = (clr) & 31;
    gg = (clr >> 5) & 31;
    bb = (clr >> 10) & 31;

    scale = max(max(rr, gg), bb);
    if (scale == 0) // Can't scale to black : use gray instead
    {
        clr_grayscale(dst, src, nclrs);
        return;
    }

    scale = ((1 << 16) / scale) & 0xffff;

    rr *= scale; // rr, gg, bb -> 0.16f
    gg *= scale;
    bb *= scale;

    for (ii = 0; ii < nclrs; ii++) {
        clr = *src++;

        // Grayscaly in 5.8f
        gray = ((clr) & 31) * 0x4C;        // 29.7%
        gray += ((clr >> 5) & 31) * 0x96;  // 58.6%
        gray += ((clr >> 10) & 31) * 0x1E; // 11.7%

        // Match onto color vector
        dst[ii] = RGB15(rr * gray >> 24, gg * gray >> 24, bb * gray >> 24);
    }
};

void clearSpriteTile(int index, int tx, int ty, int width) {
    uint8_t tmp[8 * 8];
    memset(tmp, 0, 64);

    TLN_SpriteData d;
    memcpy(d.name, "tile", 5);
    d.w = 8;
    d.h = 8;

    d.x = 8 * (tx + index);
    d.y = 8 * ty;

    TLN_SetSpritesetData(spriteset, 127, &d, tmp, 8);
};

void clearTiles(int tileset, int index, int count) {
    u8 tmp[8 * 8];

    memset(tmp, 0, 64);
    for (int ii = 0; ii < count; ii++) {

        TLN_SetTilesetPixels(tilesets[tileset], index + ii, tmp, 8);
    }
};

void clearTilemapEntries(int tilemap, int index, int count) {
    // log(std::to_string(index) + " " + std::to_string(count));
    for (int i = index; i < count + index; i++) {
        setTile(tilemap, i % 32, i / 32, 0);
    }
}

void stopDMA() {};
void setMosaic(int sx, int sy) {
    for (int i = 0; i < 4; i++) {
        if (!sx || !sy)
            TLN_DisableLayerMosaic(i);
        else
            TLN_SetLayerMosaic(i, sx, sy);
    }
};

void sleep() {};
void rumble_set_state(int n) {};

void onVBlank() {
    if (canDraw) {
        canDraw = 0;

        scene->draw();
    }

    frameCounter++;
};

void onHBlank(int line) {
    if (!gradientEnabled) {
        return;
    }

    ColorSplit c = ColorSplit(gradientTable[line]);
    TLN_SetBGColor(c.r, c.g, c.b);
}

void toggleHBlank(bool state) {
    if (state)
        gradientEnabled = true;
}

s16 sinLut(int angle) {
    float r = (float)angle * ((3.1459) / 256);
    int adj = (int)((float)(1 << 12) * (sin(r)));
    return (adj & 0xffff);
}

COLOR rgb8to5(u32* c) {
    int b = (*c >> (16 + 3)) & 0x1f;
    int g = (*c >> (8 + 3)) & 0x1f;
    int r = (*c >> (0 + 3)) & 0x1f;

    return r | (g << 5) | (b << 10);
}

void savePalette(COLOR* dst) {
    for (int i = 0; i < 32; i++)
        for (int j = 0; j < 16; j++)
            *dst++ = rgb8to5((u32*)TLN_GetPaletteData(palettes[i], j));
}

void color_fade_fast(int palette, int index, const COLOR* src, COLOR color,
                     int count, u32 alpha) {
    u16 temp[count];

    color_fade(temp, src, color, count, alpha);

    if (count > 16) {
        int n = count / 16;

        for (int i = 0; i < (n); i++)
            loadPalette(palette + i, index, &temp[i * 16], 16);
    } else {
        loadPalette(palette, index, temp, count);
    }
}

void mirrorPalettes(int index, int count) {
    for (int i = 0; i < count; i++) {
        COLOR temp[16];

        temp[0] = 0;

        for (int j = index; j < 16; j++)
            temp[j] = rgb8to5((u32*)TLN_GetPaletteData(palettes[i + 16], j));

        loadPalette(i, 0, temp, 16);
        // for(int j = 0; j < 16; j++){
        //     setPaletteColor(i, j, rgb8to5((u32*)
        //     TLN_GetPaletteData(palettes[i + 16], j)), 1);
        // }
    }
}

void sprite_enable_blend(OBJ_ATTR* sprite) { sprite->blend = true; }

void addColorToPalette(int palette, int index, COLOR color, int count) {

    COLOR dst[count];

    for (int i = 0; i < count; i++)
        dst[i] =
            rgb8to5((u32*)TLN_GetPaletteData(palettes[palette], index + i)) +
            color;

    loadPalette(palette, index, dst, count);
}

uint8_t customBlend(uint8_t a, uint8_t b) { return (a + b + b + b + b) / 5; }

void enableLayerWindow(int layer, int x1, int y1, int x2, int y2, bool invert) {
    TLN_SetLayerWindow(layerInfo[layer].id, x1 + offsetx, y1 + offsety,
                       x2 + offsetx, y2 + offsety, invert);
}

void disableLayerWindow(int layer) {
    TLN_DisableLayerWindow(layerInfo[layer].id);
}

void showFPS() {
    if (savefile->settings.showFPS) {
        if (fpsHistory.size() > 9) {
            fpsHistory.pop_front();
        }

        fpsHistory.push_back(fps);

        float average = 0;

        for (const float value : fpsHistory)
            average += value;

        average /= fpsHistory.size();

        char buff[8];
        snprintf(buff, 8, "%.1f", average);

        const int width = 30 - 2 * (savefile->settings.aspectRatio == 1);
        aprintColor(buff, width - 4, 0, 1);
    }
}

void setSpriteMaskRegion(int shift) {
    const int top = offsety + 164 + shift;
    const int bot = offsety + shift;

    TLN_SetSpritesMaskRegion(top, bot);
}

#endif
