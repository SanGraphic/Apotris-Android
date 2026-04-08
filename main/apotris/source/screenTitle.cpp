#include "blockEngine.hpp"
#include "def.h"
#include "flashSaves.h"
#include "menu.h"
#include "rumblePatterns.hpp"
#include "sceneColorEditor.hpp"
#include "skinEditor.hpp"
#include "sprite41tiles_bin.h"
#include "tetromino.hpp"
#include "text.h"
#include <list>
#include <string>

#include "classic_pal_bin.h"
#include "logging.h"
#include "scene.hpp"
#include "sprites.h"
#include <posprintf.h>

#include "sceneAudio.hpp"
#include "sceneControls.hpp"
#include "sceneGraphics.hpp"
#include "sceneHandling.hpp"
#include "sceneModes.hpp"
#include "sceneStats.hpp"
#ifdef GBA
#include "detectEmulators.h"
#endif

std::string timeToStringHours(int frames);
void initFallingBlocks();
void checkCustomSkins();

WordSprite* wordSprites[MAX_WORD_SPRITES];

int titleFloat = 0;
OBJ_ATTR* titleSprites[2];

#ifdef GBA
const int bgGridHeight = 30;
#endif
#ifndef GBA
const int bgGridHeight = 42;
#endif
u16 backgroundArray[bgGridHeight][30];

int bgSpawnBlock = 0;
int bgSpawnBlockMax = 30;
int gravity = 0;
int gravityMax = 10;
int previousOptionScreen = -1;
bool goToOptions = false;
int previousSelection = 0;
int previousOptionMax = 0;

// std::list<int> keyHistory;

Skin previousSkins[MAX_CUSTOM_SKINS];
std::unique_ptr<Settings> previousSettings = nullptr;

bool bigMode = false;

const std::list<std::string> menuOptions = {
    "Play",        "Settings",     "Stats",  "Links",
    "Skin Editor", "Color Editor", "Credits"};
const std::list<std::string> gameOptions = {
    "Marathon", "Sprint",    "Dig",        "Ultra",   "Blitz",
    "Combo",    "Survival",  "Classic",    "Master",  "Death",
    "Zen",      "2P Battle", "CPU Battle", "Training"};
const std::list<std::string> settingsOptions = {
    "Graphics", "Audio", "Controls", "Handling", "Gameplay", "Saving"};

// const int secretCombo[11] =
// {KEY_UP,KEY_UP,KEY_DOWN,KEY_DOWN,KEY_LEFT,KEY_RIGHT,KEY_LEFT,KEY_RIGHT,KEY_B,KEY_A,KEY_START};

bool proMode = false;

bool demo = false;

std::string previousElement;

std::list<std::string> path;

MenuKeys menuKeys;

void showTitleSprites() {
    for (int i = 0; i < 2; i++)
        titleSprites[i] = &obj_buffer[14 + i];
    for (int i = 0; i < 2; i++) {
        sprite_unhide(titleSprites[i], 0);
        sprite_set_attr(titleSprites[i], ShapeWide, 3, 512 + 64 + i * 32, 12,
                        0);
        sprite_enable_affine(titleSprites[i], i, true);
        sprite_set_size(titleSprites[i], float2fx(0.5), i);

        int offset = (sinLut(titleFloat) * 3) >> 12;

        if (offset == 3)
            offset = 2;

        sprite_set_pos(titleSprites[i], 120 - 64 + 128 * i - 64, 24 + offset);
    }

    titleFloat += 3;
    if (titleFloat >= 512)
        titleFloat = 0;
}

