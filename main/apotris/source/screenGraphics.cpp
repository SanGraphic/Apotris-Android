#include "def.h"
#include "logging.h"
#include "platform.hpp"
#include "scene.hpp"
#include "sceneGraphics.hpp"
#include "sprites.h"
#include "text.h"
#include <posprintf.h>
#include <string>

using namespace BlockEngine;

static void screenShake();
void setClearEffect();

void GradientEditorScene::init() {
    toggleBG(0, true);
    toggleBG(1, false);
    toggleBG(2, false);
    toggleBG(3, false);

    for (int i = 0; i < 3; i++) {
        color1[i] = (savefile->settings.backgroundGradient >> (5 * i)) & 0x1f;
    }

    for (int i = 0; i < 3; i++) {
        color2[i] =
            (savefile->settings.backgroundGradient >> (5 * i + 16)) & 0x1f;
    }

    clearSpriteTiles(256, 12 * MAX_WORD_SPRITES, 1);
    for (int i = 0; i < MAX_WORD_SPRITES; i++)
        wordSprites[i] = new WordSprite(i, 32 + i * 3, 256 + i * 12);

    resetSmallText();

    setSmallTextArea(256, 0, 18, 30, 20);

    drawText();

    setMaxSelection();

    loadSpriteTiles(254, &sprite48tiles_bin, 1, 1);

    loadSpriteTiles(255, blockSprite, 1, 1);
    cursorSprite = &obj_buffer[14];
    sprite_set_attr(cursorSprite, ShapeSquare, 0, 255, 5, 0);
    sprite_enable_affine(cursorSprite, 10, true);
    sprite_hide(cursorSprite);

    for (int i = 0; i < 6; i++) {
        colorSprites[i] = &obj_buffer[15 + i];

        sprite_set_attr(colorSprites[i], ShapeSquare, 0, 254, i % 3, 0);
        sprite_hide(colorSprites[i]);
    }

    // color sprite colors
    setPaletteColor(16 + 0, 15, RGB15(31, 0, 0), 1);
    setPaletteColor(16 + 1, 15, RGB15(0, 31, 0), 1);
    setPaletteColor(16 + 2, 15, RGB15(5, 5, 31), 1);
}

