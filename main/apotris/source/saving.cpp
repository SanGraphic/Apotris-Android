#include "achievementStructure.h"
#include "def.h"
#include "flashSaves.h"
#include "logging.h"
#include "prng.h"
#include "sprite1tiles_bin.h"
#include "sprites.h"
#include <algorithm>
#include <cstring>
#include <memory>
#include <string>

class SaveChange {
public:
    int saveId;
    int location;
    int size;

    SaveChange(int _id, int _location, int _size) {
        saveId = _id;
        location = _location;
        size = _size;
    }
};

void setDefaultKeys();
void resetSkins(Save* save);
void setDefaults(Save* save, int depth);
void setDefaultGraphics(Save* save, int depth);
void applySaveChanges(u8* newSave, u8* oldSave, int newSize);
void convertScores(Save* save);

const SaveChange saveChanges[] = {
    SaveChange(0x50, 132, 20 * 4), // add zone + buffer to keys
    SaveChange(0x50, 3120, 4096),  // add zone + buffer to scoreboards
    SaveChange(0x51, 624, 192),    // add zone + buffer to scoreboards
};
int changeLength = 3;

// Hash entire save file deterministically
u32 hashSave() {
    u32* dump = (u32*)savefile;
    u32 hash = 0;
    for (int i = 0; i < (int)(sizeof(Save) / sizeof(u32)); i++) {
        hash = whisky2(hash, dump[i]);
    }
    return hash;
}