void fallingBlocks() {
    gravity++;
    bgSpawnBlock++;

    int i, j;
    if (gravity > gravityMax) {
        gravity = 0;

        for (i = bgGridHeight - 1; i >= 0; i--) {
            for (j = 0; j < 30; j++) {
                if (i == 0)
                    backgroundArray[i][j] = 0;
                else
                    backgroundArray[i][j] = backgroundArray[i - 1][j];
            }
        }
    }

    bool bigMode = savefile->settings.big;

    int offset = 4 + 6 * bigMode;

    for (i = 0; i < (bgGridHeight - 10); i++) {
        for (j = 0; j < 30; j++) {
            if (backgroundArray[i + offset][j]) {
                int n = (backgroundArray[i + offset][j] - 1) & 0xf;
                int r = backgroundArray[i + offset][j] >> 4;

                if (savefile->settings.skin >= 11)
                    setTile(25, j, i,
                            tileBuild(128 + GameInfo::connectedConversion[r],
                                      false, false, n));
                // *dest++ = 128 + GameInfo::connectedConversion[r] + ((n) <<
                // 12);
                else if (savefile->settings.skin < 7 ||
                         savefile->settings.skin > 8)
                    setTile(25, j, i, tileBuild(1, false, false, n));
                // *dest++ = (1 + ((n) << 12));
                else {
                    setTile(25, j, i, tileBuild(48, false, false, n));
                    // *dest++ = (48 + n + ((n) << 12));
                }
            } else {
                setTile(25, j, i, 0);
                // *dest++ = 0;
            }
        }
        // dest += 2;
    }

    if (bgSpawnBlock > bgSpawnBlockMax) {

        int n = (int)(randNext() % 7);
        int* p =
            BlockEngine::getShape(n, (int)(randNext() % 4), BlockEngine::SRS);

        bool found = false;

        if (!bigMode) {
            int x = (int)(randNext() % 27);
            // int x = (int)(randNext() % 7);
            // x += 20 * (int)(randNext() % 2);

            for (i = 0; i < 4; i++)
                for (j = 0; j < 4; j++)
                    if (backgroundArray[i][j + x])
                        found = true;

            if (!found)
                for (i = 0; i < 4; i++)
                    for (j = 0; j < 4; j++)
                        if (p[i * 4 + j])
                            backgroundArray[i][j + x] = n + p[i * 4 + j];
        } else {
            u32 x = (randNext() % 13) * 2;

            for (i = 0; i < 8; i++)
                for (j = 0; j < 8; j++)
                    if (j + x * 2 >= 30 || backgroundArray[i][j + x * 2])
                        found = true;

            if (!found) {
                for (i = 0; i < 4; i++) {
                    for (j = 0; j < 4; j++) {
                        int xoffset = (j + x) * 2;
                        int yoffset = i * 2;

                        if (!p[i * 4 + j] || yoffset < 0 || yoffset > 23 ||
                            xoffset < 0 || xoffset > 29)
                            continue;

                        backgroundArray[yoffset][xoffset] = n + 1;
                        backgroundArray[yoffset][xoffset + 1] = n + 1;
                        backgroundArray[yoffset + 1][xoffset] = n + 1;
                        backgroundArray[yoffset + 1][xoffset + 1] = n + 1;
                    }
                }
            }
        }

        delete[] p;

        bgSpawnBlock = 0;
    }
}

std::string timeToStringHours(int frames) {
    int t = (int)frames * 0.0167f;
    int minutes = (t / 60) % 60;
    int hours = t / 3600;

    char res[30];

    posprintf(res, "%02d:%02d", hours, minutes);

    std::string result = "";

    result = res;

    return result;
}

bool settingsChanged() {
    Settings temp = savefile->settings;

    temp.menuKeys = menuKeys;

    int* v1 = (int*)&temp;
    int* v2 = (int*)&*previousSettings;

    for (int i = 0; i < (int)sizeof(Settings) / 4; i++) {
        if (v1[i] != v2[i])
            return true;
    }

    return false;
}

void TitleScene::init() {
    reset();
    toggleBG(3, false);

    loadSpriteTiles(512 + 64, title1tiles_bin, 8, 4);
    loadSpriteTiles(512 + 96, title2tiles_bin, 8, 4);

    loadPalette(12 + 16, 0, title_pal_bin, title_pal_bin_size / 2);
    loadTiles(2, 102, sprite37tiles_bin, sprite37tiles_bin_size / 32);
    loadTiles(2, 105, sprite41tiles_bin, sprite41tiles_bin_size / 32);

    setSkin();
    setLightMode();

    clearSprites(128);

    playSongRandom(0);

    // backgroundGrid
    setTiles(26, 0, 32 * 32,
             tileBuild(35 * (!savefile->settings.lightMode), false, false, 0));

    clearSpriteTiles(256, 12 * MAX_WORD_SPRITES, 1);
    for (int i = 0; i < MAX_WORD_SPRITES; i++)
        wordSprites[i].setup(i, 64 + i * 3, 256 + i * 12, false);

    wordSprites[0].setText("Press Any");
    wordSprites[1].setText("Button");

    versionText.setText("v" APOTRIS_VERSION);

    nameText.setText("akouzoukos");

    enableBlend((0b000000 << 8) + (1 << 6) + (1 << 3));
}