void GradientEditorScene::draw() {

    const int space = 4 + hsvMode;
    const int height = 11;

    c = 0;

    const int tile = 105;

    if (savefile->settings.backgroundType == 0) {
        for (int i = 0; i < 3; i++) {
            int x = 19 + (i - 1) * space;

            char buff[8];

            if (hsvMode) {
                switch (i) {
                case 0:
                    posprintf(buff, "H%03d", hsv1.hue);
                    break;
                case 1:
                    posprintf(buff, "S%03d", (int)(hsv1.saturation * 100));
                    break;
                case 2:
                    posprintf(buff, "V%03d", (int)(hsv1.value * 100));
                    break;
                }

            } else {
                posprintf(buff, "%02d", color1[i]);
            }

            aprintColor(buff, x - hsvMode, height, !onValues);

            if (i == colorSelection && onValues) {
                setTile(29, x, height - 1, tileBuild(tile, false, false, 15));
                setTile(29, x + 1, height - 1,
                        tileBuild(tile + 1, false, false, 15));
                setTile(29, x, height + 1, tileBuild(tile, false, true, 15));
                setTile(29, x + 1, height + 1,
                        tileBuild(tile + 1, false, true, 15));
            } else {
                aprint("  ", x, height - 1);
                aprint("  ", x, height + 1);
            }

            if (!hsvMode) {
                sprite_unhide(colorSprites[i], 0);
                c += color1[i] << (5 * i);
            } else {
                sprite_hide(colorSprites[i]);
            }
            sprite_set_pos(colorSprites[i], x * 8 - 9, height * 8);
        }

        if (hsvMode) {
            c = hsv2rgb(hsv1);
        }

        for (int i = 0; i < 3; i++)
            sprite_hide(colorSprites[i + 3]);

    } else if (savefile->settings.backgroundType == 1) {
        int height = 8;

        for (int i = 0; i < 3; i++) {
            int x = 19 + (i - 1) * space;

            char buff[8];
            if (hsvMode) {
                switch (i) {
                case 0:
                    posprintf(buff, "H%03d", hsv1.hue);
                    break;
                case 1:
                    posprintf(buff, "S%03d", (int)(hsv1.saturation * 100));
                    break;
                case 2:
                    posprintf(buff, "V%03d", (int)(hsv1.value * 100));
                    break;
                }

            } else {
                posprintf(buff, "%02d", color1[i]);
            }

            aprintColor(buff, x - hsvMode, height,
                        !(selection == 1 && onValues));

            if (i == colorSelection && onValues && selection == 1) {
                setTile(29, x, height - 1, tileBuild(tile, false, false, 15));
                setTile(29, x + 1, height - 1,
                        tileBuild(tile + 1, false, false, 15));
                setTile(29, x, height + 1, tileBuild(tile, false, true, 15));
                setTile(29, x + 1, height + 1,
                        tileBuild(tile + 1, false, true, 15));
            } else {
                aprint("  ", x, height - 1);
                aprint("  ", x, height + 1);
            }

            if (!hsvMode) {
                c += color1[i] << (5 * i);
                sprite_unhide(colorSprites[i], 0);
            } else {
                sprite_hide(colorSprites[i]);
            }

            sprite_set_pos(colorSprites[i], x * 8 - 9, height * 8);
        }

        height = 13;

        for (int i = 0; i < 3; i++) {
            int x = 19 + (i - 1) * space;

            char buff[8];
            if (hsvMode) {
                switch (i) {
                case 0:
                    posprintf(buff, "H%03d", hsv2.hue);
                    break;
                case 1:
                    posprintf(buff, "S%03d", (int)(hsv2.saturation * 100));
                    break;
                case 2:
                    posprintf(buff, "V%03d", (int)(hsv2.value * 100));
                    break;
                }

            } else {
                posprintf(buff, "%02d", color2[i]);
            }

            aprintColor(buff, x - hsvMode, height,
                        !(selection == 2 && onValues));

            if (i == colorSelection && onValues && selection == 2) {
                setTile(29, x, height - 1, tileBuild(tile, false, false, 15));
                setTile(29, x + 1, height - 1,
                        tileBuild(tile + 1, false, false, 15));
                setTile(29, x, height + 1, tileBuild(tile, false, true, 15));
                setTile(29, x + 1, height + 1,
                        tileBuild(tile + 1, false, true, 15));
            } else {
                aprint("  ", x, height - 1);
                aprint("  ", x, height + 1);
            }

            if (!hsvMode) {
                c += color2[i] << (5 * i + 16);
                sprite_unhide(colorSprites[i + 3], 0);
            } else {
                sprite_hide(colorSprites[i + 3]);
            }
            sprite_set_pos(colorSprites[i + 3], x * 8 - 9, height * 8);
        }

        if (hsvMode) {
            c = hsv2rgb(hsv1) + (hsv2rgb(hsv2) << 16);
        }
    }

    std::string temp;

    switch (savefile->settings.backgroundType) {
    case 0:
        temp = "Single Color";
        break;
    case 1:
        temp = "Dual Color";
        break;
    default:
        temp = "Single Color";
        break;
    }

    std::string typeString;
    if (selection == 0) {
        if (savefile->settings.backgroundType > 0)
            typeString += "<";
        else
            typeString += " ";

        typeString += temp;

        if (savefile->settings.backgroundType < 1)
            typeString += ">";
        else
            typeString += " ";
    } else {
        typeString += " " + temp + " ";
    }

    wordSprites[0]->setText(typeString);
    wordSprites[0]->show(25 * 8 - wordSprites[0]->width, 3 * 8, 15);

    showSprites(128);

    int offset = (sinLut(cursorFloat) * 2) >> 12;
    FIXED scale = float2fx((1.0 - ((float)0.1 * offset)));

    sprite_unhide(cursorSprite, 0);
    sprite_enable_affine(cursorSprite, 10, true);
    sprite_set_size(cursorSprite, scale, 10);

    if (selection == 0) {

        int x = 3 * 8 - ((8) + offset + 4) - 4;

        sprite_set_pos(cursorSprite, x, 3 * 8 - 5);
    } else {
        int x = 3 * 8 - ((8) + offset + 4) - 4;

        if (savefile->settings.backgroundType == 0) {
            sprite_set_pos(cursorSprite, x, 11 * 8 - 5);
        } else if (savefile->settings.backgroundType == 1) {
            if (selection == 1) {
                sprite_set_pos(cursorSprite, x, 8 * 8 - 5);
            } else if (selection == 2) {
                sprite_set_pos(cursorSprite, x, 13 * 8 - 5);
            }
        }
    }
}

