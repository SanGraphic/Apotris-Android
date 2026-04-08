#include "platform.hpp"

#include "n3ds/renderer_p.hpp"

#include "n3ds/citro3d++.hpp"
#include "n3ds/ctru++.hpp"
#include "n3ds/maybe_uninit.hpp"

#include "def.h"

#include <cassert>
#include <cinttypes>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdio>

#include <array>
#include <set>

#include <malloc.h>

#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>

#ifdef DEBUG_UI
#include "imgui_memory_editor.h"
#include <imgui.h>
#endif

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

// Tiling Next Gen
namespace tng {

class LayerInfo {
public:
    int id = -1;
    bool enabled = true;
    int priority = -1;
    int sbb = 0;
    int cbb = 0;
    int scroll_x = 0;
    int scroll_y = 0;
};

static LayerInfo layerInfo[4];

enum BlendMode {
    BLEND_OFF = 0,
    BLEND_STD = 1,
    BLEND_WHITE = 2,
    BLEND_BLACK = 3,
};

std::array<bool, BG_LAYERS_COUNT> blend_top_bg = {};
bool blend_top_sprites = false;
bool blend_top_backdrop = false;
std::array<bool, BG_LAYERS_COUNT> blend_bottom_bg = {};
bool blend_bottom_sprites = false;
bool blend_bottom_backdrop = false;

BlendMode blend_mode = BLEND_OFF;

// Mosaic size - 1
u8 bg_mosaic_x = 0;
u8 bg_mosaic_y = 0;

enum PaletteType {
    PALETTE_BG = 0u,
    PALETTE_SPRITE = 1u,
};

// The palettes now use 3DS RGB5A1 format.
static std::array<std::array<std::array<u16, 16>, 16>, 2> palettes = {};

static std::array<std::array<u16, 16>, 16>& palettes_bg = palettes[PALETTE_BG];
static std::array<std::array<u16, 16>, 16>& palettes_sprite =
    palettes[PALETTE_SPRITE];

constexpr u16 color_rgb5a1_unsafe(u8 c_r, u8 c_g, u8 c_b, u8 c_a = 1) {
    return (c_r << 11) | (c_g << 6) | (c_b << 1) | (c_a);
}

constexpr u16 color_gba_to_tng(const u16 colorgb5) {
    const u8 c_r = colorgb5 & 0b0001'1111;
    const u8 c_g = (colorgb5 >> 5) & 0b0001'1111;
    const u8 c_b = (colorgb5 >> 10) & 0b0001'1111;
    const u8 c_a = 0xff;
    return color_rgb5a1_unsafe(c_r, c_g, c_b, c_a & 0x1);
}

constexpr u16 color_tng_to_gba(const u16 rgb5a1) {
    const u8 c_r = (rgb5a1 >> 11) & 0b0001'1111;
    const u8 c_g = (rgb5a1 >> 6) & 0b0001'1111;
    const u8 c_b = (rgb5a1 >> 1) & 0b0001'1111;
    const u8 c_a = rgb5a1 & 0x1;
    return RGB15(c_r, c_g, c_b);
}

constexpr void palette_gba_to_tng(const u16* src, u16* dst, size_t count) {
    for (; count > 0; count--, src++, dst++) {
        *dst = color_gba_to_tng(*src);
    }
}

constexpr void palette_tng_to_gba(const u16* src, u16* dst, size_t count) {
    for (; count > 0; count--, src++, dst++) {
        *dst = color_tng_to_gba(*src);
    }
}

ImageTile sprite_tile_images[SPRITE_TILE_COUNT];

bool sprite_tile_dirty_flag[SPRITE_TILE_COUNT] = {};

// Using the same global sprite attributes
SpriteAttributes* sprite_obj_attrs = obj_buffer;

bool sprite_obj_dirty_flag[SPRITE_OBJ_COUNT] = {};

static u8 bg_storage[BG_TILEMAP_STORAGE_SIZE] = {};

BgScreenblockAll& bg_screenblocks = *(BgScreenblockAll*)&bg_storage;
BgCharblockAll& bg_charblocks = *(BgCharblockAll*)&bg_storage;
static_assert(sizeof(bg_screenblocks) == sizeof(bg_storage));
static_assert(sizeof(bg_charblocks) == sizeof(bg_storage));

std::array<bool, std::extent_v<BgScreenblockAll>> bg_screenblock_fill_screen{};
bool* const bg_screenblock_fill_screen_p = bg_screenblock_fill_screen.data();

} /* namespace tng */

using tng::LayerInfo;
using tng::layerInfo;

namespace tex_util {

// Fill texture with noise to help identify undrawn regions
void noise_fill_rgba5551(C3D_Tex& tex) {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    if (tex.fmt != GPU_RGBA5551) {
        fprintf(stderr, "noise_fill_rgba5551: Unexpected texture format\n");
        return;
    }

    const bool isVRAM = (uintptr_t)tex.data >= OS_VRAM_VADDR &&
                        (uintptr_t)tex.data < OS_VRAM_VADDR + OS_VRAM_SIZE;

    ctru::linear::ptr<u16[]> vramBuf;
    const size_t size = tex.width * tex.height * sizeof(u16);
    u16* p;
    if (isVRAM) {
        vramBuf = ctru::linear::make_ptr_nothrow_for_overwrite<u16[]>(
            tex.width * tex.height);
        p = vramBuf.get();
    } else {
        p = (u16*)tex.data;
    }
    u16* const end = p + tex.width * tex.height;
    for (; p < end; p++) {
        int r = rand();
        *p = r | 0x1;
    }

    if (isVRAM) {
        GX_TextureCopy((u32*)vramBuf.get(), GX_BUFFER_DIM(tex.width * 2, 0),
                       (u32*)tex.data, GX_BUFFER_DIM(tex.width * 2, 0), size,
                       GX_TRANSFER_RAW_COPY(1));
        gspWaitForPPF();
    }
}

// Stuff for handing Morton swizzling:

static constexpr u8 tex_swizzle_table[64] = {
    0x00, 0x01, 0x04, 0x05, 0x10, 0x11, 0x14, 0x15, 0x02, 0x03, 0x06,
    0x07, 0x12, 0x13, 0x16, 0x17, 0x08, 0x09, 0x0c, 0x0d, 0x18, 0x19,
    0x1c, 0x1d, 0x0a, 0x0b, 0x0e, 0x0f, 0x1a, 0x1b, 0x1e, 0x1f, 0x20,
    0x21, 0x24, 0x25, 0x30, 0x31, 0x34, 0x35, 0x22, 0x23, 0x26, 0x27,
    0x32, 0x33, 0x36, 0x37, 0x28, 0x29, 0x2c, 0x2d, 0x38, 0x39, 0x3c,
    0x3d, 0x2a, 0x2b, 0x2e, 0x2f, 0x3a, 0x3b, 0x3e, 0x3f,
};

/// Calculates the pixel offset of the start of the 8x8 tile this pixel is
/// located on.
constexpr size_t swizzled_px_block_offset(size_t x, size_t y, size_t width) {
    size_t tile_x = x / 8;
    size_t tile_y = y / 8;
    return tile_y * 8 * width + tile_x * 64;
}

/// Calculates the pixel offset from the start of the 8x8 tile this pixel is
/// located on. Probably slower than the LUT version.
constexpr size_t swizzled_px_intile_offset_calc(size_t x, size_t y) {
    return ((y & 0b100) << 3) | ((x & 0b100) << 2) | ((y & 0b10) << 2) |
           ((x & 0b10) << 1) | ((y & 0b1) << 1) | (x & 0b1);
}

/// Calculates the pixel offset from the start of the 8x8 tile this pixel is
/// located on, using a lookup table.
constexpr size_t swizzled_px_intile_offset_lut(size_t x, size_t y) {
    size_t subtile_x = x % 8;
    size_t subtile_y = y % 8;
    return tex_swizzle_table[subtile_y * 8 + subtile_x];
}

constexpr size_t swizzled_px_intile_offset(size_t x, size_t y) {
    return swizzled_px_intile_offset_lut(x, y);
}

} /* namespace tex_util */

// Citro3D Renderer
namespace cr {

static int uLoc_projection;
static C3D_Mtx projection_top;
static C3D_Mtx projection_bottom;

struct SpriteVertexConst {
    struct VertSpritesheetTopleft {
        s16 x;
        s16 y;
    } tl;
    struct VertUV {
        s8 u;
        s8 v;
    } uv; // Texture UV simply using 0, 1 (even for y) with top-left corner
          // origin
};

struct SpriteVertexVar {
    struct VertSpritePos {
        s16 x;
        s16 y;
    } sprite_pos;
    struct VertSpriteSize {
        s8 w;
        s8 h;
    } sprite_size;
    struct VertFlip {
        // 0: no flip, 1: flip
        s8 flip_x;
        // 0: no flip, 1: flip
        s8 flip_y;
    } flip;
    float affine[4];
    struct {
        u8 blend;       // sprite-specific blending - 0: no blend, 1: blend
        u8 double_size; // change origin of affine transform - 0: normal, 1:
                        // double size
    };
};
static_assert(offsetof(SpriteVertexVar, flip) -
                  offsetof(SpriteVertexVar, sprite_size) ==
              2);
static_assert(offsetof(SpriteVertexVar, affine) -
                  offsetof(SpriteVertexVar, flip) ==
              2);

constexpr size_t SPRITES_TRIANGLE_PER_SPRITE = 2;
constexpr size_t SPRITES_VBO_VERTEX_PER_SPRITE = 4;
constexpr size_t SPRITES_TRIANGLE_COUNT =
    tng::SPRITE_OBJ_COUNT * SPRITES_TRIANGLE_PER_SPRITE;
constexpr size_t SPRITES_VBO_VERTEX_COUNT =
    tng::SPRITE_OBJ_COUNT * SPRITES_VBO_VERTEX_PER_SPRITE;
constexpr size_t SPRITES_IDX_MAX_VERTEX_COUNT = tng::SPRITE_OBJ_COUNT * 6;

constexpr size_t SPRITESHEET_NUM_SPRITES_X = 16;
constexpr size_t SPRITESHEET_NUM_SPRITES_Y = 8;
static_assert(SPRITESHEET_NUM_SPRITES_X * SPRITESHEET_NUM_SPRITES_Y ==
              tng::SPRITE_OBJ_COUNT);

constexpr size_t SPRITESHEET_SPRITE_W = 8 * 8;
constexpr size_t SPRITESHEET_SPRITE_H = 8 * 8;

constexpr size_t SPRITESHEET_W =
    SPRITESHEET_SPRITE_W * SPRITESHEET_NUM_SPRITES_X;
constexpr size_t SPRITESHEET_H =
    SPRITESHEET_SPRITE_H * SPRITESHEET_NUM_SPRITES_Y;

constexpr size_t SPRITESHEET_PIXEL_SIZE = 2; // RGBA5551

class SpritesRender {
    SpritesRender(const SpritesRender&) = delete;
    SpritesRender(SpritesRender&&) = delete;
    SpritesRender& operator=(const SpritesRender&) = delete;
    SpritesRender& operator=(SpritesRender&&) = delete;

    ctru::shader::Program m_program;

    C3D_AttrInfo m_attr_info;
    C3D_BufInfo m_buf_info;

    // 1024x512 texture for sprite graphics
    c3d::Tex m_spritesheet_tex{SPRITESHEET_W, SPRITESHEET_H, GPU_RGBA5551};

    // TODO: worth allocating on VRAM?
    ctru::linear::ptr<SpriteVertexConst[]> m_vbo_data_const{
        ctru::linear::make_ptr_nothrow_for_overwrite<SpriteVertexConst[]>(
            SPRITES_VBO_VERTEX_COUNT)};
    ctru::linear::ptr<SpriteVertexVar[]> m_vbo_data_var{
        ctru::linear::make_ptr_nothrow_for_overwrite<SpriteVertexVar[]>(
            SPRITES_VBO_VERTEX_COUNT)};

    ctru::linear::ptr<u16[]> m_indices_buffer{
        ctru::linear::make_ptr_nothrow_for_overwrite<u16[]>(
            SPRITES_IDX_MAX_VERTEX_COUNT)};
    // Pairs of start index and vertex count
    std::array<std::pair<u16, u16>, 4> m_priority_indices_count;

    inline void init_sprites_vbo();

public:
    explicit inline SpritesRender(ctru::shader::Program program) noexcept
        : m_program{std::move(program)} {
        C3D_TexSetFilter(m_spritesheet_tex.get(), GPU_NEAREST, GPU_NEAREST);
        C3D_TexSetWrap(m_spritesheet_tex.get(), GPU_CLAMP_TO_EDGE,
                       GPU_CLAMP_TO_EDGE);

#ifdef DEBUG_UI
        // debug
        tex_util::noise_fill_rgba5551(*m_spritesheet_tex.get());
#endif

        init_sprites_vbo();
    }

    const c3d::Tex& spritesheet_tex() const { return m_spritesheet_tex; }

    void prep_spritesheet() noexcept;

    void prep_sprite_vbo() noexcept;

    void draw_sprites(size_t priority) noexcept;
};

inline void SpritesRender::init_sprites_vbo() {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    // Configure attributes for use with the vertex shader
    AttrInfo_Init(&m_attr_info);
    AttrInfo_AddLoader(&m_attr_info, 0, GPU_SHORT,
                       2); // v0 = spritesheet topleft
    AttrInfo_AddLoader(&m_attr_info, 1, GPU_BYTE, 2); // v1 = uv
    AttrInfo_AddLoader(&m_attr_info, 2, GPU_SHORT,
                       2); // v2 = sprite position
    AttrInfo_AddLoader(&m_attr_info, 3, GPU_BYTE,
                       2);                             // v3 = sprite size
    AttrInfo_AddLoader(&m_attr_info, 4, GPU_BYTE, 2);  // v4 = flip
    AttrInfo_AddLoader(&m_attr_info, 5, GPU_FLOAT, 4); // v5 = affine
    AttrInfo_AddLoader(&m_attr_info, 6, GPU_BYTE,
                       2);              // v6.x = blend, v6.y = double size
    AttrInfo_AddFixed(&m_attr_info, 7); // v7 = blend factor

    const auto add_quad_const = [](cr::SpriteVertexConst* vbo, s16 sheet_x,
                                   s16 sheet_y, s8 u0, s8 u1, s8 v0,
                                   s8 v1) -> size_t {
        vbo[0] = {{sheet_x, sheet_y}, {u0, v0}};
        vbo[1] = {{sheet_x, sheet_y}, {u0, v1}};
        vbo[2] = {{sheet_x, sheet_y}, {u1, v1}};
        vbo[3] = {{sheet_x, sheet_y}, {u1, v0}};
        return 4;
    };

    size_t vi = 0;
    for (size_t i = 0; i < tng::SPRITE_OBJ_COUNT; i++) {
        int sheet_x =
            (i % cr::SPRITESHEET_NUM_SPRITES_X) * cr::SPRITESHEET_SPRITE_W;
        int sheet_y =
            (i / cr::SPRITESHEET_NUM_SPRITES_X) * cr::SPRITESHEET_SPRITE_H;

        vi +=
            add_quad_const(&m_vbo_data_const[vi], sheet_x, sheet_y, 0, 1, 0, 1);
    }

    // Configure buffers
    BufInfo_Init(&m_buf_info);
    BufInfo_Add(&m_buf_info, m_vbo_data_const.get(),
                sizeof(cr::SpriteVertexConst), 2, 0x10);
    BufInfo_Add(&m_buf_info, m_vbo_data_var.get(), sizeof(cr::SpriteVertexVar),
                5, 0x65432);
}

constexpr size_t BG_NUM_TILES_W = 32;
constexpr size_t BG_NUM_TILES_H = 32;

struct BgVertexConst {
    struct VertXY {
        s16 x;
        s16 y;
    } pos; // x, y top-left corner
};

struct BgVertexVar {
    s16 char_slot;
    struct VertFlip {
        // 0: no flip, 1: flip
        s8 flip_x;
        // 0: no flip, 1: flip
        s8 flip_y;
    } flip;
};

constexpr size_t BG_LAYER_TILES_COUNT = BG_NUM_TILES_W * BG_NUM_TILES_H;

using CharKey = s32;
constexpr CharKey CHARKEY_NULL = -1;

constexpr CharKey make_charkey(u16 char_idx, u8 palette) {
    return (char_idx & 0x3ff) | ((palette & 0x0f) << 12);
}

constexpr u16 charkey_to_char_idx(CharKey charkey) { return charkey & 0x3ff; }

constexpr u8 charkey_to_palette(CharKey charkey) {
    return (charkey & 0xf000) >> 12;
}

constexpr size_t BG_PIXEL_SIZE = 2; // RGBA5551

class BGRender {
    BGRender(const BGRender&) = delete;
    BGRender(BGRender&&) = delete;
    BGRender& operator=(const BGRender&) = delete;
    BGRender& operator=(BGRender&&) = delete;

