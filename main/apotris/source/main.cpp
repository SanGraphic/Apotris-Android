#include "sceneModes.hpp"
#include <string>

#include "blockEngine.hpp"
#include "cmath"
#include "def.h"
#include "platform.hpp"
#include "sceneGraphics.hpp"
#include "sprites.h"

#ifdef GBA
#include "LinkUniversal.hpp"
#endif

#include "achievementStructure.h"
#include "logging.h"

#include "text.h"

#include "flashSaves.h"

#include "rumble.h"
#include <posprintf.h>

#include "classic1tiles_bin.h"
#include "classic_pal_bin.h"

#include "tetromino.hpp"

#include "defaultGradient_bin.h"

#include "menu.h"
#include "scene.hpp"

#include "font1tiles_bin.h"
#include "font2tiles_bin.h"

#include "sceneColorEditor.hpp"

using namespace BlockEngine;

[[maybe_unused]] const std::string save_type = "SRAM_Vnnn";
int getClassicPalette();
void genWidths();

OBJ_ATTR obj_buffer[128];

int frameCounter = 1;

int shake = 0;

int gameSeconds;

bool paused = false;

int marathonClearTimer = 20;

int initialLevel = 0;

bool canDraw = false;

int profileResults[10];

void addToResults(int input, int index);

Save* savefile = nullptr;

u8* blockSprite;

bool restart = false;

bool playAgain = false;
int nextSeed = 0;

bool multiplayer = false;
bool playingBotGame = false;

bool rumble_enabled = false;

Scene* scene = nullptr;

bool gradientEnabled = false;

Options* previousGameOptions = nullptr;

#ifdef GBA
bool gbpChecked = false;
#endif

#ifdef GBA
__attribute((section(".ewram.status")))
#endif
StatusData status{};

#ifdef GBA
u16 gradientTable[192 + 1];
#endif

#ifndef GBA
u16 gradientTable[SCREEN_HEIGHT + 1];
#endif

const std::list<std::string> controlOptions = {
    "Move Left", "Move Right", "Rotate Left", "Rotate Right",  "Rotate 180",
    "Soft Drop", "Hard Drop",  "Hold",        "Activate Zone",
};

const std::list<std::string> menuControlOptions = {
    "Up",    "Down",    "Left",      "Right",     "Confirm",   "Cancel",
    "Pause", "Restart", "Special 1", "Special 2", "Special 3",
};

void initialize() {
    initRumble();

#ifdef PC
    setFullscreen(savefile != nullptr && savefile->settings.fullscreen);

#endif

#if defined(PC) || defined(PORTMASTER)
    if (savefile->settings.shaders != 0) {
        shaderInit(savefile->settings.shaders);
    }
#endif

    toggleSprites(true);

    buildBG(0, 2, 29, 0, 0, true);
    buildBG(1, 0, 26, 0, 3, true);
    buildBG(2, 0, 25, 0, 2, true);
    buildBG(3, 0, 27, 0, 1, true);

    for (int i = 0; i < 4; i++) {
        toggleBG(i, true);
        setLayerScroll(i, 0, 0);
    }

    // Load bg tiles

    loadTiles(0, 2, sprite3tiles_bin, sprite3tiles_bin_size / 32);
    loadTiles(0, 4, sprite5tiles_bin, sprite5tiles_bin_size / 32);
    loadTiles(0, 6, sprite8tiles_bin, sprite8tiles_bin_size / 32);
    loadTiles(0, 12, sprite10tiles_bin, sprite10tiles_bin_size / 32);
    loadTiles(0, 13, sprite11tiles_bin, sprite11tiles_bin_size / 32);
    loadTiles(0, 14, sprite12tiles_bin, sprite12tiles_bin_size / 32);
    loadTiles(0, 15, sprite13tiles_bin, sprite13tiles_bin_size / 32);
    loadTiles(0, 26, sprite17tiles_bin, sprite17tiles_bin_size / 32);
    loadTiles(0, 27, sprite20tiles_bin, sprite20tiles_bin_size / 32);
    loadTiles(0, 30, sprite22tiles_bin, sprite22tiles_bin_size / 32);
    loadTiles(0, 31, sprite23tiles_bin, sprite23tiles_bin_size / 32);
    loadTiles(0, 32, sprite24tiles_bin, sprite24tiles_bin_size / 32);
    loadTiles(0, 33, sprite42tiles_bin, sprite42tiles_bin_size / 32);
    loadTiles(0, 34, sprite45tiles_bin, sprite45tiles_bin_size / 32);
    loadTiles(0, 35, sprite46tiles_bin, sprite46tiles_bin_size / 32);
    loadTiles(0, 36, sprite47tiles_bin, sprite47tiles_bin_size / 32);

    for (int i = 0; i < 8; i++)
        loadSpriteTiles(512 + 128 + i, moveSpriteTiles[i], 1, 1);

    clearTiles(2, 0, 1);
    loadTiles(2, 1, font1tiles_bin, font1tiles_bin_size / 32);

    setSkin();
    setPalette();
    setClearEffect();

    initFallingBlocks();

    refreshWindowSize();

    setGradient(GRADIENT_COLOR);

    checkSongs();
}