void GradientEditorScene::drawText() {
    clearText();

    int offset = 0;
    if (savefile->settings.aspectRatio == 1) {
        offset = (240 - 214) / 2;
    }

    std::string keyString =
        getStringFromKey(savefile->settings.menuKeys.cancel);

    std::transform(keyString.begin(), keyString.end(), keyString.begin(),
                   ::toupper);

    std::string s = "Press " + keyString + " to Exit";
    aprints(s, 30 * 8 - 4 * s.size() - offset, 10, 2);

    std::string colorModeString =
        getStringFromKey(savefile->settings.menuKeys.special1);

    std::transform(colorModeString.begin(), colorModeString.end(),
                   colorModeString.begin(), ::toupper);

    std::string colorMode =
        "Press " + colorModeString + " for " + ((!hsvMode) ? "HSV" : "RGB");
    aprints(colorMode, 30 * 8 - 4 * colorMode.size() - offset, 0, 2);

    naprint("Type:", 3 * 8, 3 * 8);

    if (savefile->settings.backgroundType == 0) {
        naprint("Color:", 3 * 8, 11 * 8);
    } else if (savefile->settings.backgroundType == 1) {
        naprint("Top:", 3 * 8, 8 * 8);
        naprint("Bottom:", 3 * 8, 13 * 8);
    }

    aprints("Randomize: " +
                getStringFromKey(savefile->settings.menuKeys.special3),
            1 + offset, 10, 2);
}

void GradientEditorScene::update() {
    canDraw = 1;
    key_poll();

    setGradient(c);
    gradient(2);

    cursorFloat += 6;
    if (cursorFloat >= 512)
        cursorFloat = 0;

    control();
}

bool GradientEditorScene::control() {
    MenuKeys k = savefile->settings.menuKeys;

    if (key_hit(k.special3)) {
        sfx(SFX_MENUCONFIRM);

        for (int i = 0; i < 3; i++) {
            color1[i] = (int)(randNext() % 31);
            color2[i] = (int)(randNext() % 31);
        }

        savefile->settings.backgroundType = ((randNext() % 10) > 3);
        drawText();
        setMaxSelection();
    }

    if (key_hit(k.special1)) {
        hsvMode = !hsvMode;

        if (hsvMode) {
            hsv1 = rgb2hsv(color1[0], color1[1], color1[2]);
            hsv2 = rgb2hsv(color2[0], color2[1], color2[2]);
        }

        sfx(SFX_MENUCONFIRM);
        drawText();
    }

    if (!onValues) {
        if (key_hit(k.up)) {
            if (selection > 0)
                selection--;

            sfx(SFX_MENUMOVE);
        }

        if (key_hit(k.down)) {
            if (selection < maxSelection - 1)
                selection++;
            sfx(SFX_MENUMOVE);
        }

        if (key_hit(k.cancel)) {
            sfx(SFX_MENUCANCEL);
            changeScene(previousScene(), Transitions::FADE);
            return true;
        }

        if (selection == 0) {
            if (key_hit(k.left)) {
                if (savefile->settings.backgroundType > 0) {
                    savefile->settings.backgroundType--;
                    drawText();
                    setMaxSelection();
                }

                sfx(SFX_MENUMOVE);
            }

            if (key_hit(k.right)) {
                if (savefile->settings.backgroundType < 1) {
                    savefile->settings.backgroundType++;
                    drawText();
                    setMaxSelection();
                }

                sfx(SFX_MENUMOVE);
            }
        } else {
            if (key_hit(k.right) || key_hit(k.confirm)) {
                onValues = true;
                colorSelection = 0;
                sfx(SFX_MENUCONFIRM);
            }
        }
    } else {
        if (key_hit(k.left)) {
            if (colorSelection > 0) {
                colorSelection--;
                sfx(SFX_MENUMOVE);
            } else {
                onValues = false;
                sfx(SFX_MENUCANCEL);
            }
        }

        if (key_hit(k.cancel)) {
            onValues = false;
            sfx(SFX_MENUCANCEL);
        }

        if (key_hit(k.right)) {
            if (colorSelection < 2)
                colorSelection++;
            sfx(SFX_MENUMOVE);
        }

        if (!hsvMode) {
            int* arr;
            if (selection == 1)
                arr = (int*)color1;
            else
                arr = (int*)color2;

            if (key_hit(k.up)) {
                if (arr[colorSelection] < 31)
                    arr[colorSelection]++;
                sfx(SFX_SHIFT2);
            }

            if (key_hit(k.down)) {
                if (arr[colorSelection] > 0)
                    arr[colorSelection]--;
                sfx(SFX_SHIFT2);
            }

            if (key_is_down(k.up) || key_is_down(k.down)) {
                das++;

                if (das < dasMax) {
                    das++;
                } else {
                    if (key_is_down(k.up)) {
                        if (arr[colorSelection] < 31) {
                            arr[colorSelection]++;
                            sfx(SFX_SHIFT2);
                        }
                    } else if (key_is_down(k.down)) {
                        if (arr[colorSelection] > 0) {
                            arr[colorSelection]--;
                            sfx(SFX_SHIFT2);
                        }
                    }
                    das -= arrMax;
                }
            } else {
                das = 0;
            }
        } else {
            if (key_hit(k.up)) {
                set(+1);
                sfx(SFX_SHIFT2);
            }

            if (key_hit(k.down)) {
                set(-1);
                sfx(SFX_SHIFT2);
            }

            if (key_is_down(k.up) || key_is_down(k.down)) {
                if (das < dasMax) {
                    das++;
                } else {
                    int dir = (key_is_down(k.down) ? -1 : 1);

                    set(dir * (1 + 2 * (colorSelection == 0)));
                    sfx(SFX_SHIFT2);

                    das -= arrMax / 2;
                }
            } else {
                das = 0;
            }
        }
    }

    return false;
}

