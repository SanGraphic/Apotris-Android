#ifdef PORTMASTER

#include "liba_portmaster.h"
#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <unordered_set>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "def.h"
#include "shader.h"

#include "nanotime.h"

#define RUMBLE_OFF 1000000
#define RUMBLE_STRONG 100000
#define RUMBLE_MEDIUM 400000

static int fd = -1;

void rumbleUpdate();
void cleanup_handler(int signum);
int set_rumble(int strength);

void handleInput();

std::unordered_set<uint32_t> currentKeys;
std::unordered_set<uint32_t> previousKeys;

std::unordered_set<uint32_t> currentlyPressed;

// int rowStart = (288 - 288) / 2;
// int rowEnd = rowStart + 288;
int rowStart = 0;
int rowEnd = SCREEN_HEIGHT;

#define FPS_TARGET 60

double clock_timer = 0;

static uint64_t frame_start = nanotime_now();

uint32_t start_time = 0;
uint32_t frame_time = 0;

float fps = 0;

SDL_Window* window;
SDL_Renderer* renderer;
SDL_GameController* controller;
SDL_Texture* texture;

bool render = true;

int screenWidth = 1280;
int screenHeight = 720;

float windowScale = 3;

nanotime_step_data stepper;

ShaderStatus shaderStatus = ShaderStatus::NOT_INITED;

// static int rumbleTimer = 0;

void windowInit() {
    // Initialize SDL
    window = NULL;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    SDL_Init(SDL_INIT_VIDEO);

    SDL_DisplayMode DM;
    SDL_GetCurrentDisplayMode(0, &DM);
    screenWidth = DM.w;
    screenHeight = DM.h;

    window = SDL_CreateWindow("Apotris PortMaster", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, screenWidth, screenHeight,
                              SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);

    renderer = SDL_CreateRenderer(window, -1, 0);

    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    if (SDL_NumJoysticks() > 0) {
        controller = SDL_GameControllerOpen(0);
    }

    refreshWindowSize();

    loadAudio("");

    nanotime_step_init(&stepper, (uint64_t)(NANOTIME_NSEC_PER_SEC / FPS_TARGET),
                       nanotime_now_max(), nanotime_now, nanotime_sleep);
}

void key_poll() {}

void handleInput() {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            quit();
            break;
        default:
            break;
        }
    }

    if (controller && SDL_GameControllerGetAttached(controller)) {
        for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++) {
            if (SDL_GameControllerGetButton(
                    controller, static_cast<SDL_GameControllerButton>(i))) {
                currentlyPressed.insert(i);
            }
        }
    }

    previousKeys = currentKeys;
    currentKeys.clear();

    currentKeys = currentlyPressed;
}

uint32_t key_is_down(uint32_t key) {
    if (key == KEY_FULL)
        return (!currentKeys.empty());

    return currentKeys.count(key);
}

uint32_t key_hit(uint32_t key) {
    if (key == KEY_FULL)
        return (previousKeys.empty() && !currentKeys.empty());

    bool prev = previousKeys.count(key);
    bool curr = currentKeys.count(key);

    return (!prev && curr);
}

uint32_t key_released(uint32_t key) {
    if (key == KEY_FULL)
        return (!previousKeys.empty() && currentKeys.empty());

    bool prev = previousKeys.count(key);
    bool curr = currentKeys.count(key);

    return (prev && !curr);
}

uint32_t key_first() {
    if (currentKeys.empty())
        return KEY_FULL - 1;

    for (auto key : currentKeys) {
        bool prev = previousKeys.count(key);

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
    const int in_width = SCREEN_WIDTH;
    const int in_height = SCREEN_HEIGHT;

    // Create the SDL_Surface from the framebuffer
    SDL_Surface* img = SDL_CreateRGBSurfaceFrom(
        framebuffer, in_width, in_height, 32, in_width * 4, 0x0000ff, 0x00ff00,
        0xff0000, 0xff000000);

    if (img == NULL) {
        printf("Failed to create SDL_Surface: %s\n", SDL_GetError());
        return;
    }

    if (shaderStatus != ShaderStatus::OK || savefile->settings.shaders == 0) {
        // Create the streaming texture once
        if (texture == NULL) {
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                                        SDL_TEXTUREACCESS_STREAMING, in_width,
                                        in_height);
            if (texture == NULL) {
                printf("Failed to create texture: %s\n", SDL_GetError());
                SDL_FreeSurface(img);
                return;
            }
        }

        // Lock the texture to update its pixel data
        void* pixels;
        int pitch;
        if (SDL_LockTexture(texture, NULL, &pixels, &pitch) != 0) {
            printf("Failed to lock texture: %s\n", SDL_GetError());
            SDL_FreeSurface(img);
            return;
        }

        // Copy the surface's pixel data into the streaming texture
        for (int y = 0; y < in_height; y++) {
            memcpy((uint8_t*)pixels + y * pitch, // Destination pointer
                   (uint8_t*)img->pixels + y * img->pitch, // Source pointer
                   in_width * 4                            // Row size in bytes
            );
        }

        SDL_UnlockTexture(texture);

        // Clear the renderer
        SDL_RenderClear(renderer);

        // Define source and destination rectangles
        SDL_Rect src_rect = {0, 0, in_width, in_height};
        const int w = in_width * windowScale;
        const int h = in_height * windowScale;
        SDL_Rect dest_rect = {-(w - screenWidth) / 2, -(h - screenHeight) / 2,
                              w, h};

        // Copy the updated texture to the renderer
        if (render) {
            SDL_RenderCopy(renderer, texture, &src_rect, &dest_rect);
        }

        SDL_RenderPresent(renderer);
    } else {
        if (render) {
            drawWithShaders(window, img, true);
        }
    }
    SDL_FreeSurface(img); // Free the surface after use

    handleInput();

    // Cap FPS
    nanotime_step(&stepper);

    Uint64 end = SDL_GetPerformanceCounter();

    float elapsedMS =
        (end - frame_start) / (float)SDL_GetPerformanceFrequency() * 1000.0f;

    if (elapsedMS > 100) {
        nanotime_step_init(&stepper,
                           (uint64_t)(NANOTIME_NSEC_PER_SEC / FPS_TARGET),
                           nanotime_now_max(), nanotime_now, nanotime_sleep);
    }

    fps = 1000.0f / elapsedMS;

    frame_start = SDL_GetPerformanceCounter();
}