void TitleScene::draw() {
    fallingBlocks();
#ifdef N3DS
    // Scroll falling blocks to top of screen when in 1x scale:
    setLayerScroll(2, 0, 40);
#endif
    showTitleSprites();
    showSprites(128);

    const int y = 160 - (1) * 8;
    versionText.show(0, y, 15);
    nameText.show(240 - nameText.width, y, 15);

    if (flashTimer < flashMax / 2) {
        int w = wordSprites[0].width + wordSprites[1].width + 6;

        wordSprites[0].show((240 - w) / 2, 14 * 8, 15);
        wordSprites[1].show((240 - w) / 2 + wordSprites[0].width + 6, 14 * 8,
                            15);
    } else {
        wordSprites[0].hide();
        wordSprites[1].hide();
    }

    if (++flashTimer > flashMax - 1) {
        flashTimer = 0;
    }
}

bool TitleScene::control() {
    if (key_hit(KEY_FULL)) {
        sfx(SFX_MENUCONFIRM);
        rumblePattern(RUMBLE_PLACE);
        changeScene(new MainMenuScene(), Transitions::FADE);
        return true;
    }

    return false;
}

void TitleScene::update() {
    canDraw = 1;
    key_poll();

    if (control()) {
        return;
    }

    demoTimer++;
    if (demoTimer >= demoMax) {
        demoTimer = 0;
        enableBot = true;
        demo = true;

        botThinkingSpeed = 6;
        botSleepDuration = 5;
        botStepMax = 1;

        startGame(BlockEngine::Options(), (int)randNext());

        changeScene(new GameScene(), Transitions::FADE);
    }
}

void TitleScene::deinit() {
    clearSprites(128);
    showSprites(128);

    clearTilemap(25);
    clearTilemap(26);
    clearTilemap(27);
    clearSpriteTiles(2, 100, 1);
    clearSpriteTiles(256, 256, 1);

    clearText();

#ifdef N3DS
    // Reset scroll for falling blocks:
    setLayerScroll(2, 0, 0);
#endif
}

Scene* MainMenuScene::previousScene() { return new TitleScene(); }

void ConfirmSaveScene::init() {
    clearSprites(128);
    showSprites(128);

    clearSpriteTiles(256, 12 * 2, 1);
    for (int i = 0; i < 2; i++)
        wordSprites[i].setup(i, 64 + i * 3, 256 + i * 12, false);

    clearTilemap(29);
    naprint("Save changes?", 64, 56);

    wordSprites[0].setText("SAVE");
    wordSprites[0].show(48, 104, 14);

    wordSprites[1].setText("DON'T SAVE");
    wordSprites[1].show(120, 104, 14);

    // backgroundGrid
    setTiles(26, 0, 32 * 32,
             tileBuild(35 * (!savefile->settings.lightMode), false, false, 0));

    setTiles(27, 0, 32 * 32, tileBuild(34, false, false, 0));

    lengths[0] = wordSprites[0].width;
    lengths[1] = wordSprites[1].width;

    pos[0] = 48 + lengths[0] / 2;
    pos[1] = 120 + lengths[1] / 2;

    loadSpriteTiles(16 * 7, blockSprite, 1, 1);
    for (int i = 0; i < 2; i++) {
        cursorSprites[i] = &obj_buffer[1 + i];
        sprite_set_attr(cursorSprites[i], ShapeSquare, 0, 7 * 16, 5, 1);
        sprite_enable_affine(cursorSprites[i], i, true);
        sprite_hide(cursorSprites[i]);
    }

    enableBlend((0b101110 << 8) + (1 << 6) + (1 << 3));
}

void ConfirmSaveScene::draw() {
    fallingBlocks();
#ifdef N3DS
    // Scroll falling blocks to top of screen when in 1x scale:
    setLayerScroll(2, 0, 40);
#endif

    int offset = (sinLut(cursorFloat) * 2) >> 12;
    FIXED scale = float2fx((1.0 - ((float)0.1 * offset)));

    const int y = 104;

    for (int i = 0; i < 2; i++) {
        sprite_unhide(cursorSprites[i], 0);
        sprite_enable_affine(cursorSprites[i], i, true);
        sprite_set_size(cursorSprites[i], scale, i);

        int x = pos[cancel] -
                ((lengths[cancel] + 8) / 2 + offset + 4) * ((i) ? -1 : 1) - 8;

        sprite_set_pos(cursorSprites[i], x, y - 5);
    }

    wordSprites[0].show(48, 104, 14 + (cancel == 0));
    wordSprites[1].show(120, 104, 14 + (cancel == 1));

    showSprites(128);
}