void GradientEditorScene::deinit() {
    savefile->settings.backgroundGradient = c;

    toggleBG(0, true);
    toggleBG(1, true);
    toggleBG(2, true);

    resetSmallText();
    clearText();

    for (int i = 0; i < MAX_WORD_SPRITES; i++)
        delete wordSprites[i];
}

bool GradientElement::action() {
    sfx(SFX_MENUCONFIRM);
    changeScene(new GradientEditorScene(), Transitions::FADE);

    return true;
}

void GraphicsScene::init() {
    loadTiles(2, 102, sprite37tiles_bin, sprite37tiles_bin_size / 32);
    loadTiles(2, 105, sprite41tiles_bin, sprite41tiles_bin_size / 32);

    clearSprites(128);

    elementList = getElementList();
    options = (int)elementList.size();

    startY = 24;
    if (options < 7) {
        startY += ((7 - options) * 8) & ~0xf;
    }

    selection = 0;

    // transparent gray
    setTiles(27, 0, 32 * 32, tileBuild(34, false, false, 0));

    for (int i = 0; i < MAX_WORD_SPRITES; i++)
        wordSprites[i] = new WordSprite(i, 32 + i * 3, 256 + i * 12);

    for (int i = 0; i < 7; i++)
        labels[i] = new WordSprite(MAX_WORD_SPRITES + i,
                                   32 + 3 * MAX_WORD_SPRITES + i * 5,
                                   512 + 64 + i * 20, true);

    for (int i = 0; i < 3; i++)
        scrollingText[i] = new WordSprite(MAX_WORD_SPRITES + 7 + i, 23 + i * 3,
                                          672 + 64 + i * 12);

    loadSpriteTiles(255, blockSprite, 1, 1);
    for (int i = 0; i < 2; i++) {
        cursorSprites[i] = &obj_buffer[14 + i];
        sprite_set_attr(cursorSprites[i], ShapeSquare, 0, 255, 5, 0);
        sprite_enable_affine(cursorSprites[i], 10, true);
        sprite_hide(cursorSprites[i]);
    }

    loadSpriteTiles(910, sprite41tiles_bin, 2, 1);
    for (int i = 0; i < 2; i++) {
        arrowSprites[i] = &obj_buffer[21 + i];
        sprite_set_attr(arrowSprites[i], ShapeWide, 0, 910, 14, 0);

        if (i)
            sprite_enable_flip(arrowSprites[1], false, true);

        sprite_hide(arrowSprites[i]);
    }

    if (name() != "") {
        bool found = false;
        int count = 0;
        auto it = path.begin();
        while (it != path.end()) {
            if (*it == name()) {
                found = true;
                break;
            }
            count++;
            it++;
        }

        if (found) {
            int n = path.size() - count;
            for (int i = 0; i < n; i++)
                path.pop_back();
        } else {
            path.push_back(name());
        }
    }

    int i = 0;
    auto it = path.begin();
    while (it != path.end()) {
        if (i != 0)
            p += " > ";

        p += *it;

        it++;
        i++;
    }

    int width = getVariableWidth(p) >> 3;
    pathCount = (width) / 12 + 1;

    int wordIndex = MAX_WORD_SPRITES - pathCount - 1;

    wordSprites[wordIndex]->setText(p);

    for (int i = 0; i < pathCount; i++)
        wordSprites[wordIndex + i]->show(
            i * 12 * 8 + 4 + 13 * savefile->settings.aspectRatio, 4, 14);

    setPalette();
    setSkin();
    setGradient(savefile->settings.backgroundGradient);

    if (savefile->settings.lightMode)
        setPaletteColor(0, 0, 0x5ad6, 1);
    else
        setPaletteColor(0, 0, 0x0000, 1);

    clearTilemap(26);

    enableFumen = true;
    fumenString = "v115@fgY4Aeo0Ae4wAeItAeYpAeolAe4hKeAgHfg4DAeoHA?"
                  "eYLAeIPAe4SAeoWAeQaQ4WaKeAAAchAAA8QeAAA";

    if (game != nullptr)
        delete game;

    game = new Game(NO_MODE, bigMode);

    game->demoGarbage();
    game->setGoal(0);
    game->setLevel(1);
    game->rotationSystem = SRS;
    game->setRandomizer(BlockEngine::Randomizers::BAG_7);

    game->update();

    game->pawn.y += 1;

    enableFumen = false;

    enableBlend((1 << 6) + (0b111110 << 8) + (0b1010));

    buildBG(3, 0, 27, 0, 0, 0);

    drawFrameBackgrounds();

    loadSpriteTiles(512, sprite6tiles_bin, 4, 4);

    loadSpriteTiles(512 + 16, sprite6tiles_bin, 4, 3);
    loadSpriteTilesPartial(512 + 16, &sprite6tiles_bin[128], 0, 3, 4, 1, 4);
    loadSpriteTiles(512 + 32, &sprite6tiles_bin[128], 4, 2);
    loadSpriteTilesPartial(512 + 32, &sprite6tiles_bin[128], 0, 2, 4, 2, 4);
    loadSpriteTiles(512 + 48, &sprite6tiles_bin[128], 4, 1);
    loadSpriteTilesPartial(512 + 48, &sprite6tiles_bin[128], 0, 1, 4, 3, 4);

    draw();

    refreshGame = true;

    for (int i = 0; i < 3; i++)
        scrollingText[i]->show(12 * 8 * i, 160 - 8, 15);

    scrollingText[0]->setTextSlow(scrollText);

    int offset = 0;
    if (savefile->settings.aspectRatio == 1) {
        offset = (240 - 214) / 2;
    }

    resetSmallText();
    setSmallTextArea(110, 0, 17, 15, 20);
    clearText();

    std::string hide = "Hide Options: " +
                       getStringFromKey(savefile->settings.menuKeys.special2);
    aprints(hide, 1 + offset, 24 - 7, 2);

    previousAspectRatio = savefile->settings.aspectRatio;
}

