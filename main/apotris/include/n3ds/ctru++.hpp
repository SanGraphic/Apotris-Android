#pragma once

#include <3ds.h>

#include <cassert>
#include <memory>
#include <new>
#include <utility>

#define CTRUPP_DISABLE_COPY(c)                                                 \
    c(const c&) = delete;                                                      \
    c& operator=(const c&) = delete;

namespace ctru {

namespace shader {

class Binary;

class Program {
    shaderProgram_s m_program;

    CTRUPP_DISABLE_COPY(Program)

public:
    explicit inline Program(const Binary& binary, size_t vsh_index) noexcept;

    explicit inline Program(const Binary& binary, size_t vsh_index,
                            size_t gsh_index, u8 stride) noexcept;

    inline ~Program() noexcept {
        if (m_program.vertexShader) {
            shaderProgramFree(&m_program);
        }
    }

    inline Program(Program&& other) noexcept : m_program{other.m_program} {
        other.m_program.vertexShader = nullptr;
        other.m_program.geometryShader = nullptr;
    }

    inline Program& operator=(Program&& other) noexcept {
        this->~Program();
        return *new (this) Program{std::move(other)};
    }

    constexpr shaderProgram_s* get() noexcept { return &m_program; }
    constexpr shaderProgram_s const* get() const noexcept { return &m_program; }

    constexpr shaderInstance_s* vsh() noexcept {
        return m_program.vertexShader;
    }

    constexpr shaderInstance_s* gsh() noexcept {
        return m_program.geometryShader;
    }
};

class Binary {
    DVLB_s* m_dvlb;

    CTRUPP_DISABLE_COPY(Binary)

public:
    explicit inline Binary(u32 const* shbinData, u32 shbinSize) noexcept
        : m_dvlb(DVLB_ParseFile(const_cast<u32*>(shbinData), shbinSize)) {}

    inline ~Binary() noexcept {
        if (m_dvlb) {
            DVLB_Free(m_dvlb);
        }
    }

    inline Binary(Binary&& other) noexcept : m_dvlb{other.m_dvlb} {
        other.m_dvlb = nullptr;
    }

    inline Binary& operator=(Binary&& other) noexcept {
        this->~Binary();
        return *new (this) Binary{std::move(other)};
    }

    constexpr DVLB_s* operator->() noexcept { return m_dvlb; }
    constexpr DVLB_s const* operator->() const noexcept { return m_dvlb; }