int main(void) {
    platformInit();

    loadSave();
    randSetSeed(savefile->seed);

    initialize();

    changeScene(new TitleScene());

    // start screen animation
    while (closed()) {
        vsync();
        scene->update();
        sampleEntropy();
    }

    deinitialize();
}

#ifdef ANDROID
extern "C" int SDL_main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    main();
    return 0;
}
#endif

#ifdef PC
int WinMain(void) {
    main();
    return 0;
}
#endif

std::string timeToString(int frames, bool small) {
    int t = (int)frames * 0.0167f;
    int millis = (int)(frames * 1.67f) % 100;
    int seconds = t % 60;
    int minutes = t / 60;

    char res[30];

    if (!small)
        posprintf(res, "%02d:%02d.%02d", minutes, seconds, millis);
    else
        posprintf(res, "%02d:%02d", minutes, seconds);

    std::string result = "";

    result = res;

    return result;
}

void addToResults(int input, int index) { profileResults[index] = input; }

void reset() {
    clearSprites(128);
    clearText();
    setLayerScroll(1, 0, 0);
    setLayerScroll(2, 0, 0);

    shake = 0;
    shakeBuff = 0;
    shakeVelocity = 0;
    push = 0;

    peek = 0;
    peekValue = 0;
    peekShift = 0;

    floatingList.clear();
    placeEffectList.clear();

    rumblePatternStop();

    resetZonePalette();

    clearTilemap(25);
    clearTilemap(26);
    clearTilemap(27);

    if (saveExists) {
        delete quickSave;
        saveExists = false;
    }

    setMosaic(0, 0);

    // reset glow
    for (int i = 0; i < 20; i++)
        for (int j = 0; j < 10; j++)
            glow[i][j] = 0;

    int g = savefile->settings.backgroundGradient;

    if (g == 0)
        memcpy16_fast(gradientTable, defaultGradient_bin,
                      defaultGradient_bin_size / 2);
    else
        setGradient(g);

    memset32_fast(current, 0, 22 * 12);
    memset32_fast(previous, 0, 22 * 12);

#ifdef GBA
    toggleSpriteSorting(false);
#endif
}