void loadSave() {
    delete savefile;

    savefile = new Save();
    loadSavefile();

    const int pmSettings = 540;
    const int pmSave = 3876;

    // 0x50-0x51, re-pack desktop keys into mask?
    // This'll skip upgrading saves for 0x50 and 0x51, was this intended to be SAVE_TAG?
    if (savefile->newGame >= 0x50 && savefile->newGame <= 0x51) {
#if defined(PC) || defined(WEB)

        GameKeys keys = savefile->settings.keys;

        savefile->settings.keys = {
            .moveLeft = (SDL_CONTROLLER_BUTTON_DPAD_LEFT << 16) |
                        packKey(keys.moveLeft),
            .moveRight = (SDL_CONTROLLER_BUTTON_DPAD_RIGHT << 16) |
                         packKey(keys.moveRight),
            .rotateCCW =
                (SDL_CONTROLLER_BUTTON_A << 16) | packKey(keys.rotateCCW),
            .rotateCW =
                (SDL_CONTROLLER_BUTTON_B << 16) | packKey(keys.rotateCW),
            .rotate180 =
                (SDL_CONTROLLER_BUTTON_Y << 16) | packKey(keys.rotate180),
            .softDrop = (SDL_CONTROLLER_BUTTON_DPAD_DOWN << 16) |
                        packKey(keys.softDrop),
            .hardDrop =
                (SDL_CONTROLLER_BUTTON_DPAD_UP << 16) | packKey(keys.hardDrop),
            .hold = (SDL_CONTROLLER_BUTTON_RIGHTSHOULDER << 16) |
                    packKey(keys.hold),
            .zone =
                (SDL_CONTROLLER_BUTTON_LEFTSHOULDER << 16) | packKey(keys.zone),
        };

        MenuKeys menu = savefile->settings.menuKeys;

        savefile->settings.menuKeys = {
            .up = (SDL_CONTROLLER_BUTTON_DPAD_UP << 16) | packKey(menu.up),
            .down =
                (SDL_CONTROLLER_BUTTON_DPAD_DOWN << 16) | packKey(menu.down),
            .left =
                (SDL_CONTROLLER_BUTTON_DPAD_LEFT << 16) | packKey(menu.left),
            .right =
                (SDL_CONTROLLER_BUTTON_DPAD_RIGHT << 16) | packKey(menu.right),
            .confirm = (SDL_CONTROLLER_BUTTON_A << 16) | packKey(menu.confirm),
            .cancel = (SDL_CONTROLLER_BUTTON_B << 16) | packKey(menu.cancel),
            .pause = (SDL_CONTROLLER_BUTTON_START << 16) | packKey(menu.pause),
            .reset = packKey(menu.reset),
            .special1 = (SDL_CONTROLLER_BUTTON_LEFTSHOULDER << 16) |
                        packKey(menu.special1),
            .special2 = (SDL_CONTROLLER_BUTTON_RIGHTSHOULDER << 16) |
                        packKey(menu.special2),
            .special3 =
                (SDL_CONTROLLER_BUTTON_BACK << 16) | packKey(menu.special3),
        };

#endif
    } else if (savefile->newGame >= 0x4e && savefile->newGame <= 0x50) {
        // purposefully empty
    } else if (savefile->newGame >= 0x4b && savefile->newGame <= 0x4d) {
        int size = 0;
        switch (savefile->newGame) {
        case 0x4b:
            size = sizeof(Test) + 1;
            break;
        case 0x4c:
            size = sizeof(Test2) + 1;
            break;
        case 0x4d:
            size = sizeof(Test3);
            break;
        }

        Save* temp = new Save();

        u8* tmp = (u8*)temp;

        u8* sf = (u8*)savefile;

        for (int i = 0; i < size; i++)
            tmp[i] = sf[i];

        memcpy32_fast(&tmp[pmSettings + sizeof(u8)], &sf[size],
                      (pmSave - size) / 4);

        memcpy32_fast(savefile, temp, pmSave / 4);

        delete temp;
    } else if (savefile->newGame != SAVE_TAG && savefile->newGame >= 0x4b && savefile->newGame < SAVE_TAG) {
        // Upgrade old saves
        Save* temp = new Save();
        u8* tmp = (u8*)temp;

        u8* sf = (u8*)savefile;

        applySaveChanges(tmp, sf, sizeof(Save));

        memcpy32_fast(savefile, temp, sizeof(Save) / 4);

        delete temp;

        switch (savefile->newGame) {
        case 0x52:
            setDefaults(savefile, 8);
            break;
        case 0x51:
            setDefaults(savefile, 7);
            break;
        case 0x50:
        case 0x4f:
            setDefaults(savefile, 6);
            break;
        case 0x4e:
            setDefaults(savefile, 4);
            break;
        case 0x4d:
            setDefaults(savefile, 3);
            break;
        case 0x4c:
            setDefaults(savefile, 2);
            setDefaultKeys();
            break;
        case 0x4b:
            setDefaults(savefile, 1);
            setDefaultKeys();
            break;
        }

        savefile->newGame = SAVE_TAG;
    } else if (savefile->newGame != SAVE_TAG) {
        // No tag recognized, create fresh save file.
        // (this can be trigger-happy if misconfigured, maybe a prompt is in order to protect against errant triggers?)
        savefile = new Save();

        setDefaults(savefile, 0);

        setDefaultKeys();

        resetSkins(savefile);

        savefile->newGame = SAVE_TAG;
        // Set initial seed deterministic to save file (sampling will change it)
        savefile->seed = (int)hashSave();
    }

    // fix invalid values, since I messed up conversion at some point
    if ((savefile->settings.lightMode != 0) &&
        (savefile->settings.lightMode != 1))
        savefile->settings.lightMode = false;

    if ((savefile->settings.rumbleStrength < 0) || (savefile->settings.rumbleStrength > 8))
        savefile->settings.rumbleStrength = 0;

    if (savefile->settings.irs < 0 || savefile->settings.irs > 1)
        savefile->settings.irs = 1;
    if (savefile->settings.ihs < 0 || savefile->settings.ihs > 1)
        savefile->settings.ihs = 1;
    if (savefile->settings.initialType < 0 ||
        savefile->settings.initialType > 1)
        savefile->settings.initialType = 1;

    if (savefile->platform != PLATFORM) {
        auto oldPlatform = savefile->platform;
        savefile->platform = PLATFORM;

        savefile->settings.menuKeys = getDefaultMenuKeys();
        savefile->settings.keys = getDefaultGameKeys();

#ifdef N3DS
        savefile->settings.n3dsMainScreenIsTop = true;
        savefile->settings.n3dsScaleMode = n3ds::ScaleMode::SCALED_SHARP;
        savefile->settings._n3dsPlaceholder1 = 0;
        savefile->settings._n3dsPlaceholder2 = 0;
        savefile->settings._n3dsPlaceholder3 = 0;
#else
        if (oldPlatform == PLATFORM_N3DS) {
            savefile->settings.integerScale = true;
            savefile->settings.fullscreen = false;
        }
#endif
    }

#ifdef GBA
    sleep_combo = get_sleep_combo(savefile->settings.sleep.sleepWakeCombo);
    status.update(&savefile->settings);
#endif
    saveSavefile();
}

