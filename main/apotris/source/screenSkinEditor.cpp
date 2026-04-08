#include "def.h"
#include "logging.h"
#include "menu.h"
#include "platform.hpp"
#include "text.h"
#include <posprintf.h>

#include "sprite29tiles_bin.h"
#include "sprite30tiles_bin.h"
#include "sprite31tiles_bin.h"
#include "sprite32tiles_bin.h"
#include "sprite33tiles_bin.h"
#include "sprite34tiles_bin.h"
#include "sprite35tiles_bin.h"

#include <algorithm>
#include <string>
#include <tuple>

#ifndef MULTIBOOT

// #include "soundbank.h"
#include "blockEngine.hpp"
#include "skinEditor.hpp"

void generateTiles();
void showColorPalette(int);
void showMinos();
void refreshSkin();
void showTools(int);
void showCursor();
void showMini();
int selector();
void helpScreen();
int optionsMenu();
void showHelpText();

TILE* customSkin;

static int cursorTimer = 0;
static int toolTimer = 0;

static int skinIndex = 0;
static bool onMini = false;
static bool refresh = false;

void EditorScene::init() {
    clearSprites(128);
    showSprites(128);

    // background
    setTiles(26, 0, 32 * 32, 35 * !(savefile->settings.lightMode));

    clearTilemap(25);
    clearTilemap(27);
    clearTilemap(29);

    onMini = false;
    gradient(false);
    setPaletteColor(0, 0, 0, 1);
    buildBG(3, 0, 27, 0, 2, 0);

    prevBld = blendInfo;
    enableBlend(0);

    setTiles(26, 0, 32 * 32, 35 * !(savefile->settings.lightMode));

    if (game != nullptr && game->gameMode == BlockEngine::CLASSIC) {
        game->gameMode = BlockEngine::SPRINT;
        setSkin();
    }
}

void EditorScene::update() {
    loadSpriteTiles(512 + 216, sprite35tiles_bin, 2, 2);
    setPalette();

    if (selector()) {
        previousElement = "Skin Editor";

        clearTilemap(27);
        buildBG(3, 0, 27, 0, 1, 0);
        toggleBG(3, true);

        changeScene(new ConfirmSaveScene(1), Transitions::FADE);
        return;
    }

    onMini = false;

    toggleBG(3, true);
    clearText();

    loadTiles(0, 106, sprite34tiles_bin, 4);

    loadSpriteTiles(512 + 200, sprite29tiles_bin, 2, 2);
    loadSpriteTiles(512 + 204, sprite31tiles_bin, 2, 2);
    loadSpriteTiles(512 + 208, sprite32tiles_bin, 2, 2);
    loadSpriteTiles(512 + 212, sprite33tiles_bin, 2, 2);

    generateTiles();

    for (int i = 0; i < 8; i++)
        customSkin->data[i] = savefile->customSkins[skinIndex].board.data[i];

    board.setBoard(customSkin, 0);
    refreshSkin();

    setSmallTextArea(110, 0, 10, 10, 20);
    clearText();

    showHelpText();

    while (closed()) {
        canDraw = true;
        vsync();

        if (control()) {
            break;
        }

        board.show(onMini);
        showMinos();
        showCursor();
        // showMini();

        showColorPalette(currentColor);
        showTools(currentTool);

        showSprites(128);
    }

    refreshSkin();

    clearSpriteTiles(712, 16, 1);
    clearSprites(128);
    showSprites(128);
    clearTilemap(25);

    clearText();
    toggleBG(3, false);

    delete customSkin;
}

void EditorScene::deinit() {
    // clean up
    clearSpriteTiles(712, 16, 1);
    clearSprites(128);
    showSprites(128);
    clearTilemap(25);
    clearTilemap(26);
    clearTilemap(27);

    setLayerScroll(3, 0, 0);

    clearTiles(2, 110, 400);
    enableBlend(prevBld);
}

