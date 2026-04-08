#include "def.h"

/**
 * By sampling a hashed user input every VBlank the PRNG is seeded effectively
 * even without a save. Whisky's hash is sufficiently different for the seed.
 */
void sampleEntropy() {
#ifdef GBA
    // GBA has limited possible inputs
    uint32_t keyMask = 0x3FF;
#else
    // Most other platforms will express inputs with the whole 32 bits
    uint32_t keyMask = KEY_FULL;
#endif
    // Read input directly
    uint32_t input = keys_raw() ^ keyMask;
    // Hash the input with the last seed
    savefile->seed = (int)whisky2(input, savefile->seed);
    // Seed 32 bits as Xoshiro128++ authors suggest
    randSetSeed(savefile->seed);
}
#ifndef GBA
void randSetSeed(uint32_t seed) { setXoshiroSeed(seed); }
uint32_t randNext() { return nextXoshiro(); }
void randSetFullState(uint32_t seed) { setXoshiroState(seed); }
void randSetSeed2(uint32_t seed) { setXoshiroSeed2(seed); }
uint32_t randNext2() { return nextXoshiro2(); }
void randSetFullState2(uint32_t seed) { setXoshiroState2(seed); }
#endif