GameKeys getDefaultGameKeys() {
    GameKeys k;

#if defined(PC) || defined(WEB)
    k = {
        .moveLeft =
            (SDL_CONTROLLER_BUTTON_DPAD_LEFT << 16) | packKey(SDLK_LEFT),
        .moveRight =
            (SDL_CONTROLLER_BUTTON_DPAD_RIGHT << 16) | packKey(SDLK_RIGHT),
        .rotateCCW = (SDL_CONTROLLER_BUTTON_A << 16) | packKey(SDLK_z),
        .rotateCW = (SDL_CONTROLLER_BUTTON_B << 16) | packKey(SDLK_UP),
        .rotate180 = (SDL_CONTROLLER_BUTTON_Y << 16) | packKey(SDLK_a),
        .softDrop =
            (SDL_CONTROLLER_BUTTON_DPAD_DOWN << 16) | packKey(SDLK_DOWN),
        .hardDrop = (SDL_CONTROLLER_BUTTON_DPAD_UP << 16) | packKey(SDLK_SPACE),
        .hold = (SDL_CONTROLLER_BUTTON_RIGHTSHOULDER << 16) | packKey(SDLK_c),
        .zone = (SDL_CONTROLLER_BUTTON_LEFTSHOULDER << 16) | packKey(SDLK_s),
    };

    return k;
#endif

    k = {
        .moveLeft = KEY_LEFT,
        .moveRight = KEY_RIGHT,
        .rotateCCW = KEY_B,
        .rotateCW = KEY_A,
        .rotate180 = UNBOUND,
        .softDrop = KEY_DOWN,
        .hardDrop = KEY_UP,
        .hold = KEY_R,
        .zone = KEY_SELECT,
    };

#if defined(GBA) || defined(N3DS)
    k.hold |= KEY_L;
#endif

    return k;
}

MenuKeys getDefaultMenuKeys() {
    MenuKeys m{
        .up = KEY_UP,
        .down = KEY_DOWN,
        .left = KEY_LEFT,
        .right = KEY_RIGHT,
        .confirm = KEY_A,
        .cancel = KEY_B,
        .pause = KEY_START,
        .reset = UNBOUND,
        .special1 = KEY_L,
        .special2 = KEY_R,
        .special3 = KEY_SELECT,
    };

#ifdef GBA
    m.reset = 0;
#endif

    return m;
}

void setDefaultKeys() {
    savefile->settings.keys = getDefaultGameKeys();

    savefile->settings.menuKeys = getDefaultMenuKeys();
}

void setDefaultGameplay(Save* save) {
    save->settings.maxQueue = 5;
    save->settings.pro = false;
    save->settings.goalLine = 2;
    save->settings.showSpawn = 0;
    save->settings.pauseCountdown = 1;
    save->settings.peek = true;
    save->settings.big = false;
    save->settings.rotationSystem = BlockEngine::SRS;
    save->settings.randomizer = BlockEngine::BAG_7;
}

void setDefaultHandling(Save* save) {
    save->settings.customDas = false;
    save->settings.das = 11;
    save->settings.arr = 2;
    save->settings.sfr = 2;
    save->settings.delaySoftDrop = false;
    save->settings.dropProtection = true;
    save->settings.dropProtectionFrames = 8;

    save->settings.directionalDas = true;

    save->settings.noDiagonals = false;
    save->settings.diagonalType = false;

    save->settings.irs = true;
    save->settings.ihs = true;
    save->settings.initialType = 1;
}

