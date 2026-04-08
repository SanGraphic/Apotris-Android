#include "sceneColorEditor.hpp"

#include "sprite30tiles_bin.h"
#include "sprites.h"
#include "text.h"
#include <posprintf.h>
#include <string>

void ColorEditorScene::init() {
    setPaletteColor(0, 0, RGB15(41, 205, 41), 1);
    setPalette();
    setSkin(1);
    toggleBG(2, true);
    toggleBG(1, true);
    buildBG(2, 1, 25, 0, 2, 0);

    // load custom palette
    for (int i = 0; i < 7; i++) {
        loadPalette(i, 1, savefile->customPalettes[paletteIndex][i], 3);
        loadPalette(i + 16, 1, savefile->customPalettes[paletteIndex][i], 3);
    }

    resetSmallText();

    setSmallTextArea(256, 0, 18, 30, 20);

    drawText();

    // background
    setTiles(26, 0, 32 * 32, 35 * !(savefile->settings.lightMode));

    // arrow
    loadTiles(2, 105, sprite41tiles_bin, sprite41tiles_bin_size / 32);

    // cursor
    loadSpriteTiles(512 + 200, sprite29tiles_bin, 2, 2);

    // color sprites
    loadSpriteTiles(254, &sprite48tiles_bin, 1, 1);

    // color sprite colors
    setPaletteColor(16 + 0, 15, RGB15(31, 0, 0), 1);
    setPaletteColor(16 + 1, 15, RGB15(0, 31, 0), 1);
    setPaletteColor(16 + 2, 15, RGB15(5, 5, 31), 1);

    // create solid color tiles for each color
    for (int i = 1; i < 4; i++) {
        TILE t;

        const u32 fill = 0x11111111 * i;

        memset32(&t, fill, 8);

        loadTiles(1, i, &t, 1);
    }

    showPalettes();

    for (int i = 0; i < 3; i++) {
        colorSprites[i] = &obj_buffer[15 + i];

        sprite_set_attr(colorSprites[i], ShapeSquare, 0, 254, i, 0);
        sprite_hide(colorSprites[i]);
    }

    // cursor
    loadSpriteTiles(16 * 7, blockSprite, 1, 1);
    for (int i = 0; i < 2; i++) {
        cursorSprites[i] = &obj_buffer[28 + i];
        sprite_set_attr(cursorSprites[i], ShapeSquare, 0, 7 * 16, 5, 1);
        sprite_enable_affine(cursorSprites[i], i, true);
        sprite_hide(cursorSprites[i]);
    }
}

void ColorEditorScene::draw() {
    showCursor();

    if (!refresh) {
        showSprites(128);

        return;
    }
    refresh = false;

    showMinos();

    const int space = 5;
    const int height = 13 + colorSelectorHeight;
    const int tile = 105;

    if (!reset) {
        for (int i = 0; i < 3; i++) {
            int x = 14 + (i - 1) * space;

            char buff[8];

            if (hsvMode) {
                switch (i) {
                case 0:
                    posprintf(buff, "H%03d", hsv.hue);
                    break;
                case 1:
                    posprintf(buff, "S%03d", (int)(hsv.saturation * 100));
                    break;
                case 2:
                    posprintf(buff, "V%03d", (int)(hsv.value * 100));
                    break;
                }

            } else {
                posprintf(buff, "%02d", color1[i]);
            }

            aprintColor(buff, x - hsvMode, height,
                        !((i == selection) && !onColors));

            if (i == selection && !onColors) {
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
            } else {
                sprite_hide(colorSprites[i]);
            }
            sprite_set_pos(colorSprites[i], x * 8 - 9, height * 8);
        }
    } else {
        for (int i = 0; i < 3; i++)
            sprite_hide(colorSprites[i]);
    }

    showSprites(128);
}

void ColorEditorScene::update() {
    if (control())
        return;

    canDraw = true;

    cursorTimer++;

    const int animationLength = 32;

    if (!onColors || cursorTimer < animationLength / 2) {
        loadSpriteTiles(512 + 200, sprite29tiles_bin, 2, 2);
    } else {
        loadSpriteTiles(512 + 200, sprite30tiles_bin, 2, 2);
        if (cursorTimer >= animationLength - 1)
            cursorTimer = 0;
    }

    cursorFloat += 6;
    if (cursorFloat >= 512)
        cursorFloat = 0;
}