    ctru::shader::Program m_program;
    int m_uLoc_gsh_projection;

    C3D_AttrInfo m_attr_info;
    C3D_BufInfo m_buf_info;

    std::array<std::array<CharKey, BG_NUM_TILES_W * BG_NUM_TILES_H>,
               tng::BG_LAYERS_COUNT>
        m_char_slot_arrays;
    // My testing shows that std::map seems to perform slightly better than
    // std::unordered_map.
    std::array<std::map<CharKey, u16>, tng::BG_LAYERS_COUNT> m_char_slot_maps;
    std::array<c3d::Tex, tng::BG_LAYERS_COUNT> m_char_sheet_tex{
        init_char_sheet_tex(), init_char_sheet_tex(), init_char_sheet_tex(),
        init_char_sheet_tex()};

    ctru::linear::ptr<BgVertexConst[]> m_vbo_data_const{
        ctru::linear::make_ptr_nothrow_for_overwrite<BgVertexConst[]>(
            BG_LAYER_TILES_COUNT * tng::BG_LAYERS_COUNT)};
    ctru::linear::ptr<BgVertexVar[]> m_vbo_data_var{
        ctru::linear::make_ptr_nothrow_for_overwrite<BgVertexVar[]>(
            BG_LAYER_TILES_COUNT * tng::BG_LAYERS_COUNT)};

    static inline c3d::Tex init_char_sheet_tex() {
        c3d::Tex tex{BG_NUM_TILES_W * 8, BG_NUM_TILES_H * 8, GPU_RGBA5551};
        C3D_TexSetFilter(tex.get(), GPU_NEAREST, GPU_NEAREST);
        C3D_TexSetWrap(tex.get(), GPU_REPEAT, GPU_REPEAT);

#ifdef DEBUG_UI
        // debug
        tex_util::noise_fill_rgba5551(*tex.get());
#endif

        return tex;
    }

    inline void init_bg_vbo();

public:
    explicit inline BGRender(ctru::shader::Program program) noexcept
        : m_program{std::move(program)},
          m_uLoc_gsh_projection{
              shaderInstanceGetUniformLocation(m_program.gsh(), "projection")} {
        for (auto& slot : m_char_slot_arrays) {
            slot.fill(CHARKEY_NULL);
        }

        init_bg_vbo();
    }

    constexpr const c3d::Tex& char_sheet_tex(size_t layer) const {
        return m_char_sheet_tex[layer];
    }

    constexpr cr::CharKey char_slot(size_t layer, size_t slot) const {
        return m_char_slot_arrays[layer][slot];
    }

    constexpr int uLoc_gsh_projection() const { return m_uLoc_gsh_projection; }

    void prep_charsheet() noexcept;
    void prep_vbo() noexcept;
    void draw_bg(int layer_idx, bool drawing_mosaic_tex) noexcept;
};

inline void BGRender::init_bg_vbo() {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    // Configure attributes for use with the vertex shader
    AttrInfo_Init(&m_attr_info);
    AttrInfo_AddLoader(&m_attr_info, 0, GPU_SHORT, 2); // v0=pos_topleft
    AttrInfo_AddLoader(&m_attr_info, 1, GPU_SHORT, 1); // v1=char_slot
    AttrInfo_AddLoader(&m_attr_info, 2, GPU_BYTE, 2);  // v2=flip(x/y)
    AttrInfo_AddFixed(&m_attr_info,
                      3); // v3=layer_props (xy = layer top-left)

    size_t vi = 0;
    for (size_t layer_idx = 0; layer_idx < tng::BG_LAYERS_COUNT; layer_idx++) {
        for (size_t j = 0; j < BG_NUM_TILES_H; j++) {
            for (size_t i = 0; i < BG_NUM_TILES_W; i++) {
                const s16 x = i * 8;
                const s16 y = j * 8;
                m_vbo_data_const[vi] = {{x, y}};
                vi += 1;
            }
        }
    }

    // Configure buffers
    BufInfo_Init(&m_buf_info);
    BufInfo_Add(&m_buf_info, m_vbo_data_const.get(), sizeof(BgVertexConst), 1,
                0x0);
    BufInfo_Add(&m_buf_info, m_vbo_data_var.get(), sizeof(BgVertexVar), 2,
                0x21);
}

constexpr size_t BG_MOSAIC_TEX_W = 256;
constexpr size_t BG_MOSAIC_TEX_H = 256;

constexpr s16 BG_MOSAIC_Y_OFFSET = -40;

struct BgMosaicTexVertexConst {
    struct VertXY {
        s16 x;
        s16 y;
    } pos;
    struct VertUV {
        float u;
        float v;
    } uv;
};

class BGMosaicRender {
    BGMosaicRender(const BGMosaicRender&) = delete;
    BGMosaicRender(BGMosaicRender&&) = delete;
    BGMosaicRender& operator=(const BGMosaicRender&) = delete;
    BGMosaicRender& operator=(BGMosaicRender&&) = delete;

    ctru::shader::Program m_program;

    C3D_AttrInfo m_attr_info;
    C3D_BufInfo m_buf_info;

    std::array<c3d::Tex, tng::BG_LAYERS_COUNT> m_tex{init_tex(), init_tex(),
                                                     init_tex(), init_tex()};
    std::array<C3D_FrameBuf, tng::BG_LAYERS_COUNT> m_tex_fb;

    // Vertex array of quads (4 vertices) shared by all layers.
    // #0: Normal layer (240px wide)
    // #1: Layer filling whole screen (400px wide wraparound)
    ctru::linear::ptr<BgMosaicTexVertexConst[]> m_vbo_data_const{
        ctru::linear::make_ptr_nothrow_for_overwrite<BgMosaicTexVertexConst[]>(
            4 * 2)};

    static inline c3d::Tex init_tex() {
        // This texture has to be on VRAM, because for whatever reason
        // C3D_FrameBufClear or GX_MemoryFill doesn't work on buffers on the
        // linear RAM, even though it was thought to be possible.
        c3d::Tex tex{BG_MOSAIC_TEX_W, BG_MOSAIC_TEX_H, GPU_RGBA5551, c3d::VRAM};
        C3D_TexSetFilter(tex.get(), GPU_NEAREST, GPU_NEAREST);
        C3D_TexSetWrap(tex.get(), GPU_REPEAT, GPU_REPEAT);

#ifdef DEBUG_UI
        // debug
        tex_util::noise_fill_rgba5551(*tex.get());
#endif

        return tex;
    }

    inline void init_bg_mosaic_vbo();

public:
    explicit inline BGMosaicRender(ctru::shader::Program program) noexcept
        : m_program{std::move(program)} {
        for (size_t layer_idx = 0; layer_idx < tng::BG_LAYERS_COUNT;
             layer_idx++) {
            c3d::Tex& tex = m_tex[layer_idx];
            C3D_FrameBuf& fb = m_tex_fb[layer_idx];

            C3D_FrameBufTex(&fb, tex.get(), GPU_TEXFACE_2D, 0);
            C3D_FrameBufDepth(&fb, nullptr, GPU_RB_DEPTH24);
        }

        init_bg_mosaic_vbo();
    }

    constexpr c3d::Tex const& tex(size_t layer) const noexcept {
        return m_tex[layer];
    }

    inline void make_current_target(size_t layer,
                                    int uLoc_gsh_projection) noexcept;

    inline void draw_tex(size_t layer) noexcept;
};

inline void BGMosaicRender::init_bg_mosaic_vbo() {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    // Configure attributes for use with the vertex shader
    AttrInfo_Init(&m_attr_info);
    AttrInfo_AddLoader(&m_attr_info, 0, GPU_SHORT, 2); // v0=pos
    AttrInfo_AddLoader(&m_attr_info, 1, GPU_FLOAT, 2); // v1=uv
    AttrInfo_AddFixed(&m_attr_info, 2);                // v2=mosaic

    const auto add_quad = [](BgMosaicTexVertexConst* vbo, s16 x, s16 y, s16 w,
                             s16 h, float u0, float u1, float v0,
                             float v1) -> size_t {
        const s16 l = x;
        const s16 t = y;
        const s16 r = x + w;
        const s16 b = y + h;
        vbo[0] = {{l, t}, {u0, v0}};
        vbo[1] = {{l, b}, {u0, v1}};
        vbo[2] = {{r, b}, {u1, v1}};
        vbo[3] = {{r, t}, {u1, v0}};
        return 4;
    };

    size_t vi = 0;

    // #0: Normal layer (240px wide)
    {
        const s16 x = 0;
        const s16 y = BG_MOSAIC_Y_OFFSET;
        constexpr s16 w = 240;
        constexpr s16 h = 240;
        constexpr float u0 = 0;
        constexpr float u1 = 240.0f / BG_MOSAIC_TEX_W;
        constexpr float v0 = 0;
        constexpr float v1 = -240.f / BG_MOSAIC_TEX_H;
        vi += add_quad(&m_vbo_data_const[vi], x, y, w, h, u0, u1, v0, v1);
    }

    // #1: Layer filling whole screen (400px wide wraparound)
    {
        const s16 x = (240 - 400) / 2;
        const s16 y = BG_MOSAIC_Y_OFFSET;
        constexpr s16 w = 400;
        constexpr s16 h = 240;
        constexpr float u0 = (240.f - 400.f) * .5f / BG_MOSAIC_TEX_W;
        constexpr float u1 = ((240.f - 400.f) * .5f + 400.f) / BG_MOSAIC_TEX_W;
        constexpr float v0 = 0;
        constexpr float v1 = -240.f / BG_MOSAIC_TEX_H;
        vi += add_quad(&m_vbo_data_const[vi], x, y, w, h, u0, u1, v0, v1);
    }

    // Configure buffers
    BufInfo_Init(&m_buf_info);
    BufInfo_Add(&m_buf_info, m_vbo_data_const.get(),
                sizeof(BgMosaicTexVertexConst), 2, 0x10);
}

struct VertexGradient {
    struct {
        s16 x;
        s16 y;
    };
    struct {
        s8 r;
        s8 g;
        s8 b;
    };
};

constexpr size_t GRADIENT_LINES_COUNT = 160;

class GradientRender {
    GradientRender(const GradientRender&) = delete;
    GradientRender(GradientRender&&) = delete;
    GradientRender& operator=(const GradientRender&) = delete;
    GradientRender& operator=(GradientRender&&) = delete;

    ctru::shader::Program m_program;

    C3D_AttrInfo m_attr_info;
    C3D_BufInfo m_buf_info;

    ctru::linear::ptr<VertexGradient[]> m_vbo_data{
        ctru::linear::make_ptr_nothrow_for_overwrite<VertexGradient[]>(
            GRADIENT_LINES_COUNT * 6)};

    size_t m_gradient_strip_count;

    inline void init_gradient_vbo();

public:
    explicit inline GradientRender(ctru::shader::Program program) noexcept
        : m_program{std::move(program)} {
        init_gradient_vbo();
    }

    void prep_vbo(bool screen_zoom_fit) noexcept;

    void draw() noexcept;
};

inline void GradientRender::init_gradient_vbo() {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    // Configure attributes for use with the vertex shader
    AttrInfo_Init(&m_attr_info);
    AttrInfo_AddLoader(&m_attr_info, 0, GPU_SHORT, 2); // v0 = pos
    AttrInfo_AddLoader(&m_attr_info, 1, GPU_BYTE, 3);  // v1 = color

    // Configure buffers
    BufInfo_Init(&m_buf_info);
    BufInfo_Add(&m_buf_info, m_vbo_data.get(), sizeof(VertexGradient), 2, 0x10);
}

constexpr size_t SCREEN_TARGET_TEX_W = 512;
constexpr size_t SCREEN_TARGET_TEX_H = 1024;

struct BgScreenTargetVertexConst {
    struct VertXY {
        s16 x;
        s16 y;
    } pos;
    struct VertUV {
        float u;
        float v;
    } uv;
};

class SceneRenderTarget {
    SceneRenderTarget(const SceneRenderTarget&) = delete;
    SceneRenderTarget(SceneRenderTarget&&) = delete;
    SceneRenderTarget& operator=(const SceneRenderTarget&) = delete;
    SceneRenderTarget& operator=(SceneRenderTarget&&) = delete;

    ctru::shader::Program m_program;

    C3D_AttrInfo m_attr_info;
    C3D_BufInfo m_buf_info;

    c3d::Tex m_tex1x{256, 512, GPU_RGB8, c3d::VRAM};
    C3D_FrameBuf m_tex1x_fb;

    c3d::Tex m_tex_scaling{SCREEN_TARGET_TEX_W, SCREEN_TARGET_TEX_H, GPU_RGB8,
                           c3d::VRAM};
    C3D_FrameBuf m_tex_scaling_fb;

    ctru::linear::ptr<BgScreenTargetVertexConst[]> m_vbo_data_const{
        ctru::linear::make_ptr_nothrow_for_overwrite<
            BgScreenTargetVertexConst[]>(4 * 4)};

    inline void init_screen_target_vbo();

public:
    // We aim to fit for the height, which means for the 400x240 top screen,
    // in native GBA size we need to fill a viewport of 266.67x160 px.
    // Obviously the fraction wouldn't work, and we also need to consider
    // image interpolation at the edges, so we round up to the next multiple
    // of 8px, which is 272x168 px.
    // In native 3DS size, this scales to 408x252 px.
    static constexpr int SCENE_W = 168;
    static constexpr int SCENE_H = 272;
    static_assert(SCENE_W * 3 < SCREEN_TARGET_TEX_W);
    static constexpr int SCENE_OFFSET_X = (SCENE_W - 160) / 2;
    static constexpr int SCENE_OFFSET_Y = (SCENE_H - 240) / 2;

    static constexpr int OUTPUT_W = SCENE_W * 1.5;
    static constexpr int OUTPUT_H = SCENE_H * 1.5;
    static constexpr int OUTPUT_OFFSET_X = (OUTPUT_W - 240) / 2;
    static constexpr int OUTPUT_OFFSET_Y = (OUTPUT_H - 400) / 2;

public:
    explicit inline SceneRenderTarget(ctru::shader::Program program) noexcept
        : m_program{std::move(program)} {
        // 1x target:
        C3D_TexSetFilter(m_tex1x.get(), GPU_NEAREST, GPU_NEAREST);
        C3D_TexSetWrap(m_tex1x.get(), GPU_CLAMP_TO_BORDER, GPU_CLAMP_TO_BORDER);

        C3D_FrameBufTex(&m_tex1x_fb, m_tex1x.get(), GPU_TEXFACE_2D, 0);
        C3D_FrameBufDepth(&m_tex1x_fb, nullptr, GPU_RB_DEPTH16);

        // Scaling target:
        C3D_TexSetFilter(m_tex_scaling.get(), GPU_LINEAR, GPU_LINEAR);
        C3D_TexSetWrap(m_tex_scaling.get(), GPU_CLAMP_TO_BORDER,
                       GPU_CLAMP_TO_BORDER);

        C3D_FrameBufTex(&m_tex_scaling_fb, m_tex_scaling.get(), GPU_TEXFACE_2D,
                        0);
        C3D_FrameBufDepth(&m_tex_scaling_fb, nullptr, GPU_RB_DEPTH16);

        init_screen_target_vbo();
    }

    constexpr c3d::Tex const& tex1x() const { return m_tex1x; }
    constexpr c3d::Tex const& tex_scaling() const { return m_tex_scaling; }

    inline void make_current_target() noexcept;

    inline void finish_current_target() noexcept;

