#pragma once

#include <citro3d.h>

#include <memory>
#include <new>
#include <utility>

#define CITRO3DPP_DISABLE_COPY(c)                                              \
    c(const c&) = delete;                                                      \
    c& operator=(const c&) = delete;

namespace c3d {

struct C3D_Deleter {
    void operator()(C3D_RenderTarget* ptr) { C3D_RenderTargetDelete(ptr); }
};

using RenderTarget = std::unique_ptr<C3D_RenderTarget, C3D_Deleter>;

inline RenderTarget RenderTargetCreate(int width, int height,
                                       GPU_COLORBUF colorFmt,
                                       C3D_DEPTHTYPE depthFmt) {
    return RenderTarget(
        C3D_RenderTargetCreate(width, height, colorFmt, depthFmt));
}

inline RenderTarget RenderTargetCreateFromTex(C3D_Tex* tex, GPU_TEXFACE face,
                                              int level,
                                              C3D_DEPTHTYPE depthFmt) {
    return RenderTarget(
        C3D_RenderTargetCreateFromTex(tex, face, level, depthFmt));
}

struct FCRAM_tag {};
struct VRAM_tag {};
static constexpr FCRAM_tag FCRAM;
static constexpr VRAM_tag VRAM;

class Tex {
    C3D_Tex m_tex;

    CITRO3DPP_DISABLE_COPY(Tex);

public:
    template <typename Tag = FCRAM_tag>
    explicit inline Tex(u16 width, u16 height, GPU_TEXCOLOR format,
                        Tag _region = FCRAM) noexcept {
        static_assert(std::is_same_v<Tag, FCRAM_tag> ||
                          std::is_same_v<Tag, VRAM_tag>,
                      "Unexpected tag type");
        bool res;
        if constexpr (std::is_same_v<Tag, VRAM_tag>) {
            res = C3D_TexInitVRAM(&m_tex, width, height, format);
        } else {
            res = C3D_TexInit(&m_tex, width, height, format);
        }
        if (!res) {
            svcBreak(USERBREAK_PANIC);
        }
    }

    inline ~Tex() noexcept {
        if (m_tex.data) {
            C3D_TexDelete(&m_tex);
        }
    }

    inline Tex(Tex&& other) noexcept : m_tex{other.m_tex} {
        other.m_tex.data = nullptr;
    }

    inline Tex& operator=(Tex&& other) noexcept {
        this->~Tex();
        return *new (this) Tex{std::move(other)};
    }

    constexpr C3D_Tex* operator->() noexcept { return &m_tex; }
    constexpr C3D_Tex const* operator->() const noexcept { return &m_tex; }

    constexpr C3D_Tex* get() noexcept { return &m_tex; }
    constexpr C3D_Tex const* get() const noexcept { return &m_tex; }
};

} /* namespace c3d */