void GraphicsScene::draw() {
    if (showText) {
        if (refreshText) {
            setTiles(27, 0, 32 * 32, tileBuild(34, false, false, 0));
        }

        const int space = 1;
        const int startX = 3 * 8;
        const int endX = 30 - 4;

        int offset = (sinLut(cursorFloat) * 2) >> 12;
        FIXED scale = float2fx((1.0 - ((float)0.1 * offset)));

        int verticalOffset = 0;
        if (moving) {
            int temp =
                lerp((movingDirection) * (space + 1) * 8, 0, 64 * movingTimer);
            verticalOffset = lerp(temp, 0, 64 * movingTimer);
            setLayerScroll(2, 0, -verticalOffset);
        } else {
            setLayerScroll(2, 0, 0);
        }

        int i = 0;       // drawing index, starts at listStart
        int counter = 0; // element index
        for (auto& option : elementList) {
            if (counter < listStart) {
                counter++;
                continue;
            }

            if (i > elementMax + 1) { // break if elements drawn reached max
                continue;
            }

            std::string opt = option->getCurrentOption();

            int index = (i + listStart) % (elementMax + 2);

            if (refreshText || refreshOption) {
                if (counter == selection)
                    wordSprites[index]->setText(
                        option->getCursor(opt)); // draw option with text cursor
                else
                    wordSprites[index]->setText(
                        " " + opt); // draw option without text cursor
            }

            if (refreshText)
                labels[index]->setText(option->getLabel());

            int x = (endX) * 8 - getVariableWidth(opt);
            int y =
                startY + 8 * (space + 1) * i + verticalOffset - spriteVOffset;

            wordSprites[index]->setPriority(0);
            wordSprites[index]->show(
                x, y,
                15 - (counter != selection) -
                    (i == elementMax + 1 || (listStart && i == 0)));

            labels[index]->show(
                startX, y,
                15 - ((counter != selection) +
                      (i == elementMax + 1 || (listStart && i == 0))));

            if (counter == selection) { // draw sprite cursor
                currentOption = option->getLabel();
                for (int i = 0; i < 2; i++) {
                    sprite_unhide(cursorSprites[i], 0);
                    sprite_enable_affine(cursorSprites[i], 10, true);
                    sprite_set_size(cursorSprites[i], scale, 10);

                    int x = startX - ((8) + offset + 4) - 4;

                    sprite_set_pos(cursorSprites[i], x, y - 5);
                }
            }

            i++;
            counter++;
        }

        for (int j = i; j < elementMax + 2; j++) {
            int index = (j + listStart) % (elementMax + 2);

            wordSprites[index]->hide();
            labels[index]->hide();
        }

        refreshOption = false;
    }

    if (refreshGame) {
        refreshGame = false;

        drawFrame(0);

        showQueue(1);
        showHold();
        showBackground(0);
    }

    showPawn();
    showShadow();
    screenShake();
    drawGrid();
    showClear();

    showPlaceEffect();

    if (savefile->settings.clearText == 2) {
        showClearText();
    }

    if (listStart > 0 && showText) {
        sprite_unhide(arrowSprites[0], 0);
        sprite_set_pos(arrowSprites[0], 240 / 2 - 8, startY - 11);
    } else {
        sprite_hide(arrowSprites[0]);
    }

    if (listStart + elementMax + 2 < (int)elementList.size() && showText) {
        sprite_unhide(arrowSprites[1], 0);
        sprite_set_pos(arrowSprites[1], 240 / 2 - 8,
                       (elementMax + 2) * 2 * 8 + startY + 0);
    } else {
        sprite_hide(arrowSprites[1]);
    }

    const int width = (savefile->settings.aspectRatio == 0) ? 240 : 214;

    if (refreshText) {
        scrollText = getDescription(currentOption);
        clearSpriteTiles(672 + 64, 144, 1);
        if (scrollText != "") {
            scrollTextLength = getVariableWidth(scrollText);

            endDelay = 0;
            scrollOffset = -max((width - scrollTextLength), 0) / 2;
            scrollDelay = 0;
            scrollingText[0]->setTextSlow(scrollText);
        } else {
            scrollingText[0]->setTextSlow(scrollText);
        }
    }

    int wordIndex = MAX_WORD_SPRITES - pathCount - 1;
    for (int i = 0; i < pathCount; i++)
        wordSprites[wordIndex + i]->show(
            i * 12 * 8 + 4 + 13 * savefile->settings.aspectRatio, 4, 14);

    showSprites(128);

    refreshText = false;
}