std::string nameInput(int place) {

    std::string result = savefile->latestName;
    int cursor = 0;

    for (int i = 0; i < (int)result.size(); i++) {
        if (result[i] == ' ') {
            cursor = i;
            break;
        }
    }

    const static int nameHeight = 14;

    int timer = 0;

    int das = 0;
    int maxDas = 12;

    int arr = 0;
    int maxArr = 5;

    bool onDone = false;

    int cursorFloat = 0;
    OBJ_ATTR* cursorSprites[2];

    for (int i = 0; i < 2; i++)
        cursorSprites[i] = &obj_buffer[1 + i];

    loadSpriteTiles(16 * 7, blockSprite, 1, 1);

    showSprites(128);

    if (place == 0 && game->gameMode != BATTLE &&
        (game->won || game->gameMode == MARATHON || game->gameMode >= BLITZ))
        aprint("New Record", 10, 5);

    aprint("Name: ", 11, nameHeight - 2);

    MenuKeys k = savefile->settings.menuKeys;

#if defined(N3DS)
    resetSmallText();
    setSmallTextArea(220, 0, 17, 30, 20);

    // Show keyboard immediately if name is empty
    bool requestKeyboard = (result.find_last_not_of(' ') == std::string::npos);
#endif

    while (closed()) {
        vsync();

        aprint("DONE", 14, 16);

        key_poll();

        aprintClearArea(13, nameHeight, 10, 1);
        aprint(result, 13, nameHeight);

#if defined(N3DS)
        {
            std::string s = "Press " + getStringFromKey(k.special3) +
                            " to use on-screen keyboard";
            aprints(s, (30 * 8 - 4 * s.size()) / 2, 8, 2);
        }

        requestKeyboard =
            requestKeyboard ||
            (key_hit(k.special3) && !(key_is_down(KEY_FULL) & (~k.special3)));

        if (requestKeyboard) {
            requestKeyboard = false;
            sfx(SFX_MENUMOVE);
            if (n3ds::keyboardNameInput(&result, 8)) {
                result.resize(8, ' ');
                cursor = max(min(result.find_last_not_of(' ') + 1, 7), 0);
                onDone = true;
                sfx(SFX_MENUCONFIRM);
            }
        }
#endif

        if (!onDone) {
            char curr = result.at(cursor);

#if defined(PC)
            if (lastInputType == InputType::KEYBOARD)
                goto keyboard;
#endif

            if (key_hit(k.confirm) || key_hit(k.right)) {
                cursor++;
                if (cursor > 7) {
                    onDone = true;
                    cursor = 7;
                    sfx(SFX_MENUCONFIRM);
                } else {
                    sfx(SFX_MENUMOVE);
                }
            }

            if (key_hit(k.cancel)) {
                result[cursor] = ' ';
                if (cursor > 0)
                    cursor--;
                sfx(SFX_MENUCANCEL);
            }

            if (key_hit(k.left)) {
                if (cursor > 0)
                    cursor--;
                sfx(SFX_MENUMOVE);
            }

            if (key_hit(k.up)) {
                if (curr == 'A')
                    result[cursor] = ' ';
                else if (curr == ' ')
                    result[cursor] = 'Z';
                else if (curr > 'A')
                    result[cursor] = curr - 1;

                sfx(SFX_MENUMOVE);
            } else if (key_hit(k.down)) {
                if (curr == 'Z')
                    result[cursor] = ' ';
                else if (curr == ' ')
                    result[cursor] = 'A';
                else if (curr < 'Z')
                    result[cursor] = curr + 1;
                sfx(SFX_MENUMOVE);
            } else if (key_is_down(k.up) || key_is_down(k.down)) {
                if (das < maxDas)
                    das++;
                else {
                    if (arr++ > maxArr) {
                        arr = 0;
                        if (key_is_down(k.up)) {
                            if (curr == 'A')
                                result[cursor] = ' ';
                            else if (curr == ' ')
                                result[cursor] = 'Z';
                            else if (curr > 'A')
                                result[cursor] = curr - 1;
                        } else {
                            if (curr == 'Z')
                                result[cursor] = ' ';
                            else if (curr == ' ')
                                result[cursor] = 'A';
                            else if (curr < 'Z')
                                result[cursor] = curr + 1;
                        }
                        sfx(SFX_MENUMOVE);
                    }
                }
            } else {
                das = 0;
                if (timer++ > 19)
                    timer = 0;

                if (timer < 10)
                    aprint("_", 13 + cursor, nameHeight);
            }

#if defined(PC) || defined(WEB)
        keyboard:

            u32 key = key_first();

            char character = getStringFromKey(key)[0];

            if (lastInputType == InputType::CONTROLLER)
                goto end;

            if (cursor > 0 &&
                ((key == SDLK_BACKSPACE) || (das == maxDas && arr == 0))) {
                if (cursor < 7 || result[cursor] == ' ') {
                    result[cursor - 1] = ' ';

                    cursor--;
                } else {
                    result[cursor] = ' ';
                }

                sfx(SFX_MENUMOVE);
            } else if (key == SDLK_SPACE && result.size() < 9) {
                result[cursor] = ' ';
                if (cursor < 7)
                    cursor++;
                sfx(SFX_MENUMOVE);
            } else if (getStringFromKey(key).size() == 1 &&
                       ((character >= 'A' && character <= 'Z') ||
                        (character >= 'a' && character <= 'z'))) {
                result[cursor] = character;

                if (cursor < 7)
                    cursor++;

                sfx(SFX_MENUMOVE);
            }

            if (key_is_down(SDLK_BACKSPACE)) {
                if (das < maxDas)
                    das++;

                if (++arr >= maxArr) {
                    arr = 0;
                }
            } else {
                das = 0;
            }

            if (timer++ > 19)
                timer = 0;

            if (timer < 10)
                aprint("_", 13 + cursor, nameHeight);

            if (key_hit(SDLK_RETURN) || key_hit(SDLK_ESCAPE)) {
                onDone = true;
                sfx(SFX_MENUCONFIRM);
            }

#endif
        end:

            if (key_hit(k.pause)) {
                onDone = true;
                sfx(SFX_MENUCONFIRM);
            }
        } else {
            if (key_hit(k.confirm) || key_hit(k.pause)) {
                sfx(SFX_MENUCONFIRM);
                break;
            }

            if (key_hit(k.cancel)) {
                onDone = false;
                sfx(SFX_MENUCANCEL);
            }
        }
        updateFluid();
        drawGrid();

        if (onDone) {
            const int offset = (sinLut(cursorFloat) * 2) >> 12;
            const FIXED scale = float2fx((1.0 - ((float)0.1 * offset)));

            for (int i = 0; i < 2; i++) {
                sprite_unhide(cursorSprites[i], 0);
                sprite_set_attr(cursorSprites[i], ShapeSquare, 0, 7 * 16, 5, 0);
                sprite_enable_affine(cursorSprites[i], 12, true);
                sprite_set_size(cursorSprites[i], scale, 12);

                const int x = (16 * 8) -
                              ((32 + 8) / 2 + offset + 4) * ((i) ? -1 : 1) - 10;

                sprite_set_pos(cursorSprites[i], x, 16 * 8 - 5);
            }
        } else {
            for (int i = 0; i < 2; i++)
                sprite_hide(cursorSprites[i]);
        }

        showSprites(128);

        cursorFloat += 6;
        if (cursorFloat >= 512)
            cursorFloat = 0;
    }

    for (int i = 0; i < 2; i++)
        sprite_hide(cursorSprites[i]);

    clearText();

#if defined(N3DS)
    resetSmallText();
    aprintClearArea(0, 17, 30 + 1, 20);
#endif

    if (result.size() >= 8)
        result = result.substr(0, 8);

    for (int i = 0; i < 8; i++)
        savefile->latestName[i] = ' ';

    for (int i = 0; i < result.size(); i++)
        savefile->latestName[i] = result.at(i);

    return result;
}

void setSkin() { setSkin(0); }

