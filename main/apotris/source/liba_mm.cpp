#ifdef MM

#include "liba_mm.h"
#include <fstream>
#include <list>

#include "SDL/SDL.h"
#include "def.h"
// #include "save.h"

Songs songs;

SDL_Surface* screen;

std::list<SDLKey> currentKeys;
std::list<SDLKey> previousKeys;
std::list<SDLKey> currentlyPressed;

// int rowStart = (512 - 240) / 2;
// int rowEnd = rowStart + 240;
int rowStart = 0;
int rowEnd = 180;

#define FPS_TARGET 60

double clock_timer = 0;

bool render = true;

void windowInit() {
    SDL_Init(SDL_INIT_VIDEO);

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
        return;
    }

    screen = SDL_SetVideoMode(640, 480, 32, SDL_SWSURFACE);
    if (screen == NULL) {
        printf("Could not create screen! SDL Error: %s\n", SDL_GetError());
        SDL_Quit();
        return;
    }
}

void key_poll() {
    previousKeys = currentKeys;
    currentKeys.clear();

    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        SDLKey key;
        auto it = currentlyPressed.begin();
        bool found = false;
        switch (event.type) {
        case SDL_KEYDOWN:
            key = event.key.keysym.sym;
            it = std::find(currentlyPressed.begin(), currentlyPressed.end(),
                           key);
            found = (it != currentlyPressed.end());
            if (!found)
                currentlyPressed.push_back(key);
            break;
        case SDL_KEYUP:
            key = event.key.keysym.sym;
            it = std::find(currentlyPressed.begin(), currentlyPressed.end(),
                           key);
            found = (it != currentlyPressed.end());
            if (found)
                currentlyPressed.remove(key);
            break;
        case SDL_QUIT:
            quit();
            break;
        default:
            break;
        }
    }

    currentKeys = currentlyPressed;
}

uint32_t key_is_down(uint32_t key) {
    if (key == KEY_FULL)
        return (!currentKeys.empty());

    auto it = currentKeys.begin();

    it = std::find(currentKeys.begin(), currentKeys.end(), key);

    return (it != currentKeys.end());
}

uint32_t key_hit(uint32_t key) {
    if (key == KEY_FULL)
        return (previousKeys.empty() && !currentKeys.empty());

    auto it = previousKeys.begin();
    it = std::find(previousKeys.begin(), previousKeys.end(), key);
    bool prev = (it != previousKeys.end());

    it = currentKeys.begin();
    it = std::find(currentKeys.begin(), currentKeys.end(), key);
    bool curr = (it != currentKeys.end());

    return (!prev && curr);
}

uint32_t key_released(uint32_t key) {
    if (key == KEY_FULL)
        return (!previousKeys.empty() && currentKeys.empty());

    auto it = previousKeys.begin();
    it = std::find(previousKeys.begin(), previousKeys.end(), key);
    bool prev = (it != previousKeys.end());

    it = currentKeys.begin();
    it = std::find(currentKeys.begin(), currentKeys.end(), key);
    bool curr = (it != currentKeys.end());

    return (prev && !curr);
}

uint32_t key_first() {
    if (currentKeys.empty())
        return KEY_FULL - 1;

    for (auto key : currentKeys) {
        auto it = previousKeys.begin();
        it = std::find(previousKeys.begin(), previousKeys.end(), key);
        bool prev = (it != previousKeys.end());

        if (!prev) {
            return key;
        }
    }

    return KEY_FULL - 1;
}

uint32_t keys_raw() {
    if (currentKeys.empty())
        return KEY_FULL - 1;

    u32 allKeys = KEY_FULL;
    for (auto key : currentKeys) {
        allKeys ^= key;
    }

    return allKeys;
}