void GraphicsScene::update() {
    canDraw = 1;
    key_poll();
    gradient(1);

    toggleBG(0, true);
    toggleBG(1, true);
    toggleBG(2, true);
    toggleBG(3, true);

    cursorFloat += 6;
    if (cursorFloat >= 512)
        cursorFloat = 0;

    if (moving) {
        if (++movingTimer > 4) {
            moving = false;
            movingHor = false;
            movingTimer = 0;
            movingDirection = 0;
            refreshText = true;
        }
    }

    if (++gridUpdateTimer >= gridUpdateTimerMax) {
        gridUpdateTimer = 0;
        updateGrid();

        FIXED temp[22][12];

        memcpy32_fast(temp, current, 22 * 12);
        memcpy32_fast(current, previous, 22 * 12);
        memcpy32_fast(previous, temp, 22 * 12);
    }

    if (dampTimer) {
        dampTimer--;
        if (!dampTimer)
            damp = 2;
    }

    const int width = (savefile->settings.aspectRatio == 0) ? 240 : 214;

    if (endDelay) {
        endDelay--;
        if (!endDelay) {
            scrollOffset = 0;
            scrollDelay = 0;
            clearSpriteTiles(672 + 64, scrollTextLength / 8 + 1, 1);
            scrollingText[0]->setTextSlow(scrollText);
        }
    } else if (scrollDelay < scrollDelayMax) {
        scrollDelay++;
    } else if (scrollTextLength > width) {
        if (++scrollTimer >= scrollTimerMax) {
            scrollTimer = 0;
            if (++scrollOffset >= max(scrollTextLength - width, 0)) {
                endDelay = endDelayMax;
            }
        }
    }

    for (int i = 0; i < 3; i++) {
        scrollingText[i]->setScroll(scrollOffset);
        scrollingText[i]->show(12 * 8 * i + 13 * savefile->settings.aspectRatio,
                               160 - 8, 14);
    }

    if (previousAspectRatio != savefile->settings.aspectRatio) {
        clearSmallText();
        int offset = 0;
        if (savefile->settings.aspectRatio == 1) {
            offset = (240 - 214) / 2;
        }

        std::string hide =
            "Hide Options: " +
            getStringFromKey(savefile->settings.menuKeys.special2);
        aprints(hide, 1 + offset, 24 - 7, 2);

        previousAspectRatio = savefile->settings.aspectRatio;
    }

    control();
}

