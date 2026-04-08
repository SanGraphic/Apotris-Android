#pragma once

#include <cinttypes>

#include "audio_files.h"
#include "liba_pc.h"

#include "SDL.h"
#include "SDL_gamecontroller.h"
#include "SDL_mixer.h"
#if defined(ANDROID) && defined(GL)
#include "glad/gles2.h"
#else
#include "glad/gl.h"
#endif

#include <map>

extern int KEY_A;
extern int KEY_B;
extern int KEY_L;
extern int KEY_R;
extern int KEY_UP;
extern int KEY_DOWN;
extern int KEY_LEFT;
extern int KEY_RIGHT;
extern int KEY_SELECT;
extern int KEY_START;

#define KEY_FULL 0xffffffff

extern void updateWindow(uint8_t*);
extern void refreshWindowSize();
extern bool closed();

extern void windowInit();

extern void key_poll();

extern void setFullscreen(bool);
extern void shaderInit(int);
extern void shaderDeinit();

extern uint32_t key_is_down(uint32_t);
extern uint32_t key_hit(uint32_t);
extern uint32_t key_released(uint32_t);
extern uint32_t key_first();
extern uint32_t keys_raw();

extern void setKey(int&, uint32_t);
extern void unbindDuplicateKey(int&, uint32_t);

extern int splitKey(uint32_t key);

extern int rowStart;
extern int rowEnd;

extern std::map<int, std::string> keyToString;

std::string stringFromKey(uint32_t key);

enum class InputType { KEYBOARD, CONTROLLER };

extern InputType lastInputType;

INLINE u16 packKey(SDL_Keycode key) {
    return (key & 0xfff) | (((key & 0xf0000000) >> 16));
}

INLINE SDL_Keycode unpackKey(u16 key) {
    return (SDL_Keycode)(key & 0xfff) | (((key & 0xf000) << 16));
}