    inline void draw(bool top_screen) noexcept;
};

inline void SceneRenderTarget::init_screen_target_vbo() {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    // Configure attributes for use with the vertex shader
    AttrInfo_Init(&m_attr_info);
    AttrInfo_AddLoader(&m_attr_info, 0, GPU_SHORT, 2); // v0=pos
    AttrInfo_AddLoader(&m_attr_info, 1, GPU_FLOAT, 2); // v1=uv

    const auto add_quad = [](BgScreenTargetVertexConst* vbo, s16 x, s16 y,
                             s16 w, s16 h, float u0, float u1, float v0,
                             float v1) -> size_t {
        const s16 l = x;
        const s16 t = y;
        const s16 r = x + w;
        const s16 b = y + h;
        vbo[0] = {{l, t}, {u0, v0}};
        vbo[1] = {{l, b}, {u0, v1}};
        vbo[2] = {{r, b}, {u1, v1}};
        vbo[3] = {{r, t}, {u1, v0}};
        return 4;
    };

    // The scene target texture is rotated, the native render target as well,
    // so is the quad.

    // 1x to scaling:
    {
        constexpr float u0 = 0.f;
        constexpr float u1 = (float)SCENE_W / 256;
        constexpr float v0 = (float)SCENE_H / 512;
        constexpr float v1 = 0.f;
        add_quad(&m_vbo_data_const[4 * 0], 0, 0, SCENE_W, SCENE_H, u0, u1, v0,
                 v1);
    }

    constexpr s16 w = OUTPUT_W;
    constexpr s16 h = OUTPUT_H;
    constexpr s16 x = -OUTPUT_OFFSET_X;
    constexpr s16 y = -OUTPUT_OFFSET_Y;

    // scaling 1x to screen:
    {
        constexpr float u0 = 0.f;
        constexpr float u1 = (float)SCENE_W / SCREEN_TARGET_TEX_W;
        constexpr float v0 = (float)SCENE_H / SCREEN_TARGET_TEX_H;
        constexpr float v1 = 0.f;
        add_quad(&m_vbo_data_const[4 * 1], x, y, w, h, u0, u1, v0, v1);
    }

    // scaling x2 to screen:
    {
        constexpr float u0 = 0.f;
        constexpr float u1 = (float)SCENE_W * 2.f / SCREEN_TARGET_TEX_W;
        constexpr float v0 = (float)SCENE_H * 2.f / SCREEN_TARGET_TEX_H;
        constexpr float v1 = 0.f;
        add_quad(&m_vbo_data_const[4 * 2], x, y, w, h, u0, u1, v0, v1);
    }

    // scaling hybrid x3/x2 to screen (for wide mode):
    {
        constexpr float u0 = 0.f;
        constexpr float u1 = (float)SCENE_W * 2.f / SCREEN_TARGET_TEX_W;
        constexpr float v0 = (float)SCENE_H * 3.f / SCREEN_TARGET_TEX_H;
        constexpr float v1 = 0.f;
        add_quad(&m_vbo_data_const[4 * 3], x, y, w, h, u0, u1, v0, v1);
    }

    // Configure buffers
    BufInfo_Init(&m_buf_info);
    BufInfo_Add(&m_buf_info, m_vbo_data_const.get(),
                sizeof(BgMosaicTexVertexConst), 2, 0x10);
}

// Scene window, using upright orientation, with top-left as origin
struct SceneOffset {
    s16 left;
    s16 top;
    s16 right;
    s16 bottom;
};

constexpr SceneOffset SCENE_OFFSET_CENTERED = {
    .left = -(400 - 240) / 2,
    .top = -(240 - 160) / 2,
    .right = 400 - (400 - 240) / 2,
    .bottom = 240 - (240 - 160) / 2,
};

// Clip window, adjusted to native screen orientation.
struct ScissorNative {
    int left;   // screen bottom
    int top;    // screen right
    int right;  // screen top
    int bottom; // screen left
    bool enabled;
    bool invert;
};

// Clip window in scene orientation.
struct ClipWindow {
    int left;
    int top;
    int right;
    int bottom;
    bool enabled;
    bool invert;
};

std::array<ClipWindow, tng::BG_LAYERS_COUNT> bg_clip_windows = {};
ClipWindow sprites_clip_window = {};

ScissorNative clip_to_scissor(const ClipWindow& window) {
    ScissorNative scissor = {};
    if (!window.enabled) {
        return scissor;
    }

    switch (savefile->settings.n3dsScaleMode) {
    case n3ds::ScaleMode::NUM_ENTRIES:
    default:
        assert(false && "Unexpected ScaleMode");
        [[fallthrough]];
    case n3ds::ScaleMode::UNSCALED: {
        const int x_adj = cr::SCENE_OFFSET_CENTERED.right;
        const int y_adj = cr::SCENE_OFFSET_CENTERED.bottom;

        // Remember that the screen is rotated 90 degrees.
        scissor.left = y_adj - window.bottom;
        scissor.right = y_adj - window.top;
        scissor.top = x_adj - window.right;
        scissor.bottom = x_adj - window.left;
        scissor.enabled = window.enabled;
        scissor.invert = window.invert;
    } break;
    case n3ds::ScaleMode::SCALED_LINEAR:
    case n3ds::ScaleMode::SCALED_SHARP:
    case n3ds::ScaleMode::SCALED_ULTRA:
        scissor.left = 160 - window.bottom + SceneRenderTarget::SCENE_OFFSET_X;
        scissor.right = 160 - window.top + SceneRenderTarget::SCENE_OFFSET_X;
        scissor.top = 240 - window.right + SceneRenderTarget::SCENE_OFFSET_Y;
        scissor.bottom = 240 - window.left + SceneRenderTarget::SCENE_OFFSET_Y;
        scissor.enabled = window.enabled;
        scissor.invert = window.invert;
    }
    if (scissor.left < 0) {
        scissor.left = 0;
    }
    if (scissor.top < 0) {
        scissor.top = 0;
    }
    return scissor;
}

bool rendering_enabled = false;

} /* namespace cr */

class RendererN3ds {
    RendererN3ds(const RendererN3ds&) = delete;
    RendererN3ds(RendererN3ds&&) = delete;
    RendererN3ds& operator=(const RendererN3ds&) = delete;
    RendererN3ds& operator=(RendererN3ds&&) = delete;

    struct ShaderInit {
        ctru::shader::Binary m_shader_binary;
        ctru::shader::Program m_program_sprite;
        ctru::shader::Program m_program_bg;
        ctru::shader::Program m_program_bg_mosaic_tex;
        ctru::shader::Program m_program_gradient;
        ctru::shader::Program m_program_simple_tex;

        inline ShaderInit() noexcept;
    };

    inline RendererN3ds(ShaderInit&&) noexcept;

public:
    RendererN3ds() noexcept;
    ~RendererN3ds() noexcept;

    void render(C3D_RenderTarget* rt, bool is_top_screen) noexcept;

private:
    void render_bg_mosaic_tex() noexcept;

    void render_setup(bool top_screen) noexcept;
    void render_draw(bool bg_mosaic) noexcept;

public:
#ifdef DEBUG_UI
    void imgui_debug_1(DebugOptions& debug_opt,
                       C3D_Tex* tilengine_tex) noexcept;
#endif

private:
    ctru::shader::Binary m_shader_binary;

    // It is important for this to be constructed first, since it allocates
    // the largest VRAM texture.
    cr::SceneRenderTarget m_scene_render_target;
    cr::SpritesRender m_sprites_render;
    cr::BGRender m_bg_render;
    cr::BGMosaicRender m_bg_mosaic_render;
    cr::GradientRender m_gradient_render;
};

static MaybeUninit<RendererN3ds> s_renderer;

void renderer_init() noexcept { s_renderer.emplace(); }

void renderer_free() noexcept { s_renderer.destruct(); }

void renderer_render(C3D_RenderTarget* rt, bool is_top_screen) noexcept {
    s_renderer.value().render(rt, is_top_screen);
}

#ifdef DEBUG_UI
void renderer_imgui_debug_1(DebugOptions& debug_opt,
                            C3D_Tex* tilengine_tex) noexcept {
    s_renderer.value().imgui_debug_1(debug_opt, tilengine_tex);
}
#endif

#include "n3ds_shader.dvle.h"
#include "n3ds_shader.shbin.h"

inline RendererN3ds::ShaderInit::ShaderInit() noexcept
    : m_shader_binary{(const u32*)n3ds_shader_shbin, n3ds_shader_shbin_size},
      m_program_sprite{m_shader_binary, N3DS_SHADER_DVLE_SHADER_SPRITE_V},
      m_program_bg{m_shader_binary, N3DS_SHADER_DVLE_SHADER_BG_V,
                   N3DS_SHADER_DVLE_SHADER_BG_G, 3},
      m_program_bg_mosaic_tex{m_shader_binary,
                              N3DS_SHADER_DVLE_SHADER_BG_MOSAIC_TEX_V},
      m_program_gradient{m_shader_binary, N3DS_SHADER_DVLE_SHADER_GRADIENT_V},
      m_program_simple_tex{m_shader_binary,
                           N3DS_SHADER_DVLE_SHADER_SIMPLE_TEX_V} {

    // Get the location of the uniforms
    cr::uLoc_projection =
        shaderInstanceGetUniformLocation(m_program_sprite.vsh(), "projection");

#ifndef NDEBUG
    // The location of the same uniform across multiple vertex shaders of a
    // binary should be identical.
    // Geometry shader does not share this however, so m_program_bg is not
    // checked.
    for (ctru::shader::Program* program :
         {&m_program_bg_mosaic_tex, &m_program_gradient,
          &m_program_simple_tex}) {
        s8 loc = shaderInstanceGetUniformLocation(program->vsh(), "projection");
        if (loc != cr::uLoc_projection) {
            fprintf(stderr,
                    "Shader uniform not identical. Expected %d, got %d\n",
                    cr::uLoc_projection, loc);
            abort();
        }
    }
#endif
}

RendererN3ds::RendererN3ds() noexcept : RendererN3ds(ShaderInit{}) {}

RendererN3ds::RendererN3ds(RendererN3ds::ShaderInit&& shader) noexcept
    : m_shader_binary{std::move(shader.m_shader_binary)},
      m_scene_render_target{std::move(shader.m_program_simple_tex)},
      m_sprites_render{std::move(shader.m_program_sprite)},
      m_bg_render{std::move(shader.m_program_bg)},
      m_bg_mosaic_render{std::move(shader.m_program_bg_mosaic_tex)},
      m_gradient_render{std::move(shader.m_program_gradient)} {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    // Compute the projection matrix
    Mtx_OrthoTilt(&cr::projection_top, cr::SCENE_OFFSET_CENTERED.left,
                  cr::SCENE_OFFSET_CENTERED.right,
                  cr::SCENE_OFFSET_CENTERED.bottom,
                  cr::SCENE_OFFSET_CENTERED.top, 0.0, 256.0, true);
    Mtx_OrthoTilt(&cr::projection_bottom, cr::SCENE_OFFSET_CENTERED.left + 40,
                  cr::SCENE_OFFSET_CENTERED.right - 40,
                  cr::SCENE_OFFSET_CENTERED.bottom,
                  cr::SCENE_OFFSET_CENTERED.top, 0.0, 256.0, true);
}

RendererN3ds::~RendererN3ds() noexcept {
    // TODO: cleanup (do we actually want to waste time doing that on exit?)
}

void RendererN3ds::render(C3D_RenderTarget* rt, bool is_top_screen) noexcept {
#ifdef DEBUG_UI
    TickCounter timing;
    osTickCounterStart(&timing);
#endif

    // First build the sprites:
    m_sprites_render.prep_spritesheet();

#ifdef DEBUG_UI
    osTickCounterUpdate(&timing);
    dbg::stat("Spr1", osTickCounterRead(&timing) * 6.0f);
#endif

    // Then build the background layers:

    m_bg_render.prep_charsheet();

#ifdef DEBUG_UI
    osTickCounterUpdate(&timing);
    dbg::stat("Bg1", osTickCounterRead(&timing) * 6.0f);
#endif

    m_sprites_render.prep_sprite_vbo();

#ifdef DEBUG_UI
    osTickCounterUpdate(&timing);
    dbg::stat("SprB", osTickCounterRead(&timing) * 6.0f);
#endif

    m_bg_render.prep_vbo();

#ifdef DEBUG_UI
    osTickCounterUpdate(&timing);
    dbg::stat("BgB", osTickCounterRead(&timing) * 6.0f);
#endif

    const bool screen_zoom_fit =
        savefile->settings.n3dsScaleMode != n3ds::ScaleMode::UNSCALED;

    // Build the gradient backdrop:
    m_gradient_render.prep_vbo(screen_zoom_fit);

#ifdef DEBUG_UI
    osTickCounterUpdate(&timing);
    dbg::stat("Grad", osTickCounterRead(&timing) * 6.0f);
#endif

    const bool enable_bg_mosaic =
#ifdef DEBUG_UI
        s_debug_opt.enable_bg_mosaic;
#else
        true;
#endif

    if (enable_bg_mosaic) {
        render_bg_mosaic_tex();
    }

#ifdef DEBUG_UI
    osTickCounterUpdate(&timing);
    dbg::stat("Mosc", osTickCounterRead(&timing) * 6.0f);
#endif

    if (!screen_zoom_fit) {
        C3D_FrameDrawOn(rt);
        render_setup(is_top_screen);
    } else {
        m_scene_render_target.make_current_target();
    }

    // Suppress rendering to fix flicker in menu.
    // Instead of skipping the rendering entirely, just set an empty viewport.
    if (!cr::rendering_enabled) {
        C3D_SetViewport(0, 0, 0, 0);
    }

    render_draw(enable_bg_mosaic);

    if (screen_zoom_fit) {
        m_scene_render_target.finish_current_target();
        C3D_FrameDrawOn(rt);
        m_scene_render_target.draw(is_top_screen);
    }

#ifdef DEBUG_UI
    osTickCounterUpdate(&timing);
    dbg::stat("Draw", osTickCounterRead(&timing) * 6.0f);
#endif
}

void cr::SpritesRender::prep_spritesheet() noexcept {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    int sprite_count = 0;
    int dirty_sprite_tile_count = 0;
    int affine_count = 0;

    for (size_t i = 0; i < tng::SPRITE_OBJ_COUNT; i++) {
        const auto& attr = tng::sprite_obj_attrs[i];
        if (!attr.enabled) {
            continue;
        }

        std::array<unsigned short, 16>& current_palette =
            tng::palettes_sprite[attr.palette];

        int sheet_x =
            (i % cr::SPRITESHEET_NUM_SPRITES_X) * cr::SPRITESHEET_SPRITE_W;
        int sheet_y =
            (i / cr::SPRITESHEET_NUM_SPRITES_X) * cr::SPRITESHEET_SPRITE_H;

        // Set up spritesheet:
        {
            size_t tile_idx = attr.tile_start;
            for (int v = 0; v < attr.sy / 8; v++) {
                for (int u = 0; u < attr.sx / 8; u++) {
                    // Handling one tile:
                    if (true || tng::sprite_tile_dirty_flag[tile_idx]) {
                        static_assert(cr::SPRITESHEET_PIXEL_SIZE ==
                                      sizeof(u16));
                        u16* const sprite_tile_start =
                            (u16*)m_spritesheet_tex->data +
                            tex_util::swizzled_px_block_offset(
                                sheet_x + u * 8, sheet_y + v * 8,
                                cr::SPRITESHEET_W);
                        for (int a = 0; a < 8; a++) {
                            u32 row = tng::sprite_tile_images[tile_idx].rows[a];
                            for (int b = 0; b < 8; b++) {
                                int pal = row & 0x0f;
                                row >>= 4;
                                u16 color;
                                if (pal) {
                                    color = current_palette[pal];
                                } else {
                                    // Palette colour 0 is transparent.
                                    color = 0;
                                }

                                size_t index =
                                    tex_util::swizzled_px_intile_offset(b, a);
                                sprite_tile_start[index] = color;
                            }
                        }
                        // tng::sprite_tile_dirty_flag[tile_idx] = false;
                        dirty_sprite_tile_count++;
                    }
                    tile_idx++;
                }
            }
        }
        sprite_count++;
        if (attr.affine) {
            affine_count++;
        }
    }

#ifdef TRACY_ENABLE
    TracyPlot("Sprite count", (int64_t)sprite_count);
    TracyPlot("Dirty sprite tile count", (int64_t)dirty_sprite_tile_count);
#endif
}

void cr::BGRender::prep_charsheet() noexcept {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    for (size_t layer_idx = 0; layer_idx < tng::BG_LAYERS_COUNT; layer_idx++) {
#ifdef TRACY_ENABLE
        ZoneScoped;
        ZoneValue(layer_idx);
#endif

        const LayerInfo& li = layerInfo[layer_idx];
        const tng::BgScreenblockTilemap& screenblock =
            tng::bg_screenblocks[li.sbb];
        const tng::BgCharblockTileset& charblock = tng::bg_charblocks[li.cbb];
        c3d::Tex& char_sheet_tex = m_char_sheet_tex[layer_idx];

        // First, we find all the used combinations of (char, palette)
        // Here, std::set turned out to be faster than std::unordered_set,
        // maybe because the element count is low?
        std::set<cr::CharKey> s_used_keys;
        for (size_t y = 0; y < cr::BG_NUM_TILES_H; y++) {
            for (size_t x = 0; x < cr::BG_NUM_TILES_W; x++) {
                const u16 se_val = screenblock[y * cr::BG_NUM_TILES_W + x];
                const int char_tile_idx = se_val & 0x3ff;
                // const bool flip_h = se_val & 0x400;
                // const bool flip_v = se_val & 0x800;
                const int use_pal = se_val >> 12;
                const cr::CharKey key =
                    cr::make_charkey(char_tile_idx, use_pal);
                s_used_keys.insert(key);
            }
        }

        // Then we assign char slots and blit the tiles onto the char sheet
        auto& char_slot_map = m_char_slot_maps[layer_idx];
        auto& char_slots = m_char_slot_arrays[layer_idx];
        for (cr::CharKey key : s_used_keys) {
            // Find / assign slot
            auto it = char_slot_map.find(key);
            if (__builtin_expect(it == char_slot_map.end(), 0)) {
                // TODO: use a smarter allocator
                for (size_t i = 0; i < char_slots.size(); i++) {
                    if (char_slots[i] == cr::CHARKEY_NULL) {
                        char_slots[i] = key;
                        std::tie(it, std::ignore) =
                            char_slot_map.insert({key, i});
                        break;
                    }
                }
                if (__builtin_expect(it == char_slot_map.end(), 0)) {
                    // TODO: garbage collection
                    // (not actually needed since there's not enough tiles)
                    fprintf(stderr, "no empty char slots!\n");
                    abort();
                }
            }

            // Blit the char tile
            const int slot = it->second;
            const int char_tile_idx = cr::charkey_to_char_idx(key);
            const int use_pal = cr::charkey_to_palette(key);
            const auto& palette = tng::palettes_bg[use_pal];
            static_assert(cr::BG_PIXEL_SIZE == sizeof(u16));
            u16* const tex_tile_start =
                (u16*)char_sheet_tex->data + tex_util::swizzled_px_block_offset(
                                                 slot % cr::BG_NUM_TILES_W * 8,
                                                 slot / cr::BG_NUM_TILES_W * 8,
                                                 cr::BG_NUM_TILES_W * 8);
            for (int a = 0; a < 8; a++) {
                u32 row = charblock[char_tile_idx].rows[a];
                for (int b = 0; b < 8; b++) {
                    int pal = row & 0x0f;
                    row >>= 4;
                    u16 color;
                    if (pal) {
                        color = palette[pal];
                    } else {
                        // Palette colour 0 is transparent.
                        color = 0;
                    }

                    size_t index = tex_util::swizzled_px_intile_offset(b, a);
                    tex_tile_start[index] = color;
                }
            }
        }
    }
}