bool GraphicsScene::control() {
    MenuKeys k = savefile->settings.menuKeys;

    if (key_hit(k.cancel)) {
        sfx(SFX_MENUCANCEL);
        previousElement = name();
        previousSelection = selection;

        if (path.size())
            path.pop_back();

        changeScene(previousScene(), Transitions::FADE);
        return true;
    }

    if (key_hit(k.left)) {
        auto it = elementList.begin();
        std::advance(it, selection);

        (*it)->action(-1);

        refreshGame = true;
        refreshOption = true;
    }

    if (key_hit(k.right)) {
        auto it = elementList.begin();
        std::advance(it, selection);

        (*it)->action(1);

        refreshGame = true;
        refreshOption = true;
    }

    if (key_hit(k.confirm)) {
        auto it = elementList.begin();
        std::advance(it, selection);

        if ((*it)->action())
            return true;

        refreshText = true;
        refreshGame = true;
        refreshOption = true;
    }

    if (key_hit(k.special2)) {
        showText = !showText;

        if (showText) {
            setTiles(27, 0, 32 * 32, tileBuild(34, false, false, 0));

            int wordIndex = MAX_WORD_SPRITES - pathCount - 1;
            for (int i = 0; i < pathCount; i++)
                wordSprites[wordIndex + i]->show(i * 12 * 8 + 4, 4, 14);

        } else {
            clearTilemap(27);

            for (int j = 0; j < elementMax + 2; j++) {
                int index = (j + listStart) % (elementMax + 2);

                wordSprites[index]->hide();
                labels[index]->hide();
            }

            for (int i = 0; i < 2; i++)
                sprite_hide(cursorSprites[i]);

            int wordIndex = MAX_WORD_SPRITES - pathCount - 1;
            for (int i = 0; i < pathCount; i++)
                wordSprites[wordIndex + i]->hide();
        }

        refreshGame = true;
    }

    int prev = listStart;

    if (key_hit(k.up)) {
        movingDirection = -1;

        if (selection == 0)
            selection = options - 1;
        else
            selection--;

        if (selection == options - 1)
            listStart = max(0, selection - elementMax);
        else if (selection < listStart + 1) {
            listStart--;
            if (listStart < 0)
                listStart = 0;
        }

        sfx(SFX_MENUMOVE);
        refreshText = true;
        // refreshGame = true;
    }

    if (key_hit(k.down)) {
        movingDirection = 1;

        if (selection == options - 1)
            selection = 0;
        else
            selection++;

        if (selection == 0)
            listStart = 0;
        else if (selection > listStart + elementMax)
            listStart++;

        sfx(SFX_MENUMOVE);
        refreshText = true;
        // refreshGame = true;
    }

    if (key_is_down(k.up) || key_is_down(k.down)) {
        if (dasVer < maxDas) {
            dasVer++;
        } else {
            int dir = (key_is_down(k.up) ? -1 : 1);

            if (selection + dir >= 0 && selection + dir <= options - 1 &&
                arr++ > maxArr) {
                arr = 0;

                movingDirection = dir;

                selection += dir;

                if (dir < 0) {
                    if (selection == options - 1)
                        listStart = max(0, selection - elementMax);
                    else if (selection < listStart + 1) {
                        listStart--;
                        if (listStart < 0)
                            listStart = 0;
                    }
                } else {
                    if (selection == 0)
                        listStart = 0;
                    else if (selection > listStart + elementMax)
                        listStart++;
                }

                refreshText = true;
                // refreshGame = true;
                sfx(SFX_MENUMOVE);
            }
        }
    } else {
        dasVer = 0;
    }

    if (key_is_down(k.left) || key_is_down(k.right)) {
        if (dasHor < maxDas) {
            dasHor++;
        } else {
            int dir = (key_is_down(k.left) ? -1 : 1);
            if (arr++ > maxArr) {
                arr = 0;

                auto it = elementList.begin();
                std::advance(it, selection);

                (*it)->action(dir);

                refreshGame = true;
                refreshOption = true;
            }
        }
    } else {
        dasHor = 0;
    }

    if (prev != listStart) {
        moving = true;
    }

    if (key_hit(k.special3)) {
        setDefaultGraphics(savefile, 0);
        setSkin();
        setPalette();
        drawFrameBackgrounds();

        refreshGame = true;

        sfx(SFX_MENUCONFIRM);
    }

    return false;
}

void GraphicsScene::deinit() {
    buildBG(3, 0, 27, 0, 1, true);

    clearSprites(128);
    showSprites(128);

    clearTilemap(25);
    clearTilemap(26);
    clearTilemap(27);
    clearSpriteTiles(2, 100, 1);
    clearSpriteTiles(256, 256, 1);
    for (int i = 0; i < 7; i++)
        clearSpriteTiles(512 + 64 + i * 20, 20, 1);

    resetSmallText();
    clearText();

    auto it = elementList.begin();

    while (it != elementList.end()) {
        delete *it;
        it++;
    }

    gradient(false);
    spriteVOffset = 0;

    for (int i = 0; i < MAX_WORD_SPRITES; i++)
        delete wordSprites[i];

    for (int i = 0; i < 7; i++)
        delete labels[i];

    for (int i = 0; i < 3; i++)
        delete scrollingText[i];

    disableLayerWindow(1);
    disableLayerWindow(2);
}

