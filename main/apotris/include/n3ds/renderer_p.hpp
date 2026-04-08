#pragma once

#include "renderer.hpp"

#include <citro3d.h>

#include <cstddef>

#include <array>

namespace cr {

extern bool rendering_enabled;

} /* namespace cr */

#ifdef DEBUG_UI
namespace dbg {

void stat(const char* key, double val);

} /* namespace dbg */

namespace imgui_helpers {

void init();
void fini();

} /* namespace imgui_helpers */

struct DebugOptions {
    bool game_receives_input = true;
    bool pause_game = false;
    bool step_frame = false;
    bool use_new3ds_speed = false;
    bool tilengine_render = false;
    bool quit_requested = false;
    bool show_imgui = false;
    bool enable_bg_mosaic = true;
    float clear_color[3] = {0.0, 0.0, 0.0};
};

extern DebugOptions s_debug_opt;
#endif

void renderer_init() noexcept;
void renderer_free() noexcept;

void renderer_render(C3D_RenderTarget* rt, bool is_top_screen) noexcept;

#ifdef DEBUG_UI
void renderer_imgui_debug_1(DebugOptions& debug_opt,
                            C3D_Tex* tilengine_tex) noexcept;
#endif

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

constexpr int TILEMAP_SIZE = 64;
