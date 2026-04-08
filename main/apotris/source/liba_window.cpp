#ifdef PC

#include "liba_window.h"
#include <cstdio>
#ifdef ANDROID
#include <SDL_system.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <unistd.h>
#endif
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unordered_set>

#include "def.h"

#ifdef ANDROID
#include "scene.hpp"
#endif

#include "shader.h"

#include "nanotime.h"

#ifdef __APPLE__
// For the Application Support directory save location
#include <cstring>
#include <pwd.h>
#include <string>
#include <unistd.h>
const char* homeDir = getenv("HOME");
std::string savefileDir() { return std::string(homeDir) + "/Library/Application Support/Apotris"; }
#else
// For Linux directory save location
#include <vector>
#endif

void handleAnalogInput(int axis, int value);
void handleInput();

std::unordered_set<uint32_t> currentKeys;
std::unordered_set<uint32_t> previousKeys;

std::unordered_set<uint32_t> currentlyPressed;

#ifdef ANDROID
static std::mutex g_androidInputMutex;
static std::atomic<bool> g_androidPortraitLayout{false};
static std::atomic<bool> g_androidRefreshLayout{false};
bool androidConsumeLayoutRefresh();
#endif

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
SDL_Texture* texture;
SDL_GameController* controller;

int controllerCount = 0;

#ifdef ANDROID
// Prefer physical pads: SDL may register Android virtual/uinput devices as joysticks at index 0,
// so opening(0) + hotplug logic can grab the wrong device after rotation + real gamepad connect.
static bool androidJoystickIndexLooksVirtual(int deviceIndex) {
    if (deviceIndex < 0 || deviceIndex >= SDL_NumJoysticks())
        return false;
    const char* name = SDL_JoystickNameForIndex(deviceIndex);
    if (!name)
        return false;
    return strstr(name, "Virtual") != nullptr || strstr(name, "virtual") != nullptr ||
           strstr(name, "uinput") != nullptr;
}

static bool androidGameControllerLooksVirtual(SDL_GameController* gc) {
    if (!gc)
        return false;
    SDL_Joystick* j = SDL_GameControllerGetJoystick(gc);
    const char* name = j ? SDL_JoystickName(j) : nullptr;
    if (!name)
        return false;
    return strstr(name, "Virtual") != nullptr || strstr(name, "virtual") != nullptr ||
           strstr(name, "uinput") != nullptr;
}

static SDL_GameController* openPreferredGameController() {
    const int n = SDL_NumJoysticks();
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < n; ++i) {
            if (!SDL_IsGameController(i))
                continue;
            if (pass == 0 && androidJoystickIndexLooksVirtual(i))
                continue;
            SDL_GameController* c = SDL_GameControllerOpen(i);
            if (c)
                return c;
        }
    }
    return nullptr;
}
#endif

bool fullscreen = false;

bool render = true;

int screenWidth = 1280;
int screenHeight = 720;

float windowScale = 3;

InputType lastInputType = InputType::KEYBOARD;

nanotime_step_data stepper;

ShaderStatus shaderStatus = ShaderStatus::NOT_INITED;

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

