#ifdef WEB

#include "liba_web.h"
#include <fstream>
#include <unordered_set>

#include "def.h"
#include "nanotime.h"
#include <dirent.h>
#include <emscripten.h>
#include <emscripten/html5.h>

void resizeWindow(int width, int height);

void handleAnalogInput(int axis, int value);
void handleInput();

std::unordered_set<uint32_t> currentKeys;
std::unordered_set<uint32_t> previousKeys;

std::unordered_set<uint32_t> currentlyPressed;

int rowStart = (240 - 240) / 2;
int rowEnd = rowStart + 240;
// int rowStart = 0;
// int rowEnd = 160;

#define FPS_TARGET 60

double clock_timer = 0;

static uint64_t frame_start = nanotime_now();
nanotime_step_data stepper;

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture* texture;
SDL_GameController* controller;

int controllerCount = 0;

bool render = true;

uint32_t start_time = 0;
uint32_t frame_time = 0;

float fps = 0;

int screenWidth = 1280;
int screenHeight = 720;

InputType lastInputType = InputType::KEYBOARD;

float windowScale = 4;

#define DEADZONE 12000

int KEY_A = (SDL_CONTROLLER_BUTTON_A << 16) | packKey(SDLK_RETURN);
int KEY_B = (SDL_CONTROLLER_BUTTON_B << 16) | packKey(SDLK_BACKSPACE);
int KEY_L = (SDL_CONTROLLER_BUTTON_LEFTSHOULDER << 16) | packKey(SDLK_1);
int KEY_R = (SDL_CONTROLLER_BUTTON_RIGHTSHOULDER << 16) | packKey(SDLK_2);
int KEY_UP = (SDL_CONTROLLER_BUTTON_DPAD_UP << 16) | packKey(SDLK_w);
int KEY_DOWN = (SDL_CONTROLLER_BUTTON_DPAD_DOWN << 16) | packKey(SDLK_s);
int KEY_LEFT = (SDL_CONTROLLER_BUTTON_DPAD_LEFT << 16) | packKey(SDLK_a);
int KEY_RIGHT = (SDL_CONTROLLER_BUTTON_DPAD_RIGHT << 16) | packKey(SDLK_d);
int KEY_SELECT = (SDL_CONTROLLER_BUTTON_BACK << 16) | packKey(SDLK_3);
int KEY_START = (SDL_CONTROLLER_BUTTON_START << 16) | packKey(SDLK_ESCAPE);

EM_JS(int, getWindowWidth, (), { return window.innerWidth; });

EM_JS(int, getWindowHeight, (), { return window.innerHeight; });

bool resizeCallback(int eventType, const EmscriptenUiEvent* event,
                    void* userData) {
    screenWidth = getWindowWidth();
    screenHeight = getWindowHeight();

    resizeWindow(screenWidth, screenHeight);
    return false;
}

void windowInit() {
    // Initialize SDL

    SDL_SetHint(SDL_HINT_EMSCRIPTEN_ASYNCIFY, "0");

    EM_ASM(
        // Make a directory other than '/'
        FS.mkdir('/offline');
        // Then mount with IDBFS type
        FS.mount(IDBFS, {}, '/offline');

        // Then sync
        FS.syncfs(true, function(err){
                            // Error
                        }););

    emscripten_sleep(100);

    window = SDL_CreateWindow("Apotris Web", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, screenWidth, screenHeight,
                              SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    resizeWindow(getWindowWidth(), getWindowHeight());

    renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    if (SDL_NumJoysticks() > 0) {
        controller = SDL_GameControllerOpen(0);
        controllerCount = SDL_NumJoysticks();
    }

    loadAudio("");

    nanotime_step_init(&stepper, (uint64_t)(NANOTIME_NSEC_PER_SEC / FPS_TARGET),
                       nanotime_now_max(), nanotime_now, nanotime_sleep);

    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr,
                                   false, resizeCallback);
}

void pressKey(int key, InputType type) {
    lastInputType = type;

    currentlyPressed.insert(key);
}

void unpressKey(int key, InputType type) {
    lastInputType = type;

    currentlyPressed.erase(key);
}

void key_poll() {}