void screenShake() {
    // int scroll = 0;
    // if(clearTimer){
    //     int n = game->previousClear.linesCleared;

    //     FIXED s = fxdiv(int2fx(8 * n * clearTimer),int2fx(maxClearTimer));

    //     scroll = fx2int(s);
    // }

    setLayerScroll(1, -push, -(shake + peekShift));
    setLayerScroll(2, -push, -(shake + peekShift));

    enableLayerWindow(1, 0, shake, 240, 168 + shake, false);
    enableLayerWindow(2, 0, shake, 240, 168 + shake, false);

    if (savefile->settings.screenShakeType == 0) {
        if (shake) {
            if (shake > 0)
                shake--;
            else
                shake++;
            shake *= -1;
        }

    } else if (savefile->settings.screenShakeType == 1) {
        shake = fx2int(shakeBuff);
        const FIXED deccel = float2fx(0.768);
        if (shakeBuff) {

            shakeBuff = fxmul(shakeBuff, deccel);

            if (abs(shakeBuff) < int2fx(1))
                shakeBuff = 0;
        }

        if (shakeVelocity) {
            shakeBuff += shakeVelocity;
            shakeVelocity = fxmul(shakeVelocity, deccel);
            if (shakeVelocity <= 0) {
                shakeVelocity = 0;
            }

            if (shake >= (shakeMax * (savefile->settings.shakeAmount) / 4)) {
                shake = (shakeMax * (savefile->settings.shakeAmount) / 4);
            }
        }
    }

    if (game->pushDir != 0) {
        if (abs(push) < pushMax * (savefile->settings.shakeAmount) / 4)
            push += game->pushDir * (1 + (savefile->settings.shakeAmount > 2));
    } else {
        if (push > 0)
            push--;
        else if (push < 0)
            push++;
    }
}

void GraphicsScene::showClear() {
    if (currentOption != "Clear Direction") {
        for (int i = 0; i < game->lengthX; i++) {
            setTile(25, 10 + i, 18,
                    tileBuild(3, false, false, savefile->settings.lightMode));
            setTile(25, 10 + i, 19,
                    tileBuild(3, false, false, savefile->settings.lightMode));
        }

        return;
    }

    if (++clearAnimationTimer >= maxClearAnimationTimer * 2) {
        clearAnimationTimer = -maxClearAnimationTimer;
    }

    clearTilemapEntries(25, 18 * 32 + 10, 10);
    clearTilemapEntries(25, 19 * 32 + 10, 10);

    if (clearAnimationTimer >= maxClearAnimationTimer) {
        return;
    }

    int timer = max(clearAnimationTimer, 0);

    for (int i = 0; i < 2; i++) {
        if (savefile->settings.clearDirection == 0) {
            for (int j = 0; j < 5; j++) {
                if (timer < maxClearAnimationTimer - 10 + j * 2)
                    setTile(25, 10 + j, 18 + i, 3);
            }
            for (int j = 5; j < 10; j++) {
                if (timer < maxClearAnimationTimer - 10 + (9 - j) * 2)
                    setTile(25, 10 + j, 18 + i, 3);
            }
        } else if (savefile->settings.clearDirection == 1) {
            for (int j = 0; j < 5; j++) {
                if (timer < maxClearAnimationTimer - 10 + (4 - j) * 2)
                    setTile(25, 10 + j, 18 + i, 3);
            }
            for (int j = 5; j < 10; j++) {
                if (timer < maxClearAnimationTimer - 10 + (j - 5) * 2)
                    setTile(25, 10 + j, 18 + i, 3);
            }
        }
    }
}

void setClearEffect() {
    switch (savefile->settings.clearEffect) {
    case 0:
        loadTiles(0, 3, sprite4tiles_bin, sprite4tiles_bin_size / 32);
        break;
    case 1:
        loadTiles(0, 3, sprite25tiles_bin, sprite25tiles_bin_size / 32);
        break;
    case 2:
        loadTiles(0, 3, sprite26tiles_bin, sprite26tiles_bin_size / 32);
        break;
    case 3:
        clearTiles(0, 3, 1);
        break;
    default:
        loadTiles(0, 3, sprite26tiles_bin, sprite26tiles_bin_size / 32);
        break;
    }
}