void windowInit() {
    // Initialize SDL
    window = nullptr;

#ifdef ANDROID
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
#else
    SDL_Init(SDL_INIT_VIDEO);
#endif

#ifdef ANDROID
    {
        const char* base = SDL_AndroidGetInternalStoragePath();
        if (base != nullptr) {
            std::string gameRoot = std::string(base) + "/game_assets";
            if (chdir(gameRoot.c_str()) != 0) {
                log("Apotris: chdir to internal game_assets failed: " + gameRoot);
            }
        }
    }
#endif

#ifdef ANDROID
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    // GLES 2.0 — matches AndroidManifest glEsVersion 0x00020000; broader device support than GLES 3.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#endif

    window = SDL_CreateWindow("Apotris PC", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, screenWidth, screenHeight,
                              SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL |
                                  SDL_WINDOW_RESIZABLE);

    renderer = SDL_CreateRenderer(window, -1, 0);

    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
#ifdef ANDROID
    controller = openPreferredGameController();
    controllerCount = SDL_NumJoysticks();
#else
    if (SDL_NumJoysticks() > 0) {
        controller = SDL_GameControllerOpen(0);
        controllerCount = SDL_NumJoysticks();
    }
#endif

    SDL_ShowCursor(SDL_DISABLE);

    refreshWindowSize();

    loadAudio("");

    nanotime_step_init(&stepper, (uint64_t)(NANOTIME_NSEC_PER_SEC / FPS_TARGET),
                       nanotime_now_max(), nanotime_now, nanotime_sleep);

#ifndef __APPLE__
    SDL_Surface* icon = SDL_LoadBMP("assets/favicon32.bmp");

    SDL_SetWindowIcon(window, icon);

    SDL_FreeSurface(icon);
#endif
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
#ifdef ANDROID
        std::lock_guard<std::mutex> androidInputLock(g_androidInputMutex);
#endif
        int key = 0;
        auto it = currentlyPressed.begin();
        bool found = false;

        switch (event.type) {
        case SDL_KEYDOWN:
            key = event.key.keysym.sym;
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
        case SDL_CONTROLLERDEVICEADDED: {
            int idx = event.cdevice.which;
#ifdef ANDROID
            if (androidJoystickIndexLooksVirtual(idx))
                break;
#endif
            SDL_JoystickID newInst = SDL_JoystickGetDeviceInstanceID(idx);
            if (controller != nullptr) {
                SDL_Joystick* oj = SDL_GameControllerGetJoystick(controller);
                if (oj != nullptr && SDL_JoystickInstanceID(oj) == newInst)
                    break;
            }
            SDL_GameController* c = SDL_GameControllerOpen(idx);
            if (c != nullptr) {
                if (controller != nullptr)
                    SDL_GameControllerClose(controller);
                controller = c;
            }
            break;
        }
        case SDL_CONTROLLERDEVICEREMOVED: {
            SDL_JoystickID jid = (SDL_JoystickID)event.cdevice.which;
            if (controller != nullptr) {
                SDL_Joystick* j = SDL_GameControllerGetJoystick(controller);
                if (j != nullptr && SDL_JoystickInstanceID(j) == jid) {
                    SDL_GameControllerClose(controller);
                    controller = nullptr;
#ifdef ANDROID
                    controller = openPreferredGameController();
#else
                    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
                        if (SDL_IsGameController(i)) {
                            controller = SDL_GameControllerOpen(i);
                            break;
                        }
                    }
#endif
                }
            }
            break;
        }
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                screenWidth = event.window.data1;
                screenHeight = event.window.data2;
                refreshWindowSize();
            }
            break;
        case SDL_JOYBUTTONDOWN:
            key = event.jbutton.button;
            pressKey(key, InputType::CONTROLLER);
            break;
        case SDL_JOYBUTTONUP:
            key = event.jbutton.button;
            unpressKey(key, InputType::CONTROLLER);
            break;
        case SDL_QUIT:
            quit();
            break;
        default:
            break;
        }
    }

#ifdef ANDROID
    std::lock_guard<std::mutex> androidInputLock(g_androidInputMutex);
#endif
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
#ifdef ANDROID
    if (androidConsumeLayoutRefresh()) {
        refreshWindowSize();
    }
    static bool s_lastPortrait = false;
    static bool s_lastPortraitGame6 = false;
    const bool portrait = screenHeight > screenWidth;
    if (portrait != s_lastPortrait) {
        s_lastPortrait = portrait;
        refreshWindowSize();
    } else if (portrait && savefile != nullptr &&
               savefile->settings.zoom <= -1 && savefile->settings.integerScale) {
        const bool portraitGame6 = scene != nullptr &&
            dynamic_cast<GameScene*>(scene) != nullptr && !paused;
        if (portraitGame6 != s_lastPortraitGame6) {
            s_lastPortraitGame6 = portraitGame6;
            refreshWindowSize();
        }
    } else if (!portrait) {
        s_lastPortraitGame6 = false;
    }