void updateWindow(uint8_t* framebuffer) {

    const int in_width = 240;
    const int in_height = 180;

    if (savefile->settings.integerScale == 1) {
        uint8_t* newBuffer = new uint8_t[640 * 480 * 4];

        uint32_t* out_ptr = (uint32_t*)newBuffer;
        uint32_t* in_ptr = (uint32_t*)framebuffer;
        int in_pitch = in_width;
        int out_pitch = 640;

        in_ptr += (((in_height - 160) / 2)) * in_pitch;
        in_ptr += (in_width - 214) / 2;

        for (int y = 0; y < 160; y++) {
            uint32_t* line_ptr = in_ptr;

            for (int x = 0; x < 214; x++) {
                uint32_t* out_line_ptr = out_ptr;
                uint32_t color = *line_ptr;

                *out_line_ptr = color;

                if (x != 213) {
                    *(out_line_ptr + 1) = color;
                    *(out_line_ptr + 2) = color;

                    out_line_ptr += out_pitch;
                    *out_line_ptr = color;
                    *(out_line_ptr + 1) = color;
                    *(out_line_ptr + 2) = color;

                    out_line_ptr += out_pitch;
                    *out_line_ptr = color;
                    *(out_line_ptr + 1) = color;
                    *(out_line_ptr + 2) = color;

                    out_ptr += 3;
                } else {
                    out_line_ptr += out_pitch;
                    *out_line_ptr = color;

                    out_line_ptr += out_pitch;
                    *out_line_ptr = color;

                    out_ptr += 1;
                }

                line_ptr++;
            }

            in_ptr += in_pitch;
            out_ptr += out_pitch * 2;
        }

        SDL_Surface* img = SDL_CreateRGBSurfaceFrom(
            newBuffer, 640, 480, 32, 640 * 4, 0x0000ff, 0x00ff00, 0xff0000, 0);

        // Center image on screen
        SDL_Rect dstrect;
        dstrect.x = (screen->w - img->w) / 2;
        dstrect.y = (screen->h - img->h) / 2;

        // Draw image on screen
        if (render)
            SDL_BlitSurface(img, NULL, screen, &dstrect);

        SDL_FreeSurface(img);
        delete[] newBuffer;

    } else {
        SDL_Surface* img = SDL_CreateRGBSurfaceFrom(
            framebuffer, in_width, in_height, 32, in_width * 4, 0x0000ff,
            0x00ff00, 0xff0000, 0);

        // Center image on screen
        SDL_Rect dstrect;
        dstrect.x = 0;
        dstrect.y = 0;
        dstrect.w = 640;
        dstrect.h = 480;

        SDL_Surface* converted = SDL_DisplayFormat(img);

        // Draw image on screen
        if (render)
            SDL_SoftStretch(converted, NULL, screen, &dstrect);

        SDL_FreeSurface(img);
        SDL_FreeSurface(converted);
    }

    SDL_Flip(screen);
}

void refreshWindowSize() {}

bool closed() {
    double newclock = SDL_GetTicks();

    double deltaticks = 1000 / FPS_TARGET - (newclock - clock_timer);

    if ((int)deltaticks > 0)
        SDL_Delay(deltaticks);

    if (deltaticks < -30) {
        clock_timer = newclock - 30;

    } else {
        clock_timer = newclock + deltaticks;
    }

    return true;
}

void sfx(int) {}

void startSong(int, bool) {}

void stopSong() {};

void resumeSong() {};

void setMusicVolume(int) {};

void setMusicTempo(int) {};

void toggleRendering(bool r) { render = r; }

void sfxRate(int, float) {};

void pauseSong() {};
void initRumble() {};

void loadSavefile() {
    // memset32_fast(savefile, 0 , sizeof(Save)/4);

    std::ifstream input("Apotris.sav", std::ios::binary | std::ios::in);

    if (savefile == nullptr)
        savefile = new Save();

    char* src = (char*)savefile;

    input.read(src, sizeof(Save));

    if (!input) {
        log("Error when trying to load save.");
        return;
    }

    input.close();
}

void saveSavefile() {
    std::ofstream output("Apotris.sav", std::ios::binary | std::ios::out);

    char* dst = (char*)savefile;

    const int saveSize = 1 << 15;

    char temp[saveSize];

    memset32_fast(temp, 0, saveSize / 4);
    memcpy32_fast(temp, dst, sizeof(Save) / 4);

    output.write(temp, saveSize);

    if (!output) {
        log("Error when trying to write save.");
        return;
    }

    output.close();
}

void quit() {
    SDL_Quit();
    exit(0);
}

std::map<int, std::string> keyToString = {
    {SW_BTN_A, "A"},         {SW_BTN_B, "B"},       {SW_BTN_X, "X"},
    {SW_BTN_Y, "Y"},         {SW_BTN_L1, "L1"},     {SW_BTN_L2, "L2"},
    {SW_BTN_R1, "R1"},       {SW_BTN_R2, "R2"},     {SW_BTN_SELECT, "Select"},
    {SW_BTN_START, "Start"}, {SW_BTN_LEFT, "Left"}, {SW_BTN_UP, "Up"},
    {SW_BTN_RIGHT, "Right"}, {SW_BTN_DOWN, "Down"},
};

#endif