void resetSkins(Save* save) {
    for (int i = 0; i < MAX_CUSTOM_SKINS; i++) {
        memcpy16_fast(&save->customSkins[i].board, sprite1tiles_bin,
                      sprite1tiles_bin_size / 2);
        memcpy16_fast(&save->customSkins[i].smallBoard, mini[0],
                      sprite1tiles_bin_size / 2);
        save->customSkins[i].changed = false;
    }
}

void setDefaults(Save* save, int depth) {

    if (depth < 1) {
        save->settings.announcer = true;
        save->settings.finesse = false;
        save->settings.das = 11;
        save->settings.arr = 2;
        save->settings.sfr = 2;
        save->settings.dropProtection = true;
        save->settings.volume = 10;

        for (int i = 0; i < 8; i++)
            savefile->latestName[i] = ' ';
        savefile->latestName[8] = '\0';

        memset32_fast(savefile->boards.marathon, 0, sizeof(EntryBoard) * 4 / 4);
        memset32_fast(savefile->boards.sprint, 0, sizeof(EntryBoard) * 3 / 4);
        memset32_fast(savefile->boards.dig, 0, sizeof(EntryBoard) * 3 / 4);
    }

    if (depth < 2) {
        for (int i = 0; i < 10; i++)
            save->settings.songList[i] = true;

        memset32_fast(savefile->boards.ultra, 0, sizeof(EntryBoard) * 3 / 4);
    }

    if (depth < 3) {
        save->settings.maxQueue = 5;
        save->settings.sfxVolume = 10;
        save->settings.directionalDas = true;
        save->settings.noDiagonals = false;
        save->settings.cycleSongs = 1; // CYCLE
        save->settings.dropProtectionFrames = 8;
        save->settings.abHold = true;
        save->settings.resetHoldType = false;

        memset32_fast(savefile->boards.blitz, 0, sizeof(EntryBoard) * 2 / 4);
        memset32_fast(&savefile->boards.combo, 0, sizeof(EntryBoard) * 1 / 4);
        memset32_fast(savefile->boards.survival, 0, sizeof(EntryBoard) * 3 / 4);
    }

    if (depth < 4) {
        save->settings.rumbleStrength = 0;
    }

    if (depth < 5) {
        memset32_fast(savefile->boards.sprintAttack, 0,
                      sizeof(EntryBoard) * 3 / 4);
        memset32_fast(savefile->boards.digEfficiency, 0,
                      sizeof(EntryBoard) * 3 / 4);
        memset32_fast(savefile->boards.classic, 0, sizeof(EntryBoard) * 2 / 4);

        save->stats.timePlayed = 0;
        save->stats.gamesStarted = 0;
        save->stats.gamesCompleted = 0;
        save->stats.gamesLost = 0;

        resetSkins(save);
    }

    if (depth < 6) {
        memset32_fast(savefile->boards.master, 0, sizeof(EntryBoard) * 2 / 4);
        memset32_fast(savefile->boards.zone, 0, sizeof(EntryBoard) * 4 / 4);

        save->settings.diagonalType = save->settings.noDiagonals;
        save->settings.delaySoftDrop = false;
        save->settings.customDas = false;
        save->settings.irs = true;
        save->settings.ihs = true;
        save->settings.initialType = 1;
        save->settings.resetHoldToggle = 0;

        // check if select is already bound - if not, bind it to zone activation
        int* keys = (int*)&save->settings.keys;

        bool found = false;

        std::list<int> foundKeys;
        for (int i = 0; i < 9; i++) {
            foundKeys.clear();

            int k = keys[i];
            int counter = 0;
            do {
                if (k & (1 << counter)) {
                    foundKeys.push_back(1 << counter);
                    k -= 1 << counter;
                }

                counter++;
            } while (k != 0);

            if (std::find(foundKeys.begin(), foundKeys.end(),
                          (int)KEY_SELECT) != foundKeys.end()) {
                found = true;
                break;
            }
        }

        if (!found)
            save->settings.keys.zone = KEY_SELECT;
        else
            save->settings.keys.zone = 0;
    }

    if (depth < 7) {
        if (depth)
            convertScores(savefile);

        save->settings.big = false;
        save->settings.pro = false;
        save->settings.goalLine = 2;
        save->settings.showSpawn = 0;
        save->settings.rotationSystem = BlockEngine::SRS;
        save->settings.randomizer = BlockEngine::BAG_7;
        save->settings.zoom = -1;
#ifdef N3DS
        save->settings.n3dsMainScreenIsTop = true;
        save->settings.n3dsScaleMode = n3ds::ScaleMode::SCALED_SHARP;
        save->settings._n3dsPlaceholder1 = 0;
        save->settings._n3dsPlaceholder2 = 0;
#else
        save->settings.integerScale = true;
#endif
        save->settings.peek = true;
        save->settings.moveSfx = 2;
        save->settings.pauseCountdown = 1;
        save->settings.clearText = 2;
        save->settings.selectedProfile = 0;
        save->settings.journey = false;
        save->settings.autosave = 1;
        save->settings.showFPS = false;
#ifdef N3DS
        save->settings._n3dsPlaceholder3 = 0;
#else
#ifdef ANDROID
        save->settings.fullscreen = true;
#else
        save->settings.fullscreen = false;
#endif
#endif
        save->settings.shaders = false;

        save->stats.maxLevel = 0;
        save->stats.totalLines = 0;
        save->stats.gameStats = BlockEngine::Stats();

        memset32_fast(savefile->boards.death, 0, sizeof(EntryBoard) * 2 / 4);

        // swapped CW/CCW rotates in settings
        int temp = save->settings.keys.rotateCW;
        save->settings.keys.rotateCW = save->settings.keys.rotateCCW;
        save->settings.keys.rotateCCW = temp;

        MenuKeys m{
            .up = KEY_UP,
            .down = KEY_DOWN,
            .left = KEY_LEFT,
            .right = KEY_RIGHT,
            .confirm = KEY_A,
            .cancel = KEY_B,
            .pause = KEY_START,
            .reset = UNBOUND,
            .special1 = KEY_L,
            .special2 = KEY_R,
            .special3 = KEY_SELECT,
        };

#ifdef GBA
        m.reset = 0;
#endif

        save->settings.menuKeys = m;

        save->platform = PLATFORM;

        strncpy(savefile->endTag, "SAVE_END", 9);
    }

    if (depth < 8) {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 7; j++) {
                memcpy16(&save->customPalettes[i][j], &palette[1][j * 16 + 1],
                         3);
            }
            loadPalette(i + 16, 0, palette, 3);
        }
    }

    setDefaultGraphics(save, depth);

    if (depth < 7) {
        for (int i = 0; i < 5; i++) {
            saveToProfile(&save->settings.profiles[i], &save->settings);
        }
    }

    if (depth < 9) {
        save->settings.zoneRumbleMode = 1; // Set to Continuous Zone Rumble
        save->settings.rumbleStyle = 1; // Set to new Dynamic rumble!
        save->settings.rumbleStrength = save->settings.rumbleStrength * 2; // Scale old value to new scale (0-4 -> 0-8)

        // -- GBA Specific-settings
        save->settings.sleep.autosleepDelay = 0; // Disable Autosleep
        save->settings.sleep.autosleepWakeCombo = 1; // Use any button to wake from Autosleep
        save->settings.sleep.sleepWakeCombo = 1; // Use L+R+Select as default Sleep/Wake combo.

        save->settings.rumbleCartType = 0; // Use ROM GPIO as default Rumble Cart type, most common and least likely to cause issues.
        save->settings.rumbleUpdateRate = 0; // Default to using VBlank for Rumble Pattern ISR

        save->settings.ezFlash3in1Strength = EZFLASH_RUMBLE_IGNORE; // By default, 3-in-1 support should be disabled.
        // -- End of GBA-specifics

        memset32_fast(savefile->boards.tower, 0, sizeof(EntryBoard) * 4 / 4);
        savefile->boards.zenLines = 0;
        savefile->boards.zenTower = 0;
    }

}