bool ColorEditorScene::control() {
    key_poll();

    MenuKeys k = savefile->settings.menuKeys;

    if (reset) {
        if (key_hit(k.cancel) || (key_hit(k.confirm) && selection == 1)) {
            reset = false;
            refresh = true;
            selection = 0;
            drawText();

            sprite_hide(cursorSprites[0]);
            sprite_hide(cursorSprites[1]);

            savefile->settings.colors = -(paletteIndex + 1);

            memcpy16(savefile->customPalettes[paletteIndex], paletteBeforeReset,
                     21);

            setPalette();

            // color sprite colors
            setPaletteColor(16 + 0, 15, RGB15(31, 0, 0), 1);
            setPaletteColor(16 + 1, 15, RGB15(0, 31, 0), 1);
            setPaletteColor(16 + 2, 15, RGB15(5, 5, 31), 1);

            sfx(SFX_MENUCANCEL);
        }

        if (key_hit(k.confirm) && selection == 0) {

            COLOR* p = new COLOR[512];
            savePalette(p);

            for (int i = 0; i < 7; i++) {
                memcpy16(savefile->customPalettes[paletteIndex][i],
                         &p[i * 16 + 1], 3);
            }

            delete[] p;

            reset = false;
            selection = 0;
            refresh = true;
            drawText();
            sfx(SFX_MENUCONFIRM);

            // color sprite colors
            setPaletteColor(16 + 0, 15, RGB15(31, 0, 0), 1);
            setPaletteColor(16 + 1, 15, RGB15(0, 31, 0), 1);
            setPaletteColor(16 + 2, 15, RGB15(5, 5, 31), 1);

            return false;
        }

        if (key_hit(k.up) || key_hit(k.down)) {
            selection = !selection;
            refresh = true;
            drawText();
            sfx(SFX_MENUMOVE);
        }

        if (selection == 0 && (key_hit(k.left) || key_hit(k.right))) {
            resetIndex += (key_hit(k.left)) ? -1 : 1;

            resetIndex = clamp(resetIndex, 0, MAX_COLORS);

            savefile->settings.colors = resetIndex;

            setPalette();

            drawText();

            refresh = true;
            sfx(SFX_MENUMOVE);
        }

        return false;
    }

    if (key_hit(k.pause)) {
        COLOR colors[32][16];
        savePalette((COLOR*)&colors);

        for (int i = 0; i < 7; i++) {
            memcpy16(savefile->customPalettes[paletteIndex][i], &colors[i][1],
                     3);
        }

        changeScene(previousScene(), Transitions::FADE);
        return true;
    }

    if (key_hit(k.special1)) {
        hsvMode = !hsvMode;

        if (hsvMode)
            setHSV();
        else
            setRGB();

        sfx(SFX_MENUCONFIRM);
        refresh = true;
        drawText();
        return false;
    }

    if (onColors) {
        if (key_hit(k.left)) {
            cursorX--;

            if (cursorX < 0)
                cursorX = 6;

            if (hsvMode)
                setHSV();
            else
                setRGB();

            refresh = true;
            sfx(SFX_SHIFT2);
        }

        if (key_hit(k.right)) {
            cursorX++;

            if (cursorX > 6)
                cursorX = 0;

            if (hsvMode)
                setHSV();
            else
                setRGB();

            refresh = true;
            sfx(SFX_SHIFT2);
        }

        if (key_hit(k.up)) {
            cursorY--;

            if (cursorY < 0)
                cursorY = 2;

            if (hsvMode)
                setHSV();
            else
                setRGB();

            refresh = true;
            sfx(SFX_SHIFT2);
        }

        if (key_hit(k.down)) {
            cursorY++;

            if (cursorY > 2)
                cursorY = 0;

            if (hsvMode)
                setHSV();
            else
                setRGB();

            refresh = true;
            sfx(SFX_SHIFT2);
        }

        if (key_hit(k.confirm)) {
            onColors = false;
            setHSV();
            setRGB();
            refresh = true;
            sfx(SFX_MENUCONFIRM);
        }
    } else {
        if (key_hit(k.confirm)) {
            calculate();
            refresh = true;
        }

        if (key_hit(k.cancel)) {
            onColors = true;
            refresh = true;
            sfx(SFX_MENUCANCEL);
        }

        if (key_hit(k.left)) {
            selection--;
            if (selection < 0)
                selection = maxSelection;
            refresh = true;
        }

        if (key_hit(k.right)) {
            selection++;
            if (selection > maxSelection)
                selection = 0;
            refresh = true;
        }

        if (!hsvMode) {
            if (key_hit(k.up)) {
                if (color1[selection] < 31)
                    color1[selection]++;
                refresh = true;
                calculate();
                sfx(SFX_SHIFT2);
            }

            if (key_hit(k.down)) {
                if (color1[selection] > 0)
                    color1[selection]--;
                refresh = true;
                calculate();
                sfx(SFX_SHIFT2);
            }

            if (key_is_down(k.up) || key_is_down(k.down)) {
                das++;

                if (das < dasMax) {
                    das++;
                } else {
                    if (key_is_down(k.up)) {
                        if (color1[selection] < 31) {
                            color1[selection]++;
                            refresh = true;
                            calculate();
                            sfx(SFX_SHIFT2);
                        }
                    } else if (key_is_down(k.down)) {
                        if (color1[selection] > 0) {
                            color1[selection]--;
                            refresh = true;
                            calculate();
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
                refresh = true;
                calculate();

                if (set(+1))
                    sfx(SFX_SHIFT2);
            }

            if (key_hit(k.down)) {
                refresh = true;
                calculate();

                if (set(-1))
                    sfx(SFX_SHIFT2);
            }

            if (key_is_down(k.up) || key_is_down(k.down)) {
                if (das < dasMax) {
                    das++;
                } else {
                    int dir = (key_is_down(k.down) ? -1 : 1);

                    if (set(dir * (1 + 2 * (selection == 0))))
                        sfx(SFX_SHIFT2);

                    refresh = true;
                    calculate();

                    das -= arrMax / 2;
                }
            } else {
                das = 0;
            }
        }
    }

    if (key_is_down(k.special2)) {
        if (++resetTimer > resetTimerMax) {
            reset = true;
            refresh = true;
            selection = 0;
            drawText();

            if (paletteBeforeReset == nullptr)
                paletteBeforeReset = new COLOR[7 * 3];

            COLOR colors[32][16];
            savePalette((COLOR*)&colors);

            for (int i = 0; i < 7; i++) {
                memcpy16(&paletteBeforeReset[i * 3], &colors[i][1], 3);
            }

            resetIndex = 0;
            savefile->settings.colors = resetIndex;
            setPalette();
        }
    } else {
        resetTimer = 0;
    }

    return false;
}

void ColorEditorScene::deinit() {
    // clean up
    clearSpriteTiles(712, 16, 1);
    clearSprites(128);
    showSprites(128);
    clearTilemap(25);
    clearTilemap(26);
    clearTilemap(27);

    setLayerScroll(3, 0, 0);

    clearTiles(2, 110, 400);
    buildBG(2, 0, 25, 0, 2, true);

    clearText();
    clearSmallText();

    delete[] paletteBeforeReset;
}

void ColorEditorScene::calculate() {
    COLOR color;
    if (hsvMode)
        color = hsv2rgb(hsv);
    else
        color = color1[0] + (color1[1] << 5) + (color1[2] << 10);

    setPaletteColor(cursorX, 3 - cursorY, color, 1);
    setPaletteColor(16 + cursorX, 3 - cursorY, color, 1);
}

void ColorEditorScene::showMinos() {

    const int x = 208 - 13 * (savefile->settings.aspectRatio != 0);

    for (int i = 0; i < 7; i++) {
        OBJ_ATTR* sprite = &obj_buffer[i + 1];

        sprite_set_attr(sprite, ShapeSquare, 2, 16 * i, i, 2);
        sprite_unhide(sprite, 0);
        const int y = 8 - 4 * (i == 0) + 4 * (i == 3) + colorSelectorHeight * 8;
        const int sx = 4 + i * 3;
        sprite_set_pos(sprite, sx * 8 - 4 * (i == 0), y);
    }
}

void ColorEditorScene::showPalettes() {
    const int add = (savefile->settings.aspectRatio == 1);

    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 3; j++) {
            const int y = j * 2 + 5 + colorSelectorHeight;
            const int x = 4 + i * 3 + 1;

            setTile(25, x, y, tileBuild(3 - j, false, false, i));
            setTile(25, x, y + 1, tileBuild(3 - j, false, false, i));
            setTile(25, x + 1, y, tileBuild(3 - j, false, false, i));
            setTile(25, x + 1, y + 1, tileBuild(3 - j, false, false, i));
        }
    }
}

void ColorEditorScene::showCursor() {
    OBJ_ATTR* sprite = &obj_buffer[32];

    if (reset) {
        sprite_hide(sprite);

        if (selection == 1) {
            const int offset = (sinLut(cursorFloat) * 2) >> 12;
            FIXED scale = float2fx((1.0 - ((float)0.1 * offset)));

            for (int i = 0; i < 2; i++) {
                sprite_unhide(cursorSprites[i], 0);
                sprite_enable_affine(cursorSprites[i], i, true);
                sprite_set_size(cursorSprites[i], scale, i);

                const int x =
                    240 / 2 -
                    ((getVariableWidth("CANCEL") + 8) / 2 + offset + 4) *
                        ((i) ? -1 : 1) -
                    8;

                sprite_set_pos(cursorSprites[i], x, 144 - 5);
            }
        } else {
            sprite_hide(cursorSprites[0]);
            sprite_hide(cursorSprites[1]);
        }

        return;
    }

    int y = cursorY * 2 + 5 + colorSelectorHeight;
    int x = 4 + cursorX * 3 + 1;

    sprite_unhide(sprite, 0);
    sprite_set_attr(sprite, ShapeSquare, 1, 712, 15 - !onColors, 0);
    sprite_set_pos(sprite, x * 8, y * 8);
}

void ColorEditorScene::setHSV() {
    COLOR colors[32][16];
    savePalette((COLOR*)&colors);

    COLOR color = colors[cursorX][3 - cursorY];

    hsv = rgb2hsv(color & 31, (color >> 5) & 31, (color >> 10) & 31);
}

void ColorEditorScene::setRGB() {
    COLOR colors[32][16];
    savePalette((COLOR*)&colors);

    COLOR color = colors[cursorX][3 - cursorY];

    color1[0] = color & 31;
    color1[1] = (color >> 5) & 31;
    color1[2] = (color >> 10) & 31;
}

void ColorEditorScene::drawText() {
    clearText();

    if (reset) {
        std::string resetText = "Reset to Palette: ";

        naprint(resetText, (240 - getVariableWidth(resetText)) / 2, 112);

        std::string cancel = "CANCEL";

        naprintColor(cancel, (240 - getVariableWidth(cancel)) / 2, 144,
                     14 + (selection == 1));

        std::string paletteText = std::to_string(resetIndex + 1);
        const int width = getVariableWidth(paletteText);

        if (selection == 0 && resetIndex > 0)
            paletteText = "<" + paletteText;

        if (selection == 0 && resetIndex < MAX_COLORS - 1)
            paletteText += ">";

        naprintColor(paletteText,
                     (240 - width) / 2 - getVariableWidth("<") *
                                             (selection == 0 && resetIndex > 0),
                     128, 15 - (selection == 1));

        return;
    }

    int offset = 1;
    if (savefile->settings.aspectRatio == 1) {
        offset += (240 - 214) / 2;
    }

    std::string colorModeString =
        getStringFromKey(savefile->settings.menuKeys.special1);

    std::string colorMode =
        "Press " + colorModeString + " for " + ((!hsvMode) ? "HSV" : "RGB");
    aprints(colorMode, offset, 0, 2);

    std::string resetHint =
        "Hold " + getStringFromKey(savefile->settings.menuKeys.special2) +
        " to reset";

    aprints(resetHint, offset, 8, 2);

    std::string keyString = getStringFromKey(savefile->settings.menuKeys.pause);

    std::transform(keyString.begin(), keyString.end(), keyString.begin(),
                   ::toupper);

    std::string s = "Press " + keyString + " to Exit";
    aprints(s, 30 * 8 - 4 * s.size() - offset, 10, 2);
}

void ColorSelectorScene::init() {
    clearSprites(128);
    showSprites(128);

    // background
    setTiles(26, 0, 32 * 32, 35 * !(savefile->settings.lightMode));

    clearTilemap(25);
    clearTilemap(27);
    clearTilemap(29);

    resetSmallText();
    clearText();
    naprint("Select a slot to edit:", 5 * 8, 6 * 8);

    // load custom palettes to sprite palette
    // and create custom palette sprites
    for (int i = 0; i < MAX_CUSTOM_PALETTES; i++) {
        for (int j = 0; j < 7; j++) {
            loadPalette(16 + j, 1 + i * 3, savefile->customPalettes[i][j], 3);

            for (int k = 0; k < 3; k++) {
                TILE tile;

                memset32(&tile, 0x11111111 * (1 + (i * 3) + k), 8);

                loadSpriteTilesPartial(512 + 200 + i * 28 + j * 4, &tile, 0,
                                       2 - k, 1, 1, 1);
            }
        }
    }

    // show palette sprites
    for (int i = 0; i < MAX_CUSTOM_PALETTES; i++) {
        for (int j = 0; j < 7; j++) {
            OBJ_ATTR* sprite = &obj_buffer[i * (7) + j + 1];

            sprite_set_attr(sprite, ShapeTall, 1, 712 + i * 28 + j * 4, j, 0);
            sprite_set_pos(sprite, (3 + i * 8) * 8 + 4 + j * 8,
                           (slotHeight * 8) + 12);
            sprite_unhide(sprite, 0);
        }
    }

    // cursor
    loadSpriteTiles(16 * 7, blockSprite, 1, 1);
    for (int i = 0; i < 2; i++) {
        cursorSprites[i] = &obj_buffer[28 + i];
        sprite_set_attr(cursorSprites[i], ShapeSquare, 0, 7 * 16, 5, 1);
        sprite_enable_affine(cursorSprites[i], i, true);
        sprite_hide(cursorSprites[i]);
    }

    showSprites(32);
}

void ColorSelectorScene::draw() {
    // for cursor positioning
    int y = 0;
    int startX = 5;
    int len = 0;

    const int spacing = 8;

    aprintColor(" C1      C2      C3 ", startX, slotHeight, 1);
    aprintColor(" BACK ", 12, 16, (selection != options - 1));

    if (selection == 0) {
        // highlight selected slot
        aprint("C" + std::to_string(paletteIndex + 1),
               (startX + 1) + paletteIndex * spacing, slotHeight);

        y = slotHeight * 8;
        startX = ((startX + paletteIndex * spacing) + 2) * 8;
        len = 13;
    } else if (selection == options - 1) {
        y = 16 * 8;
        startX = (15) * 8;
        len = 4 * 8;
    }

    const int offset = (sinLut(cursorFloat) * 2) >> 12;
    FIXED scale = float2fx((1.0 - ((float)0.1 * offset)));

    for (int i = 0; i < 2; i++) {
        sprite_unhide(cursorSprites[i], 0);
        sprite_enable_affine(cursorSprites[i], i, true);
        sprite_set_size(cursorSprites[i], scale, i);

        const int x =
            startX - ((len + 8) / 2 + offset + 4) * ((i) ? -1 : 1) - 8;

        sprite_set_pos(cursorSprites[i], x, y - 5);
    }

    showSprites(32);
}

void ColorSelectorScene::update() {
    if (control())
        return;

    cursorFloat += 6;
    if (cursorFloat >= 512)
        cursorFloat = 0;

    canDraw = true;
}

void ColorSelectorScene::leave() {
    previousElement = "Color Editor";

    clearTilemap(27);
    buildBG(3, 0, 27, 0, 1, 0);
    toggleBG(3, true);

    changeScene(new ConfirmSaveScene(2), Transitions::FADE);
}

bool ColorSelectorScene::control() {
    MenuKeys k = savefile->settings.menuKeys;

    key_poll();

    if (key_hit(k.confirm)) {
        if (selection == 0) {
            sfx(SFX_MENUCONFIRM);
            changeScene(new ColorEditorScene(paletteIndex), Transitions::FADE);
        } else if (selection == 1) {
            sfx(SFX_MENUCANCEL);
            leave();
        }
        return true;
    }

    if (key_hit(k.cancel)) {
        leave();
        return true;
    }

    if (key_hit(k.up)) {
        selection--;
        if (selection < 0)
            selection = options - 1;
        sfx(SFX_MENUMOVE);
    }

    if (key_hit(k.down)) {
        selection++;
        if (selection > options - 1)
            selection = 0;
        sfx(SFX_MENUMOVE);
    }

    if (key_hit(k.left) && paletteIndex > 0 && !selection) {
        paletteIndex--;
        sfx(SFX_MENUMOVE);
    }

    if (key_hit(k.right) && paletteIndex < (MAX_CUSTOM_PALETTES - 1) &&
        !selection) {
        paletteIndex++;
        sfx(SFX_MENUMOVE);
    }

    if (key_is_down(k.left) || key_is_down(k.right)) {
        das++;

        if (das == dasMax) {
            das -= arr;

            if (key_is_down(k.left) && paletteIndex > 0 && !selection) {
                paletteIndex--;
                sfx(SFX_MENUMOVE);
            } else if (key_is_down(k.right) &&
                       paletteIndex < (MAX_CUSTOM_PALETTES - 1) && !selection) {
                paletteIndex++;
                sfx(SFX_MENUMOVE);
            }
        }
    } else {
        das = 0;
    }

    if (key_hit(k.cancel)) {
        sfx(SFX_MENUCANCEL);
        return 1;
    }

    return false;
}

void ColorSelectorScene::deinit() {
    clearText();
    clearSprites(128);
}