void ConfirmSaveScene::update() {
    canDraw = 1;
    key_poll();

    cursorFloat += 6;
    if (cursorFloat >= 512)
        cursorFloat = 0;

    control();
}

bool ConfirmSaveScene::control() {
    MenuKeys k = savefile->settings.menuKeys;

    if (key_hit(k.left) || key_hit(k.right)) {
        cancel = !cancel;
        sfx(SFX_MENUMOVE);
    }

    if (key_hit(k.confirm)) {
        sfx(SFX_MENUCONFIRM);

        if (cancel) {

            switch (type) {
            case 0:
                if (previousSettings != nullptr)
                    savefile->settings = *previousSettings;
                break;
            case 1:
                for (int i = 0; i < MAX_CUSTOM_SKINS; i++)
                    savefile->customSkins[i] = previousSkins[i];
                break;
            case 2:
                if (previousSettings != nullptr)
                    savefile->settings = *previousSettings;

                if (previousPalette != nullptr)
                    memcpy16(savefile->customPalettes, previousPalette,
                             3 * 7 * 3);
                break;
            }

            setMusicVolume(512 * ((float)savefile->settings.volume / 10));
            setSkin();
            setPalette();
            setGradient(savefile->settings.backgroundGradient);
        } else {
            savefile->settings.menuKeys = menuKeys;

            checkCustomSkins();

            saveSavefile();
        }

#ifdef PC
        setFullscreen(savefile->settings.fullscreen);
#endif

#ifdef GBA
        sleep_combo = get_sleep_combo(savefile->settings.sleep.sleepWakeCombo);
        initRumbleCart();
#endif

        changeScene(new MainMenuScene(), Transitions::FADE);

        return true;
    } else if (key_hit(k.cancel)) {
        sfx(SFX_MENUCANCEL);

        switch (type) {
        case 0:
            changeScene(new SettingsScene(), Transitions::FADE);
            break;
        case 1:
            changeScene(new EditorScene(), Transitions::FADE);
            break;
        case 2:
            changeScene(new ColorSelectorScene(), Transitions::FADE);
            break;
        }

        return true;
    }

    return false;
}

void ConfirmSaveScene::deinit() {
#ifdef N3DS
    // Reset scroll for falling blocks:
    setLayerScroll(2, 0, 0);
#endif
}

void ConfirmEmuScene::init() {
    clearSprites(128);
    showSprites(128);

    clearSpriteTiles(256, 12 * 2, 1);
    for (int i = 0; i < 2; i++)
        wordSprites[i].setup(i, 64 + i * 3, 256 + i * 12, false);

    clearTilemap(29);
    naprint("Are you on an emulator?", 40, 56);

    wordSprites[0].setText("I AM");
    wordSprites[0].show(48, 104, 14);

    wordSprites[1].setText("I AM NOT");
    wordSprites[1].show(120, 104, 14);

    // backgroundGrid
    setTiles(26, 0, 32 * 32,
             tileBuild(35 * (!savefile->settings.lightMode), false, false, 0));

    setTiles(27, 0, 32 * 32, tileBuild(34, false, false, 0));

    lengths[0] = wordSprites[0].width;
    lengths[1] = wordSprites[1].width;

    pos[0] = 48 + lengths[0] / 2;
    pos[1] = 120 + lengths[1] / 2;

    loadSpriteTiles(16 * 7, blockSprite, 1, 1);
    for (int i = 0; i < 2; i++) {
        cursorSprites[i] = &obj_buffer[1 + i];
        sprite_set_attr(cursorSprites[i], ShapeSquare, 0, 7 * 16, 5, 1);
        sprite_enable_affine(cursorSprites[i], i, true);
        sprite_hide(cursorSprites[i]);
    }

    enableBlend((0b101110 << 8) + (1 << 6) + (1 << 3));
}