void handleInput() {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        uint32_t key = 0;
        auto it = currentlyPressed.begin();
        bool found = false;

        switch (event.type) {
        case SDL_KEYDOWN:
            key = event.key.keysym.sym;
            if (key == SDLK_F11) {
                // log("here!");
                // emscripten_request_fullscreen(document.documentElement,1);
                SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
                break;
            }

            pressKey(key, InputType::KEYBOARD);
            break;
        case SDL_CONTROLLERBUTTONDOWN:
            key = event.cbutton.button;
            pressKey(key, InputType::CONTROLLER);
            break;
        case SDL_KEYUP:
            key = event.key.keysym.sym;
            unpressKey(key, InputType::KEYBOARD);
            break;
        case SDL_CONTROLLERBUTTONUP:
            key = event.cbutton.button;
            unpressKey(key, InputType::CONTROLLER);
            break;
        case SDL_CONTROLLERAXISMOTION:
            handleAnalogInput(event.caxis.axis, event.caxis.value);
            break;
        case SDL_QUIT:
            quit();
            break;
        default:
            break;
        }
    }

    previousKeys = currentKeys;
    currentKeys.clear();

    currentKeys = currentlyPressed;
}

int splitKey(uint32_t key) {
    if (lastInputType == InputType::KEYBOARD) {
        return unpackKey(key & 0xffff);
    } else {
        return (key & 0xffff0000) >> 16;
    }
}

uint32_t key_is_down(uint32_t key) {
    if (key == KEY_FULL)
        return (!currentKeys.empty());

    key = splitKey(key);

    return currentKeys.count(key);
}

uint32_t key_hit(uint32_t key) {
    if (key == KEY_FULL)
        return (previousKeys.empty() && !currentKeys.empty());

    key = splitKey(key);

    bool prev = previousKeys.count(key);
    bool curr = currentKeys.count(key);

    return (!prev && curr);
}

uint32_t key_released(uint32_t key) {
    if (key == KEY_FULL)
        return (!previousKeys.empty() && currentKeys.empty());

    key = splitKey(key);

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

    // Create the streaming texture once
    if (texture == NULL) {
        texture =
            SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                              SDL_TEXTUREACCESS_STREAMING, in_width, in_height);
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
        memcpy((uint8_t*)pixels + y * pitch,           // Destination pointer
               (uint8_t*)img->pixels + y * img->pitch, // Source pointer
               in_width * 4                            // Row size in bytes
        );
    }

    SDL_UnlockTexture(texture);
    SDL_FreeSurface(img); // Free the surface after use

    // Clear the renderer
    SDL_RenderClear(renderer);

    // Define source and destination rectangles
    SDL_Rect src_rect = {0, 0, in_width, in_height};
    const int w = in_width * windowScale;
    const int h = in_height * windowScale;
    SDL_Rect dest_rect = {-(w - screenWidth) / 2, -(h - screenHeight) / 2, w,
                          h};

    // Copy the updated texture to the renderer
    if (render) {
        SDL_RenderCopy(renderer, texture, &src_rect, &dest_rect);
    }

    SDL_RenderPresent(renderer);

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

    // handle controller hotplug
    if (SDL_NumJoysticks() != controllerCount) {
        if (controllerCount == 0) {
            controller = SDL_GameControllerOpen(0);
        } else if (SDL_NumJoysticks() == 0) {
            SDL_GameControllerClose(controller);
        }

        controllerCount = SDL_NumJoysticks();
    }
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

    if (savefile != nullptr)
        setGradient(savefile->settings.backgroundGradient);
}

bool closed() {
    emscripten_sleep(0);
    songEndHandler();
    rumblePatternLoop();

    // // Cap to 60 FPS

    // //show fps
    // frame_time = SDL_GetTicks()-start_time;
    // fps = (frame_time > 0)? 1000.0 / frame_time : 0.0;
    // start_time = SDL_GetTicks();

    // std::cout << "fps: " << fps << "\n";

    return true;
}

void initRumble() {};