void cr::SpritesRender::prep_sprite_vbo() noexcept {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    m_priority_indices_count = {};

    const auto add_quad_sprite =
        [](cr::SpriteVertexVar* vbo, s16 sprite_x, s16 sprite_y, s8 spr_w,
           s8 spr_h, float scale_x, float scale_y, bool flip_x, bool flip_y,
           bool blend, const float affine[4], bool double_size) -> size_t {
        u8 double_size_v = double_size ? 1 : 0;
        vbo[0] = {{sprite_x, sprite_y},
                  {spr_w, spr_h},
                  {flip_x, flip_y},
                  {affine[0], affine[1], affine[2], affine[3]},
                  {blend, double_size_v}};
        vbo[1] = {{sprite_x, sprite_y},
                  {spr_w, spr_h},
                  {flip_x, flip_y},
                  {affine[0], affine[1], affine[2], affine[3]},
                  {blend, double_size_v}};
        vbo[2] = {{sprite_x, sprite_y},
                  {spr_w, spr_h},
                  {flip_x, flip_y},
                  {affine[0], affine[1], affine[2], affine[3]},
                  {blend, double_size_v}};
        vbo[3] = {{sprite_x, sprite_y},
                  {spr_w, spr_h},
                  {flip_x, flip_y},
                  {affine[0], affine[1], affine[2], affine[3]},
                  {blend, double_size_v}};
        return 4;
    };

    size_t vi = 0;
    for (size_t i = 0; i < tng::SPRITE_OBJ_COUNT; i++) {
        const auto& attr = tng::sprite_obj_attrs[i];
        if (!attr.enabled) {
            vi += 4;
            continue;
        }

        float x = attr.x;  // * 8;
        float y = attr.y;  // * 8;
        float w = attr.sx; // * 8;
        float h = attr.sy; // * 8;
        constexpr float IDENTITY[4] = {1, 0, 0, 1};
        vi += add_quad_sprite(&m_vbo_data_var[vi], x, y, w, h, attr.scalex,
                              attr.scaley, attr.flipx, attr.flipy, attr.blend,
                              attr.affine ? attr.affine_mat : IDENTITY,
                              attr.doubleSize);
    }

    // Prepare the index buffer in reverse priority order since we need to
    // draw from bottom to top, and higher sprite to lower.
    size_t ii = 0;
    for (int pri = 3; pri >= 0; pri--) {
        m_priority_indices_count[pri].first = ii;
        for (int i = tng::SPRITE_OBJ_COUNT - 1; i >= 0; i--) {
            const auto& attr = tng::sprite_obj_attrs[i];
            if (!attr.enabled) {
                continue;
            }
            if (((attr.priority >> 4) & 0x3) != pri) {
                continue;
            }
            m_indices_buffer[ii + 0] = i * 4 + 0;
            m_indices_buffer[ii + 1] = i * 4 + 1;
            m_indices_buffer[ii + 2] = i * 4 + 2;
            m_indices_buffer[ii + 3] = i * 4 + 0;
            m_indices_buffer[ii + 4] = i * 4 + 2;
            m_indices_buffer[ii + 5] = i * 4 + 3;
            ii += 6;
        }
        m_priority_indices_count[pri].second =
            ii - m_priority_indices_count[pri].first;
    }
}

void cr::BGRender::prep_vbo() noexcept {
#ifdef TRACY_ENABLE
    ZoneScopedN("BGRender::prep_vbo");
#endif

    cr::BgVertexVar *vi_ptr = m_vbo_data_var.get();
    for (size_t layer_idx = 0; layer_idx < tng::BG_LAYERS_COUNT; layer_idx++) {
#ifdef TRACY_ENABLE
        ZoneScopedN("BGRender::prep_vbo layer");
        ZoneValue(layer_idx);
#endif

        const LayerInfo& li = layerInfo[layer_idx];
        const tng::BgScreenblockTilemap& screenblock =
            tng::bg_screenblocks[li.sbb];
        const tng::BgCharblockTileset& charblock = tng::bg_charblocks[li.cbb];
        const auto& char_slot_map = m_char_slot_maps[layer_idx];

        // Now we prepare the VBO
        for (size_t j = 0; j < cr::BG_NUM_TILES_H; j++) {
            for (size_t i = 0; i < cr::BG_NUM_TILES_W; i++) {
                const u16 se_val = screenblock[j * cr::BG_NUM_TILES_W + i];
                const int char_tile_idx = se_val & 0x3ff;
                const bool flip_h = se_val & 0x400;
                const bool flip_v = se_val & 0x800;
                const int use_pal = se_val >> 12;
                const cr::CharKey key =
                    cr::make_charkey(char_tile_idx, use_pal);

                auto it = char_slot_map.find(key);
                if (__builtin_expect(it == char_slot_map.end(), 0)) {
                    fprintf(
                        stderr,
                        "No char slot for this tile! This shouldn't happen.\n");
                    abort();
                }

                const s16 slot = (s16)it->second;
                *vi_ptr = {slot, {flip_h, flip_v}};
                vi_ptr++;
            }
        }
    }
}

void cr::GradientRender::prep_vbo(bool screen_zoom_fit) noexcept {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    size_t gradient_strip_count = 0;

    cr::VertexGradient* next_buf = m_vbo_data.get();
    u16 last_color = gradientTable[0];
    size_t same_color_count = 0;
    // Add one so that the first strip extends upwards to -1, needed to
    // prevent bleeding of texture edge colour in zoom fit mode.
    same_color_count += 1;

    const auto add_gradient = [screen_zoom_fit](cr::VertexGradient* vbo,
                                                u16 colorgb5, s16 from_y,
                                                s16 to_y) {
        s8 c_r = colorgb5 & 0b0001'1111;
        s8 c_g = (colorgb5 >> 5) & 0b0001'1111;
        s8 c_b = (colorgb5 >> 10) & 0b0001'1111;
        const s16 l = cr::SCENE_OFFSET_CENTERED.left;
        const s16 r = cr::SCENE_OFFSET_CENTERED.right;
        const s16 y_adj = cr::SCENE_OFFSET_CENTERED.top;
        const s16 t = (screen_zoom_fit ? from_y : (from_y * 3 / 2 + y_adj));
        const s16 b = (screen_zoom_fit ? to_y : (to_y * 3 / 2 + y_adj));
        vbo[0] = {{l, t}, {c_r, c_g, c_b}};
        vbo[1] = {{l, b}, {c_r, c_g, c_b}};
        vbo[2] = {{r, b}, {c_r, c_g, c_b}};
        vbo[3] = {{l, t}, {c_r, c_g, c_b}};
        vbo[4] = {{r, b}, {c_r, c_g, c_b}};
        vbo[5] = {{r, t}, {c_r, c_g, c_b}};
        return 6;
    };

    if (gradientEnabled) {
        for (size_t i = 0; i < cr::GRADIENT_LINES_COUNT; i++) {
            u16 colorgb5 = gradientTable[i];
            if (colorgb5 == last_color) {
                same_color_count++;
                continue;
            }

            next_buf +=
                add_gradient(next_buf, last_color, i - same_color_count, i);
            gradient_strip_count++;

            last_color = colorgb5;
            same_color_count = 1;

            if (gradient_strip_count == 16) {
                // Sanity check. In case of adding new background patterns,
                // please consider maybe there is a more efficient way to
                // implement it for N3DS?
                break;
            }
        }
    } else {
        // Treat background colour as a single gradient strip.
        last_color = tng::palettes_bg[0][0];
        same_color_count = cr::GRADIENT_LINES_COUNT;
    }

    // Add one so that the last strip extends downwards to 161, needed to
    // prevent bleeding of texture edge colour in zoom fit mode.
    next_buf += add_gradient(next_buf, last_color,
                             cr::GRADIENT_LINES_COUNT - same_color_count,
                             cr::GRADIENT_LINES_COUNT + 1);
    gradient_strip_count++;

    m_gradient_strip_count = gradient_strip_count;
}

inline void cr::GradientRender::draw() noexcept {
    C3D_BindProgram(m_program.get());
    C3D_SetAttrInfo(&m_attr_info);
    C3D_SetBufInfo(&m_buf_info);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

    C3D_DrawArrays(GPU_TRIANGLES, 0, m_gradient_strip_count * 6);
}

inline void cr::BGRender::draw_bg(int layer_idx,
                                  bool drawing_mosaic_tex) noexcept {
#ifdef TRACY_ENABLE
    ZoneScopedN("Background");
    ZoneValue(layer_idx);
#endif

    const LayerInfo& li = layerInfo[layer_idx];

    C3D_BindProgram(m_program.get());
    C3D_SetAttrInfo(&m_attr_info);
    C3D_SetBufInfo(&m_buf_info);

    if (!drawing_mosaic_tex && tng::blend_top_bg[layer_idx]) {
        C3D_TexEnv* env = C3D_GetTexEnv(0);
        C3D_TexEnvInit(env);
        C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_CONSTANT);
        C3D_TexEnvFunc(env, C3D_Alpha, GPU_MODULATE);
        C3D_TexEnvColor(env, 0xbf000000);
    } else {
        C3D_TexEnv* env = C3D_GetTexEnv(0);
        C3D_TexEnvInit(env);
        C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0);
        C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
    }

    const auto window = cr::clip_to_scissor(cr::bg_clip_windows[layer_idx]);
    if (!drawing_mosaic_tex && window.enabled) {
        C3D_SetScissor(window.invert ? GPU_SCISSOR_INVERT : GPU_SCISSOR_NORMAL,
                       window.left, window.top, window.right, window.bottom);
    }

    // xy = layer top-left
    C3D_FixedAttribSet(3, li.scroll_x, li.scroll_y, 0.0, 0.0);

    C3D_TexBind(0, m_char_sheet_tex[layer_idx].get());

    C3D_DrawArrays(GPU_GEOMETRY_PRIM, layer_idx * cr::BG_LAYER_TILES_COUNT,
                   cr::BG_LAYER_TILES_COUNT);

    if (!drawing_mosaic_tex && window.enabled) {
        C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
    }
}

inline void cr::SpritesRender::draw_sprites(const size_t priority) noexcept {
    auto [idx_start, vert_count] = m_priority_indices_count[priority];
    if (vert_count == 0) {
        return;
    }
#ifdef TRACY_ENABLE
    ZoneScopedN("Sprites");
    ZoneTextF("pri=%zu idx=%u count=%u", priority, idx_start, vert_count);
#endif

    C3D_BindProgram(m_program.get());
    C3D_SetAttrInfo(&m_attr_info);
    C3D_SetBufInfo(&m_buf_info);

    C3D_TexBind(0, m_spritesheet_tex.get());

    float blend_factor;
    if (tng::blend_top_sprites) {
        // Override all alpha if global sprites blending is active.
        C3D_TexEnv* env = C3D_GetTexEnv(0);
        C3D_TexEnvInit(env);
        C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_CONSTANT);
        C3D_TexEnvFunc(env, C3D_Alpha, GPU_MODULATE);
        C3D_TexEnvColor(env, 0x33000000);

        // x = blend factor
        C3D_FixedAttribSet(7, 1.0, 0.0, 0.0, 0.0);
    } else {
        C3D_TexEnv* env = C3D_GetTexEnv(0);
        C3D_TexEnvInit(env);
        C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR);
        C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);

        // x = blend factor
        C3D_FixedAttribSet(7, 0.75, 0.0, 0.0, 0.0);
    }

    const auto window = cr::clip_to_scissor(cr::sprites_clip_window);
    if (window.enabled) {
        C3D_SetScissor(window.invert ? GPU_SCISSOR_INVERT : GPU_SCISSOR_NORMAL,
                       window.left, window.top, window.right, window.bottom);
    }

    C3D_DrawElements(GPU_TRIANGLES, vert_count, C3D_UNSIGNED_SHORT,
                     &m_indices_buffer[idx_start]);

    if (window.enabled) {
        C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
    }
}

void RendererN3ds::render_bg_mosaic_tex() noexcept {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
    C3D_AlphaTest(false, GPU_ALWAYS, 0);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE,
                   GPU_ZERO);
    C3D_CullFace(GPU_CULL_NONE);

    for (size_t layer_idx = 0; layer_idx < tng::BG_LAYERS_COUNT; layer_idx++) {
        const LayerInfo& li = layerInfo[layer_idx];
        if (!li.enabled) {
            continue;
        }

#ifdef TRACY_ENABLE
        ZoneScopedN("Background (tex)");
        ZoneValue(layer_idx);
#endif

        m_bg_mosaic_render.make_current_target(
            layer_idx, m_bg_render.uLoc_gsh_projection());

        m_bg_render.draw_bg(layer_idx, true);
    }
}

inline void
cr::BGMosaicRender::make_current_target(size_t layer,
                                        int uLoc_gsh_projection) noexcept {
    const LayerInfo& li = layerInfo[layer];

    C3D_FrameBuf& fb = m_tex_fb[layer];
    C3D_SetFrameBuf(&fb);

    float x_scale = tng::bg_mosaic_x + 1;
    float y_scale = tng::bg_mosaic_y + 1;
    int scaled_w = ::ceilf((float)cr::BG_MOSAIC_TEX_W / x_scale);
    int scaled_h = ::ceilf((float)cr::BG_MOSAIC_TEX_H / y_scale);

    C3D_SetViewport(0, cr::BG_MOSAIC_TEX_H - scaled_h, scaled_w, scaled_h);

    C3D_Mtx projection;

    // Compute the projection matrix
    float proj_w = scaled_w * x_scale;
    float proj_h = scaled_h * y_scale;
    constexpr float base_offset = -0.5f + 1.f / 32.f;
    Mtx_Ortho(&projection, base_offset, proj_w + base_offset,
              proj_h + base_offset + cr::BG_MOSAIC_Y_OFFSET,
              base_offset + cr::BG_MOSAIC_Y_OFFSET, 0.0, 256.0, true);

    // Update the uniforms
    C3D_FVUnifMtx4x4(GPU_GEOMETRY_SHADER, uLoc_gsh_projection, &projection);
}

inline void cr::BGMosaicRender::draw_tex(size_t layer_idx) noexcept {
#ifdef TRACY_ENABLE
    ZoneScopedN("Background (tex)");
    ZoneValue(layer_idx);
#endif

    const LayerInfo& li = layerInfo[layer_idx];

    C3D_BindProgram(m_program.get());
    C3D_SetAttrInfo(&m_attr_info);
    C3D_SetBufInfo(&m_buf_info);

    if (tng::blend_top_bg[layer_idx]) {
        C3D_TexEnv* env = C3D_GetTexEnv(0);
        C3D_TexEnvInit(env);
        C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_CONSTANT);
        C3D_TexEnvFunc(env, C3D_Alpha, GPU_MODULATE);
        C3D_TexEnvColor(env, 0xbf000000);
    } else {
        C3D_TexEnv* env = C3D_GetTexEnv(0);
        C3D_TexEnvInit(env);
        C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0);
        C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
    }

    const auto window = cr::clip_to_scissor(cr::bg_clip_windows[layer_idx]);
    if (window.enabled) {
        C3D_SetScissor(window.invert ? GPU_SCISSOR_INVERT : GPU_SCISSOR_NORMAL,
                       window.left, window.top, window.right, window.bottom);
    }

    float x_scale = tng::bg_mosaic_x + 1;
    float y_scale = tng::bg_mosaic_y + 1;
    float x_mosaic_scale = 1.f / x_scale;
    float y_mosaic_scale = 1.f / y_scale;
    C3D_FixedAttribSet(2, x_mosaic_scale, y_mosaic_scale, 0.0, 0.0);

    C3D_TexBind(0, m_tex[layer_idx].get());

    const size_t idx = tng::bg_screenblock_fill_screen[li.sbb] ? 1 : 0;
    C3D_DrawArrays(GPU_TRIANGLE_FAN, idx * 4, 4);

    if (window.enabled) {
        C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
    }
}

