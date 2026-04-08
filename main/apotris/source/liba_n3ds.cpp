#include "platform.hpp"

#ifdef N3DS

#include "def.h"
#include "n3ds/audio.hpp"
#include "n3ds/citro3d++.hpp"
#include "n3ds/ctru++.hpp"
#include "n3ds/maybe_uninit.hpp"
#include "n3ds/renderer_p.hpp"
#include "scene.hpp"

#include <cassert>
#include <cinttypes>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdio>

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <type_traits>

#include <malloc.h>

#include <3ds.h>
#include <citro3d.h>

#ifdef DEBUG_UI
#include "imgui_citro3d.h"
#include "imgui_ctru.h"
#include <imgui.h>
#endif

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

extern "C" {

// HACK: Use Citro3D internal API to wait for queue completion.
extern void C3Di_RenderQueueWaitDone(void);
}

#ifdef DEBUG_UI
namespace dbg {

namespace _p {

constexpr size_t VALUES_COUNT = 28;

enum class ValueType {
    NIL = 0,
    SEPARATOR,
    S32,
    FLOAT_D2,
};

struct SeparatorTag_t {
} separator_tag;

union ValueData {
    s32 _s32;
    float _float;
};

ValueData values[VALUES_COUNT] = {};
ValueType values_types[VALUES_COUNT] = {};
const char* values_names[VALUES_COUNT] = {};

template <typename T> static void add_stat(const char* key, T val) {
    size_t i;
    for (i = 0; i < VALUES_COUNT; i++) {
        if (values_names[i] == key) {
            break;
        }
        if (!values_names[i]) {
            break;
        }
    }
    if (i == VALUES_COUNT) {
        fprintf(stderr, "dbg: No room for stat \"%s\"", key);
        return;
    }
    if (!values_names[i]) {
        values_names[i] = key;
    }
    if constexpr (std::is_same_v<T, SeparatorTag_t>) {
        values_types[i] = ValueType::SEPARATOR;
    } else if constexpr (std::is_same_v<T, s32>) {
        values_types[i] = ValueType::S32;
        values[i]._s32 = val;
    } else if constexpr (std::is_same_v<T, float>) {
        values_types[i] = ValueType::FLOAT_D2;
        values[i]._float = val;
    } else {
        static_assert(!std::is_same_v<T, T>, "Unhandled type");
    }
}

float fps = -1;

} // namespace _p

inline void sep(const char* key) { _p::add_stat(key, _p::separator_tag); }

inline void stat(const char* key, int val) { _p::add_stat(key, (s32)val); }

inline void stat(const char* key, long val) { _p::add_stat(key, (s32)val); }

inline void stat(const char* key, float val) { _p::add_stat(key, val); }

void stat(const char* key, double val) { _p::add_stat(key, (float)val); }

inline void show_fps(float fps) { _p::fps = fps; }

inline void hide_fps() { _p::fps = -1; }

static void print_stats() {
#if 0
    using namespace _p;
    fputs("\x1b[s\x1b[1;31H", stdout);

    if (fps >= 0) {
        static int c = 0;
        c = (c + 1) % 4;
        printf("FPS%6.2f%c", fps, "|/-\\"[c]);
    }

    for (size_t i = 0; i < VALUES_COUNT; i++) {
        switch (values_types[i]) {
        case ValueType::NIL:
            break;
        case ValueType::SEPARATOR:
            printf("\x1b[%d;31H" "%-10s", i + 2, values_names[i]);
            break;
        case ValueType::FLOAT_D2:
            printf("\x1b[%d;31H" "%-4s" "%6.2f", i + 2, values_names[i], values[i]._float);
            break;
        case ValueType::S32:
            printf("\x1b[%d;31H" "%-5s" "%5" PRId32, i + 2, values_names[i], values[i]._s32);
            break;
        }
    }

    fputs("\n\x1b[u", stdout);
#endif
}

void show_stats_imgui(bool* p_open) {
    using namespace _p;

    auto* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({240, vp->WorkPos.y}, ImGuiCond_Once);
    ImGui::SetNextWindowSize({80, 240}, ImGuiCond_Once);
    ImGui::SetNextWindowSizeConstraints({0, 0}, {FLT_MAX, vp->WorkSize.y});

    constexpr float column = 32.f;

    char buf[64];
    snprintf(buf, 64, "FPS: %.2f###stats", fps);
    if (!ImGui::Begin(buf, p_open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    constexpr auto strip_lead = [](const char* c) {
        while (c && (*c == ' ' || *c == '-')) {
            c++;
        }
        return c;
    };

    for (size_t i = 0; i < VALUES_COUNT; i++) {
        switch (values_types[i]) {
        case ValueType::NIL:
            break;
        case ValueType::SEPARATOR:
            ImGui::SeparatorText(strip_lead(values_names[i]));
            break;
        case ValueType::FLOAT_D2:
            ImGui::TextUnformatted(strip_lead(values_names[i]));
            ImGui::SameLine(column);
            ImGui::Text("%.2f", values[i]._float);
            break;
        case ValueType::S32:
            ImGui::TextUnformatted(strip_lead(values_names[i]));
            ImGui::SameLine(column);
            ImGui::Text("%.2" PRId32, values[i]._s32);
            break;
        }
    }

    ImGui::End();
}

} /* namespace dbg */
#endif

std::map<int, std::string> keyToString = {
    {KEY_A, "A"},
    {KEY_B, "B"},
    {KEY_SELECT, "Select"},
    {KEY_START, "Start"},
    {KEY_DRIGHT, "d-R"},
    {KEY_DLEFT, "d-L"},
    {KEY_DUP, "d-U"},
    {KEY_DDOWN, "d-D"},
    {KEY_R, "R"},
    {KEY_L, "L"},
    {KEY_X, "X"},
    {KEY_Y, "Y"},
    {KEY_ZL, "ZL"},
    {KEY_ZR, "ZR"},
    {KEY_TOUCH, "Touch"},
    {KEY_CSTICK_RIGHT, "c-R"},
    {KEY_CSTICK_LEFT, "c-L"},
    {KEY_CSTICK_UP, "c-U"},
    {KEY_CSTICK_DOWN, "c-D"},
    {KEY_CPAD_RIGHT, "a-R"},
    {KEY_CPAD_LEFT, "a-L"},
    {KEY_CPAD_UP, "a-U"},
    {KEY_CPAD_DOWN, "a-D"},
};

static constexpr int PIXEL_SIZE = 4;

int spriteVOffset = 0;

int blendInfo = 0;

static void onHBlank(int line);
uint8_t customBlend(uint8_t a, uint8_t b);
static void showFPS();

static std::list<float> fpsHistory;
float fps = 0;

static inline int rgb5to8(u16 color) {
    return (((color) & 0x1f) << 3) + (((color >> 5) & 0x1f) << (3 + 8)) +
           (((color >> 10) & 0x1f) << (3 + 16));
}

#ifdef DEBUG_UI
TLN_Palette palettes[32];

TLN_Tileset tilesets[4];

TLN_Tilemap tilemaps[32];

TLN_Spriteset spriteset;

TLN_SpriteData spriteData[128];

TLN_Bitmap spriteBitmap;
#endif

static int link3dsSocket = -1;

static void checkSaveDir();

class Input {
    u32 previousKeys = 0;
    u32 currentKeys = 0;

    Input(const Input&) = delete;
    Input(Input&&) = delete;
    Input& operator=(const Input&) = delete;
    Input& operator=(Input&&) = delete;

public:
    Input() {
        hidScanInput();
        previousKeys = currentKeys = hidKeysHeld() & VALID_KEY_MASK;
    }
    ~Input() = default;

    inline void scan_input() { hidScanInput(); }
    inline void key_poll();
    inline uint32_t key_is_down(uint32_t key) const;
    inline uint32_t key_hit(uint32_t key) const;
    inline uint32_t key_released(uint32_t key) const;
    inline uint32_t keys_raw() const;
};

class ApotrisN3ds {
private:
    static MaybeUninit<ApotrisN3ds> s_instance;

    ApotrisN3ds();
    ~ApotrisN3ds();

    ApotrisN3ds(const ApotrisN3ds&) = delete;
    ApotrisN3ds(ApotrisN3ds&&) = delete;
    ApotrisN3ds& operator=(const ApotrisN3ds&) = delete;
    ApotrisN3ds& operator=(ApotrisN3ds&&) = delete;

    static inline void init();
    static inline void deinit();

    friend void platformInit();
    friend void deinitialize();

    friend MaybeUninit<ApotrisN3ds>;

public:
    static constexpr ApotrisN3ds& instance();

private:
    Input m_input;

    c3d::RenderTarget m_top_screen;
    c3d::RenderTarget m_bottom_screen;

#ifdef DEBUG_UI
    ctru::linear::ptr<u32[]> m_tilengine_render_fb;
    c3d::Tex m_tilengineTex;
#endif

    TickCounter m_fps_tick_counter;

    aptHookCookie m_pause_hook_cookie;

public:
    constexpr Input& input() { return m_input; }
    constexpr Input const& input() const { return m_input; }

#ifdef DEBUG_UI
    inline void render(TickCounter& timing);
#else
    inline void render();
#endif
};

MaybeUninit<ApotrisN3ds> ApotrisN3ds::s_instance;

constexpr ApotrisN3ds& ApotrisN3ds::instance() { return s_instance.value(); }

inline void ApotrisN3ds::init() { s_instance.emplace(); }

inline void ApotrisN3ds::deinit() { s_instance.destruct(); }

void platformInit() {
    // osSetSpeedupEnable(true);

    // Init libs
    romfsInit();
    hidInit();
    gspInit();

    // Clear all of VRAM to prevent showing garbage.
    GX_MemoryFill((u32*)OS_VRAM_VADDR, 0,
                  (u32*)(OS_VRAM_VADDR + OS_VRAM_SIZE / 2),
                  GX_FILL_32BIT_DEPTH | GX_FILL_TRIGGER,
                  (u32*)(OS_VRAM_VADDR + OS_VRAM_SIZE / 2), 0,
                  (u32*)(OS_VRAM_VADDR + OS_VRAM_SIZE),
                  GX_FILL_32BIT_DEPTH | GX_FILL_TRIGGER);
    gspWaitForPSC0();

    gfxInit(GSP_BGR8_OES, GSP_BGR8_OES, true);

    // consoleInit(GFX_BOTTOM, nullptr);
    consoleDebugInit(debugDevice_SVC);

#ifdef TRACY_ENABLE
    if (true)
#else
    if (__3dslink_host.s_addr != 0)
#endif
    {
        constexpr size_t SOC_ALIGN = 0x1000;
        constexpr size_t SOC_BUFFERSIZE = 0x100000;
        static u32* SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
        socInit(SOC_buffer, SOC_BUFFERSIZE);
    }
    if (__3dslink_host.s_addr != 0) {
        link3dsSocket = link3dsStdio();
    }

#ifdef TRACY_ENABLE
    tracy::StartupProfiler();
    ZoneScopedC(tracy::Color::AliceBlue);
#endif

    puts("Starting!");

    {
        u8 model = 6;
        if (R_SUCCEEDED(cfguInit())) {
            if (R_FAILED(CFGU_GetSystemModel(&model))) {
                model = 6;
            }
            cfguExit();
        }
        if (model <= 6) {
            // Old2DS does not support wide mode
            n3ds::wideModeSupported = (model != CFG_MODEL_2DS);
        }

        constexpr u32 SYSTEM_INFO_CITRA_INFORMATION = 0x20000;
        constexpr s32 SYSTEM_INFO_CITRA_INFORMATION_EMULATOR_ID = 0;
        s64 emuId = 0;
        if (R_FAILED(
                svcGetSystemInfo(&emuId, SYSTEM_INFO_CITRA_INFORMATION,
                                 SYSTEM_INFO_CITRA_INFORMATION_EMULATOR_ID))) {
            emuId = 0;
        }
        if (emuId) {
            // Citra does not handle wide mode
            n3ds::wideModeSupported = false;
        }
    }

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

    ApotrisN3ds::init();
}

void deinitialize() {
    {
#ifdef TRACY_ENABLE
        ZoneScopedC(tracy::Color::AliceBlue);
#endif

        ApotrisN3ds::deinit();

        C3D_Fini();
    }

#ifdef TRACY_ENABLE
    tracy::ShutdownProfiler();
#endif

    // Deinit libs
    gfxExit();
    gspExit();
    hidExit();
    romfsExit();

    if (link3dsSocket >= 0) {
        // FIXME: Despite the docs for `link3dsConnectToHost` claiming that the
        // socket should be closed during application cleanup, doing it may
        // cause a crash on exit. This may have been a regression introduced to
        // devkitARM between 2025 and early 2026...
        // close(link3dsSocket);
    }
    socExit();
}

static inline c3d::RenderTarget createTopScreen() {
    // Allocate a framebuffer that fits wide mode. We will adjust its height
    // as needed.
    auto screen = c3d::RenderTargetCreate(
        GSP_SCREEN_WIDTH, GSP_SCREEN_HEIGHT_TOP_2X, GPU_RB_RGBA8, -1);
    if (!screen) {
        svcBreak(USERBREAK_PANIC);
    }
    C3D_RenderTargetSetOutput(screen.get(), GFX_TOP, GFX_LEFT,
                              GX_TRANSFER_FLIP_VERT(0) |
                                  GX_TRANSFER_OUT_TILED(0) |
                                  GX_TRANSFER_RAW_COPY(0) |
                                  GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
                                  GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |
                                  GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
    return screen;
}

static inline c3d::RenderTarget createBottomScreen() {
    auto screen = c3d::RenderTargetCreate(
        GSP_SCREEN_WIDTH, GSP_SCREEN_HEIGHT_BOTTOM, GPU_RB_RGBA8, -1);
    if (!screen) {
        svcBreak(USERBREAK_PANIC);
    }
    C3D_RenderTargetSetOutput(screen.get(), GFX_BOTTOM, GFX_LEFT,
                              GX_TRANSFER_FLIP_VERT(0) |
                                  GX_TRANSFER_OUT_TILED(0) |
                                  GX_TRANSFER_RAW_COPY(0) |
                                  GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
                                  GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |
                                  GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
    return screen;
}

#ifdef DEBUG_UI
static inline c3d::Tex createTilengineTex() {
    c3d::Tex tex{256, 256, GPU_RGB8};
    C3D_TexSetWrap(tex.get(), GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    C3D_TexSetFilter(tex.get(), GPU_NEAREST, GPU_NEAREST);
    return tex;
}
#endif

ApotrisN3ds::ApotrisN3ds()
    : m_input{}, m_top_screen{createTopScreen()},
      m_bottom_screen{createBottomScreen()}
#ifdef DEBUG_UI
      ,
      m_tilengine_render_fb{
          ctru::linear::make_ptr_nothrow<u32[]>(256 * SCREEN_HEIGHT)},
      m_tilengineTex{createTilengineTex()}
#endif
{

    renderer_init();

#ifdef DEBUG_UI
    TLN_Init(SCREEN_WIDTH, SCREEN_HEIGHT, 4, 128, 0);

    // Tilengine needs patching -
    // https://github.com/megamarc/Tilengine/issues/126
    TLN_SetRenderTarget((uint8_t*)m_tilengine_render_fb.get(), 4 * 256);

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
#endif

    osTickCounterStart(&m_fps_tick_counter);

#ifdef DEBUG_UI
    ImGui::CreateContext();
    imgui::ctru::init();
    imgui::citro3d::init();

    auto& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(320, 240);

    // disable imgui.ini file
    io.IniFilename = nullptr;

    auto& s = ImGui::GetStyle();
    s.WindowPadding = {2, 2};
    s.FramePadding = {2, 2};
    s.ItemSpacing = {4, 2};
    s.ItemInnerSpacing = {2, 2};
    s.IndentSpacing = 8;

    s.SeparatorTextPadding = {8, 1};
    s.DisplayWindowPadding = {16, 16};

    io.FontGlobalScale = 0.8;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    imgui_helpers::init();
#endif

    loadAudio("sdmc:" DATA_DIR_BASE "/");

    checkSaveDir();

    aptHook(
        &m_pause_hook_cookie,
        [](APT_HookType type, void*) {
            if (type == APTHOOK_ONSUSPEND || type == APTHOOK_ONSLEEP) {
                n3ds::wasAppPaused = true;
            }
        },
        nullptr);
}

ApotrisN3ds::~ApotrisN3ds() {
    aptUnhook(&m_pause_hook_cookie);

    freeAudio();

#ifdef DEBUG_UI
    imgui_helpers::fini();

    imgui::citro3d::exit();
    ImGui::DestroyContext();
#endif

#ifdef DEBUG_UI
    TLN_DeleteSpriteset(spriteset);

    TLN_DeleteBitmap(spriteBitmap);

    for (int i = 0; i < 32; i++)
        TLN_DeleteTilemap(tilemaps[i]);

    for (int i = 1; i < 4; i++)
        TLN_DeleteTileset(tilesets[i]);

    for (int i = 0; i < 32; i++)
        TLN_DeletePalette(palettes[i]);

    TLN_Deinit();
#endif

    renderer_free();
}

#ifdef DEBUG_UI
DebugOptions s_debug_opt;
#endif

void vsync() {
    if (!savefile->settings.n3dsMainScreenIsTop &&
        savefile->settings.n3dsScaleMode != n3ds::ScaleMode::UNSCALED) {
        // Force 4:3 for scaled bottom screen
        savefile->settings.aspectRatio = 1;
    } else {
        savefile->settings.aspectRatio = n3ds::savedAspectRatio;
    }

#ifdef DEBUG_UI
    TickCounter timing;
    osTickCounterStart(&timing);
#endif
    onVBlank();

#ifdef DEBUG_UI
    dbg::sep("   time(%)");

    osTickCounterUpdate(&timing);
    dbg::stat("Scn", osTickCounterRead(&timing) * 6.0f);

    if (s_debug_opt.step_frame) {
        s_debug_opt.step_frame = false;
        s_debug_opt.pause_game = true;
    }
#endif

#ifdef DEBUG_UI
    do {
        ApotrisN3ds::instance().render(timing);
        if (s_debug_opt.step_frame) {
            s_debug_opt.pause_game = false;
        }
        if (s_debug_opt.pause_game) {
            if (!aptMainLoop()) {
                quit();
            }
        }
        if (s_debug_opt.quit_requested) {
            quit();
        }
    } while (s_debug_opt.pause_game);
#else
    ApotrisN3ds::instance().render();
#endif
}

#ifdef DEBUG_UI
inline void ApotrisN3ds::render(TickCounter& timing)
#else
inline void ApotrisN3ds::render()
#endif
{
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

#ifdef DEBUG_UI
    if (s_debug_opt.tilengine_render) {
#ifdef TRACY_ENABLE
        ZoneScopedN("Tilengine");
        ZoneColor(tracy::Color::Pink);
        {
            ZoneScopedN("Tilengine update");
#endif
            TLN_UpdateFrame(0);
#ifdef TRACY_ENABLE
        }
#endif

        osTickCounterUpdate(&timing);
        dbg::stat("TLN", osTickCounterRead(&timing) * 6.0f);

#ifdef TRACY_ENABLE
        {
            ZoneScopedN("Tilengine blit");
#endif
            // ABGR8 to RGBA8 tex
            for (int y = 0; y < SCREEN_HEIGHT; y++) {
                for (int x = 0; x < SCREEN_WIDTH; x++) {
                    u32* src = m_tilengine_render_fb.get() + (y * 256 + x);
                    *src = ((*src & 0xff) << 24) | ((*src & 0xff00) << 8) |
                           ((*src & 0xff0000) >> 8) |
                           ((*src & 0xff000000) >> 24);
                }
            }
#ifdef TRACY_ENABLE
        }
#endif

        osTickCounterUpdate(&timing);
        dbg::stat("blt", osTickCounterRead(&timing) * 6.0f);

#ifdef TRACY_ENABLE
        {
            ZoneScopedN("Tilengine flush");
#endif
            // Flush needed since this is outside C3D frame
            GSPGPU_FlushDataCache(m_tilengine_render_fb.get(),
                                  256 * SCREEN_HEIGHT * 4);
            C3D_SyncDisplayTransfer(
                m_tilengine_render_fb.get(), GX_BUFFER_DIM(256, SCREEN_HEIGHT),
                (u32*)m_tilengineTex->data, GX_BUFFER_DIM(256, SCREEN_HEIGHT),
                GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
                    GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |
                    GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_FLIP_VERT(/*1*/ 0) |
                    GX_TRANSFER_RAW_COPY(0) |
                    GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
#ifdef TRACY_ENABLE
        }
#endif

        osTickCounterUpdate(&timing);
        dbg::stat("xfer", osTickCounterRead(&timing) * 6.0f);
    } else {
        dbg::stat("TLN", 0.f);
        dbg::stat("blt", 0.f);
        dbg::stat("xfer", 0.f);
    }
#endif /* DEBUG_UI */

    const bool top_screen_as_main = savefile->settings.n3dsMainScreenIsTop;

    if (top_screen_as_main &&
        savefile->settings.n3dsScaleMode == n3ds::ScaleMode::SCALED_ULTRA &&
        n3ds::wideModeSupported) {
        m_top_screen->frameBuf.height = GSP_SCREEN_HEIGHT_TOP_2X;
        gfxSetWide(true);
    } else {
        m_top_screen->frameBuf.height = GSP_SCREEN_HEIGHT_TOP;
        gfxSetWide(false);
    }

    // Render the scene
    C3D_FrameBegin(/*C3D_FRAME_SYNCDRAW*/ 0);
    C3D_RenderTargetClear(m_top_screen.get(), C3D_CLEAR_ALL, 0xff, 0);
    C3D_RenderTargetClear(m_bottom_screen.get(), C3D_CLEAR_ALL, 0xff, 0);

#ifdef DEBUG_UI
    osTickCounterUpdate(&timing);
    dbg::stat("Beg", osTickCounterRead(&timing) * 6.0f);
#endif

    const bool screen_zoom_fit =
        savefile->settings.n3dsScaleMode != n3ds::ScaleMode::UNSCALED;

    C3D_RenderTarget* main_screen =
        top_screen_as_main ? m_top_screen.get() : m_bottom_screen.get();
    renderer_render(main_screen, top_screen_as_main);

    if (!top_screen_as_main) {
        // HACK: Clear the top screen
        // This has the overhead of an useless DisplayTransfer, assuming
        // we really have nothing to draw on the top screen.
        C3D_FrameDrawOn(m_top_screen.get());
    }

    C3D_FrameDrawOn(m_bottom_screen.get());

#ifdef DEBUG_UI
    if (s_debug_opt.show_imgui) {
        osTickCounterUpdate(&timing);

#ifdef TRACY_ENABLE
        ZoneScopedN("ImGui");
        ZoneColor(tracy::Color::Purple);
#endif
        const C3D_TexEnv oldEnv = *C3D_GetTexEnv(0);

        imgui::ctru::newFrame(false, true, !s_debug_opt.game_receives_input);
        ImGui::NewFrame();
        renderer_imgui_debug_1(s_debug_opt, m_tilengineTex.get());

        osTickCounterUpdate(&timing);
        dbg::stat("ImGu", osTickCounterRead(&timing) * 6.0f);

#ifdef TRACY_ENABLE
        {
            ZoneScopedN("ImGui render");
#endif

            imgui::ctru::beforeRender();
            ImGui::Render();
            imgui::citro3d::render(nullptr, nullptr, m_bottom_screen.get());
#ifdef TRACY_ENABLE
        }
#endif

        *C3D_GetTexEnv(0) = oldEnv;

        osTickCounterUpdate(&timing);
        dbg::stat("ImGR", osTickCounterRead(&timing) * 6.0f);
    }
#endif /* DEBUG_UI */

    C3D_FrameEnd(0);

#ifdef DEBUG_UI
    osTickCounterUpdate(&timing);
    dbg::stat("FEnd", osTickCounterRead(&timing) * 6.0f);
#endif

    audioProcess();

#ifdef TRACY_ENABLE
    FrameMark;
#endif
    // HACK: Make sure rendering has finished before waiting for vblank.
    C3Di_RenderQueueWaitDone();
    C3D_FrameSync();

    input().scan_input();

#ifdef DEBUG_UI
    osTickCounterUpdate(&timing);
    dbg::stat("Vbl", osTickCounterRead(&timing) * 6.0f);
#endif

    osTickCounterUpdate(&m_fps_tick_counter);
    double elapsedMS = osTickCounterRead(&m_fps_tick_counter);
    fps = 1000.0f / elapsedMS;
    if (fps < 59) {
        // printf("fps stutter: %.2f (%.2f ms)\n", fps, elapsedMS);
    }

    showFPS();

#ifdef DEBUG_UI
    dbg::sep("- C3D:");
    dbg::stat("CPU", C3D_GetProcessingTime() * 6.0f);
    dbg::stat("GPU", C3D_GetDrawingTime() * 6.0f);
    dbg::stat("Buf%", C3D_GetCmdBufUsage() * 100.0f);
    dbg::sep("- Sprites");
    // dbg::stat("#\0sprite", sprite_count);
    // dbg::stat("DT#", dirty_sprite_tile_count);
    // dbg::stat("AFF#", affine_count);

    dbg::print_stats();
#endif

#ifdef DEBUG_UI
    if (hidKeysDown() & KEY_TOUCH) {
        s_debug_opt.show_imgui = true;
    }
#endif
}

void stopDMA() {};

void sleep() {};

void rumbleOutput(uint16_t strength) {}
void rumbleStop() {}

void onVBlank() {
    if (canDraw) {
        canDraw = 0;

#ifdef TRACY_ENABLE
        ZoneScopedN("Draw scene");
        ZoneColor(tracy::Color::Yellow);
#endif
        scene->draw();
    }

    frameCounter++;
};

void onHBlank(int line) {
#ifdef DEBUG_UI
    if (!gradientEnabled) {
        return;
    }

    ColorSplit c = ColorSplit(gradientTable[line]);
    TLN_SetBGColor(c.r, c.g, c.b);
#endif
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

uint8_t customBlend(uint8_t a, uint8_t b) { return (a + b + b + b + b) / 5; }

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

#ifdef DEBUG_UI
        dbg::show_fps(average);
#endif

        // HACK: Save and restore fill screen state (for credits letterboxing)
        const bool fillScreen = tng::bg_screenblock_fill_screen_p[29];

        char buff[8];
        snprintf(buff, 8, "%.2f", average);

        const int width = 30 - 2 * (savefile->settings.aspectRatio == 1);
        aprintColor(buff, width - 5, 0, 1);

        tng::bg_screenblock_fill_screen_p[29] = fillScreen;
    }
}

bool closed() {
    n3ds::wasAppPaused = false;

    if (!aptMainLoop()) {
        quit();
    }
    return true;
}

void toggleRendering(bool r) { cr::rendering_enabled = r; }

void initRumble() {};

void quit() {
    deinitialize();
    exit(0);
}

void refreshWindowSize() {}

static bool s_save_failed = false;

static void checkSaveDir() {
    std::error_code ec;
    std::filesystem::path save_path{SDMC_SAVE_FILE_PATH};
    std::filesystem::path dir_path{save_path.parent_path()};
    auto dir_status = std::filesystem::status(dir_path, ec);
    if (std::filesystem::is_directory(dir_status)) {
        return;
    }
    if (std::filesystem::exists(dir_path, ec)) {
        // Exists and _not_ a dir means it probably is a file...
        errorConf err;
        errorInit(&err, ERROR_TEXT_WORD_WRAP, CFG_LANGUAGE_EN);
        errorText(&err, "The save path `" DATA_DIR_BASE
                        "` on the SD card exists but is not a "
                        "directory.\nSaving will not work.");
        errorDisp(&err);
        s_save_failed = true;
        return;
    }
    if (!std::filesystem::create_directories(dir_path, ec)) {
        errorConf err;
        errorInit(&err, ERROR_TEXT_WORD_WRAP, CFG_LANGUAGE_EN);
        errorText(&err, "Failed to create the save folder `" DATA_DIR_BASE
                        "` on the SD card.\nSaving will not work.");
        errorDisp(&err);
        s_save_failed = true;
        return;
    }
}

void loadSavefile() {
#ifdef TRACY_ENABLE
    ZoneScoped;
    ZoneColor(tracy::Color::LimeGreen);
#endif

    std::ifstream input(SDMC_SAVE_FILE_PATH, std::ios::binary | std::ios::in);

    if (savefile == nullptr)
        savefile = new Save();

    char* src = (char*)savefile;

    input.read(src, sizeof(Save));

    if (!input) {
        log("Error when trying to load save.");
        return;
    }
}

void saveSavefile() {
#ifdef TRACY_ENABLE
    ZoneScoped;
    ZoneColor(tracy::Color::Lime);
#endif

    std::ofstream output(SDMC_SAVE_FILE_PATH, std::ios::binary | std::ios::out);

    char* dst = (char*)savefile;

    const int saveSize = 1 << 15;

    std::vector<u32> temp(saveSize / 4, 0);
    memcpy32_fast((char*)temp.data(), dst, sizeof(Save) / 4);

    output.write((char*)temp.data(), saveSize);

    if (!output) {
        log("Error when trying to write save.");
        if (!s_save_failed) {
            errorConf err;
            errorInit(&err, ERROR_TEXT_WORD_WRAP, CFG_LANGUAGE_EN);
            errorText(&err, "Failed to write save file `" SAVE_FILE_PATH
                            "`on the SD card.");
            errorDisp(&err);
            s_save_failed = true;
        }
        return;
    }
}

inline void Input::key_poll() {
    previousKeys = currentKeys;
    currentKeys = hidKeysHeld() & VALID_KEY_MASK & ~KEY_TOUCH;

#ifdef DEBUG_UI
    if (!s_debug_opt.game_receives_input) {
        currentKeys = previousKeys;
    }
#endif
}

inline uint32_t Input::key_is_down(uint32_t key) const {
    return currentKeys & key;
}

inline uint32_t Input::key_hit(uint32_t key) const {
    return (~previousKeys & currentKeys) & key;
}

inline uint32_t Input::key_released(uint32_t key) const {
    return (previousKeys & ~currentKeys) & key;
}

inline uint32_t Input::keys_raw() const {
    return currentKeys;
}

void key_poll() { ApotrisN3ds::instance().input().key_poll(); }

uint32_t key_is_down(uint32_t key) {
    return ApotrisN3ds::instance().input().key_is_down(key);
}

uint32_t key_hit(uint32_t key) {
    return ApotrisN3ds::instance().input().key_hit(key);
}

uint32_t key_released(uint32_t key) {
    return ApotrisN3ds::instance().input().key_released(key);
}

uint32_t keys_raw() {
    return ApotrisN3ds::instance().input().keys_raw();
}

// ******************
// N3DS-specific
// ******************

namespace n3ds {

bool wideModeSupported{false};

int savedAspectRatio{};

bool wasAppPaused{};

static inline SwkbdCallbackResult nameInputCallback(void* user,
                                                    const char** ppMessage,
                                                    const char* text,
                                                    size_t textlen) {
    for (size_t i = 0; i < textlen; i++) {
        if (text[i] == ' ') {
            continue;
        }
        if (text[i] >= 'A' && text[i] <= 'Z') {
            continue;
        }
        if (text[i] >= 'a' && text[i] <= 'z') {
            continue;
        }
        *ppMessage = "Name can only include\nalphabets and spaces.";
        return SWKBD_CALLBACK_CONTINUE;
    }
    return SWKBD_CALLBACK_OK;
}

bool keyboardNameInput(std::string* result, size_t length) {
    if (!savefile->settings.n3dsMainScreenIsTop) {
        // Copy the bottom screen to the top:
        bool wasWide = gfxIsWide();
        gfxSetWide(false);

        // Fill bottom screen with black
        u16 botW, botH;
        u8* bot = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &botW, &botH);
        C3D_SyncMemoryFill((u32*)bot, 0, (u32*)(bot + botW * botH * 3),
                           GX_FILL_24BIT_DEPTH | GX_FILL_TRIGGER, nullptr, 0,
                           nullptr, 0);

        // Swap bottom screen buffers and get the last framebuffer
        gspWaitForVBlank1();
        gfxScreenSwapBuffers(GFX_BOTTOM, false);
        bot = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &botW, &botH);

        u16 topW, topH;
        u8* top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &topW, &topH);
        top += (400 - 320) / 2 * 240 * 3;
        C3D_SyncTextureCopy((u32*)bot, GX_BUFFER_DIM(240 * 3 / 16, 0),
                            (u32*)top, GX_BUFFER_DIM(240 * 3 / 16, 0),
                            240 * 320 * 3, GX_TRANSFER_RAW_COPY(1));
        gfxScreenSwapBuffers(GFX_TOP, false);
        gfxSetWide(wasWide);
    }

    SwkbdState kbd;
    std::vector<char> inputBuffer(length + 1);

    std::string orig;
    auto pos = result->find_last_not_of(' ');
    if (pos != std::string::npos) {
        orig.assign(result->data(), pos + 1);
    }

    swkbdInit(&kbd, SWKBD_TYPE_QWERTY, 2, length);
    swkbdSetButton(&kbd, SWKBD_BUTTON_LEFT, "Cancel", false);
    swkbdSetButton(&kbd, SWKBD_BUTTON_RIGHT, "OK", true);
    swkbdSetInitialText(&kbd, orig.c_str());
    swkbdSetHintText(&kbd, "Your Name");
    swkbdSetFeatures(&kbd, SWKBD_ALLOW_HOME);
    swkbdSetValidation(&kbd, SWKBD_NOTEMPTY_NOTBLANK,
                       SWKBD_FILTER_DIGITS | SWKBD_FILTER_AT |
                           SWKBD_FILTER_PERCENT | SWKBD_FILTER_BACKSLASH |
                           SWKBD_FILTER_CALLBACK,
                       0);
    swkbdSetFilterCallback(&kbd, nameInputCallback, nullptr);

    auto const button =
        swkbdInputText(&kbd, inputBuffer.data(), inputBuffer.size());
    if (button != SWKBD_BUTTON_RIGHT) {
        return false;
    }

    size_t i;
    for (i = 0; i < length; i++) {
        if (!inputBuffer[i]) {
            break;
        }
        if (inputBuffer[i] >= 'A' && inputBuffer[i] <= 'Z') {
            continue;
        }
        if (inputBuffer[i] >= 'a' && inputBuffer[i] <= 'z') {
            inputBuffer[i] += 'A' - 'a';
            continue;
        }
        inputBuffer[i] = ' ';
    }

    for (; i < length; i++) {
        inputBuffer[i] = ' ';
    }

    inputBuffer[length] = '\0';

    result->assign(inputBuffer.data());
    return true;
}

} /* namespace n3ds */

#endif
