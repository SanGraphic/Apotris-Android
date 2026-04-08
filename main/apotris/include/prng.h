#pragma once
#include "whisky.h"
#ifndef GBA
/**
 * Random generation for all platforms other than GBA needs an implementation of
 * the Xoshiro128++ PRNG, hooked here in C. GBA uses ASM from libutil.
 */
#include "xoshiro128plusplus.h"

void randSetFullState(uint32_t seed);
void randSetSeed(uint32_t seed);
uint32_t randNext();
void randSetFullState2(uint32_t seed);
void randSetSeed2(uint32_t seed);
uint32_t randNext2();
#endif

void sampleEntropy();