bool ConfirmEmuScene::control() {
    MenuKeys k = savefile->settings.menuKeys;

    if (key_hit(k.left) || key_hit(k.right)) {
        cancel = !cancel;
        sfx(SFX_MENUMOVE);
    }

    if (key_hit(k.confirm)) {
        sfx(SFX_MENUCONFIRM);
#ifdef GBA
        if (!cancel) {
            inaccurateEmulator = detect_inaccurate_emulator();
        }
        setUpLinkUniversal(!cancel);
        emulatorPrompted = true;
#endif
        changeScene(new MultBattleScene(), Transitions::FADE);
    } else if (key_hit(k.cancel)) {
        sfx(SFX_MENUCANCEL);
        changeScene(new MainMenuScene(), Transitions::FADE);
    }

    return false;
}

void ConfirmEmuScene::draw() {
    fallingBlocks();
#ifdef N3DS
    // Scroll falling blocks to top of screen when in 1x scale:
    setLayerScroll(2, 0, 40);
#endif

    int offset = (sinLut(cursorFloat) * 2) >> 12;
    FIXED scale = float2fx((1.0 - ((float)0.1 * offset)));

    const int y = 104;

    for (int i = 0; i < 2; i++) {
        sprite_unhide(cursorSprites[i], 0);
        sprite_enable_affine(cursorSprites[i], i, true);
        sprite_set_size(cursorSprites[i], scale, i);

        int x = pos[cancel] -
                ((lengths[cancel] + 8) / 2 + offset + 4) * ((i) ? -1 : 1) - 8;

        sprite_set_pos(cursorSprites[i], x, y - 5);
    }

    wordSprites[0].show(48, 104, 14 + (cancel == 0));
    wordSprites[1].show(120, 104, 14 + (cancel == 1));

    showSprites(128);
}

void ConfirmEmuScene::update() {
    canDraw = true;
    key_poll();

    cursorFloat += 6;
    if (cursorFloat >= 512)
        cursorFloat = 0;

    control();
}

void ConfirmEmuScene::deinit() {
    clearText();
    clearSmallText();
    resetSmallText();
}

void MainMenuScene::init() {
    path.clear();
    SimpleListScene::init();

#if defined(PC) || defined(WEB)
    if (lastInputType == InputType::CONTROLLER)
        return;

    int offset = 0;
    if (savefile->settings.aspectRatio == 1) {
        offset = (240 - 214) / 2;
    }

    setSmallTextArea(110, 0, 17, 30, 20);
    clearText();

    MenuKeys m = savefile->settings.menuKeys;

    std::string move =
        "Move: " + getStringFromKey(m.up) + " " + getStringFromKey(m.left) +
        " " + getStringFromKey(m.down) + " " + getStringFromKey(m.right) + " ";

    std::string confirm = "Confirm: " + getStringFromKey(m.confirm);

    std::string cancel = "Cancel: " + getStringFromKey(m.cancel);

    aprints(move, 1 + offset, 18, 2);
    aprints(confirm, 30 * 8 - confirm.size() * 4 - offset, 11, 2);
    aprints(cancel, 30 * 8 - cancel.size() * 4 - offset, 18, 2);

#endif

#ifdef GBA
    if (!gbpChecked && savefile->settings.gameBoyPlayerSupport) {
        naprint("Reboot to enable GBP Rumble!", 4, 4);
    }
#endif
}

void MainMenuScene::deinit() {
    SimpleListScene::deinit();

    resetSmallText();
    clearText();
}

void initFallingBlocks() { memset16(backgroundArray, 0, 30 * (bgGridHeight)); }

void checkCustomSkins() {
    for (int i = 0; i < MAX_CUSTOM_SKINS; i++) {
        bool changed = false;

        int* skin = (int*)&savefile->customSkins[i].board;
        int* defaultSkin = (int*)sprite1tiles_bin;

        int sum = 0;

        for (int count = (sizeof(TILE)) / 4; count; count--) {
            sum += *skin;
            if (*skin != *defaultSkin) {
                changed = true;
            }

            skin++;
            defaultSkin++;
        }

        skin = (int*)&savefile->customSkins[i].smallBoard;
        defaultSkin = (int*)mini[0];

        for (int count = (sizeof(TILE)) / 4; count; count--) {
            sum += *skin;
            if (*skin != *defaultSkin) {
                changed = true;
            }

            skin++;
            defaultSkin++;
        }

        // only set to changed if the skin is non-empty
        savefile->customSkins[i].changed = changed && sum;
    }
}