void RendererN3ds::render_setup(bool top_screen) noexcept {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    // C3D_LightEnvBind(&lightEnv);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
    C3D_AlphaTest(true, GPU_GREATER, 0);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA,
                   GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA,
                   GPU_ONE_MINUS_SRC_ALPHA);
    C3D_CullFace(GPU_CULL_NONE);

    // Update the uniforms
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, cr::uLoc_projection,
                     top_screen ? &cr::projection_top : &cr::projection_bottom);
}

inline void cr::SceneRenderTarget::make_current_target() noexcept {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    // C3D_LightEnvBind(&lightEnv);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
    C3D_AlphaTest(true, GPU_GREATER, 0);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA,
                   GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA,
                   GPU_ONE_MINUS_SRC_ALPHA);
    C3D_CullFace(GPU_CULL_NONE);

    C3D_SetViewport(0, 0, SCENE_W, SCENE_H);

    static C3D_Mtx projection = []() {
        C3D_Mtx projection;
        constexpr int left = -SCENE_OFFSET_Y;
        constexpr int bottom = -SCENE_OFFSET_X;
        Mtx_OrthoTilt(&projection, left, left + SCENE_H, bottom + SCENE_W,
                      bottom, 0.0, 256.0, true);
        return projection;
    }();
    // Update the uniforms
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, cr::uLoc_projection, &projection);

    // Clear the texture intentionally bypassing C3D to:
    // - Limit the number of rows to clear
    // - Avoid splitting the C3D command queue
    // But actually, we don't need to clear it at all because we always overdraw
    // the previous image with the gradient background.
    if (savefile->settings.n3dsScaleMode == n3ds::ScaleMode::SCALED_LINEAR) {
#if 0
        u8* start =
            (u8*)m_tex_scaling->data +
            cr::SCREEN_TARGET_TEX_W * (cr::SCREEN_TARGET_TEX_H - SCENE_H) * 3;
        u8* end = (u8*)m_tex_scaling->data +
                  cr::SCREEN_TARGET_TEX_W * cr::SCREEN_TARGET_TEX_H * 3;
        GX_MemoryFill((u32*)start, 0, (u32*)end,
                      GX_FILL_24BIT_DEPTH | GX_FILL_TRIGGER, nullptr, 0,
                      nullptr, 0);
#endif
        C3D_SetFrameBuf(&m_tex_scaling_fb);
    } else {
#if 0
        u8* start = (u8*)m_tex1x->data + 256 * (512 - SCENE_H) * 3;
        u8* end = (u8*)m_tex1x->data + 256 * 512 * 3;
        GX_MemoryFill((u32*)start, 0, (u32*)end,
                      GX_FILL_24BIT_DEPTH | GX_FILL_TRIGGER, nullptr, 0,
                      nullptr, 0);
#endif
        C3D_SetFrameBuf(&m_tex1x_fb);
    }
}

void cr::SceneRenderTarget::finish_current_target() noexcept {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    int texScaleX;
    int texScaleY;
    switch (savefile->settings.n3dsScaleMode) {
    case n3ds::ScaleMode::UNSCALED:
    case n3ds::ScaleMode::NUM_ENTRIES:
    default:
        assert(false && "Unexpected ScaleMode");
        [[fallthrough]];
    case n3ds::ScaleMode::SCALED_LINEAR:
        return;
    case n3ds::ScaleMode::SCALED_SHARP:
        texScaleX = 2;
        texScaleY = 2;
        break;
    case n3ds::ScaleMode::SCALED_ULTRA:
        texScaleX = 2;
        texScaleY = 3;
        break;
    }

    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
    C3D_AlphaTest(false, GPU_ALWAYS, 0);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE,
                   GPU_ZERO);
    C3D_CullFace(GPU_CULL_NONE);

    C3D_SetViewport(0, 0, SCENE_W * texScaleX, SCENE_H * texScaleY);

    static C3D_Mtx projection = []() {
        C3D_Mtx projection;
        Mtx_Ortho(&projection, 0, SCENE_W, SCENE_H, 0, 0.0, 256.0, true);
        return projection;
    }();
    // Update the uniforms
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, cr::uLoc_projection, &projection);

    C3D_SetFrameBuf(&m_tex_scaling_fb);

    // ---

    C3D_BindProgram(m_program.get());
    C3D_SetAttrInfo(&m_attr_info);
    C3D_SetBufInfo(&m_buf_info);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

    C3D_TexBind(0, m_tex1x.get());

    C3D_DrawArrays(GPU_TRIANGLE_FAN, 0, 4);
}

void RendererN3ds::render_draw(bool bg_mosaic) noexcept {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    // Draw the gradient / colour backdrop first:
    m_gradient_render.draw();

    for (int pri = 3; pri >= 0; pri--) {
#ifdef TRACY_ENABLE
        ZoneScopedN("Priority layer");
        ZoneValue(pri);
#endif

        for (int layer_idx = tng::BG_LAYERS_COUNT - 1; layer_idx >= 0;
             layer_idx--) {
            const LayerInfo& li = layerInfo[layer_idx];
            if (!li.enabled) {
                continue;
            }
            if (((li.priority >> 4) & 0x3) != pri) {
                continue;
            }

            if (bg_mosaic) {
                m_bg_mosaic_render.draw_tex(layer_idx);
            } else {
                m_bg_render.draw_bg(layer_idx, false);
            }
        }

        m_sprites_render.draw_sprites(pri);
    }
}

inline void cr::SceneRenderTarget::draw(bool top_screen) noexcept {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
    C3D_AlphaTest(false, GPU_ALWAYS, 0);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE,
                   GPU_ZERO);
    C3D_CullFace(GPU_CULL_NONE);

    static C3D_Mtx projection_top = []() {
        C3D_Mtx projection;
        Mtx_Ortho(&projection, 0, 240, 400, 0, 0.0, 256.0, true);
        return projection;
    }();

    static C3D_Mtx projection_bottom = []() {
        C3D_Mtx projection;
        Mtx_Ortho(&projection, 0, 240, 360, 40, 0.0, 256.0, true);
        return projection;
    }();
    // Update the uniforms
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, cr::uLoc_projection,
                     top_screen ? &projection_top : &projection_bottom);

    C3D_BindProgram(m_program.get());
    C3D_SetAttrInfo(&m_attr_info);
    C3D_SetBufInfo(&m_buf_info);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

    C3D_TexBind(0, m_tex_scaling.get());

    const int texScale = [&]() {
        switch (savefile->settings.n3dsScaleMode) {
        case n3ds::ScaleMode::UNSCALED:
        case n3ds::ScaleMode::NUM_ENTRIES:
        default:
            assert(false && "Unexpected ScaleMode");
            [[fallthrough]];
        case n3ds::ScaleMode::SCALED_LINEAR:
            return 1;
        case n3ds::ScaleMode::SCALED_SHARP:
            return 2;
        case n3ds::ScaleMode::SCALED_ULTRA:
            return 3;
        }
    }();

    C3D_DrawArrays(GPU_TRIANGLE_FAN, texScale * 4, 4);
}

#ifdef DEBUG_UI

// Custom ImGui helper stuff
namespace imgui_helpers {

namespace _p {

static C3D_Tex transparency_checkerboard_tex;

} // namespace _p

void init() {
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    using namespace _p;

    // Don't ask me why this has to be 64px wide - any smaller and it just
    // deadlocks later in the main loop.
    if (!C3D_TexInitVRAM(&transparency_checkerboard_tex, 64, 8, GPU_RGBA8)) {
        puts("C3D_TexInitVRAM(&transparency_checkerboard_tex) failed");
        exit(1);
    }
    C3D_TexSetWrap(&transparency_checkerboard_tex, GPU_REPEAT, GPU_REPEAT);
    C3D_TexSetFilter(&transparency_checkerboard_tex, GPU_NEAREST, GPU_NEAREST);

    ctru::linear::ptr<u32[]> temp =
        ctru::linear::make_ptr_nothrow_for_overwrite<u32[]>(64 * 8);
    constexpr u32 checkerboard[] = {0x7f7f7fff, 0xd0d0d0ff};
    for (int i = 0; i < 64 * 8; i++) {
        temp[i] = checkerboard[(i / (64 * 4)) ^ ((i / 32) % 2)];
    }
    Result res = GSPGPU_FlushDataCache(temp.get(), 64 * 8 * 4);
    if (R_FAILED(res)) {
        fprintf(stderr, "GSPGPU_FlushDataCache error: %08" PRIx32 "\n", res);
        exit(1);
    }
    res = GX_DisplayTransfer(
        temp.get(), GX_BUFFER_DIM(64, 8),
        (u32*)transparency_checkerboard_tex.data, GX_BUFFER_DIM(64, 8),
        GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
            GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) |
            GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_FLIP_VERT(0) |
            GX_TRANSFER_RAW_COPY(0) |
            GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
    if (R_FAILED(res)) {
        fprintf(stderr, "GSPGPU_FlushDataCache error: %08" PRIx32 "\n", res);
        exit(1);
    }
    gspWaitForPPF();
}

void fini() { C3D_TexDelete(&_p::transparency_checkerboard_tex); }

constexpr ImVec2 operator-(const ImVec2& a) { return {-a.x, -a.y}; }

constexpr ImVec2 operator+(const ImVec2& a, const ImVec2& b) {
    return {a.x + b.x, a.y + b.y};
}

constexpr ImVec2 operator-(const ImVec2& a, const ImVec2& b) {
    return a + (-b);
}

constexpr ImVec2 operator*(const ImVec2& a, float b) {
    return {a.x * b, a.y * b};
}

constexpr ImVec2 operator/(const ImVec2& a, float b) {
    return {a.x / b, a.y / b};
}

void draw_checkerboard(
    const ImVec2& p_min, const ImVec2& p_max,
    ImDrawList* draw_list = ImGui::GetWindowDrawList(),
    const ImVec2& screen_topleft = ImGui::GetCursorScreenPos()) {
    constexpr float checker_scale = 1.f / 16.f;
    const ImVec2& tl = screen_topleft;
    const ImVec2 sz = p_max - p_min;
    const ImVec2 uv_off = {tl.x * checker_scale, tl.y * checker_scale};
    draw_list->AddImage(&_p::transparency_checkerboard_tex, tl + p_min,
                        tl + p_max, uv_off, uv_off + sz * checker_scale);
}

} /* namespace imgui_helpers */

namespace dbg {

void show_stats_imgui(bool* p_open = nullptr);

} /* namespace dbg */