#endif
    const int in_width = 512;
    const int in_height = 512;

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

    SDL_FreeSurface(img);

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

    // Controller hotplug fallback (if SDL events were missed)
    {
        const int n = SDL_NumJoysticks();
        if (n != controllerCount) {
            controllerCount = n;
            if (n == 0) {
                if (controller != nullptr) {
                    SDL_GameControllerClose(controller);
                    controller = nullptr;
                }
            } else {
#ifdef ANDROID
                bool needOpen = (controller == nullptr ||
                                 !SDL_GameControllerGetAttached(controller));
                if (!needOpen && controller != nullptr &&
                    androidGameControllerLooksVirtual(controller)) {
                    for (int i = 0; i < n; ++i) {
                        if (!SDL_IsGameController(i) || androidJoystickIndexLooksVirtual(i))
                            continue;
                        needOpen = true;
                        break;
                    }
                }
                if (needOpen) {
                    if (controller != nullptr) {
                        SDL_GameControllerClose(controller);
                        controller = nullptr;
                    }
                    controller = openPreferredGameController();
                }
#else
                if (controller == nullptr || !SDL_GameControllerGetAttached(controller)) {
                    if (controller != nullptr)
                        SDL_GameControllerClose(controller);
                    controller = nullptr;
                    for (int i = 0; i < n; ++i) {
                        if (SDL_IsGameController(i)) {
                            controller = SDL_GameControllerOpen(i);
                            break;
                        }
                    }
                }
#endif
            }
        }
    }
}

