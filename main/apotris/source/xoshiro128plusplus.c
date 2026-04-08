/*  Written in 2019 by David Blackman and Sebastiano Vigna (vigna@acm.org)

To the extent possible under law, the author has dedicated all copyright
and related and neighboring rights to this software to the public domain
worldwide.

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#ifndef GBA

#include <stdint.h>

/* This is xoshiro128++ 1.0, one of our 32-bit all-purpose, rock-solid
   generators. It has excellent speed, a state size (128 bits) that is
   large enough for mild parallelism, and it passes all tests we are aware
   of.

   For generating just single-precision (i.e., 32-bit) floating-point
   numbers, xoshiro128+ is even faster.

   The state must be seeded so that it is not everywhere zero. */

static inline uint32_t rotl(const uint32_t x, int k) {
    return (x << k) | (x >> (32 - k));
}

// One must set the state such that some bit is non-zero
static uint32_t state[4] = {0x0, 0x0, 0x0, 0x1};
static uint32_t state2[4] = {0x0, 0x0, 0x0, 0x1};

void setXoshiroSeed(uint32_t seed) { state[0] = seed; }
void setXoshiroSeed2(uint32_t seed) { state2[0] = seed; }

void setXoshiroState(uint32_t seed) {
    state[0] = seed;
    // We additionally clobber the rest of the state to the default to allow
    // reproducibility of seeded results within game sessions.
    state[1] = 0x0;
    state[2] = 0x0;
    state[3] = 0x1;
}

void setXoshiroState2(uint32_t seed) {
    state2[0] = seed;
    // We additionally clobber the rest of the state to the default to allow
    // reproducibility of seeded results within game sessions.
    state2[1] = 0x0;
    state2[2] = 0x0;
    state2[3] = 0x1;
}

uint32_t nextXoshiro(void) {
    const uint32_t result = rotl(state[0] + state[3], 7) + state[0];

    const uint32_t t = state[1] << 9;

    state[2] ^= state[0];
    state[3] ^= state[1];
    state[1] ^= state[2];
    state[0] ^= state[3];

    state[2] ^= t;

    state[3] = rotl(state[3], 11);

    return result;
}

uint32_t nextXoshiro2(void) {
    const uint32_t result = rotl(state2[0] + state2[3], 7) + state2[0];

    const uint32_t t = state2[1] << 9;

    state2[2] ^= state2[0];
    state2[3] ^= state2[1];
    state2[1] ^= state2[2];
    state2[0] ^= state2[3];

    state2[2] ^= t;

    state2[3] = rotl(state2[3], 11);

    return result;
}
#endif