void RendererN3ds::imgui_debug_1(DebugOptions& debug_opt,
                                 C3D_Tex* tilengine_tex) noexcept {
    static bool s_show_stats = true;
    static bool s_show_sprites_viewer = false;
    static bool s_show_bg_viewer = false;
    static bool s_show_color_viewer = false;
    static bool s_show_mosaic_cfg = false;
    static bool s_show_scaling_tex = false;
    static bool s_show_imgui_demo = false;
    static bool s_show_clear_color_dlg = false;
    static bool s_show_quit_confirmation = false;

#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Engine")) {
                if (ImGui::MenuItem("Use New 3DS speed", nullptr,
                                    &debug_opt.use_new3ds_speed)) {
                    osSetSpeedupEnable(debug_opt.use_new3ds_speed);
                }
                ImGui::MenuItem("Tilengine Render", nullptr,
                                &debug_opt.tilengine_render);
                ImGui::MenuItem("Enable BG mosaic", nullptr,
                                &debug_opt.enable_bg_mosaic);
                bool screen_zoom_fit = savefile->settings.n3dsScaleMode !=
                                       n3ds::ScaleMode::UNSCALED;
                if (ImGui::BeginMenu("Scale")) {
                    auto& m = savefile->settings.n3dsScaleMode;
                    if (ImGui::MenuItem("1x", nullptr,
                                        m == n3ds::ScaleMode::UNSCALED)) {
                        m = n3ds::ScaleMode::UNSCALED;
                    }
                    if (ImGui::MenuItem("1.5x smooth", nullptr,
                                        m == n3ds::ScaleMode::SCALED_LINEAR)) {
                        m = n3ds::ScaleMode::SCALED_LINEAR;
                    }
                    if (ImGui::MenuItem("1.5x sharp", nullptr,
                                        m == n3ds::ScaleMode::SCALED_SHARP)) {
                        m = n3ds::ScaleMode::SCALED_SHARP;
                    }
                    if (ImGui::MenuItem("1.5x ultra", nullptr,
                                        m == n3ds::ScaleMode::SCALED_ULTRA)) {
                        m = n3ds::ScaleMode::SCALED_ULTRA;
                    }
                    ImGui::EndMenu();
                }
                bool top_screen_as_main =
                    savefile->settings.n3dsMainScreenIsTop;
                if (ImGui::MenuItem("Top screen as main", nullptr,
                                    &top_screen_as_main)) {
                    savefile->settings.n3dsMainScreenIsTop = top_screen_as_main;
                }
                ImGui::MenuItem("Set clear color", nullptr,
                                &s_show_clear_color_dlg);
                if (ImGui::MenuItem("Hide debug UI (tap to show)")) {
                    debug_opt.show_imgui = false;
                    debug_opt.game_receives_input = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Quit")) {
                    s_show_quit_confirmation = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Tools")) {
                ImGui::MenuItem("Stats", nullptr, &s_show_stats);
                ImGui::MenuItem("Sprites viewer", nullptr,
                                &s_show_sprites_viewer);
                ImGui::MenuItem("Background viewer", nullptr,
                                &s_show_bg_viewer);
                ImGui::MenuItem("Colour viewer", nullptr, &s_show_color_viewer);
                ImGui::MenuItem("Set mosaic", nullptr, &s_show_mosaic_cfg);
                ImGui::MenuItem("Scaling textures viewer", nullptr,
                                &s_show_scaling_tex);
                ImGui::Separator();
                ImGui::MenuItem("Dear ImGui Demo", nullptr, &s_show_imgui_demo);
                ImGui::EndMenu();
            }

            auto& style = ImGui::GetStyle();
            float width = ImGui::CalcTextSize("Game input").x +
                          ImGui::CalcTextSize("Paused").x +
                          ImGui::CalcTextSize("step").x;
            width += (ImGui::GetFrameHeight() + style.ItemInnerSpacing.x +
                      style.ItemSpacing.x) *
                         2 +
                     style.FramePadding.x * 2 + style.ItemSpacing.x;
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - width -
                                 style.WindowPadding.x);
            ImGui::Checkbox("Game input", &debug_opt.game_receives_input);
            ImGui::Checkbox("Paused", &debug_opt.pause_game);
            ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
            if (ImGui::Button("Step")) {
                debug_opt.step_frame = true;
            }
            ImGui::PopItemFlag();
            ImGui::EndMainMenuBar();
        }

        if (s_show_quit_confirmation) {
            s_show_quit_confirmation = false;
            ImGui::OpenPopup("Apotris##quit msgbox");
        }
        if (ImGui::BeginPopupModal("Apotris##quit msgbox", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Quit?");
            ImGui::PushStyleColor(ImGuiCol_Button, {0.8, 0.5, 0.0, 0.5});
            if (ImGui::Button("Quit")) {
                debug_opt.quit_requested = true;
            }
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::SetItemDefaultFocus();
            if (ImGui::Button("Do not")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    if (s_show_clear_color_dlg) {
        ImGui::SetNextWindowSize({150, 0});
        if (ImGui::Begin("Set clear color", &s_show_clear_color_dlg,
                         ImGuiWindowFlags_NoResize)) {
            ImGui::ColorPicker3("##clear color", debug_opt.clear_color);
        }
        ImGui::End();
    }

    if (s_show_stats) {
        dbg::show_stats_imgui(&s_show_stats);
    }

    if (s_show_sprites_viewer) {
        using namespace imgui_helpers;
        auto* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos, ImGuiCond_Once);
        ImGui::SetNextWindowSize(vp->WorkSize, ImGuiCond_Once);
        if (ImGui::Begin("Sprites viewer", &s_show_sprites_viewer,
                         /*ImGuiWindowFlags_HorizontalScrollbar*/ 0)) {
            if (ImGui::BeginTabBar("sprites viewer tab bar")) {
                if (ImGui::BeginTabItem("Overview")) {
                    auto& style = ImGui::GetStyle();
                    float avail_w =
                        ImGui::GetWindowWidth() -
                        (style.WindowPadding.x + style.ItemSpacing.x) * 2;
                    constexpr float item_size = cr::SPRITESHEET_SPRITE_W + 2;
                    int num_cols = std::max(int(avail_w / item_size), 1);
                    int num_rows =
                        std::ceilf((float)tng::SPRITE_OBJ_COUNT / num_cols);
                    ImVec2 grid_size = {num_cols * item_size,
                                        num_rows * item_size};

                    static bool s_show_only_enabled = false;
                    static bool s_crop_sprites = true;
                    static ImVec4 s_background = {0, 0, 0, 0};
                    ImGui::Checkbox("Show only enabled", &s_show_only_enabled);
                    ImGui::SameLine();
                    ImGui::Checkbox("Crop sprites", &s_crop_sprites);
                    ImGui::SameLine();
                    ImGui::ColorEdit4("Background", &s_background.x,
                                      ImGuiColorEditFlags_NoInputs |
                                          ImGuiColorEditFlags_NoOptions |
                                          ImGuiColorEditFlags_AlphaPreview);

                    ImGui::BeginChild("sprites overview", {0, 0}, 0, 0);

                    auto* draw_list = ImGui::GetWindowDrawList();
                    ImVec2 topleft = ImGui::GetCursorScreenPos();
                    draw_checkerboard({0, 0}, grid_size, draw_list, topleft);
                    draw_list->AddRectFilled(
                        topleft, topleft + grid_size,
                        ImGui::ColorConvertFloat4ToU32(s_background));

                    ImGui::Dummy(grid_size);

                    int row = 0;
                    int col = 0;
                    for (size_t i = 0; i < tng::SPRITE_OBJ_COUNT; i++) {
                        float x = col * item_size;
                        float y = row * item_size;
                        ImGui::SetCursorPos({x, y});
                        auto screen_pos = ImGui::GetCursorScreenPos();

                        const auto& attr = tng::sprite_obj_attrs[i];
                        if (!attr.enabled && s_show_only_enabled) {
                            draw_list->AddRectFilled(screen_pos,
                                                     {screen_pos.x + item_size,
                                                      screen_pos.y + item_size},
                                                     IM_COL32(255, 0, 0, 127));
                        } else {
                            int sheet_x = (i % cr::SPRITESHEET_NUM_SPRITES_X) *
                                          cr::SPRITESHEET_SPRITE_W;
                            int sheet_y = (i / cr::SPRITESHEET_NUM_SPRITES_X) *
                                          cr::SPRITESHEET_SPRITE_H;

                            float w = cr::SPRITESHEET_SPRITE_W;
                            float h = cr::SPRITESHEET_SPRITE_H;
                            if (s_crop_sprites) {
                                w = attr.sx;
                                h = attr.sy;
                            }
                            float u0 = sheet_x * (1. / cr::SPRITESHEET_W);
                            float u1 = u0 + w * (1. / cr::SPRITESHEET_W);
                            float v0 = 1. - sheet_y * (1. / cr::SPRITESHEET_H);
                            float v1 = v0 - h * (1. / cr::SPRITESHEET_H);
                            ImGui::Image(
                                (void*)m_sprites_render.spritesheet_tex().get(),
                                {w, h}, {u0, v0}, {u1, v1}, {1, 1, 1, 1},
                                attr.enabled ? ImVec4{0, 1, 0, 1}
                                             : ImVec4{1, 0, 0, 1});
                        }

                        col++;
                        if (col >= num_cols) {
                            col = 0;
                            row++;
                        }
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Detail")) {
                    static int s_current_selected = 0;
                    static bool s_crop_sprites = true;
                    auto& style = ImGui::GetStyle();
                    float avail_w =
                        ImGui::GetWindowWidth() -
                        (style.WindowPadding.x + style.ItemSpacing.x) * 2;
                    ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
                    ImGui::BeginDisabled(s_current_selected == 0);
                    if (ImGui::ArrowButton("##prev", ImGuiDir_Left)) {
                        s_current_selected = s_current_selected - 1;
                    }
                    ImGui::EndDisabled();
                    ImGui::SameLine();
                    ImGui::PushItemWidth(-ImGui::GetFrameHeight() -
                                         style.WindowPadding.x -
                                         style.ItemSpacing.x);
                    ImGui::SliderInt("##slider", &s_current_selected, 0,
                                     tng::SPRITE_OBJ_COUNT - 1, "#%d");
                    ImGui::PopItemWidth();
                    ImGui::SameLine();
                    ImGui::BeginDisabled(s_current_selected ==
                                         tng::SPRITE_OBJ_COUNT - 1);
                    if (ImGui::ArrowButton("##next", ImGuiDir_Right)) {
                        s_current_selected = s_current_selected + 1;
                    }
                    ImGui::EndDisabled();
                    ImGui::PopItemFlag(); // ImGuiItemFlags_ButtonRepeat

                    ImGui::Checkbox("Crop sprites", &s_crop_sprites);

                    ImGui::BeginChild("##sprite detail", {0, 0}, 0,
                                      ImGuiWindowFlags_HorizontalScrollbar);
                    draw_checkerboard({0, 0}, {cr::SPRITESHEET_SPRITE_W + 2.f,
                                               cr::SPRITESHEET_SPRITE_H + 2.f});

                    const int& i = s_current_selected;
                    auto& attr = tng::sprite_obj_attrs[i];

                    int sheet_x = (i % cr::SPRITESHEET_NUM_SPRITES_X) *
                                  cr::SPRITESHEET_SPRITE_W;
                    int sheet_y = (i / cr::SPRITESHEET_NUM_SPRITES_X) *
                                  cr::SPRITESHEET_SPRITE_H;

                    float w = cr::SPRITESHEET_SPRITE_W;
                    float h = cr::SPRITESHEET_SPRITE_H;
                    if (s_crop_sprites) {
                        w = attr.sx;
                        h = attr.sy;
                    }
                    float u0 = sheet_x * (1. / cr::SPRITESHEET_W);
                    float u1 = u0 + w * (1. / cr::SPRITESHEET_W);
                    float v0 = 1. - sheet_y * (1. / cr::SPRITESHEET_H);
                    float v1 = v0 - h * (1. / cr::SPRITESHEET_H);
                    ImGui::BeginGroup();
                    ImGui::Image(
                        (void*)m_sprites_render.spritesheet_tex().get(), {w, h},
                        {u0, v0}, {u1, v1}, {1, 1, 1, 1},
                        attr.enabled ? ImVec4{0, 1, 0, 1} : ImVec4{1, 0, 0, 1});
                    ImGui::SetCursorPosY(cr::SPRITESHEET_SPRITE_H + 2 +
                                         style.ItemSpacing.y);
                    ImGui::Checkbox("Enabled", &attr.enabled);
                    ImGui::EndGroup();

                    if (ImGui::GetWindowWidth() <
                        cr::SPRITESHEET_SPRITE_W * 2.f) {
                        ImGui::SetCursorPos({0, cr::SPRITESHEET_SPRITE_W + 2.f +
                                                    style.ItemSpacing.x});
                    } else {
                        ImGui::SetCursorPos({cr::SPRITESHEET_SPRITE_W + 2.f +
                                                 style.ItemSpacing.y,
                                             0});
                    }
                    ImGui::BeginGroup();
                    ImGui::Text("Pos: (%d, %d)", attr.x, attr.y);
                    ImGui::Text("Size: %d x %d", attr.sx, attr.sy);
                    int priority = attr.priority >> 4;
                    ImGui::PushItemWidth(100);
                    if (ImGui::SliderInt("##priority", &priority, 0, 3)) {
                        attr.priority =
                            (priority << 4) | (attr.priority & 0x03);
                    }
                    ImGui::PopItemWidth();
                    ImGui::SameLine();
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("Priority (%d)", attr.priority);
                    ImGui::Text("Flip: %s %s", attr.flipx ? "[X]" : "[ ]",
                                attr.flipy ? "[Y]" : "[ ]");
                    ImGui::Text("Scale: %.2f %.2f", attr.scalex, attr.scaley);
                    ImGui::Text("DblSz: %s", attr.doubleSize ? "yes" : "no");
                    ImGui::Text("Affine: %s", attr.affine ? "yes" : "no");
                    ImGui::Text("Mosaic: %s", attr.mosaic ? "yes" : "no");
                    ImGui::Text("Blend: %s", attr.blend ? "yes" : "no");
                    ImGui::Text("Palette: %d", attr.palette);
                    ImGui::Text("Tile start: %d", attr.tile_start);
                    ImGui::EndGroup();

                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Spritesheet")) {
                    ImGui::BeginChild("spritesheet frame", {0, 0}, 0,
                                      ImGuiWindowFlags_HorizontalScrollbar);
                    ImVec2 topleft = ImGui::GetCursorScreenPos();
                    draw_checkerboard({0, 0}, {cr::SPRITESHEET_W + 2.f,
                                               cr::SPRITESHEET_H + 2.f});
                    ImGui::Image(
                        (void*)m_sprites_render.spritesheet_tex().get(),
                        {cr::SPRITESHEET_W, cr::SPRITESHEET_H}, {0, 1}, {1, 0},
                        {1, 1, 1, 1}, {1, 0, 0, 1});
                    if (ImGui::IsWindowFocused() &&
                        ImGui::IsMouseHoveringRect(
                            topleft, topleft + ImVec2{cr::SPRITESHEET_W + 2,
                                                      cr::SPRITESHEET_H + 2})) {
                        ImVec2 mouse = ImGui::GetMousePos() - topleft;
                        const int tx = std::min(std::max((int)mouse.x - 1, 0),
                                                (int)cr::SPRITESHEET_W) /
                                       cr::SPRITESHEET_SPRITE_W;
                        const int ty = std::min(std::max((int)mouse.y - 1, 0),
                                                (int)cr::SPRITESHEET_H) /
                                       cr::SPRITESHEET_SPRITE_H;
                        const int sprite_idx =
                            tx + ty * cr::SPRITESHEET_NUM_SPRITES_X;
                        ImGui::BeginTooltip();
                        ImGui::Text("Sprite #%d", sprite_idx);
                        ImGui::EndTooltip();
                        ImVec2 tileTl =
                            topleft + ImVec2(tx * cr::SPRITESHEET_SPRITE_W,
                                             ty * cr::SPRITESHEET_SPRITE_H);
                        ImGui::GetWindowDrawList()->AddRect(
                            tileTl,
                            tileTl + ImVec2{cr::SPRITESHEET_SPRITE_W + 2,
                                            cr::SPRITESHEET_SPRITE_H + 2},
                            IM_COL32(0, 255, 0, 255));
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Misc")) {
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted("Blend");
                    ImGui::SameLine();
                    ImGui::Checkbox("Top", &tng::blend_top_sprites);
                    ImGui::SameLine();
                    ImGui::Checkbox("Bottom", &tng::blend_bottom_sprites);

                    ImGui::Checkbox("Clip Window",
                                    &cr::sprites_clip_window.enabled);
                    ImGui::PushItemWidth(75);
                    ImGui::InputInt("l##scissor left",
                                    &cr::sprites_clip_window.left, 1, 10);
                    ImGui::SameLine();
                    ImGui::InputInt("t##scissor top",
                                    &cr::sprites_clip_window.top, 1, 10);
                    ImGui::InputInt("r##scissor right",
                                    &cr::sprites_clip_window.right, 1, 10);
                    ImGui::SameLine();
                    ImGui::InputInt("b##scissor bottom",
                                    &cr::sprites_clip_window.bottom, 1, 10);
                    ImGui::PopItemWidth();

                    auto scissor = cr::clip_to_scissor(cr::sprites_clip_window);
                    ImGui::Text("Scissor: (%d, %d), (%d, %d)", scissor.left,
                                scissor.top, scissor.right, scissor.bottom);

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

    if (s_show_bg_viewer) {
        static MemoryEditor s_memory_editor = []() {
            MemoryEditor me;
            me.Cols = 8;
            me.PreviewDataType = ImGuiDataType_U16;
            return me;
        }();

        {
            MemoryEditor::Sizes s;
            auto* vp = ImGui::GetMainViewport();
            ImGui::SetNextWindowSize(vp->WorkSize, ImGuiCond_Once);
            ImGui::SetNextWindowPos(vp->WorkPos, ImGuiCond_Once);
        }
        if (ImGui::Begin("Background viewer", &s_show_bg_viewer)) {
            static bool s_show_mem_edit = false;
            if (ImGui::BeginTabBar("bg viewer tab bar")) {

                if (ImGui::BeginTabItem("Info")) {
                    ImGui::BeginChild("info frame", {0, 0}, 0,
                                      ImGuiWindowFlags_HorizontalScrollbar);
                    for (size_t i = 0; i < tng::BG_LAYERS_COUNT; i++) {
                        ImGui::PushID(i);
                        auto& l = layerInfo[i];
                        char buf[32];
                        snprintf(buf, 32, "BG Layer #%d: %s##bg%d", i,
                                 l.enabled ? "Enabled" : "Disabled", i);
                        if (!ImGui::CollapsingHeader(
                                buf, ImGuiTreeNodeFlags_DefaultOpen)) {
                            ImGui::PopID();
                            continue;
                        }
                        ImGui::Checkbox("Enabled", &l.enabled);
                        ImGui::SameLine();
                        ImGui::AlignTextToFramePadding();
                        ImGui::Text("ID: %d", l.id);
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted("Blend");
                        ImGui::SameLine();
                        ImGui::Checkbox("Top", &tng::blend_top_bg[i]);
                        ImGui::SameLine();
                        ImGui::Checkbox("Bottom", &tng::blend_bottom_bg[i]);
                        int priority = l.priority >> 4;
                        ImGui::PushItemWidth(100);
                        if (ImGui::SliderInt("##priority", &priority, 0, 3)) {
                            l.priority = (priority << 4) | (l.priority & 0x03);
                        }
                        ImGui::PopItemWidth();
                        ImGui::SameLine();
                        ImGui::AlignTextToFramePadding();
                        ImGui::Text("Priority (%d)", l.priority);
                        ImGui::AlignTextToFramePadding();
                        ImGui::Text("sbb: %d", l.sbb);
                        ImGui::SameLine();
                        snprintf(buf, 32, "go to 0x%X##sbb%d",
                                 l.sbb *
                                     tng::BG_SCREENBLOCK_TILEMAP_TILE_COUNT * 2,
                                 i);
                        if (ImGui::Button(buf)) {
                            s_memory_editor.GotoAddrAndHighlight(
                                l.sbb * tng::BG_SCREENBLOCK_TILEMAP_TILE_COUNT *
                                    2,
                                (l.sbb + 1) *
                                    tng::BG_SCREENBLOCK_TILEMAP_TILE_COUNT * 2);
                            s_show_mem_edit = true;
                        }
                        ImGui::SameLine();
                        bool fillScreen =
                            tng::bg_screenblock_fill_screen[l.sbb];
                        ImGui::Checkbox("fill screen", &fillScreen);
                        ImGui::AlignTextToFramePadding();
                        ImGui::Text("cbb: %d", l.cbb);
                        ImGui::SameLine();
                        snprintf(buf, 32, "go to 0x%X##cbb%d",
                                 l.cbb * tng::BG_CHARBLOCK_TILESET_TILE_COUNT *
                                     2,
                                 i);
                        if (ImGui::Button(buf)) {
                            s_memory_editor.GotoAddrAndHighlight(
                                l.cbb * tng::BG_CHARBLOCK_TILESET_TILE_COUNT *
                                    2,
                                (l.cbb + 1) *
                                    tng::BG_CHARBLOCK_TILESET_TILE_COUNT * 2);
                            s_show_mem_edit = true;
                        }
                        ImGui::PushItemWidth(75);
                        if (ImGui::InputInt("##scroll x", &l.scroll_x, 1, 1)) {
                            l.scroll_x =
                                std::min(std::max(l.scroll_x, -512), 511);
                        }
                        ImGui::SameLine();
                        if (ImGui::InputInt("##scroll y", &l.scroll_y, 1, 1)) {
                            l.scroll_y =
                                std::min(std::max(l.scroll_y, -512), 511);
                        }
                        ImGui::PopItemWidth();
                        ImGui::SameLine();
                        ImGui::AlignTextToFramePadding();
                        ImGui::Text("Scroll");

                        ImGui::Checkbox("Clip Window",
                                        &cr::bg_clip_windows[i].enabled);
                        ImGui::PushItemWidth(75);
                        ImGui::InputInt("l##scissor left",
                                        &cr::bg_clip_windows[i].left, 1, 10);
                        ImGui::SameLine();
                        ImGui::InputInt("t##scissor top",
                                        &cr::bg_clip_windows[i].top, 1, 10);
                        ImGui::InputInt("r##scissor right",
                                        &cr::bg_clip_windows[i].right, 1, 10);
                        ImGui::SameLine();
                        ImGui::InputInt("b##scissor bottom",
                                        &cr::bg_clip_windows[i].bottom, 1, 10);
                        ImGui::PopItemWidth();

                        auto scissor =
                            cr::clip_to_scissor(cr::bg_clip_windows[i]);
                        ImGui::Text("Scissor: (%d, %d), (%d, %d)", scissor.left,
                                    scissor.top, scissor.right, scissor.bottom);

                        ImGui::PopID();
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Texture")) {
                    using namespace imgui_helpers;
                    static int s_cur_bg = 0;
                    ImGui::SliderInt("##slider", &s_cur_bg, 0,
                                     tng::BG_LAYERS_COUNT - 1, "BG Layer #%d");
                    ImGui::BeginChild("tex frame", {0, 0}, 0,
                                      ImGuiWindowFlags_HorizontalScrollbar);
                    ImVec2 topleft = ImGui::GetCursorScreenPos();
                    draw_checkerboard({0, 0}, {cr::BG_MOSAIC_TEX_W + 2.f,
                                               cr::BG_MOSAIC_TEX_H + 2.f});
                    ImGui::Image((void*)m_bg_mosaic_render.tex(s_cur_bg).get(),
                                 {cr::BG_MOSAIC_TEX_W, cr::BG_MOSAIC_TEX_H},
                                 {0, 1}, {1, 0}, {1, 1, 1, 1}, {1, 0, 0, 1});
                    if (ImGui::IsWindowFocused() &&
                        ImGui::IsMouseHoveringRect(
                            topleft,
                            topleft + ImVec2{cr::BG_MOSAIC_TEX_W + 2,
                                             cr::BG_MOSAIC_TEX_H + 2})) {
                        ImVec2 mouse = ImGui::GetMousePos() - topleft;
                        const int tx = std::min(std::max((int)mouse.x - 1, 0),
                                                (int)cr::BG_MOSAIC_TEX_W) /
                                       8;
                        // BG texture is shifted up by 40px
                        const int ty = (std::min(std::max((int)mouse.y - 1, 0),
                                                 (int)cr::BG_MOSAIC_TEX_H) +
                                        256 - 40) %
                                       256 / 8;
                        const LayerInfo& li = layerInfo[s_cur_bg];
                        const tng::BgScreenblockTilemap& screenblock =
                            tng::bg_screenblocks[li.sbb];
                        ImGui::BeginTooltip();
                        const u8* const p_sb =
                            (u8*)&screenblock[ty * cr::BG_NUM_TILES_W + tx];
                        ImGui::Text("%d, %d\noff: 0x%X (0x%X)", tx, ty,
                                    p_sb - (u8*)screenblock,
                                    p_sb - (u8*)tng::bg_screenblocks);
                        const u16 se_val =
                            screenblock[ty * cr::BG_NUM_TILES_W + tx];
                        const int char_tile_idx = se_val & 0x3ff;
                        const bool flip_h = se_val & 0x400;
                        const bool flip_v = se_val & 0x800;
                        const int use_pal = se_val >> 12;
                        ImGui::Text("idx %d pal %d%s%s", char_tile_idx, use_pal,
                                    flip_h ? " flipH" : "",
                                    flip_v ? " flipV" : "");
                        ImGui::EndTooltip();
                        ImVec2 tileTl =
                            topleft + ImVec2(tx, (ty + (40 / 8)) % 32) * 8;
                        ImGui::GetWindowDrawList()->AddRect(
                            tileTl, tileTl + ImVec2{10, 10},
                            IM_COL32(0, 255, 0, 255));
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Char sheet")) {
                    using namespace imgui_helpers;
                    static int s_cur_bg = 0;
                    ImGui::SliderInt("##slider", &s_cur_bg, 0,
                                     tng::BG_LAYERS_COUNT - 1, "BG Layer #%d");
                    ImGui::BeginChild("tex frame", {0, 0}, 0,
                                      ImGuiWindowFlags_HorizontalScrollbar);
                    ImVec2 topleft = ImGui::GetCursorScreenPos();
                    draw_checkerboard({0, 0}, {cr::BG_NUM_TILES_W * 8 + 2.f,
                                               cr::BG_NUM_TILES_H * 8 + 2.f});
                    ImGui::Image(
                        (void*)m_bg_render.char_sheet_tex(s_cur_bg).get(),
                        {cr::BG_NUM_TILES_W * 8, cr::BG_NUM_TILES_H * 8},
                        {0, 1}, {1, 0}, {1, 1, 1, 1}, {1, 0, 0, 1});
                    if (ImGui::IsWindowFocused() &&
                        ImGui::IsMouseHoveringRect(
                            topleft,
                            topleft + ImVec2{cr::BG_NUM_TILES_W * 8 + 2,
                                             cr::BG_NUM_TILES_H * 8 + 2})) {
                        ImVec2 mouse = ImGui::GetMousePos() - topleft;
                        const int tx = std::min(std::max((int)mouse.x - 1, 0),
                                                (int)cr::BG_NUM_TILES_W * 8) /
                                       8;
                        const int ty = std::min(std::max((int)mouse.y - 1, 0),
                                                (int)cr::BG_NUM_TILES_H * 8) /
                                       8;
                        const int slot = tx + ty * cr::BG_NUM_TILES_W;
                        ImGui::BeginTooltip();
                        cr::CharKey key = m_bg_render.char_slot(s_cur_bg, slot);
                        ImGui::Text("Slot %d\nchar %d, pal %d", slot,
                                    cr::charkey_to_char_idx(key),
                                    cr::charkey_to_palette(key));
                        ImGui::EndTooltip();
                        ImVec2 tileTl = topleft + ImVec2(tx, ty) * 8;
                        ImGui::GetWindowDrawList()->AddRect(
                            tileTl, tileTl + ImVec2{10, 10},
                            IM_COL32(0, 255, 0, 255));
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem(
                        "Memory", nullptr,
                        s_show_mem_edit ? ImGuiTabItemFlags_SetSelected : 0)) {
                    s_show_mem_edit = false;
                    ImGui::BeginChild("mem edit", {0, 0}, 0,
                                      ImGuiWindowFlags_NoScrollbar);
                    s_memory_editor.DrawContents(tng::bg_storage,
                                                 sizeof(tng::bg_storage), 0);
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

    if (s_show_color_viewer) {
        using namespace imgui_helpers;
        auto* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos, ImGuiCond_Once);
        ImGui::SetNextWindowSize(vp->WorkSize, ImGuiCond_Once);
        if (ImGui::Begin("Colour viewer", &s_show_color_viewer,
                         /*ImGuiWindowFlags_HorizontalScrollbar*/ 0)) {
            if (ImGui::BeginTabBar("palette tab bar")) {

                if (ImGui::BeginTabItem("Palette")) {
                    ImGui::BeginTable("table", 2, ImGuiTableFlags_ScrollY);
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted("Native");
                        ImGui::PushID("##native");
                        // ImGui::BeginChild("##native", {0, 0}, 0,
                        // ImGuiWindowFlags_AlwaysHorizontalScrollbar |
                        // ImGuiWindowFlags_NoScrollbar);
                        {
                            for (int i = 0; i < 32; i++) {
                                ImGui::PushID(i);
                                ImGui::AlignTextToFramePadding();
                                ImGui::Text("%d", i);
                                ImGui::SameLine(16);
                                ImGui::BeginGroup();
                                for (int j = 0; j < 16; j++) {
                                    ImGui::PushID(j);
                                    float col[3];
                                    u16& colorgb5 =
                                        tng::palettes[i / 16][i % 16][j];
                                    col[0] =
                                        ((colorgb5 >> 11) & 0b0001'1111) / 31.f;
                                    col[1] =
                                        ((colorgb5 >> 6) & 0b0001'1111) / 31.f;
                                    col[2] =
                                        ((colorgb5 >> 1) & 0b0001'1111) / 31.f;
                                    if (ImGui::ColorEdit3(
                                            "##color", col,
                                            ImGuiColorEditFlags_NoInputs |
                                                ImGuiColorEditFlags_NoOptions)) {
                                        colorgb5 =
                                            (u16)std::roundf(col[0] * 31.f)
                                                << 11 |
                                            ((u16)std::roundf(col[1] * 31.f)
                                             << 6) |
                                            ((u16)std::roundf(col[2] * 31.f)
                                             << 1) |
                                            0x1;
                                    }
                                    ImGui::SameLine();
                                    ImGui::PopID();
                                }
                                ImGui::EndGroup();
                                ImGui::PopID();
                            }
                        }
                        // ImGui::EndChild();
                        ImGui::PopID();

                        ImGui::PushID("##tilengine");
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted("Tilengine");
                        // ImGui::BeginChild("##tilengine", {0, 0}, 0,
                        // ImGuiWindowFlags_AlwaysHorizontalScrollbar |
                        // ImGuiWindowFlags_NoScrollbar);
                        {
                            for (int i = 0; i < 32; i++) {
                                ImGui::PushID(i);
                                ImGui::AlignTextToFramePadding();
                                ImGui::Text("%d", i);
                                ImGui::SameLine(16);
                                ImGui::BeginGroup();
                                for (int j = 0; j < 16; j++) {
                                    ImGui::PushID(j);
                                    float col[3];
                                    u8* rgb8 =
                                        TLN_GetPaletteData(palettes[i], j);
                                    col[0] = (rgb8[0] & 0xff) / 255.f;
                                    col[1] = (rgb8[1] & 0xff) / 255.f;
                                    col[2] = (rgb8[2] & 0xff) / 255.f;
                                    ImGui::ColorEdit3(
                                        "##color", col,
                                        ImGuiColorEditFlags_NoInputs |
                                            ImGuiColorEditFlags_NoOptions);
                                    ImGui::SameLine();
                                    ImGui::PopID();
                                }
                                ImGui::EndGroup();
                                ImGui::PopID();
                            }
                        }
                        // ImGui::EndChild();
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Gradient")) {
                    ImGui::Checkbox("Gradient enabled", &gradientEnabled);
                    ImGui::BeginChild("##native", {0, 0}, 0,
                                      ImGuiWindowFlags_HorizontalScrollbar);
                    for (size_t i = 0; i <= cr::GRADIENT_LINES_COUNT; i++) {
                        ImGui::PushID(i);
                        if (i % 16 == 0) {
                            ImGui::Text("%zd", i);
                            ImGui::SameLine(16);
                        } else {
                            ImGui::SameLine();
                        }
                        float col[3];
                        u16& colorgb5 = gradientTable[i];
                        col[0] = (colorgb5 & 0b0001'1111) / 31.f;
                        col[1] = ((colorgb5 >> 5) & 0b0001'1111) / 31.f;
                        col[2] = ((colorgb5 >> 10) & 0b0001'1111) / 31.f;
                        if (ImGui::ColorEdit3(
                                "##color", col,
                                ImGuiColorEditFlags_NoInputs |
                                    ImGuiColorEditFlags_NoOptions)) {
                            colorgb5 = (u16)std::roundf(col[0] * 31.f) |
                                       ((u16)std::roundf(col[1] * 31.f) << 5) |
                                       ((u16)std::roundf(col[2] * 31.f) << 10);
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

    if (s_show_mosaic_cfg) {
        ImGui::SetNextWindowSize({200, 0}, ImGuiCond_Once);
        if (ImGui::Begin("Mosaic", &s_show_mosaic_cfg)) {
            int x = tng::bg_mosaic_x;
            int y = tng::bg_mosaic_y;
            bool changed = false;

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("X");
            ImGui::SameLine(16);
            changed |= ImGui::SliderInt("##x", &x, 0, 15);

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Y");
            ImGui::SameLine(16);
            changed |= ImGui::SliderInt("##y", &y, 0, 15);

            if (changed) {
                setMosaic(x, y);
            }
        }
        ImGui::End();
    }

    if (s_show_scaling_tex) {
        auto* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos, ImGuiCond_Once);
        ImGui::SetNextWindowSize(vp->WorkSize, ImGuiCond_Once);
        if (ImGui::Begin("Scaling textures", &s_show_scaling_tex)) {
            if (ImGui::BeginTabBar("scaling tex tab bar")) {
                if (ImGui::BeginTabItem("1x texture")) {
                    ImGui::BeginChild("tex frame", {0, 0}, 0,
                                      ImGuiWindowFlags_HorizontalScrollbar);

                    ImGui::Image((void*)m_scene_render_target.tex1x().get(),
                                 {256, 512}, {0, 1}, {1, 0}, {1, 1, 1, 1},
                                 {1, 0, 0, 1});

                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Scaling texture")) {
                    static int s_scale = 1;
                    ImGui::RadioButton("1x", &s_scale, 1);
                    ImGui::SameLine();
                    ImGui::RadioButton("1/2x", &s_scale, 2);
                    ImGui::SameLine();
                    ImGui::RadioButton("1/3x", &s_scale, 3);

                    ImGui::BeginChild("tex frame", {0, 0}, 0,
                                      ImGuiWindowFlags_HorizontalScrollbar);

                    ImGui::Image(
                        (void*)m_scene_render_target.tex_scaling().get(),
                        {cr::SCREEN_TARGET_TEX_W / (float)s_scale,
                         cr::SCREEN_TARGET_TEX_H / (float)s_scale},
                        {0, 1}, {1, 0}, {1, 1, 1, 1}, {1, 0, 0, 1});

                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }
            }

            ImGui::EndTabBar();
        }
        ImGui::End();
    }

    if (s_show_imgui_demo) {
        ImGui::ShowDemoWindow(&s_show_imgui_demo);
    }

    if (debug_opt.tilengine_render) {
        auto* vp = ImGui::GetMainViewport();
        ImGui::GetBackgroundDrawList()->AddImage(
            tilengine_tex, {0, vp->WorkPos.y + 1},
            {SCREEN_WIDTH, vp->WorkPos.y + 1 + SCREEN_HEIGHT}, {0, 1},
            {SCREEN_WIDTH / 256.f, 1.f - SCREEN_HEIGHT / 256.f});
    }
}

#endif /* DEBUG_UI */

#ifdef DEBUG_UI
static bool blendSprites = false;
constexpr int offsetx = 0;
constexpr int offsety = 0;
// static int offsetx = (SCREEN_WIDTH - 240)/2;
// static int offsety = (SCREEN_HEIGHT - 160)/2;
#endif

void toggleBG(int layer, bool state) {
#ifdef DEBUG_UI
    int n = layerInfo[layer].id;

    if (state) {
        TLN_EnableLayer(n);
    } else {
        TLN_DisableLayer(n);
    }
#endif
    layerInfo[layer].enabled = state;
}

void toggleSprites(bool state) {
    // TODO: stub
}

INLINE u8 buildLayerPrio(int layer, int priority) {
    return ((priority & 0x3) << 4) + (layer & 0x3);
}

void buildBG(int layer, int cbb, int sbb, int size, int prio, int mos) {
    int p = buildLayerPrio(layer, prio);

    layerInfo[layer].priority = p;
    layerInfo[layer].sbb = sbb;
    layerInfo[layer].cbb = cbb;

#ifdef DEBUG_UI
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
#endif
}

void showSprites(int n) {
#ifdef DEBUG_UI
    std::list<int> spriteList;

    for (int i = 3; i >= 0; i--) {
        for (int counter = 127; counter >= 0; counter--) {
            const OBJ_ATTR* sprite = &obj_buffer[counter];
            if (sprite->enabled && sprite->priority == buildLayerPrio(0, i) &&
                sprite->id >= 0 && sprite->id < 128) {
                spriteList.push_back(sprite->id);
            }
        }
    }

    for (int i = 0; i < 128; i++) {
        TLN_DisableSprite(i);
    }

    int counter = 0;
    for (auto const& id : spriteList) {
        const OBJ_ATTR* sprite = &obj_buffer[id];

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
            TLN_SetSpritePivot(counter, 0.5, 0.5);
            log(sprite->scalex);
            TLN_SetSpriteScaling(counter, sprite->scalex, sprite->scaley);
            float scaleOffset = (1 + sprite->doubleSize) / 2.0;
            TLN_SetSpritePosition(
                counter, sprite->x + offsetx + sprite->sx * scaleOffset,
                sprite->y + offsety + sprite->sy * scaleOffset);
        } else if (sprite->affine && sprite->rotate) {
            TLN_SetSpritePivot(counter, 0, 0);
            TLN_SetSpritePosition(counter, sprite->x + offsetx,
                                  sprite->y + offsety);
            TLN_ResetSpriteScaling(counter);
        } else {
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
                ((sprite->priority == buildLayerPrio(0, 0)) * FLAG_PRIORITY) +
                FLAG_MASKED);

        counter++;
    }
#endif
}

#ifdef DEBUG_UI
static void loadPaletteTE(int palette, int index, const void* src, int count);
#endif

void loadPalette(int palette, int index, const void* src, int count) {
    tng::palette_gba_to_tng((const u16*)src,
                            &tng::palettes[palette / 16][palette % 16][index],
                            count);

#ifdef DEBUG_UI
    loadPaletteTE(palette, index, src, count);
#endif
}

#ifdef DEBUG_UI
static void loadPaletteTE(int palette, int index, const void* src, int count) {
    // TODO: stub
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
#endif

void loadTiles(int tileset, int index, const void* src, int count) {
    memcpy32_fast(&tng::bg_charblocks[tileset][index], src, count * 8);

#ifdef DEBUG_UI
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
#endif
}

void loadTilemap(int tilemap, int index, const void* src, int count) {
    u16* dst = tng::bg_screenblocks[tilemap];
    memcpy16_fast(&dst[index], src, count);

    tng::bg_screenblock_fill_screen[tilemap] = false;

#ifdef DEBUG_UI
    u16* source = (u16*)src;

    for (int i = 0; i < count; i++) {
        setTileTLN(tilemap, (i + index) % TILEMAP_SIZE,
                   (i + index) / TILEMAP_SIZE, source[i]);
    }
#endif
}

void clearTilemap(int tilemap) {
    memset16(tng::bg_screenblocks[tilemap], 0, 32 * 32);

    tng::bg_screenblock_fill_screen[tilemap] = false;

#ifdef DEBUG_UI
    for (int i = 0; i < TILEMAP_SIZE; i++) {
        for (int j = 0; j < TILEMAP_SIZE; j++) {
            setTileTLN(tilemap, i, j, 0);
        }
    }
#endif
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

    int index = sprite - obj_buffer;
    assert(index >= 0 && index < 128);
    tng::sprite_obj_dirty_flag[index] = true;
};

void sprite_set_attr(OBJ_ATTR* sprite, int shape, int size, int tile_start,
                     int palette, int priority) {
    sprite->sx = sizeTable[shape][size][0];
    sprite->sy = sizeTable[shape][size][1];

    sprite->tile_start = tile_start;
    sprite->palette = palette;
    sprite->priority = buildLayerPrio(0, priority);
    sprite->affine = false;
    sprite->doubleSize = false;
    sprite->flipx = false;
    sprite->flipy = false;
    sprite->rotate = false;
    sprite->scalex = 1;
    sprite->scaley = 1;
    sprite->blend = false;
    sprite->mosaic = false;

    sprite->affine_mat[0] = 1;
    sprite->affine_mat[1] = 0;
    sprite->affine_mat[2] = 0;
    sprite->affine_mat[3] = 1;

    int index = sprite - obj_buffer;
    assert(index >= 0 && index < 128);
    tng::sprite_obj_dirty_flag[index] = true;
};

void setLayerScroll(int layer, int x, int y) {
    layerInfo[layer].scroll_x = x;
    layerInfo[layer].scroll_y = y;

#ifdef DEBUG_UI
    if (!layerInfo[layer].enabled)
        return;

    TLN_SetLayerPosition(layerInfo[layer].id, x - offsetx, y - offsety);
#endif
}

void loadSpriteTiles(int index, const void* src, int lengthX, int lengthY) {
    memcpy32_fast(&tng::sprite_tile_images[index], src, lengthX * lengthY * 8);
    for (int i = 0; i < lengthX + lengthY; i++) {
        tng::sprite_tile_dirty_flag[index + i] = true;
    }

#ifdef DEBUG_UI
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
#endif
}

void loadSpriteTilesPartial(int index, const void* src, int tx, int ty,
                            int lengthX, int lengthY, int rowLength) {
    memcpy32_fast(&tng::sprite_tile_images[index + ty * rowLength + tx], src,
                  lengthX * lengthY * 8);
    for (int i = 0; i < lengthX + lengthY; i++) {
        tng::sprite_tile_dirty_flag[index + ty * rowLength + tx + i] = true;
    }

#ifdef DEBUG_UI
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
#endif
}

void clearSpriteTiles(int index, int lengthX, int lengthY) {
    memset32_fast(&tng::sprite_tile_images[index], 0, lengthX * lengthY * 8);
    for (int i = 0; i < lengthX + lengthY; i++) {
        tng::sprite_tile_dirty_flag[index + i] = true;
    }

#ifdef DEBUG_UI
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
#endif
}

void clearSpriteTile(int index, int tx, int ty, int width) {
    memset32_fast(&tng::sprite_tile_images[index + ty * width + tx], 0, 8);
    tng::sprite_tile_dirty_flag[index + ty * width + tx] = true;

#ifdef DEBUG_UI
    uint8_t tmp[8 * 8];
    memset(tmp, 0, 64);

    TLN_SpriteData d;
    memcpy(d.name, "tile", 5);
    d.w = 8;
    d.h = 8;

    d.x = 8 * (tx + index);
    d.y = 8 * ty;

    TLN_SetSpritesetData(spriteset, 127, &d, tmp, 8);
#endif
}

void enableBlend(int info) {
    tng::blend_mode = (tng::BlendMode)((info >> 6) & 0x03);
    u32 blend_top = info & 0x3f;
    tng::blend_top_bg[0] = blend_top & 0b1;
    tng::blend_top_bg[1] = blend_top & 0b10;
    tng::blend_top_bg[2] = blend_top & 0b100;
    tng::blend_top_bg[3] = blend_top & 0b1000;
    tng::blend_top_sprites = blend_top & 0b10000;
    tng::blend_top_backdrop = blend_top & 0b100000;
    u32 blend_bottom = (info >> 8) & 0x3f;
    tng::blend_bottom_bg[0] = blend_bottom & 0b1;
    tng::blend_bottom_bg[1] = blend_bottom & 0b10;
    tng::blend_bottom_bg[2] = blend_bottom & 0b100;
    tng::blend_bottom_bg[3] = blend_bottom & 0b1000;
    tng::blend_bottom_sprites = blend_bottom & 0b10000;
    tng::blend_bottom_backdrop = blend_bottom & 0b100000;

#ifdef DEBUG_UI
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
#endif
}

void setTiles(int tilemap, int index, int count, u32 value) {
    u16* dst = tng::bg_screenblocks[tilemap];
    memset16(&dst[index], value, count);

    if (count == 32 * 32) {
        tng::bg_screenblock_fill_screen[tilemap] = true;
    } else {
        tng::bg_screenblock_fill_screen[tilemap] = false;
    }

#ifdef DEBUG_UI
    if (count == 32 * 32)
        count = TILEMAP_SIZE * TILEMAP_SIZE;

    // TLN_Tile t = new Tile();
    Tile t_{};
    TLN_Tile t = &t_;
    t->value = value;

    for (int i = index; i < count + index; i++) {
        TLN_SetTilemapTile(tilemaps[tilemap], (i / TILEMAP_SIZE),
                           (i % TILEMAP_SIZE), t);
    }

    // delete t;
#endif
}

void clearSprites(int n) {
    for (int counter = 0; counter < n; counter++) {
        sprite_hide(&obj_buffer[counter]);
    }
}

void setPaletteColor(int palette, int index, u16 color, int count) {
    memset16(&tng::palettes[palette / 16][palette % 16][index],
             tng::color_gba_to_tng(color), count);

#ifdef DEBUG_UI
    ColorSplit c = ColorSplit(color);
    if (palette == 0 && index == 0)
        TLN_SetBGColor(c.r, c.g, c.b);

    for (int i = 0; i < count; i++) {
        TLN_SetPaletteColor(palettes[palette], index + i, c.r, c.g, c.b);
    }
#endif
}

void sprite_enable_mosaic(OBJ_ATTR* sprite) {
    sprite->mosaic = true;
    int index = sprite - obj_buffer;
    assert(index >= 0 && index < 128);
    tng::sprite_obj_dirty_flag[index] = true;
}

void sprite_set_size(OBJ_ATTR* sprite, FIXED size, int aff_id) {
    float n = fx2float(size);
    if (size == 0 || n == 0)
        return;

    float scale = 1 / (n);

    sprite->scalex = scale;
    sprite->scaley = scale;

    sprite->affine_mat[0] = scale;
    sprite->affine_mat[1] = 0;
    sprite->affine_mat[2] = 0;
    sprite->affine_mat[3] = scale;

    int index = sprite - obj_buffer;
    assert(index >= 0 && index < 128);
    tng::sprite_obj_dirty_flag[index] = true;
}

void sprite_rotscale(OBJ_ATTR* sprite, FIXED sizex, FIXED sizey, int angle,
                     int aff_id) {
    // sprite->affine = false;
    sprite->flipx = (sizex < 0);
    // sprite->flipy = (sizey < 0);

    int n = angle / 0x4000;

    if ((abs(n)) % 2 == 1) {
        sprite->rotate = true;
    }

    // if(n > 1){
    //     sprite->flipy = !sprite->flipy;
    // }

    // sprite_set_size(sprite, sizex, aff_id);

    float angle_f = (float)angle * (M_PI * 2.f / 0x10000);
    float fcos = std::cosf(angle_f);
    float fsin = std::sinf(angle_f);
    sprite->affine_mat[0] = fcos;
    sprite->affine_mat[1] = fsin;
    sprite->affine_mat[2] = -fsin;
    sprite->affine_mat[3] = fcos;

    int index = sprite - obj_buffer;
    assert(index >= 0 && index < 128);
    tng::sprite_obj_dirty_flag[index] = true;
};

void sprite_enable_affine(OBJ_ATTR* sprite, int affineId, bool doubleSize) {
    sprite->affine = true;
    sprite->doubleSize = doubleSize;

    int index = sprite - obj_buffer;
    assert(index >= 0 && index < 128);
    tng::sprite_obj_dirty_flag[index] = true;
}

void sprite_enable_flip(OBJ_ATTR* sprite, bool flipX, bool flipY) {
    sprite->flipx = flipX;
    sprite->flipy = flipY;

    int index = sprite - obj_buffer;
    assert(index >= 0 && index < 128);
    tng::sprite_obj_dirty_flag[index] = true;
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
    u16* dst = &tng::palettes[palette / 16][palette % 16][index];
    tng::palette_tng_to_gba(dst, dst, count);
    color_fade(dst, src, color, count, alpha);
    tng::palette_gba_to_tng(dst, dst, count);

#ifdef DEBUG_UI
    u16 temp[count];

    color_fade(temp, src, color, count, alpha);

    int remaining = index + count;

    int counter = 0;
    while (remaining > 0) {

        int m = remaining % 16;
        if (m) {

            loadPaletteTE(palette, index, temp, m);

            remaining -= m;
        } else if (counter == 0 && index) {
            loadPaletteTE(palette, index, &temp[0], 16 - index);
            remaining -= 16;
        } else {
            loadPaletteTE(palette + counter, 0, &temp[count - remaining], 16);
            remaining -= 16;
        }

        counter++;
    }
#endif
}

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

void color_adj_MEM_tng(COLOR* dst, const COLOR* src, u32 count, u32 bright) {
    u32 ii, clr;
    int rr, gg, bb;

    bright >>= 3;
    for (ii = 0; ii < count; ii++) {
        clr = src[ii];

        rr = ((clr) & 31) + bright;
        gg = ((clr >> 5) & 31) + bright;
        bb = ((clr >> 10) & 31) + bright;

        dst[ii] = tng::color_rgb5a1_unsafe(bf_clamp(rr, 5), bf_clamp(gg, 5),
                                           bf_clamp(bb, 5));
    }
}

void color_adj_brightness(int palette, int index, const COLOR* src, u32 count,
                          FIXED bright) {
    color_adj_MEM_tng(&tng::palettes[palette / 16][palette % 16][index], src,
                      count, bright);

#ifdef DEBUG_UI
    COLOR dst[count];

    color_adj_MEM(dst, src, count, bright);

    loadPaletteTE(palette, index, dst, count);
#endif
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
}

void clearTiles(int tileset, int index, int count) {
    memset32_fast(&tng::bg_charblocks[tileset][index], 0, 8 * count);

#ifdef DEBUG_UI
    u8 tmp[8 * 8];

    memset(tmp, 0, 64);
    for (int ii = 0; ii < count; ii++) {

        TLN_SetTilesetPixels(tilesets[tileset], index + ii, tmp, 8);
    }
#endif
}

void clearTilemapEntries(int tilemap, int index, int count) {
    u16* dst = tng::bg_screenblocks[tilemap];
    memset16(&dst[index], 0, count);

    tng::bg_screenblock_fill_screen[tilemap] = false;

#ifdef DEBUG_UI
    // log(std::to_string(index) + " " + std::to_string(count));
    for (int i = index; i < count + index; i++) {
        setTileTLN(tilemap, i % 32, i / 32, 0);
    }
#endif
}

void setMosaic(int sx, int sy) {
    tng::bg_mosaic_x = sx;
    tng::bg_mosaic_y = sy;

#ifdef DEBUG_UI
    for (int i = 0; i < 4; i++) {
        if (!sx && !sy)
            TLN_DisableLayerMosaic(i);
        else
            TLN_SetLayerMosaic(i, sx + 1, sy + 1);
    }
#endif
}

COLOR rgb8to5(u32* c) {
    int b = (*c >> (16 + 3)) & 0x1f;
    int g = (*c >> (8 + 3)) & 0x1f;
    int r = (*c >> (0 + 3)) & 0x1f;

    return r | (g << 5) | (b << 10);
}

void savePalette(COLOR* dst) {
    tng::palette_tng_to_gba((u16*)tng::palettes.data(), dst, 512);

#ifdef DEBUG_UI
    // for(int i = 0; i < 32; i++)
    //     for(int j = 0; j < 16; j++)
    //         *dst++ = rgb8to5((u32*) TLN_GetPaletteData(palettes[i], j));
#endif
}

void color_fade_fast(int palette, int index, const COLOR* src, COLOR color,
                     int count, u32 alpha) {
    u16* dst = &tng::palettes[palette / 16][palette % 16][index];
    tng::palette_tng_to_gba(dst, dst, count);
    color_fade(dst, src, color, count, alpha);
    tng::palette_gba_to_tng(dst, dst, count);

#ifdef DEBUG_UI
    u16 temp[count];

    color_fade(temp, src, color, count, alpha);

    if (count > 16) {
        int n = count / 16;

        for (int i = 0; i < (n); i++)
            loadPaletteTE(palette + i, index, &temp[i * 16], 16);
    } else {
        loadPaletteTE(palette, index, temp, count);
    }
#endif
}

void mirrorPalettes(int index, int count) {
    for (int i = index; i < 8 * 16; i++) {
        tng::palettes_bg[i / 16][i % 16] = tng::palettes_sprite[i / 16][i % 16];
    }

#ifdef DEBUG_UI
    for (int i = 0; i < count; i++) {
        COLOR temp[16];

        temp[0] = 0;

        for (int j = index; j < 16; j++)
            temp[j] = rgb8to5((u32*)TLN_GetPaletteData(palettes[i + 16], j));

        loadPaletteTE(i, 0, temp, 16);
        // for(int j = 0; j < 16; j++){
        //     setPaletteColor(i, j, rgb8to5((u32*)
        //     TLN_GetPaletteData(palettes[i + 16], j)), 1);
        // }
    }
#endif
}

void sprite_enable_blend(OBJ_ATTR* sprite) {
    sprite->blend = true;

    int index = sprite - obj_buffer;
    assert(index >= 0 && index < 128);
    tng::sprite_obj_dirty_flag[index] = true;
}

void addColorToPalette(int palette, int index, COLOR color, int count) {
    for (int i = 0; i < count; i++) {
        u16 c = tng::palettes[palette / 16][palette % 16][index + i];
        c = tng::color_tng_to_gba(c);
        c += color;
        c = tng::color_gba_to_tng(c);
        tng::palettes[palette / 16][palette % 16][index + i] = c;
    }

#ifdef DEBUG_UI
    COLOR dst[count];

    for (int i = 0; i < count; i++)
        dst[i] =
            rgb8to5((u32*)TLN_GetPaletteData(palettes[palette], index + i)) +
            color;

    loadPaletteTE(palette, index, dst, count);
#endif
}

void enableLayerWindow(int layer, int x1, int y1, int x2, int y2, bool invert) {
    auto& window = cr::bg_clip_windows[layer];
    window.left = x1;
    window.top = y1;
    window.right = x2;
    window.bottom = y2;
    window.enabled = true;
    window.invert = invert;

#ifdef DEBUG_UI
    TLN_SetLayerWindow(layerInfo[layer].id, x1 + offsetx, y1 + offsety,
                       x2 + offsetx, y2 + offsety, invert);
#endif
}

void disableLayerWindow(int layer) {
    cr::bg_clip_windows[layer].enabled = false;

#ifdef DEBUG_UI
    TLN_DisableLayerWindow(layerInfo[layer].id);
#endif
}

void setSpriteMaskRegion(int shift) {
    auto& window = cr::sprites_clip_window;
    window.left = cr::SCENE_OFFSET_CENTERED.left;
    window.top = shift;
    window.right = cr::SCENE_OFFSET_CENTERED.right;
    window.bottom = shift + 164;
    window.enabled = true;
    window.invert = false;

#ifdef DEBUG_UI
    const int top = offsety + 164 + shift;
    const int bot = offsety + shift;

    TLN_SetSpritesMaskRegion(top, bot);
#endif
}
