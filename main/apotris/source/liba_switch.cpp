#ifdef SWITCH

#include "liba_switch.h"
#include <fstream>
#include <unordered_set>

#include "def.h"
#include "switch.h"

#include "nanotime.h"

void rumbleUpdate();

void handleInput();

std::unordered_set<uint32_t> currentKeys;
std::unordered_set<uint32_t> previousKeys;

std::unordered_set<uint32_t> currentlyPressed;

// static int rumbleTimer = 0;

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

bool render = true;

int screenWidth = 1280;
int screenHeight = 720;

float windowScale = 4;

nanotime_step_data stepper;

// rumble
HidVibrationDeviceHandle VibrationDeviceHandles[2][2];
Result rc = 0, rc2 = 0;
u32 target_device = 0;

HidVibrationValue VibrationValue;
HidVibrationValue VibrationValue_stop;
HidVibrationValue VibrationValues[2];
PadState pad;

void windowInit() {
    // Initialize SDL
    romfsInit();
    // chdir("romfs:/");

    window = NULL;
    window =
        SDL_CreateWindow("Apotris Switch", SDL_WINDOWPOS_CENTERED,
                         SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_SHOWN);

    renderer = SDL_CreateRenderer(window, -1, 0);

    SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    SDL_JoystickEventState(SDL_ENABLE);
    SDL_JoystickOpen(0);

    loadAudio("");

    nanotime_step_init(&stepper, (uint64_t)(NANOTIME_NSEC_PER_SEC / FPS_TARGET),
                       nanotime_now_max(), nanotime_now, nanotime_sleep);
}

void key_poll() {}

void handleInput() {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        uint32_t key = 0;
        bool found = false;

        switch (event.type) {
        case SDL_JOYBUTTONDOWN:
            key = event.jbutton.button;
            currentlyPressed.insert(key);
            break;
        case SDL_JOYBUTTONUP:
            key = event.jbutton.button;
            currentlyPressed.erase(key);
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

    // Cap to 60 FPS
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

    if (savefile != nullptr)
        setGradient(savefile->settings.backgroundGradient);
}

bool closed() {
    songEndHandler();
    padUpdate(&pad);
    rumbleUpdate();

    return true;
}

void toggleRendering(bool r) { render = r; }

void initRumble() {
    // Configure our supported input layout: a single player with standard
    // controller styles
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    // Initialize the default gamepad (which reads handheld mode inputs as well
    // as the first connected controller)
    padInitializeDefault(&pad);

    // Two VibrationDeviceHandles are returned: first one for left-joycon,
    // second one for right-joycon. Change the total_handles param to 1, and
    // update the hidSendVibrationValues calls, if you only want 1
    // VibrationDeviceHandle.
    rc = hidInitializeVibrationDevices(VibrationDeviceHandles[0], 2,
                                       HidNpadIdType_Handheld,
                                       HidNpadStyleTag_NpadHandheld);

    // Setup VibrationDeviceHandles for HidNpadIdType_No1 too, since we want to
    // support both HidNpadIdType_Handheld and HidNpadIdType_No1.
    if (R_SUCCEEDED(rc))
        rc = hidInitializeVibrationDevices(VibrationDeviceHandles[1], 2,
                                           HidNpadIdType_No1,
                                           HidNpadStyleTag_NpadJoyDual);

    memset(VibrationValues, 0, sizeof(VibrationValues));

    memset(&VibrationValue_stop, 0, sizeof(HidVibrationValue));
    // Switch like stop behavior with muted band channels and frequencies set to
    // default.
    VibrationValue_stop.freq_low = 160.0f;
    VibrationValue_stop.freq_high = 320.0f;
}

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
    if (!R_SUCCEEDED(rc))
        return;

    float factor = 1;

    if (savefile != nullptr) {
        factor *= (float)savefile->settings.rumbleStrength / 8.0f;
    }

    VibrationValue.amp_low = 1.0f * factor;
    VibrationValue.freq_low = 100.0f * factor;
    VibrationValue.amp_high = 1.0f * factor;
    VibrationValue.freq_high = 100.0f * factor;

    // Select which devices to vibrate.
    target_device = padIsHandheld(&pad) ? 0 : 1;

    memcpy(&VibrationValues[0], &VibrationValue, sizeof(HidVibrationValue));
    memcpy(&VibrationValues[1], &VibrationValue, sizeof(HidVibrationValue));

    rc2 = hidSendVibrationValues(VibrationDeviceHandles[target_device],
                                 VibrationValues, 2);
    if (R_FAILED(rc2))
        printf("hidSendVibrationValues() returned: 0x%x\n", rc2);
}

void rumbleUpdate() {
    rumblePatternLoop();
}

void rumbleStop() {
    // Select which devices to vibrate.
    target_device = padIsHandheld(&pad) ? 0 : 1;

    memcpy(&VibrationValues[0], &VibrationValue_stop,
           sizeof(HidVibrationValue));
    memcpy(&VibrationValues[1], &VibrationValue_stop,
           sizeof(HidVibrationValue));

    rc2 = hidSendVibrationValues(VibrationDeviceHandles[target_device],
                                 VibrationValues, 2);
    if (R_FAILED(rc2))
        printf("hidSendVibrationValues() for stop returned: 0x%x\n", rc2);

    // Could also do this with 1 hidSendVibrationValues() call + a larger
    // VibrationValues array.
    rc2 = hidSendVibrationValues(VibrationDeviceHandles[1 - target_device],
                                 VibrationValues, 2);
    if (R_FAILED(rc2))
        printf(
            "hidSendVibrationValues() for stop other device returned: 0x%x\n",
            rc2);
}

std::map<int, std::string> keyToString = {
    {0, "A"},        {1, "B"},       {2, "X"},       {3, "Y"},
    {6, "L"},        {7, "R"},       {8, "ZL"},      {9, "ZR"},
    {10, "+"},       {11, "-"},      {12, "Left"},   {13, "Up"},
    {14, "Right"},   {15, "Down"},   {16, "L.Left"}, {17, "L.Up"},
    {18, "L.Right"}, {19, "L.Down"}, {21, "R.Left"}, {22, "R.Up"},
    {23, "R.Right"}, {24, "R.Down"}, {43, "L3"},     {53, "R3"},
};

void quit() {
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    freeAudio();
    exit(0);
}

#endif
