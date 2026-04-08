#pragma once

#include <cinttypes>

#include "audio_files.h"
#include "liba_pc.h"

#include "SDL.h"
// #include "SDL_mixer.h"

#include <map>

#define JOY_A 0
#define JOY_B 1
#define JOY_X 2
#define JOY_Y 3
#define JOY_PLUS 10
#define JOY_MINUS 11
#define JOY_LEFT 12
#define JOY_UP 13
#define JOY_RIGHT 14
#define JOY_DOWN 15

#define KEY_A JOY_A
#define KEY_B JOY_B
#define KEY_L 6
#define KEY_R 7
#define KEY_UP JOY_UP
#define KEY_DOWN JOY_DOWN
#define KEY_LEFT JOY_LEFT
#define KEY_RIGHT JOY_RIGHT
#define KEY_SELECT JOY_MINUS
#define KEY_START JOY_PLUS
#define KEY_FULL 0xffffffff

extern void updateWindow(uint8_t*);
extern void refreshWindowSize();
extern bool closed();

extern void sfx(int);
extern void startSong(int, bool);
extern void stopSong();
extern void resumeSong();
extern void setMusicVolume(int);
extern void setMusicTempo(int);
extern void toggleRendering(bool);
extern void sfxRate(int, float);

extern void pauseSong();

extern void windowInit();

extern void key_poll();

extern uint32_t key_is_down(uint32_t);
extern uint32_t key_hit(uint32_t);
extern uint32_t key_released(uint32_t);
extern uint32_t key_first();
extern uint32_t keys_raw();

extern int rowStart;
extern int rowEnd;

extern std::map<int, std::string> keyToString;
