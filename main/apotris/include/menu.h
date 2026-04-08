#pragma once

#include "font1tiles_bin.h"
#include "platform.hpp"
#include "save.h"
#include "text.h"
#include <list>
#include <mutex>
#include <string>

#define MAX_WORD_SPRITES 12

class WordSprite {
public:
    std::string text = "";
    int startIndex = 0;
    int startTiles = 0;
    int id = 0;
    int priority = 0;
    bool blend = false;
    OBJ_ATTR* sprites[5];
    int width = 0;
    bool big = false;

    int timer = 0;
    bool slow = false;
    int cursor = 0;
    int w = 0;
    const int timerMax = 1;
    const int count = 1;
    int scroll = 0;
    int startDelay = 0;
    const int startDelayMax = 10;

    void show(int x, int y, int palette) {
        int tempX = x - (scroll & 0x7);

        if (savefile->settings.aspectRatio == 1 && !(scroll || slow)) {
            if (tempX < 13)
                tempX += 13;
            else if (tempX + width > 227)
                tempX -= 13;
        }

        for (int i = 0; i < 3 * (1 + big) - big; i++) {
            sprite_unhide(sprites[i], 0);
            sprite_enable_mosaic(sprites[i]);
            sprite_set_attr(sprites[i], ShapeWide, 1,
                            startTiles + i * 4 + (scroll >> 3), palette,
                            priority);

            if (blend)
                sprite_enable_blend(sprites[i]);

            sprite_set_pos(sprites[i], tempX + i * 32, y);
        }

        if (slow)
            slowPrint();
    }

    void show(int x, int y, int palette, FIXED scale) {
        for (int i = 0; i < 3 * (1 + big) - big; i++) {
            int affId = id * 3 + i;
            sprite_unhide(sprites[i], 0);
            sprite_set_attr(sprites[i], ShapeWide, 1, startTiles + i * 4,
                            palette, priority);

            if (blend)
                sprite_enable_blend(sprites[i]);

            sprite_set_pos(sprites[i], x + fx2int(fxdiv(int2fx(i * 32), scale)),
                           y);
            sprite_set_size(sprites[i], scale, affId);
            sprite_enable_affine(sprites[i], affId, true);
            sprite_enable_mosaic(sprites[i]);
        }

        if (slow)
            slowPrint();
    }

    void setPriority(int newPriority) { priority = newPriority; }

    void hide() {
        for (int i = 0; i < 3 * (1 + big) - big; i++)
            sprite_hide(sprites[i]);
    }

    void enableBlend(bool state) { blend = state; }

    void setScroll(int s) { scroll = s; }

    void slowPrint() {
        if (cursor >= (int)text.size())
            return;

        if (startDelay) {
            startDelay--;
            return;
        }

        if (++timer >= timerMax) {
            timer = 0;

            const int maxWidth =
                ((savefile->settings.aspectRatio) ? 214 : 240) + 4;

            int counter = count;
            while ((w <= (maxWidth) + scroll) && counter--) {
                std::string c = text.substr(cursor, 1);

                naprintSpriteOffset(c, startTiles, w);

                w += getVariableWidth(c);

                cursor++;
            }
        }
    }

    void setText(const std::string& _text);
    void setTextNum(int n);

    void setTextSlow(std::string _text) {
        text = _text;

        slow = true;
        timer = 0;
        cursor = 0;
        w = 0;
        startDelay = startDelayMax;

        if (text == "") {
            clearSpriteTiles(startTiles, (big) ? 20 : 12, 0);
            // memset32(&tile_mem[4][startTiles], 0, 12 * 8);
            return;
        }

        width = getVariableWidth(text);

        clearSpriteTiles(startTiles, (big) ? 20 : 12, 1);
    }

    void setup(int _id, int _index, int _tiles, bool _big);

    WordSprite() { setup(0, 0, 0, false); }

    WordSprite(int _id, int _index, int _tiles) {
        setup(_id, _index, _tiles, false);
    }

    WordSprite(int _id, int _index, int _tiles, bool _big) {
        setup(_id, _index, _tiles, _big);
    }
};
