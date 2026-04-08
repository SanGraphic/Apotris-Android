#pragma once

#include <list>

#define PLATFORM_N3DS 6

#ifdef GBA

#include "liba_gba.h"
#include "maxmod.h"
#include "tonc.h"
#include "util/random.h"
#include "whisky.h"

#define PLATFORM 0
#endif

#ifdef PC

#include "liba_pc.h"
#include "liba_sdl_audio.hpp"
#include "liba_window.h"

#define PLATFORM 1
#endif

#ifdef MM
#include "liba_mm.h"
#define PLATFORM 2
#endif

#ifdef WEB

#include "liba_sdl_audio.hpp"
#include "liba_web.h"
#define PLATFORM 3

#endif

#ifdef SWITCH

#define PLATFORM 4
#include "liba_sdl_audio.hpp"
#include "liba_switch.h"

#endif

#ifdef PORTMASTER

#define PLATFORM 5
#include "liba_portmaster.h"
#include "liba_sdl_audio.hpp"

#endif

#ifdef N3DS

#define PLATFORM PLATFORM_N3DS
#include "liba_n3ds.h"

#endif

#define MAX_REPLAY_SIZE 4096

extern void onVBlank();

extern OBJ_ATTR obj_buffer[128];

enum {
    ShapeSquare,
    ShapeWide,
    ShapeTall,
};

const int sizeTable[3][4][2] = {
    {{8, 8}, {16, 16}, {32, 32}, {64, 64}}, // Square
    {{16, 8}, {32, 8}, {32, 16}, {64, 32}}, // Wide
    {{8, 16}, {8, 32}, {16, 32}, {32, 64}}, // Tall
};

class Songs {
public:
    std::list<int> menu;
    std::list<int> game;
};

extern Songs songs;

extern int spriteVOffset;

extern void toggleBG(int layer, bool state);
extern void toggleSprites(bool state);
extern void clearSprites(int count);

extern void buildBG(int layer, int cbb, int sbb, int size, int prio, int mos);

extern void platformInit();

extern void platformDeinit();
extern bool closed();

extern void vsync();
extern void showSprites(int count);

extern void loadPalette(int palette, int index, const void* src, int count);
extern void loadTiles(int tileset, int index, const void* src, int count);
extern void loadTiles8(int tileset, int index, const void* src, int count);
extern void loadTilemap(int tilemap, int index, const void* src, int count);

extern void loadSpriteTiles(int index, const void* src, int lengthX,
                            int lengthY);
extern void loadSpriteTilesPartial(int index, const void* src, int tx, int ty,
                                   int lengthX, int lengthY, int rowLength);

extern void setPaletteColor(int palette, int index, u16 color, int count);

extern void sprite_set_id(OBJ_ATTR* sprite, int id);
extern void sprite_hide(OBJ_ATTR* sprite);
extern void sprite_unhide(OBJ_ATTR* sprite, int mode);
extern void sprite_set_pos(OBJ_ATTR* sprite, int x, int y);
extern void sprite_set_attr(OBJ_ATTR* sprite, int shape, int size,
                            int tile_start, int palette, int priority);
extern void sprite_enable_mosaic(OBJ_ATTR* sprite);
extern void sprite_enable_affine(OBJ_ATTR* sprite, int affineId,
                                 bool doubleSize);
extern void sprite_enable_blend(OBJ_ATTR* sprite);
extern void sprite_enable_flip(OBJ_ATTR* sprite, bool flipX, bool flipY);
extern void sprite_set_size(OBJ_ATTR* sprite, FIXED size, int aff_id);
extern void sprite_rotscale(OBJ_ATTR* sprite, FIXED sizex, FIXED sizey,
                            int angle, int aff_id);

extern void setLayerScroll(int layer, int x, int y);

extern void clearTilemap(int tilemap);
extern void clearTilemapEntries(int tilemap, int index, int count);
extern void clearTiles(int tileset, int index, int count);
extern void clearSpriteTiles(int index, int lengthX, int lengthY);
extern void clearSpriteTile(int index, int tx, int ty, int width);
extern void setTiles(int tilemap, int index, int count, u32 value);

extern void enableBlend(int info);

extern void color_fade_palette(int palette, int index, const COLOR* src,
                               COLOR color, int count, u32 alpha);
extern void color_fade(COLOR* dst, const COLOR* src, COLOR color, int count,
                       u32 alpha);
extern void color_blend(COLOR* dst, const COLOR* srca, const COLOR* srcb,
                        int nclrs, u32 alpha);

extern void color_adj_brightness(int palette, int index, const COLOR* src,
                                 u32 count, FIXED bright);
extern void color_adj_MEM(COLOR* dst, const COLOR* src, u32 count, u32 bright);

extern void addColorToPalette(int palette, int index, COLOR color, int count);

extern void enableLayerWindow(int layer, int x1, int y1, int x2, int y2,
                              bool invert);

extern void disableLayerWindow(int layer);

extern void sleep();

extern void sfx(int sound);

extern void sfxRate(int sound, float rate);

extern void stopDMA();
extern void toggleHBlank(bool state);

extern void setMosaic(int sx, int sy);

extern void setMusicTempo(int tempo);
extern void setMusicVolume(int volume);
extern void startSong(int song, bool loop);
extern void stopSong();
extern void pauseSong();
extern void resumeSong();
extern std::string getSongName(int song);

extern void loadSavefile();
extern void saveSavefile();
#ifdef __APPLE__
extern const char* homeDir;
extern std::string savefileDir();
#endif

extern s16 sinLut(int angle);

extern void savePalette(COLOR* dst);

extern void mirrorPalettes(int index, int count);

extern void toggleRendering(bool state);
extern void initRumble();
extern void rumbleOutput(uint16_t strength);
extern void rumbleStop();

extern void quit();

extern void deinitialize();
extern void refreshWindowSize();

extern float fps;