bool compareVersion(const SaveChange& first, const SaveChange& second) {
    return first.saveId < second.saveId;
}

bool compareLocation(const SaveChange& first, const SaveChange& second) {
    return first.location < second.location;
}

void applySaveChanges(u8* newSave, u8* oldSave, int newSize) {
    int migSize = 3876;
    bool changed = false;

    int oldPos = 0;
    int newPos = 0;

    int i = 0;
    while (i < changeLength) {
        if (saveChanges[i].saveId <= savefile->newGame) {
            migSize += saveChanges[i].size;
            i++;
            continue;
        }

        changed = true;

        int count = saveChanges[i].location - oldPos;

        memcpy32_fast(&newSave[newPos], &oldSave[oldPos], count / 4);

        oldPos = saveChanges[i].location;
        newPos += count + saveChanges[i].size;

        i++;

        if (i >= changeLength ||
            saveChanges[i - 1].saveId != saveChanges[i].saveId) {
            memcpy32_fast(&newSave[newPos], &oldSave[oldPos],
                          (migSize - oldPos) / 4);

            memcpy32_fast(oldSave, newSave, (newPos + migSize - oldPos) / 4);

            migSize += newPos - oldPos;

            oldPos = newPos = 0;
        }
    }

    if (!changed) {
        memcpy32_fast(newSave, oldSave, sizeof(Save) / 4);
    }
}