void setSkin(int rotation) {

    switch (savefile->settings.skin) {
    case 0:
        blockSprite = (u8*)sprite1tiles_bin;
        break;
    case 1:
        blockSprite = (u8*)sprite7tiles_bin;
        break;
    case 2:
        blockSprite = (u8*)sprite9tiles_bin;
        break;
    case 3:
        blockSprite = (u8*)sprite14tiles_bin;
        break;
    case 4:
        blockSprite = (u8*)sprite18tiles_bin;
        break;
    case 5:
        blockSprite = (u8*)sprite19tiles_bin;
        break;
    case 6:
        blockSprite = (u8*)sprite21tiles_bin;
        break;
    case 7:

        for (int i = 0; i < 8; i++)
            loadTiles(0, 48 + i, classicTiles[0][i],
                      classic1tiles_bin_size / 32);

        blockSprite = (u8*)classicTiles[0][0];
        break;
    case 8:
        for (int i = 0; i < 8; i++)
            loadTiles(0, 48 + i, classicTiles[1][i],
                      classic1tiles_bin_size / 32);

        blockSprite = (u8*)classicTiles[1][0];
        break;
    case 9:
        blockSprite = (u8*)sprite27tiles_bin;
        break;
    case 10:
        blockSprite = (u8*)sprite28tiles_bin;
        break;
    case 11:
        blockSprite = (u8*)&sprite38tiles_bin[12 * 32];

        loadTiles(0, 128, sprite38tiles_bin, sprite38tiles_bin_size / 32);
        break;
    case 12:
        blockSprite = (u8*)&sprite39tiles_bin[12 * 32];

        loadTiles(0, 128, sprite39tiles_bin, sprite39tiles_bin_size / 32);
        break;
    case 13:
        blockSprite = (u8*)&sprite40tiles_bin[12 * 32];

        loadTiles(0, 128, sprite40tiles_bin, sprite40tiles_bin_size / 32);
        break;
    case 14:
        blockSprite = (u8*)&sprite53tiles_bin[12 * 32];

        loadTiles(0, 128, sprite53tiles_bin, sprite53tiles_bin_size / 32);
        break;
    case 1001:
        blockSprite = (u8*)sprite55tiles_bin;
        break;
    case 1002:
        blockSprite = (u8*)sprite56tiles_bin;
        break;
    default:
        if (savefile->settings.skin < 0) {
            int n = savefile->settings.skin;
            n *= -1;
            n--;

            blockSprite = (u8*)&savefile->customSkins[n].board;
        }
        break;
    }

    if (savefile->settings.skin < 7 || savefile->settings.skin == 9 ||
        savefile->settings.skin == 10 || savefile->settings.skin > 1000) {
        // TODO:
        // vsync might not be necessary here
        vsync();
        int n = savefile->settings.skin;

        if (n < 0) {
            n *= -1;
            n--;
            buildMini(&savefile->customSkins[n].smallBoard);
        } else if (n == 1001 || n == 1002) {
            buildMini((TILE*)boneMini[n - 1001]);
        } else {
            if (n >= 9)
                n -= 2;
            buildMini((TILE*)mini[n]);
        }
    }

    loadTiles(0, 1, blockSprite, 1);
    loadTiles(2, 97, blockSprite, 1);

    int* board;
    for (int i = 0; i < 7; i++) {
        int n;
        if (game == nullptr)
            n = SRS;
        else
            n = game->rotationSystem;

        board = getShape(i, rotation, n);

        int iy = 16 * i;
        for (int j = 0; j < 4; j++) {
            for (int k = 0; k < 4; k++) {
                int ix = j * 4 + k;
                if (board[ix]) {
                    if (savefile->settings.skin == 11)
                        loadSpriteTilesPartial(
                            iy,
                            &sprite38tiles_bin[GameInfo::connectedConversion
                                                   [(board[ix]) >> 4] *
                                               32],
                            k, j, 1, 1, 4);
                    else if (savefile->settings.skin == 12)
                        loadSpriteTilesPartial(
                            iy,
                            &sprite39tiles_bin[GameInfo::connectedConversion
                                                   [(board[ix]) >> 4] *
                                               32],
                            k, j, 1, 1, 4);
                    else if (savefile->settings.skin == 13)
                        loadSpriteTilesPartial(
                            iy,
                            &sprite40tiles_bin[GameInfo::connectedConversion
                                                   [(board[ix]) >> 4] *
                                               32],
                            k, j, 1, 1, 4);
                    else if (savefile->settings.skin == 14)
                        loadSpriteTilesPartial(
                            iy,
                            &sprite53tiles_bin[GameInfo::connectedConversion
                                                   [(board[ix]) >> 4] *
                                               32],
                            k, j, 1, 1, 4);
                    else if (savefile->settings.skin < 7 ||
                             savefile->settings.skin > 8)
                        loadSpriteTilesPartial(iy, blockSprite, k, j, 1, 1, 4);
                    else
                        loadSpriteTilesPartial(
                            iy, classicTiles[savefile->settings.skin - 7][i], k,
                            j, 1, 1, 4);
                } else {
                    clearSpriteTile(iy, k, j, 4);
                }
            }
        }

        delete[] board;
    }
}