void refreshWindowSize() {
#ifndef ANDROID
    const int scaleRef = screenHeight;
#endif

    if (savefile != nullptr) {
        if (savefile->settings.zoom > -1) {
            windowScale = 1 + (float)savefile->settings.zoom / 10;
        } else {
#ifdef ANDROID
            if (savefile->settings.integerScale) {
                if (screenWidth >= screenHeight) {
                    windowScale = 5;
                } else {
                    const bool portraitActivePlay =
                        scene != nullptr &&
                        dynamic_cast<GameScene*>(scene) != nullptr && !paused;
                    windowScale = portraitActivePlay ? 6.0f : 4.0f;
                }
            } else {
                windowScale = screenHeight / 200.0;
            }
#else
            if (savefile->settings.integerScale) {
                windowScale = (int)windowScale;

                while (windowScale > 0 &&
                       scaleRef / windowScale > 160 * 2) {
                    windowScale++;
                }
                while (windowScale > 1 && scaleRef / windowScale < 160) {
                    windowScale--;
                }
            } else {
                windowScale = scaleRef / 200.0;
            }
#endif
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
    rumblePatternLoop();

    return true;
}

void toggleRendering(bool r) { render = r; }

void initRumble() {};

std::string getSavefilePath() {
#ifdef __APPLE__
    // Get the user's home directory
    if (!homeDir) {
        struct passwd* pw = getpwuid(getuid());
        homeDir = pw->pw_dir;
    }

    // Build the path: ~/Library/Application Support/Apotris/
    // Create directory if it doesn't exist
    mkdir(savefileDir().c_str(), 0755);
    return savefileDir() + "/Apotris.sav";
#else
    // Build list of possible save file locations in priority order
    std::vector<std::string> possibleSaveDirs;

    // XDG_DATA_HOME location
    const char* xdgDataHome = getenv("XDG_DATA_HOME");
    if (xdgDataHome) {
        possibleSaveDirs.push_back(std::string(xdgDataHome) + "/Apotris");
    }

    // HOME/.local/share location
    const char* homeDir = getenv("HOME");
    if (homeDir) {
        possibleSaveDirs.push_back(std::string(homeDir) +
                                   "/.local/share/Apotris");
    }

    // Current directory location
    possibleSaveDirs.push_back(".");

    std::string saveFileDir = possibleSaveDirs.at(0);

    if (saveFileDir != ".") {
// Create directory if it doesn't exist
#ifdef WIN32
        mkdir(saveFileDir.c_str());
#else
        mkdir(saveFileDir.c_str(), 0755);
#endif
    }

    std::string preferredSavefile = saveFileDir + "/Apotris.sav";
    struct stat fileStat;
    // Return early if only one possible savedir or preferred savefile exists
    if (possibleSaveDirs.size() == 1 ||
        stat(preferredSavefile.c_str(), &fileStat) == 0) {
        return preferredSavefile;
    }

    // Logic for moving save files in lower priority location:
    //
    // Find the most recently modified existing save file
    std::string mostRecentSavefile;
    time_t mostRecentTime = 0;
    for (auto it = possibleSaveDirs.begin() + 1; it != possibleSaveDirs.end();
         ++it) {
        auto savefilePath = *it + "/Apotris.sav";
        struct stat fileStat;
        if (stat(savefilePath.c_str(), &fileStat) == 0) {
            if (fileStat.st_mtime > mostRecentTime) {
                mostRecentTime = fileStat.st_mtime;
                mostRecentSavefile = savefilePath;
            }
        }
    }

    // If we found an existing save file and it's not in the preferred location
    if (!mostRecentSavefile.empty() &&
        mostRecentSavefile != preferredSavefile) {
        // Move the most recent save file to the preferred location
        if (rename(mostRecentSavefile.c_str(), preferredSavefile.c_str()) ==
            0) {
            log("Moved save file from " + mostRecentSavefile + " to " +
                preferredSavefile);
        } else {
            log("Failed to move save file from " + mostRecentSavefile + " to " +
                preferredSavefile);
        }
    }
    return preferredSavefile;
#endif
}

void loadSavefile() {
    std::string filePath = getSavefilePath();
    std::ifstream input(filePath, std::ios::binary | std::ios::in);
    if (!input.is_open()) {
        // Savefile does not exist, return early
        return;
    }

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
    std::string filePath = getSavefilePath();
    std::ofstream output(filePath, std::ios::binary | std::ios::out);

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
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    freeAudio();
    exit(0);
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

void setFullscreen(bool state) {
    fullscreen = state;
    if (!fullscreen) {
        SDL_SetWindowFullscreen(window, 0);
    } else {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
}

#ifdef ANDROID
static void androidReleaseRendererForGLES() {
    if (texture != nullptr) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
    if (renderer != nullptr) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
}

static void androidRestoreRendererAfterGLES() {
    if (window == nullptr || renderer != nullptr) {
        return;
    }
    renderer = SDL_CreateRenderer(window, -1, 0);
    if (renderer == nullptr) {
        log(std::string("SDL_CreateRenderer failed after shaders: ") +
            SDL_GetError());
    }
}
#endif

void shaderInit(int index) {
    initShaders(window, index);
#ifdef ANDROID
    if (shaderStatus == ShaderStatus::OK) {
        androidReleaseRendererForGLES();
    }
#endif
}

void shaderDeinit() {
    freeShaders();
#ifdef ANDROID
    androidRestoreRendererAfterGLES();
#endif
}

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

#ifdef ANDROID
void androidVirtualButtonEvent(int buttonId, bool pressed) {
    std::lock_guard<std::mutex> lock(g_androidInputMutex);
    if (pressed) {
        pressKey(buttonId, InputType::CONTROLLER);
    } else {
        unpressKey(buttonId, InputType::CONTROLLER);
    }
}

void androidSetPortraitMode(bool portrait) {
    g_androidPortraitLayout.store(portrait);
    g_androidRefreshLayout.store(true);
}

bool androidConsumeLayoutRefresh() {
    return g_androidRefreshLayout.exchange(false);
}

bool androidPortraitStackedLayout() {
    return g_androidPortraitLayout.load();
}
#endif
#endif
