#pragma once

#include <cstddef>

extern "C" {
#include <3ds/types.h>
}

class SpriteAttributes;

namespace tng {

constexpr size_t SPRITE_TILE_COUNT = 1024;
constexpr size_t SPRITE_OBJ_COUNT = 128;

// 4bpp 8x8 tile
struct ImageTile {
    u32 rows[8];
};

extern ImageTile sprite_tile_images[SPRITE_TILE_COUNT];

extern bool sprite_tile_dirty_flag[SPRITE_TILE_COUNT];

extern SpriteAttributes* sprite_obj_attrs;

extern bool sprite_obj_dirty_flag[SPRITE_OBJ_COUNT];

// GBA has 64 KB of VRAM for this?
constexpr size_t BG_TILEMAP_STORAGE_SIZE = 64 * 1024;

constexpr size_t BG_SCREENBLOCK_TILEMAP_TILE_COUNT = 1024;
constexpr size_t BG_SCREENBLOCK_TILEMAP_COUNT =
    BG_TILEMAP_STORAGE_SIZE / 2 / BG_SCREENBLOCK_TILEMAP_TILE_COUNT;

constexpr size_t BG_CHARBLOCK_TILESET_COUNT = 4;
constexpr size_t BG_CHARBLOCK_TILESET_TILE_COUNT =
    BG_TILEMAP_STORAGE_SIZE / BG_CHARBLOCK_TILESET_COUNT / sizeof(ImageTile);

using BgScreenblockTilemap = u16[BG_SCREENBLOCK_TILEMAP_TILE_COUNT];
using BgScreenblockAll = BgScreenblockTilemap[BG_SCREENBLOCK_TILEMAP_COUNT];
extern BgScreenblockAll& bg_screenblocks;

using BgCharblockTileset = ImageTile[BG_CHARBLOCK_TILESET_TILE_COUNT];
using BgCharblockAll = BgCharblockTileset[BG_CHARBLOCK_TILESET_COUNT];
extern BgCharblockAll& bg_charblocks;

extern bool* const bg_screenblock_fill_screen_p;

constexpr size_t BG_LAYERS_COUNT = 4;

} /* namespace tng */