void setLightMode() {
    if (savefile->settings.lightMode) {
        setPaletteColor(0, 0, 0x5ad6, 1); // background gray
        setPaletteColor(8, 4, 0x0421, 1); // progressbar
        setPaletteColor(0, 8, 0x5ad6, 1);

        setPaletteColor(16 + 8, 6, 0x6739, 1); // frame background

        // faded Text Palette
        color_adj_brightness(13, 0, fontPalettes[1][0], 8, int2fx(1) >> 2);
        color_adj_brightness(13 + 16, 0, fontPalettes[1][0], 8, int2fx(1) >> 2);
    } else {
        setPaletteColor(0, 0, 0x0000, 1); // background gray
        setPaletteColor(8, 4, 0x7fff, 1); // progressbar
        setPaletteColor(0, 8, 0x0c63, 1);

        setPaletteColor(16 + 8, 6, 0x0c63, 1); // frame background

        // faded Text Palette
        color_fade_palette(13, 0, fontPalettes[0][0], 0, 8, 15);
        color_fade_palette(13 + 16, 0, fontPalettes[0][0], 0, 8, 15);
    }

    // main text
    loadPalette(15, 0, fontPalettes[savefile->settings.lightMode][1], 5);
    loadPalette(15 + 16, 0, fontPalettes[savefile->settings.lightMode][1], 5);

    // sub text
    loadPalette(14, 0, fontPalettes[savefile->settings.lightMode][0], 5);
    loadPalette(14 + 16, 0, fontPalettes[savefile->settings.lightMode][0], 5);

    setGradient(savefile->settings.backgroundGradient);
}

void diagnose() {
#ifdef NO_DIAGNOSE
    return;
#endif
}

void setPalette() {
    int n = (savefile->settings.colors < 2 && savefile->settings.colors >= 0)
                ? savefile->settings.colors
                : 0;

    for (int i = 0; i < 16; i++) {
        if (i < 8 && savefile->settings.colors < 2) {
            loadPalette(i, 0, &palette[n][i * 16], 8);
            loadPalette(i + 16, 0, &palette[n][i * 16], 8);

            if (i < 7 && savefile->settings.colors < 0) {
                int customIndex = -(savefile->settings.colors + 1);
                loadPalette(i, 1, savefile->customPalettes[customIndex][i], 3);
                loadPalette(i + 16, 1, savefile->customPalettes[customIndex][i],
                            3);
            }

        } else if (i < 8) {
            loadPalette(i, 4, &palette[n][i * 16 + 4], 4);
            loadPalette(i + 16, 4, &palette[n][i * 16 + 4], 4);
        } else if (i > 9) {
            loadPalette(i, 0, &palette[n][i * 16], 16);
            loadPalette(i + 16, 0, &palette[n][i * 16], 16);
        }
    }

    int color =
        savefile->settings.palette + 2 * (savefile->settings.palette > 6);

    if (savefile->settings.colors == 2) {
        for (int i = 0; i < 9; i++) {
            loadPalette(i, 0, classic_pal_bin, 4);
            loadPalette(i + 16, 0, classic_pal_bin, 4);
        }
    } else if (savefile->settings.colors == 3) {
        // set frame color
        loadPalette(8, 0, &palette[n][color * 16], 16);
        loadPalette(8 + 16, 0, &palette[n][color * 16], 16);

        int classic = getClassicPalette();

        for (int i = 0; i < 8; i++) {
            loadPalette(i, 1, &nesPalette[classic][0], 4);
            loadPalette(i + 16, 1, &nesPalette[classic][0], 4);
        }

    } else if (savefile->settings.colors == 4) {
        COLOR c;
        if (savefile->settings.palette < 7) {
            int* clr = (int*)GameInfo::colors[savefile->settings.palette];
            c = RGB15_SAFE(clr[0] >> 3, clr[1] >> 3, clr[2] >> 3);
        } else
            c = monoPalette[savefile->settings.lightMode][0];

        for (int i = 0; i < 9; i++) {
            setPaletteColor(i, 1, c, 4);
            setPaletteColor(i + 16, 1, c, 4);
        }

    } else if (savefile->settings.colors == 5) {
        for (int i = 0; i < 8; i++) {
            loadPalette(i, 1, &arsPalette[0][i], 4);
            loadPalette(i + 16, 1, &arsPalette[0][i], 4);
        }

        // set frame color
        loadPalette(8, 0, &palette[0][color * 16], 16);
        loadPalette(8 + 16, 0, &palette[0][color * 16], 16);
    } else if (savefile->settings.colors == 6) {
        for (int i = 0; i < 8; i++) {
            loadPalette(i, 1, &arsPalette[1][i], 4);
            loadPalette(i + 16, 1, &arsPalette[1][i], 4);
        }

        // set frame color
        loadPalette(8, 0, &palette[1][color * 16], 16);
        loadPalette(8 + 16, 0, &palette[1][color * 16], 16);
    } else {
        const int index = clamp(savefile->settings.colors, 0, 3);

        // set frame color
        loadPalette(8, 0, &palette[index][color * 16], 16);
        loadPalette(8 + 16, 0, &palette[index][color * 16], 16);

        // needs base case to fill all of the colors
        if (savefile->settings.colors < 0) {
            COLOR* src =
                savefile
                    ->customPalettes[-(savefile->settings.colors + 1)][color];

            // set frame color
            loadPalette(8, 1, src, 3);
            loadPalette(8 + 16, 1, src, 3);
        }
    }

    setLightMode();

    if (savefile->settings.effects == 2) {
        for (int i = 0; i < 16; i++) {
            int value = (savefile->settings.lightMode) ? 29 - i : i;
            setPaletteColor(i, 5, RGB15(value, value, value), 1);
        }
    }

    setPaletteColor(15, 5, RGB15(25, 5, 5), 1);

    for (int i = 0; i < 16; i++) {
        if (i == 12)
            continue;

        setPaletteColor(i, 15, RGB15(0, 31, 0), 1);
        setPaletteColor(16 + i, 15, RGB15(0, 31, 0), 1);
    }

    setPaletteColor(30, 14, RGB15(25, 27, 28), 1);
    setPaletteColor(31, 14, RGB15(31, 5, 5), 1);
}