void setDefaultGraphics(Save* save, int depth) {
    if (depth < 1) {
        save->settings.floatText = true;
        save->settings.shake = true;
        save->settings.effects = true;
    }

    if (depth < 2) {
        save->settings.edges = true;
        save->settings.backgroundGrid = 6;
        save->settings.skin = 12;
        save->settings.palette = 5;
        save->settings.shadow = 5;
        save->settings.lightMode = false;
    }

    if (depth < 3) {
        save->settings.colors = 1;
        save->settings.clearEffect = 2;

        if (save->settings.shake) {
            save->settings.shakeAmount = 2;
        } else {
            save->settings.shake = true;
            save->settings.shakeAmount = 0;
        }
    }

    if (depth < 4) {
        save->settings.placeEffect = true;
        // save->settings.backgroundGradient = 0x6c525141;
        save->settings.backgroundGradient = 0x2dd827bf;
    }

    if (depth < 7) {
        save->settings.frameBackground = 1;
        save->settings.backgroundType = 1;
#ifdef N3DS
        n3ds::savedAspectRatio = 1;
#endif
        save->settings.aspectRatio = 1;
        save->settings.clearText = 2;

#if defined(GBA)
        save->settings.screenShakeType = 0;
#else
        save->settings.screenShakeType = 1;
#endif

        save->settings.clearDirection = 0;
    }
}