    constexpr DVLB_s* get() noexcept { return m_dvlb; }
    constexpr DVLB_s const* get() const noexcept { return m_dvlb; }
};

inline Program::Program(const Binary& binary, size_t vsh_index) noexcept {
    assert(vsh_index < binary->numDVLE);

    shaderProgramInit(&m_program);
    shaderProgramSetVsh(&m_program, &binary->DVLE[vsh_index]);
}

inline Program::Program(const Binary& binary, size_t vsh_index,
                        size_t gsh_index, u8 stride) noexcept {
    assert(vsh_index < binary->numDVLE);

    shaderProgramInit(&m_program);
    shaderProgramSetVsh(&m_program, &binary->DVLE[vsh_index]);
    shaderProgramSetGsh(&m_program, &binary->DVLE[gsh_index], stride);
}

} /* namespace shader */

namespace linear {

namespace _detail {

template <typename T> struct _deleter {
    void operator()(T* p) noexcept {
        p->~T();
        linearFree((void*)p);
    }

    constexpr _deleter() noexcept = default;

    template <typename U,
              typename = std::enable_if<std::is_convertible_v<U*, T*>>>
    _deleter(const _deleter<U>&) noexcept {}
};

template <typename T> struct _deleter<T[]> {
    void operator()(T* p) noexcept {
        // We don't actually have proper size info so we cannot run destructors
        // properly.
        static_assert(std::is_trivially_destructible_v<T>,
                      "Destructing array for this type is not supported");
#if 0
        if constexpr (!std::is_trivially_destructible_v<T>) {
            // Note: This is incorrect as the size is padded to alignment.
            size_t size = linearGetSize(p);
            size_t n = size / sizeof(T);
            T* end = p + n;
            for (T* i = end--; i >= p; i--) {
                i->~T();
            }
        }
#endif
        linearFree((void*)p);
    }

    constexpr _deleter() noexcept = default;

    template <typename U, typename = std::enable_if_t<
                              std::is_convertible_v<U (*)[], T (*)[]>>>
    _deleter(const _deleter<U[]>&) noexcept {}
};

template <typename T, size_t N> struct _deleter<T[N]> {
    void operator()(T* p) = delete;
};

template <typename T> using _ptr = std::unique_ptr<T, _deleter<T>>;

template <typename T> struct _enable_make {
    using _non_array = _ptr<T>;
};

template <typename T> struct _enable_make<T[]> {
    using _array = _ptr<T[]>;
};

template <typename T, size_t N> struct _enable_make<T[N]> {
    static_assert(false, "Array of known bound is disallowed");
};

template <typename T, size_t alignment> constexpr bool _alignment_check() {
    static_assert(alignment >= alignof(T));
    static_assert(!(alignment & (alignment - 1)),
                  "Alignment must be a power of 2");
    return true;
}

template <typename T> constexpr bool _pod_check() {
    static_assert(std::is_standard_layout_v<T>);
    static_assert(std::is_trivially_default_constructible_v<T>);
    static_assert(std::is_nothrow_default_constructible_v<T>);
    return true;
}

template <typename T, size_t alignment, bool for_overwrite>
inline typename _enable_make<T>::_non_array _make_ptr_detail() noexcept {
    static_assert(_alignment_check<T, alignment>());
    static_assert(_pod_check<T>());

    void* p = linearMemAlign(sizeof(T), alignment);
    if (!p) {
        return {};
    }
    if constexpr (for_overwrite) {
        return _ptr<T>{new (p) T};
    } else {
        return _ptr<T>{new (p) T()};
    }
}

template <typename T, size_t alignment, bool for_overwrite>
inline typename _enable_make<T>::_array
_make_ptr_detail_array(size_t size) noexcept {
    using X = std::remove_extent_t<T>;

    static_assert(_alignment_check<T, alignment>());
    static_assert(_pod_check<X>());

    size_t alloc_size;
    if (__builtin_mul_overflow(sizeof(X), size, &alloc_size)) {
        return {};
    }
    void* p = linearMemAlign(alloc_size, alignment);
    if (!p) {
        return {};
    }
    if constexpr (for_overwrite) {
        return _ptr<T>{new (p) X[size]};
    } else {
        return _ptr<T>{new (p) X[size]()};
    }
}

} /* namespace _detail */

template <typename T> using ptr = _detail::_ptr<T>;

constexpr size_t DEFAULT_ALIGNMENT = 0x80;

template <typename T, size_t alignment, typename... Args>
inline typename _detail::_enable_make<T>::_non_array
make_ptr_align_nothrow(Args&&... args) noexcept {
    return _detail::_make_ptr_detail<T, alignment, false>(
        std::forward<Args>(args)...);
}

template <typename T, typename... Args>
inline typename _detail::_enable_make<T>::_non_array
make_ptr_nothrow(Args&&... args) noexcept {
    constexpr size_t alignment = std::max(alignof(T), DEFAULT_ALIGNMENT);
    return _detail::_make_ptr_detail<T, alignment, false>(
        std::forward<Args>(args)...);
}

template <typename T, size_t alignment>
inline typename _detail::_enable_make<T>::_array
make_ptr_align_nothrow(size_t size) noexcept {
    return _detail::_make_ptr_detail_array<T, alignment, false>(size);
}

template <typename T>
inline typename _detail::_enable_make<T>::_array
make_ptr_nothrow(size_t size) noexcept {
    constexpr size_t alignment = std::max(alignof(T), DEFAULT_ALIGNMENT);
    return _detail::_make_ptr_detail_array<T, alignment, false>(size);
}

template <typename T, size_t alignment>
inline typename _detail::_enable_make<T>::_non_array
make_ptr_align_nothrow_for_overwrite() noexcept {
    return _detail::_make_ptr_detail<T, alignment, true>();
}

template <typename T>
inline typename _detail::_enable_make<T>::_non_array
make_ptr_nothrow_for_overwrite() noexcept {
    constexpr size_t alignment = std::max(alignof(T), DEFAULT_ALIGNMENT);
    return _detail::_make_ptr_detail<T, alignment, true>();
}

template <typename T, size_t alignment>
inline typename _detail::_enable_make<T>::_array
make_ptr_align_nothrow_for_overwrite(size_t size) noexcept {
    return _detail::_make_ptr_detail_array<T, alignment, true>(size);
}

template <typename T>
inline typename _detail::_enable_make<T>::_array
make_ptr_nothrow_for_overwrite(size_t size) noexcept {
    constexpr size_t alignment = std::max(alignof(T), DEFAULT_ALIGNMENT);
    return _detail::_make_ptr_detail_array<T, alignment, true>(size);
}

} /* namespace linear */

} /* namespace ctru */