int getClassicPalette() {
    int n = 0;

    if (game == nullptr) {
        n = abs(savefile->seed) % 10;
        return n;
    }

    int mode = game->gameMode;
    if ((mode == MARATHON || mode == BLITZ || mode == CLASSIC) &&
        game->level >= 0 && game->level < 255)
        n = (game->level - (game->gameMode != CLASSIC)) % 10;
    else
        n = abs(game->initSeed) % 10;

    return n;
}

void buildMini(TILE* customSkin) {
    for (int i = 0; i < 7; i++)
        clearSpriteTiles(9 * 16 + 8 * i, 4, 2);

    int add = 0;
    int rs = SRS;
    if (game != nullptr) {
        add = (game->rotationSystem == NRS || game->rotationSystem == ARS ||
               game->rotationSystem == BARS || game->rotationSystem == SDRS);
        rs = game->rotationSystem;
    }

    for (int i = 0; i < 7; i++) {
        int* p = getShape(i, 0, rs);
        int tileStart = 9 * 16 + i * 8;

        for (int y = 0; y < 2; y++) {
            for (int x = 0; x < 4; x++) {
                if (!p[(y + add) * 4 + x])
                    continue;

                for (int ii = 0; ii < 6; ii++) {
                    for (int jj = 0; jj < 6; jj++) {
                        setSpritePixel(
                            tileStart, (x * 6 + jj) / 8, (y * 6 + ii) / 8, 4,
                            ((x * 6 + jj) % 8), (y * 6 + ii) % 8,
                            (customSkin->data[ii] >> (4 * jj)) & 0xf);
                    }
                }
            }
        }

        delete[] p;
    }
}

Tuning getTuning(Save* save) {
    Tuning t = {
        t.das = save->settings.das,
        t.arr = save->settings.arr,
        t.sfr = save->settings.sfr,
        t.dropProtection = save->settings.dropProtectionFrames,
        t.directionalDas = save->settings.directionalDas,
        t.delaySoftDrop = save->settings.delaySoftDrop,
        t.ihs = save->settings.ihs,
        t.irs = save->settings.irs,
        t.initialType = save->settings.initialType,
    };

    return t;
}
Tuning getTuning() { return getTuning(savefile); }

void setGradient(int color) {
    stopDMA();

    int div = 20;

    int start = 0;
    int end = 160;

    COLOR src = 0;

#ifdef TE

    start = rowStart;
    end = rowEnd;

#endif

    if (savefile->settings.backgroundType == 0)
        div = (end - start) / 8;
    else if (savefile->settings.backgroundType == 1)
        div = (end - start) / 16;

    if (div == 0)
        div = 1;

    COLOR color1 = color & 0xffff;
    COLOR color2 = (color >> 16) & 0xffff;

    if (!(savefile->settings.lightMode && color == 0)) {
        for (int i = 0; i < end - start; i++) {
            if (savefile->settings.backgroundType == 0)
                color_fade((COLOR*)&gradientTable[i + start], &src, color, 1,
                           (((end - start) - 1 - i) / div) * 2 + 16);
            else if (savefile->settings.backgroundType == 1)
                color_fade((COLOR*)&gradientTable[i + start], &color2, color1,
                           1, (((end - start) - 1 - i) / div) * 2);
        }

#ifdef GBA
        memset16(&gradientTable[SCREEN_HEIGHT - 1], gradientTable[0],
                 192 - SCREEN_HEIGHT);
// gradientTable[SCREEN_HEIGHT-1] = gradientTable[0];
// gradientTable[192-1] = gradientTable[0];
#endif
    } else {
        for (int i = 0; i < end - start; i++)
            gradientTable[i + start] = 0x5ad6;
    }
}

void setDefaultGradient() {
#ifdef GBA
    memcpy16_fast(gradientTable, defaultGradient_bin,
                  defaultGradient_bin_size / 2);
#endif

#if defined(PC) || defined(N3DS)

    int div = SCREEN_HEIGHT / 8;
    int n = (savefile->settings.colors < 2) ? savefile->settings.colors : 0;

    COLOR* src = (COLOR*)&palette[n];

    for (int i = 0; i < SCREEN_HEIGHT; i++) {
        color_fade((COLOR*)&gradientTable[i], src, GRADIENT_COLOR, 1,
                   ((SCREEN_HEIGHT - 1 - i) / div) * 2 + 16);
    }

#endif
}