bool EditorScene::control() {
    key_poll();

    int dx = 0;
    int dy = 0;

    MenuKeys k = savefile->settings.menuKeys;

    if (key_is_down(k.special2)) {
        if (key_hit(k.left)) {
            currentColor--;
            if (currentColor < 0)
                currentColor = 5;
            sfx(SFX_MENUMOVE);
        }

        if (key_hit(k.right)) {
            currentColor++;
            if (currentColor > 5)
                currentColor = 0;
            sfx(SFX_MENUMOVE);
        }

        if (key_hit(k.up)) {
            currentTool--;
            if (currentTool < 0)
                currentTool = 2;
            sfx(SFX_MENUMOVE);
        }

        if (key_hit(k.down)) {
            currentTool++;
            if (currentTool > 2)
                currentTool = 0;
            sfx(SFX_MENUMOVE);
        }
    } else if (key_is_down(k.special1)) {
        if (key_hit(k.up)) {
            savefile->settings.colors--;
            if (savefile->settings.colors < 0)
                savefile->settings.colors = MAX_COLORS - 1;
            sfx(SFX_MENUMOVE);
        }

        if (key_hit(k.down)) {
            savefile->settings.colors++;
            if (savefile->settings.colors > MAX_COLORS - 1)
                savefile->settings.colors = 0;
            sfx(SFX_MENUMOVE);
        }

        if (key_hit(k.up) || key_hit(k.down))
            setPalette();

        if (key_hit(k.left) || key_hit(k.right)) {
            refreshSkin();
            onMini = !onMini;

            if (onMini) {
                for (int i = 0; i < 8; i++)
                    customSkin->data[i] =
                        savefile->customSkins[skinIndex].smallBoard.data[i];

                if (board.cursor.x > 0)
                    board.cursor.x--;
                if (board.cursor.x > 5)
                    board.cursor.x = 5;

                if (board.cursor.y > 0)
                    board.cursor.y--;
                if (board.cursor.y > 5)
                    board.cursor.y = 5;
            } else {
                for (int i = 0; i < 8; i++)
                    customSkin->data[i] =
                        savefile->customSkins[skinIndex].board.data[i];
                board.cursor.x++;
                board.cursor.y++;
            }

            board.setBoard(customSkin, onMini);
            sfx(SFX_MENUCONFIRM);
        }

    } else {
        if (key_hit(k.left) || key_hit(k.right)) {
            dx = ((key_hit(k.right) != 0) - (key_hit(k.left) != 0));
        }

        if (key_hit(k.up) || key_hit(k.down)) {
            dy = ((key_hit(k.down) != 0) - (key_hit(k.up) != 0));
        }

        if (key_is_down(k.left) || key_is_down(k.right) || key_is_down(k.up) ||
            key_is_down(k.down))
            das++;
        else
            das = 0;

        if (das >= dasMax) {

            if (key_is_down(k.left))
                dx = -1;
            else if (key_is_down(k.right))
                dx = 1;

            if (key_is_down(k.up))
                dy = -1;
            else if (key_is_down(k.down))
                dy = 1;

            das -= arr;
        }

        bool moved = board.cursor.move(dx, dy, onMini);
        if ((dx || dy) && key_is_down(k.confirm)) {
            switch (currentTool) {
            case 0:
                board.set(0, true);
                break;
            case 1:
                board.set(currentColor, true);
                break;
            }
            if (moved)
                sfx(SFX_PLACE);
            refreshSkin();
        }
    }

    if (key_hit(k.confirm)) {
        switch (currentTool) {
        case 0:
            board.set(0, true);
            break;
        case 1:
            board.set(currentColor, true);
            break;
        case 2:
            board.fill(currentColor, true);
            break;
        }

        sfx(SFX_PLACE);
        refreshSkin();
    }

    if (key_hit(k.cancel)) {
        if (board.undo())
            sfx(SFX_ROTATE);
        else
            sfx(SFX_MENUCANCEL);

        refreshSkin();
    }

    if (key_hit(k.pause)) {
        sfx(SFX_MENUCONFIRM);
        return true;
    }

    if (key_hit(k.special3)) {
        helpScreen();
    }

    if (key_is_down(k.special1) && key_is_down(k.special2)) {
        if (++timer > 50) {
            board.clear();
            refreshSkin();
            timer = 0;
        }
    } else {
        timer = 0;
    }

    return false;
}