void convertScores(Save* save) {
    OldScoreboards* oldBoards = (OldScoreboards*)&save->boards;

    ModeBoards newBoards;

    memcpy32_fast(&newBoards.marathon, &oldBoards->marathon,
                  sizeof(EntryBoard) * 4 / 4);
    memcpy32_fast(&newBoards.sprint, &oldBoards->sprint,
                  sizeof(EntryBoard) * 3 / 4);
    memcpy32_fast(&newBoards.dig, &oldBoards->dig, sizeof(EntryBoard) * 3 / 4);
    memcpy32_fast(&newBoards.ultra, &oldBoards->ultra,
                  sizeof(EntryBoard) * 3 / 4);
    memcpy32_fast(&newBoards.blitz, &oldBoards->blitz,
                  sizeof(EntryBoard) * 2 / 4);
    memcpy32_fast(&newBoards.combo, &oldBoards->combo,
                  sizeof(EntryBoard) * 1 / 4);
    memcpy32_fast(&newBoards.survival, &oldBoards->survival,
                  sizeof(EntryBoard) * 3 / 4);
    memcpy32_fast(&newBoards.sprintAttack, &oldBoards->sprintAttack,
                  sizeof(EntryBoard) * 3 / 4);
    memcpy32_fast(&newBoards.digEfficiency, &oldBoards->digEfficiency,
                  sizeof(EntryBoard) * 3 / 4);
    memcpy32_fast(&newBoards.classic, &oldBoards->classic,
                  sizeof(EntryBoard) * 2 / 4);
    memcpy32_fast(&newBoards.zone, &oldBoards->zone,
                  sizeof(EntryBoard) * 4 / 4);

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 5; j++) {
            newBoards.master[i].entries[j].grade =
                oldBoards->master[i].grade[j];
            newBoards.master[i].entries[j].value =
                oldBoards->master[i].times[j].frames;

            if (oldBoards->master[i].times[j].frames > 0) {
                strncpy(newBoards.master[i].entries[j].name,
                        oldBoards->master[i].times[j].name, 9);
            } else {
                newBoards.master[i].entries[j].name[0] = '\0';
            }
        }
    }

    EntryBoard* b = (EntryBoard*)&newBoards;

    for (int i = 0; i < 33; i++) {
        for (int j = 0; j < 5; j++) {
            b->entries[j].pro = false;
        }
        b++;
    }

    memcpy32_fast(&save->boards, &newBoards, sizeof(ModeBoards) / 4);
}

void profileToSave(GraphicsOptions* profile, Settings* settings) {
    settings->clearText = profile->clearText;
    settings->shake = profile->shake;
    settings->effects = profile->effects;
    settings->edges = profile->edges;
    settings->backgroundGrid = profile->backgroundGrid;
    settings->skin = profile->skin;
    settings->palette = profile->palette;
    settings->shadow = profile->shadow;
    settings->lightMode = profile->lightMode;
    settings->colors = profile->colors;
    settings->clearEffect = profile->clearEffect;
    settings->shakeAmount = profile->shakeAmount;
    settings->backgroundGradient = profile->backgroundGradient;
    settings->placeEffect = profile->placeEffect;
    settings->frameBackground = profile->frameBackground;
    settings->backgroundType = profile->backgroundType;
#ifdef N3DS
    n3ds::savedAspectRatio = profile->aspectRatio;
#else
    settings->aspectRatio = profile->aspectRatio;
#endif
    settings->screenShakeType = profile->screenShakeType;
    settings->clearDirection = profile->clearDirection;
}

void saveToProfile(GraphicsOptions* profile, Settings* settings) {
    profile->clearText = settings->clearText;
    profile->shake = settings->shake;
    profile->effects = settings->effects;
    profile->edges = settings->edges;
    profile->backgroundGrid = settings->backgroundGrid;
    profile->skin = settings->skin;
    profile->palette = settings->palette;
    profile->shadow = settings->shadow;
    profile->lightMode = settings->lightMode;
    profile->colors = settings->colors;
    profile->clearEffect = settings->clearEffect;
    profile->shakeAmount = settings->shakeAmount;
    profile->backgroundGradient = settings->backgroundGradient;
    profile->placeEffect = settings->placeEffect;
    profile->frameBackground = settings->frameBackground;
    profile->backgroundType = settings->backgroundType;
#ifdef N3DS
    profile->aspectRatio = n3ds::savedAspectRatio;
#else
    profile->aspectRatio = settings->aspectRatio;
#endif
    profile->screenShakeType = settings->screenShakeType;
    profile->clearDirection = settings->clearDirection;
}

void autosave() {
    if (!savefile->settings.autosave || multiplayer)
        return;

    if (framesSinceLastSave < savefile->settings.autosave * 5 * FRAMES_PER_MIN)
        return;

    saveSavefile();
    framesSinceLastSave = 0;
}