void gradient(int state) {
    if (state == 1) {
        gradientEnabled = true;
        bool light = savefile->settings.lightMode;
        if ((!light && gradientTable[0] == 0) ||
            (light && gradientTable[0] == 0x5ad6)) {
            setPaletteColor(0, 0, gradientTable[0], 1);
            return;
        }
        toggleHBlank(false);
    } else if (state == 2) {
        stopDMA();
        gradientEnabled = false;
        toggleHBlank(true);
    } else {
        stopDMA();
        gradientEnabled = false;
        toggleHBlank(false);
    }
}

void startGame(BlockEngine::Options options, int seed) {
    delete game;
    game = new Game(options.mode, seed, options.bigMode);
    game->setOptions(options);

    if (!demo)
        savefile->stats.gamesStarted++;

    delete previousGameOptions;
    previousGameOptions = new Options();

    *previousGameOptions = options;

    if (!replaying && enableBot) {
        log("starting bot!");

        delete testBot;
        testBot = new Bot(game);
    } else if (options.mode == BATTLE && !multiplayer) {
        startBotGame(seed);
    }

    if (options.mode == BlockEngine::ZEN) {
        if (game->subMode == 1) {
            game->score = savefile->boards.zenTower;
            game->towerScrollCount = savefile->boards.zenTower;
        }else {
            game->score = savefile->boards.zen;
        }
    }

    status.startGame();
    status.Options = previousGameOptions;
}

void startGame(int seed) {
    if (previousGameOptions == nullptr)
        previousGameOptions = new Options();

    startGame(*previousGameOptions, seed);
}

void startGame() {
    if (previousGameOptions == nullptr)
        previousGameOptions = new Options();

    startGame(*previousGameOptions, (int)randNext());
}

void startBotGame(int seed) {
    startMultiplayerGame(seed);
    multiplayer = false;

    delete botGame;

    botGame = new Game(BATTLE, seed & 0x1fff, false);
    botGame->setOptions(*previousGameOptions);
    botGame->setGoal(100);
    botGame->setLevel(1);
    botGame->maxClearDelay = 20;

    delete testBot;
    testBot = new Bot(botGame);

    clearText();

    status.startGame();
    status.Options = previousGameOptions;
}

void setPawnPalette(int dest, int n, int blend, bool flip) {
    dest += 16;

    if (savefile->settings.lightMode != flip) {
        if (savefile->settings.colors == 2) {
            color_fade_palette(dest, 0, (COLOR*)classic_pal_bin, 0x0000, 5,
                               blend);
        } else if (savefile->settings.colors == 3) {
            n = getClassicPalette();
            color_fade_palette(dest, 1, &nesPalette[n][0], 0x0000, 4, blend);
        } else if (savefile->settings.colors == 4) {
            COLOR c;
            if (savefile->settings.palette < 7) {
                int* clr = (int*)GameInfo::colors[savefile->settings.palette];
                c = RGB15_SAFE(clr[0] >> 3, clr[1] >> 3, clr[2] >> 3);
            } else {
                c = monoPalette[savefile->settings.lightMode][0];
            }
            COLOR mono[4];
            memset16(mono, c, 4);

            color_fade_palette(dest, 1, (COLOR*)&mono[0], 0x0000, 4, blend);
        } else if (savefile->settings.colors == 5) {
            color_fade_palette(dest, 1, (COLOR*)&arsPalette[0][n], 0x0000, 4,
                               blend);
        } else if (savefile->settings.colors == 6) {
            color_fade_palette(dest, 1, (COLOR*)&arsPalette[1][n], 0x0000, 4,
                               blend);
        } else {
            const int index =
                savefile->settings.colors >= 0 ? savefile->settings.colors : 0;

            color_fade_palette(dest, 0, (COLOR*)&palette[index][n * 16], 0x0000,
                               5, blend);
        }

        if (savefile->settings.colors < 0) {
            color_fade_palette(
                dest, 1,
                savefile->customPalettes[-(savefile->settings.colors + 1)][n],
                0x0000, 3, blend);
        }
    } else {
        if (dest == 11 + 16)
            blend = int2fx(blend) >> 5;
        else if (dest == 10 + 16)
            blend = float2fx(0.25);

        if (savefile->settings.colors == 2)
            color_adj_brightness(dest, 0, (COLOR*)classic_pal_bin, 5, blend);
        else if (savefile->settings.colors == 3) {
            color_adj_brightness(
                dest, 1, (COLOR*)&nesPalette[getClassicPalette()][0], 4, blend);
        } else if (savefile->settings.colors == 4) {
            COLOR c;
            if (savefile->settings.palette < 7) {
                int* clr = (int*)GameInfo::colors[savefile->settings.palette];
                c = RGB15_SAFE(clr[0] >> 3, clr[1] >> 3, clr[2] >> 3);
            } else {
                c = monoPalette[savefile->settings.lightMode][0];
            }
            COLOR mono[4];
            memset16(mono, c, 4);

            color_adj_brightness(dest, 1, (COLOR*)&mono[0], 4, blend);
        } else if (savefile->settings.colors == 5) {
            color_adj_brightness(dest, 1, (COLOR*)&arsPalette[0][n], 4, blend);
        } else if (savefile->settings.colors == 6) {
            color_adj_brightness(dest, 1, (COLOR*)&arsPalette[1][n], 4, blend);
        } else {
            const int index =
                savefile->settings.colors >= 0 ? savefile->settings.colors : 0;

            color_adj_brightness(dest, 0, (COLOR*)&palette[index][n * 16], 5,
                                 blend);
        }

        if (savefile->settings.colors < 0) {
            color_adj_brightness(
                dest, 1,
                savefile->customPalettes[-(savefile->settings.colors + 1)][n],
                3, blend);
        }
    }
}