void loadSavefile() {
    EM_ASM(var s = FS.open("/offline/Apotris.sav", "a"); FS.close(s););

    std::ifstream input("/offline/Apotris.sav",
                        std::ios::binary | std::ios::in);

    if (!input) {
        log("Error when trying to load save.");
        return;
    }

    if (savefile == nullptr)
        savefile = new Save();

    char* src = (char*)savefile;

    input.read(src, sizeof(Save));

    input.close();
}

void saveSavefile() {
    std::ofstream output("/offline/Apotris.sav",
                         std::ios::binary | std::ios::out);

    if (!output) {
        log("Error when trying to write save.");
        return;
    }

    char* dst = (char*)savefile;

    const int saveSize = 1 << 15;

    char temp[saveSize];

    memset32_fast(temp, 0, saveSize / 4);
    memcpy32_fast(temp, dst, sizeof(Save) / 4);

    output.write(temp, saveSize);

    output.close();

    EM_ASM(FS.syncfs(function(err){
        // Error
    }););
    emscripten_sleep(100);
}

void quit() {
    freeAudio();
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void resizeWindow(int width, int height) {
    emscripten_set_canvas_size(width, height);
    SDL_SetWindowSize(window, width, height);

    screenWidth = width;
    screenHeight = height;
}

std::string stringFromKey(uint32_t key) {
    if (lastInputType == InputType::KEYBOARD) {
        key = unpackKey(key & 0xffff);

        if (key == 0xffff)
            return "";

        return SDL_GetKeyName(key);
    } else {
        return keyToString[key >> 16];
    }
}

void toggleRendering(bool r) { render = r; }

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
    {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, "LB"},
    {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, "RB"},
    {SDL_CONTROLLER_BUTTON_LEFTSTICK, "LS"},
    {SDL_CONTROLLER_BUTTON_RIGHTSTICK, "RS"},
    {(1 << 14) | (0 << 5) | (0), "Left +X"},
    {(1 << 14) | (1 << 5) | (0), "Left -X"},
    {(1 << 14) | (0 << 5) | (1), "Left +Y"},
    {(1 << 14) | (1 << 5) | (1), "Left -Y"},
    {(1 << 14) | (0 << 5) | (2), "Right +X"},
    {(1 << 14) | (1 << 5) | (2), "Right -X"},
    {(1 << 14) | (0 << 5) | (3), "Right +Y"},
    {(1 << 14) | (1 << 5) | (3), "Right -Y"},
    {(1 << 14) | (0 << 5) | (4), "LT"},
    {(1 << 14) | (0 << 5) | (5), "RT"},
    {0xffff, ""},
};

void setKey(int& dest, uint32_t key) {
    if (lastInputType == InputType::KEYBOARD) {
        dest = (dest & 0xffff0000) | packKey(key);
    } else {
        dest = (dest & 0xffff) | (key << 16);
    }
}

void unbindDuplicateKey(int& dest, uint32_t key) {
    if (lastInputType == InputType::KEYBOARD) {
        if ((dest & 0xffff) == (key & 0xffff))
            dest = (dest & 0xffff0000) | 0xfffe;
    } else {
        if ((dest >> 16) == key)
            dest = (dest & 0xffff) | (0xfffe << 16);
    }
}

void rumbleOutput(uint16_t strength) {
    if (lastInputType != InputType::CONTROLLER)
        return;

    uint16_t sdl_strength = std::min(0xFFFF, strength * 0xFFFF / 8); // Cap to 0xFFFF
    SDL_GameControllerRumble(controller, sdl_strength, sdl_strength, 64);
}

void rumbleStop() { SDL_GameControllerRumble(controller, 0, 0, 1); }

void handleAnalogInput(int axis, int value) {
    lastInputType = InputType::CONTROLLER;

    int key = 0;

    key |= (axis & 0xf) | (1 << 14);

    if (value > DEADZONE) {
        pressKey((key), InputType::CONTROLLER);
        unpressKey((key) | (1 << 5), InputType::CONTROLLER);
    } else if (value < -DEADZONE) {
        pressKey((key) | (1 << 5), InputType::CONTROLLER);
        unpressKey((key), InputType::CONTROLLER);
    } else {
        unpressKey((key), InputType::CONTROLLER);
        unpressKey((key) | (1 << 5), InputType::CONTROLLER);
    }
}
#endif