int selector() {
    int das = 0;
    const int arr = 4;
    const int dasMax = 20;
    int cursorFloat = 0;

    int selection = 0;
    const int options = 2;

    const int slotHeight = 9;

    resetSmallText();
    clearText();

    naprint("Select a slot to edit:", 5 * 8, 6 * 8);

    for (int i = 0; i < 5; i++) {
        OBJ_ATTR* sprite = &obj_buffer[i + 1];

        loadSpriteTiles(512 + 200 + i, &savefile->customSkins[i].board, 1, 1);

        sprite_set_attr(sprite, ShapeSquare, 0, 712 + i, 5, 0);
        sprite_set_pos(sprite, (4 + i * 5) * 8 + 4, (slotHeight * 8) + 12);
        sprite_unhide(sprite, 0);
    }

    OBJ_ATTR* cursorSprites[2];

    loadSpriteTiles(16 * 7, blockSprite, 1, 1);
    for (int i = 0; i < 2; i++) {
        cursorSprites[i] = &obj_buffer[7 + i];
        sprite_set_attr(cursorSprites[i], ShapeSquare, 0, 7 * 16, 5, 1);
        sprite_enable_affine(cursorSprites[i], i, true);
        sprite_hide(cursorSprites[i]);
    }

    showSprites(32);

    MenuKeys k = savefile->settings.menuKeys;

    while (closed()) {
        vsync();
        key_poll();

        aprintColor(" C1   C2   C3   C4   C5 ", 3, slotHeight, 1);

        if (selection == 0) {
            aprint("C" + std::to_string(skinIndex + 1), 4 + skinIndex * 5,
                   slotHeight);
        }

        aprintColor(" BACK ", 12, 16, (selection != options - 1));

        int offset = (sinLut(cursorFloat) * 2) >> 12;
        FIXED scale = float2fx((1.0 - ((float)0.1 * offset)));
        int y = 0;
        int startX = 0;
        int len = 0;

        if (selection == 0) {
            y = slotHeight * 8;
            startX = ((3 + skinIndex * 5) + 2) * 8;
            len = 13;
        } else if (selection == options - 1) {
            y = 16 * 8;
            startX = (15) * 8;
            len = 4 * 8;
        }

        for (int i = 0; i < 2; i++) {
            sprite_unhide(cursorSprites[i], 0);
            sprite_enable_affine(cursorSprites[i], i, true);
            sprite_set_size(cursorSprites[i], scale, i);

            const int x =
                startX - ((len + 8) / 2 + offset + 4) * ((i) ? -1 : 1) - 8;

            sprite_set_pos(cursorSprites[i], x, y - 5);
        }

        if (key_hit(k.confirm) || key_hit(k.pause)) {
            if (selection == 0) {
                sfx(SFX_MENUCONFIRM);
                break;
            } else if (selection == 1) {
                sfx(SFX_MENUCANCEL);
                return 1;
            }
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

        if (key_hit(k.left) && skinIndex > 0 && !selection) {
            skinIndex--;
            sfx(SFX_MENUMOVE);
        }

        if (key_hit(k.right) && skinIndex < 4 && !selection) {
            skinIndex++;
            sfx(SFX_MENUMOVE);
        }

        if (key_is_down(k.left) || key_is_down(k.right)) {
            das++;

            if (das == dasMax) {
                das -= arr;

                if (key_is_down(k.left) && skinIndex > 0 && !selection) {
                    skinIndex--;
                    sfx(SFX_MENUMOVE);
                } else if (key_is_down(k.right) && skinIndex < 4 &&
                           !selection) {
                    skinIndex++;
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

        cursorFloat += 6;
        if (cursorFloat >= 512)
            cursorFloat = 0;

        showSprites(32);
    }

    return 0;
}

void generateTiles() {

    TILE* t = new TILE();

    for (int i = 0; i < 6; i++) {
        int n = 0;
        for (int j = 0; j < 8; j++)
            n += (i + (i == 5)) << (j * 4);

        for (int j = 0; j < 8; j++)
            t->data[j] = n;

        loadTiles(0, 100 + i, t, 1);
    }

    delete t;

    customSkin = new TILE();
    for (int j = 0; j < 8; j++) {
        customSkin->data[j] = 0;
    }
}

void showColorPalette(int c) {

    const int add = 2 * (savefile->settings.aspectRatio != 0);

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 8; j++) {
            if (i == 1) {
                if (j > 0 && j < 7)
                    setTile(27, j + add, i,
                            tileBuild(100 + j - 1, false, false, 0));
                else
                    setTile(27, j + add, i, tileBuild(27, (j == 7), false, 6));
            } else {
                if (j > 0 && j < 7)
                    setTile(27, j + add, i, tileBuild(28, false, (i == 2), 6));
                else
                    setTile(27, j + add, i,
                            tileBuild(29, (j == 7), (i == 2), 6));
            }
        }
    }

    aprint("      ", 0, 1);

    setLayerScroll(3, 4, 4);

    OBJ_ATTR* sprite = &obj_buffer[11];

    sprite_set_attr(sprite, ShapeSquare, 1, 512 + 216, 15, 0);
    sprite_set_pos(sprite,
                   3 + c * 8 + 16 * (savefile->settings.aspectRatio != 0), 3);
    sprite_unhide(sprite, 0);
}

void showTools(int c) {
    OBJ_ATTR* sprites[3];

    for (int i = 0; i < 3; i++) {
        sprites[i] = &obj_buffer[i + 8];
        sprite_set_attr(sprites[i], ShapeSquare, 1, 716 + i * 4, 15, 2);

        int offset = 0;

        if (i == c) {
            aprint("<", 3 + 2 * (savefile->settings.aspectRatio != 0),
                   4 + i * 2);

            offset = ((sinLut(toolTimer) * 2) >> 12);

        } else
            aprint(" ", 3 + 2 * (savefile->settings.aspectRatio != 0),
                   4 + i * 2);

        sprite_set_pos(sprites[i],
                       4 + 13 * (savefile->settings.aspectRatio != 0),
                       32 + (i * 16) - 4 + offset);
        sprite_unhide(sprites[i], 0);
    }

    toolTimer += 6;
    if (toolTimer >= 512)
        toolTimer = 0;
}

void showMinos() {

    for (int i = 0; i < 7; i++) {
        OBJ_ATTR* sprite = &obj_buffer[i + 1];

        if (!onMini) {
            sprite_set_attr(sprite, ShapeSquare, 2, 16 * i, i, 2);
            sprite_unhide(sprite, 0);
        } else {
            sprite_unhide(sprite, 0);
            sprite_set_attr(sprite, ShapeWide, 2, 9 * 16 + 8 * i, i, 3);
        }
        sprite_set_pos(sprite,
                       208 - (4 - onMini) * (i == 0 || i == 3) -
                           13 * (savefile->settings.aspectRatio != 0),
                       i * 24 - 4);
    }
}

void refreshSkin() {
    if (!onMini) {
        savefile->settings.skin = -(skinIndex + 1);
        for (int i = 0; i < 8; i++) {
            savefile->customSkins[skinIndex].board.data[i] =
                customSkin->data[i];
        }
        setSkin();
    } else {
        for (int i = 0; i < 8; i++) {
            savefile->customSkins[skinIndex].smallBoard.data[i] =
                customSkin->data[i];
        }

        refresh = true;
    }
}

void showCursor() {
    cursorTimer++;

    const int animationLength = 32;

    if (cursorTimer < animationLength / 2) {
        loadSpriteTiles(512 + 200, sprite29tiles_bin, 2, 2);
    } else {
        loadSpriteTiles(512 + 200, sprite30tiles_bin, 2, 2);
        if (cursorTimer >= animationLength - 1)
            cursorTimer = 0;
    }
}

void showMini() {
    for (int i = 0; i < 7; i++) {
        OBJ_ATTR* sprite = &obj_buffer[12 + i];

        sprite_unhide(sprite, 0);
        sprite_set_attr(sprite, ShapeWide, 2, 9 * 16 + 8 * i, i, 3);
        sprite_set_pos(sprite, 192, i * 24 + 4);
    }
}

void helpScreen() {
    sfx(SFX_MENUCONFIRM);
    setSmallTextArea(110, 8, 1, 25, 21);
    clearText();

    const int startX = 2;
    const int startY = 1;
    u8 count = 0;

    MenuKeys m = savefile->settings.menuKeys;

    // aprints("-Move using the D-Pad",startX, (startY+count)*4,2);
    // count+= 3;

    aprints("-Use tools by pressing " + getStringFromKey(m.confirm), startX,
            (startY + count) * 4, 2);
    count += 3;

    aprints("-Undo by pressing " + getStringFromKey(m.cancel), startX,
            (startY + count) * 4, 2);
    count += 3;

    aprints("-Change tools by holding " + getStringFromKey(m.special2), startX,
            (startY + count) * 4, 2);
    aprints(" and pressing Up/Down", startX, (startY + count + 2) * 4, 2);
    count += 5;

    aprints("-Change colors by holding " + getStringFromKey(m.special2), startX,
            (startY + count) * 4, 2);
    aprints(" and pressing Left/Right", startX, (startY + count + 2) * 4, 2);
    count += 5;

    aprints("-Cycle palettes by holding " + getStringFromKey(m.special1),
            startX, (startY + count) * 4, 2);
    aprints(" and pressing Up/Down", startX, (startY + count + 2) * 4, 2);
    count += 5;

    aprints("-Edit preview/hold by holding " + getStringFromKey(m.special1),
            startX, (startY + count) * 4, 2);
    aprints(" and pressing Left/Right", startX, (startY + count + 2) * 4, 2);
    count += 5;

    aprints("-Clear the canvas by", startX, (startY + count) * 4, 2);
    aprints(" holding " + getStringFromKey(m.special1) + " and " +
                getStringFromKey(m.special2),
            startX, (startY + count + 2) * 4, 2);
    count += 5;

    aprints("-Exit by pressing " + getStringFromKey(m.pause), startX,
            (startY + count) * 4, 2);
    count += 3;

    clearSprites(128);
    showSprites(128);

    toggleBG(2, false);
    toggleBG(3, false);

    while (closed()) {
        vsync();
        key_poll();

        if (key_hit(KEY_FULL))
            break;
    }

    toggleBG(2, true);
    toggleBG(3, true);

    clearSmallText();
    setSmallTextArea(110, 0, 10, 10, 20);
    clearText();

    showHelpText();

    sfx(SFX_MENUCANCEL);
}

void EditorScene::draw() {
    if (refresh) {
        refresh = false;
        buildMini(customSkin);
    }
}

void showHelpText() {
    if (savefile->settings.aspectRatio == 0) {
        aprints("Press " +
                    getStringFromKey(savefile->settings.menuKeys.special3),
                2, 64, 2);
        aprints("for help", 2, 72, 2);

    } else if (savefile->settings.aspectRatio == 1) {
        aprints("Press", 15, 48, 2);
        aprints(getStringFromKey(savefile->settings.menuKeys.special3), 15, 56,
                2);
        aprints("for", 15, 64, 2);
        aprints("help", 15, 72, 2);
    }
}

#endif