std::string getStringFromKey(int key) {
#if defined(GBA) || defined(N3DS)

    std::string result;

    if ((key & (key - 1)) == 0) {
        result = keyToString[key];
    } else {

        u32 k = key;

        for (int i = 31; i >= 0; --i) {
            if (k & (1 << i)) {
                result += keyToString[1 << i] + " ";
                k &= ~(1 << i);
            }
        }

        // remove trailing space
        result.pop_back();
    }

    if (result != "") {
        return result;
    } else {
        return "?";
    }

#elif defined(SWITCH) || defined(MM) || defined(PORTMASTER)

    return keyToString[key];

#elif defined(WEB) || defined(PC)

    return stringFromKey(key);

#endif

    return "?";
}

void genWidths() {
    const int len = 96;

    int output[len][2];

    output[0][0] = 0;
    output[0][1] = 0;

    for (int i = 0; i < len; i++) {
        TILE* t = (TILE*)&font1tiles_bin[i * 32];

        int found = 0;
        for (int x = 7; x >= 0; x--) {
            for (int y = 0; y < 8; y++) {
                if ((t->data[y] >> (4 * x)) & 0xf) {
                    output[i][1] = x;
                    found = 1;
                }

                if (found)
                    break;
            }
            if (found)
                break;
        }
    }

    for (int i = 0; i < len; i++) {
        TILE* t = (TILE*)&font1tiles_bin[i * 32];

        int found = 0;
        for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 8; y++) {
                if ((t->data[y] >> (4 * x)) & 0xf) {
                    output[i][0] = x;
                    found = 1;
                }

                if (found)
                    break;
            }
            if (found)
                break;
        }
    }

    std::string str;
    for (int i = 0; i < len; i++) {
        output[i][1] -= output[i][0] - 1;

        str += "{" + std::to_string(output[i][0]) + ", " +
               std::to_string(output[i][1]) + "}, ";

        if (((i + 1) % 8) == 0) {
            log(str);
            str = "";
        }
    }
}
COLOR hsv2rgb(HSV_COLOR hsv) {
    return hsv2rgb(hsv.hue, hsv.saturation, hsv.value);
}

COLOR hsv2rgb(int hue, float saturation, float value) {
    if (saturation == 0.0f) { // Achromatic (gray)
        const int gray = (int)(value * 31);

        return RGB15_SAFE(gray, gray, gray);
    }

    hue = hue % 360;
    float h_prime = hue / 60.0f;
    int i = (int)(h_prime);
    float f = h_prime - i; // fractional part of h_prime
    float p = value * (1 - saturation);
    float q = value * (1 - saturation * f);
    float t = value * (1 - saturation * (1 - f));

    float r = 0, g = 0, b = 0;

    switch (i) {
    case 0:
        r = value;
        g = t;
        b = p;
        break;
    case 1:
        r = q;
        g = value;
        b = p;
        break;
    case 2:
        r = p;
        g = value;
        b = t;
        break;
    case 3:
        r = p;
        g = q;
        b = value;
        break;
    case 4:
        r = t;
        g = p;
        b = value;
        break;
    case 5:
        r = value;
        g = p;
        b = q;
        break;
    }

    return RGB15_SAFE((int)(r * 31), (int)(g * 31), (int)(b * 31));
}

HSV_COLOR rgb2hsv(int r, int g, int b) {
    // Normalize RGB values to the range [0, 1]
    float r_norm = r / 31.0f;
    float g_norm = g / 31.0f;
    float b_norm = b / 31.0f;

    float c_max = std::max({r_norm, g_norm, b_norm});
    float c_min = std::min({r_norm, g_norm, b_norm});
    float delta = c_max - c_min;

    float h = 0, s = 0, v = c_max;

    if (delta == 0) {
        h = 0; // Hue is undefined for achromatic colors (black, white, grays)
    } else {
        if (c_max == r_norm) {
            h = 60 * (fmod(((g_norm - b_norm) / delta), 6));
        } else if (c_max == g_norm) {
            h = 60 * (((b_norm - r_norm) / delta) + 2);
        } else if (c_max == b_norm) {
            h = 60 * (((r_norm - g_norm) / delta) + 4);
        }
    }

    if (c_max == 0) {
        s = 0; // Saturation is 0 when color is black
    } else {
        s = delta / c_max;
    }

    // Ensure hue is non-negative
    if (h < 0) {
        h += 360;
    }

    return HSV_COLOR{(int)h, s, v};
}
