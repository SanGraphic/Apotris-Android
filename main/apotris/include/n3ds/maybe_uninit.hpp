#pragma once

#include <cassert>
#include <cstdio>

#include <utility>

/// Creates a pre-allocated storage that allows manual control of its
/// initialization and lifetime.
template <typename T> class MaybeUninit {
    union Storage {
        T value;

        constexpr Storage(){};
        ~Storage(){};
    };
    Storage storage;
#ifndef NDEBUG
    bool valid{false};
#endif

    MaybeUninit(const MaybeUninit&) = delete;
    MaybeUninit(MaybeUninit&&) = delete;

    MaybeUninit& operator=(const MaybeUninit&) = delete;
    MaybeUninit& operator=(MaybeUninit&&) = delete;

public:
    constexpr MaybeUninit() {}

#ifdef NDEBUG
    ~MaybeUninit() = default;
#else
    ~MaybeUninit() {
        if (valid) {
            fputs("Warning: MaybeUninit<T> still contains a valid value before "
                  "the end of its lifetime, value will be leaked",
                  stderr);
        }
    }
#endif

    template <typename... Args>
    constexpr T&
    emplace(Args&&... args) noexcept(noexcept(T(std::forward<Args>(args)...))) {
#ifndef NDEBUG
        assert(!valid &&
               "MaybeUninit<T>::emplace called while containing a valid value");
#endif
        new (&this->storage.value) T(std::forward<Args>(args)...);
#ifndef NDEBUG
        this->valid = true;
#endif
        return this->storage.value;
    }

    inline void destruct() noexcept {
#ifndef NDEBUG
        assert(valid && "MaybeUninit<T>::destruct called while not containing "
                        "a valid value");
#endif
        this->storage.value.~T();
#ifndef NDEBUG
        this->valid = false;
#endif
    }

    constexpr T& value() {
#ifndef NDEBUG
        assert(valid && "MaybeUninit<T>::value called while not containing "
                        "a valid value");
#endif
        return this->storage.value;
    }

    constexpr T const& value() const {
#ifndef NDEBUG
        assert(valid && "MaybeUninit<T>::value called while not containing "
                        "a valid value");
#endif
        return this->storage.value;
    }
};