void refreshWindowSize() {
    if (savefile != nullptr) {
        if (savefile->settings.zoom > -1) {
            windowScale = 1 + (float)savefile->settings.zoom / 10;
        } else {
            if (savefile->settings.integerScale) {
                windowScale = (int)windowScale;

                while (windowScale > 0 &&
                       screenHeight / windowScale > 160 * 2) {
                    windowScale++;
                }
                while (windowScale > 1 && screenHeight / windowScale < 160) {
                    windowScale--;
                }
            } else {
                windowScale = screenHeight / 200.0;
            }
        }
    }

    if (windowScale <= 0)
        return;

    rowStart = (SCREEN_HEIGHT - (screenHeight / windowScale)) / 2;
    rowEnd = (SCREEN_HEIGHT + (screenHeight / windowScale)) / 2;

    if (rowStart < 0)
        rowStart = 0;

    if (rowEnd > SCREEN_HEIGHT)
        rowEnd = SCREEN_HEIGHT;

    if (savefile != nullptr) {
        setGradient(savefile->settings.backgroundGradient);
        if (savefile->settings.shaders != 0 &&
            shaderStatus == ShaderStatus::OK) {
            refreshShaderResolution(screenWidth, screenHeight, windowScale);
        }
    }
}

bool closed() {
    songEndHandler();
    rumbleUpdate();

    return true;
}

void rumbleUpdate() {
    rumblePatternLoop();
}

void toggleRendering(bool r) { render = r; }

void initRumble() {
    signal(SIGINT, cleanup_handler);
    signal(SIGTERM, cleanup_handler);

    fd = open("/sys/class/pwm/pwmchip0/pwm0/duty_cycle", O_WRONLY);
    if (fd < 0) {
        log("couldn't open rumble file 0");
        fd = open("/sys/class/pwm/pwmchip1/pwm0/duty_cycle", O_WRONLY);
        if (fd < 0) {
            log("couldn't open rumble file");
            fprintf(stderr, "Failed to open PWM: %s\n", strerror(errno));
            return;
        }
    }
    return;
};

void loadSavefile() {

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

void rumbleOutput(uint16_t strength) {
    uint16_t sdl_strength = std::min(0xFFFF, strength * 0xFFFF / 8); // Cap to 0xFFFF
    set_rumble(RUMBLE_STRONG);
    SDL_GameControllerRumble(controller, sdl_strength, sdl_strength, 64);
}

void rumbleStop() {
    set_rumble(RUMBLE_OFF);
    SDL_GameControllerRumble(controller, 0, 0, 1);
}

std::map<int, std::string> keyToString = {
    {SDL_CONTROLLER_BUTTON_A, "A"},
    {SDL_CONTROLLER_BUTTON_B, "B"},
    {SDL_CONTROLLER_BUTTON_X, "X"},
    {SDL_CONTROLLER_BUTTON_Y, "Y"},
    {SDL_CONTROLLER_BUTTON_BACK, "Select"},
    {SDL_CONTROLLER_BUTTON_START, "Start"},
    {SDL_CONTROLLER_BUTTON_DPAD_LEFT, "Left"},
    {SDL_CONTROLLER_BUTTON_DPAD_UP, "Up"},
    {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, "Right"},
    {SDL_CONTROLLER_BUTTON_DPAD_DOWN, "Down"},
    {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, "L"},
    {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, "R"},
};

// rumble signal handler
void cleanup_handler(int signum) {
    if (fd >= 0) {
        char str[16];
        snprintf(str, sizeof(str), "%d", RUMBLE_OFF);
        write(fd, str, strlen(str));
        close(fd);
    }
    exit(signum);
}

int set_rumble(int strength) {
    if (fd < 0)
        return -1;

    char str[16];
    snprintf(str, sizeof(str), "%d", strength);

    if (write(fd, str, strlen(str)) < 0) {
        fprintf(stderr, "Failed to set rumble: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

void quit() {
    // shutdown rumble
    if (fd >= 0) {
        char str[16];
        snprintf(str, sizeof(str), "%d", RUMBLE_OFF);
        write(fd, str, strlen(str));
        close(fd);
        fd = -1;
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    freeAudio();
    exit(0);
}

void shaderInit(int index) { initShaders(window, index); }

void shaderDeinit() { freeShaders(); }
#endif
