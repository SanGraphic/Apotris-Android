#pragma once

#include <cinttypes>

#include "audio_files.h"
#include "liba_pc.h"
#include <map>

#include "SDL/SDL.h"

#define SW_BTN_UP SDLK_UP
#define SW_BTN_DOWN SDLK_DOWN
#define SW_BTN_LEFT SDLK_LEFT
#define SW_BTN_RIGHT SDLK_RIGHT
#define SW_BTN_A SDLK_SPACE
#define SW_BTN_B SDLK_LCTRL
#define SW_BTN_X SDLK_LSHIFT
#define SW_BTN_Y SDLK_LALT
#define SW_BTN_L1 SDLK_e
#define SW_BTN_R1 SDLK_t
#define SW_BTN_L2 SDLK_TAB
#define SW_BTN_R2 SDLK_BACKSPACE
#define SW_BTN_SELECT SDLK_RCTRL
#define SW_BTN_START SDLK_RETURN
#define SW_BTN_MENU SDLK_ESCAPE
#define SW_BTN_POWER SDLK_FIRST

#define KEY_A SW_BTN_A
#define KEY_B SW_BTN_B
#define KEY_L SW_BTN_L2
#define KEY_R SW_BTN_R2
#define KEY_UP SW_BTN_UP
#define KEY_DOWN SW_BTN_DOWN
#define KEY_LEFT SW_BTN_LEFT
#define KEY_RIGHT SW_BTN_RIGHT
#define KEY_SELECT SW_BTN_SELECT
#define KEY_START SW_BTN_START
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
